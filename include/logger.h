#pragma once

#include "log_message.h"
#include "mpsc_ring_buffer.h"
#include <cstdint>
#include <utility>
#include <cstring>

namespace logF {

class Logger {
public:
    Logger(MpscRingBuffer<LogMessage>& ring_buffer) : ring_buffer_(ring_buffer) {}
    
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

#define LOG_INFO(logger, format, ...) (logger).log(logF::LogLevel::INFO, __FILENAME__, __LINE__, format, __VA_ARGS__)
#define LOG_WARNING(logger, format, ...) (logger).log(logF::LogLevel::WARNING, __FILENAME__, __LINE__, format, __VA_ARGS__)
#define LOG_ERROR(logger, format, ...) (logger).log(logF::LogLevel::ERROR, __FILENAME__, __LINE__, format, __VA_ARGS__)
