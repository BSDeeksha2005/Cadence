#pragma once

#include <string>

#include "cadence/types.hpp"

namespace cadence {

enum class OpType { Compute, Lock, Unlock, Sleep };

class Operation {
public:
    // Factory functions: the only way to build an Operation.
    static Operation compute(Tick ticks);    // ticks >= 1
    static Operation lock(MutexId mutex);
    static Operation unlock(MutexId mutex);
    static Operation sleep(Tick ticks);      // ticks >= 1

    OpType type() const { return type_; }

    Tick ticks() const;      // Compute/Sleep only, else throws std::logic_error
    MutexId mutex() const;   // Lock/Unlock only, else throws std::logic_error

    std::string to_string() const;

    bool operator==(const Operation& other) const;
    bool operator!=(const Operation& other) const { return !(*this == other); }

private:
    Operation(OpType type, Tick ticks, MutexId mutex)
        : type_(type), ticks_(ticks), mutex_(mutex) {}

    OpType type_;
    Tick ticks_;
    MutexId mutex_;
};

}  // namespace cadence