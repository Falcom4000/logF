cmake_minimum_required(VERSION 3.10)
project(logF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add debug flags with better symbol information for profiling
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -fno-omit-frame-pointer -fno-inline-functions-called-once")

include_directories(include)

add_library(logF_lib src/logger.cpp src/double_buffer.cpp src/ring_buffer.cpp src/consumer.cpp src/mmap_writer.cpp)

add_executable(example examples/main.cpp)
target_link_libraries(example logF_lib)

add_executable(logF_benchmark examples/logF_benchmark.cpp)
target_link_libraries(logF_benchmark logF_lib)

find_package(spdlog REQUIRED)
add_executable(spdlog_benchmark examples/spdlog_benchmark.cpp)
target_link_libraries(spdlog_benchmark spdlog::spdlog)

find_package(glog REQUIRED)
find_package(GFlags REQUIRED)
add_executable(glog_benchmark examples/glog_benchmark.cpp)
target_link_libraries(glog_benchmark glog::glog gflags)


