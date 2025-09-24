#include "../include/logger.h"
#include "../src/consumer.cpp"
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>

constexpr int NUM_THREADS = 8;
constexpr int NUM_MESSAGES_PER_THREAD = 1000000;

int main() {
    logF::DoubleBuffer double_buffer(8192 * 10 * 10 );
    logF::Logger logger(double_buffer);
    logF::Consumer consumer(double_buffer, "benchmark_log.txt");

    consumer.start();

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&logger, i]() {  // 按值捕获 i，按引用捕获 logger
            for (int j = 0; j < NUM_MESSAGES_PER_THREAD; ++j) {
                LOG_INFO(logger, "Thread %: message %", i, j);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    // Wait for the consumer to finish writing
    std::this_thread::sleep_for(std::chrono::seconds(2));
    consumer.stop();

    long long total_messages = NUM_THREADS * NUM_MESSAGES_PER_THREAD;
    double messages_per_second = total_messages / elapsed.count();
    
    const auto& stats = consumer.get_stats();

    std::cout << "--- Benchmark Results ---" << std::endl;
    std::cout << "Threads: " << NUM_THREADS << std::endl;
    std::cout << "Messages per thread: " << NUM_MESSAGES_PER_THREAD << std::endl;
    std::cout << "Total messages sent: " << total_messages << std::endl;
    std::cout << "Total messages received: " << stats.total_messages << std::endl;
    std::cout << "Dropped messages: " << stats.dropped_messages << std::endl;
    std::cout << "Drop rate: " << (stats.dropped_messages * 100.0 / total_messages) << "%" << std::endl;
    std::cout << "Total time: " << elapsed.count() << " seconds" << std::endl;
    std::cout << "Messages per second: " << messages_per_second << std::endl;
    
    std::cout << "\n--- Frontend Latency (cycles) ---" << std::endl;
    if (stats.total_messages > 0) {
        std::cout << "Min: " << stats.min_frontend_latency << std::endl;
        std::cout << "Max: " << stats.max_frontend_latency << std::endl;
        std::cout << "Avg: " << (stats.total_frontend_latency / stats.total_messages) << std::endl;
    }
    
    std::cout << "\n--- End-to-End Latency (cycles) ---" << std::endl;
    if (stats.total_messages > 0) {
        std::cout << "Min: " << stats.min_end_to_end_latency << std::endl;
        std::cout << "Max: " << stats.max_end_to_end_latency << std::endl;
        std::cout << "Avg: " << (stats.total_end_to_end_latency / stats.total_messages) << std::endl;
    }


    return 0;
}