#pragma once

#include "double_buffer.h"
#include "log_message.h"
#include "ring_buffer.h"
#include "mmap_writer.h"
#include <cstdint>
#include <string>
#include <thread>
#include <atomic>

namespace logF {

class Consumer {
public:
    Consumer(DoubleBuffer& double_buffer, const std::string& log_dir, size_t mmap_file_size = 1024 * 1024 * 16);
    void start();
    void stop();
    uint64_t get_processed_count() const { return message_count_; }

private:
    void run();
    void format_log(const LogMessage& msg);
    
    // 非原子变量
    DoubleBuffer& double_buffer_;
    MMapFileWriter mmap_writer_;
    std::thread thread_;
    uint64_t message_count_ = 0;
    CharRingBuffer char_buffer_;
    
    // 原子变量64字节对齐
    alignas(64) std::atomic<bool> running_ = false;
};

}