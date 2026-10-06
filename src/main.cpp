#include <iostream>

#include "cadence/engine.hpp"
#include "cadence/operation.hpp"
#include "cadence/task.hpp"



int main() {
    using cadence::Operation;

    cadence::Engine engine;
    engine.add_task(cadence::Task(1, "sensor", 5,
                                  {Operation::compute(2), Operation::sleep(2),
                                   Operation::compute(1)}));
    engine.add_task(cadence::Task(2, "logger", 1, {Operation::compute(4)}));

    engine.run_until_done(50);
    
    std::cout << "Cadence v0.1.0 - Step 4\nTimeline: ";
    for (cadence::TaskId id : engine.timeline()) {
        if (id == cadence::kIdle) {
            std::cout << "-- ";
        } else {
            std::cout << "T" << id << " ";
        }
    }
    std::cout << "\n";
    return 0;
}