cmake_minimum_required(VERSION 4.2)
project(php_uamqp)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(php_uamqp
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
