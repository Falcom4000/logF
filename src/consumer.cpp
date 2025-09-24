#include "../include/double_buffer.h"
#include "../include/log_message.h"
#include <cstdint>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <variant>
#include <sstream>
#include <iomanip>

namespace logF {
class Consumer {
public:
    Consumer(DoubleBuffer& double_buffer, const std::string& filepath);
    void start();
    u_int64_t stop();

private:
    void run();
    void format_log(const LogMessage& msg);
    DoubleBuffer& double_buffer_;
    std::string filepath_;
    std::ofstream file_;
    std::atomic<bool> running_ = false;
    std::thread thread_;
    uint64_t message_count_ = 0;
};

Consumer::Consumer(DoubleBuffer& double_buffer, const std::string& filepath)
    : double_buffer_(double_buffer), filepath_(filepath) {}

void Consumer::start() {
    running_.store(true, std::memory_order_release);
    file_.open(filepath_, std::ios::out | std::ios::app);
    thread_ = std::thread(&Consumer::run, this);
}

uint64_t Consumer::stop() {
    running_.store(false, std::memory_order_release);
    if (thread_.joinable()) {
        thread_.join();
    }
    file_.close();
    return message_count_;
}

void Consumer::run() {
    while (running_.load(std::memory_order_acquire)) {
        auto data = double_buffer_.read_and_swap();
        for (const auto& msg : data) {
            format_log(msg);
            message_count_++;
        }
        std::this_thread::yield();
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