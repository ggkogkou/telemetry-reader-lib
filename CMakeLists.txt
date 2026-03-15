cmake_minimum_required(VERSION 3.21)
project(foc_tlm_parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost CONFIG REQUIRED COMPONENTS asio)

add_executable(foc_tlm_parser
        main.cpp
        TelemetryParser.cpp
        TelemetryLogger.cpp
        AD2S12101.cpp
        ResolverLogger.cpp
)

target_link_libraries(foc_tlm_parser PRIVATE
        Boost::asio
        ws2_32
)

target_include_directories(foc_tlm_parser PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(foc_tlm_parser PRIVATE
        _WIN32_WINNT=0x0A00
)

if (MSVC)
    target_compile_options(foc_tlm_parser PRIVATE
            $<$<CONFIG:Release>:/O2 /DNDEBUG>
            $<$<CONFIG:RelWithDebInfo>:/O2 /DNDEBUG>
    )
else()
    target_compile_options(foc_tlm_parser PRIVATE
            $<$<CONFIG:Release>:-O3 -DNDEBUG>
            $<$<CONFIG:RelWithDebInfo>:-O2 -g -DNDEBUG>
    )
endif()