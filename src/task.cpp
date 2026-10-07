#include "cadence/task.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace cadence {

Task::Task(
    TaskId id,
    std::string name,
    Priority base_priority,
    std::vector<Operation> program,
    Tick release,
    std::optional<Tick> relative_deadline
)
    : id_(id),
      name_(std::move(name)),
      base_priority_(base_priority),
      effective_priority_(base_priority),
      release_(release),
      relative_deadline_(relative_deadline),
      program_(std::move(program)),
      state_(TaskState::New) {
    if (release < 0) {
        throw std::invalid_argument(
            "task release time cannot be negative"
        );
    }

    if (relative_deadline.has_value()) {
        if (relative_deadline.value() <= 0) {
            throw std::invalid_argument(
                "relative deadline must be positive"
            );
        }

        if (relative_deadline.value() >
            std::numeric_limits<Tick>::max() - release) {
            throw std::invalid_argument(
                "absolute deadline overflows Tick"
            );
        }
    }
}

std::string Task::to_string() const {
    std::string out =
        "Task " + std::to_string(id_) +
        " '" + name_ +
        "' prio=" + std::to_string(base_priority_) +
        " state=" + cadence::to_string(state_) +
        " program=[";

    for (std::size_t i = 0; i < program_.size(); ++i) {
        if (i > 0) {
            out += ", ";
        }

        out += program_[i].to_string();
    }

    out += "]";
    return out;
}

}  // namespace cadence