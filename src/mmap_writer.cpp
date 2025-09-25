#include "../include/mmap_writer.h"
#include <iostream>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <sys/stat.h>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace logF {

MMapFileWriter::MMapFileWriter(const std::string& log_dir, size_t file_size)
    : log_dir_(log_dir), file_size_(file_size) {
    // Ensure the log directory exists
    mkdir(log_dir_.c_str(), 0755);
}

MMapFileWriter::~MMapFileWriter() {
    close();
}

MMapFileWriter::MMapFileWriter(MMapFileWriter&& other) noexcept
    : log_dir_(std::move(other.log_dir_))
    , current_filepath_(std::move(other.current_filepath_))
    , file_index_(other.file_index_)
    , fd_(other.fd_)
    , mapped_memory_(other.mapped_memory_)
    , file_size_(other.file_size_)
    , write_pos_(other.write_pos_) {
    
    other.fd_ = -1;
    other.mapped_memory_ = nullptr;
    other.file_size_ = 0;
    other.write_pos_ = 0;
    other.file_index_ = 0;
}

MMapFileWriter& MMapFileWriter::operator=(MMapFileWriter&& other) noexcept {
    if (this != &other) {
        close();
        
        log_dir_ = std::move(other.log_dir_);
        current_filepath_ = std::move(other.current_filepath_);
        file_index_ = other.file_index_;
        fd_ = other.fd_;
        mapped_memory_ = other.mapped_memory_;
        file_size_ = other.file_size_;
        write_pos_ = other.write_pos_;
        
        other.fd_ = -1;
        other.mapped_memory_ = nullptr;
        other.file_size_ = 0;
        other.write_pos_ = 0;
        other.file_index_ = 0;
    }
    return *this;
}

void MMapFileWriter::generate_new_filepath() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm = *std::localtime(&in_time_t);

    char filepath_buffer[256]; // 在栈上分配足够大的缓冲区

    // 格式化日期部分
    char date_buffer[11]; // YYYY-MM-DD\0
    std::strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d", &local_tm);

    // 使用 snprintf 高效、安全地拼接所有部分
    int len = std::snprintf(filepath_buffer, sizeof(filepath_buffer),
                            "%s/%s_%d.log",
                            log_dir_.c_str(),
                            date_buffer,
                            file_index_++);

    // 检查是否发生截断（虽然不太可能）
    if (len > 0 && static_cast<size_t>(len) < sizeof(filepath_buffer)) {
        current_filepath_.assign(filepath_buffer, len);
    } else {
        // 异常处理：如果路径太长，回退到 stringstream
        std::stringstream ss;
        ss << log_dir_ << "/" << date_buffer << "_" << (file_index_ - 1) << ".log";
        current_filepath_ = ss.str();
    }
}

bool MMapFileWriter::open() {
    generate_new_filepath();
    
    fd_ = ::open(current_filepath_.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd_ == -1) [[unlikely]] {
        std::cerr << "Failed to open file " << current_filepath_ << ": " << std::strerror(errno) << std::endl;
        return false;
    }
    
    if (ftruncate(fd_, file_size_) == -1) [[unlikely]] {
        std::cerr << "Failed to expand file: " << std::strerror(errno) << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    
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
    
    write_pos_ = 0;
    return true;
}

void MMapFileWriter::close() {
    if (mapped_memory_ != nullptr) {
        msync(mapped_memory_, write_pos_, MS_SYNC);
        
        if (write_pos_ > 0 && write_pos_ < file_size_) {
            if (ftruncate(fd_, write_pos_) == -1) {
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
    
    // Don't reset file_size_ here, it's needed for the next file
    write_pos_ = 0;
}

bool MMapFileWriter::rotate_file() {
    close();
    return open();
}

bool MMapFileWriter::write(const char* data, size_t len) {
    if (!is_open() || len == 0) [[unlikely]] {
        return false;
    }
    
    if (write_pos_ + len > file_size_) [[unlikely]] {
        if (!rotate_file()) {
            return false;
        }
    }
    
    std::memcpy(mapped_memory_ + write_pos_, data, len);
    write_pos_ += len;
    
    return true;
}

void MMapFileWriter::flush() {
    if (is_open()) {
        msync(mapped_memory_, write_pos_, MS_ASYNC);
    }
}

}