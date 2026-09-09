cmake_minimum_required(VERSION 4.1.2)
execute_process(
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/version.sh"
        OUTPUT_VARIABLE PHUAMQP_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/version.sh")
project(php_uamqp VERSION "${PHUAMQP_VERSION}")

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(php_uamqp
        AmqpValueBodyDecoder.cpp
        AmqpValueBodyDecoder.h
        Connection.cpp
        Connection.h
        Consumer.cpp
        Consumer.h
        main.cpp
        Message.cpp
        Message.h
        Producer.cpp
        Producer.h
        Session.cpp
        Session.h)

target_compile_definitions(php_uamqp PRIVATE PHUAMQP_VERSION="${PHUAMQP_VERSION}")
