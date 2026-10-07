#include "cadence/scenario.hpp"

#include <stdexcept>
#include <utility>

#include "cadence/engine.hpp"
#include "cadence/validate.hpp"

namespace cadence {

Scenario make_s1() {
    Scenario scenario;
    scenario.name = "S1";
    scenario.horizon = 30;
    scenario.mutexes = {0};

    scenario.tasks.push_back(Task(
        0,
        "L",
        1,
        {
            Operation::compute(1),
            Operation::lock(0),
            Operation::compute(3),
            Operation::unlock(0),
            Operation::compute(1)
        },
        0
    ));

    scenario.tasks.push_back(Task(
        1,
        "M",
        2,
        {Operation::compute(4)},
        2,
        10
    ));

    scenario.tasks.push_back(Task(
        2,
        "H",
        3,
        {
            Operation::compute(1),
            Operation::lock(0),
            Operation::compute(1),
            Operation::unlock(0)
        },
        3,
        6
    ));

    return scenario;
}

SimulationResult simulate_scenario(
    const Scenario& scenario,
    Protocol protocol
) {
    const ValidationResult validation = validate(scenario);

    if (!validation.valid()) {
        throw std::invalid_argument(
            validation.errors.front().code == ValidationCode::MissingHorizon
                ? "scenario missing horizon"
                : validation.errors.front().message
        );
    }

    Engine engine(scenario.horizon.value(), protocol);

    for (const Task& task : scenario.tasks) {
        engine.add_task(task);
    }

    engine.run_to_horizon();

    return engine.result();
}

}  // namespace cadence