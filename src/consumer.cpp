#include "../include/consumer.h"
#include <cstdint>
#include <iostream>
#include <chrono>
#include <variant>
#include <ctime>

namespace logF {
struct TimeCache {
    int64_t cached_milliseconds = 0;
    char cached_time_str[32] = {0};  // MM-DD HH:MM:SS.sss 格式预留足够空间
    
    // 只在毫秒变化时重新格式化时间字符串
    void update_time_string(const std::chrono::system_clock::time_point& timestamp) {
        // 获取毫秒级时间戳
        auto ms_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()).count();
        
        if (ms_since_epoch == cached_milliseconds) return;
        
        auto seconds_since_epoch = ms_since_epoch / 1000;
        int milliseconds = static_cast<int>(ms_since_epoch - seconds_since_epoch * 1000);
        
        std::time_t seconds = static_cast<std::time_t>(seconds_since_epoch);
        
        // 获取本地时间
        std::tm* local_tm = std::localtime(&seconds);
        
        // 使用strftime格式化基本时间部分
        char base_time[16];
        std::strftime(base_time, sizeof(base_time), "%m-%d %H:%M:%S", local_tm);
        
        // 添加毫秒部分
        std::snprintf(cached_time_str, sizeof(cached_time_str),
                     "%s.%03d", base_time, milliseconds);
        
        cached_milliseconds = ms_since_epoch;
    }
};
TimeCache time_cache;

Consumer::Consumer(DoubleBuffer& double_buffer, const std::string& log_dir, size_t mmap_file_size)
    : double_buffer_(double_buffer), mmap_writer_(log_dir, mmap_file_size), 
      char_buffer_(65536*2) {}

void Consumer::start() {
    running_.store(true, std::memory_order_release);
    if (!mmap_writer_.open()) [[unlikely]] {
        std::cerr << "Failed to open mmap writer" << std::endl;
        return;
    }
    thread_ = std::thread(&Consumer::run, this);
}

void Consumer::stop() {
    running_.store(false, std::memory_order_release);
    if (thread_.joinable()) {
        thread_.join();
    }
    mmap_writer_.close();
    return;
}

void Consumer::run() {
    uint64_t local_count = 0;
    while (running_.load(std::memory_order_acquire)) {
        auto buffer_view = double_buffer_.read_and_swap();
        if( buffer_view.size == 0) {
            if(local_count < 50) {
                local_count++;
                continue;
            }
            local_count = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        for (const auto& msg : buffer_view) {
            format_log(msg);
            message_count_++;
        }
        
    }
    // Flush any remaining data when stopping
    char_buffer_.flush_to_mmap(mmap_writer_);
    char_buffer_.clear();
}

void Consumer::format_log(const LogMessage& msg) {
    // Check if we need to flush the buffer (leave some space for current message)
    if (!char_buffer_.has_space(256)) [[unlikely]] {
        char_buffer_.flush_to_mmap(mmap_writer_);
        char_buffer_.clear();
    }
    switch (static_cast<LogLevel>(msg.level)) {
        case LogLevel::INFO:
            char_buffer_.append("[INFO]");
            break;
        case LogLevel::WARNING:
            char_buffer_.append("[WARNING]");
            break;
        case LogLevel::ERROR:
            char_buffer_.append("[ERROR]");
            break;
    }
    time_cache.update_time_string(msg.timestamp);
    
    // Append all components directly to char buffer
    char_buffer_.append(time_cache.cached_time_str);


    if (msg.file) [[likely]] {
        char_buffer_.append(msg.file);
    } else [[unlikely]] {
        char_buffer_.append("unknown");
    }
    char_buffer_.append(":");
    char_buffer_.append_number(static_cast<long long>(msg.line));
    char_buffer_.append(" ");
    
    // Process format string and arguments
    size_t last_pos = 0;
    size_t find_pos = 0;
    size_t arg_index = 0;

    while ((find_pos = msg.format.find('%', last_pos)) != std::string::npos && arg_index < msg.args.size()) {
        // Append text before the placeholder
        if (find_pos > last_pos) {
            std::string_view substr = msg.format.substr(last_pos, find_pos - last_pos);
            char_buffer_.append(substr.data(), substr.size());
        }
        
        // Append the argument value
        const auto& arg = msg.args[arg_index];
        switch (arg.get_type()) {
            case LogVariant::Type::CSTR:
                char_buffer_.append(arg.as_cstr());
                break;
            case LogVariant::Type::DOUBLE:
                char_buffer_.append_number(arg.as_double());
                break;
            case LogVariant::Type::INT:
                char_buffer_.append_number(static_cast<long long>(arg.as_int()));
                break;
        }
        
        arg_index++;
        last_pos = find_pos + 1;
    }
    
    // Append remaining text after last placeholder
    if (last_pos < msg.format.size()) {
        std::string_view remaining = msg.format.substr(last_pos);
        char_buffer_.append(remaining.data(), remaining.size());
    }
    
    char_buffer_.append('\n');
}

}