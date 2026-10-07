#include <gtest/gtest.h>

#include <vector>

#include "cadence/metrics.hpp"
#include "cadence/scenario.hpp"

using cadence::Protocol;

TEST(S1Test, NoneMatchesGoldenTimelineAndMetrics) {
    const cadence::Scenario scenario = cadence::make_s1();

    const cadence::SimulationResult result =
        cadence::simulate_scenario(
            scenario,
            Protocol::NONE
        );

    const cadence::RunMetrics metrics =
        cadence::compute_metrics(
            scenario,
            result
        );

    ASSERT_EQ(result.end_time, 11);
    ASSERT_EQ(result.ticks.size(), 11u);

    const std::vector<cadence::TaskId> timeline = [&] {
        std::vector<cadence::TaskId> ids;

        for (const auto& row : result.ticks) {
            ids.push_back(row.running);
        }

        return ids;
    }();

    EXPECT_EQ(
        timeline,
        (std::vector<cadence::TaskId>{
            0, 0, 1, 2, 1, 1, 1, 0, 0, 2, 0
        })
    );

    EXPECT_EQ(metrics.tasks[0].response.value(), 11);
    EXPECT_EQ(metrics.tasks[1].response.value(), 5);
    EXPECT_EQ(metrics.tasks[2].response.value(), 7);

    EXPECT_EQ(metrics.tasks[2].blocked_ticks, 5);
    EXPECT_EQ(metrics.tasks[2].inversion_ticks, 3);
    EXPECT_EQ(metrics.tasks[2].legitimate_blocking, 2);

    EXPECT_TRUE(metrics.tasks[2].deadline_missed);
    EXPECT_EQ(metrics.tasks[2].lateness.value(), 1);

    EXPECT_EQ(metrics.preemptions, 3);
    EXPECT_EQ(metrics.context_switches, 6);
    EXPECT_EQ(metrics.busy_ticks, 11);
    EXPECT_EQ(metrics.total_ticks, 11);
}

TEST(S1Test, PipMatchesGoldenTimelineAndMetrics) {
    const cadence::Scenario scenario = cadence::make_s1();

    const cadence::SimulationResult result =
        cadence::simulate_scenario(
            scenario,
            Protocol::PIP
        );

    const cadence::RunMetrics metrics =
        cadence::compute_metrics(
            scenario,
            result
        );

    const std::vector<cadence::TaskId> timeline = [&] {
        std::vector<cadence::TaskId> ids;

        for (const auto& row : result.ticks) {
            ids.push_back(row.running);
        }

        return ids;
    }();

    EXPECT_EQ(
        timeline,
        (std::vector<cadence::TaskId>{
            0, 0, 1, 2, 0, 0, 2, 1, 1, 1, 0
        })
    );

    EXPECT_EQ(metrics.tasks[0].response.value(), 11);
    EXPECT_EQ(metrics.tasks[1].response.value(), 8);
    EXPECT_EQ(metrics.tasks[2].response.value(), 4);

    EXPECT_EQ(metrics.tasks[2].blocked_ticks, 2);
    EXPECT_EQ(metrics.tasks[2].inversion_ticks, 0);
    EXPECT_EQ(metrics.tasks[2].legitimate_blocking, 2);

    EXPECT_FALSE(metrics.tasks[2].deadline_missed);
    EXPECT_EQ(metrics.tasks[2].lateness.value(), 0);

    EXPECT_EQ(metrics.preemptions, 3);
    EXPECT_EQ(metrics.context_switches, 6);
    EXPECT_EQ(metrics.busy_ticks, 11);
    EXPECT_EQ(metrics.total_ticks, 11);
}