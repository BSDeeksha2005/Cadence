#include <iostream>
#include <vector>

#include "cadence/operation.hpp"
#include "cadence/task.hpp"

int main() {
    using cadence::Operation;

    cadence::Task task(1, "sensor", 5,
                       {Operation::compute(2), Operation::lock(0),
                        Operation::compute(3), Operation::unlock(0),
                        Operation::sleep(4)});

    std::cout << "Cadence v0.1.0 - Step 1\n" << task.to_string() << "\n";
    return 0;
}