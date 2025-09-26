#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <array>
#include <cstdint>

namespace logF {

// The type of a variant argument
enum class LogVariantType : uint8_t {
    INT = 0,
    DOUBLE = 1,
    NONE = 2 // Represents an unused argument slot
};

constexpr size_t MAX_LOG_ARGS = 3;

enum class LogLevel : uint8_t {
    INFO = 0,
    WARNING = 1,
    ERROR = 2
};

struct LogMessage {
    union ArgData {
        int32_t i;
        double d;
    };

    std::chrono::system_clock::time_point timestamp;  // 8 bytes
    const char* file;                                 // 8 bytes  
    std::string_view format;                          // 16 bytes
    std::array<ArgData, MAX_LOG_ARGS> args_data;       // 3 * 8 = 24 bytes
    std::array<LogVariantType, MAX_LOG_ARGS> args_types; // 3 * 1 = 3 bytes
    uint16_t line;                                    // 2 bytes
    uint8_t level;                                    // 1 byte
    uint8_t num_args;                                 // 1 byte
    // Total: 8 + 8 + 16 + 24 + 3 + 2 + 1 + 1 = 63 bytes (fits within 64 bytes)

    LogMessage() : file(nullptr), format(""), line(0), level(0), num_args(0) {
        args_types.fill(LogVariantType::NONE);
    }
    // 构造函数
   template<typename... Args>
    LogMessage(const char* file, uint16_t line, LogLevel level, const char* format, Args&&... args)
        : file(file), format(format), line(line), level(static_cast<uint8_t>(level)), num_args(0) {
        static_assert(sizeof...(args) <= MAX_LOG_ARGS, "Too many log arguments");
        args_types.fill(LogVariantType::NONE);
        auto process_arg = [&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_convertible_v<T, int32_t> && !std::is_same_v<T, double>) {
                this->args_types[num_args] = LogVariantType::INT;
                this->args_data[num_args].i = arg;
            } else if constexpr (std::is_convertible_v<T, double>) {
                this->args_types[num_args] = LogVariantType::DOUBLE;
                this->args_data[num_args].d = arg;
            }
            num_args++;
        };
        (process_arg(std::forward<Args>(args)), ...);
    }
};

}