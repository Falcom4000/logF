#pragma once

#include "variant.h"
#include <chrono>
#include <string>
#include <string_view>
#include <array>
#include <cstdint>

namespace logF {

constexpr size_t MAX_LOG_ARGS = 4;

enum class LogLevel : uint8_t {
    INFO = 0,
    WARNING = 1,
    ERROR = 2
};

struct LogMessage {
    std::chrono::system_clock::time_point timestamp;  // 8 bytes
    const char* file;                                 // 8 bytes  
    const char* format;                               // 8 bytes
    std::array<LogVariant, MAX_LOG_ARGS> args;        // 4 * (8 + 1)bytes
    uint16_t line;                                    // 2 bytes
    uint8_t level;                                    // 1 byte
    uint8_t num_args;                                 // 1 byte
                                                      // 1 byte padding

    LogMessage() : file(nullptr), format(""), line(0), level(0), num_args(0) {
        args.fill(LogVariant());
    }
    // 构造函数
   template<typename... Args>
    LogMessage(const char* file, uint16_t line, LogLevel level, const char* format, Args&&... args)
        : file(file), format(format), line(line), level(static_cast<uint8_t>(level)), num_args(sizeof...(args)) {
        static_assert(sizeof...(args) <= MAX_LOG_ARGS, "Too many log arguments");
        this->args.fill(LogVariant());
        size_t arg_idx = 0;
        ( (this->args[arg_idx++] = std::forward<Args>(args)), ... );
    }
};

}