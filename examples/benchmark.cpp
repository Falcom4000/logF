#include "../include/logger.h"
#include "../src/consumer.cpp"
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <immintrin.h>
#include <iomanip>

constexpr int NUM_THREADS = 4;
constexpr int NUM_MESSAGES_PER_THREAD = 100000;

static inline uint64_t rdtscp() {
    uint64_t low, high;
    // a = low, d = high, c = processor id
    __asm__ __volatile__ (
        "rdtscp"
        : "=a"(low), "=d"(high)
        :: "%rcx"
    );
    return (high << 32) | low;
}
// Function to calculate P99 latency
double calculate_p99(std::vector<uint64_t>& data) {
    if (data.empty()) return 0.0;
    
    std::sort(data.begin(), data.end());
    size_t index = static_cast<size_t>(data.size() * 0.99);
    if (index >= data.size()) index = data.size() - 1;
    return static_cast<double>(data[index]);
}

int main() {
    logF::DoubleBuffer double_buffer(1024 * 1024 * 8 );
    logF::Logger logger(double_buffer);
    logF::Consumer consumer(double_buffer, "benchmark_log.txt");
    
    // Storage for all latency data from all threads
    std::vector<std::vector<uint64_t>> all_latencies(NUM_THREADS);

    consumer.start();

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&logger, &all_latencies, i]() {  
            std::vector<uint64_t> thread_latencies;
            thread_latencies.reserve(NUM_MESSAGES_PER_THREAD);
            
            for (int j = 0; j < NUM_MESSAGES_PER_THREAD; ++j) {
                uint64_t start_cycles = rdtscp();
                LOG_INFO(logger, "Thread %: message %, pi = %", i, j, 3.14159 + j);
                uint64_t end_cycles = rdtscp();
                thread_latencies.push_back(end_cycles - start_cycles);
                std::this_thread::sleep_for(std::chrono::nanoseconds(100));
            }
            
            // Move local latencies to global storage
            all_latencies[i] = std::move(thread_latencies);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    // Wait for the consumer to finish writing
    std::this_thread::sleep_for(std::chrono::seconds(5));
    uint64_t processed_messages = consumer.stop();

    uint64_t total_messages = NUM_THREADS * NUM_MESSAGES_PER_THREAD;
    std::cout << "等待Consumer处理完缓冲区中的消息..." << std::endl;
    
    // 等待一段时间让consumer处理，然后检查处理是否稳定
    uint64_t last_count = 0;
    int stable_count = 0;
    const int max_stable_checks = 10; // 连续10次检查处理数量没变化就认为处理完成
    
    while (stable_count < max_stable_checks) {
        uint64_t current_count = consumer.get_processed_count();
        
        if (current_count == last_count) {
            stable_count++;
        } else {
            stable_count = 0; // 重置计数器
            last_count = current_count;
        }
        std::this_thread::yield();
    }
    
    auto e2e_end_time = std::chrono::high_resolution_clock::now();
    uint64_t processed_messages = consumer.stop();
    double messages_per_second = total_messages / elapsed.count();

    double processed_rate = (total_messages > 0) ? 
        (static_cast<double>(processed_messages) / total_messages * 100.0) : 0.0;

    // Combine all latency data and calculate statistics
    std::vector<uint64_t> combined_latencies;
    for (const auto& thread_latencies : all_latencies) {
        combined_latencies.insert(combined_latencies.end(), 
                                thread_latencies.begin(), 
                                thread_latencies.end());
    }

    double p99_cycles = calculate_p99(combined_latencies);
    
    // Calculate average latency
    uint64_t total_cycles = 0;
    for (uint64_t cycles : combined_latencies) {
        total_cycles += cycles;
    }
    double avg_cycles = static_cast<double>(total_cycles) / combined_latencies.size();

    // Output results
    // 计算端到端时间
    std::chrono::duration<double> e2e_elapsed = e2e_end_time - start_time;
    
    std::cout << "=== Benchmark Results ===" << std::endl;
    std::cout << "Total messages sent: " << total_messages << std::endl;
    std::cout << "Processed rate: " << std::fixed << std::setprecision(4) << processed_rate << "%" << std::endl;
    std::cout << "Producer time: " << elapsed.count() << " seconds" << std::endl;
    std::cout << "End-to-end time: " << e2e_elapsed.count() << " seconds" << std::endl;
    std::cout << "Messages per second (producer): " << messages_per_second << std::endl;
    std::cout << "Messages per second (end-to-end): " << total_messages / e2e_elapsed.count() << std::endl;
    std::cout << "Average latency: " << avg_cycles << " cycles" << std::endl;
    std::cout << "P99 latency: " << p99_cycles << " cycles" << std::endl;
    
    // Explicitly clear large data structures to help with cleanup
    combined_latencies.clear();
    all_latencies.clear();
    
    return 0;
}