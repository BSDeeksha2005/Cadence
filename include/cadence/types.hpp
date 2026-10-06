#pragma once

#include <cstdint>
inline constexpr TaskId kIdle = -1;  // timeline marker: nobody ran

namespace cadence {

using Tick     = std::int64_t;
using TaskId   = std::int32_t;
using MutexId  = std::int32_t;
using Priority = std::int32_t;

}  // namespace cadence