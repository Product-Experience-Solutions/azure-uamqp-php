#include "AmqpValueBodyDecoder.h"
#include <cstdlib>
#include <iostream>
#include <string>

static bool assert_binary_body(const unsigned char* bytes, size_t length)
{
    amqp_binary binary = {bytes, static_cast<uint32_t>(length)};
    AMQP_VALUE value = amqpvalue_create_binary(binary);
    if (value == NULL) {
        std::cerr << "Could not create binary AMQP value" << std::endl;
        return false;
    }

    const std::string expected(reinterpret_cast<const char*>(bytes), length);
    const std::string actual = decode_amqp_value_body(value);
    amqpvalue_destroy(value);

    if (actual != expected) {
        std::cerr << "Decoded AMQP binary value does not match its source bytes" << std::endl;
        return false;
    }

    return true;
}

static bool assert_non_binary_body_is_stringified()
{
    AMQP_VALUE value = amqpvalue_create_string("text");
    if (value == NULL) {
        std::cerr << "Could not create string AMQP value" << std::endl;
        return false;
    }

    char* expected_value = amqpvalue_to_string(value);
    if (expected_value == NULL) {
        amqpvalue_destroy(value);
        std::cerr << "Could not stringify string AMQP value" << std::endl;
        return false;
    }

    const std::string expected(expected_value);
    std::free(expected_value);
    const std::string actual = decode_amqp_value_body(value);
    amqpvalue_destroy(value);

    if (actual != expected) {
        std::cerr << "Non-binary AMQP value stringification changed" << std::endl;
        return false;
    }

    return true;
}

int main()
{
    const unsigned char json[] = {'{', '"', 'i', 'd', '"', ':', '1', '}'};
    if (!assert_binary_body(json, sizeof(json))) {
        return 1;
    }

    const unsigned char binary_with_null[] = {'a', 0, 'b'};
    if (!assert_binary_body(binary_with_null, sizeof(binary_with_null))) {
        return 1;
    }

    if (!assert_non_binary_body_is_stringified()) {
        return 1;
    }

    std::cout << "AMQP value body decoder tests passed" << std::endl;
    return 0;
}
