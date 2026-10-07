#pragma once

#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

#include "cadence/ready_queue.hpp"
#include "cadence/task.hpp"
#include "cadence/types.hpp"

namespace cadence {

enum class Protocol {
    NONE,
    PIP
};

class Engine {
public:
    explicit Engine(Tick horizon,
                    Protocol protocol = Protocol::NONE);

    explicit Engine(Protocol protocol = Protocol::NONE);

    void add_task(Task task);

    void step();
    void run(Tick ticks);
    bool run_until_done(Tick max_ticks);

    // Run until the simulation reaches its horizon
    // or all tasks complete.
    bool run_to_horizon();

    Tick now() const {
        return now_;
    }

    Tick horizon() const {
        return horizon_;
    }

    bool finished() const {
        return finished_;
    }

    bool all_completed() const;

    Protocol protocol() const {
        return protocol_;
    }

    const std::vector<TaskId>& timeline() const {
        return timeline_;
    }

    const Task& task(TaskId id) const;

    TaskId mutex_owner(MutexId id) const;

    std::vector<TaskId> mutex_waiters(
        MutexId id
    ) const;

    bool deadline_missed(TaskId id) const;

    std::optional<Tick> completion_time(TaskId id) const;

private:
    static constexpr std::size_t kNone =
        static_cast<std::size_t>(-1);

    static constexpr MutexId kNoMutex = -1;

    static constexpr Tick kDefaultHorizon =
        std::numeric_limits<Tick>::max();

    struct MutexSlot {
        MutexId id;
        TaskId owner;
        std::vector<TaskId> waiters;
    };

    struct Slot {
        Task task;
        std::size_t pc;
        Tick remaining;
        Tick wake_at;
        MutexId blocked_on;
        std::vector<MutexId> held_mutexes;

        std::optional<Tick> completion_time;
        bool deadline_missed;
    };

    std::size_t index_of(TaskId id) const;

    MutexSlot& get_or_create_mutex(MutexId id);

    const MutexSlot* find_mutex(
        MutexId id
    ) const;

    bool holds_mutex(
        const Slot& slot,
        MutexId mutex
    ) const;

    void add_held_mutex(
        Slot& slot,
        MutexId mutex
    );

    void remove_held_mutex(
        Slot& slot,
        MutexId mutex
    );

    std::size_t choose_waiter_index(
        const MutexSlot& mutex
    ) const;

    Priority calculate_effective_priority(
        TaskId id
    ) const;

    void recompute_priority(TaskId id);

    void propagate_priority(TaskId id);

    void resolve();

    void activate_at_now();

    void check_deadlines();

    void preempt_if_needed();

    std::vector<std::size_t>
    ordered_slot_indices_by_task_id() const;

    std::vector<Slot> slots_;
    std::vector<MutexSlot> mutexes_;

    ReadyQueue ready_;

    std::size_t running_ = kNone;
    Tick now_ = 0;
    Tick horizon_ = kDefaultHorizon;

    Protocol protocol_ = Protocol::NONE;

    bool finished_ = false;

    std::vector<TaskId> timeline_;
};

}  // namespace cadence