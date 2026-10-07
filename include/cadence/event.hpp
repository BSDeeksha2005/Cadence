#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "cadence/task_state.hpp"
#include "cadence/types.hpp"

namespace cadence {

enum class EventKind {
    Release,
    Wake,
    Dispatch,
    Preempt,
    SleepStart,
    LockAcquire,
    LockBlock,
    Unlock,
    Handoff,
    PriorityChange,
    Complete,
    DeadlineMiss,
    DeadlockDetected
};

const char* to_string(EventKind kind);

struct Event {
    std::uint64_t seq = 0;
    Tick time = 0;
    EventKind kind = EventKind::Dispatch;
    TaskId task = kIdle;
    std::optional<MutexId> mutex;
    std::int64_t a = 0;
    std::int64_t b = 0;
    std::vector<TaskId> cycle;
};

struct TaskTickSnapshot {
    TaskId id = kIdle;
    TaskState state = TaskState::New;
    Priority base = 0;
    Priority eff = 0;
    MutexId blocked_on = -1;
};

struct MutexTickSnapshot {
    MutexId id = -1;
    TaskId owner = kIdle;
    std::vector<TaskId> waiters;
};

struct TickRow {
    Tick time = 0;
    TaskId running = kIdle;
    std::vector<TaskTickSnapshot> tasks;
    std::vector<MutexTickSnapshot> mutexes;
};

enum class Protocol {
    NONE,
    PIP
};

enum class RunStatus {
    Running,
    Completed,
    HorizonReached,
    Deadlock
};

const char* to_string(RunStatus status);

struct SimulationResult {
    RunStatus status = RunStatus::Running;
    Protocol protocol = Protocol::NONE;
    Tick end_time = 0;
    std::vector<Event> events;
    std::vector<TickRow> ticks;
};

std::string canonical_trace(const SimulationResult& result);
std::uint64_t trace_hash(const SimulationResult& result);

}  // namespace cadence