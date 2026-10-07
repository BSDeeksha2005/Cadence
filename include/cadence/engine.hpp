#pragma once

#include <cstddef>
#include <vector>

#include "cadence/ready_queue.hpp"
#include "cadence/task.hpp"
#include "cadence/types.hpp"

namespace cadence {

class Engine {
public:
    void add_task(Task task);  // only before the first tick; ids must be unique

    void step();                          // simulate exactly one tick
    void run(Tick ticks);                 // step() n times
    bool run_until_done(Tick max_ticks);  // true if everything completed

    Tick now() const { return now_; }
    bool all_completed() const;

    const std::vector<TaskId>& timeline() const {
        return timeline_;
    }

    const Task& task(TaskId id) const;  // throws std::out_of_range

    // Step 6 inspection helpers.
    // Return kIdle when the mutex does not currently exist or is unlocked.
    TaskId mutex_owner(MutexId id) const;

    // Returns waiter order exactly as stored by the mutex.
    std::vector<TaskId> mutex_waiters(MutexId id) const;

private:
    static constexpr std::size_t kNone =
        static_cast<std::size_t>(-1);

    static constexpr MutexId kNoMutex = -1;

    struct MutexSlot {
        MutexId id;
        TaskId owner;
        std::vector<TaskId> waiters;
    };

    struct Slot {
        Task task;
        std::size_t pc;   // current operation
        Tick remaining;   // ticks left in current COMPUTE
        Tick wake_at;     // wake instant for SLEEP
        MutexId blocked_on;
        std::vector<MutexId> held_mutexes;
    };

    std::size_t index_of(TaskId id) const;

    MutexSlot& get_or_create_mutex(MutexId id);
    const MutexSlot* find_mutex(MutexId id) const;

    void resolve();
    void wake_sleepers();
    void preempt_if_needed();

    void add_held_mutex(Slot& slot, MutexId mutex);
    void remove_held_mutex(Slot& slot, MutexId mutex);
    bool holds_mutex(const Slot& slot, MutexId mutex) const;

    std::size_t choose_waiter_index(
        const MutexSlot& mutex) const;

    std::vector<Slot> slots_;
    std::vector<MutexSlot> mutexes_;

    ReadyQueue ready_;

    std::size_t running_ = kNone;
    Tick now_ = 0;

    std::vector<TaskId> timeline_;
};

}  // namespace cadence