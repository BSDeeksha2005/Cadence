#include <iostream>
#include <string>

#include "cadence/event.hpp"
#include "cadence/metrics.hpp"
#include "cadence/scenario.hpp"
#include "cadence/adapter/scenario_json.hpp"

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

void print_usage(const char* program) {
    std::cerr << "Usage: " << program
              << " [--scenario FILE] [--protocol NONE|PIP|BOTH]\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        cadence::Scenario scenario = cadence::make_s1();
        std::string protocol = "BOTH";
        bool scenario_set = false;

        for (int i = 1; i < argc; ++i) {
            const std::string argument(argv[i]);
            if (argument == "--scenario" && i + 1 < argc) {
                scenario = cadence::load_scenario_json_file(argv[++i]);
                scenario_set = true;
            } else if (argument == "--protocol" && i + 1 < argc) {
                protocol = argv[++i];
            } else if (argument == "--none") {
                protocol = "NONE";
            } else if (argument == "--pip") {
                protocol = "PIP";
            } else if (argument == "--both") {
                protocol = "BOTH";
            } else if (argument == "--help" || argument == "-h") {
                print_usage(argv[0]);
                return 0;
            } else {
                print_usage(argv[0]);
                return 2;
            }
        }

        if (!scenario_set && argc == 1) scenario = cadence::make_s1();
        if (protocol == "BOTH") {
            print_run(scenario, cadence::Protocol::NONE);
            print_run(scenario, cadence::Protocol::PIP);
        } else if (protocol == "NONE") {
            print_run(scenario, cadence::Protocol::NONE);
        } else if (protocol == "PIP") {
            print_run(scenario, cadence::Protocol::PIP);
        } else {
            throw std::invalid_argument("protocol must be NONE, PIP, or BOTH");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "cadence: " << error.what() << '\n';
        return 2;
    }
}
