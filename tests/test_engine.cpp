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

    e.add_task(Task(
        1,
        "a",
        1,
        {Operation::compute(3)}
    ));

    EXPECT_TRUE(e.run_until_done(10));
    EXPECT_EQ(e.timeline(), (Timeline{1, 1, 1}));
    EXPECT_EQ(e.now(), 3);
    EXPECT_EQ(e.task(1).state(), TaskState::Completed);
}

TEST(EngineTest, HigherPriorityRunsFirst) {
    Engine e;

    e.add_task(Task(
        1,
        "low",
        1,
        {Operation::compute(2)}
    ));

    e.add_task(Task(
        2,
        "high",
        5,
        {Operation::compute(2)}
    ));

    e.run_until_done(10);

    EXPECT_EQ(e.timeline(), (Timeline{2, 2, 1, 1}));
}

TEST(EngineTest, EqualPriorityFollowsAddOrder) {
    Engine e;

    e.add_task(Task(
        3,
        "a",
        4,
        {Operation::compute(2)}
    ));

    e.add_task(Task(
        1,
        "b",
        4,
        {Operation::compute(2)}
    ));

    e.run_until_done(10);

    EXPECT_EQ(e.timeline(), (Timeline{3, 3, 1, 1}));
}

TEST(EngineTest, SleepCreatesIdleGap) {
    Engine e;

    e.add_task(Task(
        1,
        "a",
        1,
        {
            Operation::compute(1),
            Operation::sleep(2),
            Operation::compute(1)
        }
    ));

    EXPECT_TRUE(e.run_until_done(10));
    EXPECT_EQ(
        e.timeline(),
        (Timeline{1, kIdle, kIdle, 1})
    );
    EXPECT_EQ(e.now(), 4);
}

TEST(EngineTest, HigherPriorityWakePreemptsAndResumes) {
    Engine e;

    e.add_task(Task(
        1,
        "hi",
        9,
        {
            Operation::compute(1),
            Operation::sleep(2),
            Operation::compute(1)
        }
    ));

    e.add_task(Task(
        2,
        "lo",
        1,
        {Operation::compute(4)}
    ));

    EXPECT_TRUE(e.run_until_done(20));

    // The high-priority task wakes at tick 3,
    // preempts the low-priority task, then the low-priority
    // task resumes with its remaining work.
    EXPECT_EQ(
        e.timeline(),
        (Timeline{1, 2, 2, 1, 2, 2})
    );

    EXPECT_EQ(e.now(), 6);
}

TEST(EngineTest, EqualPriorityDoesNotPreempt) {
    Engine e;

    e.add_task(Task(
        1,
        "a",
        5,
        {
            Operation::sleep(1),
            Operation::compute(1)
        }
    ));

    e.add_task(Task(
        2,
        "b",
        5,
        {Operation::compute(3)}
    ));

    EXPECT_TRUE(e.run_until_done(20));

    EXPECT_EQ(
        e.timeline(),
        (Timeline{2, 2, 2, 1})
    );
}

TEST(EngineTest, PreemptedTaskGoesToFrontOfItsLevel) {
    Engine e;

    e.add_task(Task(
        1,
        "h",
        9,
        {
            Operation::sleep(1),
            Operation::compute(1)
        }
    ));

    e.add_task(Task(
        2,
        "a",
        1,
        {Operation::compute(3)}
    ));

    e.add_task(Task(
        3,
        "b",
        1,
        {Operation::compute(1)}
    ));

    EXPECT_TRUE(e.run_until_done(20));

    // Tick 0: A runs.
    // Tick 1: H wakes and preempts A.
    // A goes to the HEAD of priority 1.
    // H finishes in tick 1.
    // Ticks 2-3: A resumes.
    // Tick 4: B runs.
    EXPECT_EQ(
        e.timeline(),
        (Timeline{2, 1, 2, 2, 3})
    );
}

TEST(EngineTest, LockNotImplementedYet) {
    Engine e;

    e.add_task(Task(
        1,
        "a",
        1,
        {Operation::lock(0)}
    ));

    EXPECT_THROW(e.step(), std::logic_error);
}

TEST(EngineTest, DuplicateIdThrows) {
    Engine e;

    e.add_task(Task(
        1,
        "a",
        1,
        {Operation::compute(1)}
    ));

    EXPECT_THROW(
        e.add_task(Task(
            1,
            "b",
            2,
            {Operation::compute(1)}
        )),
        std::invalid_argument
    );
}

TEST(EngineTest, EmptyEngineIsIdle) {
    Engine e;

    e.step();

    EXPECT_EQ(
        e.timeline(),
        (Timeline{kIdle})
    );

    EXPECT_EQ(e.now(), 1);
}

TEST(EngineTest, RunUntilDoneStopsAtLimit) {
    Engine e;

    e.add_task(Task(
        1,
        "a",
        1,
        {Operation::compute(5)}
    ));

    EXPECT_FALSE(e.run_until_done(3));
    EXPECT_EQ(e.now(), 3);
}

TEST(EngineTest, UnknownTaskThrows) {
    Engine e;

    EXPECT_THROW(
        e.task(99),
        std::out_of_range
    );
}

TEST(EngineTest, EmptyProgramCompletesImmediately) {
    Engine e;

    e.add_task(Task(
        1,
        "a",
        1,
        {}
    ));

    e.step();

    EXPECT_EQ(
        e.task(1).state(),
        TaskState::Completed
    );

    EXPECT_EQ(
        e.timeline(),
        (Timeline{kIdle})
    );
}