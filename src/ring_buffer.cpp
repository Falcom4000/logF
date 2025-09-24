#include "../include/ring_buffer.h"
#include <cstddef> // For size_t
#include <utility> // For std::move
#include <atomic>

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

}