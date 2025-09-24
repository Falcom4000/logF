#pragma once

#include <vector>
#include <optional>
#include <atomic>
#include "variant.h"

namespace logF {

class RingBuffer {
public:
    RingBuffer(size_t capacity);
    bool try_push(LogVariant item);
    std::optional<LogVariant> try_pop();

private:
    size_t capacity_;
    std::atomic<size_t> head_ = 0;
    std::atomic<size_t> tail_ = 0;
    std::vector<LogVariant> buffer_;
};

}