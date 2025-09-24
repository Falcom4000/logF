#include "../include/mmap_writer.h"
#include <iostream>
#include <cerrno>
#include <cstring>
#include <algorithm>

namespace logF {

MMapFileWriter::MMapFileWriter(const std::string& filepath, size_t initial_size)
    : filepath_(filepath), file_size_(initial_size) {}

MMapFileWriter::~MMapFileWriter() {
    close();
}

MMapFileWriter::MMapFileWriter(MMapFileWriter&& other) noexcept
    : filepath_(std::move(other.filepath_))
    , fd_(other.fd_)
    , mapped_memory_(other.mapped_memory_)
    , file_size_(other.file_size_)
    , write_pos_(other.write_pos_.load()) {
    
    other.fd_ = -1;
    other.mapped_memory_ = nullptr;
    other.file_size_ = 0;
    other.write_pos_.store(0);
}

MMapFileWriter& MMapFileWriter::operator=(MMapFileWriter&& other) noexcept {
    if (this != &other) {
        close();
        
        filepath_ = std::move(other.filepath_);
        fd_ = other.fd_;
        mapped_memory_ = other.mapped_memory_;
        file_size_ = other.file_size_;
        write_pos_.store(other.write_pos_.load());
        
        other.fd_ = -1;
        other.mapped_memory_ = nullptr;
        other.file_size_ = 0;
        other.write_pos_.store(0);
    }
    return *this;
}

bool MMapFileWriter::open() {
    // Open file with read/write permissions, create if doesn't exist, truncate to clear content
    fd_ = ::open(filepath_.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd_ == -1) [[unlikely]] {
        std::cerr << "Failed to open file " << filepath_ << ": " << std::strerror(errno) << std::endl;
        return false;
    }
    
    // Expand file to desired size (file was truncated by O_TRUNC)
    if (ftruncate(fd_, file_size_) == -1) [[unlikely]] {
        std::cerr << "Failed to expand file: " << std::strerror(errno) << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    
    // Memory map the file
    mapped_memory_ = static_cast<char*>(mmap(nullptr, file_size_, 
                                           PROT_READ | PROT_WRITE, 
                                           MAP_SHARED, fd_, 0));
    
    if (mapped_memory_ == MAP_FAILED) [[unlikely]] {
        std::cerr << "Failed to mmap file: " << std::strerror(errno) << std::endl;
        mapped_memory_ = nullptr;
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    
    // File was truncated, so we start writing from position 0
    write_pos_.store(0);
    
    return true;
}

void MMapFileWriter::close() {
    if (mapped_memory_ != nullptr) {
        // Sync any pending writes
        msync(mapped_memory_, file_size_, MS_SYNC);
        
        // Truncate file to actual content size
        size_t final_pos = write_pos_.load();
        if (final_pos > 0 && final_pos < file_size_) [[likely]] {
            if (ftruncate(fd_, final_pos) == -1) [[unlikely]] {
                std::cerr << "Warning: Failed to truncate file to final size: " 
                          << std::strerror(errno) << std::endl;
            }
        }
        
        munmap(mapped_memory_, file_size_);
        mapped_memory_ = nullptr;
    }
    
    if (fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
    }
    
    file_size_ = 0;
    write_pos_.store(0);
}

bool MMapFileWriter::write(const char* data, size_t len) {
    if (!is_open() || len == 0) [[unlikely]] {
        return false;
    }
    
    size_t current_pos = write_pos_.load(std::memory_order_acquire);
    
    // Check if we need to expand the file
    if (current_pos + len > file_size_) [[unlikely]] {
        size_t new_size = next_power_of_2(current_pos + len);
        if (!expand_file(new_size)) [[unlikely]] {
            return false;
        }
    }
    
    // Atomic check and increment of write position
    size_t expected_pos = current_pos;
    while (!write_pos_.compare_exchange_weak(expected_pos, current_pos + len, 
                                            std::memory_order_acq_rel)) {
        current_pos = expected_pos;
        
        // Recheck if we need expansion with the updated position
        if (current_pos + len > file_size_) [[unlikely]] {
            size_t new_size = next_power_of_2(current_pos + len);
            if (!expand_file(new_size)) [[unlikely]] {
                return false;
            }
        }
    }
    
    // Copy data to memory-mapped region
    std::memcpy(mapped_memory_ + current_pos, data, len);
    
    return true;
}

void MMapFileWriter::flush() {
    if (is_open()) {
        size_t current_pos = write_pos_.load(std::memory_order_acquire);
        msync(mapped_memory_, current_pos, MS_ASYNC);
    }
}

bool MMapFileWriter::expand_file(size_t new_size) {
    if (new_size <= file_size_) [[unlikely]] {
        return true; // No expansion needed
    }
    
    // Unmap current memory
    if (munmap(mapped_memory_, file_size_) == -1) [[unlikely]] {
        std::cerr << "Failed to unmap memory: " << std::strerror(errno) << std::endl;
        return false;
    }
    
    // Expand file
    if (ftruncate(fd_, new_size) == -1) [[unlikely]] {
        std::cerr << "Failed to expand file to " << new_size << " bytes: " 
                  << std::strerror(errno) << std::endl;
        
        // Try to remap with old size
        mapped_memory_ = static_cast<char*>(mmap(nullptr, file_size_, 
                                               PROT_READ | PROT_WRITE, 
                                               MAP_SHARED, fd_, 0));
        return false;
    }
    
    // Remap with new size
    mapped_memory_ = static_cast<char*>(mmap(nullptr, new_size, 
                                           PROT_READ | PROT_WRITE, 
                                           MAP_SHARED, fd_, 0));
    
    if (mapped_memory_ == MAP_FAILED) [[unlikely]] {
        std::cerr << "Failed to remap expanded file: " << std::strerror(errno) << std::endl;
        mapped_memory_ = nullptr;
        return false;
    }
    
    file_size_ = new_size;
    return true;
}

size_t MMapFileWriter::next_power_of_2(size_t n) {
    if (n <= 1) return 1;
    
    // Find the next power of 2 greater than or equal to n
    size_t power = 1;
    while (power < n) {
        power <<= 1;
    }
    
    // Ensure minimum growth to avoid frequent expansions
    size_t min_growth = file_size_ + (file_size_ >> 1); // 1.5x current size
    return std::max(power, min_growth);
}

}