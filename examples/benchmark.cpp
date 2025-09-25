#include "../include/logger.h"
#include "../src/consumer.cpp"
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <immintrin.h>
#include <iomanip>
#include <fstream>

constexpr int NUM_THREADS = 8;
constexpr int NUM_MESSAGES_PER_THREAD = 1000000;

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
    //  落盘分析
    std::ofstream ofs("latency_analysis.txt");
    for (const auto& latency : data) {
        ofs << latency << "\n";
    }
    return static_cast<double>(data[index]);
}

int main() {
    logF::DoubleBuffer double_buffer(1024 * 32 );
    logF::Logger logger(double_buffer);
    logF::Consumer consumer(double_buffer, "logs", 1024 * 1024 * 32);
    
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
                if (j % 10 == 0){
                    std::this_thread::sleep_for(std::chrono::nanoseconds(100));
                }
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

    uint64_t total_messages = NUM_THREADS * NUM_MESSAGES_PER_THREAD;
    std::this_thread::sleep_for(std::chrono::seconds(1)); // Ensure all messages are processed
    consumer.stop();
    double messages_per_second = consumer.get_processed_count() / elapsed.count();
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
        double processed_rate = (total_messages > 0) ? 
        (static_cast<double>(consumer.get_processed_count()) / total_messages * 100.0) : 0.0;
    // Output results
    std::cout << "=== Benchmark Results ===" << std::endl;
    std::cout << "Total messages sent: " << total_messages << std::endl;
    std::cout << "Processed rate: " << std::fixed << std::setprecision(7) << processed_rate << "%" << std::endl;
    std::cout << "Producer time: " << elapsed.count() << " seconds" << std::endl;
    std::cout << "Messages per second: " << messages_per_second << std::endl;
    std::cout << "Average latency: " << avg_cycles << " cycles" << std::endl;
    std::cout << "P99 latency: " << p99_cycles << " cycles" << std::endl;
    
    // Explicitly clear large data structures to help with cleanup
    combined_latencies.clear();
    all_latencies.clear();
    
    return 0;
}