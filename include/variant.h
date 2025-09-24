#pragma once

#include <variant>

namespace logF {
    using LogVariant = std::variant<int, long, double, const char*>;
}