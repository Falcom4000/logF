#include "../include/logger.h"
#include "../include/consumer.h"
#include <thread>
#include <vector>

int main() {
    logF::MpscRingBuffer<logF::LogMessage> ring_buffer(8192);
    logF::Logger logger(ring_buffer);
    logF::Consumer consumer(ring_buffer, "logs", 1024 * 1024 * 32);

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