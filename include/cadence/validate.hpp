#pragma once

#include <string>
#include <vector>

#include "cadence/scenario.hpp"

namespace cadence {

enum class ValidationCode {
    DuplicateTaskId,
    DuplicateMutexId,
    UnknownMutex,
    PriorityOutOfRange,
    NegativeRelease,
    NonpositiveDuration,
    NoCompute,
    UnlockNotHeld,
    RelockHeld,
    EndWhileHolding,
    TooManyTasks,
    TooManyMutexes,
    MissingHorizon
};

const char* to_string(ValidationCode code);

struct ValidationError {
    ValidationCode code;
    std::string path;
    std::string message;
};

struct ValidationResult {
    std::vector<ValidationError> errors;

    bool valid() const {
        return errors.empty();
    }
};

ValidationResult validate(const Scenario& scenario);

}  // namespace cadence