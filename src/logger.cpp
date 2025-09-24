#include "../include/logger.h"

namespace logF {

std::atomic<uint64_t> Logger::next_sequence_number_(0);

Logger::Logger(DoubleBuffer& double_buffer) : double_buffer_(double_buffer) {}

} // namespace logF
