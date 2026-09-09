#ifndef UAMQP_PHP_AMQP_VALUE_DECODER_H
#define UAMQP_PHP_AMQP_VALUE_DECODER_H

#include "azure_uamqp_c/amqpvalue.h"
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

using OwnedAmqpValue = std::unique_ptr<std::remove_pointer<AMQP_VALUE>::type, decltype(&amqpvalue_destroy)>;

// An owned, PHP-independent representation used by metadata getters and native tests.
struct DecodedAmqpValue
{
    enum class Type { Null, Boolean, Integer, Double, String, Map, List };
    Type type = Type::Null;
    bool boolean = false;
    int64_t integer = 0;
    double number = 0;
    std::string text;
    std::map<std::string, DecodedAmqpValue> map;
    std::vector<DecodedAmqpValue> list;
};

// Borrows value. Binary lengths and unsigned integers outside the PHP integer range
// are preserved (the latter as decimal strings); timestamps remain milliseconds.
DecodedAmqpValue decode_amqp_value(AMQP_VALUE value, unsigned int depth = 0);

#endif
