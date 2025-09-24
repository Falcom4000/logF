#pragma once

#include "double_buffer.h"
#include "log_message.h"
#include "ring_buffer.h"
#include <cstdint>
#include <fstream>
#include <string>
#include <thread>
#include <atomic>

namespace logF {

class Consumer {
public:
    Consumer(DoubleBuffer& double_buffer, const std::string& filepath);
    void start();
    uint64_t stop();

private:
    void run();
    void format_log(const LogMessage& msg);
    DoubleBuffer& double_buffer_;
    std::string filepath_;
    std::ofstream file_;
    std::atomic<bool> running_ = false;
    std::thread thread_;
    uint64_t message_count_ = 0;
    CharRingBuffer char_buffer_;
};

}