#pragma once

#include <string>
#include <cstddef>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <atomic>

namespace logF {

class MMapFileWriter {
public:
    explicit MMapFileWriter(const std::string& filepath, size_t initial_size = 64 * 1024 * 1024); // 64MB default
    ~MMapFileWriter();
    
    // Delete copy constructor and assignment
    MMapFileWriter(const MMapFileWriter&) = delete;
    MMapFileWriter& operator=(const MMapFileWriter&) = delete;
    
    // Move constructor and assignment
    MMapFileWriter(MMapFileWriter&& other) noexcept;
    MMapFileWriter& operator=(MMapFileWriter&& other) noexcept;
    
    bool open();
    void close();
    
    // Write data to the memory-mapped file
    bool write(const char* data, size_t len);
    
    // Flush pending writes to disk
    void flush();
    
    // Get current write position
    size_t position() const { return write_pos_.load(std::memory_order_acquire); }
    
    // Check if writer is ready
    bool is_open() const { return fd_ != -1 && mapped_memory_ != nullptr; }

private:
    // 非原子变量
    std::string filepath_;
    int fd_ = -1;
    char* mapped_memory_ = nullptr;
    size_t file_size_ = 0;
    
    // 原子变量64字节对齐
    alignas(64) std::atomic<size_t> write_pos_{0};
    
    // Expand the file and remap memory when needed
    bool expand_file(size_t new_size);
    
    // Helper to calculate next power of 2
    size_t next_power_of_2(size_t n);
};

}