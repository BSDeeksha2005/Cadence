#include "cadence/engine.hpp"

#include <algorithm>
#include <cstddef>
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

Engine::MutexSlot& Engine::get_or_create_mutex(MutexId id) {
    for (MutexSlot& mutex : mutexes_) {
        if (mutex.id == id) {
            return mutex;
        }
    }

    if (id < 0) {
        throw std::invalid_argument(
            "mutex id must be non-negative"
        );
    }

    mutexes_.push_back(MutexSlot{
        id,
        kIdle,
        {}
    });

    return mutexes_.back();
}

const Engine::MutexSlot* Engine::find_mutex(MutexId id) const {
    for (const MutexSlot& mutex : mutexes_) {
        if (mutex.id == id) {
            return &mutex;
        }
    }

    return nullptr;
}

TaskId Engine::mutex_owner(MutexId id) const {
    const MutexSlot* mutex = find_mutex(id);

    if (mutex == nullptr) {
        return kIdle;
    }

    return mutex->owner;
}

std::vector<TaskId> Engine::mutex_waiters(MutexId id) const {
    const MutexSlot* mutex = find_mutex(id);

    if (mutex == nullptr) {
        return {};
    }

    return mutex->waiters;
}

void Engine::add_task(Task task) {
    if (now_ != 0) {
        throw std::logic_error(
            "tasks must be added before the first tick"
        );
    }

    for (const Slot& slot : slots_) {
        if (slot.task.id() == task.id()) {
            throw std::invalid_argument(
                "duplicate task id"
            );
        }
    }

    const TaskId id = task.id();
    const Priority priority = task.base_priority();

    slots_.push_back(Slot{
        std::move(task),
        0,
        0,
        0,
        kNoMutex,
        {}
    });

    slots_.back().task.set_state(TaskState::Ready);
    ready_.push(id, priority);
}

bool Engine::all_completed() const {
    for (const Slot& slot : slots_) {
        if (slot.task.state() != TaskState::Completed) {
            return false;
        }
    }

    return true;
}

bool Engine::holds_mutex(
    const Slot& slot,
    MutexId mutex
) const {
    return std::find(
        slot.held_mutexes.begin(),
        slot.held_mutexes.end(),
        mutex
    ) != slot.held_mutexes.end();
}

void Engine::add_held_mutex(
    Slot& slot,
    MutexId mutex
) {
    if (holds_mutex(slot, mutex)) {
        throw std::logic_error(
            "task already holds mutex"
        );
    }

    slot.held_mutexes.push_back(mutex);
}

void Engine::remove_held_mutex(
    Slot& slot,
    MutexId mutex
) {
    const auto it = std::find(
        slot.held_mutexes.begin(),
        slot.held_mutexes.end(),
        mutex
    );

    if (it == slot.held_mutexes.end()) {
        throw std::logic_error(
            "task does not hold mutex"
        );
    }

    slot.held_mutexes.erase(it);
}

std::size_t Engine::choose_waiter_index(
    const MutexSlot& mutex
) const {
    if (mutex.waiters.empty()) {
        throw std::logic_error(
            "cannot choose waiter from empty list"
        );
    }

    std::size_t best = 0;

    for (std::size_t i = 1;
         i < mutex.waiters.size();
         ++i) {
        const TaskId candidate_id = mutex.waiters[i];
        const TaskId best_id = mutex.waiters[best];

        const Priority candidate_priority =
            task(candidate_id).base_priority();

        const Priority best_priority =
            task(best_id).base_priority();

        if (candidate_priority > best_priority) {
            best = i;
        }
    }

    return best;
}

void Engine::wake_sleepers() {
    for (Slot& slot : slots_) {
        if (slot.task.state() == TaskState::Sleeping &&
            slot.wake_at <= now_) {

            slot.task.set_state(TaskState::Ready);

            ready_.push(
                slot.task.id(),
                slot.task.base_priority()
            );
        }
    }
}

void Engine::preempt_if_needed() {
    if (running_ == kNone || ready_.empty()) {
        return;
    }

    Slot& current = slots_[running_];

    if (ready_.peek_priority() >
        current.task.base_priority()) {

        current.task.set_state(TaskState::Ready);

        ready_.push_front(
            current.task.id(),
            current.task.base_priority()
        );

        running_ = kNone;
    }
}

void Engine::resolve() {
    while (true) {
        if (running_ == kNone) {
            if (ready_.empty()) {
                return;
            }

            const TaskId next = ready_.pop();
            running_ = index_of(next);

            slots_[running_].task.set_state(
                TaskState::Running
            );
        } else {
            preempt_if_needed();

            if (running_ == kNone) {
                continue;
            }
        }

        Slot& slot = slots_[running_];

        // Implicit END.
        if (slot.pc >= slot.task.program().size()) {
            if (!slot.held_mutexes.empty()) {
                throw std::logic_error(
                    "task completed while holding a mutex"
                );
            }

            slot.task.set_state(TaskState::Completed);
            running_ = kNone;
            continue;
        }

        const Operation& op =
            slot.task.program()[slot.pc];

        switch (op.type()) {
            case OpType::Compute: {
                if (slot.remaining == 0) {
                    slot.remaining = op.ticks();
                }

                return;
            }

            case OpType::Sleep: {
                slot.task.set_state(TaskState::Sleeping);
                slot.wake_at = now_ + op.ticks();
                ++slot.pc;
                running_ = kNone;
                continue;
            }

            case OpType::Lock: {
                const MutexId mutex_id = op.mutex();

                MutexSlot& mutex =
                    get_or_create_mutex(mutex_id);

                const TaskId task_id = slot.task.id();

                if (mutex.owner == task_id) {
                    throw std::logic_error(
                        "recursive mutex lock"
                    );
                }

                if (mutex.owner == kIdle) {
                    mutex.owner = task_id;

                    add_held_mutex(
                        slot,
                        mutex_id
                    );

                    ++slot.pc;
                    continue;
                }

                slot.task.set_state(TaskState::Blocked);
                slot.blocked_on = mutex_id;
                mutex.waiters.push_back(task_id);
                running_ = kNone;

                continue;
            }

            case OpType::Unlock: {
                const MutexId mutex_id = op.mutex();

                MutexSlot& mutex =
                    get_or_create_mutex(mutex_id);

                const TaskId task_id = slot.task.id();

                if (mutex.owner != task_id) {
                    throw std::logic_error(
                        "task does not own mutex"
                    );
                }

                ++slot.pc;

                remove_held_mutex(
                    slot,
                    mutex_id
                );

                if (mutex.waiters.empty()) {
                    mutex.owner = kIdle;
                    continue;
                }

                const std::size_t waiter_index =
                    choose_waiter_index(mutex);

                const TaskId waiter_id =
                    mutex.waiters[waiter_index];

                mutex.waiters.erase(
                    mutex.waiters.begin() +
                    static_cast<std::ptrdiff_t>(
                        waiter_index
                    )
                );

                mutex.owner = waiter_id;

                Slot& waiter =
                    slots_[index_of(waiter_id)];

                add_held_mutex(
                    waiter,
                    mutex_id
                );

                waiter.blocked_on = kNoMutex;
                ++waiter.pc;

                waiter.task.set_state(TaskState::Ready);

                ready_.push(
                    waiter_id,
                    waiter.task.base_priority()
                );

                continue;
            }
        }
    }
}

void Engine::step() {
    // B1
    resolve();

    // B2
    wake_sleepers();

    // B3
    resolve();

    // B5 / tick execution
    if (running_ == kNone) {
        timeline_.push_back(kIdle);
    } else {
        Slot& slot = slots_[running_];

        timeline_.push_back(slot.task.id());

        if (slot.remaining <= 0) {
            throw std::logic_error(
                "running task has no remaining compute ticks"
            );
        }

        --slot.remaining;

        if (slot.remaining == 0) {
            ++slot.pc;

            // Final COMPUTE completed at this instant.
            if (slot.pc >= slot.task.program().size()) {
                if (!slot.held_mutexes.empty()) {
                    throw std::logic_error(
                        "task completed while holding a mutex"
                    );
                }

                slot.task.set_state(
                    TaskState::Completed
                );

                running_ = kNone;
            }
        }
    }

    // B6
    ++now_;
}

void Engine::run(Tick ticks) {
    if (ticks < 0) {
        throw std::invalid_argument(
            "tick count cannot be negative"
        );
    }

    for (Tick i = 0; i < ticks; ++i) {
        step();
    }
}

bool Engine::run_until_done(Tick max_ticks) {
    if (max_ticks < 0) {
        throw std::invalid_argument(
            "max_ticks cannot be negative"
        );
    }

    while (!all_completed() &&
           now_ < max_ticks) {
        step();
    }

    return all_completed();
}

}  // namespace cadence