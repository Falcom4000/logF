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
    
    // 将 write_buffer_index_ 和 write_pos_ 合并成一个64位原子变量
    // 最高位是 buffer index, 其余位是 position
    alignas(64) std::atomic<uint64_t> write_state_{0};
};

}