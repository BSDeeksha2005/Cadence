#include <gtest/gtest.h>

#include <stdexcept>

#include "cadence/operation.hpp"

using cadence::OpType;
using cadence::Operation;

TEST(OperationTest, ComputeStoresTicks) {
    Operation op = Operation::compute(5);
    EXPECT_EQ(op.type(), OpType::Compute);
    EXPECT_EQ(op.ticks(), 5);
    EXPECT_EQ(op.to_string(), "COMPUTE(5)");
}

TEST(OperationTest, SleepStoresTicks) {
    Operation op = Operation::sleep(3);
    EXPECT_EQ(op.type(), OpType::Sleep);
    EXPECT_EQ(op.ticks(), 3);
    EXPECT_EQ(op.to_string(), "SLEEP(3)");
}

TEST(OperationTest, LockAndUnlockStoreMutex) {
    Operation l = Operation::lock(2);
    Operation u = Operation::unlock(2);
    EXPECT_EQ(l.type(), OpType::Lock);
    EXPECT_EQ(u.type(), OpType::Unlock);
    EXPECT_EQ(l.mutex(), 2);
    EXPECT_EQ(u.mutex(), 2);
    EXPECT_EQ(l.to_string(), "LOCK(2)");
    EXPECT_EQ(u.to_string(), "UNLOCK(2)");
}

TEST(OperationTest, ZeroOrNegativeTicksRejected) {
    EXPECT_THROW(Operation::compute(0), std::invalid_argument);
    EXPECT_THROW(Operation::compute(-1), std::invalid_argument);
    EXPECT_THROW(Operation::sleep(0), std::invalid_argument);
    EXPECT_THROW(Operation::sleep(-4), std::invalid_argument);
}

TEST(OperationTest, WrongAccessorThrows) {
    EXPECT_THROW(Operation::lock(1).ticks(), std::logic_error);
    EXPECT_THROW(Operation::compute(1).mutex(), std::logic_error);
}

TEST(OperationTest, Equality) {
    EXPECT_EQ(Operation::compute(2), Operation::compute(2));
    EXPECT_NE(Operation::compute(2), Operation::compute(3));
    EXPECT_NE(Operation::compute(2), Operation::sleep(2));
    EXPECT_NE(Operation::lock(1), Operation::unlock(1));
}