ROOT := $(abspath ..)
UAMQP_ROOT := $(ROOT)/libs-build/azure-uamqp-c
SHARED_ROOT := $(ROOT)/libs-build/azure-c-shared-utility
LOGGING_ROOT := $(SHARED_ROOT)/deps/c-logging/v2
BUILD_DIR := $(CURDIR)/build

CXX ?= g++
CXXFLAGS := -std=c++20 -Wall -Wextra -pedantic -I/usr/local/include -I$(UAMQP_ROOT)/inc -I$(SHARED_ROOT)/inc -I$(SHARED_ROOT)/deps/c-logging/v2/inc -I$(SHARED_ROOT)/deps/macro-utils-c/inc -I$(SHARED_ROOT)/deps/umock-c/inc
LDFLAGS := -L/usr/local/lib -L$(UAMQP_ROOT)/cmake -L$(SHARED_ROOT)/cmake -L$(LOGGING_ROOT)/cmake
LDLIBS := -luamqp -laziotsharedutil -lc_logging_v2 -luuid -lpthread -ldl

.PHONY: all clean run

all: $(BUILD_DIR)/subscription_consumer

$(BUILD_DIR)/subscription_consumer: subscription_consumer.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) $(LDLIBS) -Wl,-rpath,/usr/local/lib -o $@

run: all
	$(BUILD_DIR)/subscription_consumer

clean:
	rm -rf $(BUILD_DIR)
