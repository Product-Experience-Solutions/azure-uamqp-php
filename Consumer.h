#ifndef UAMQP_PHP_CONSUMER_H
#define UAMQP_PHP_CONSUMER_H
#include <phpcpp.h>
#include "Session.h"
#include "Message.h"

class Consumer
{
private:
    Session *session = NULL;
    std::string resourceName;
    Php::Value callbackFn;
    bool stopRunning = false;
    std::string exceptionMessage;
    bool closeRequested = false;

    LINK_HANDLE link = NULL;
    MESSAGE_RECEIVER_HANDLE message_receiver = NULL;
    AMQP_VALUE source = NULL;
    AMQP_VALUE target = NULL;

public:
    Consumer(Session *session, std::string resourceName);
    virtual ~Consumer() = default;

    void setCallback(Php::Value &callback, Php::Value &loopFn);
    void consume();
    void close();
    bool wasCloseRequested();
    bool handleMessage(MESSAGE_HANDLE message);
    void handleLinkDetach(ERROR_HANDLE error);
    void handleCallbackException(const std::string &message);

private:
    void requestStop();
};

#endif
