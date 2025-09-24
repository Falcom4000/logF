cmake_minimum_required(VERSION 3.10)
project(logF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add AddressSanitizer and debug flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address -O3")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

include_directories(include)

add_library(logF_lib src/logger.cpp src/double_buffer.cpp)

add_executable(example examples/main.cpp)
target_link_libraries(example logF_lib)

add_executable(benchmark examples/benchmark.cpp)
target_link_libraries(benchmark logF_lib)
