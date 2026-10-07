#include "cadence/metrics.hpp"
#include "cadence/priority.hpp"

#include <algorithm>
#include <limits>

namespace cadence {
namespace {

const Task* find_scenario_task(
    const Scenario& scenario,
    TaskId id
) {
    for (const Task& task : scenario.tasks) {
        if (task.id() == id) {
            return &task;
        }
    }
    return nullptr;
}

const TaskTickSnapshot* find_task(
    const TickRow& row,
    TaskId id
) {
    for (const TaskTickSnapshot& task : row.tasks) {
        if (task.id == id) {
            return &task;
        }
    }
    return nullptr;
}

const MutexTickSnapshot* find_mutex(
    const TickRow& row,
    MutexId id
) {
    for (const MutexTickSnapshot& mutex : row.mutexes) {
        if (mutex.id == id) {
            return &mutex;
        }
    }
    return nullptr;
}

std::vector<TaskId> blocking_chain(
    const TickRow& row,
    TaskId blocked
) {
    std::vector<TaskId> chain;
    TaskId current = blocked;

    while (true) {
        const TaskTickSnapshot* task_snapshot = find_task(row, current);
        if (task_snapshot == nullptr ||
            task_snapshot->state != TaskState::Blocked ||
            task_snapshot->blocked_on < 0) {
            break;
        }

        const MutexTickSnapshot* mutex =
            find_mutex(row, task_snapshot->blocked_on);
        if (mutex == nullptr || mutex->owner == kIdle) {
            break;
        }

        current = mutex->owner;
        if (std::find(chain.begin(), chain.end(), current) != chain.end()) {
            break;
        }
        chain.push_back(current);
    }

    return chain;
}

TaskId last_non_idle_before(
    const SimulationResult& result,
    Tick time,
    TaskId fallback
) {
    TaskId last = fallback;
    for (const TickRow& row : result.ticks) {
        if (row.time >= time) {
            break;
        }
        if (row.running != kIdle) {
            last = row.running;
        }
    }
    return last;
}

}  // namespace

RunMetrics compute_metrics(
    const Scenario& scenario,
    const SimulationResult& result
) {
    RunMetrics metrics;
    metrics.protocol = result.protocol;
    metrics.status = result.status;
    metrics.end_time = result.end_time;
    metrics.total_ticks = result.end_time;

    for (const Task& task : scenario.tasks) {
        metrics.tasks.push_back(TaskMetrics{});
        metrics.tasks.back().id = task.id();
    }

    for (const Event& event : result.events) {
        for (TaskMetrics& tm : metrics.tasks) {
            if (tm.id != event.task) {
                continue;
            }

            if (event.kind == EventKind::Dispatch &&
                !tm.first_dispatch.has_value()) {
                tm.first_dispatch = event.time;
                const Task* source = find_scenario_task(scenario, tm.id);
                if (source != nullptr) {
                    tm.start_latency = event.time - source->release();
                }
            }

            if (event.kind == EventKind::Complete) {
                tm.completion = event.time;
                const Task* source = find_scenario_task(scenario, tm.id);
                if (source != nullptr) {
                    tm.response = event.time - source->release();
                }
            }

            if (event.kind == EventKind::DeadlineMiss) {
                tm.deadline_missed = true;
            }
        }
    }

    for (const Task& task : scenario.tasks) {
        for (TaskMetrics& tm : metrics.tasks) {
            if (tm.id != task.id()) {
                continue;
            }

            if (tm.completion.has_value() &&
                task.absolute_deadline().has_value()) {
                const Tick late =
                    tm.completion.value() -
                    task.absolute_deadline().value();
                tm.lateness = std::max<Tick>(0, late);
            }
        }
    }

    for (const TickRow& row : result.ticks) {
        if (row.running != kIdle) {
            ++metrics.busy_ticks;
        }

        for (const TaskTickSnapshot& task_snapshot : row.tasks) {
            TaskMetrics* target = nullptr;
            for (TaskMetrics& tm : metrics.tasks) {
                if (tm.id == task_snapshot.id) {
                    target = &tm;
                    break;
                }
            }

            if (target == nullptr) {
                continue;
            }

            if (task_snapshot.state == TaskState::Blocked) {
                ++target->blocked_ticks;

                if (row.running != kIdle) {
                    const TaskTickSnapshot* running =
                        find_task(row, row.running);
                    if (running != nullptr &&
                        higherThan(task_snapshot.eff, running->eff)) {
                        const std::vector<TaskId> chain =
                            blocking_chain(row, task_snapshot.id);

                        if (std::find(
                                chain.begin(),
                                chain.end(),
                                row.running
                            ) == chain.end()) {
                            ++target->inversion_ticks;
                        }
                    }
                }
            }
        }
    }

    for (TaskMetrics& tm : metrics.tasks) {
        tm.legitimate_blocking =
            tm.blocked_ticks - tm.inversion_ticks;
    }

    for (const Event& event : result.events) {
        if (event.kind != EventKind::Preempt) {
            continue;
        }
        ++metrics.preemptions;
    }

    bool saw_first_dispatch = false;
    for (const Event& event : result.events) {
        if (event.kind != EventKind::Dispatch) {
            continue;
        }

        if (!saw_first_dispatch) {
            saw_first_dispatch = true;
            continue;
        }

        const TaskId previous =
            last_non_idle_before(result, event.time, kIdle);

        if (previous != kIdle && previous != event.task) {
            ++metrics.context_switches;
        }
    }

    return metrics;
}

}  // namespace cadence