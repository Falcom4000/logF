#include "../include/ring_buffer.h"
#include "../include/mmap_writer.h"
#include <cstddef> // For size_t
#include <cstring> // For memcpy, strlen
#include <cstdio>  // For snprintf

namespace logF {

// CharRingBuffer implementation
CharRingBuffer::CharRingBuffer(size_t capacity) 
    : capacity_(capacity), buffer_(capacity) {}

void CharRingBuffer::append(const char* data, size_t len) {
    if (write_pos_ + len >= capacity_) [[unlikely]] {
        // Buffer would overflow, this shouldn't happen in normal usage
        // For safety, just truncate
        len = capacity_ - write_pos_ - 1;
    }
    std::memcpy(&buffer_[write_pos_], data, len);
    write_pos_ += len;
}

void CharRingBuffer::append(const char* str) {
    if (str) [[likely]] {
        append(str, std::strlen(str));
    }
}

void CharRingBuffer::append(char c) {
    if (write_pos_ < capacity_ - 1) [[likely]] {
        buffer_[write_pos_++] = c;
    }
}

void CharRingBuffer::append_number(long long num) {
    char temp[32];
    int len = std::snprintf(temp, sizeof(temp), "%lld", num);
    if (len > 0) [[likely]] {
        append(temp, len);
    }
}

void CharRingBuffer::append_number(double num) {
    char temp[32];
    int len = std::snprintf(temp, sizeof(temp), "%g", num);
    if (len > 0) [[likely]] {
        append(temp, len);
    }
}

void CharRingBuffer::flush_to_mmap(MMapFileWriter& writer) {
    if (write_pos_ > 0) [[likely]] {
        writer.write(buffer_.data(), write_pos_);
        writer.write("\n", 1);
    }
}

void CharRingBuffer::clear() {
    write_pos_ = 0;
}

}