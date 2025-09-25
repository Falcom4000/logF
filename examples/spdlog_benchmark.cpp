#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
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
    __asm__ __volatile__ (
        "rdtscp"
        : "=a"(low), "=d"(high)
        :: "%rcx"
    );
    return (high << 32) | low;
}

double calculate_p99(std::vector<uint64_t>& data) {
    if (data.empty()) return 0.0;
    
    std::sort(data.begin(), data.end());
    size_t index = static_cast<size_t>(data.size() * 0.99);
    if (index >= data.size()) index = data.size() - 1;
    
    std::ofstream ofs("spdlog_latency_analysis.txt");
    for (const auto& latency : data) {
        ofs << latency << "\n";
    }
    return static_cast<double>(data[index]);
}

int main() {
    try {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/spdlog_benchmark.log", true);
        // Increase queue size to 128k and use non-blocking overflow policy
        spdlog::init_thread_pool(131072, 1);
        auto logger = std::make_shared<spdlog::async_logger>("spdlog_async_logger", file_sink, spdlog::thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
        spdlog::register_logger(logger);

        std::vector<std::vector<uint64_t>> all_latencies(NUM_THREADS);

        auto start_time = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> threads;
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([&logger, &all_latencies, i]() {
                std::vector<uint64_t> thread_latencies;
                thread_latencies.reserve(NUM_MESSAGES_PER_THREAD);
                
                for (int j = 0; j < NUM_MESSAGES_PER_THREAD; ++j) {
                    uint64_t start_cycles = rdtscp();
                    logger->info("Thread {}: message {}, pi = {}", i, j, 3.14159 + j);
                    uint64_t end_cycles = rdtscp();
                    thread_latencies.push_back(end_cycles - start_cycles);
                    if (j % 10 == 0){
                        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
                    }
                }
                all_latencies[i] = std::move(thread_latencies);
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end_time - start_time;

        spdlog::drop_all(); 

        uint64_t total_messages = NUM_THREADS * NUM_MESSAGES_PER_THREAD;
        double messages_per_second = total_messages / elapsed.count();

        std::vector<uint64_t> combined_latencies;
        for (const auto& thread_latencies : all_latencies) {
            combined_latencies.insert(combined_latencies.end(), 
                                    thread_latencies.begin(), 
                                    thread_latencies.end());
        }

        double p99_cycles = calculate_p99(combined_latencies);
        
        uint64_t total_cycles = 0;
        for (uint64_t cycles : combined_latencies) {
            total_cycles += cycles;
        }
        double avg_cycles = static_cast<double>(total_cycles) / combined_latencies.size();

        std::cout << "--- Spdlog Benchmark ---" << std::endl;
        std::cout << "Total messages: " << total_messages << std::endl;
        std::cout << "Elapsed time: " << std::fixed << std::setprecision(3) << elapsed.count() << " s" << std::endl;
        std::cout << "Messages per second: " << std::fixed << std::setprecision(0) << messages_per_second << std::endl;
        std::cout << "Average latency: " << std::fixed << std::setprecision(2) << avg_cycles << " cycles" << std::endl;
        std::cout << "P99 latency: " << std::fixed << std::setprecision(2) << p99_cycles << " cycles" << std::endl;

    } catch (const spdlog::spdlog_ex& ex) {
        std::cout << "Log init failed: " << ex.what() << std::endl;
    }

    return 0;
}
