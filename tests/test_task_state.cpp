#include <gtest/gtest.h>

#include "cadence/task_state.hpp"

using cadence::TaskState;

TEST(TaskStateTest, ToStringCoversAllStates) {
    EXPECT_EQ(cadence::to_string(TaskState::New), "NEW");
    EXPECT_EQ(cadence::to_string(TaskState::Ready), "READY");
    EXPECT_EQ(cadence::to_string(TaskState::Running), "RUNNING");
    EXPECT_EQ(cadence::to_string(TaskState::Blocked), "BLOCKED");
    EXPECT_EQ(cadence::to_string(TaskState::Sleeping), "SLEEPING");
    EXPECT_EQ(cadence::to_string(TaskState::Completed), "COMPLETED");
}