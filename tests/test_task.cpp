#include <gtest/gtest.h>

#include "cadence/task.hpp"

using cadence::Operation;
using cadence::Task;
using cadence::TaskState;

TEST(TaskTest, StartsInNewState) {
    Task t(1, "a", 3, {Operation::compute(1)});
    EXPECT_EQ(t.state(), TaskState::New);
}

TEST(TaskTest, StoresFields) {
    Task t(7, "logger", 2, {Operation::compute(1), Operation::sleep(2)});
    EXPECT_EQ(t.id(), 7);
    EXPECT_EQ(t.name(), "logger");
    EXPECT_EQ(t.base_priority(), 2);
    EXPECT_EQ(t.program_size(), 2u);
}

TEST(TaskTest, ProgramKeepsOrder) {
    Task t(1, "a", 1,
           {Operation::lock(0), Operation::compute(2), Operation::unlock(0)});
    ASSERT_EQ(t.program().size(), 3u);
    EXPECT_EQ(t.program()[0], Operation::lock(0));
    EXPECT_EQ(t.program()[1], Operation::compute(2));
    EXPECT_EQ(t.program()[2], Operation::unlock(0));
}

TEST(TaskTest, SetStateChangesState) {
    Task t(1, "a", 1, {});
    t.set_state(TaskState::Ready);
    EXPECT_EQ(t.state(), TaskState::Ready);
}

TEST(TaskTest, ToStringIncludesProgram) {
    Task t(1, "a", 4, {Operation::compute(2), Operation::sleep(1)});
    EXPECT_EQ(t.to_string(),
              "Task 1 'a' prio=4 state=NEW program=[COMPUTE(2), SLEEP(1)]");
}