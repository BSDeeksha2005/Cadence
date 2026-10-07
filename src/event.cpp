#include "cadence/event.hpp"

#include <sstream>

namespace cadence {

const char* to_string(EventKind kind) {
    switch (kind) {
        case EventKind::Release:          return "RELEASE";
        case EventKind::Wake:             return "WAKE";
        case EventKind::Dispatch:         return "DISPATCH";
        case EventKind::Preempt:          return "PREEMPT";
        case EventKind::SleepStart:       return "SLEEP_START";
        case EventKind::LockAcquire:      return "LOCK_ACQUIRE";
        case EventKind::LockBlock:        return "LOCK_BLOCK";
        case EventKind::Unlock:           return "UNLOCK";
        case EventKind::Handoff:          return "HANDOFF";
        case EventKind::PriorityChange:  return "PRIORITY_CHANGE";
        case EventKind::Complete:         return "COMPLETE";
        case EventKind::DeadlineMiss:     return "DEADLINE_MISS";
        case EventKind::DeadlockDetected: return "DEADLOCK_DETECTED";
    }
    return "UNKNOWN";
}

const char* to_string(RunStatus status) {
    switch (status) {
        case RunStatus::Running:        return "RUNNING";
        case RunStatus::Completed:      return "COMPLETED";
        case RunStatus::HorizonReached: return "HORIZON_REACHED";
        case RunStatus::Deadlock:       return "DEADLOCK";
    }
    return "UNKNOWN";
}

std::string canonical_trace(const SimulationResult& result) {
    std::ostringstream out;

    out << "status=" << to_string(result.status)
        << ";protocol="
        << (result.protocol == Protocol::PIP ? "PIP" : "NONE")
        << ";end=" << result.end_time << '\n';

    for (const Event& event : result.events) {
        out << "E|" << event.seq
            << '|' << event.time
            << '|' << to_string(event.kind)
            << '|' << event.task
            << '|';

        if (event.mutex.has_value()) {
            out << event.mutex.value();
        } else {
            out << '-';
        }

        out << '|' << event.a << '|' << event.b << '|';

        for (std::size_t i = 0; i < event.cycle.size(); ++i) {
            if (i != 0) {
                out << ',';
            }
            out << event.cycle[i];
        }

        out << '\n';
    }

    for (const TickRow& row : result.ticks) {
        out << "T|" << row.time << '|' << row.running << '|';

        for (const TaskTickSnapshot& task : row.tasks) {
            out << task.id << ':'
                << static_cast<int>(task.state) << ':'
                << task.base << ':'
                << task.eff << ':'
                << task.blocked_on << ';';
        }

        out << '|';

        for (const MutexTickSnapshot& mutex : row.mutexes) {
            out << mutex.id << ':' << mutex.owner << ':';

            for (std::size_t i = 0; i < mutex.waiters.size(); ++i) {
                if (i != 0) {
                    out << ',';
                }
                out << mutex.waiters[i];
            }

            out << ';';
        }

        out << '\n';
    }

    return out.str();
}

std::uint64_t trace_hash(const SimulationResult& result) {
    const std::string text = canonical_trace(result);

    // FNV-1a 64-bit. This is intentionally local and deterministic.
    std::uint64_t hash = 14695981039346656037ULL;

    for (unsigned char c : text) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= 1099511628211ULL;
    }

    return hash;
}

}  // namespace cadence