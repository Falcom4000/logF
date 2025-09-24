#pragma once

#include "log_message.h"
#include "double_buffer.h"
#include <atomic>
#include <cstdint>

namespace logF {

class Logger {
public:
    Logger(DoubleBuffer& double_buffer);
    
    template<typename... Args>
    void log(LogLevel level, const char* file, int line, const char* format, Args&&... args) {
        static_assert(sizeof...(args) <= MAX_LOG_ARGS, "Too many log arguments");

        uint64_t start_cycles = rdtsc();
        
        LogMessage msg;
        msg.timestamp = std::chrono::system_clock::now();
        msg.file = file;
        msg.line = line;
        msg.level = level;
        msg.format = format;
        msg.frontend_start_cycles = start_cycles;
        msg.sequence_number = next_sequence_number_.fetch_add(1, std::memory_order_relaxed);
        
        size_t arg_idx = 0;
        ( (msg.args[arg_idx++] = std::forward<Args>(args)), ... );
        msg.num_args = sizeof...(args);

        msg.frontend_end_cycles = rdtsc();
        double_buffer_.write(std::move(msg));
    }

private:
    DoubleBuffer& double_buffer_;
    static std::atomic<uint64_t> next_sequence_number_;
    
    static uint64_t rdtsc() {
#ifdef _MSC_VER
        return __rdtsc();
#else
        return __builtin_ia32_rdtsc();
#endif
    }
};

}

#define LOG_INFO(logger, format, ...) (logger).log(logF::LogLevel::INFO, __FILE__, __LINE__, format, __VA_ARGS__)
#define LOG_WARNING(logger, format, ...) (logger).log(logF::LogLevel::WARNING, __FILE__, __LINE__, format, __VA_ARGS__)
#define LOG_ERROR(logger, format, ...) (logger).log(logF::LogLevel::ERROR, __FILE__, __LINE__, format, __VA_ARGS__)
