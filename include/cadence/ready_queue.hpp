#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "cadence/types.hpp"


namespace cadence {

// Holds ready tasks and picks the next one deterministically:
// highest priority first, FIFO among equal priorities.
class ReadyQueue {
public:
    void push(TaskId id, Priority priority);  // throws if id already queued
    TaskId peek() const;       
    Priority peek_priority() const;           // priority of the best entry; throws if empty               // throws if empty
    TaskId pop();                             // throws if empty
    bool remove(TaskId id);                   // true if it was queued
    bool contains(TaskId id) const;

    bool empty() const { return entries_.empty(); }
    std::size_t size() const { return entries_.size(); }

private:
    struct Entry {
        TaskId id;
        Priority priority;
        std::uint64_t seq;  // insertion order, breaks ties
    };

    // The ONLY place that defines scheduling order.
    static bool outranks(const Entry& a, const Entry& b);

    std::size_t best_index() const;

    std::vector<Entry> entries_;
    std::uint64_t next_seq_ = 0;
};

}  // namespace cadence