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

TEST(EngineTest, SingleTaskRunsToCompletion)
{

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

TEST(EngineTest, HigherPriorityRunsFirst)
{

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

TEST(EngineTest, EqualPriorityFollowsTaskIdAtSameRelease)
{

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

              (Timeline{1, 1, 3, 3}));
}

TEST(EngineTest, SleepCreatesIdleGap)
{

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

TEST(EngineTest, HigherPriorityWakePreemptsAndResumes)
{

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

TEST(EngineTest, EqualPriorityDoesNotPreempt)
{

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

TEST(EngineTest, PreemptedTaskGoesToFrontOfItsLevel)
{

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

TEST(EngineTest, DuplicateIdThrows)
{

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

TEST(EngineTest, EmptyEngineEndsImmediately)
{

    Engine e(10);

    e.step();

    EXPECT_TRUE(e.finished());

    EXPECT_TRUE(e.all_completed());

    EXPECT_TRUE(e.timeline().empty());

    EXPECT_EQ(e.now(), 0);
}

TEST(EngineTest, RunUntilDoneStopsAtLimit)
{

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

TEST(EngineTest, UnknownTaskThrows)
{

    Engine e;

    EXPECT_THROW(

        e.task(99),

        std::out_of_range

    );
}

TEST(EngineTest, EmptyProgramCompletesImmediately)
{

    Engine e(10);

    e.add_task(Task(1, "a", 1, {}));

    e.step();

    EXPECT_EQ(

        e.task(1).state(),

        TaskState::Completed

    );

    EXPECT_TRUE(e.timeline().empty());

    EXPECT_EQ(e.now(), 0);

    EXPECT_EQ(

        e.completion_time(1).value(),

        0

    );
}

// -----------------------------

// Step 6: Mutex + Blocking

// -----------------------------

TEST(EngineTest, LockAcquireAndUnlockWorks)
{

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

TEST(EngineTest, SecondTaskBlocksOnOwnedMutex)
{

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

TEST(EngineTest, UnlockDirectlyHandsMutexToWaiter)
{

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

TEST(EngineTest, HighestPriorityWaiterWinsAtUnlock)
{

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

TEST(EngineTest, EqualPrioritySameReleaseUsesTaskIdOrdering)
{

    Engine e(10);

    e.add_task(

        Task(

            3,

            "a",

            4,

            {Operation::compute(2)}

            )

    );

    e.add_task(

        Task(

            1,

            "b",

            4,

            {Operation::compute(2)}

            )

    );

    EXPECT_TRUE(e.run_until_done(10));

    EXPECT_EQ(

        e.timeline(),

        (Timeline{1, 1, 3, 3})

    );
}

TEST(EngineTest, RecursiveLockThrows)
{

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
        std::logic_error);

    EXPECT_THROW(
        e.step(),
        std::logic_error);
}

TEST(EngineTest, UnlockByNonOwnerThrows)
{

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

TEST(EngineTest, CompletingWhileHoldingMutexThrows)
{

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

    EXPECT_NO_THROW(e.step());

    EXPECT_THROW(
        e.step(),
        std::logic_error);
}

// -----------------------------

// Step 7: Priority Inheritance

// -----------------------------

TEST(PriorityInheritanceTest, NoneKeepsBasePriority)
{

    Engine e(cadence::Protocol::NONE);

    e.add_task(Task(

        1,

        "L",

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

        "M",

        2,

        {

            Operation::sleep(1),

            Operation::compute(2)

        }

        ));

    e.add_task(Task(

        3,

        "H",

        3,

        {

            Operation::sleep(2),

            Operation::lock(0),

            Operation::compute(1),

            Operation::unlock(0)

        }

        ));

    e.step();

    e.step();

    e.step();

    EXPECT_EQ(

        e.task(1).effective_priority(),

        1

    );

    EXPECT_EQ(

        e.task(3).state(),

        TaskState::Blocked

    );

    EXPECT_EQ(

        e.mutex_owner(0),

        1

    );

    EXPECT_TRUE(

        e.run_until_done(20)

    );

    EXPECT_EQ(

        e.timeline(),

        (Timeline{1, 2, 2, 1, 1, 3, 1})

    );
}

TEST(PriorityInheritanceTest, PIPBoostsOwner)
{

    Engine e(cadence::Protocol::PIP);

    e.add_task(Task(

        1,

        "L",

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

        "M",

        2,

        {

            Operation::sleep(1),

            Operation::compute(2)

        }

        ));

    e.add_task(Task(

        3,

        "H",

        3,

        {

            Operation::sleep(2),

            Operation::lock(0),

            Operation::compute(1),

            Operation::unlock(0)

        }

        ));

    e.step(); // tick 0: L runs

    e.step(); // tick 1: M runs

    e.step(); // tick 2: H blocks, L inherits 3

    EXPECT_EQ(

        e.task(1).effective_priority(),

        3

    );

    EXPECT_EQ(

        e.task(3).state(),

        TaskState::Blocked

    );

    EXPECT_EQ(

        e.mutex_owner(0),

        1

    );

    EXPECT_EQ(

        e.timeline(),

        (Timeline{1, 2, 1})

    );

    // tick 3: L finishes its final COMPUTE.

    // UNLOCK executes at the next boundary.

    e.step();

    EXPECT_EQ(

        e.task(1).effective_priority(),

        3

    );

    EXPECT_EQ(

        e.mutex_owner(0),

        1

    );

    // tick 4 boundary: L executes UNLOCK,

    // hands R directly to H, then loses inheritance.

    e.step();

    EXPECT_EQ(

        e.task(1).effective_priority(),

        1

    );

    EXPECT_EQ(

        e.mutex_owner(0),

        3

    );

    EXPECT_TRUE(

        e.run_until_done(20)

    );

    EXPECT_EQ(

        e.timeline(),

        (Timeline{1, 2, 1, 1, 3, 2, 1})

    );
}

TEST(PriorityInheritanceTest,

     ChainedInheritancePropagates)
{

    Engine e(cadence::Protocol::PIP);

    // L owns R2.

    e.add_task(Task(

        1,

        "L",

        1,

        {

            Operation::lock(2),

            Operation::compute(3),

            Operation::unlock(2)

        }

        ));

    // M owns R1, then blocks on R2.

    e.add_task(Task(

        2,

        "M",

        2,

        {

            Operation::sleep(1),

            Operation::lock(1),

            Operation::lock(2),

            Operation::compute(1),

            Operation::unlock(2),

            Operation::unlock(1)

        }

        ));

    // H blocks on R1, which M owns.

    e.add_task(Task(

        3,

        "H",

        3,

        {

            Operation::sleep(2),

            Operation::lock(1),

            Operation::compute(1),

            Operation::unlock(1)

        }

        ));

    e.step(); // L runs

    e.step(); // M blocks on L's R2

    e.step(); // H blocks on M's R1

    EXPECT_EQ(

        e.task(2).effective_priority(),

        3

    );

    EXPECT_EQ(

        e.task(1).effective_priority(),

        3

    );

    EXPECT_EQ(

        e.task(3).state(),

        TaskState::Blocked

    );

    EXPECT_EQ(

        e.mutex_owner(2),

        1

    );

    e.step(); // L hands R2 to M

    EXPECT_EQ(

        e.task(1).effective_priority(),

        1

    );

    EXPECT_EQ(

        e.task(2).effective_priority(),

        3

    );

    EXPECT_EQ(

        e.mutex_owner(2),

        2

    );

    EXPECT_TRUE(

        e.run_until_done(30)

    );
}

TEST(EngineTest, TaskIsReleasedAtItsReleaseTime)
{

    Engine e(10);

    e.add_task(
        Task(
            1,
            "late",
            5,
            {Operation::compute(2)},
            3));

    e.step(); // t=0
    e.step(); // t=1
    e.step(); // t=2

    EXPECT_EQ(
        e.timeline(),
        (Timeline{kIdle, kIdle, kIdle}));

    EXPECT_EQ(
        e.task(1).state(),
        TaskState::New);

    e.step(); // t=3: release + first compute tick

    EXPECT_EQ(
        e.timeline(),
        (Timeline{
            kIdle,
            kIdle,
            kIdle,
            1}));

    EXPECT_EQ(
        e.task(1).state(),
        TaskState::Running);

    e.step(); // t=4: second/final compute tick

    EXPECT_EQ(
        e.timeline(),
        (Timeline{
            kIdle,
            kIdle,
            kIdle,
            1,
            1}));

    e.step(); // t=5 boundary: implicit END -> COMPLETED

    EXPECT_EQ(
        e.task(1).state(),
        TaskState::Completed);

    EXPECT_EQ(e.now(), 5);
}

TEST(EngineTest, ReleaseOrderingUsesTaskId)
{

    Engine e(10);

    e.add_task(

        Task(

            20,

            "twenty",

            5,

            {Operation::compute(1)},

            2

            )

    );

    e.add_task(

        Task(

            10,

            "ten",

            5,

            {Operation::compute(1)},

            2

            )

    );

    EXPECT_TRUE(e.run_to_horizon());

    EXPECT_EQ(

        e.timeline(),

        (Timeline{kIdle, kIdle, 10, 20})

    );
}

TEST(EngineTest, DeadlineMissIsRecorded)
{

    Engine e(10);

    e.add_task(

        Task(

            1,

            "late",

            5,

            {Operation::compute(3)},

            1,

            2

            )

    );

    EXPECT_TRUE(

        e.run_until_done(10)

    );

    EXPECT_TRUE(

        e.deadline_missed(1)

    );

    ASSERT_TRUE(

        e.completion_time(1).has_value()

    );

    EXPECT_EQ(

        e.completion_time(1).value(),

        4

    );
}

TEST(EngineTest, CompletionExactlyAtDeadlineDoesNotMiss)
{

    Engine e(10);

    e.add_task(

        Task(

            1,

            "on_time",

            5,

            {Operation::compute(2)},

            1,

            2

            )

    );

    EXPECT_TRUE(

        e.run_until_done(10)

    );

    EXPECT_FALSE(

        e.deadline_missed(1)

    );

    ASSERT_TRUE(

        e.completion_time(1).has_value()

    );

    EXPECT_EQ(

        e.completion_time(1).value(),

        3

    );
}

TEST(EngineTest, HorizonStopsBeforeNextTick)
{

    Engine e(3);

    e.add_task(

        Task(

            1,

            "long",

            5,

            {Operation::compute(10)}

            )

    );

    EXPECT_FALSE(

        e.run_to_horizon()

    );

    EXPECT_EQ(

        e.now(),

        3

    );

    EXPECT_EQ(

        e.timeline(),

        (Timeline{1, 1, 1})

    );

    EXPECT_EQ(

        e.task(1).state(),

        TaskState::Running

    );
}

TEST(EngineTest, DeadlineAtHorizonIsChecked)
{

    Engine e(3);

    e.add_task(

        Task(

            1,

            "late",

            5,

            {Operation::compute(10)},

            0,

            3

            )

    );

    EXPECT_FALSE(

        e.run_to_horizon()

    );

    EXPECT_TRUE(

        e.finished()

    );

    EXPECT_TRUE(

        e.deadline_missed(1)

    );

    EXPECT_EQ(

        e.now(),

        3

    );

    EXPECT_EQ(

        e.timeline(),

        (Timeline{1, 1, 1})

    );
}

// -----------------------------
// Step 9: Deadlock Detection
// -----------------------------

TEST(DeadlockTest, TwoTaskDeadlockDetected)
{

    Engine e(20, cadence::Protocol::NONE);

    // Task 1 owns R0, then sleeps while holding it.
    // It later requests R1.
    e.add_task(Task(
        1,
        "A",
        1,
        {Operation::lock(0),
         Operation::sleep(1),
         Operation::lock(1)},
        0));

    // Task 2 arrives at t=1, owns R1, then requests R0.
    e.add_task(Task(
        2,
        "B",
        2,
        {Operation::lock(1),
         Operation::lock(0)},
        1));

    EXPECT_FALSE(e.run_to_horizon());

    EXPECT_TRUE(e.finished());

    EXPECT_EQ(
        e.status(),
        cadence::RunStatus::Deadlock);

    EXPECT_TRUE(e.deadlocked());

    EXPECT_EQ(
        e.task(1).state(),
        TaskState::Blocked);

    EXPECT_EQ(
        e.task(2).state(),
        TaskState::Blocked);

    EXPECT_EQ(
        e.mutex_owner(0),
        1);

    EXPECT_EQ(
        e.mutex_owner(1),
        2);

    EXPECT_EQ(
        e.now(),
        1);

    EXPECT_EQ(
        e.timeline(),
        (Timeline{kIdle}));
}

TEST(DeadlockTest, TwoTaskDeadlockDetectedUnderPIP)
{

    Engine e(20, cadence::Protocol::PIP);

    e.add_task(Task(
        1,
        "A",
        1,
        {Operation::lock(0),
         Operation::sleep(1),
         Operation::lock(1)},
        0));

    e.add_task(Task(
        2,
        "B",
        2,
        {Operation::lock(1),
         Operation::lock(0)},
        1));

    EXPECT_FALSE(e.run_to_horizon());

    EXPECT_TRUE(e.finished());

    EXPECT_EQ(
        e.status(),
        cadence::RunStatus::Deadlock);

    EXPECT_TRUE(e.deadlocked());

    EXPECT_EQ(
        e.task(1).state(),
        TaskState::Blocked);

    EXPECT_EQ(
        e.task(2).state(),
        TaskState::Blocked);

    EXPECT_EQ(
        e.mutex_owner(0),
        1);

    EXPECT_EQ(
        e.mutex_owner(1),
        2);
}

TEST(DeadlockTest, ThreeTaskCycleDetected)
{

    Engine e(30, cadence::Protocol::PIP);

    // A owns R0 and wakes at t=3.
    e.add_task(Task(
        1,
        "A",
        1,
        {Operation::lock(0),
         Operation::sleep(3),
         Operation::lock(2)},
        0));

    // B owns R1 and blocks on R0.
    e.add_task(Task(
        2,
        "B",
        2,
        {Operation::lock(1),
         Operation::lock(0)},
        1));

    // C owns R2 and blocks on R1.
    e.add_task(Task(
        3,
        "C",
        3,
        {Operation::lock(2),
         Operation::lock(1)},
        2));

    EXPECT_FALSE(e.run_to_horizon());

    EXPECT_TRUE(e.finished());

    EXPECT_EQ(
        e.status(),
        cadence::RunStatus::Deadlock);

    EXPECT_TRUE(e.deadlocked());

    EXPECT_EQ(
        e.task(1).state(),
        TaskState::Blocked);

    EXPECT_EQ(
        e.task(2).state(),
        TaskState::Blocked);

    EXPECT_EQ(
        e.task(3).state(),
        TaskState::Blocked);

    EXPECT_EQ(
        e.mutex_owner(0),
        1);

    EXPECT_EQ(
        e.mutex_owner(1),
        2);

    EXPECT_EQ(
        e.mutex_owner(2),
        3);
}

TEST(DeadlockTest, BlockingChainWithoutCycleIsNotDeadlock)
{

    Engine e(20, cadence::Protocol::PIP);

    // A owns R0 and releases it after waking.
    e.add_task(Task(
        1,
        "A",
        1,
        {Operation::lock(0),
         Operation::sleep(1),
         Operation::unlock(0)},
        0));

    // B arrives while A owns R0.
    e.add_task(Task(
        2,
        "B",
        2,
        {Operation::lock(0),
         Operation::compute(1),
         Operation::unlock(0)},
        1));

    EXPECT_TRUE(e.run_to_horizon());

    EXPECT_TRUE(e.finished());

    EXPECT_EQ(
        e.status(),
        cadence::RunStatus::Completed);

    EXPECT_FALSE(e.deadlocked());

    EXPECT_EQ(
        e.task(1).state(),
        TaskState::Completed);

    EXPECT_EQ(
        e.task(2).state(),
        TaskState::Completed);

    EXPECT_EQ(
        e.mutex_owner(0),
        kIdle);
}