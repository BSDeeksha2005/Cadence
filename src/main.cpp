#include <iostream>
#include <string>

#include "cadence/event.hpp"
#include "cadence/metrics.hpp"
#include "cadence/scenario.hpp"

namespace {

void print_run(
    const cadence::Scenario& scenario,
    cadence::Protocol protocol
) {
    const cadence::SimulationResult result =
        cadence::simulate_scenario(scenario, protocol);

    const cadence::RunMetrics metrics =
        cadence::compute_metrics(scenario, result);

    std::cout
        << (protocol == cadence::Protocol::PIP ? "PIP" : "NONE")
        << " status=" << cadence::to_string(result.status)
        << " end=" << result.end_time
        << " trace_hash=" << cadence::trace_hash(result)
        << "\n";

    std::cout << "timeline:";
    for (const cadence::TickRow& row : result.ticks) {
        std::cout << ' ' << row.running;
    }
    std::cout << "\n";

    for (const cadence::TaskMetrics& task : metrics.tasks) {
        std::cout
            << "T" << task.id
            << " response="
            << (task.response.has_value()
                    ? std::to_string(task.response.value())
                    : "-")
            << " blocked=" << task.blocked_ticks
            << " inversion=" << task.inversion_ticks
            << " legit_blocking=" << task.legitimate_blocking
            << " deadline_miss="
            << (task.deadline_missed ? "yes" : "no")
            << "\n";
    }

    std::cout
        << "preemptions=" << metrics.preemptions
        << " context_switches=" << metrics.context_switches
        << " busy_ticks=" << metrics.busy_ticks
        << "/" << metrics.total_ticks
        << "\n\n";
}

}  // namespace

int main(int argc, char** argv) {
    const cadence::Scenario scenario = cadence::make_s1();

    if (argc == 1) {
        print_run(scenario, cadence::Protocol::NONE);
        print_run(scenario, cadence::Protocol::PIP);
        return 0;
    }

    const std::string arg(argv[1]);

    if (arg == "--none") {
        print_run(scenario, cadence::Protocol::NONE);
        return 0;
    }

    if (arg == "--pip") {
        print_run(scenario, cadence::Protocol::PIP);
        return 0;
    }

    if (arg == "--both") {
        print_run(scenario, cadence::Protocol::NONE);
        print_run(scenario, cadence::Protocol::PIP);
        return 0;
    }

    std::cerr << "Usage: cadence [--none|--pip|--both]\n";
    return 2;
}