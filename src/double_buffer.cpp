#include "../include/double_buffer.h"
#include <atomic>
#include <utility>
#include <shared_mutex>
#include <mutex>

namespace logF {

DoubleBuffer::DoubleBuffer(size_t capacity) : capacity_(capacity) {
    buffers_[0].resize(capacity);
    buffers_[1].resize(capacity);
}
    
BufferView DoubleBuffer::read_and_swap() {
    uint64_t old_state = write_state_.load(std::memory_order_acquire);
    
    while (true) {
        size_t write_pos = old_state & POSITION_MASK;
        if (write_pos == 0) {
            // No new messages to read
            return BufferView{nullptr, 0};
        }

        // Prepare new state: flip index bit, reset position to 0
        uint64_t new_state = (old_state ^ INDEX_BIT) & INDEX_BIT;

        // Atomically swap the state
        if (write_state_.compare_exchange_weak(old_state, new_state, 
                                               std::memory_order_acq_rel, 
                                               std::memory_order_acquire)) {
            // Success! Unpack old state to return the correct buffer view
            int read_idx = (old_state & INDEX_BIT) ? 1 : 0;
            return BufferView{
                buffers_[read_idx].data(),
                std::min(write_pos, capacity_)
            };
        }
        // If CAS fails, old_state is automatically updated with the current value.
        // The loop will retry with the fresh value.
    }
}

}