#pragma once

#include "variant.h"
#include <chrono>
#include <string>
#include <string_view>
#include <array>
#include <cstdint>

namespace logF {

constexpr size_t MAX_LOG_ARGS = 3;

enum class LogLevel : uint8_t {
    INFO = 0,
    WARNING = 1,
    ERROR = 2
};

struct LogMessage {
    std::chrono::system_clock::time_point timestamp;  // 8 bytes
    const char* file;                                 // 8 bytes  
    std::string_view format;                          // 16 bytes
    std::array<LogVariant, MAX_LOG_ARGS> args;        // 28 bytes
    uint16_t line;                                    // 2 bytes
    uint8_t level;                                    // 1 byte
    uint8_t num_args;                                 // 1 byte
};

}