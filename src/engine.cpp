#include "cadence/engine.hpp"

#include <stdexcept>
#include <utility>

namespace cadence {

std::size_t Engine::index_of(TaskId id) const {
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (slots_[i].task.id() == id) {
            return i;
        }
    }

    throw std::out_of_range("unknown task id");
}

const Task& Engine::task(TaskId id) const {
    return slots_[index_of(id)].task;
}

void Engine::add_task(Task task) {
    if (now_ != 0) {
        throw std::logic_error("tasks must be added before the first tick");
    }

    for (const Slot& s : slots_) {
        if (s.task.id() == task.id()) {
            throw std::invalid_argument("duplicate task id");
        }
    }

    const TaskId id = task.id();
    const Priority prio = task.base_priority();

    slots_.push_back(Slot{std::move(task), 0, 0, 0});
    slots_.back().task.set_state(TaskState::Ready);

    ready_.push(id, prio);
}

bool Engine::all_completed() const {
    for (const Slot& s : slots_) {
        if (s.task.state() != TaskState::Completed) {
            return false;
        }
    }

    return true;
}

// Looks at the task's current operation and applies anything that costs
// no CPU time. `at` is the tick boundary where this happens.
void Engine::settle(Slot& slot, Tick at) {
    if (slot.pc >= slot.task.program().size()) {
        slot.task.set_state(TaskState::Completed);
        return;
    }

    const Operation& op = slot.task.program()[slot.pc];

    switch (op.type()) {
        case OpType::Compute:
            if (slot.remaining == 0) {
                slot.remaining = op.ticks();
            }
            return;

        case OpType::Sleep:
            slot.task.set_state(TaskState::Sleeping);
            slot.wake_at = at + op.ticks();
            ++slot.pc;
            return;

        case OpType::Lock:
        case OpType::Unlock:
            throw std::logic_error("LOCK/UNLOCK not implemented yet");
    }
}

void Engine::wake_sleepers() {
    for (Slot& s : slots_) {
        if (s.task.state() == TaskState::Sleeping &&
            s.wake_at <= now_) {

            s.task.set_state(TaskState::Ready);

            // Newly READY tasks go to the tail.
            ready_.push(
                s.task.id(),
                s.task.base_priority()
            );
        }
    }
}

void Engine::dispatch() {
    while (running_ == kNone && !ready_.empty()) {
        const std::size_t idx = index_of(ready_.pop());
        Slot& s = slots_[idx];

        s.task.set_state(TaskState::Running);
        settle(s, now_);

        if (s.task.state() == TaskState::Running) {
            running_ = idx;
        }
    }
}

void Engine::preempt_if_needed() {
    if (running_ == kNone || ready_.empty()) {
        return;
    }

    Slot& cur = slots_[running_];

    if (ready_.peek_priority() > cur.task.base_priority()) {
        cur.task.set_state(TaskState::Ready);

        // A preempted task goes to the HEAD of its priority level.
        ready_.push_front(
            cur.task.id(),
            cur.task.base_priority()
        );

        running_ = kNone;
    }
}

void Engine::step() {
    wake_sleepers();
    preempt_if_needed();
    dispatch();

    if (running_ == kNone) {
        timeline_.push_back(kIdle);
    } else {
        Slot& s = slots_[running_];

        timeline_.push_back(s.task.id());

        --s.remaining;

        if (s.remaining == 0) {
            ++s.pc;

            settle(s, now_ + 1);

            if (s.task.state() != TaskState::Running) {
                running_ = kNone;
            }
        }
    }

    ++now_;
}

void Engine::run(Tick ticks) {
    for (Tick i = 0; i < ticks; ++i) {
        step();
    }
}

bool Engine::run_until_done(Tick max_ticks) {
    while (!all_completed() && now_ < max_ticks) {
        step();
    }

    return all_completed();
}

}  // namespace cadence