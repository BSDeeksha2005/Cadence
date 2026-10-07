#pragma once

#include "cadence/types.hpp"

namespace cadence {

inline bool higherThan(Priority a, Priority b) {
    return a > b;
}

}  // namespace cadence