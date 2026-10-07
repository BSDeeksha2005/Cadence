#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "cadence/types.hpp"

namespace cadence {

// Holds ready tasks and picks the next one deterministically:
// highest priority first, FIFO among normally-ready equal priorities.
// Preempted tasks can explicitly be placed at the head of their level.
class ReadyQueue {
public:
    void push(TaskId id, Priority priority);       // add to tail
    void push_front(TaskId id, Priority priority); // add to head

    TaskId peek() const;
    Priority peek_priority() const;
    TaskId pop();

    bool remove(TaskId id);
    bool contains(TaskId id) const;

    bool empty() const { return entries_.empty(); }
    std::size_t size() const { return entries_.size(); }

private:
    struct Entry {
        TaskId id;
        Priority priority;
        std::uint64_t seq;
    };

    // The ONLY place that defines scheduling order.
    static bool outranks(const Entry& a, const Entry& b);

    std::size_t best_index() const;

    std::vector<Entry> entries_;

    // Normal entries start at 1.
    // seq == 0 is reserved for a preempted task at the head.
    std::uint64_t next_seq_ = 1;
};

}  // namespace cadence