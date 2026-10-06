#include "cadence/operation.hpp"

#include <stdexcept>

namespace cadence {

Operation Operation::compute(Tick ticks) {
    if (ticks < 1) {
        throw std::invalid_argument("COMPUTE requires ticks >= 1");
    }
    return Operation(OpType::Compute, ticks, 0);
}

Operation Operation::lock(MutexId mutex) {
    return Operation(OpType::Lock, 0, mutex);
}

Operation Operation::unlock(MutexId mutex) {
    return Operation(OpType::Unlock, 0, mutex);
}

Operation Operation::sleep(Tick ticks) {
    if (ticks < 1) {
        throw std::invalid_argument("SLEEP requires ticks >= 1");
    }
    return Operation(OpType::Sleep, ticks, 0);
}

Tick Operation::ticks() const {
    if (type_ != OpType::Compute && type_ != OpType::Sleep) {
        throw std::logic_error("ticks() only valid for COMPUTE/SLEEP");
    }
    return ticks_;
}

MutexId Operation::mutex() const {
    if (type_ != OpType::Lock && type_ != OpType::Unlock) {
        throw std::logic_error("mutex() only valid for LOCK/UNLOCK");
    }
    return mutex_;
}

std::string Operation::to_string() const {
    switch (type_) {
        case OpType::Compute: return "COMPUTE(" + std::to_string(ticks_) + ")";
        case OpType::Lock:    return "LOCK(" + std::to_string(mutex_) + ")";
        case OpType::Unlock:  return "UNLOCK(" + std::to_string(mutex_) + ")";
        case OpType::Sleep:   return "SLEEP(" + std::to_string(ticks_) + ")";
    }
    return "UNKNOWN";
}

bool Operation::operator==(const Operation& other) const {
    return type_ == other.type_ && ticks_ == other.ticks_ &&
           mutex_ == other.mutex_;
}

}  // namespace cadence