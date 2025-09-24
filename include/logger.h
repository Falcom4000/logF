#pragma once

#include "log_message.h"
#include "double_buffer.h"
#include <cstdint>
#include <utility>
#include <cstring>

namespace logF {

class Logger {
public:
    Logger(DoubleBuffer& double_buffer);
    
    template<typename... Args>
    void log(LogLevel level, const char* file, int line, const char* format, Args&&... args) {
        static_assert(sizeof...(args) <= MAX_LOG_ARGS, "Too many log arguments");

        LogMessage msg{};  
        msg.timestamp = std::chrono::system_clock::now();
        msg.file = file;
        msg.line = static_cast<uint16_t>(line > 65535 ? 65535 : line);  // 限制在uint16_t范围内
        msg.level = static_cast<uint8_t>(level);
        msg.format = format;
        
        size_t arg_idx = 0;
        ( (msg.args[arg_idx++] = std::forward<Args>(args)), ... );
        msg.num_args = sizeof...(args);

        double_buffer_.write(std::move(msg));
    }

private:
    DoubleBuffer& double_buffer_;
    
};

}

// 提取相对路径的宏，避免存储完整的绝对路径
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define LOG_INFO(logger, format, ...) (logger).log(logF::LogLevel::INFO, __FILENAME__, __LINE__, format, __VA_ARGS__)
#define LOG_WARNING(logger, format, ...) (logger).log(logF::LogLevel::WARNING, __FILENAME__, __LINE__, format, __VA_ARGS__)
#define LOG_ERROR(logger, format, ...) (logger).log(logF::LogLevel::ERROR, __FILENAME__, __LINE__, format, __VA_ARGS__)
