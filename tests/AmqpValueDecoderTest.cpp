#include "AmqpValueDecoder.h"
#include "azure_uamqp_c/uamqp.h"
#include <iostream>
#include <limits>
#include <stdexcept>

static void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

static DecodedAmqpValue decode_owned(AMQP_VALUE value)
{
    OwnedAmqpValue owned(value, amqpvalue_destroy);
    require(value != NULL, "Could not create test value");
    return decode_amqp_value(value);
}

static void set_item(AMQP_VALUE map, AMQP_VALUE key, AMQP_VALUE value)
{
    OwnedAmqpValue ownedKey(key, amqpvalue_destroy);
    OwnedAmqpValue ownedValue(value, amqpvalue_destroy);
    require(key != NULL && value != NULL && amqpvalue_set_map_value(map, key, value) == 0,
        "Could not build test metadata map");
}

static void test_scalar_values()
{
    require(decode_amqp_value(NULL).type == DecodedAmqpValue::Type::Null, "Missing values must be null");
    require(decode_owned(amqpvalue_create_null()).type == DecodedAmqpValue::Type::Null, "Null value changed");
    require(decode_owned(amqpvalue_create_boolean(true)).boolean, "Boolean value changed");
    require(decode_owned(amqpvalue_create_byte(-5)).integer == -5, "Byte value changed");
    require(decode_owned(amqpvalue_create_int(-54321)).integer == -54321, "Integer value changed");
    require(decode_owned(amqpvalue_create_uint(4294967295U)).integer == 4294967295LL, "Unsigned int overflow");
    require(decode_owned(amqpvalue_create_long(-9000000000000LL)).integer == -9000000000000LL,
        "Long value changed");
    require(decode_owned(amqpvalue_create_timestamp(1770000000123LL)).integer == 1770000000123LL,
        "Timestamp must retain millisecond precision");
    require(decode_owned(amqpvalue_create_double(1.25)).number == 1.25, "Double value changed");
    require(decode_owned(amqpvalue_create_string("message-id")).text == "message-id", "String changed");
    require(decode_owned(amqpvalue_create_symbol("x-opt-sequence-number")).text == "x-opt-sequence-number",
        "Symbol changed");
    const uint64_t maxSigned = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    require(decode_owned(amqpvalue_create_ulong(maxSigned)).integer == std::numeric_limits<int64_t>::max(),
        "Largest signed integer changed");
    require(decode_owned(amqpvalue_create_ulong(maxSigned + 1)).text == "9223372036854775808",
        "Unsigned integer overflow must preserve decimal representation");
    require(decode_owned(amqpvalue_create_ulong(std::numeric_limits<uint64_t>::max())).text == "18446744073709551615",
        "Largest unsigned integer changed");

    const unsigned char bytes[] = {'a', 0, 'b'};
    amqp_binary binary = {bytes, sizeof(bytes)};
    require(decode_owned(amqpvalue_create_binary(binary)).text == std::string("a\0b", 3),
        "Binary metadata must retain embedded NUL bytes");
    amqp_binary empty = {NULL, 0};
    require(decode_owned(amqpvalue_create_binary(empty)).text.empty(), "Empty binary metadata changed");
    uuid id = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    require(decode_owned(amqpvalue_create_uuid(id)).text == "00112233-4455-6677-8899-aabbccddeeff",
        "UUID must use its standard string representation");
}

static void test_described_maps_and_lifetime()
{
    OwnedAmqpValue map(amqpvalue_create_map(), amqpvalue_destroy);
    set_item(map.get(), amqpvalue_create_string("active"), amqpvalue_create_boolean(true));
    set_item(map.get(), amqpvalue_create_symbol("x-opt-sequence-number"), amqpvalue_create_long(1234567890123LL));
    set_item(map.get(), amqpvalue_create_ulong(123), amqpvalue_create_string("numeric-key"));
    OwnedAmqpValue list(amqpvalue_create_list(), amqpvalue_destroy);
    OwnedAmqpValue listItem(amqpvalue_create_string("child"), amqpvalue_destroy);
    require(amqpvalue_set_list_item_count(list.get(), 1) == 0 &&
        amqpvalue_set_list_item(list.get(), 0, listItem.get()) == 0, "Could not build list");
    set_item(map.get(), amqpvalue_create_string("list"), amqpvalue_clone(list.get()));
    OwnedAmqpValue section(amqpvalue_create_application_properties(map.get()), amqpvalue_destroy);
    const DecodedAmqpValue decoded = decode_amqp_value(section.get());
    map.reset();
    section.reset();
    list.reset();
    listItem.reset();
    require(decoded.map.at("active").boolean, "Map boolean changed");
    require(decoded.map.at("x-opt-sequence-number").integer == 1234567890123LL, "Annotation changed");
    require(decoded.map.at("123").text == "numeric-key", "Numeric annotation key changed");
    require(decoded.map.at("list").list.at(0).text == "child", "Nested values must outlive AMQP handles");

    OwnedAmqpValue array(amqpvalue_create_array(), amqpvalue_destroy);
    OwnedAmqpValue item(amqpvalue_create_uint(42), amqpvalue_destroy);
    require(amqpvalue_add_array_item(array.get(), item.get()) == 0, "Could not build array");
    require(decode_amqp_value(array.get()).list.at(0).integer == 42, "Array metadata changed");
}

static void test_nesting_limit()
{
    OwnedAmqpValue value(amqpvalue_create_string("nested"), amqpvalue_destroy);
    OwnedAmqpValue descriptor(amqpvalue_create_ulong(1), amqpvalue_destroy);
    for (int index = 0; index < 34; ++index) {
        value.reset(amqpvalue_create_described(descriptor.get(), value.get()));
    }
    bool rejected = false;
    try {
        decode_amqp_value(value.get());
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected, "Overly nested metadata must be rejected");
}

int main()
{
    try {
        test_scalar_values();
        test_described_maps_and_lifetime();
        test_nesting_limit();
        std::cout << "AMQP metadata decoder tests passed" << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
