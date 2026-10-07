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
    EXPECT_EQ(e.task(1).state(),
              TaskState::Completed);
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

    EXPECT_TRUE(e.run_until_done(10));
    EXPECT_EQ(e.timeline(),
              (Timeline{2, 2, 1, 1}));
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

    EXPECT_TRUE(e.run_until_done(10));
    EXPECT_EQ(e.timeline(),
              (Timeline{3, 3, 1, 1}));
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

    EXPECT_EQ(
        e.timeline(),
        (Timeline{2, 1, 2, 2, 3})
    );
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

    EXPECT_FALSE(
        e.run_until_done(3)
    );

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

// -----------------------------
// Step 6: Mutex + Blocking
// -----------------------------

TEST(EngineTest, LockAcquireAndUnlockWorks) {
    Engine e;

    e.add_task(Task(
        1,
        "owner",
        1,
        {
            Operation::lock(0),
            Operation::compute(2),
            Operation::unlock(0),
            Operation::compute(1)
        }
    ));

    EXPECT_TRUE(e.run_until_done(20));

    EXPECT_EQ(
        e.timeline(),
        (Timeline{1, 1, 1})
    );

    EXPECT_EQ(
        e.mutex_owner(0),
        kIdle
    );

    EXPECT_TRUE(
        e.mutex_waiters(0).empty()
    );

    EXPECT_EQ(
        e.task(1).state(),
        TaskState::Completed
    );
}

TEST(EngineTest, SecondTaskBlocksOnOwnedMutex) {
    Engine e;

    e.add_task(Task(
        1,
        "low",
        1,
        {
            Operation::lock(0),
            Operation::compute(2),
            Operation::unlock(0),
            Operation::compute(1)
        }
    ));

    e.add_task(Task(
        2,
        "high",
        5,
        {
            Operation::sleep(1),
            Operation::lock(0),
            Operation::compute(1),
            Operation::unlock(0)
        }
    ));

    e.step();

    EXPECT_EQ(
        e.mutex_owner(0),
        1
    );

    e.step();

    EXPECT_EQ(
        e.task(2).state(),
        TaskState::Blocked
    );

    EXPECT_EQ(
        e.mutex_waiters(0),
        (std::vector<TaskId>{2})
    );

    EXPECT_EQ(
        e.mutex_owner(0),
        1
    );

    EXPECT_EQ(
        e.timeline()[1],
        1
    );

    EXPECT_TRUE(
        e.run_until_done(20)
    );

    EXPECT_EQ(
        e.timeline(),
        (Timeline{1, 1, 2, 1})
    );

    EXPECT_EQ(
        e.task(1).state(),
        TaskState::Completed
    );

    EXPECT_EQ(
        e.task(2).state(),
        TaskState::Completed
    );

    EXPECT_EQ(
        e.mutex_owner(0),
        kIdle
    );
}

TEST(EngineTest, UnlockDirectlyHandsMutexToWaiter) {
    Engine e;

    e.add_task(Task(
        1,
        "owner",
        1,
        {
            Operation::lock(0),
            Operation::compute(2),
            Operation::unlock(0),
            Operation::compute(1)
        }
    ));

    e.add_task(Task(
        2,
        "waiter",
        5,
        {
            Operation::sleep(1),
            Operation::lock(0),
            Operation::compute(1),
            Operation::unlock(0)
        }
    ));

    e.step();
    e.step();

    EXPECT_EQ(
        e.task(2).state(),
        TaskState::Blocked
    );

    e.step();

    EXPECT_EQ(
        e.mutex_owner(0),
        2
    );

    EXPECT_EQ(
        e.task(2).state(),
        TaskState::Running
    );

    EXPECT_EQ(
        e.task(2).program_size(),
        4u
    );
}

TEST(EngineTest, HighestPriorityWaiterWinsAtUnlock) {
    Engine e;

    e.add_task(Task(
        1,
        "owner",
        1,
        {
            Operation::lock(0),
            Operation::compute(3),
            Operation::unlock(0),
            Operation::compute(1)
        }
    ));

    e.add_task(Task(
        2,
        "medium",
        2,
        {
            Operation::sleep(1),
            Operation::lock(0),
            Operation::compute(1),
            Operation::unlock(0)
        }
    ));

    e.add_task(Task(
        3,
        "high",
        3,
        {
            Operation::sleep(2),
            Operation::lock(0),
            Operation::compute(1),
            Operation::unlock(0)
        }
    ));

    EXPECT_TRUE(e.run_until_done(30));

    EXPECT_EQ(
        e.mutex_owner(0),
        kIdle
    );

    EXPECT_EQ(
        e.timeline(),
        (Timeline{1, 1, 1, 3, 2, 1})
    );
}

TEST(EngineTest, EqualPriorityWaitersKeepArrivalOrder) {
    Engine e;

    e.add_task(Task(
        1,
        "owner",
        1,
        {
            Operation::lock(0),
            Operation::compute(3),
            Operation::unlock(0),
            Operation::compute(1)
        }
    ));

    e.add_task(Task(
        2,
        "first",
        2,
        {
            Operation::sleep(1),
            Operation::lock(0),
            Operation::compute(1),
            Operation::unlock(0)
        }
    ));

    e.add_task(Task(
        3,
        "second",
        2,
        {
            Operation::sleep(2),
            Operation::lock(0),
            Operation::compute(1),
            Operation::unlock(0)
        }
    ));

    EXPECT_TRUE(e.run_until_done(30));

    EXPECT_EQ(
        e.timeline(),
        (Timeline{1, 1, 1, 2, 3, 1})
    );

    EXPECT_EQ(
        e.mutex_owner(0),
        kIdle
    );
}

TEST(EngineTest, RecursiveLockThrows) {
    Engine e;

    e.add_task(Task(
        1,
        "recursive",
        1,
        {
            Operation::lock(0),
            Operation::lock(0)
        }
    ));

    EXPECT_THROW(
        e.step(),
        std::logic_error
    );
}

TEST(EngineTest, UnlockByNonOwnerThrows) {
    Engine e;

    e.add_task(Task(
        1,
        "bad",
        1,
        {
            Operation::unlock(0)
        }
    ));

    EXPECT_THROW(
        e.step(),
        std::logic_error
    );
}

TEST(EngineTest, CompletingWhileHoldingMutexThrows) {
    Engine e;

    e.add_task(Task(
        1,
        "bad",
        1,
        {
            Operation::lock(0),
            Operation::compute(1)
        }
    ));

    EXPECT_THROW(
        e.step(),
        std::logic_error
    );
}