#include "cadence/ready_queue.hpp"

#include <cstddef>
#include <stdexcept>

namespace cadence {

bool ReadyQueue::outranks(const Entry& a, const Entry& b) {
    if (a.priority != b.priority) {
        return a.priority > b.priority;  // higher number wins
    }
    return a.seq < b.seq;  // earlier arrival wins
}

std::size_t ReadyQueue::best_index() const {
    if (entries_.empty()) {
        throw std::logic_error("ReadyQueue is empty");
    }
    std::size_t best = 0;
    for (std::size_t i = 1; i < entries_.size(); ++i) {
        if (outranks(entries_[i], entries_[best])) {
            best = i;
        }
    }
    return best;
}

void ReadyQueue::push(TaskId id, Priority priority) {
    if (contains(id)) {
        throw std::logic_error("task already in ReadyQueue");
    }
    entries_.push_back(Entry{id, priority, next_seq_++});
}

TaskId ReadyQueue::peek() const {
    return entries_[best_index()].id;
}

TaskId ReadyQueue::pop() {
    const std::size_t idx = best_index();
    const TaskId id = entries_[idx].id;
    entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(idx));
    return id;
}

bool ReadyQueue::remove(TaskId id) {
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].id == id) {
            entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(i));
            return true;
        }
    }
    return false;
}

bool ReadyQueue::contains(TaskId id) const {
    for (const Entry& e : entries_) {
        if (e.id == id) return true;
    }
    return false;
}

Priority ReadyQueue::peek_priority() const {
    return entries_[best_index()].priority;
}

}  // namespace cadence