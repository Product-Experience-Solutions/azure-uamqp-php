#include <phpcpp.h>
#include <exception>
#include <cstdio>
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
void ensureLoggerConfigured()
{
    static bool loggerConfigured = false;

    if (loggerConfigured) {
        return;
    }

    if (logger_init() != 0) {
        throw Php::Exception("Could not initialize logger");
    }

    static const LOG_SINK_IF *sinks[] = { &log_sink_console };
    LOGGER_CONFIG config = { 1, sinks };
    logger_set_config(config);

    loggerConfigured = true;
}
}

void Connection::__construct(Php::Parameters &params)
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

    host    = params[0].stringValue();
    port    = params[1].numericValue();
    useTls  = params[2].boolValue();
    keyName = params[3].stringValue();
    key     = params[4].stringValue();
    debug   = params.size() == 6 ? params[5].boolValue() : false;
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
    try {
        close();
    } catch (...) {
        // Destructors must never throw during PHP shutdown.
    }
}

void Connection::connect()
{
    if (isConnected) {
        return;
    }

    closeRequested = false;

    bool useAuth = !keyName.empty() && !key.empty();

    if (debug) {
        ensureLoggerConfigured();
    }

    if (platform_init() == 0) {
        platformInitialized = true;
    } else {
        //throw Php::Exception("Could not run platform_init");
    }

    if (useTls) {
        tls_io_config = { host.c_str(), port };
        /* create the TLS IO */
        tlsio_interface = platform_get_default_tlsio();
        tls_io = xio_create(tlsio_interface, &tls_io_config);
    } else {
        socketio_config = { host.c_str(), port, NULL };
        socket_io = xio_create(socketio_get_interface_description(), &socketio_config);
    }

    if (useAuth) {
        sasl_plain_config = { keyName.c_str(), key.c_str(), NULL };
        /* create SASL PLAIN handler */
        sasl_mechanism_handle = saslmechanism_create(saslplain_get_interface(), &sasl_plain_config);
        /* create the SASL client IO using the TLS IO or SOCKET OI */
        if (useTls) {
            sasl_io_config.underlying_io = tls_io;
        } else {
            sasl_io_config.underlying_io = socket_io;
        }
        sasl_io_config.sasl_mechanism = sasl_mechanism_handle;
        sasl_io = xio_create(saslclientio_get_interface_description(), &sasl_io_config);
    }

    /* create the connection */
    connection = connection_create(useAuth ? sasl_io : socket_io, host.c_str(), "some", NULL, NULL);
    if (connection == NULL) {
        throw Php::Exception("Could not create connection");
    }
    if (isDebugOn()) {
        connection_set_trace(connection, true);
    }

    // Session
    session = new Session(this);

    isConnected = true;
}

void Connection::publish(Php::Parameters &params)
{
    connect();

    std::string resourceName = params[0].stringValue();
    Message *message = (Message*) params[1].implementation();

    Producer producer(session, resourceName);
    producer.publish(message);
}

void Connection::setCallback(Php::Parameters &params)
{
    connect();

    std::string resourceName = params[0].stringValue();
    Php::Value callback = params[1];
    Php::Value loopFn = params[2];

    consumer = new Consumer(session, resourceName);
    consumer->setCallback(callback, loopFn);
}

void Connection::consume()
{
    if (consumer != NULL && !consumer->wasCloseRequested()) {
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
    if (connection != NULL) {
        connection_dowork(connection);
    }
}

bool Connection::isDebugOn()
{
    return debug;
}

void Connection::close()
{
    std::string closeError;

    if (closeRequested) {
        return;
    }

    closeRequested = true;

    if (consumer != NULL && !consumer->wasCloseRequested()) {
        try {
            consumer->close();
        } catch (const std::exception &e) {
            closeError = e.what();
        } catch (...) {
            closeError = "Unknown consumer shutdown error";
        }
    }

    if (session != NULL) {
        session->close();
    }

    if (connection != NULL) {
        connection_destroy(connection);
        connection = NULL;
    }
    if (sasl_io != NULL) {
        xio_destroy(sasl_io);
        sasl_io = NULL;
    }
    if (tls_io != NULL) {
        xio_destroy(tls_io);
        tls_io = NULL;
    }
    if (sasl_mechanism_handle != NULL) {
        saslmechanism_destroy(sasl_mechanism_handle);
        sasl_mechanism_handle = NULL;
    }
    if (platformInitialized) {
        platform_deinit();
        platformInitialized = false;
    }

    isConnected = false;
    session = NULL;
    consumer = NULL;


    if (!closeError.empty()) {
        throw Php::Exception(closeError);
    }
}
