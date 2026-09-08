#include "Message.h"
#include "AmqpValueBodyDecoder.h"
#include "AmqpValueDecoder.h"
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

static Php::Value to_php_value(const DecodedAmqpValue &value)
{
    switch (value.type) {
        case DecodedAmqpValue::Type::Null: return Php::Value();
        case DecodedAmqpValue::Type::Boolean: return value.boolean;
        case DecodedAmqpValue::Type::Integer: return value.integer;
        case DecodedAmqpValue::Type::Double: return value.number;
        case DecodedAmqpValue::Type::String: return value.text;
        case DecodedAmqpValue::Type::Map: {
            Php::Value result = Php::Array();
            for (const auto &item : value.map) {
                result[item.first] = to_php_value(item.second);
            }
            return result;
        }
        case DecodedAmqpValue::Type::List: {
            Php::Value result = Php::Array();
            int index = 0;
            for (const auto &item : value.list) {
                result[index++] = to_php_value(item);
            }
            return result;
        }
    }
    throw Php::Exception("Unsupported decoded AMQP metadata type");
}

static Php::Value decode_metadata(AMQP_VALUE value)
{
    try {
        return to_php_value(decode_amqp_value(value));
    } catch (const std::exception &error) {
        throw Php::Exception(error.what());
    }
}

static OwnedAmqpValue get_metadata(MESSAGE_HANDLE message, bool annotation)
{
    AMQP_VALUE value = NULL;
    const int result = annotation ? message_get_message_annotations(message, &value)
        : message_get_application_properties(message, &value);
    if (result != 0) {
        throw Php::Exception("Could not read AMQP message metadata");
    }
    return OwnedAmqpValue(value, amqpvalue_destroy);
}

static AMQP_VALUE get_metadata_map(AMQP_VALUE value)
{
    unsigned int depth = 0;
    while (value != NULL && amqpvalue_get_type(value) == AMQP_TYPE_DESCRIBED && depth++ < 32) {
        value = amqpvalue_get_inplace_described_value(value);
    }
    if (value == NULL || amqpvalue_get_type(value) != AMQP_TYPE_MAP) {
        throw Php::Exception("AMQP message metadata must contain a map");
    }
    return value;
}

// uAMQP's clone increments a reference count. Copy the container before changing
// an entry so setters do not change another PHP clone or the borrowed delivery.
static OwnedAmqpValue copy_metadata_container(AMQP_VALUE source, AMQP_VALUE replacedKey = NULL)
{
    const bool map = amqpvalue_get_type(source) == AMQP_TYPE_MAP;
    uint32_t count = 0;
    if ((map ? amqpvalue_get_map_pair_count(source, &count)
        : amqpvalue_get_composite_item_count(source, &count)) != 0) {
        throw Php::Exception("Could not read AMQP metadata container");
    }
    OwnedAmqpValue result(map ? amqpvalue_create_map()
        : amqpvalue_create_composite(amqpvalue_get_inplace_descriptor(source), count), amqpvalue_destroy);
    if (!result) {
        throw Php::Exception("Could not copy AMQP metadata container");
    }
    for (uint32_t index = 0; index < count; ++index) {
        int status;
        if (map) {
            AMQP_VALUE key = NULL;
            AMQP_VALUE value = NULL;
            if (amqpvalue_get_map_key_value_pair(source, index, &key, &value) != 0) {
                throw Php::Exception("Could not read AMQP metadata entry");
            }
            OwnedAmqpValue ownedKey(key, amqpvalue_destroy);
            OwnedAmqpValue ownedValue(value, amqpvalue_destroy);
            if (replacedKey != NULL && amqpvalue_are_equal(key, replacedKey)) {
                continue;
            }
            status = amqpvalue_set_map_value(result.get(), key, value);
        } else {
            status = amqpvalue_set_composite_item(result.get(), index,
                amqpvalue_get_composite_item_in_place(source, index));
        }
        if (status != 0) {
            throw Php::Exception("Could not copy AMQP metadata entry");
        }
    }
    return result;
}

static Php::Value apply_requested_type(Php::Value value, Php::Parameters &params)
{
    if (value.isNull() || params.size() < 2 || params[1].stringValue().empty()) {
        return value;
    }
    const std::string type = params[1].stringValue();
    if (type == "I" || type == "T" || type == "L") {
        return value.numericValue();
    }
    if (type == "S") {
        return value.stringValue();
    }
    if (type == "D") {
        return value.floatValue();
    }
    if (type == "B") {
        return value.boolValue();
    }
    throw Php::Exception("Unsupported AMQP metadata type; expected I, S, T, D, L or B");
}

static void set_metadata(MESSAGE_HANDLE message, bool annotation, Php::Parameters &params)
{
    OwnedAmqpValue metadata = get_metadata(message, annotation);
    if (!metadata) {
        metadata.reset(amqpvalue_create_map());
    }
    const std::string key = params[0].stringValue();
    OwnedAmqpValue name(annotation ? amqpvalue_create_symbol(key.c_str())
        : amqpvalue_create_string(key.c_str()), amqpvalue_destroy);
    OwnedAmqpValue value(NULL, amqpvalue_destroy);
    const std::string type = params[1].stringValue();
    if (type == "I") {
        value.reset(amqpvalue_create_int(static_cast<int32_t>(params[2])));
    } else if (type == "S") {
        value.reset(amqpvalue_create_string(params[2].stringValue().c_str()));
    } else if (type == "T") {
        value.reset(amqpvalue_create_timestamp(static_cast<int64_t>(params[2])));
    } else if (type == "D") {
        value.reset(amqpvalue_create_double(static_cast<double>(params[2])));
    } else if (type == "L") {
        value.reset(amqpvalue_create_long(static_cast<int64_t>(params[2])));
    } else if (type == "B") {
        value.reset(amqpvalue_create_boolean(params[2].boolValue()));
    } else {
        throw Php::Exception("Unsupported AMQP metadata type; expected I, S, T, D, L or B");
    }
    if (!name || !value) {
        throw Php::Exception("Could not create AMQP message metadata");
    }
    OwnedAmqpValue updatedMap = copy_metadata_container(get_metadata_map(metadata.get()), name.get());
    AMQP_VALUE map = updatedMap.get();
    if (amqpvalue_set_map_value(map, name.get(), value.get()) != 0) {
        throw Php::Exception("Could not set AMQP message metadata");
    }
    // The sender wraps application properties itself, but encodes annotations as
    // supplied. Keep exactly one annotations section around the underlying map.
    OwnedAmqpValue section(annotation ? amqpvalue_create_message_annotations(map) : NULL, amqpvalue_destroy);
    if (annotation && !section) {
        throw Php::Exception("Could not create AMQP message annotations section");
    }
    const int result = annotation ? message_set_message_annotations(message, section.get())
        : message_set_application_properties(message, map);
    if (result != 0) {
        throw Php::Exception("Could not store AMQP message metadata");
    }
}

Message::Message() :
    message(NULL),
    binary_data{}
{
    message = message_create();
    if (message == NULL) {
        throw Php::Exception("Could not create AMQP message");
    }
}

Message::~Message()
{
    if (message != NULL) {
        message_destroy(message);
    }
}

Message::Message(const Message &other) : Message()
{
    setMessageHandler(other.message);
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
    Php::Value values = getApplicationProperties();
    return apply_requested_type(values[params[0].stringValue()], params);
}

Php::Value Message::getApplicationProperties()
{
    OwnedAmqpValue value = get_metadata(message, false);
    return value ? decode_metadata(value.get()) : Php::Array();
}

Php::Value Message::getMessageAnnotation(Php::Parameters &params)
{
    Php::Value values = getMessageAnnotations();
    return apply_requested_type(values[params[0].stringValue()], params);
}

Php::Value Message::getMessageAnnotations()
{
    OwnedAmqpValue value = get_metadata(message, true);
    return value ? decode_metadata(value.get()) : Php::Array();
}

Php::Value Message::getMessageId()
{
    PROPERTIES_HANDLE properties = NULL;
    if (message_get_properties(message, &properties) != 0) {
        throw Php::Exception("Could not read AMQP message properties");
    }
    std::unique_ptr<std::remove_pointer<PROPERTIES_HANDLE>::type, decltype(&properties_destroy)>
        ownedProperties(properties, properties_destroy);
    AMQP_VALUE id = NULL;
    if (properties == NULL || properties_get_message_id(properties, &id) != 0) {
        return Php::Value();
    }
    // properties_get_message_id returns a borrowed value owned by properties.
    return decode_metadata(id);
}

void Message::setMessageId(Php::Parameters &params)
{
    PROPERTIES_HANDLE properties = NULL;
    if (message_get_properties(message, &properties) != 0) {
        throw Php::Exception("Could not read AMQP message properties");
    }
    std::unique_ptr<std::remove_pointer<PROPERTIES_HANDLE>::type, decltype(&properties_destroy)>
        ownedProperties(properties != NULL ? properties : properties_create(), properties_destroy);
    if (properties != NULL) {
        OwnedAmqpValue section(amqpvalue_create_properties(properties), amqpvalue_destroy);
        if (!section) {
            throw Php::Exception("Could not read AMQP properties section");
        }
        OwnedAmqpValue updated = copy_metadata_container(section.get());
        PROPERTIES_HANDLE independentProperties = NULL;
        if (amqpvalue_get_properties(updated.get(), &independentProperties) != 0) {
            throw Php::Exception("Could not copy AMQP message properties");
        }
        ownedProperties.reset(independentProperties);
    }
    OwnedAmqpValue id(amqpvalue_create_string(params[0].stringValue().c_str()), amqpvalue_destroy);
    if (!ownedProperties || !id || properties_set_message_id(ownedProperties.get(), id.get()) != 0 ||
        message_set_properties(message, ownedProperties.get()) != 0) {
        throw Php::Exception("Could not set AMQP message ID");
    }
}

Php::Value Message::getDeliveryCount()
{
    HEADER_HANDLE header = NULL;
    if (message_get_header(message, &header) != 0) {
        throw Php::Exception("Could not read AMQP message header");
    }
    std::unique_ptr<std::remove_pointer<HEADER_HANDLE>::type, decltype(&header_destroy)>
        ownedHeader(header, header_destroy);
    uint32_t count = 0;
    if (header != NULL && header_get_delivery_count(header, &count) != 0) {
        throw Php::Exception("Could not read AMQP message delivery count");
    }
    return static_cast<int64_t>(count);
}

void Message::setApplicationProperty(Php::Parameters &params)
{
    set_metadata(message, false, params);
}

void Message::setMessageAnnotation(Php::Parameters &params)
{
    set_metadata(message, true, params);
}

MESSAGE_HANDLE Message::getMessageHandler()
{
    return message;
}

void Message::setMessageHandler(MESSAGE_HANDLE message)
{
    MESSAGE_HANDLE clone = message_clone(message);
    if (clone == NULL) {
        throw Php::Exception("Could not retain received AMQP message");
    }
    message_destroy(this->message);
    this->message = clone;
    body.clear();
    bodyDecoded = false;
    binary_data = {};
}
