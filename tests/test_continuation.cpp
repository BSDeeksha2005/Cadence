#include <gtest/gtest.h>
#include <vector>

#include "cadence/engine.hpp"
#include "cadence/operation.hpp"
#include "cadence/task.hpp"

TEST(ContinuationTest, CompletionAtReleaseInstantIsProcessedBeforeRelease) {
    cadence::Engine e(10);

    e.add_task(cadence::Task(
        1, "A", 5,
        {cadence::Operation::compute(1)},
        0
    ));
    e.add_task(cadence::Task(
        2, "B", 9,
        {cadence::Operation::compute(1)},
        1
    ));

    EXPECT_TRUE(e.run_to_horizon());
    ASSERT_EQ(e.timeline().size(), 2u);
    EXPECT_EQ(e.timeline()[0], 1);
    EXPECT_EQ(e.timeline()[1], 2);
    EXPECT_FALSE(e.deadline_missed(1));
}

TEST(ContinuationTest, SameInstantWakeAndReleaseUseTaskIdOrdering) {
    cadence::Engine e(10);

    e.add_task(cadence::Task(
        3, "wake", 5,
        {
            cadence::Operation::sleep(1),
            cadence::Operation::compute(1)
        },
        0
    ));

    e.add_task(cadence::Task(
        1, "release", 5,
        {cadence::Operation::compute(1)},
        1
    ));

    EXPECT_TRUE(e.run_to_horizon());
    EXPECT_EQ(
        e.timeline(),
        (std::vector<cadence::TaskId>{cadence::kIdle, 1, 3})
    );
}