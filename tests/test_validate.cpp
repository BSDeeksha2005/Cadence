#include <gtest/gtest.h>

#include "cadence/scenario.hpp"
#include "cadence/validate.hpp"

TEST(ValidationTest, MissingHorizonIsReported) {
    cadence::Scenario scenario;
    scenario.name = "bad";

    scenario.tasks.push_back(
        cadence::Task(
            1,
            "t",
            1,
            {cadence::Operation::compute(1)}
        )
    );

    const auto result = cadence::validate(scenario);

    ASSERT_FALSE(result.valid());
    ASSERT_EQ(
        result.errors.front().code,
        cadence::ValidationCode::MissingHorizon
    );
}

TEST(ValidationTest, StaticLockErrorsAreReported) {
    cadence::Scenario scenario;
    scenario.name = "bad";
    scenario.horizon = 20;
    scenario.mutexes = {0};

    scenario.tasks.push_back(cadence::Task(
        1,
        "t",
        5,
        {
            cadence::Operation::compute(1),
            cadence::Operation::lock(0),
            cadence::Operation::lock(0)
        }
    ));

    const auto result = cadence::validate(scenario);

    ASSERT_FALSE(result.valid());

    bool saw_relock = false;
    bool saw_end_while_holding = false;

    for (const auto& error : result.errors) {
        saw_relock |=
            error.code == cadence::ValidationCode::RelockHeld;

        saw_end_while_holding |=
            error.code ==
            cadence::ValidationCode::EndWhileHolding;
    }

    EXPECT_TRUE(saw_relock);
    EXPECT_TRUE(saw_end_while_holding);
}

TEST(ValidationTest, UnknownMutexIsReported) {
    cadence::Scenario scenario;
    scenario.name = "bad";
    scenario.horizon = 20;
    scenario.mutexes = {0};

    scenario.tasks.push_back(cadence::Task(
        1,
        "t",
        5,
        {
            cadence::Operation::compute(1),
            cadence::Operation::lock(1),
            cadence::Operation::unlock(1)
        }
    ));

    const auto result = cadence::validate(scenario);

    ASSERT_FALSE(result.valid());

    bool saw_unknown = false;

    for (const auto& error : result.errors) {
        saw_unknown |=
            error.code ==
            cadence::ValidationCode::UnknownMutex;
    }

    EXPECT_TRUE(saw_unknown);
}

TEST(ValidationTest, DuplicateTaskIdIsReported) {
    cadence::Scenario scenario;
    scenario.name = "bad";
    scenario.horizon = 20;

    scenario.tasks.push_back(
        cadence::Task(
            1,
            "a",
            1,
            {cadence::Operation::compute(1)}
        )
    );

    scenario.tasks.push_back(
        cadence::Task(
            1,
            "b",
            2,
            {cadence::Operation::compute(1)}
        )
    );

    const auto result = cadence::validate(scenario);

    ASSERT_FALSE(result.valid());

    bool saw_duplicate = false;

    for (const auto& error : result.errors) {
        saw_duplicate |=
            error.code ==
            cadence::ValidationCode::DuplicateTaskId;
    }

    EXPECT_TRUE(saw_duplicate);
}

TEST(ValidationTest, MissingComputeIsReported) {
    cadence::Scenario scenario;
    scenario.name = "bad";
    scenario.horizon = 20;

    scenario.tasks.push_back(
        cadence::Task(
            1,
            "sleepy",
            1,
            {cadence::Operation::sleep(2)}
        )
    );

    const auto result = cadence::validate(scenario);

    ASSERT_FALSE(result.valid());

    bool saw_no_compute = false;

    for (const auto& error : result.errors) {
        saw_no_compute |=
            error.code ==
            cadence::ValidationCode::NoCompute;
    }

    EXPECT_TRUE(saw_no_compute);
}