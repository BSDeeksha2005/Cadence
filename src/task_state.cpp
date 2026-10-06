#include "cadence/task_state.hpp"

namespace cadence {

std::string to_string(TaskState state) {
    switch (state) {
        case TaskState::New:       return "NEW";
        case TaskState::Ready:     return "READY";
        case TaskState::Running:   return "RUNNING";
        case TaskState::Blocked:   return "BLOCKED";
        case TaskState::Sleeping:  return "SLEEPING";
        case TaskState::Completed: return "COMPLETED";
    }
    return "UNKNOWN";
}

}  // namespace cadence