#pragma once

#include <optional>
#include <string>
#include <vector>

#include "cadence/event.hpp"
#include "cadence/task.hpp"

namespace cadence {

struct Scenario {
    std::string name;
    std::optional<Tick> horizon;
    std::vector<MutexId> mutexes;
    std::vector<Task> tasks;
};

Scenario make_s1();

SimulationResult simulate_scenario(
    const Scenario& scenario,
    Protocol protocol
);

}  // namespace cadence