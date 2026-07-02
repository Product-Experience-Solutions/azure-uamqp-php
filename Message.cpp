#include "Message.h"
static void add_map_item(AMQP_VALUE map, const char* name, AMQP_VALUE amqp_value_value)
{
    AMQP_VALUE amqp_value_name = amqpvalue_create_symbol(name);
    amqpvalue_set_map_value(map, amqp_value_name, amqp_value_value);
    amqpvalue_destroy(amqp_value_value);
    amqpvalue_destroy(amqp_value_name);
}
static void add_map_string(AMQP_VALUE map, const char* name, const char* value)
{
    AMQP_VALUE amqp_value_value = amqpvalue_create_string(value);
    add_map_item(map, name, amqp_value_value);
}
static void add_map_timestamp(AMQP_VALUE map, const char* name, int64_t value)
{
    AMQP_VALUE amqp_value_value = amqpvalue_create_timestamp(value);
    add_map_item(map, name, amqp_value_value);
}
static void add_map_int(AMQP_VALUE map, const char* name, int32_t value)
{
    AMQP_VALUE amqp_value_value = amqpvalue_create_int(value);
    add_map_item(map, name, amqp_value_value);
}
static void add_map_double(AMQP_VALUE map, const char* name, double value)
{
    AMQP_VALUE amqp_value_value = amqpvalue_create_double(value);
    add_map_item(map, name, amqp_value_value);
}
static void add_map_value(AMQP_VALUE map, const char* key, const char type, Php::Value value)
{
    switch (type) {
        case 'I':
            add_map_int(map, key, static_cast<int32_t>(value));
            break;
        case 'S':
            add_map_string(map, key, value.stringValue().c_str());
            break;
        case 'T':
            add_map_timestamp(map, key, static_cast<int64_t>(value));
            break;
        case 'D':
            add_map_double(map, key, static_cast<double>(value));
            break;
    }
}

static void add_amqp_message_annotation(MESSAGE_HANDLE message, AMQP_VALUE msg_annotations_map)
{
    AMQP_VALUE msg_annotations;
    msg_annotations = amqpvalue_create_message_annotations(msg_annotations_map);
    message_set_message_annotations(message, (annotations)msg_annotations);
    annotations_destroy(msg_annotations);
}

Message::Message()
{
    message = message_create();

    application_properties = amqpvalue_create_map();
    annotations_map = amqpvalue_create_map();

    message_set_application_properties(message, application_properties);
    add_amqp_message_annotation(message, annotations_map);
}

void Message::__construct(Php::Parameters &params)
{
    setBody(params[0].stringValue());
}

Php::Value Message::getBody()
{
    if (body.empty()) {
        message_get_body_amqp_data_in_place(message, 0, &binary_data);
        for (size_t i = 0; i < binary_data.length; ++i) {
            body += binary_data.bytes[i];
        }
    }

    return body;
}

void Message::setBody(std::string body)
{
    this->body = body;

    bodyBytes.assign(body.begin(), body.end());
    binary_data.bytes = bodyBytes.empty() ? NULL : bodyBytes.data();
    binary_data.length = bodyBytes.size();
    message_add_body_amqp_data(message, binary_data);
}

Php::Value Message::getApplicationProperty(Php::Parameters &params)
{
    (void)params;
    return Php::Value();
}

Php::Value Message::getApplicationProperties()
{
    return Php::Array();
}

Php::Value Message::getMessageAnnotation(Php::Parameters &params)
{
    (void)params;
    return Php::Value();
}

void Message::setApplicationProperty(Php::Parameters &params)
{
    add_map_value(application_properties, params[0].stringValue().c_str(), params[1].stringValue().at(0), params[2]);
}

void Message::setMessageAnnotation(Php::Parameters &params)
{
    add_map_value(annotations_map, params[0].stringValue().c_str(), params[1].stringValue().at(0), params[2]);
}

MESSAGE_HANDLE Message::getMessageHandler()
{
    return message;
}

void Message::setMessageHandler(MESSAGE_HANDLE message)
{
    this->message = message;
}
