#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "cadence/operation.hpp"
#include "cadence/task_state.hpp"
#include "cadence/types.hpp"

namespace cadence {

class Task {
public:
    Task(TaskId id, std::string name, Priority base_priority,
         std::vector<Operation> program);

    TaskId id() const { return id_; }
    const std::string& name() const { return name_; }

    Priority base_priority() const {
        return base_priority_;
    }

    Priority effective_priority() const {
        return effective_priority_;
    }

    void set_effective_priority(Priority priority) {
        effective_priority_ = priority;
    }

    const std::vector<Operation>& program() const {
        return program_;
    }

    std::size_t program_size() const {
        return program_.size();
    }

    TaskState state() const {
        return state_;
    }

    void set_state(TaskState state) {
        state_ = state;
    }

    std::string to_string() const;

private:
    TaskId id_;
    std::string name_;
    Priority base_priority_;
    Priority effective_priority_;
    std::vector<Operation> program_;
    TaskState state_;
};

}  // namespace cadence