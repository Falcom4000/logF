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
    // 科学计数法，保留四位有效数字
    if (num == 0.0) {
        append('0');
        return;
    }
    
    bool negative = num < 0;
    if (negative) {
        append('-');
        num = -num;
    }
    
    // 处理特殊值
    if (num != num) { // NaN
        append("nan");
        return;
    }
    if (num > 1e308) { // Infinity
        append("inf");
        return;
    }
    
    // 计算指数
    int exponent = 0;
    if (num >= 10.0) {
        while (num >= 10.0) {
            num /= 10.0;
            exponent++;
        }
    } else if (num < 1.0) {
        while (num < 1.0) {
            num *= 10.0;
            exponent--;
        }
    }
    
    // 现在 num 在 [1.0, 10.0) 范围内
    // 保留四位有效数字：1位整数 + 3位小数
    long long scaled = static_cast<long long>(num * 1000 + 0.5);
    
    // 处理进位情况（比如 9.999 四舍五入变成 10.000）
    if (scaled >= 10000) {
        scaled = 1000;
        exponent++;
    }
    
    long long integer_part = scaled / 1000;  // 应该总是1-9
    long long fractional_part = scaled % 1000;
    
    // 输出尾数部分
    append_number(integer_part);
    append('.');
    
    // 输出三位小数（保证四位有效数字）
    if (fractional_part < 100) append('0');
    if (fractional_part < 10) append('0');
    append_number(fractional_part);
    
    // 输出指数部分
    append('e');
    if (exponent >= 0) {
        append_number(static_cast<long long>(exponent));
    } else {
        append('-');
        append_number(static_cast<long long>(-exponent));
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