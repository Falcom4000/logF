#include "../include/double_buffer.h"
#include "../include/log_message.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <variant>
#include <sstream>
#include <iomanip>

namespace logF {

struct PerformanceStats {
    uint64_t total_messages = 0;
    uint64_t dropped_messages = 0;
    uint64_t last_sequence_number = 0;
    uint64_t min_frontend_latency = UINT64_MAX;
    uint64_t max_frontend_latency = 0;
    uint64_t total_frontend_latency = 0;
    uint64_t min_end_to_end_latency = UINT64_MAX;
    uint64_t max_end_to_end_latency = 0;
    uint64_t total_end_to_end_latency = 0;
    
    void update_frontend_latency(uint64_t latency) {
        min_frontend_latency = std::min(min_frontend_latency, latency);
        max_frontend_latency = std::max(max_frontend_latency, latency);
        total_frontend_latency += latency;
    }
    
    void update_end_to_end_latency(uint64_t latency) {
        min_end_to_end_latency = std::min(min_end_to_end_latency, latency);
        max_end_to_end_latency = std::max(max_end_to_end_latency, latency);
        total_end_to_end_latency += latency;
    }
};

class Consumer {
public:
    Consumer(DoubleBuffer& double_buffer, const std::string& filepath);
    void start();
    void stop();
    const PerformanceStats& get_stats() const { return stats_; }

private:
    void run();
    void format_log(const LogMessage& msg);
    
    static uint64_t rdtsc() {
#ifdef _MSC_VER
        return __rdtsc();
#else
        return __builtin_ia32_rdtsc();
#endif
    }

    DoubleBuffer& double_buffer_;
    std::string filepath_;
    std::ofstream file_;
    std::atomic<bool> running_ = false;
    std::thread thread_;
    PerformanceStats stats_;
};

Consumer::Consumer(DoubleBuffer& double_buffer, const std::string& filepath)
    : double_buffer_(double_buffer), filepath_(filepath) {}

void Consumer::start() {
    running_.store(true, std::memory_order_release);
    file_.open(filepath_, std::ios::out | std::ios::app);
    thread_ = std::thread(&Consumer::run, this);
}

void Consumer::stop() {
    running_.store(false, std::memory_order_release);
    if (thread_.joinable()) {
        thread_.join();
    }
    file_.close();
}

void Consumer::run() {
    while (running_.load(std::memory_order_acquire)) {
        auto data = double_buffer_.read_and_swap();

        for (const auto& msg : data) {
            uint64_t processing_start = rdtsc();
            
            // 检测丢包
            if (stats_.total_messages > 0) {
                uint64_t expected_seq = stats_.last_sequence_number + 1;
                if (msg.sequence_number > expected_seq) {
                    stats_.dropped_messages += (msg.sequence_number - expected_seq);
                }
            }
            stats_.last_sequence_number = msg.sequence_number;
            stats_.total_messages++;
            
            // 计算前端延迟
            uint64_t frontend_latency = msg.frontend_end_cycles - msg.frontend_start_cycles;
            stats_.update_frontend_latency(frontend_latency);
            
            // 计算端到端延迟
            uint64_t end_to_end_latency = processing_start - msg.frontend_start_cycles;
            stats_.update_end_to_end_latency(end_to_end_latency);
            
            format_log(msg);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 处理剩余的消息
    auto final_data = double_buffer_.read_and_swap();
    for (const auto& msg : final_data) {
        uint64_t processing_start = rdtsc();
        
        if (stats_.total_messages > 0) {
            uint64_t expected_seq = stats_.last_sequence_number + 1;
            if (msg.sequence_number > expected_seq) {
                stats_.dropped_messages += (msg.sequence_number - expected_seq);
            }
        }
        stats_.last_sequence_number = msg.sequence_number;
        stats_.total_messages++;
        
        uint64_t frontend_latency = msg.frontend_end_cycles - msg.frontend_start_cycles;
        stats_.update_frontend_latency(frontend_latency);
        
        uint64_t end_to_end_latency = processing_start - msg.frontend_start_cycles;
        stats_.update_end_to_end_latency(end_to_end_latency);
        
        format_log(msg);
    }
}

void Consumer::format_log(const LogMessage& msg) {
    std::stringstream ss;
    auto in_time_t = std::chrono::system_clock::to_time_t(msg.timestamp);
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
    ss << " " << msg.file << ":" << msg.line << " ";

    std::string result = ss.str();
    size_t last_pos = 0;
    size_t find_pos = 0;
    size_t arg_index = 0;

    while ((find_pos = msg.format.find('%', last_pos)) != std::string::npos && arg_index < msg.args.size()) {
        result += msg.format.substr(last_pos, find_pos - last_pos);
        
        std::visit([&](auto&& value) {
            std::stringstream temp_ss;
            temp_ss << value;
            result += temp_ss.str();
        }, msg.args[arg_index]);
        
        arg_index++;
        last_pos = find_pos + 1;
    }
    result += msg.format.substr(last_pos);
    file_ << result << std::endl;
}

}