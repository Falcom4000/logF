#pragma once

#include <vector>
#include <atomic>
#include <shared_mutex>
#include "log_message.h"

namespace logF {

struct BufferView {
    const LogMessage* data;
    size_t size;
    
    const LogMessage* begin() const { return data; }
    const LogMessage* end() const { return data + size; }
};

class DoubleBuffer {
public:
    DoubleBuffer(size_t capacity);
    void write(LogMessage item);
    BufferView read_and_swap();

private:
    // 非原子变量放在前面
    std::vector<LogMessage> buffers_[2];
    size_t capacity_;
    
    // 原子变量使用64字节对齐，避免虚假共享
    alignas(64) std::atomic<int> write_buffer_index_{0};    // 0 或 1，指示生产者当前写入哪个buffer
    alignas(64) std::atomic<size_t> write_pos_{0};          // 当前写入buffer中的位置索引
};

}