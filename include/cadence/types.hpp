#pragma once

#include <cstdint>

namespace cadence {

using Tick = std::int64_t;
using TaskId = std::int32_t;
using MutexId = std::int32_t;
using Priority = std::int32_t;

inline constexpr TaskId kIdle = -1;

}  // namespace cadence