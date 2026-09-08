#ifndef UAMQP_PHP_CONSUMER_H
#define UAMQP_PHP_CONSUMER_H
#include <phpcpp.h>
#include <exception>
#include "Session.h"
#include "Message.h"

class Consumer
{
private:
    Session *session = NULL;
    std::string resourceName;
    Php::Value callbackFn;
    std::string exceptionMessage;
    std::exception_ptr callbackException;
    bool closeRequested = false;
    bool closed = false;
    bool closing = false;
    bool detachReceived = false;
    MESSAGE_RECEIVER_STATE receiverState = MESSAGE_RECEIVER_STATE_IDLE;
    uint32_t creditRemaining;
    bool finiteBatch;
    std::string stopReason;

    LINK_HANDLE link = NULL;
    MESSAGE_RECEIVER_HANDLE message_receiver = NULL;

public:
    Consumer(Session *session, std::string resourceName, uint32_t maxLinkCredit = 0);
    ~Consumer();

    void setCallback(Php::Value &callback, Php::Value &loopFn);
    void consume();
    void close();
    bool wasCloseRequested();
    AMQP_VALUE handleMessage(MESSAGE_HANDLE message);
    void handleLinkDetach(ERROR_HANDLE error);
    void handleCallbackException(const std::string &message, std::exception_ptr error = nullptr);
    void handleReceiverState(MESSAGE_RECEIVER_STATE state);
    void requestStop(const std::string &reason = "close-requested");

private:
    void destroyHandles();
    void throwPendingException();
    void traceDeliveryOutcome(const char *outcome) noexcept;
    void traceDelivery(Message &message, const char *outcome);
};

#endif
