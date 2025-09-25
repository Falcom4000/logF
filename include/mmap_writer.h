#pragma once

#include <string>
#include <cstddef>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace logF {

class MMapFileWriter {
public:
    explicit MMapFileWriter(const std::string& log_dir, size_t file_size = 1024 * 1024 * 16); // 16MB default
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
    size_t position() const { return write_pos_; }
    
    // Check if writer is ready
    bool is_open() const { return fd_ != -1 && mapped_memory_ != nullptr; }

private:
    void generate_new_filepath();
    bool rotate_file();

    std::string log_dir_;
    std::string current_filepath_;
    int file_index_ = 0;
    int fd_ = -1;
    char* mapped_memory_ = nullptr;
    size_t file_size_ = 0;
    size_t write_pos_ = 0;
};

}