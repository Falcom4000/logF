cmake_minimum_required(VERSION 3.10)
project(logF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add debug flags (AddressSanitizer disabled for thread safety)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3")

include_directories(include)

add_library(logF_lib src/logger.cpp src/double_buffer.cpp src/ring_buffer.cpp src/consumer.cpp)

add_executable(example examples/main.cpp)
target_link_libraries(example logF_lib)

add_executable(benchmark examples/benchmark.cpp)
target_link_libraries(benchmark logF_lib)
