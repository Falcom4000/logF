#include "../include/consumer.h"
#include <iostream>
#include <chrono>
#include <variant>
#include <ctime>

namespace logF {

// 线程本地时间缓存
struct TimeCache {
    std::time_t cached_seconds = 0;
    char cached_time_str[32] = {0};
    
    // 快速更新时间字符串，避免重复的strftime调用
    void update_time_string(std::time_t new_seconds) {
        if (new_seconds == cached_seconds) return;
        
        // 只有在秒数真正变化时才调用昂贵的时间格式化
        std::strftime(cached_time_str, sizeof(cached_time_str), 
                      "%Y-%m-%d %X", std::localtime(&new_seconds));
        cached_seconds = new_seconds;
    }
};
thread_local TimeCache time_cache;

Consumer::Consumer(DoubleBuffer& double_buffer, const std::string& filepath)
    : double_buffer_(double_buffer), filepath_(filepath), mmap_writer_(filepath), 
      char_buffer_(65536*2) {}

void Consumer::start() {
    running_.store(true, std::memory_order_release);
    if (!mmap_writer_.open()) [[unlikely]] {
        std::cerr << "Failed to open mmap writer" << std::endl;
        return;
    }
    thread_ = std::thread(&Consumer::run, this);
}

uint64_t Consumer::stop() {
    running_.store(false, std::memory_order_release);
    if (thread_.joinable()) {
        thread_.join();
    }
    mmap_writer_.close();
    return message_count_;
}

void Consumer::run() {
    while (running_.load(std::memory_order_acquire)) {
        auto buffer_view = double_buffer_.read_and_swap();
        for (const auto& msg : buffer_view) {
            format_log(msg);
            message_count_++;
        }
        std::this_thread::yield();
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
    
    // 高性能时间格式化 - 缓存避免重复的时区转换和strftime调用
    auto in_time_t = std::chrono::system_clock::to_time_t(msg.timestamp);
    time_cache.update_time_string(in_time_t);
    
    // Append all components directly to char buffer
    char_buffer_.append(time_cache.cached_time_str);
    char_buffer_.append(" ");
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
        std::visit([&](auto&& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, const char*>) {
                char_buffer_.append(value);
            } else if constexpr (std::is_same_v<T, double>) {
                char_buffer_.append_number(value);
            } else {
                char_buffer_.append_number(static_cast<long long>(value));
            }
        }, msg.args[arg_index]);
        
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