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
    if (num == 0) {
        append('0');
        return;
    }
    
    char temp[32];
    char* p = temp + sizeof(temp) - 1;
    *p = '\0';
    
    bool negative = num < 0;
    if (negative) num = -num;
    
    // 快速整数转换
    while (num > 0) {
        *--p = '0' + (num % 10);
        num /= 10;
    }
    
    if (negative) *--p = '-';
    
    append(p, temp + sizeof(temp) - 1 - p);
}

void CharRingBuffer::append_number(double num) {
    // 简化的double处理，适合日志场景
    if (num == 0.0) {
        append('0');
        return;
    }
    
    if (num < 0) {
        append('-');
        num = -num;
    }
    
    // 转换为整数处理（保留3位小数）
    long long scaled = static_cast<long long>(num * 1000 + 0.5);
    long long integer_part = scaled / 1000;
    long long fractional_part = scaled % 1000;
    
    append_number(integer_part);
    
    if (fractional_part > 0) {
        append('.');
        if (fractional_part < 100) append('0');
        if (fractional_part < 10) append('0');
        append_number(fractional_part);
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