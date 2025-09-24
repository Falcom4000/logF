#include "../include/ring_buffer.h"
#include <cstddef> // For size_t
#include <utility> // For std::move
#include <atomic>
#include <cstring> // For memcpy, strlen
#include <cstdio>  // For snprintf
#include <fstream> // For ofstream

namespace logF {

RingBuffer::RingBuffer(size_t capacity) : capacity_(capacity + 1), buffer_(capacity + 1) {}

bool RingBuffer::try_push(LogVariant item) {
    const auto current_head = head_.load(std::memory_order_acquire);
    const auto next_head = (current_head + 1) % capacity_;

    if (next_head == tail_.load(std::memory_order_acquire)) {
        return false; // buffer is full
    }

    buffer_[current_head] = std::move(item);
    head_.store(next_head, std::memory_order_release);
    return true;
}

std::optional<LogVariant> RingBuffer::try_pop() {
    const auto current_tail = tail_.load(std::memory_order_acquire);
    if (current_tail == head_.load(std::memory_order_acquire)) {
        return std::nullopt; // buffer is empty
    }

    LogVariant item = std::move(buffer_[current_tail]);
    tail_.store((current_tail + 1) % capacity_, std::memory_order_release);
    return item;
}

// CharRingBuffer implementation
CharRingBuffer::CharRingBuffer(size_t capacity) 
    : capacity_(capacity), buffer_(capacity) {}

void CharRingBuffer::append(const char* data, size_t len) {
    if (write_pos_ + len >= capacity_) {
        // Buffer would overflow, this shouldn't happen in normal usage
        // For safety, just truncate
        len = capacity_ - write_pos_ - 1;
    }
    std::memcpy(&buffer_[write_pos_], data, len);
    write_pos_ += len;
}

void CharRingBuffer::append(const char* str) {
    if (str) {
        append(str, std::strlen(str));
    }
}

void CharRingBuffer::append(char c) {
    if (write_pos_ < capacity_ - 1) {
        buffer_[write_pos_++] = c;
    }
}

void CharRingBuffer::append_number(long long num) {
    char temp[32];
    int len = std::snprintf(temp, sizeof(temp), "%lld", num);
    if (len > 0) {
        append(temp, len);
    }
}

void CharRingBuffer::append_number(double num) {
    char temp[32];
    int len = std::snprintf(temp, sizeof(temp), "%g", num);
    if (len > 0) {
        append(temp, len);
    }
}

void CharRingBuffer::flush_to_file(std::ofstream& file) {
    if (write_pos_ > 0) {
        file.write(buffer_.data(), write_pos_);
        file << '\n';
    }
}

void CharRingBuffer::clear() {
    write_pos_ = 0;
}

}