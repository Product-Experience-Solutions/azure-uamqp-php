#include "AmqpValueDecoder.h"
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

static void check_decode(int result)
{
    if (result != 0) {
        throw std::runtime_error("Could not decode AMQP metadata value");
    }
}

template<typename T>
static DecodedAmqpValue decode_integer(AMQP_VALUE value, int (*getter)(AMQP_VALUE, T*))
{
    T integer;
    check_decode(getter(value, &integer));
    DecodedAmqpValue result;
    result.type = DecodedAmqpValue::Type::Integer;
    result.integer = static_cast<int64_t>(integer);
    return result;
}

DecodedAmqpValue decode_amqp_value(AMQP_VALUE value, unsigned int depth)
{
    if (depth > 32) {
        throw std::runtime_error("AMQP metadata nesting exceeds 32 levels");
    }
    DecodedAmqpValue result;
    if (value == NULL) {
        return result;
    }
    switch (amqpvalue_get_type(value)) {
        case AMQP_TYPE_NULL:
            break;
        case AMQP_TYPE_BOOL:
            result.type = DecodedAmqpValue::Type::Boolean;
            check_decode(amqpvalue_get_boolean(value, &result.boolean));
            break;
        case AMQP_TYPE_BYTE: return decode_integer(value, amqpvalue_get_byte);
        case AMQP_TYPE_UBYTE: return decode_integer(value, amqpvalue_get_ubyte);
        case AMQP_TYPE_SHORT: return decode_integer(value, amqpvalue_get_short);
        case AMQP_TYPE_USHORT: return decode_integer(value, amqpvalue_get_ushort);
        case AMQP_TYPE_INT: return decode_integer(value, amqpvalue_get_int);
        case AMQP_TYPE_UINT: return decode_integer(value, amqpvalue_get_uint);
        case AMQP_TYPE_LONG: return decode_integer(value, amqpvalue_get_long);
        case AMQP_TYPE_CHAR: return decode_integer(value, amqpvalue_get_char);
        case AMQP_TYPE_TIMESTAMP: return decode_integer(value, amqpvalue_get_timestamp);
        case AMQP_TYPE_ULONG: {
            uint64_t integer;
            check_decode(amqpvalue_get_ulong(value, &integer));
            if (integer > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                result.type = DecodedAmqpValue::Type::String;
                result.text = std::to_string(integer);
            } else {
                result.type = DecodedAmqpValue::Type::Integer;
                result.integer = static_cast<int64_t>(integer);
            }
            break;
        }
        case AMQP_TYPE_FLOAT: {
            float number;
            check_decode(amqpvalue_get_float(value, &number));
            result.type = DecodedAmqpValue::Type::Double;
            result.number = number;
            break;
        }
        case AMQP_TYPE_DOUBLE:
            result.type = DecodedAmqpValue::Type::Double;
            check_decode(amqpvalue_get_double(value, &result.number));
            break;
        case AMQP_TYPE_STRING:
        case AMQP_TYPE_SYMBOL: {
            const char* text = NULL;
            check_decode(amqpvalue_get_type(value) == AMQP_TYPE_STRING
                ? amqpvalue_get_string(value, &text) : amqpvalue_get_symbol(value, &text));
            result.type = DecodedAmqpValue::Type::String;
            result.text = text != NULL ? text : "";
            break;
        }
        case AMQP_TYPE_BINARY: {
            amqp_binary binary = {};
            check_decode(amqpvalue_get_binary(value, &binary));
            result.type = DecodedAmqpValue::Type::String;
            if (binary.length > 0) {
                if (binary.bytes == NULL) {
                    throw std::runtime_error("AMQP binary metadata has no bytes");
                }
                result.text.assign(static_cast<const char*>(binary.bytes), binary.length);
            }
            break;
        }
        case AMQP_TYPE_UUID: {
            uuid bytes;
            check_decode(amqpvalue_get_uuid(value, &bytes));
            std::ostringstream text;
            text << std::hex << std::setfill('0');
            for (size_t index = 0; index < sizeof(bytes); ++index) {
                if (index == 4 || index == 6 || index == 8 || index == 10) {
                    text << '-';
                }
                text << std::setw(2) << static_cast<unsigned int>(bytes[index]);
            }
            result.type = DecodedAmqpValue::Type::String;
            result.text = text.str();
            break;
        }
        case AMQP_TYPE_DESCRIBED: {
            AMQP_VALUE inner = amqpvalue_get_inplace_described_value(value);
            if (inner == NULL) {
                throw std::runtime_error("AMQP metadata has no described value");
            }
            return decode_amqp_value(inner, depth + 1);
        }
        case AMQP_TYPE_MAP: {
            result.type = DecodedAmqpValue::Type::Map;
            uint32_t count;
            check_decode(amqpvalue_get_map_pair_count(value, &count));
            for (uint32_t index = 0; index < count; ++index) {
                AMQP_VALUE key = NULL;
                AMQP_VALUE item = NULL;
                check_decode(amqpvalue_get_map_key_value_pair(value, index, &key, &item));
                OwnedAmqpValue ownedKey(key, amqpvalue_destroy);
                OwnedAmqpValue ownedItem(item, amqpvalue_destroy);
                const DecodedAmqpValue decodedKey = decode_amqp_value(key, depth + 1);
                std::string name;
                if (decodedKey.type == DecodedAmqpValue::Type::String) {
                    name = decodedKey.text;
                } else if (decodedKey.type == DecodedAmqpValue::Type::Integer) {
                    name = std::to_string(decodedKey.integer);
                } else {
                    throw std::runtime_error("AMQP metadata map key must be a string or integer");
                }
                result.map[name] = decode_amqp_value(item, depth + 1);
            }
            break;
        }
        case AMQP_TYPE_LIST:
        case AMQP_TYPE_ARRAY: {
            const bool array = amqpvalue_get_type(value) == AMQP_TYPE_ARRAY;
            result.type = DecodedAmqpValue::Type::List;
            uint32_t count;
            check_decode(array ? amqpvalue_get_array_item_count(value, &count)
                : amqpvalue_get_list_item_count(value, &count));
            for (uint32_t index = 0; index < count; ++index) {
                OwnedAmqpValue item(array ? amqpvalue_get_array_item(value, index)
                    : amqpvalue_get_list_item(value, index), amqpvalue_destroy);
                if (!item) {
                    throw std::runtime_error("Could not read AMQP metadata list item");
                }
                result.list.push_back(decode_amqp_value(item.get(), depth + 1));
            }
            break;
        }
        default:
            throw std::runtime_error("Unsupported AMQP metadata type");
    }
    return result;
}
