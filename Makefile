.RECIPEPREFIX +=
.PHONY: $(filter-out compile, $(MAKECMDGOALS))

NAME          = uamqpphpbinding
INI_DIR       = $(shell php -i | grep -i "additional .ini files" | head -n 1 | cut -f2 -d'>' | xargs)
EXTENSION_DIR = $(shell php-config --extension-dir)
EXTENSION     = ${NAME}.so
INI           = ${NAME}.ini

override PHUAMQP_VERSION := $(shell bash scripts/version.sh)
ifeq ($(PHUAMQP_VERSION),)
$(error Could not read a valid extension version from VERSION)
endif

COMPILER            = g++
LINKER              = g++
COMPILER_FLAGS      = -Wall -c -O2 -std=c++11 -fpic -MMD -MP -I/usr/local/include -I/usr/local/include/c_logging/v2 -I/usr/local/include/azureiot -I/usr/local/include/macro_utils -I/usr/local/include/umock_c -o
LINKER_FLAGS        = -shared -L/usr/local/lib
LINKER_DEPENDENCIES = -lphpcpp -luamqp -laziotsharedutil -luuid
TEST_LINKER_DEPENDENCIES = -luamqp -laziotsharedutil -luuid

RM =   rm -f
CP =   cp -f

SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:%.cpp=%.o)
DEPENDENCIES = $(OBJECTS:%.o=%.d)
TEST_BINARY = tests/amqp-value-body-decoder-test
TEST_METADATA_BINARY = tests/amqp-value-decoder-test
TEST_CREDIT_BINARY = tests/link-receiver-credit-test
UAMQP_SOURCE_DIR ?= $(if $(PHUAMQP_LIBS_BUILD_DIR),$(PHUAMQP_LIBS_BUILD_DIR),libs-build)/azure-uamqp-c

# =-=-=
# Tasks
# =-=-=

all: ${OBJECTS} ${EXTENSION} ## compile extension where u're running make

${EXTENSION}: ${OBJECTS}
	${LINKER} ${LINKER_FLAGS} -o $@ ${OBJECTS} ${LINKER_DEPENDENCIES}

${OBJECTS}: %.o: %.cpp
	${COMPILER} ${CPPFLAGS} ${COMPILER_FLAGS} $@ $<

main.o: VERSION scripts/version.sh
main.o: override CPPFLAGS += -DPHUAMQP_VERSION='"${PHUAMQP_VERSION}"'

test: ${TEST_BINARY} ${TEST_METADATA_BINARY} ## run native regression tests
	./${TEST_BINARY}
	./${TEST_METADATA_BINARY}

test-versioning: ## verify native and package versions, including incremental builds
	bash tests/versioning-test.sh

test-credit: ${TEST_CREDIT_BINARY} ## verify manual receiver credit and drain in patched uAMQP
	./${TEST_CREDIT_BINARY}
	UAMQP_SOURCE_DIR="${UAMQP_SOURCE_DIR}" bash tests/uamqp-patch-test.sh

${TEST_CREDIT_BINARY}: tests/LinkReceiverCreditTest.c ${UAMQP_SOURCE_DIR}/src/link.c ${UAMQP_SOURCE_DIR}/inc/azure_uamqp_c/link.h patches/azure-uamqp-c-manual-receiver-credit.patch
	${CC} -Wall -O2 -I${UAMQP_SOURCE_DIR} -I${UAMQP_SOURCE_DIR}/inc -I/usr/local/include -I/usr/local/include/c_logging/v2 -I/usr/local/include/azureiot -I/usr/local/include/macro_utils -I/usr/local/include/umock_c $< -L/usr/local/lib -o $@ ${TEST_LINKER_DEPENDENCIES} -lc_logging_v2

${TEST_BINARY}: tests/AmqpValueBodyDecoderTest.cpp AmqpValueBodyDecoder.cpp AmqpValueBodyDecoder.h
	${COMPILER} -Wall -O2 -std=c++11 -I. -I/usr/local/include -I/usr/local/include/c_logging/v2 -I/usr/local/include/azureiot -I/usr/local/include/macro_utils -I/usr/local/include/umock_c $< AmqpValueBodyDecoder.cpp -L/usr/local/lib -o $@ ${TEST_LINKER_DEPENDENCIES}

${TEST_METADATA_BINARY}: tests/AmqpValueDecoderTest.cpp AmqpValueDecoder.cpp AmqpValueDecoder.h
	${COMPILER} -Wall -O2 -std=c++11 -I. -I/usr/local/include -I/usr/local/include/c_logging/v2 -I/usr/local/include/azureiot -I/usr/local/include/macro_utils -I/usr/local/include/umock_c $< AmqpValueDecoder.cpp -L/usr/local/lib -o $@ ${TEST_LINKER_DEPENDENCIES}

help: ## shows help
	@echo "\033[33mUsage:\033[0m\n  make [target] [arg=\"val\"...]\n\n\033[33mTargets:\033[0m"
	@grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[32m%-15s\033[0m %s\n", $$1, $$2}'

install: ## install extension ini and so files in the proper locations
	${CP} ${EXTENSION} ${EXTENSION_DIR}
	# ${CP} ${INI} ${INI_DIR}

clean: ## remove object files
	${RM} ${EXTENSION} ${OBJECTS} ${DEPENDENCIES} ${TEST_BINARY} ${TEST_METADATA_BINARY} ${TEST_CREDIT_BINARY}

-include ${DEPENDENCIES}
