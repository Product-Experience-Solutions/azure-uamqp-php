#include "AmqpValueBodyDecoder.h"
#include <cstdlib>
#include <stdexcept>

std::string decode_amqp_value_body(AMQP_VALUE value)
{
    if (value == NULL) {
        throw std::runtime_error("Could not decode AMQP value message body");
    }

    if (amqpvalue_get_type(value) == AMQP_TYPE_BINARY) {
        amqp_binary binary = {};
        if (amqpvalue_get_binary(value, &binary) != 0 ||
            (binary.bytes == NULL && binary.length > 0)) {
            throw std::runtime_error("Could not decode binary AMQP value message body");
        }

        if (binary.length == 0) {
            return std::string();
        }

        return std::string(static_cast<const char*>(binary.bytes), binary.length);
    }

    char* value_string = amqpvalue_to_string(value);
    if (value_string == NULL) {
        throw std::runtime_error("Could not stringify AMQP value message body");
    }

    std::string result(value_string);
    std::free(value_string);
    return result;
}
