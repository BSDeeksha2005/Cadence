#include <gtest/gtest.h>

#include "cadence/task.hpp"
#include <stdexcept>

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

TEST(TaskTest, StoresReleaseAndDeadline) {
    Task task(
        7,
        "job",
        5,
        {Operation::compute(2)},
        4,
        6
    );

    EXPECT_EQ(task.release(), 4);
    ASSERT_TRUE(task.relative_deadline().has_value());
    EXPECT_EQ(task.relative_deadline().value(), 6);

    ASSERT_TRUE(task.absolute_deadline().has_value());
    EXPECT_EQ(task.absolute_deadline().value(), 10);
}

TEST(TaskTest, ReleaseCannotBeNegative) {
    EXPECT_THROW(
        Task(
            1,
            "bad",
            1,
            {Operation::compute(1)},
            -1
        ),
        std::invalid_argument
    );
}

TEST(TaskTest, DeadlineMustBePositive) {
    EXPECT_THROW(
        Task(
            1,
            "bad",
            1,
            {Operation::compute(1)},
            0,
            0
        ),
        std::invalid_argument
    );
}