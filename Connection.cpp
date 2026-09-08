#include <phpcpp.h>
#include <exception>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <limits>
#include <sstream>
#include <iomanip>
#include <unistd.h>
#include "c_logging/logger.h"
#include "c_logging/log_sink_console.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/tlsio.h"
#include "azure_c_shared_utility/socketio.h"
#include "azure_uamqp_c/uamqp.h"
#include "Connection.h"
#include "Session.h"
#include "Producer.h"
#include "Consumer.h"
#include "Message.h"

namespace
{
    FILE* debugLogFile = NULL;
    unsigned long connectionSequence = 0;

    long long timestampMilliseconds()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    void onIoError(void *context)
    {
        static_cast<Connection *>(context)->handleIoError();
    }

    int debugFileSinkInit()
    {
        const char* path = std::getenv("UAMQP_DEBUG_FILE");
        if (path == NULL || path[0] == '\0')
        {
            return -1;
        }

        debugLogFile = std::fopen(path, "a");
        return debugLogFile == NULL ? -1 : 0;
    }

    void debugFileSinkLog(LOG_LEVEL logLevel, LOG_CONTEXT_HANDLE, const char* file, const char* func,
                          int line, const char* messageFormat, va_list args)
    {
        if (debugLogFile == NULL || messageFormat == NULL)
        {
            return;
        }

        std::fprintf(debugLogFile, "ts=%lld pid=%ld %s%s:%d %s: ",
                     timestampMilliseconds(), static_cast<long>(getpid()),
                     logLevel == LOG_LEVEL_VERBOSE ? "[DEBUG] " : "",
                     file == NULL ? "" : file, line,
                     func == NULL ? "" : func);
        std::vfprintf(debugLogFile, messageFormat, args);
        std::fputc('\n', debugLogFile);
        std::fflush(debugLogFile);
    }

    void debugFileSinkDeinit()
    {
        if (debugLogFile != NULL)
        {
            std::fclose(debugLogFile);
            debugLogFile = NULL;
        }
    }

    const LOG_SINK_IF debugFileSink = {
        debugFileSinkInit,
        debugFileSinkLog,
        debugFileSinkDeinit
    };

    void ensureLoggerConfigured()
    {
        static bool loggerConfigured = false;

        if (loggerConfigured)
        {
            return;
        }

        const char* debugFilePath = std::getenv("UAMQP_DEBUG_FILE");
        const LOG_SINK_IF* sink = &log_sink_console;
        if (debugFilePath != NULL && debugFilePath[0] != '\0')
        {
            sink = &debugFileSink;
        }

        static const LOG_SINK_IF* sinks[1];
        sinks[0] = sink;
        LOGGER_CONFIG config = {1, sinks};
        logger_set_config(config);

        if (logger_init() != 0)
        {
            if (sink == &debugFileSink)
            {
                std::printf("Could not open UAMQP_DEBUG_FILE '%s'; using standard output\n",
                            debugFilePath);
                sinks[0] = &log_sink_console;
                logger_set_config(config);
                if (logger_init() != 0)
                {
                    throw Php::Exception("Could not initialize logger");
                }
            }
            else
            {
                throw Php::Exception("Could not initialize logger");
            }
        }

        loggerConfigured = true;
    }
}

void Connection::__construct(Php::Parameters& params)
{
    port = 0;
    useTls = false;
    debug = false;
    isConnected = false;
    closeRequested = false;
    platformInitialized = false;
    session = NULL;
    consumer = NULL;
    connection = NULL;
    sasl_io = NULL;
    socket_io = NULL;
    tlsio_interface = NULL;
    sasl_mechanism_handle = NULL;
    tls_io = NULL;

    host = params[0].stringValue();
    port = params[1].numericValue();
    useTls = params[2].boolValue();
    keyName = params[3].stringValue();
    key = params[4].stringValue();
    debug = params.size() == 6 ? params[5].boolValue() : false;
}

Connection::Connection()
{
    port = 0;
    useTls = false;
    debug = false;
    isConnected = false;
    closeRequested = false;
    platformInitialized = false;
    session = NULL;
    consumer = NULL;
    connection = NULL;
    sasl_io = NULL;
    socket_io = NULL;
    tlsio_interface = NULL;
    sasl_mechanism_handle = NULL;
    tls_io = NULL;
}

Connection::~Connection()
{
    try
    {
        close();
    }
    catch (...)
    {
        // Destructors must never throw during PHP shutdown.
    }
}

void Connection::connect()
{
    if (doingWork || receiverRunActive) {
        throw Php::Exception("Cannot reconnect or create a link while a receiver callback loop is active");
    }
    if (isConnected)
    {
        return;
    }

    closeRequested = false;
    ioError = false;
    connectionId = std::to_string(getpid()) + "-" + std::to_string(++connectionSequence);

    bool useAuth = !keyName.empty() && !key.empty();

    // c_logging aborts on every log call before initialization, including errors
    // when tracing is disabled. Debug controls wire tracing, not logger lifetime.
    ensureLoggerConfigured();

    if (platform_init() == 0)
    {
        platformInitialized = true;
    }
    else
    {
        //throw Php::Exception("Could not run platform_init");
    }

    if (useTls)
    {
        tls_io_config = {host.c_str(), port};
        /* create the TLS IO */
        tlsio_interface = platform_get_default_tlsio();
        tls_io = xio_create(tlsio_interface, &tls_io_config);
    }
    else
    {
        socketio_config = {host.c_str(), port, NULL};
        socket_io = xio_create(socketio_get_interface_description(), &socketio_config);
    }

    if (useAuth)
    {
        sasl_plain_config = {keyName.c_str(), key.c_str(), NULL};
        /* create SASL PLAIN handler */
        sasl_mechanism_handle = saslmechanism_create(saslplain_get_interface(), &sasl_plain_config);
        /* create the SASL client IO using the TLS IO or SOCKET OI */
        if (useTls)
        {
            sasl_io_config.underlying_io = tls_io;
        }
        else
        {
            sasl_io_config.underlying_io = socket_io;
        }
        sasl_io_config.sasl_mechanism = sasl_mechanism_handle;
        sasl_io = xio_create(saslclientio_get_interface_description(), &sasl_io_config);
    }

    /* create the connection */
    XIO_HANDLE transport = useAuth ? sasl_io : (useTls ? tls_io : socket_io);
    connection = connection_create2(transport, host.c_str(), connectionId.c_str(), NULL, NULL,
        NULL, NULL, onIoError, this);
    if (connection == NULL)
    {
        throw Php::Exception("Could not create connection");
    }
    if (isDebugOn())
    {
        connection_set_trace(connection, true);
    }

    // Session
    session = new Session(this);

    isConnected = true;
}

void Connection::publish(Php::Parameters& params)
{
    connect();

    std::string resourceName = params[0].stringValue();
    Message* message = (Message*)params[1].implementation();

    Producer producer(session, resourceName);
    producer.publish(message);
}

void Connection::setCallback(Php::Parameters& params)
{
    uint32_t maxLinkCredit = 0;
    if (params.size() > 3) {
        int64_t requestedCredit = params[3].numericValue();
        if (requestedCredit < 1 || static_cast<uint64_t>(requestedCredit) > std::numeric_limits<uint32_t>::max()) {
            throw Php::Exception("maxLinkCredit must be between 1 and 4294967295; omit it for continuous consumption");
        }
        maxLinkCredit = static_cast<uint32_t>(requestedCredit);
    }
    connect();

    std::string resourceName = params[0].stringValue();
    Php::Value callback = params[1];
    Php::Value loopFn = params[2];

    if (consumer != NULL) {
        consumer->close();
        delete consumer;
        consumer = NULL;
    }
    consumer = new Consumer(session, resourceName, maxLinkCredit);
    receiverRunActive = true;
    try {
        consumer->setCallback(callback, loopFn);
    } catch (...) {
        receiverRunActive = false;
        // Preserve the original callback/loop exception even if cleanup also fails.
        try {
            close();
        } catch (...) {
        }
        throw;
    }
    receiverRunActive = false;
    if (closeRequested) {
        close();
    }
}

void Connection::consume()
{
    if (doingWork) {
        throw Php::Exception("Cannot recursively consume from a message callback");
    }
    if (consumer != NULL)
    {
        consumer->consume();
    }
}

Php::Value Connection::wasCloseRequested()
{
    return closeRequested || (consumer != NULL && consumer->wasCloseRequested());
}

std::string Connection::getHost()
{
    return host;
}

CONNECTION_HANDLE Connection::getConnectionHandler()
{
    return connection;
}

void Connection::doWork()
{
    if (doingWork) {
        throw Php::Exception("Cannot recursively dispatch AMQP I/O");
    }
    if (connection != NULL)
    {
        doingWork = true;
        try {
            connection_dowork(connection);
        } catch (...) {
            doingWork = false;
            throw;
        }
        doingWork = false;
    }
}

bool Connection::isDebugOn()
{
    return debug;
}

void Connection::close()
{
    std::string closeError;
    std::exception_ptr closeException;
    closeRequested = true;
    if (consumer != NULL) {
        consumer->requestStop("connection-close");
    }
    // The receive callback runs inside connection_dowork. Its stack still borrows
    // the link/session/transport until dispatch returns.
    if (doingWork || receiverRunActive || closing)
    {
        return;
    }
    closing = true;
    if (consumer != NULL)
    {
        try
        {
            consumer->close();
        }
        catch (Php::Throwable& e)
        {
            closeException = std::current_exception();
        }
        catch (const std::exception& e)
        {
            closeError = e.what();
        }
        catch (...)
        {
            closeError = "Unknown consumer shutdown error";
        }
        delete consumer;
        consumer = NULL;
    }

    if (session != NULL)
    {
        session->close();
        delete session;
        session = NULL;
    }

    if (connection != NULL)
    {
        connection_destroy(connection);
        connection = NULL;
    }
    if (sasl_io != NULL)
    {
        xio_destroy(sasl_io);
        sasl_io = NULL;
    }
    if (tls_io != NULL)
    {
        xio_destroy(tls_io);
        tls_io = NULL;
    }
    if (socket_io != NULL) {
        xio_destroy(socket_io);
        socket_io = NULL;
    }
    if (sasl_mechanism_handle != NULL)
    {
        saslmechanism_destroy(sasl_mechanism_handle);
        sasl_mechanism_handle = NULL;
    }
    if (platformInitialized)
    {
        platform_deinit();
        platformInitialized = false;
    }

    isConnected = false;
    closing = false;
    session = NULL;
    consumer = NULL;


    if (closeException)
    {
        std::rethrow_exception(closeException);
    }
    if (!closeError.empty())
    {
        throw Php::Exception(closeError);
    }
}

bool Connection::isDoingWork() const
{
    return doingWork;
}

bool Connection::hasIoError() const
{
    return ioError;
}

void Connection::handleIoError()
{
    ioError = true;
    if (consumer != NULL) {
        consumer->handleCallbackException("AMQP transport I/O failed");
    }
}

std::string Connection::quote(const std::string &value)
{
    std::ostringstream output;
    output << '"';
    for (size_t index = 0; index < value.size(); ++index) {
        unsigned char character = value[index];
        if (character == '"' || character == '\\') {
            output << '\\' << character;
        } else if (character >= 0x80) {
            // Keep valid UTF-8 IDs unchanged, escaping malformed/binary bytes.
            size_t length = character >= 0xc2 && character <= 0xdf ? 2 :
                (character >= 0xe0 && character <= 0xef ? 3 :
                (character >= 0xf0 && character <= 0xf4 ? 4 : 0));
            bool valid = length != 0 && index + length <= value.size();
            for (size_t offset = 1; valid && offset < length; ++offset) {
                unsigned char next = value[index + offset];
                valid = next >= 0x80 && next <= 0xbf;
                if (offset == 1) {
                    valid = valid && !(character == 0xe0 && next < 0xa0) &&
                        !(character == 0xed && next >= 0xa0) && !(character == 0xf0 && next < 0x90) &&
                        !(character == 0xf4 && next >= 0x90);
                }
            }
            if (valid) {
                output.write(value.data() + index, length);
                index += length - 1;
            } else {
                output << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(character);
            }
        } else if (character < 0x20) {
            output << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(character);
        } else {
            output << character;
        }
    }
    output << '"';
    return output.str();
}

void Connection::trace(const std::string &event, const std::string &fields) const
{
    if (!debug) {
        return;
    }
    FILE *destination = debugLogFile == NULL ? stderr : debugLogFile;
    const std::string record = "{\"timestampMs\":" + std::to_string(timestampMilliseconds()) +
        ",\"pid\":" + std::to_string(getpid()) + ",\"connection\":" + quote(connectionId) +
        ",\"event\":" + quote(event) + (fields.empty() ? "" : "," + fields) + "}";
    std::fprintf(destination, "%s\n", record.c_str());
    std::fflush(destination);
}
