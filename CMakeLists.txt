cmake_minimum_required(VERSION 3.17)

project(boostasiotest)

find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

add_executable(simple_http_server simple_server.cpp)
target_link_libraries(simple_http_server pthread)
set_property(TARGET simple_http_server PROPERTY CXX_STANDARD 11)
