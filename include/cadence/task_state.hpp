#pragma once

#include <string>

namespace cadence {

enum class TaskState {
    New,
    Ready,
    Running,
    Blocked,
    Sleeping,
    Completed
};

std::string to_string(TaskState state);

}  // namespace cadence