#include "cadence/engine.hpp"
#include "cadence/priority.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace cadence {

Engine::Engine(Tick horizon, Protocol protocol)
    : horizon_(horizon), protocol_(protocol) {
    if (horizon < 0) {
        throw std::invalid_argument("horizon cannot be negative");
    }
}

Engine::Engine(Protocol protocol)
    : protocol_(protocol) {}

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
        throw std::invalid_argument("mutex id must be non-negative");
    }

    mutexes_.push_back(MutexSlot{id, kIdle, {}});
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
    return mutex == nullptr ? kIdle : mutex->owner;
}

std::vector<TaskId> Engine::mutex_waiters(MutexId id) const {
    const MutexSlot* mutex = find_mutex(id);
    return mutex == nullptr ? std::vector<TaskId>{} : mutex->waiters;
}

bool Engine::deadline_missed(TaskId id) const {
    return slots_[index_of(id)].deadline_missed;
}

std::optional<Tick> Engine::completion_time(TaskId id) const {
    return slots_[index_of(id)].completion_time;
}

void Engine::add_task(Task task_value) {
    if (now_ != 0 || !timeline_.empty()) {
        throw std::logic_error("tasks must be added before the first tick");
    }

    if (finished_) {
        throw std::logic_error("cannot add task after simulation has finished");
    }

    for (const Slot& slot : slots_) {
        if (slot.task.id() == task_value.id()) {
            throw std::invalid_argument("duplicate task id");
        }
    }

    slots_.push_back(Slot{
        std::move(task_value),
        0,
        0,
        0,
        kNoMutex,
        {},
        std::nullopt,
        false,
        0
    });
    slots_.back().task.set_state(TaskState::New);
}

bool Engine::all_completed() const {
    for (const Slot& slot : slots_) {
        if (slot.task.state() != TaskState::Completed) {
            return false;
        }
    }
    return true;
}

bool Engine::holds_mutex(const Slot& slot, MutexId mutex) const {
    return std::find(
        slot.held_mutexes.begin(),
        slot.held_mutexes.end(),
        mutex
    ) != slot.held_mutexes.end();
}

void Engine::add_held_mutex(Slot& slot, MutexId mutex) {
    if (holds_mutex(slot, mutex)) {
        throw std::logic_error("task already holds mutex");
    }
    slot.held_mutexes.push_back(mutex);
}

void Engine::remove_held_mutex(Slot& slot, MutexId mutex) {
    const auto it = std::find(
        slot.held_mutexes.begin(),
        slot.held_mutexes.end(),
        mutex
    );
    if (it == slot.held_mutexes.end()) {
        throw std::logic_error("task does not hold mutex");
    }
    slot.held_mutexes.erase(it);
}

std::vector<std::size_t> Engine::ordered_slot_indices_by_task_id() const {
    std::vector<std::size_t> order(slots_.size());
    std::iota(order.begin(), order.end(), static_cast<std::size_t>(0));
    std::sort(
        order.begin(),
        order.end(),
        [this](std::size_t lhs, std::size_t rhs) {
            return slots_[lhs].task.id() < slots_[rhs].task.id();
        }
    );
    return order;
}

Priority Engine::calculate_effective_priority(TaskId id) const {
    const Slot& slot = slots_[index_of(id)];
    Priority effective = slot.task.base_priority();

    if (protocol_ == Protocol::NONE) {
        return effective;
    }

    for (MutexId mutex_id : slot.held_mutexes) {
        const MutexSlot* mutex = find_mutex(mutex_id);
        if (mutex == nullptr) {
            throw std::logic_error("held mutex does not exist");
        }

        for (TaskId waiter_id : mutex->waiters) {
            const Priority waiter_eff = task(waiter_id).effective_priority();
            if (higherThan(waiter_eff, effective)) {
                effective = waiter_eff;
            }
        }
    }

    return effective;
}

void Engine::recompute_priority(TaskId id) {
    Slot& slot = slots_[index_of(id)];
    const Priority old_priority = slot.task.effective_priority();
    const Priority new_priority = calculate_effective_priority(id);

    if (old_priority == new_priority) {
        return;
    }

    slot.task.set_effective_priority(new_priority);
    emit(
        EventKind::PriorityChange,
        id,
        std::nullopt,
        old_priority,
        new_priority
    );

    if (slot.task.state() == TaskState::Ready) {
        if (!ready_.remove(id)) {
            throw std::logic_error("READY task missing from ReadyQueue");
        }
        ready_.push(id, new_priority);
    }
}

void Engine::propagate_priority(TaskId id) {
    std::vector<TaskId> seen;
    TaskId current = id;

    while (current != kIdle) {
        if (std::find(seen.begin(), seen.end(), current) != seen.end()) {
            throw std::logic_error("priority inheritance cycle");
        }
        seen.push_back(current);

        Slot& slot = slots_[index_of(current)];
        const Priority old_priority = slot.task.effective_priority();
        const Priority new_priority = calculate_effective_priority(current);

        if (old_priority == new_priority) {
            break;
        }

        slot.task.set_effective_priority(new_priority);
        emit(
            EventKind::PriorityChange,
            current,
            std::nullopt,
            old_priority,
            new_priority
        );

        if (slot.task.state() == TaskState::Ready) {
            if (!ready_.remove(current)) {
                throw std::logic_error("READY task missing from ReadyQueue");
            }
            ready_.push(current, new_priority);
        }

        if (slot.task.state() == TaskState::Blocked &&
            slot.blocked_on != kNoMutex) {
            const MutexSlot* blocked_mutex = find_mutex(slot.blocked_on);
            if (blocked_mutex == nullptr || blocked_mutex->owner == kIdle) {
                throw std::logic_error("blocked mutex has no owner");
            }
            current = blocked_mutex->owner;
            continue;
        }

        break;
    }
}

std::size_t Engine::choose_waiter_index(const MutexSlot& mutex) const {
    if (mutex.waiters.empty()) {
        throw std::logic_error("cannot choose waiter from empty list");
    }

    std::size_t best = 0;
    for (std::size_t i = 1; i < mutex.waiters.size(); ++i) {
        const Priority candidate = task(mutex.waiters[i]).effective_priority();
        const Priority current_best = task(mutex.waiters[best]).effective_priority();
        if (higherThan(candidate, current_best)) {
            best = i;
        }
    }
    return best;
}

std::vector<TaskId> Engine::deadlock_cycle(
    TaskId requester,
    MutexId mutex_id
) const {
    std::vector<TaskId> cycle{requester};
    const MutexSlot* mutex = find_mutex(mutex_id);

    if (mutex == nullptr || mutex->owner == kIdle) {
        return cycle;
    }

    TaskId current = mutex->owner;
    while (current != requester) {
        cycle.push_back(current);
        const Slot& slot = slots_[index_of(current)];

        if (slot.task.state() != TaskState::Blocked ||
            slot.blocked_on == kNoMutex) {
            break;
        }

        const MutexSlot* next = find_mutex(slot.blocked_on);
        if (next == nullptr || next->owner == kIdle) {
            break;
        }
        current = next->owner;
    }

    cycle.push_back(requester);
    return cycle;
}

bool Engine::would_create_deadlock(TaskId requester, MutexId mutex_id) const {
    const MutexSlot* requested_mutex = find_mutex(mutex_id);
    if (requested_mutex == nullptr || requested_mutex->owner == kIdle) {
        return false;
    }

    TaskId current = requested_mutex->owner;
    std::vector<TaskId> seen;

    while (current != kIdle) {
        if (current == requester) {
            return true;
        }

        if (std::find(seen.begin(), seen.end(), current) != seen.end()) {
            return true;
        }
        seen.push_back(current);

        const Slot& owner_slot = slots_[index_of(current)];
        if (owner_slot.task.state() != TaskState::Blocked ||
            owner_slot.blocked_on == kNoMutex) {
            return false;
        }

        const MutexSlot* next_mutex = find_mutex(owner_slot.blocked_on);
        if (next_mutex == nullptr || next_mutex->owner == kIdle) {
            return false;
        }

        current = next_mutex->owner;
    }

    return false;
}

void Engine::mark_deadlock(TaskId requester, MutexId mutex_id) {
    emit(
        EventKind::DeadlockDetected,
        requester,
        mutex_id,
        mutex_id,
        requester,
        deadlock_cycle(requester, mutex_id)
    );
    running_ = kNone;
    finished_ = true;
    status_ = RunStatus::Deadlock;
}

void Engine::emit(
    EventKind kind,
    TaskId task_id,
    std::optional<MutexId> mutex,
    std::int64_t a,
    std::int64_t b,
    std::vector<TaskId> cycle
) {
    events_.push_back(Event{
        next_event_seq_++,
        now_,
        kind,
        task_id,
        mutex,
        a,
        b,
        std::move(cycle)
    });
}

void Engine::dispatch(std::size_t slot_index) {
    Slot& slot = slots_[slot_index];
    running_ = slot_index;
    slot.task.set_state(TaskState::Running);
    emit(EventKind::Dispatch, slot.task.id());
}

void Engine::complete_task(std::size_t slot_index) {
    Slot& slot = slots_[slot_index];

    if (!slot.held_mutexes.empty()) {
        throw std::logic_error("task completed while holding a mutex");
    }

    slot.task.set_state(TaskState::Completed);
    slot.completion_time = now_;
    running_ = kNone;
    emit(EventKind::Complete, slot.task.id());
}

void Engine::activate_at_now() {
    const std::vector<std::size_t> order = ordered_slot_indices_by_task_id();

    for (std::size_t index : order) {
        Slot& slot = slots_[index];

        if (slot.task.state() == TaskState::New &&
            slot.task.release() == now_) {
            slot.task.set_state(TaskState::Ready);
            ready_.push(slot.task.id(), slot.task.effective_priority());
            emit(EventKind::Release, slot.task.id());
        } else if (slot.task.state() == TaskState::Sleeping &&
                   slot.wake_at == now_) {
            slot.task.set_state(TaskState::Ready);
            ready_.push(slot.task.id(), slot.task.effective_priority());
            emit(EventKind::Wake, slot.task.id());
        }
    }
}

void Engine::check_deadlines() {
    const std::vector<std::size_t> order = ordered_slot_indices_by_task_id();

    for (std::size_t index : order) {
        Slot& slot = slots_[index];
        const std::optional<Tick> deadline = slot.task.absolute_deadline();

        if (!deadline.has_value() || slot.deadline_missed ||
            slot.task.state() == TaskState::Completed) {
            continue;
        }

        if (deadline.value() == now_) {
            slot.deadline_missed = true;
            emit(
                EventKind::DeadlineMiss,
                slot.task.id(),
                std::nullopt,
                deadline.value(),
                0
            );
        }
    }
}

void Engine::preempt_if_needed() {
    if (running_ == kNone || ready_.empty()) {
        return;
    }

    Slot& current = slots_[running_];

    if (higherThan(ready_.peek_priority(), current.task.effective_priority())) {
        const TaskId id = current.task.id();
        const Priority eff = current.task.effective_priority();
        current.task.set_state(TaskState::Ready);
        ready_.push_front(id, eff);
        running_ = kNone;
        emit(EventKind::Preempt, id, std::nullopt, eff, ready_.peek_priority());
    }
}

void Engine::resolve() {
    std::size_t iterations = 0;

    while (true) {
        if (++iterations > kMaxResolveIterations) {
            throw std::logic_error("resolve iteration bound exceeded");
        }

        if (running_ == kNone) {
            if (ready_.empty()) {
                return;
            }
            dispatch(index_of(ready_.pop()));
        } else {
            preempt_if_needed();
            if (running_ == kNone) {
                continue;
            }
        }

        Slot& slot = slots_[running_];

        if (slot.pc >= slot.task.program().size()) {
            complete_task(running_);
            continue;
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
                slot.wake_at = now_ + op.ticks();
                ++slot.pc;
                running_ = kNone;
                emit(EventKind::SleepStart, slot.task.id(), std::nullopt,
                     slot.wake_at, op.ticks());
                continue;

            case OpType::Lock: {
                const MutexId mutex_id = op.mutex();
                MutexSlot& mutex = get_or_create_mutex(mutex_id);
                const TaskId id = slot.task.id();

                if (mutex.owner == id) {
                    throw std::logic_error("recursive mutex lock");
                }

                if (mutex.owner == kIdle) {
                    mutex.owner = id;
                    add_held_mutex(slot, mutex_id);
                    ++slot.pc;
                    emit(EventKind::LockAcquire, id, mutex_id);
                    continue;
                }

                const bool deadlock =
                    would_create_deadlock(id, mutex_id);

                emit(
                    EventKind::LockBlock,
                    id,
                    mutex_id,
                    mutex.owner,
                    id
                );

                slot.task.set_state(TaskState::Blocked);
                slot.blocked_on = mutex_id;
                mutex.waiters.push_back(id);
                running_ = kNone;

                if (deadlock) {
                    mark_deadlock(id, mutex_id);
                    return;
                }

                if (protocol_ == Protocol::PIP) {
                    propagate_priority(mutex.owner);
                }
                continue;
            }

            case OpType::Unlock: {
                const MutexId mutex_id = op.mutex();
                MutexSlot& mutex = get_or_create_mutex(mutex_id);
                const TaskId owner_id = slot.task.id();

                if (mutex.owner != owner_id) {
                    throw std::logic_error("task does not own mutex");
                }

                ++slot.pc;
                emit(EventKind::Unlock, owner_id, mutex_id);

                remove_held_mutex(slot, mutex_id);

                TaskId waiter_id = kIdle;
                if (mutex.waiters.empty()) {
                    mutex.owner = kIdle;
                } else {
                    const std::size_t waiter_index =
                        choose_waiter_index(mutex);
                    waiter_id = mutex.waiters[waiter_index];
                    mutex.waiters.erase(
                        mutex.waiters.begin() +
                        static_cast<std::ptrdiff_t>(waiter_index)
                    );

                    mutex.owner = waiter_id;

                    Slot& waiter = slots_[index_of(waiter_id)];
                    add_held_mutex(waiter, mutex_id);
                    waiter.blocked_on = kNoMutex;
                    ++waiter.pc;
                    waiter.task.set_state(TaskState::Ready);

                    ready_.push(waiter_id, waiter.task.effective_priority());

                    emit(
                        EventKind::Handoff,
                        waiter_id,
                        mutex_id,
                        owner_id,
                        waiter_id
                    );
                }

                if (protocol_ == Protocol::PIP) {
                    recompute_priority(owner_id);
                    if (waiter_id != kIdle) {
                        recompute_priority(waiter_id);
                    }
                }

                continue;
            }
        }
    }
}

void Engine::record_tick_row() {
    TickRow row;
    row.time = now_;
    row.running =
        running_ == kNone ? kIdle : slots_[running_].task.id();

    for (std::size_t index : ordered_slot_indices_by_task_id()) {
        const Slot& slot = slots_[index];
        row.tasks.push_back(TaskTickSnapshot{
            slot.task.id(),
            slot.task.state(),
            slot.task.base_priority(),
            slot.task.effective_priority(),
            slot.blocked_on
        });
    }

    for (const MutexSlot& mutex : mutexes_) {
        row.mutexes.push_back(
            MutexTickSnapshot{mutex.id, mutex.owner, mutex.waiters}
        );
    }

    tick_rows_.push_back(std::move(row));
    if (running_ == kNone) {
        timeline_.push_back(kIdle);
    } else {
        timeline_.push_back(slots_[running_].task.id());
    }
}

void Engine::execute_tick() {
    if (running_ != kNone) {
        Slot& slot = slots_[running_];

        if (slot.remaining <= 0) {
            throw std::logic_error(
                "running task has no remaining compute ticks"
            );
        }

        --slot.remaining;
        ++slot.executed_ticks;

        if (slot.remaining == 0) {
            ++slot.pc;
        }
    }

    ++now_;
}

void Engine::finish_status_if_boundary() {
    if (all_completed()) {
        finished_ = true;
        status_ = RunStatus::Completed;
        return;
    }

    if (now_ >= horizon_) {
        finished_ = true;
        status_ = RunStatus::HorizonReached;
    }
}

void Engine::step() {
    if (finished_) {
        return;
    }

    // B1
    resolve();

    if (finished_) {
        return;
    }

    // B2
    activate_at_now();

    // B3
    resolve();

    if (finished_) {
        return;
    }

    // B4
    check_deadlines();

    // B5: horizon/completion is an end instant; it has no tick row.
    if (all_completed() || now_ >= horizon_) {
        check_invariants();
        finish_status_if_boundary();
        return;
    }

    check_invariants();
    record_tick_row();

    // B6
    execute_tick();
    check_invariants();

    // A completed compute reaches END on the next boundary (B1),
    // preserving continuation-first ordering.
}

void Engine::run(Tick ticks) {
    if (ticks < 0) {
        throw std::invalid_argument("tick count cannot be negative");
    }

    for (Tick i = 0; i < ticks && !finished_; ++i) {
        step();
    }
}

bool Engine::run_until_done(Tick max_ticks) {
    if (max_ticks < 0) {
        throw std::invalid_argument("max_ticks cannot be negative");
    }

    while (!finished_ && now_ < max_ticks) {
        step();
    }

    if (!finished_ && now_ == horizon_ && horizon_ <= max_ticks) {
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

SimulationResult Engine::result() const {
    return SimulationResult{
        status_,
        protocol_,
        now_,
        events_,
        tick_rows_
    };
}

void Engine::check_invariants() const {
    // I1: exactly one state and running pointer consistency.
    std::size_t running_count = 0;
    for (const Slot& slot : slots_) {
        if (slot.task.state() == TaskState::Running) {
            ++running_count;
            if (running_ == kNone ||
                slots_[running_].task.id() != slot.task.id()) {
                throw std::logic_error("I1: running pointer mismatch");
            }
        }
    }

    if (running_count > 1) {
        throw std::logic_error("I1: multiple RUNNING tasks");
    }
    if ((running_ == kNone) != (running_count == 0)) {
        throw std::logic_error("I1: running/state disagreement");
    }

    // I2: READY iff exactly in the ready queue at its current level.
    for (const Slot& slot : slots_) {
        const bool queued = ready_.contains(slot.task.id());
        const bool ready = slot.task.state() == TaskState::Ready;
        if (queued != ready) {
            throw std::logic_error("I2: READY/queue mismatch");
        }
    }

    // I3/I4: after resolution there is no READY task above RUNNING,
    // and READY implies RUNNING.
    if (running_ != kNone) {
        const Priority running_eff =
            slots_[running_].task.effective_priority();
        for (const Slot& slot : slots_) {
            if (slot.task.state() == TaskState::Ready &&
                higherThan(slot.task.effective_priority(), running_eff)) {
                throw std::logic_error("I3: higher READY task exists");
            }
        }
    } else {
        for (const Slot& slot : slots_) {
            if (slot.task.state() == TaskState::Ready) {
                throw std::logic_error("I4: READY task with idle CPU");
            }
        }
    }

    // I5: blocked state matches exactly one waiter entry.
    for (const Slot& slot : slots_) {
        if (slot.task.state() != TaskState::Blocked) {
            continue;
        }
        if (slot.blocked_on == kNoMutex) {
            throw std::logic_error("I5: blocked task has no mutex");
        }

        const MutexSlot* mutex = find_mutex(slot.blocked_on);
        if (mutex == nullptr ||
            std::count(mutex->waiters.begin(),
                       mutex->waiters.end(),
                       slot.task.id()) != 1) {
            throw std::logic_error("I5: blocked/waiter mismatch");
        }
    }

    for (const MutexSlot& mutex : mutexes_) {
        for (TaskId waiter : mutex.waiters) {
            const Slot& slot = slots_[index_of(waiter)];
            if (slot.task.state() != TaskState::Blocked ||
                slot.blocked_on != mutex.id) {
                throw std::logic_error("I5: stray waiter");
            }
        }
    }

    // I6: ownership and held sets agree.
    for (const MutexSlot& mutex : mutexes_) {
        if (mutex.owner != kIdle) {
            const Slot& owner = slots_[index_of(mutex.owner)];
            if (!holds_mutex(owner, mutex.id)) {
                throw std::logic_error("I6: owner missing held mutex");
            }
            if (std::count(
                    mutex.waiters.begin(),
                    mutex.waiters.end(),
                    mutex.owner
                ) != 0) {
                throw std::logic_error("I6: owner also in waiter list");
            }
        }
    }

    for (const Slot& slot : slots_) {
        for (MutexId mutex_id : slot.held_mutexes) {
            const MutexSlot* mutex = find_mutex(mutex_id);
            if (mutex == nullptr || mutex->owner != slot.task.id()) {
                throw std::logic_error("I6: held mutex ownership mismatch");
            }
        }
    }

    // I7: effective priority oracle.
    for (const Slot& slot : slots_) {
        const Priority expected = calculate_effective_priority(slot.task.id());
        if (slot.task.effective_priority() != expected ||
            higherThan(slot.task.base_priority(), slot.task.effective_priority())) {
            throw std::logic_error("I7: effective priority mismatch");
        }
    }

    // I8: PIP owner dominates every blocked waiter.
    if (protocol_ == Protocol::PIP) {
        for (const MutexSlot& mutex : mutexes_) {
            if (mutex.owner == kIdle) {
                continue;
            }
            const Priority owner_eff =
                task(mutex.owner).effective_priority();
            for (TaskId waiter : mutex.waiters) {
                if (higherThan(task(waiter).effective_priority(), owner_eff)) {
                    throw std::logic_error("I8: owner below waiter priority");
                }
            }
        }
    }

    // I9: current COMPUTE conservation and total executed ticks.
    Tick total_executed = 0;
    for (const Slot& slot : slots_) {
        total_executed += slot.executed_ticks;

        if (slot.pc < slot.task.program().size() &&
            slot.task.program()[slot.pc].type() == OpType::Compute &&
            slot.remaining > 0) {
            const Tick n = slot.task.program()[slot.pc].ticks();
            if (slot.remaining > n) {
                throw std::logic_error("I9: remaining exceeds compute duration");
            }
        }
    }

    Tick busy_ticks = 0;
    for (const TickRow& row : tick_rows_) {
        if (row.running != kIdle) {
            ++busy_ticks;
        }
    }
    if (total_executed != busy_ticks) {
        throw std::logic_error("I9/I10: executed ticks mismatch");
    }

    // I10: trace sequence and tick-row count.
    if (timeline_.size() != tick_rows_.size()) {
        throw std::logic_error("I10: timeline/tick-row mismatch");
    }
    for (std::size_t i = 1; i < events_.size(); ++i) {
        if (events_[i-1].seq >= events_[i].seq ||
            events_[i-1].time > events_[i].time) {
            throw std::logic_error("I10: event ordering violation");
        }
    }
    for (std::size_t i = 1; i < tick_rows_.size(); ++i) {
        if (tick_rows_[i-1].time >= tick_rows_[i].time) {
            throw std::logic_error("I10: tick-row time violation");
        }
    }

    // I11: completed tasks are clean.
    for (const Slot& slot : slots_) {
        if (slot.task.state() != TaskState::Completed) {
            continue;
        }
        if (!slot.held_mutexes.empty() ||
            ready_.contains(slot.task.id())) {
            throw std::logic_error("I11: completed task is still owned/queued");
        }
        for (const MutexSlot& mutex : mutexes_) {
            if (std::count(
                    mutex.waiters.begin(),
                    mutex.waiters.end(),
                    slot.task.id()
                ) != 0) {
                throw std::logic_error("I11: completed task is a waiter");
            }
        }
    }

    // I12: NEW/SLEEPING time predicates.
    for (const Slot& slot : slots_) {
        if (slot.task.state() == TaskState::New &&
            !(slot.task.release() >= now_)) {
            throw std::logic_error("I12: NEW task release predicate failed");
        }
        if (slot.task.state() == TaskState::Sleeping &&
            !(slot.wake_at >= now_)) {
            throw std::logic_error("I12: SLEEPING wake predicate failed");
        }
    }
}

}  // namespace cadence