#pragma once

#include "variant.h"
#include <chrono>
#include <string>
#include <vector>
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
    std::string format;
    std::vector<LogVariant> args;
    
    // 性能测量字段
    uint64_t frontend_start_cycles;
    uint64_t frontend_end_cycles;
    uint64_t sequence_number;  // 用于丢包检测
};

}