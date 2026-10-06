#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "cadence/engine.hpp"

using cadence::Engine;
using cadence::kIdle;
using cadence::Operation;
using cadence::Task;
using cadence::TaskId;
using cadence::TaskState;

using Timeline = std::vector<TaskId>;

TEST(EngineTest, SingleTaskRunsToCompletion) {
    Engine e;
    e.add_task(Task(1, "a", 1, {Operation::compute(3)}));
    EXPECT_TRUE(e.run_until_done(10));
    EXPECT_EQ(e.timeline(), (Timeline{1, 1, 1}));
    EXPECT_EQ(e.now(), 3);
    EXPECT_EQ(e.task(1).state(), TaskState::Completed);
}

TEST(EngineTest, HigherPriorityRunsFirst) {
    Engine e;
    e.add_task(Task(1, "low", 1, {Operation::compute(2)}));
    e.add_task(Task(2, "high", 5, {Operation::compute(2)}));
    e.run_until_done(10);
    EXPECT_EQ(e.timeline(), (Timeline{2, 2, 1, 1}));
}

TEST(EngineTest, EqualPriorityFollowsAddOrder) {
    Engine e;
    e.add_task(Task(3, "a", 4, {Operation::compute(2)}));
    e.add_task(Task(1, "b", 4, {Operation::compute(2)}));
    e.run_until_done(10);
    EXPECT_EQ(e.timeline(), (Timeline{3, 3, 1, 1}));
}

TEST(EngineTest, SleepCreatesIdleGap) {
    Engine e;
    e.add_task(Task(1, "a", 1,
                    {Operation::compute(1), Operation::sleep(2),
                     Operation::compute(1)}));
    EXPECT_TRUE(e.run_until_done(10));
    EXPECT_EQ(e.timeline(), (Timeline{1, kIdle, kIdle, 1}));
    EXPECT_EQ(e.now(), 4);
}

TEST(EngineTest, SleepingTaskYieldsCpuAndNoPreemptionYet) {
    Engine e;
    e.add_task(Task(1, "hi", 9,
                    {Operation::compute(1), Operation::sleep(2),
                     Operation::compute(1)}));
    e.add_task(Task(2, "lo", 1, {Operation::compute(4)}));
    EXPECT_TRUE(e.run_until_done(20));
    // hi wakes at tick 3 but lo keeps the CPU until its COMPUTE ends.
    EXPECT_EQ(e.timeline(), (Timeline{1, 2, 2, 2, 2, 1}));
    EXPECT_EQ(e.now(), 6);
}

TEST(EngineTest, LockNotImplementedYet) {
    Engine e;
    e.add_task(Task(1, "a", 1, {Operation::lock(0)}));
    EXPECT_THROW(e.step(), std::logic_error);
}

TEST(EngineTest, DuplicateIdThrows) {
    Engine e;
    e.add_task(Task(1, "a", 1, {Operation::compute(1)}));
    EXPECT_THROW(e.add_task(Task(1, "b", 2, {Operation::compute(1)})),
                 std::invalid_argument);
}

TEST(EngineTest, EmptyEngineIsIdle) {
    Engine e;
    e.step();
    EXPECT_EQ(e.timeline(), (Timeline{kIdle}));
    EXPECT_EQ(e.now(), 1);
}

TEST(EngineTest, RunUntilDoneStopsAtLimit) {
    Engine e;
    e.add_task(Task(1, "a", 1, {Operation::compute(5)}));
    EXPECT_FALSE(e.run_until_done(3));
    EXPECT_EQ(e.now(), 3);
}

TEST(EngineTest, UnknownTaskThrows) {
    Engine e;
    EXPECT_THROW(e.task(99), std::out_of_range);
}

TEST(EngineTest, EmptyProgramCompletesImmediately) {
    Engine e;
    e.add_task(Task(1, "a", 1, {}));
    e.step();
    EXPECT_EQ(e.task(1).state(), TaskState::Completed);
    EXPECT_EQ(e.timeline(), (Timeline{kIdle}));
}