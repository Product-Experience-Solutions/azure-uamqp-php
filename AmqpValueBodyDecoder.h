#ifndef UAMQP_PHP_AMQP_VALUE_BODY_DECODER_H
#define UAMQP_PHP_AMQP_VALUE_BODY_DECODER_H

#include <string>
#include "azure_uamqp_c/uamqp.h"

std::string decode_amqp_value_body(AMQP_VALUE value);

#endif
