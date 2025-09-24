#pragma once

#include <vector>
#include <cstddef>

// Forward declaration
namespace logF {
    class MMapFileWriter;
}

namespace logF {

// Character ring buffer for efficient log formatting
class CharRingBuffer {
public:
    CharRingBuffer(size_t capacity = 65536); // 64KB default
    void append(const char* data, size_t len);
    void append(const char* str);
    void append(char c);
    void append_number(long long num);
    void append_number(double num);
    void flush_to_mmap(MMapFileWriter& writer);
    void clear();
    size_t size() const { return write_pos_; }
    bool has_space(size_t needed) const { return write_pos_ + needed < capacity_; }

private:
    std::vector<char> buffer_;
    size_t write_pos_ = 0;
    size_t capacity_;
};

}