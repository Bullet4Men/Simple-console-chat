cmake_minimum_required(VERSION 3.26)
project(simple_console_chat)

set(CMAKE_CXX_STANDARD 11)

add_executable(server server.cpp message.h)
add_executable(client client.cpp message.h)

target_link_libraries(client boost_thread)
