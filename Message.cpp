#include "Message.h"
#include "AmqpValueBodyDecoder.h"
#include <exception>

static const char* body_type_name(MESSAGE_BODY_TYPE body_type)
{
    switch (body_type) {
        case MESSAGE_BODY_TYPE_NONE: return "none";
        case MESSAGE_BODY_TYPE_DATA: return "data";
        case MESSAGE_BODY_TYPE_VALUE: return "value";
        case MESSAGE_BODY_TYPE_SEQUENCE: return "sequence";
        default: return "unknown";
    }
}

static std::string decode_amqp_value_body_checked(AMQP_VALUE value)
{
    try {
        return decode_amqp_value_body(value);
    } catch (const std::exception &error) {
        throw Php::Exception(error.what());
    }
}

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

Message::Message() :
    message(NULL),
    application_properties(NULL),
    annotations_map(NULL),
    binary_data{}
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
    if (bodyDecoded) {
        return body;
    }
    if (message == NULL) {
        throw Php::Exception("Message body is not available");
    }

    MESSAGE_BODY_TYPE body_type = MESSAGE_BODY_TYPE_NONE;
    if (message_get_body_type(message, &body_type) != 0) {
        throw Php::Exception("Could not determine AMQP message body type");
    }

    body.clear();
    binary_data = {};

    switch (body_type) {
        case MESSAGE_BODY_TYPE_NONE:
            break;
        case MESSAGE_BODY_TYPE_DATA: {
            size_t data_count = 0;
            if (message_get_body_amqp_data_count(message, &data_count) != 0) {
                throw Php::Exception("Could not determine AMQP data body count");
            }
            for (size_t index = 0; index < data_count; ++index) {
                BINARY_DATA data = {};
                if (message_get_body_amqp_data_in_place(message, index, &data) != 0) {
                    throw Php::Exception("Could not decode AMQP data message body");
                }
                if (data.bytes != NULL && data.length > 0) {
                    body.append(reinterpret_cast<const char*>(data.bytes), data.length);
                }
            }
            break;
        }
        case MESSAGE_BODY_TYPE_VALUE: {
            AMQP_VALUE value = NULL;
            if (message_get_body_amqp_value_in_place(message, &value) != 0 || value == NULL) {
                throw Php::Exception("Could not decode AMQP value message body");
            }
            body = decode_amqp_value_body_checked(value);
            break;
        }
        case MESSAGE_BODY_TYPE_SEQUENCE: {
            size_t sequence_count = 0;
            if (message_get_body_amqp_sequence_count(message, &sequence_count) != 0) {
                throw Php::Exception("Could not determine AMQP sequence body count");
            }
            for (size_t index = 0; index < sequence_count; ++index) {
                AMQP_VALUE sequence = NULL;
                if (message_get_body_amqp_sequence_in_place(message, index, &sequence) != 0 ||
                    sequence == NULL) {
                    throw Php::Exception("Could not decode AMQP sequence message body");
                }
                if (index > 0) {
                    body += "\n";
                }
                body += decode_amqp_value_body_checked(sequence);
            }
            break;
        }
        default:
            throw Php::Exception("Unsupported AMQP message body type");
    }

    bodyDecoded = true;
    return body;
}

Php::Value Message::getBodyType()
{
    if (message == NULL) {
        throw Php::Exception("Message body type is not available");
    }

    MESSAGE_BODY_TYPE body_type = MESSAGE_BODY_TYPE_NONE;
    if (message_get_body_type(message, &body_type) != 0) {
        throw Php::Exception("Could not determine AMQP message body type");
    }

    return body_type_name(body_type);
}

void Message::setBody(std::string body)
{
    this->body = body;
    bodyDecoded = true;

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
    body.clear();
    bodyDecoded = false;
    binary_data = {};
}
