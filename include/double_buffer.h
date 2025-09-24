#pragma once

#include <vector>
#include <atomic>
#include <shared_mutex>
#include "log_message.h"

namespace logF {

class DoubleBuffer {
public:
    DoubleBuffer(size_t capacity);
    void write(LogMessage item);
    std::vector<LogMessage> read_and_swap();

private:
    std::vector<LogMessage> buffers_[2];
    std::atomic<int> write_buffer_index_{0};    // 0 或 1，指示生产者当前写入哪个buffer
    std::atomic<size_t> write_pos_{0};          // 当前写入buffer中的位置索引
    size_t capacity_;
};

}