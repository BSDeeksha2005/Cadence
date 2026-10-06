#include "cadence/task.hpp"

#include <utility>

namespace cadence {

Task::Task(TaskId id, std::string name, Priority base_priority,
           std::vector<Operation> program)
    : id_(id),
      name_(std::move(name)),
      base_priority_(base_priority),
      program_(std::move(program)),
      state_(TaskState::New) {}

std::string Task::to_string() const {
    std::string out = "Task " + std::to_string(id_) + " '" + name_ +
                      "' prio=" + std::to_string(base_priority_) +
                      " state=" + cadence::to_string(state_) + " program=[";
    for (std::size_t i = 0; i < program_.size(); ++i) {
        if (i > 0) out += ", ";
        out += program_[i].to_string();
    }
    out += "]";
    return out;
}

}  // namespace cadence