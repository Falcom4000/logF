#include "../include/double_buffer.h"
#include <utility>
#include <shared_mutex>
#include <mutex>

namespace logF {

DoubleBuffer::DoubleBuffer(size_t capacity) : capacity_(capacity) {
    buffers_[0].resize(capacity);
    buffers_[1].resize(capacity);
}

void DoubleBuffer::write(LogMessage item) {
    
    // 原子地获取写入位置
    size_t pos = write_pos_.fetch_add(1, std::memory_order_acq_rel);
    
    // 如果超出容量则丢弃
    if (pos >= capacity_) {
        return;
    }
    
    // 获取当前写入缓冲区索引
    int buffer_idx = write_buffer_index_.load(std::memory_order_acquire);
    
    // 写入对应位置
    buffers_[buffer_idx][pos] = std::move(item);
}

std::vector<LogMessage> DoubleBuffer::read_and_swap() {
    
    // 获取当前写入缓冲区索引
    int current_write_idx = write_buffer_index_.load(std::memory_order_acquire);
    int read_idx = 1 - current_write_idx;  // 读取缓冲区是另一个
    
    // 获取当前写入位置，这决定了需要读取多少数据
    size_t write_pos = write_pos_.load(std::memory_order_acquire);
    
    // 准备结果vector，只包含有效数据
    std::vector<LogMessage> result;
    if (write_pos > 0) {
        result.reserve(write_pos);
        
        // 从读取缓冲区中复制有效数据
        for (size_t i = 0; i < write_pos && i < capacity_; ++i) {
            result.push_back(std::move(buffers_[read_idx][i]));
        }
    }
    
    // 交换缓冲区：A变成1-A
    write_buffer_index_.store(read_idx, std::memory_order_release);
    
    // 重置写入位置
    write_pos_.store(0, std::memory_order_release);

    return result;
}

}