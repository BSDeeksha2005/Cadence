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
    const std::vector<TaskId>& timeline() const { return timeline_; }
    const Task& task(TaskId id) const;  // throws std::out_of_range

private:
    struct Slot {
        Task task;
        std::size_t pc;   // index of the current operation
        Tick remaining;   // ticks left in the current COMPUTE
        Tick wake_at;     // tick when a sleeping task becomes ready
    };

    static constexpr std::size_t kNone = static_cast<std::size_t>(-1);

    std::size_t index_of(TaskId id) const;
    static void settle(Slot& slot, Tick at);  // handle zero-time ops
    void wake_sleepers();
    void preempt_if_needed();
    void dispatch();

    std::vector<Slot> slots_;  // insertion order = deterministic iteration
    ReadyQueue ready_;
    std::size_t running_ = kNone;
    Tick now_ = 0;
    std::vector<TaskId> timeline_;
};

}  // namespace cadence