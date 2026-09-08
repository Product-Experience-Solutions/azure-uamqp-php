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
COMPILER_FLAGS      = -Wall -c -O2 -std=c++11 -fpic -I/usr/local/include -I/usr/local/include/c_logging/v2 -I/usr/local/include/azureiot -I/usr/local/include/macro_utils -I/usr/local/include/umock_c -o
LINKER_FLAGS        = -shared -L/usr/local/lib
LINKER_DEPENDENCIES = -lphpcpp -luamqp -laziotsharedutil -luuid
TEST_LINKER_DEPENDENCIES = -luamqp -laziotsharedutil -luuid

RM =   rm -f
CP =   cp -f

SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:%.cpp=%.o)
TEST_BINARY = tests/amqp-value-body-decoder-test

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

test: ${TEST_BINARY} ## run native regression tests
	./${TEST_BINARY}

test-versioning: ## verify native and package versions, including incremental builds
	bash tests/versioning-test.sh

${TEST_BINARY}: tests/AmqpValueBodyDecoderTest.cpp AmqpValueBodyDecoder.cpp AmqpValueBodyDecoder.h
	${COMPILER} -Wall -O2 -std=c++11 -I. -I/usr/local/include -I/usr/local/include/c_logging/v2 -I/usr/local/include/azureiot -I/usr/local/include/macro_utils -I/usr/local/include/umock_c $< AmqpValueBodyDecoder.cpp -L/usr/local/lib -o $@ ${TEST_LINKER_DEPENDENCIES}

help: ## shows help
	@echo "\033[33mUsage:\033[0m\n  make [target] [arg=\"val\"...]\n\n\033[33mTargets:\033[0m"
	@grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[32m%-15s\033[0m %s\n", $$1, $$2}'

install: ## install extension ini and so files in the proper locations
	${CP} ${EXTENSION} ${EXTENSION_DIR}
	# ${CP} ${INI} ${INI_DIR}

clean: ## remove object files
	${RM} ${EXTENSION} ${OBJECTS} ${TEST_BINARY}
