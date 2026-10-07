#pragma once

#include <optional>
#include <vector>

#include "cadence/event.hpp"
#include "cadence/scenario.hpp"

namespace cadence {

struct TaskMetrics {
    TaskId id = kIdle;
    std::optional<Tick> response;
    std::optional<Tick> start_latency;
    Tick blocked_ticks = 0;
    Tick inversion_ticks = 0;
    Tick legitimate_blocking = 0;
    bool deadline_missed = false;
    std::optional<Tick> lateness;
    std::optional<Tick> first_dispatch;
    std::optional<Tick> completion;
};

struct RunMetrics {
    Protocol protocol = Protocol::NONE;
    RunStatus status = RunStatus::Running;
    Tick end_time = 0;
    Tick busy_ticks = 0;
    Tick total_ticks = 0;
    Tick preemptions = 0;
    Tick context_switches = 0;
    std::vector<TaskMetrics> tasks;
};

RunMetrics compute_metrics(
    const Scenario& scenario,
    const SimulationResult& result
);

}  // namespace cadence