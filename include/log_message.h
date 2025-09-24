#pragma once

#include "variant.h"
#include <chrono>
#include <string>
#include <string_view>
#include <array>
#include <cstdint>

namespace logF {

constexpr size_t MAX_LOG_ARGS = 10;

enum class LogLevel {
    INFO,
    WARNING,
    ERROR
};

struct LogMessage {
    std::chrono::system_clock::time_point timestamp;
    const char* file;
    int line;
    LogLevel level;
    std::string_view format;
    std::array<LogVariant, MAX_LOG_ARGS> args;
    uint8_t num_args = 0;
    
    // 性能测量字段
    uint64_t frontend_start_cycles;
    uint64_t frontend_end_cycles;
    uint64_t sequence_number;  // 用于丢包检测
};

}