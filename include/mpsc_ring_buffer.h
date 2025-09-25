#pragma once

#include <iostream>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>
#include <memory>
#include <functional>
#include <cstdint> // for uint64_t
#include <iterator> // for std::iterator traits
#include <new>      // for placement new
#include <type_traits> // for std::is_trivially_destructible_v

/**
 * @brief 基于 LMAX Disruptor 思想的多生产者、单消费者无锁环形缓冲区。
 * 支持在缓冲区内部直接构造对象 (Emplace)。此版本修复了 MPSC 竞态条件。
 * @tparam T 存储在缓冲区中的元素类型。
 */
namespace logF {
template<typename T>
class MpscRingBuffer {
public:
    class ReadView;

    explicit MpscRingBuffer(size_t capacity);
    ~MpscRingBuffer();

    // Non-copyable, non-movable
    MpscRingBuffer(const MpscRingBuffer&) = delete;
    MpscRingBuffer& operator=(const MpscRingBuffer&) = delete;

    /**
     * @brief (多线程安全) 在缓冲区中直接构造一个对象。
     * @param args T 的构造函数所需的参数。
     * @return 如果构造成功则返回 true，如果缓冲区已满则返回 false。
     */
    template<typename... Args>
    bool emplace(Args&&... args);

    ReadView read();

    class ReadView {
    public:
        class iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using pointer = T*;
            using reference = T&;

            reference operator*() const {
                return *reinterpret_cast<T*>(&buffer_->buffer_[current_seq_ & buffer_->capacity_mask_]);
            }
            pointer operator->() const { return &operator*(); }
            iterator& operator++() { ++current_seq_; return *this; }
            iterator operator++(int) { iterator tmp = *this; ++(*this); return tmp; }
            bool operator==(const iterator& other) const { return current_seq_ == other.current_seq_; }
            bool operator!=(const iterator& other) const { return !(*this == other); }

        private:
            friend class ReadView;
            iterator(MpscRingBuffer<T>* buffer, uint64_t seq) : buffer_(buffer), current_seq_(seq) {}
            MpscRingBuffer<T>* buffer_;
            uint64_t current_seq_;
        };

        ReadView(const ReadView&) = delete;
        ReadView& operator=(const ReadView&) = delete;
        ReadView(ReadView&& other) noexcept
            : buffer_(other.buffer_), begin_seq_(other.begin_seq_), end_seq_(other.end_seq_) {
            other.buffer_ = nullptr;
        }
        ReadView& operator=(ReadView&& other) noexcept {
            if (this != &other) {
                release();
                buffer_ = other.buffer_;
                begin_seq_ = other.begin_seq_;
                end_seq_ = other.end_seq_;
                other.buffer_ = nullptr;
            }
            return *this;
        }
        ~ReadView() { release(); }

        iterator begin() const { return iterator(buffer_, begin_seq_); }
        iterator end() const { return iterator(buffer_, end_seq_); }
        size_t size() const { return end_seq_ - begin_seq_; }
        bool empty() const { return begin_seq_ == end_seq_; }

    private:
        friend class MpscRingBuffer<T>;
        ReadView(MpscRingBuffer<T>* buffer, uint64_t begin_seq, uint64_t end_seq)
            : buffer_(buffer), begin_seq_(begin_seq), end_seq_(end_seq) {}

        void release() {
            if (buffer_) {
                if constexpr (!std::is_trivially_destructible_v<T>) {
                    for (uint64_t i = begin_seq_; i < end_seq_; ++i) {
                        T* obj_ptr = reinterpret_cast<T*>(&buffer_->buffer_[i & buffer_->capacity_mask_]);
                        obj_ptr->~T();
                    }
                }
                buffer_->read_cursor_.store(end_seq_, std::memory_order_release);
            }
        }

        MpscRingBuffer<T>* buffer_;
        uint64_t begin_seq_;
        uint64_t end_seq_;
    };

private:
    using Storage = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

    const size_t capacity_;
    const size_t capacity_mask_;
    std::unique_ptr<Storage[]> buffer_;
    
    // 用于发布写入完成的序列号数组
    std::unique_ptr<std::atomic<uint64_t>[]> slot_sequences_;

    alignas(64) std::atomic<uint64_t> write_cursor_;
    alignas(64) std::atomic<uint64_t> read_cursor_;
};

// --- 实现 ---

template<typename T>
MpscRingBuffer<T>::MpscRingBuffer(size_t capacity)
    : capacity_(capacity),
      capacity_mask_(capacity - 1),
      buffer_(std::make_unique<Storage[]>(capacity)),
      slot_sequences_(std::make_unique<std::atomic<uint64_t>[]>(capacity)),
      write_cursor_(0),
      read_cursor_(0)
{
    if (capacity_ == 0 || (capacity_ & (capacity_ - 1)) != 0) {
        throw std::invalid_argument("Capacity must be a power of 2.");
    }
    for (size_t i = 0; i < capacity_; ++i) {
        slot_sequences_[i].store(i - capacity_, std::memory_order_relaxed);
    }
}

template<typename T>
MpscRingBuffer<T>::~MpscRingBuffer() {
    if constexpr (!std::is_trivially_destructible_v<T>) {
        const uint64_t write_pos = write_cursor_.load(std::memory_order_relaxed);
        const uint64_t read_pos = read_cursor_.load(std::memory_order_relaxed);
        for (uint64_t i = read_pos; i < write_pos; ++i) {
            T* obj_ptr = reinterpret_cast<T*>(&buffer_[i & capacity_mask_]);
            obj_ptr->~T();
        }
    }
}

template<typename T>
template<typename... Args>
bool MpscRingBuffer<T>::emplace(Args&&... args) {
    uint64_t current_write_seq;
    do {
        current_write_seq = write_cursor_.load(std::memory_order_relaxed);
        if (current_write_seq - read_cursor_.load(std::memory_order_acquire) >= capacity_) {
            return false;
        }
    } while (!write_cursor_.compare_exchange_weak(
        current_write_seq, current_write_seq + 1, 
        std::memory_order_release, std::memory_order_relaxed));

    new (&buffer_[current_write_seq & capacity_mask_]) T(std::forward<Args>(args)...);

    slot_sequences_[current_write_seq & capacity_mask_].store(current_write_seq, std::memory_order_release);
    
    return true;
}

template<typename T>
typename MpscRingBuffer<T>::ReadView MpscRingBuffer<T>::read() {
    const uint64_t current_read = read_cursor_.load(std::memory_order_relaxed);
    // 缓存一次 write_cursor，作为本次读取操作的上限，避免循环追赶。
    const uint64_t write_cursor_snapshot = write_cursor_.load(std::memory_order_acquire);
    
    uint64_t end_of_batch_seq = current_read;

    // 在 [current_read, write_cursor_snapshot) 范围内查找连续的已发布块
    while (end_of_batch_seq < write_cursor_snapshot &&
           (slot_sequences_[end_of_batch_seq & capacity_mask_].load(std::memory_order_acquire) == end_of_batch_seq)) {
        end_of_batch_seq++;
    }

    return ReadView(this, current_read, end_of_batch_seq);
}


} // namespace logF