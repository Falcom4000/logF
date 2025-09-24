#pragma once

#include <variant>
#include <string>

namespace logF {
    using LogVariant = std::variant<int, long, double, std::string>;
}