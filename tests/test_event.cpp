#include <cstddef>
#include <gtest/gtest.h>

#include "cadence/engine.hpp"
#include "cadence/event.hpp"
#include "cadence/scenario.hpp"

TEST(EventTest, SequenceIsStrictlyIncreasing) {
    const auto result =
        cadence::simulate_scenario(
            cadence::make_s1(),
            cadence::Protocol::PIP
        );

    ASSERT_FALSE(result.events.empty());

    for (std::size_t i = 1; i < result.events.size(); ++i) {
        EXPECT_LT(result.events[i - 1].seq, result.events[i].seq);
        EXPECT_LE(result.events[i - 1].time, result.events[i].time);
    }
}

TEST(EventTest, S1ContainsPriorityChangeAndDeadlineMissOnlyUnderNone) {
    const auto none =
        cadence::simulate_scenario(
            cadence::make_s1(),
            cadence::Protocol::NONE
        );

    const auto pip =
        cadence::simulate_scenario(
            cadence::make_s1(),
            cadence::Protocol::PIP
        );

    bool none_miss = false;
    bool pip_miss = false;
    bool pip_priority_change = false;

    for (const auto& event : none.events) {
        none_miss |= event.kind == cadence::EventKind::DeadlineMiss;
    }

    for (const auto& event : pip.events) {
        pip_miss |= event.kind == cadence::EventKind::DeadlineMiss;
        pip_priority_change |=
            event.kind == cadence::EventKind::PriorityChange;
    }

    EXPECT_TRUE(none_miss);
    EXPECT_FALSE(pip_miss);
    EXPECT_TRUE(pip_priority_change);
}

TEST(EventTest, InvariantsHoldAtEndOfS1) {
    cadence::Engine engine(
        30,
        cadence::Protocol::PIP
    );

    const auto scenario = cadence::make_s1();

    for (const auto& task : scenario.tasks) {
        engine.add_task(task);
    }

    engine.run_to_horizon();

    EXPECT_NO_THROW(
        engine.check_invariants()
    );
}