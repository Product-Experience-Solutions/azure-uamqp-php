#include "Consumer.h"
#include "azure_uamqp_c/uamqp.h"
#include "Session.h"
#include "Message.h"
#include <chrono>
#include <exception>
#include <memory>
#include <thread>

namespace
{
    void onLinkDetach(void *context, ERROR_HANDLE error)
    {
        static_cast<Consumer *>(context)->handleLinkDetach(error);
    }

    template<typename Context>
    void onReceiverState(Context context, MESSAGE_RECEIVER_STATE state, MESSAGE_RECEIVER_STATE)
    {
        const_cast<Consumer *>(static_cast<const Consumer *>(context))->handleReceiverState(state);
    }

    AMQP_VALUE onMessage(const void *context, MESSAGE_HANDLE message)
    {
        Consumer *consumer = const_cast<Consumer *>(static_cast<const Consumer *>(context));
        if (consumer == NULL) {
            return messaging_delivery_released();
        }
        // Never unwind a PHP/C++ exception through the C library's callback stack.
        try {
            return consumer->handleMessage(message);
        } catch (Php::Throwable &error) {
            // Keep PHP-CPP's wrapper alive: destroying it here clears Zend's
            // pending exception and loses the original PHP class/code/object.
            consumer->handleCallbackException(error.what(), std::current_exception());
        } catch (const std::exception &error) {
            consumer->handleCallbackException(error.what());
        } catch (...) {
            consumer->handleCallbackException("Unknown message callback exception");
        }
        // Abandon a failed attempt; Rejected would dead-letter a transient failure.
        return messaging_delivery_modified(true, false, NULL);
    }
}

Consumer::Consumer(Session *session, std::string resourceName, uint32_t maxLinkCredit)
    : session(session), resourceName(resourceName),
      creditRemaining(maxLinkCredit == 0 ? 1 : maxLinkCredit), finiteBatch(maxLinkCredit != 0)
{
    AMQP_VALUE source = messaging_create_source(
        ("amqps://" + session->getConnection()->getHost() + "/" + resourceName).c_str());
    AMQP_VALUE target = messaging_create_target("ingress-rx");
    if (source != NULL && target != NULL) {
        link = link_create(session->getSessionHandler(), "receiver-link", role_receiver, source, target);
    }
    amqpvalue_destroy(source);
    amqpvalue_destroy(target);

    if (link == NULL || link_set_rcv_settle_mode(link, receiver_settle_mode_first) != 0 ||
        link_set_receiver_credit(link, creditRemaining) != 0 ||
        link_subscribe_on_link_detach_received(link, onLinkDetach, this) == NULL) {
        destroyHandles();
        throw Php::Exception("Could not configure message receiver credit");
    }
    message_receiver = messagereceiver_create(link, onReceiverState, this);
    if (message_receiver == NULL) {
        destroyHandles();
        throw Php::Exception("Could not create message receiver");
    }
    session->getConnection()->trace("receiver-created", "\"resource\":" + Connection::quote(resourceName) +
        ",\"credit\":" + std::to_string(creditRemaining) + ",\"finite\":" + (finiteBatch ? "true" : "false"));
}

Consumer::~Consumer()
{
    destroyHandles();
}

void Consumer::setCallback(Php::Value &callback, Php::Value &loopFn)
{
    callbackFn = callback;
    if (messagereceiver_open(message_receiver, onMessage, this) != 0) {
        throw Php::Exception("Could not open the message receiver");
    }
    loopFn();
    close();
}

AMQP_VALUE Consumer::handleMessage(MESSAGE_HANDLE message)
{
    if (creditRemaining > 0) {
        --creditRemaining;
    }
    // A single connection_dowork can dispatch more than one already-authorized transfer.
    if (closeRequested) {
        // A PHP Throwable can still be pending while shutdown drains the link.
        // Do not construct PHP objects or invoke PHP conversions for surplus.
        traceDeliveryOutcome("released-unhandled");
        return messaging_delivery_released();
    }
    std::unique_ptr<Message> ownedMessage(new Message());
    ownedMessage->setMessageHandler(message);
    Message *msg = ownedMessage.get();
    Php::Object object("Azure\\uAMQP\\Message", ownedMessage.release());
    if (callbackFn.isNull()) {
        throw Php::Exception("Consumer callback is not set");
    }
    try {
        Php::Value result = callbackFn(object);
        if (result.isBool() && !result.boolValue()) {
            requestStop("callback-stop");
        } else if (finiteBatch && creditRemaining == 0) {
            requestStop("batch-complete");
        }
        traceDelivery(*msg, closeRequested ? "accepted-stop" : "accepted-continue");
        return messaging_delivery_accepted();
    } catch (...) {
        traceDeliveryOutcome("abandoned-error");
        throw;
    }
}

void Consumer::traceDeliveryOutcome(const char *outcome) noexcept
{
    // Failure and drain diagnostics must neither access PHP's pending exception
    // state through metadata conversion nor replace it with a logging failure.
    try {
        Connection *connection = session->getConnection();
        if (!connection->isDebugOn()) {
            return;
        }
        delivery_number deliveryId = 0;
        messagereceiver_get_received_message_id(message_receiver, &deliveryId);
        connection->trace("delivery", "\"resource\":" + Connection::quote(resourceName) +
            ",\"deliveryId\":" + std::to_string(deliveryId) +
            ",\"dispositionAttempt\":" + Connection::quote(outcome));
    } catch (...) {
    }
}

void Consumer::traceDelivery(Message &message, const char *outcome)
{
    Connection *connection = session->getConnection();
    if (!connection->isDebugOn()) {
        return;
    }
    try {
        delivery_number deliveryId = 0;
        messagereceiver_get_received_message_id(message_receiver, &deliveryId);
        Php::Value id = message.getMessageId();
        Php::Value annotations = message.getMessageAnnotations();
        Php::Value sequenceNumber = annotations["x-opt-sequence-number"];
        Php::Value lockedUntil = annotations["x-opt-locked-until"];
        connection->trace("delivery", "\"resource\":" + Connection::quote(resourceName) +
            ",\"deliveryId\":" + std::to_string(deliveryId) +
            ",\"messageId\":" + (id.isNull() ? "null" : Connection::quote(id.stringValue())) +
            ",\"deliveryCount\":" + message.getDeliveryCount().stringValue() +
            ",\"sequenceNumber\":" + Connection::quote(sequenceNumber.stringValue()) +
            ",\"lockedUntil\":" + Connection::quote(lockedUntil.stringValue()) +
            ",\"dispositionAttempt\":" + Connection::quote(outcome));
    } catch (...) {
        connection->trace("metadata-unavailable", "\"dispositionAttempt\":" + Connection::quote(outcome));
    }
}

void Consumer::handleLinkDetach(ERROR_HANDLE error)
{
    detachReceived = true;
    const char *condition = NULL;
    const char *description = NULL;
    if (error != NULL) {
        error_get_condition(error, &condition);
        error_get_description(error, &description);
    }
    if (!closing || condition != NULL || description != NULL) {
        handleCallbackException("Receiver detached: " + std::string(condition == NULL ? "no condition" : condition) +
            " " + std::string(description == NULL ? "" : description));
    }
    requestStop("peer-detach");
}

void Consumer::handleReceiverState(MESSAGE_RECEIVER_STATE state)
{
    receiverState = state;
    if (state == MESSAGE_RECEIVER_STATE_ERROR && !closed) {
        handleCallbackException("AMQP receiver entered an error state");
    }
}

void Consumer::handleCallbackException(const std::string &message, std::exception_ptr error)
{
    if (error && !callbackException) {
        callbackException = error;
    }
    if (exceptionMessage.empty()) {
        exceptionMessage = message;
    }
    requestStop("error");
}

void Consumer::requestStop(const std::string &reason)
{
    if (!closeRequested) {
        stopReason = reason;
    }
    closeRequested = true;
}

void Consumer::consume()
{
    if (!closeRequested) {
        session->getConnection()->doWork();
    }
    if (session->getConnection()->hasIoError()) {
        handleCallbackException("AMQP connection failed while receiving");
    }
    if (closeRequested) {
        close();
    } else if (!finiteBatch && creditRemaining == 0) {
        // Refill after uAMQP has emitted the previous delivery's disposition.
        if (link_set_receiver_credit(link, 1) != 0) {
            handleCallbackException("Could not replenish receiver credit");
            close();
        }
        creditRemaining = 1;
    }
}

void Consumer::close()
{
    requestStop("receive-finished");
    Connection *connection = session->getConnection();
    if (connection->isDoingWork() || closing) {
        return;
    }
    if (!closed) {
        closing = true;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        const auto pump = [&]() {
            connection->doWork();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        };
        while (receiverState == MESSAGE_RECEIVER_STATE_OPENING && !connection->hasIoError() &&
            std::chrono::steady_clock::now() < deadline) {
            pump();
        }
        if (receiverState == MESSAGE_RECEIVER_STATE_OPEN && !detachReceived && !connection->hasIoError()) {
            bool drained = false;
            if (link_drain(link) == 0) {
                while (!detachReceived && !connection->hasIoError() &&
                    std::chrono::steady_clock::now() < deadline) {
                    if (link_get_drain_complete(link, &drained) != 0 || drained) {
                        break;
                    }
                    pump();
                }
            }
            connection->trace("receiver-drain", "\"complete\":" + std::string(drained ? "true" : "false"));
            if (!drained && !detachReceived) {
                handleCallbackException("Receiver drain did not complete before shutdown");
            }
        }
        if (message_receiver != NULL && !detachReceived && !connection->hasIoError()) {
            if (messagereceiver_close(message_receiver) != 0) {
                handleCallbackException("Could not close AMQP receiver");
            }
            // Even a timed-out drain must leave time to transmit DETACH and read its reply.
            const auto detachDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            while (!detachReceived && !connection->hasIoError() &&
                std::chrono::steady_clock::now() < detachDeadline) {
                pump();
            }
            if (!detachReceived) {
                handleCallbackException("AMQP receiver shutdown timed out");
            }
        }
        connection->trace("receiver-closed", "\"reason\":" + Connection::quote(stopReason) +
            ",\"peerDetach\":" + (detachReceived ? "true" : "false") +
            ",\"error\":" + Connection::quote(exceptionMessage));
        destroyHandles();
        closed = true;
        closing = false;
    }
    throwPendingException();
}

void Consumer::throwPendingException()
{
    if (callbackException) {
        std::exception_ptr error = callbackException;
        callbackException = nullptr;
        exceptionMessage.clear();
        // This is outside uAMQP dispatch. PHP-CPP can now rethrow the original
        // pending Throwable into PHP after settlement and bounded cleanup.
        std::rethrow_exception(error);
    }
    if (!exceptionMessage.empty()) {
        std::string error = exceptionMessage;
        exceptionMessage.clear();
        throw Php::Exception(error);
    }
}

bool Consumer::wasCloseRequested()
{
    return closeRequested;
}

void Consumer::destroyHandles()
{
    if (message_receiver != NULL) {
        messagereceiver_destroy(message_receiver);
        message_receiver = NULL;
    }
    if (link != NULL) {
        link_destroy(link);
        link = NULL;
    }
}
