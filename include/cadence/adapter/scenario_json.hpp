#pragma once

#include <string>

#include "cadence/scenario.hpp"

namespace cadence {

// JSON and filesystem operations stay outside the deterministic kernel.
Scenario parse_scenario_json(const std::string& text);
Scenario load_scenario_json_file(const std::string& path);

}  // namespace cadence
