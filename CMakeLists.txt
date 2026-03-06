cmake_minimum_required(VERSION 3.21)
project(serial_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost CONFIG REQUIRED COMPONENTS asio)

add_executable(serial_app
        main.cpp
        TelemetryParser.cpp
)

target_link_libraries(serial_app PRIVATE Boost::asio)
target_include_directories(serial_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})