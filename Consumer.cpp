#include "Consumer.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/tlsio.h"
#include "azure_c_shared_utility/socketio.h"
#include "azure_uamqp_c/uamqp.h"
#include "Session.h"
#include "Message.h"
#include <exception>

static void on_link_detach_received_consumer(void* context, ERROR_HANDLE error)
{
    auto *consumer = static_cast<Consumer*>(context);
    if (consumer == nullptr) {
        return;
    }

    consumer->handleLinkDetach(error);
}

static AMQP_VALUE on_message_received(const void* context, MESSAGE_HANDLE message)
{
    auto *consumer = const_cast<Consumer*>(static_cast<const Consumer*>(context));
    if (consumer == nullptr) {
        throw Php::Exception("Consumer context is not set");
    }

    try {
        consumer->handleMessage(message);
    } catch (const Php::Exception &error) {
        consumer->handleCallbackException(error.what());
        return messaging_delivery_rejected("amqp:internal-error", error.what());
    } catch (const std::exception &error) {
        consumer->handleCallbackException(error.what());
        return messaging_delivery_rejected("amqp:internal-error", error.what());
    }

    return messaging_delivery_accepted();
}

Consumer::Consumer(Session *session, std::string resourceName)
{
    this->session = session;
    this->resourceName = resourceName;
    stopRunning = false;
    closeRequested = false;
    exceptionMessage.clear();
    link = NULL;
    message_receiver = NULL;
    source = NULL;
    target = NULL;

    source = messaging_create_source(("amqps://" + session->getConnection()->getHost() + "/" + resourceName).c_str());
    target = messaging_create_target("ingress-rx");
    link = link_create(session->getSessionHandler(), "receiver-link", role_receiver, source, target);
    link_set_rcv_settle_mode(link, receiver_settle_mode_first);
    link_subscribe_on_link_detach_received(link, on_link_detach_received_consumer, this);

    amqpvalue_destroy(source);
    amqpvalue_destroy(target);

    /* create a message receiver */
    message_receiver = messagereceiver_create(link, NULL, NULL);

    if (message_receiver == NULL) {
        throw Php::Exception("Could not create message receiver");
    }

    if (session->getConnection()->isDebugOn()) {
        messagereceiver_set_trace(message_receiver, true);
    }
}

void Consumer::setCallback(Php::Value &callback, Php::Value &loopFn)
{
    callbackFn = callback;

    if (messagereceiver_open(message_receiver, on_message_received, this) != 0) {
        throw Php::Exception("Could not open the message receiver");
    }

    loopFn();
}

bool Consumer::handleMessage(MESSAGE_HANDLE message)
{
    if (callbackFn.isNull()) {
        throw Php::Exception("Consumer callback is not set");
    }

    Message *msg = new Message();
    msg->setMessageHandler(message);

    Php::Value callbackResult = callbackFn(Php::Object("Azure\\uAMQP\\Message", msg));
    if (callbackResult.isBool() && !callbackResult.boolValue()) {
        // The callback asked to stop after this message; keep the current delivery accepted.
        requestStop();
        return false;
    }

    return true;
}

void Consumer::handleLinkDetach(ERROR_HANDLE error)
{
    const char* condition = NULL;
    const char* description = NULL;

    if (error != NULL) {
        error_get_condition(error, &condition);
        error_get_description(error, &description);
    }

    stopRunning = true;

    if (condition != NULL || description != NULL) {
        exceptionMessage += "(" + std::string(condition != NULL ? condition : "unknown") + ") " +
            std::string(description != NULL ? description : "no description");
    }
}

void Consumer::handleCallbackException(const std::string &message)
{
    stopRunning = true;
    exceptionMessage = "Message callback failed: " + message;
}

void Consumer::consume()
{
    if (closeRequested) {
        return;
    }

    if (stopRunning) {
        requestStop();
    } else {
        session->getConnection()->doWork();
    }
}

void Consumer::close()
{
    requestStop();

    if (!exceptionMessage.empty()) {
        throw Php::Exception(exceptionMessage);
    }
}

bool Consumer::wasCloseRequested()
{
    return closeRequested;
}

void Consumer::requestStop()
{
    if (closeRequested) {
        return;
    }

    closeRequested = true;
    stopRunning = true;

    if (message_receiver != NULL) {
        messagereceiver_destroy(message_receiver);
        message_receiver = NULL;
    }

    if (link != NULL) {
        link_destroy(link);
        link = NULL;
    }
}
