#include <azure_c_shared_utility/platform.h>
#include <azure_c_shared_utility/socketio.h>
#include <azure_c_shared_utility/tlsio.h>
#include <azure_uamqp_c/uamqp.h>
#include <c_logging/log_sink_console.h>
#include <c_logging/logger.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{
struct Endpoint
{
    std::string host;
    std::string resource;
    std::string user;
    std::string password;
    int port = 5671;
};

struct Options
{
    Endpoint endpoint;
    std::string user;
    std::string password;
    int count = 1;
    int timeoutSeconds = 60;
};

struct State
{
    CONNECTION_STATE connectionState = CONNECTION_STATE_START;
    MESSAGE_RECEIVER_STATE receiverState = MESSAGE_RECEIVER_STATE_IDLE;
    bool ioError = false;
    bool linkError = false;
    bool stopped = false;
    int received = 0;
    int targetCount = 1;
    std::string error;
};

void usage(const char *program)
{
    std::cerr
        << "Usage: " << program << " --endpoint HOST:PORT/ENTITY[/Subscriptions/SUBSCRIPTION]"
        << " --user USER --password PASSWORD [--count N] [--timeout SECONDS]\n"
        << "Credentials should be supplied through environment variables, for example:\n"
        << "  UAMQP_ENDPOINT='host:5671/entity/Subscriptions/sub' "
           "UAMQP_USER='name' UAMQP_PASSWORD='secret' "
           "debugging/build/subscription_consumer --count 2\n";
}

int parsePositive(const char *name, const std::string &value)
{
    char *end = nullptr;
    long parsed = std::strtol(value.c_str(), &end, 10);
    if (value.empty() || end == value.c_str() || *end != '\0' || parsed <= 0 || parsed > 86400) {
        throw std::runtime_error(std::string(name) + " must be a positive integer");
    }
    return static_cast<int>(parsed);
}

int hexDigit(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

std::string decodePercent(const std::string &value)
{
    std::string decoded;
    for (std::string::size_type i = 0; i < value.size(); ++i) {
        if (value[i] != '%') {
            decoded += value[i];
            continue;
        }
        if (i + 2 >= value.size()) {
            throw std::runtime_error("invalid percent-encoding in credential");
        }
        int high = hexDigit(value[i + 1]);
        int low = hexDigit(value[i + 2]);
        if (high < 0 || low < 0) {
            throw std::runtime_error("invalid percent-encoding in credential");
        }
        decoded += static_cast<char>((high << 4) | low);
        i += 2;
    }
    return decoded;
}

Endpoint parseEndpoint(std::string value)
{
    const std::string scheme = "amqps://";
    if (value.compare(0, scheme.size(), scheme) == 0) {
        value.erase(0, scheme.size());
    }

    const std::string query = "?";
    std::string::size_type queryPosition = value.find(query);
    if (queryPosition != std::string::npos) {
        value.erase(queryPosition);
    }

    std::string::size_type slash = value.find('/');
    if (slash == std::string::npos || slash == 0 || slash == value.size() - 1) {
        throw std::runtime_error("endpoint must contain HOST:PORT/RESOURCE");
    }

    Endpoint endpoint;
    std::string hostPort = value.substr(0, slash);
    endpoint.resource = value.substr(slash + 1);
    std::string::size_type at = hostPort.rfind('@');
    if (at != std::string::npos) {
        std::string userInfo = hostPort.substr(0, at);
        hostPort.erase(0, at + 1);
        std::string::size_type separator = userInfo.find(':');
        if (separator == std::string::npos || separator == 0 || separator == userInfo.size() - 1) {
            throw std::runtime_error("endpoint credentials must use USER:PASSWORD@HOST");
        }
        endpoint.user = decodePercent(userInfo.substr(0, separator));
        endpoint.password = decodePercent(userInfo.substr(separator + 1));
    }
    std::string::size_type colon = hostPort.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon == hostPort.size() - 1) {
        throw std::runtime_error("endpoint must contain HOST:PORT");
    }

    endpoint.host = hostPort.substr(0, colon);
    endpoint.port = parsePositive("port", hostPort.substr(colon + 1));
    return endpoint;
}

std::string requiredValue(const char *option, const std::string &value)
{
    if (value.empty()) {
        throw std::runtime_error(std::string(option) + " is required");
    }
    return value;
}

Options parseOptions(int argc, char **argv)
{
    Options options;
    std::string endpoint = std::getenv("UAMQP_ENDPOINT") ? std::getenv("UAMQP_ENDPOINT") : "";
    options.user = std::getenv("UAMQP_USER") ? std::getenv("UAMQP_USER") : "";
    options.password = std::getenv("UAMQP_PASSWORD") ? std::getenv("UAMQP_PASSWORD") : "";

    for (int i = 1; i < argc; ++i) {
        std::string argument(argv[i]);
        auto value = [&](const char *name) {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string(name) + " requires a value");
            }
            return std::string(argv[++i]);
        };
        if (argument == "--endpoint") {
            endpoint = value("--endpoint");
        } else if (argument == "--user") {
            options.user = value("--user");
        } else if (argument == "--password") {
            options.password = value("--password");
        } else if (argument == "--count") {
            options.count = parsePositive("--count", value("--count"));
        } else if (argument == "--timeout") {
            options.timeoutSeconds = parsePositive("--timeout", value("--timeout"));
        } else if (argument == "--help" || argument == "-h") {
            usage(argv[0]);
            std::exit(0);
        } else {
            throw std::runtime_error("unknown option: " + argument);
        }
    }

    options.endpoint = parseEndpoint(requiredValue("--endpoint/UAMQP_ENDPOINT", endpoint));
    if (options.user.empty()) {
        options.user = options.endpoint.user;
    }
    if (options.password.empty()) {
        options.password = options.endpoint.password;
    }
    options.user = decodePercent(options.user);
    options.password = decodePercent(options.password);
    requiredValue("--user/UAMQP_USER", options.user);
    requiredValue("--password/UAMQP_PASSWORD", options.password);
    return options;
}

const char *connectionStateName(CONNECTION_STATE state)
{
    switch (state) {
    case CONNECTION_STATE_OPENED: return "OPENED";
    case CONNECTION_STATE_ERROR: return "ERROR";
    case CONNECTION_STATE_END: return "END";
    default: return "TRANSITION";
    }
}

void onConnectionState(void *context, CONNECTION_STATE current, CONNECTION_STATE previous)
{
    auto *state = static_cast<State *>(context);
    state->connectionState = current;
    std::cerr << "[connection] " << connectionStateName(previous) << " -> "
              << connectionStateName(current) << "\n";
}

void onIoError(void *context)
{
    auto *state = static_cast<State *>(context);
    state->ioError = true;
    state->error = "uAMQP IO error";
    std::cerr << "[error] uAMQP IO error\n";
}

void onReceiverState(const void *context, MESSAGE_RECEIVER_STATE current, MESSAGE_RECEIVER_STATE previous)
{
    auto *state = const_cast<State *>(static_cast<const State *>(context));
    state->receiverState = current;
    std::cerr << "[receiver] " << previous << " -> " << current << "\n";
}

void onDetach(void *context, ERROR_HANDLE error)
{
    auto *state = static_cast<State *>(context);
    state->linkError = true;
    const char *condition = nullptr;
    const char *description = nullptr;
    if (error != nullptr) {
        error_get_condition(error, &condition);
        error_get_description(error, &description);
    }
    state->error = std::string("receiver link detached: ") +
        (condition ? condition : "unknown") + " " + (description ? description : "");
    std::cerr << "[error] " << state->error << "\n";
}

AMQP_VALUE onMessage(const void *context, MESSAGE_HANDLE message)
{
    auto *state = const_cast<State *>(static_cast<const State *>(context));
    ++state->received;
    std::cerr << "[message] received #" << state->received << "\n";

    MESSAGE_BODY_TYPE bodyType;
    if (message_get_body_type(message, &bodyType) == 0) {
        if (bodyType == MESSAGE_BODY_TYPE_DATA) {
            size_t dataCount = 0;
            if (message_get_body_amqp_data_count(message, &dataCount) == 0) {
                std::cout << "[message] body: ";
                for (size_t i = 0; i < dataCount; ++i) {
                    BINARY_DATA data = {};
                    if (message_get_body_amqp_data_in_place(message, i, &data) == 0) {
                        std::cout.write(reinterpret_cast<const char *>(data.bytes), data.length);
                    }
                }
                std::cout << "\n";
            } else {
                std::cerr << "[message] unable to determine data body count\n";
            }
        } else if (bodyType == MESSAGE_BODY_TYPE_VALUE) {
            AMQP_VALUE body = nullptr;
            if (message_get_body_amqp_value_in_place(message, &body) == 0) {
                char *bodyText = amqpvalue_to_string(body);
                std::cout << "[message] body: " << (bodyText ? bodyText : "<unavailable>") << "\n";
                std::free(bodyText);
            } else {
                std::cerr << "[message] unable to read AMQP value body\n";
            }
        } else if (bodyType == MESSAGE_BODY_TYPE_SEQUENCE) {
            size_t sequenceCount = 0;
            if (message_get_body_amqp_sequence_count(message, &sequenceCount) == 0) {
                std::cout << "[message] body: ";
                for (size_t i = 0; i < sequenceCount; ++i) {
                    AMQP_VALUE sequence = nullptr;
                    if (message_get_body_amqp_sequence_in_place(message, i, &sequence) == 0) {
                        char *sequenceText = amqpvalue_to_string(sequence);
                        std::cout << (sequenceText ? sequenceText : "<unavailable>");
                        std::free(sequenceText);
                    }
                }
                std::cout << "\n";
            } else {
                std::cerr << "[message] unable to determine sequence body count\n";
            }
        } else {
            std::cout << "[message] body: <empty>\n";
        }
    } else {
        std::cerr << "[message] unable to inspect body type\n";
    }

    if (state->received >= state->targetCount) {
        state->stopped = true;
    }
    return messaging_delivery_accepted();
}

void destroyResources(CONNECTION_HANDLE connection, XIO_HANDLE saslIo, XIO_HANDLE tlsIo,
    XIO_HANDLE socketIo, SASL_MECHANISM_HANDLE mechanism, MESSAGE_RECEIVER_HANDLE receiver,
    LINK_HANDLE link, SESSION_HANDLE session, State &state)
{
    if (receiver != nullptr) {
        messagereceiver_destroy(receiver);
    }
    if (link != nullptr) {
        link_destroy(link);
    }
    if (session != nullptr) {
        session_destroy(session);
    }
    if (connection != nullptr) {
        connection_destroy(connection);
    }
    if (saslIo != nullptr) {
        xio_destroy(saslIo);
    }
    if (tlsIo != nullptr) {
        xio_destroy(tlsIo);
    }
    if (socketIo != nullptr) {
        xio_destroy(socketIo);
    }
    if (mechanism != nullptr) {
        saslmechanism_destroy(mechanism);
    }
    platform_deinit();
    std::cerr << "[cleanup] complete\n";
    (void)state;
}
}

int main(int argc, char **argv)
{
    try {
        Options options = parseOptions(argc, argv);
        std::cerr << "[config] host=" << options.endpoint.host << " port=" << options.endpoint.port
                  << " resource=" << options.endpoint.resource << " count=" << options.count
                  << " timeout=" << options.timeoutSeconds << "s\n";

        if (logger_init() != 0) {
            throw std::runtime_error("logger_init failed");
        }
        const LOG_SINK_IF *sinks[] = {&log_sink_console};
        logger_set_config({1, sinks});
        if (platform_init() != 0) {
            throw std::runtime_error("platform_init failed");
        }

        State state;
        state.targetCount = options.count;
        TLSIO_CONFIG tlsConfig = {options.endpoint.host.c_str(), options.endpoint.port, nullptr, nullptr, false};
        XIO_HANDLE tlsIo = xio_create(platform_get_default_tlsio(), &tlsConfig);
        if (tlsIo == nullptr) {
            throw std::runtime_error("could not create TLS IO");
        }
        SASL_PLAIN_CONFIG plainConfig = {options.user.c_str(), options.password.c_str(), nullptr};
        SASL_MECHANISM_HANDLE mechanism = saslmechanism_create(saslplain_get_interface(), &plainConfig);
        if (mechanism == nullptr) {
            throw std::runtime_error("could not create SASL PLAIN mechanism");
        }
        SASLCLIENTIO_CONFIG saslConfig = {tlsIo, mechanism};
        XIO_HANDLE saslIo = xio_create(saslclientio_get_interface_description(), &saslConfig);
        if (saslIo == nullptr) {
            throw std::runtime_error("could not create SASL client IO");
        }
        std::cerr << "[connection] creating TLS + SASL connection\n";
        CONNECTION_HANDLE connection = connection_create2(
            saslIo, options.endpoint.host.c_str(), "uamqp-subscription-debugger",
            nullptr, nullptr, onConnectionState, &state, onIoError, &state);
        if (connection == nullptr) {
            throw std::runtime_error("connection_create2 failed");
        }
        connection_set_trace(connection, true);

        SESSION_HANDLE session = session_create(connection, nullptr, nullptr);
        if (session == nullptr) {
            throw std::runtime_error("session_create failed");
        }
        session_set_incoming_window(session, 2147483647);
        session_set_outgoing_window(session, 65536);

        std::string address = "amqps://" + options.endpoint.host + "/" + options.endpoint.resource;
        AMQP_VALUE source = messaging_create_source(address.c_str());
        AMQP_VALUE target = messaging_create_target("ingress-rx");
        LINK_HANDLE link = link_create(session, "subscription-debug-receiver", role_receiver, source, target);
        amqpvalue_destroy(source);
        amqpvalue_destroy(target);
        if (link == nullptr) {
            throw std::runtime_error("link_create failed");
        }
        link_set_rcv_settle_mode(link, receiver_settle_mode_first);
        link_subscribe_on_link_detach_received(link, onDetach, &state);
        MESSAGE_RECEIVER_HANDLE receiver = messagereceiver_create(link, onReceiverState, &state);
        if (receiver == nullptr) {
            throw std::runtime_error("messagereceiver_create failed");
        }
        messagereceiver_set_trace(receiver, true);
        if (messagereceiver_open(receiver, onMessage, &state) != 0) {
            throw std::runtime_error("messagereceiver_open failed");
        }
        std::cerr << "[receiver] opened for " << address << "\n";

        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(options.timeoutSeconds);
        while (!state.stopped && state.received < options.count &&
            !state.ioError && !state.linkError && std::chrono::steady_clock::now() < deadline) {
            connection_dowork(connection);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        int result = 0;
        if (state.received < options.count) {
            std::cerr << "[error] receive timeout or transport failure after "
                      << state.received << " message(s)\n";
            result = 1;
        } else {
            std::cerr << "[success] consumed " << state.received << " message(s)\n";
        }
        destroyResources(connection, saslIo, tlsIo, nullptr, mechanism, receiver, link, session, state);
        return result;
    } catch (const std::exception &error) {
        std::cerr << "[fatal] " << error.what() << "\n";
        usage(argv[0]);
        return 2;
    }
}
