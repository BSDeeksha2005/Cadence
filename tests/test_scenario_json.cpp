#include <gtest/gtest.h>

#include <string>

#include "cadence/adapter/scenario_json.hpp"
#include "cadence/metrics.hpp"

TEST(ScenarioJsonTest, ParsesScenarioAndNamedMutexOperations) {
    const cadence::Scenario scenario = cadence::parse_scenario_json(R"json(
      {"name":"tiny","horizon":8,"mutexes":["R"],"tasks":[
        {"id":7,"name":"worker","priority":4,"release":0,"deadline":6,
         "program":[["LOCK","R"],["COMPUTE",2],["UNLOCK","R"]]}
      ]}
    )json");

    ASSERT_EQ(scenario.name, "tiny");
    ASSERT_EQ(scenario.horizon.value(), 8);
    ASSERT_EQ(scenario.mutexes.size(), 1u);
    ASSERT_EQ(scenario.tasks.size(), 1u);
    EXPECT_EQ(scenario.tasks[0].id(), 7);
    EXPECT_EQ(scenario.tasks[0].absolute_deadline().value(), 6);
    ASSERT_EQ(scenario.tasks[0].program_size(), 3u);
    EXPECT_EQ(scenario.tasks[0].program()[0], cadence::Operation::lock(0));
    EXPECT_EQ(scenario.tasks[0].program()[1], cadence::Operation::compute(2));
}

TEST(ScenarioJsonTest, ParsesAppendixS1Shape) {
    const std::string json = R"json({"name":"S1","horizon":30,"mutexes":["R"],"tasks":[
      {"id":0,"name":"L","priority":1,"release":0,"program":[["COMPUTE",1],["LOCK","R"],["COMPUTE",3],["UNLOCK","R"],["COMPUTE",1]]},
      {"id":1,"name":"M","priority":2,"release":2,"deadline":10,"program":[["COMPUTE",4]]},
      {"id":2,"name":"H","priority":3,"release":3,"deadline":6,"program":[["COMPUTE",1],["LOCK","R"],["COMPUTE",1],["UNLOCK","R"]]}
    ]})json";
    const cadence::Scenario scenario = cadence::parse_scenario_json(json);

    for (cadence::Protocol protocol : {cadence::Protocol::NONE, cadence::Protocol::PIP}) {
        const cadence::SimulationResult result = cadence::simulate_scenario(scenario, protocol);
        ASSERT_EQ(result.ticks.size(), 11u);
    }
}

TEST(ScenarioJsonTest, S2ShowsChainedInheritanceAndDeadlineEffect) {
    // Hand-derived from scenarios/S2.json and the boundary rules in SEMANTICS.md.
    const cadence::Scenario scenario = cadence::parse_scenario_json(R"json(
      {"name":"S2","horizon":30,"mutexes":["A","B"],"tasks":[
        {"id":0,"name":"L","priority":1,"release":0,"program":[["LOCK","A"],["COMPUTE",4],["UNLOCK","A"],["COMPUTE",1]]},
        {"id":1,"name":"M","priority":2,"release":1,"program":[["LOCK","B"],["LOCK","A"],["COMPUTE",1],["UNLOCK","A"],["UNLOCK","B"],["COMPUTE",1]]},
        {"id":2,"name":"H","priority":3,"release":2,"deadline":5,"program":[["LOCK","B"],["COMPUTE",1],["UNLOCK","B"],["COMPUTE",1]]},
        {"id":3,"name":"X","priority":2,"release":2,"program":[["COMPUTE",2]]}
      ]}
    )json");

    const auto none = cadence::simulate_scenario(scenario, cadence::Protocol::NONE);
    const auto pip = cadence::simulate_scenario(scenario, cadence::Protocol::PIP);
    const std::vector<cadence::TaskId> none_timeline = {
        0, 0, 3, 3, 0, 0, 1, 2, 2, 1, 0
    };
    const std::vector<cadence::TaskId> pip_timeline = {
        0, 0, 0, 0, 1, 2, 2, 1, 3, 3, 0
    };
    ASSERT_EQ(none.ticks.size(), none_timeline.size());
    ASSERT_EQ(pip.ticks.size(), pip_timeline.size());
    for (std::size_t i = 0; i < none_timeline.size(); ++i) {
        EXPECT_EQ(none.ticks[i].running, none_timeline[i]);
        EXPECT_EQ(pip.ticks[i].running, pip_timeline[i]);
    }

    const auto none_metrics = cadence::compute_metrics(scenario, none);
    const auto pip_metrics = cadence::compute_metrics(scenario, pip);
    EXPECT_EQ(none_metrics.tasks[2].completion.value(), 9);
    EXPECT_EQ(none_metrics.tasks[2].response.value(), 7);
    EXPECT_TRUE(none_metrics.tasks[2].deadline_missed);
    EXPECT_EQ(none_metrics.tasks[2].lateness.value(), 2);
    EXPECT_EQ(none_metrics.tasks[2].blocked_ticks, 5);
    EXPECT_EQ(none_metrics.tasks[2].inversion_ticks, 2);
    EXPECT_EQ(none_metrics.tasks[2].legitimate_blocking, 3);

    EXPECT_EQ(pip_metrics.tasks[2].completion.value(), 7);
    EXPECT_EQ(pip_metrics.tasks[2].response.value(), 5);
    EXPECT_FALSE(pip_metrics.tasks[2].deadline_missed);
    EXPECT_EQ(pip_metrics.tasks[2].blocked_ticks, 3);
    EXPECT_EQ(pip_metrics.tasks[2].inversion_ticks, 0);
    EXPECT_EQ(pip_metrics.tasks[2].legitimate_blocking, 3);
}

TEST(ScenarioJsonTest, RejectsFractionalSimulationValues) {
    EXPECT_THROW(cadence::parse_scenario_json(
        R"json({"horizon":2.5,"mutexes":[],"tasks":[]})json"),
        std::invalid_argument);
}

TEST(ScenarioJsonTest, DecodesUnicodeEscapesInNames) {
    const cadence::Scenario scenario = cadence::parse_scenario_json(
        R"json({"name":"caf\u00e9","horizon":2,"mutexes":[],"tasks":[
          {"id":1,"name":"worker \ud83d\ude80","priority":1,"release":0,
           "program":[["COMPUTE",1]]}
        ]})json");
    EXPECT_EQ(scenario.name, "café");
    EXPECT_EQ(scenario.tasks[0].name(), "worker 🚀");
}

TEST(ScenarioJsonTest, RejectsInvalidScenarioAfterParsing) {
    EXPECT_THROW(cadence::parse_scenario_json(
        R"json({"horizon":5,"mutexes":[],"tasks":[{"id":0,"name":"x","priority":1,"release":0,"program":[["SLEEP",2]]}]})json"),
        std::invalid_argument);
}
