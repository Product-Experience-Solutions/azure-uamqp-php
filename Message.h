#ifndef UAMQP_PHP_MESSAGE_H
#define UAMQP_PHP_MESSAGE_H
#include <phpcpp.h>
#include <vector>
#include "azure_uamqp_c/uamqp.h"

class Message : public Php::Base
{
private:
    std::string body;
    std::vector<unsigned char> bodyBytes;
    bool bodyDecoded = false;
    MESSAGE_HANDLE message;
    BINARY_DATA binary_data;

public:
    Message();
    virtual ~Message();
    Message(const Message &other);
    Message& operator=(const Message&) = delete;

    void setMessageHandler(MESSAGE_HANDLE message);
    MESSAGE_HANDLE getMessageHandler();
    void setBody(std::string body);

    void __construct(Php::Parameters &params);
    Php::Value getBody();
    Php::Value getBodyType();
    Php::Value getMessageId();
    void setMessageId(Php::Parameters &params);
    Php::Value getDeliveryCount();
    Php::Value getApplicationProperty(Php::Parameters &params);
    Php::Value getApplicationProperties();
    void setApplicationProperty(Php::Parameters &params);
    Php::Value getMessageAnnotation(Php::Parameters &params);
    Php::Value getMessageAnnotations();
    void setMessageAnnotation(Php::Parameters &params);
};

#endif
