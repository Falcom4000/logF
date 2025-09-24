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
};

}