#include "cadence/engine.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace cadence {

Engine::Engine(
    Tick horizon,
    Protocol protocol
)
    : horizon_(horizon),
      protocol_(protocol)
{
    if (horizon < 0) {
        throw std::invalid_argument(
            "horizon cannot be negative"
        );
    }
}

Engine::Engine(Protocol protocol)
    : horizon_(kDefaultHorizon),
      protocol_(protocol)
{
}

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

Engine::MutexSlot& Engine::get_or_create_mutex(
    MutexId id
) {
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

    mutexes_.push_back(
        MutexSlot{id, kIdle, {}}
    );

    return mutexes_.back();
}

const Engine::MutexSlot* Engine::find_mutex(
    MutexId id
) const {
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

std::vector<TaskId> Engine::mutex_waiters(
    MutexId id
) const {
    const MutexSlot* mutex = find_mutex(id);

    if (mutex == nullptr) {
        return {};
    }

    return mutex->waiters;
}

bool Engine::deadline_missed(TaskId id) const {
    return slots_[index_of(id)].deadline_missed;
}

std::optional<Tick> Engine::completion_time(
    TaskId id
) const {
    return slots_[index_of(id)].completion_time;
}

void Engine::add_task(Task task) {
    if (now_ != 0 || !timeline_.empty()) {
        throw std::logic_error(
            "tasks must be added before the first tick"
        );
    }

    if (finished_) {
        throw std::logic_error(
            "cannot add task after simulation has finished"
        );
    }

    for (const Slot& slot : slots_) {
        if (slot.task.id() == task.id()) {
            throw std::invalid_argument(
                "duplicate task id"
            );
        }
    }

    slots_.push_back(
        Slot{
            std::move(task),
            0,
            0,
            0,
            kNoMutex,
            {},
            std::nullopt,
            false
        }
    );

    // Every task starts NEW.
    // B2 performs the actual release.
    slots_.back().task.set_state(
        TaskState::New
    );
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

std::vector<std::size_t>
Engine::ordered_slot_indices_by_task_id() const {
    std::vector<std::size_t> order(
        slots_.size()
    );

    std::iota(
        order.begin(),
        order.end(),
        static_cast<std::size_t>(0)
    );

    std::sort(
        order.begin(),
        order.end(),
        [this](std::size_t lhs, std::size_t rhs) {
            return slots_[lhs].task.id() <
                   slots_[rhs].task.id();
        }
    );

    return order;
}

Priority Engine::calculate_effective_priority(
    TaskId id
) const {
    const Slot& slot =
        slots_[index_of(id)];

    Priority effective =
        slot.task.base_priority();

    if (protocol_ == Protocol::NONE) {
        return effective;
    }

    for (MutexId mutex_id : slot.held_mutexes) {
        const MutexSlot* mutex =
            find_mutex(mutex_id);

        if (mutex == nullptr) {
            throw std::logic_error(
                "held mutex does not exist"
            );
        }

        for (TaskId waiter_id : mutex->waiters) {
            const Priority waiter_eff =
                task(waiter_id).effective_priority();

            if (waiter_eff > effective) {
                effective = waiter_eff;
            }
        }
    }

    return effective;
}

void Engine::recompute_priority(TaskId id) {
    Slot& slot = slots_[index_of(id)];

    const Priority old_priority =
        slot.task.effective_priority();

    const Priority new_priority =
        calculate_effective_priority(id);

    if (old_priority == new_priority) {
        return;
    }

    slot.task.set_effective_priority(
        new_priority
    );

    if (slot.task.state() == TaskState::Ready) {
        if (!ready_.remove(id)) {
            throw std::logic_error(
                "READY task missing from ReadyQueue"
            );
        }

        ready_.push(
            id,
            new_priority
        );
    }
}

void Engine::propagate_priority(TaskId id) {
    std::vector<TaskId> seen;

    TaskId current = id;

    while (current != kIdle) {
        if (std::find(
                seen.begin(),
                seen.end(),
                current
            ) != seen.end())
        {
            throw std::logic_error(
                "priority inheritance cycle"
            );
        }

        seen.push_back(current);

        Slot& slot =
            slots_[index_of(current)];

        const Priority old_priority =
            slot.task.effective_priority();

        const Priority new_priority =
            calculate_effective_priority(
                current
            );

        if (old_priority == new_priority) {
            break;
        }

        slot.task.set_effective_priority(
            new_priority
        );

        if (slot.task.state() ==
            TaskState::Ready)
        {
            if (!ready_.remove(current)) {
                throw std::logic_error(
                    "READY task missing from ReadyQueue"
                );
            }

            ready_.push(
                current,
                new_priority
            );
        }

        if (slot.task.state() ==
                TaskState::Blocked &&
            slot.blocked_on != kNoMutex)
        {
            const MutexSlot* blocked_mutex =
                find_mutex(slot.blocked_on);

            if (blocked_mutex == nullptr) {
                throw std::logic_error(
                    "blocked mutex does not exist"
                );
            }

            current = blocked_mutex->owner;
            continue;
        }

        break;
    }
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
         ++i)
    {
        const Priority candidate =
            task(mutex.waiters[i])
                .effective_priority();

        const Priority current_best =
            task(mutex.waiters[best])
                .effective_priority();

        if (candidate > current_best) {
            best = i;
        }

        // Equal priority keeps earliest arrival.
    }

    return best;
}

void Engine::activate_at_now() {
    const std::vector<std::size_t> order =
        ordered_slot_indices_by_task_id();

    for (std::size_t index : order) {
        Slot& slot = slots_[index];

        if (slot.task.state() ==
                TaskState::New &&
            slot.task.release() == now_)
        {
            slot.task.set_state(
                TaskState::Ready
            );

            ready_.push(
                slot.task.id(),
                slot.task.effective_priority()
            );

            continue;
        }

        if (slot.task.state() ==
                TaskState::Sleeping &&
            slot.wake_at == now_)
        {
            slot.task.set_state(
                TaskState::Ready
            );

            ready_.push(
                slot.task.id(),
                slot.task.effective_priority()
            );
        }
    }
}

void Engine::check_deadlines() {
    const std::vector<std::size_t> order =
        ordered_slot_indices_by_task_id();

    for (std::size_t index : order) {
        Slot& slot = slots_[index];

        const std::optional<Tick> deadline =
            slot.task.absolute_deadline();

        if (!deadline.has_value()) {
            continue;
        }

        if (slot.deadline_missed) {
            continue;
        }

        if (deadline.value() == now_ &&
            slot.task.state() !=
                TaskState::Completed)
        {
            slot.deadline_missed = true;
        }
    }
}

void Engine::preempt_if_needed() {
    if (running_ == kNone ||
        ready_.empty())
    {
        return;
    }

    Slot& current =
        slots_[running_];

    if (ready_.peek_priority() >
        current.task.effective_priority())
    {
        current.task.set_state(
            TaskState::Ready
        );

        ready_.push_front(
            current.task.id(),
            current.task.effective_priority()
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

            const TaskId next =
                ready_.pop();

            running_ = index_of(next);

            slots_[running_].task.set_state(
                TaskState::Running
            );
        }
        else {
            preempt_if_needed();

            if (running_ == kNone) {
                continue;
            }
        }

        Slot& slot =
            slots_[running_];

        // Implicit END.
        if (slot.pc >=
            slot.task.program().size())
        {
            if (!slot.held_mutexes.empty()) {
                throw std::logic_error(
                    "task completed while holding a mutex"
                );
            }

            slot.task.set_state(
                TaskState::Completed
            );

            slot.completion_time = now_;

            running_ = kNone;

            continue;
        }

        const Operation& op =
            slot.task.program()[slot.pc];

        switch (op.type()) {
            case OpType::Compute: {
                if (slot.remaining == 0) {
                    slot.remaining =
                        op.ticks();
                }

                return;
            }

            case OpType::Sleep: {
                slot.task.set_state(
                    TaskState::Sleeping
                );

                slot.wake_at =
                    now_ + op.ticks();

                ++slot.pc;

                running_ = kNone;

                continue;
            }

            case OpType::Lock: {
                const MutexId mutex_id =
                    op.mutex();

                MutexSlot& mutex =
                    get_or_create_mutex(
                        mutex_id
                    );

                const TaskId task_id =
                    slot.task.id();

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

                slot.task.set_state(
                    TaskState::Blocked
                );

                slot.blocked_on =
                    mutex_id;

                mutex.waiters.push_back(
                    task_id
                );

                running_ = kNone;

                if (protocol_ ==
                    Protocol::PIP)
                {
                    propagate_priority(
                        mutex.owner
                    );
                }

                continue;
            }

            case OpType::Unlock: {
                const MutexId mutex_id =
                    op.mutex();

                MutexSlot& mutex =
                    get_or_create_mutex(
                        mutex_id
                    );

                const TaskId owner_id =
                    slot.task.id();

                if (mutex.owner != owner_id) {
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

                    if (protocol_ ==
                        Protocol::PIP)
                    {
                        recompute_priority(
                            owner_id
                        );
                    }

                    continue;
                }

                const std::size_t waiter_index =
                    choose_waiter_index(
                        mutex
                    );

                const TaskId waiter_id =
                    mutex.waiters[waiter_index];

                mutex.waiters.erase(
                    mutex.waiters.begin() +
                    static_cast<
                        std::ptrdiff_t
                    >(waiter_index)
                );

                mutex.owner = waiter_id;

                Slot& waiter =
                    slots_[index_of(
                        waiter_id
                    )];

                add_held_mutex(
                    waiter,
                    mutex_id
                );

                waiter.blocked_on =
                    kNoMutex;

                ++waiter.pc;

                if (protocol_ ==
                    Protocol::PIP)
                {
                    recompute_priority(
                        owner_id
                    );

                    recompute_priority(
                        waiter_id
                    );
                }

                waiter.task.set_state(
                    TaskState::Ready
                );

                ready_.push(
                    waiter_id,
                    waiter.task.effective_priority()
                );

                continue;
            }
        }
    }
}

void Engine::step() {
    if (finished_) {
        return;
    }

    // B1
    resolve();

    // B2
    activate_at_now();

    // B3
    resolve();

    // B4
    check_deadlines();

    // End instant: no tick executes here.
    if (all_completed() ||
        now_ >= horizon_)
    {
        finished_ = true;
        return;
    }

    // B5/B6: execute one tick.
    if (running_ == kNone) {
        timeline_.push_back(kIdle);
    }
    else {
        Slot& slot =
            slots_[running_];

        timeline_.push_back(
            slot.task.id()
        );

        if (slot.remaining <= 0) {
            throw std::logic_error(
                "running task has no remaining compute ticks"
            );
        }

        --slot.remaining;

        if (slot.remaining == 0) {
            ++slot.pc;

            if (slot.pc >=
                slot.task.program().size())
            {
                if (!slot.held_mutexes.empty()) {
                    throw std::logic_error(
                        "task completed while holding a mutex"
                    );
                }

                slot.task.set_state(
                    TaskState::Completed
                );

                slot.completion_time =
                    now_ + 1;

                running_ = kNone;
            }
        }
    }

    ++now_;

    if (all_completed()) {
        finished_ = true;
    }
}

void Engine::run(Tick ticks) {
    if (ticks < 0) {
        throw std::invalid_argument(
            "tick count cannot be negative"
        );
    }

    for (Tick i = 0;
         i < ticks && !finished_;
         ++i)
    {
        step();
    }
}

bool Engine::run_until_done(
    Tick max_ticks
) {
    if (max_ticks < 0) {
        throw std::invalid_argument(
            "max_ticks cannot be negative"
        );
    }

    while (!finished_ &&
           now_ < max_ticks)
    {
        step();
    }

    // If the horizon itself was reached,
    // process its boundary so deadline misses
    // at exactly the horizon are observed.
    if (!finished_ &&
        now_ == horizon_ &&
        horizon_ <= max_ticks)
    {
        step();
    }

    return all_completed();
}

bool Engine::run_to_horizon() {
    while (!finished_) {
        step();
    }

    return all_completed();
}

}  // namespace cadence