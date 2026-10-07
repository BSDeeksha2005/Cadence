#include <gtest/gtest.h>

#include "cadence/metrics.hpp"
#include "cadence/scenario.hpp"

TEST(MetricsTest, CompletionAtDeadlineIsNotLate) {
    cadence::Scenario scenario;
    scenario.name = "deadline";
    scenario.horizon = 10;

    scenario.tasks.push_back(cadence::Task(
        1,
        "T",
        5,
        {cadence::Operation::compute(2)},
        0,
        2
    ));

    const auto result =
        cadence::simulate_scenario(
            scenario,
            cadence::Protocol::NONE
        );

    const auto metrics =
        cadence::compute_metrics(
            scenario,
            result
        );

    ASSERT_EQ(metrics.tasks.size(), 1u);
    EXPECT_FALSE(metrics.tasks[0].deadline_missed);

    ASSERT_TRUE(metrics.tasks[0].lateness.has_value());
    EXPECT_EQ(metrics.tasks[0].lateness.value(), 0);
}

TEST(MetricsTest, BlockingAndInversionUseTickSnapshots) {
    const auto scenario = cadence::make_s1();

    const auto result =
        cadence::simulate_scenario(
            scenario,
            cadence::Protocol::NONE
        );

    const auto metrics =
        cadence::compute_metrics(
            scenario,
            result
        );

    EXPECT_EQ(metrics.tasks[2].blocked_ticks, 5);
    EXPECT_EQ(metrics.tasks[2].inversion_ticks, 3);
    EXPECT_EQ(metrics.tasks[2].legitimate_blocking, 2);
}