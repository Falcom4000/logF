#pragma once

#include "log_message.h"
#include "mpsc_ring_buffer.h"
#include <cstdint>
#include <utility>
#include <cstring>

namespace logF {

template<LogLevel MinLevel = LogLevel::INFO>
class Logger {
public:
    explicit Logger(MpscRingBuffer<LogMessage>& ring_buffer) : ring_buffer_(ring_buffer) {}
    
    static constexpr LogLevel min_level() { return MinLevel; }
    
    template<typename... Args>
    void log(LogLevel level, const char* file, int line, const char* format, Args&&... args) {
        ring_buffer_.emplace(file, static_cast<uint16_t>(line), level, format, std::forward<Args>(args)...);
    }

private:
    MpscRingBuffer<LogMessage>& ring_buffer_;
};

}

// 提取相对路径的宏，避免存储完整的绝对路径
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// 编译期判断的日志宏
#define LOG_INFO(logger, format, ...) \
    do { \
        if constexpr (decltype(logger)::min_level() <= logF::LogLevel::INFO) { \
            (logger).log(logF::LogLevel::INFO, __FILENAME__, __LINE__, format, __VA_ARGS__); \
        } \
    } while(0)

#define LOG_WARNING(logger, format, ...) \
    do { \
        if constexpr (decltype(logger)::min_level() <= logF::LogLevel::WARNING) { \
            (logger).log(logF::LogLevel::WARNING, __FILENAME__, __LINE__, format, __VA_ARGS__); \
        } \
    } while(0)

#define LOG_ERROR(logger, format, ...) \
    do { \
        if constexpr (decltype(logger)::min_level() <= logF::LogLevel::ERROR) { \
            (logger).log(logF::LogLevel::ERROR, __FILENAME__, __LINE__, format, __VA_ARGS__); \
        } \
    } while(0)
