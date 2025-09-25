#pragma once

#include <vector>
#include <atomic>
#include "log_message.h"

namespace logF {

struct BufferView {
    const LogMessage* data;
    size_t size;
    
    const LogMessage* begin() const { return data; }
    const LogMessage* end() const { return data + size; }
};

class DoubleBuffer {
public:
    DoubleBuffer(size_t capacity);

    template<typename F>
    void write(F&& f) {
        // Atomically increment position, get old state
        uint64_t old_state = write_state_.fetch_add(1, std::memory_order_acq_rel);
        
        // Unpack old state
        int buffer_idx = (old_state & INDEX_BIT) ? 1 : 0;
        size_t pos = old_state & POSITION_MASK;

        // If capacity is exceeded, do nothing (drop the message)
        [[unlikely]]if (pos >= capacity_) {
            return;
        }
        
        // Invoke the function on the object at the allocated position
        f(buffers_[buffer_idx][pos]);
    }

    BufferView read_and_swap();

private:
    // Constants for state packing
    static constexpr uint64_t INDEX_BIT = 1ULL << 63;
    static constexpr uint64_t POSITION_MASK = ~(1ULL << 63);

    // Non-atomic members first
    std::vector<LogMessage> buffers_[2];
    size_t capacity_;
    
    // Atomic state for write index and position
    alignas(64) std::atomic<uint64_t> write_state_{0};
};

}