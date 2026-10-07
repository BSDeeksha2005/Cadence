#include <algorithm>
#include <gtest/gtest.h>

#include "cadence/event.hpp"
#include "cadence/scenario.hpp"

TEST(DeterminismTest, S1TraceHashIsStable) {
    const auto scenario = cadence::make_s1();

    const auto first =
        cadence::simulate_scenario(scenario, cadence::Protocol::PIP);
    const auto second =
        cadence::simulate_scenario(scenario, cadence::Protocol::PIP);

    EXPECT_EQ(cadence::trace_hash(first), cadence::trace_hash(second));
    EXPECT_EQ(cadence::canonical_trace(first),
              cadence::canonical_trace(second));
}

TEST(DeterminismTest, TaskDeclarationOrderDoesNotMatter) {
    auto first_scenario = cadence::make_s1();
    auto second_scenario = first_scenario;
    std::reverse(
        second_scenario.tasks.begin(),
        second_scenario.tasks.end()
    );

    const auto first =
        cadence::simulate_scenario(first_scenario, cadence::Protocol::NONE);
    const auto second =
        cadence::simulate_scenario(second_scenario, cadence::Protocol::NONE);

    EXPECT_EQ(cadence::trace_hash(first), cadence::trace_hash(second));
}