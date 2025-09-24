#include "../include/logger.h"
#include "../src/consumer.cpp"
#include <thread>
#include <vector>

int main() {
    logF::DoubleBuffer double_buffer(8192);
    logF::Logger logger(double_buffer);
    logF::Consumer consumer(double_buffer, "log.txt");

    consumer.start();

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 1000; ++j) {
                LOG_INFO(logger, "This is a test message, number % and string %", j, "hello");
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    consumer.stop();

    return 0;
}