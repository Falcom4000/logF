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
    [[unlikely]]if (pos >= capacity_) {
        return;
    }
    else{
            // 获取当前写入缓冲区索引
        int buffer_idx = write_buffer_index_.load(std::memory_order_acquire);
    // 写入对应位置
        buffers_[buffer_idx][pos] = std::move(item);
    }    
}
    

BufferView DoubleBuffer::read_and_swap() {
    
    // 获取当前写入缓冲区索引和写入位置
    int current_write_idx = write_buffer_index_.load(std::memory_order_acquire);
    size_t write_pos = write_pos_.load(std::memory_order_acquire);
    
    // 计算读取缓冲区索引（非活跃的那个）
    int read_idx = 1 - current_write_idx;
    
    // 交换缓冲区：让生产者写入之前的读取缓冲区
    write_buffer_index_.store(read_idx, std::memory_order_release);
    
    // 重置写入位置
    write_pos_.store(0, std::memory_order_release);
    
    // 返回指向当前写入缓冲区的视图（交换前的写入缓冲区，现在变成读取缓冲区）
    // 这个缓冲区包含了之前生产者写入的数据，现在可以安全读取
    return BufferView{
        buffers_[current_write_idx].data(),
        std::min(write_pos, capacity_)
    };
}

}