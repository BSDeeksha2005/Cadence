#include "cadence/validate.hpp"

#include <algorithm>
#include <set>
#include <sstream>

namespace cadence {

const char* to_string(ValidationCode code) {
    switch (code) {
        case ValidationCode::DuplicateTaskId:
            return "DUPLICATE_TASK_ID";

        case ValidationCode::DuplicateMutexId:
            return "DUPLICATE_MUTEX_ID";

        case ValidationCode::UnknownMutex:
            return "UNKNOWN_MUTEX";

        case ValidationCode::PriorityOutOfRange:
            return "PRIORITY_OUT_OF_RANGE";

        case ValidationCode::NegativeRelease:
            return "NEGATIVE_RELEASE";

        case ValidationCode::NonpositiveDuration:
            return "NONPOSITIVE_DURATION";

        case ValidationCode::NoCompute:
            return "NO_COMPUTE";

        case ValidationCode::UnlockNotHeld:
            return "UNLOCK_NOT_HELD";

        case ValidationCode::RelockHeld:
            return "RELOCK_HELD";

        case ValidationCode::EndWhileHolding:
            return "END_WHILE_HOLDING";

        case ValidationCode::TooManyTasks:
            return "TOO_MANY_TASKS";

        case ValidationCode::TooManyMutexes:
            return "TOO_MANY_MUTEXES";

        case ValidationCode::MissingHorizon:
            return "MISSING_HORIZON";
    }

    return "UNKNOWN";
}

ValidationResult validate(const Scenario& scenario) {
    ValidationResult result;

    if (!scenario.horizon.has_value()) {
        result.errors.push_back({
            ValidationCode::MissingHorizon,
            "horizon",
            "scenario horizon is required"
        });
    } else if (scenario.horizon.value() < 0) {
        result.errors.push_back({
            ValidationCode::NonpositiveDuration,
            "horizon",
            "scenario horizon cannot be negative"
        });
    }

    if (scenario.tasks.size() > 64) {
        result.errors.push_back({
            ValidationCode::TooManyTasks,
            "tasks",
            "scenario exceeds the 64-task cap"
        });
    }

    if (scenario.mutexes.size() > 32) {
        result.errors.push_back({
            ValidationCode::TooManyMutexes,
            "mutexes",
            "scenario exceeds the 32-mutex cap"
        });
    }

    std::set<TaskId> task_ids;

    for (std::size_t i = 0; i < scenario.tasks.size(); ++i) {
        const Task& task = scenario.tasks[i];

        if (!task_ids.insert(task.id()).second) {
            result.errors.push_back({
                ValidationCode::DuplicateTaskId,
                "tasks[" + std::to_string(i) + "].id",
                "duplicate task id"
            });
        }

        if (task.base_priority() < 1 ||
            task.base_priority() > 32) {
            result.errors.push_back({
                ValidationCode::PriorityOutOfRange,
                "tasks[" + std::to_string(i) + "].priority",
                "priority must be in [1, 32]"
            });
        }

        if (task.release() < 0) {
            result.errors.push_back({
                ValidationCode::NegativeRelease,
                "tasks[" + std::to_string(i) + "].release",
                "release cannot be negative"
            });
        }

        if (task.relative_deadline().has_value() &&
            task.relative_deadline().value() <= 0) {
            result.errors.push_back({
                ValidationCode::NonpositiveDuration,
                "tasks[" + std::to_string(i) + "].deadline",
                "deadline must be positive"
            });
        }
    }

    std::set<MutexId> mutex_ids;

    for (std::size_t i = 0; i < scenario.mutexes.size(); ++i) {
        const MutexId mutex = scenario.mutexes[i];

        if (mutex < 0) {
            result.errors.push_back({
                ValidationCode::DuplicateMutexId,
                "mutexes[" + std::to_string(i) + "]",
                "mutex id must be non-negative"
            });
        }

        if (!mutex_ids.insert(mutex).second) {
            result.errors.push_back({
                ValidationCode::DuplicateMutexId,
                "mutexes[" + std::to_string(i) + "]",
                "duplicate mutex id"
            });
        }
    }

    for (std::size_t i = 0; i < scenario.tasks.size(); ++i) {
        const Task& task = scenario.tasks[i];

        std::set<MutexId> held;
        bool saw_compute = false;

        for (std::size_t pc = 0;
             pc < task.program_size();
             ++pc) {
            const Operation& op = task.program()[pc];

            const std::string path =
                "tasks[" + std::to_string(i) +
                "].program[" + std::to_string(pc) + "]";

            if (op.type() == OpType::Compute) {
                saw_compute = true;

                if (op.ticks() <= 0) {
                    result.errors.push_back({
                        ValidationCode::NonpositiveDuration,
                        path,
                        "COMPUTE duration must be positive"
                    });
                }
            } else if (op.type() == OpType::Sleep) {
                if (op.ticks() <= 0) {
                    result.errors.push_back({
                        ValidationCode::NonpositiveDuration,
                        path,
                        "SLEEP duration must be positive"
                    });
                }
            } else {
                const MutexId mutex = op.mutex();

                if (!mutex_ids.count(mutex)) {
                    result.errors.push_back({
                        ValidationCode::UnknownMutex,
                        path,
                        "operation refers to an undeclared mutex"
                    });
                }

                if (op.type() == OpType::Lock) {
                    if (held.count(mutex) != 0) {
                        result.errors.push_back({
                            ValidationCode::RelockHeld,
                            path,
                            "recursive lock is invalid"
                        });
                    } else {
                        held.insert(mutex);
                    }
                } else {
                    if (held.count(mutex) == 0) {
                        result.errors.push_back({
                            ValidationCode::UnlockNotHeld,
                            path,
                            "UNLOCK names a mutex not held on this path"
                        });
                    } else {
                        held.erase(mutex);
                    }
                }
            }
        }

        if (!saw_compute) {
            result.errors.push_back({
                ValidationCode::NoCompute,
                "tasks[" + std::to_string(i) + "].program",
                "task program must contain at least one COMPUTE"
            });
        }

        if (!held.empty()) {
            result.errors.push_back({
                ValidationCode::EndWhileHolding,
                "tasks[" + std::to_string(i) + "].program",
                "program ends while holding one or more mutexes"
            });
        }
    }

    return result;
}

}  // namespace cadence