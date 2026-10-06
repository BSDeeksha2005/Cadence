#include <gtest/gtest.h>

#include <stdexcept>

#include "cadence/ready_queue.hpp"

using cadence::ReadyQueue;

TEST(ReadyQueueTest, StartsEmpty) {
    ReadyQueue q;
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);
}

TEST(ReadyQueueTest, PopsHighestPriorityFirst) {
    ReadyQueue q;
    q.push(1, 2);
    q.push(2, 9);
    q.push(3, 5);
    EXPECT_EQ(q.pop(), 2);
    EXPECT_EQ(q.pop(), 3);
    EXPECT_EQ(q.pop(), 1);
    EXPECT_TRUE(q.empty());
}

TEST(ReadyQueueTest, EqualPriorityIsFifo) {
    ReadyQueue q;
    q.push(10, 5);
    q.push(11, 5);
    q.push(12, 5);
    EXPECT_EQ(q.pop(), 10);
    EXPECT_EQ(q.pop(), 11);
    EXPECT_EQ(q.pop(), 12);
}

TEST(ReadyQueueTest, FifoIsByArrivalNotById) {
    ReadyQueue q;
    q.push(9, 5);
    q.push(1, 5);
    EXPECT_EQ(q.pop(), 9);
    EXPECT_EQ(q.pop(), 1);
}

TEST(ReadyQueueTest, RePushGoesToBackOfItsLevel) {
    ReadyQueue q;
    q.push(1, 5);
    q.push(2, 5);
    EXPECT_EQ(q.pop(), 1);
    q.push(1, 5);
    EXPECT_EQ(q.pop(), 2);
    EXPECT_EQ(q.pop(), 1);
}

TEST(ReadyQueueTest, PeekDoesNotRemove) {
    ReadyQueue q;
    q.push(1, 3);
    q.push(2, 7);
    EXPECT_EQ(q.peek(), 2);
    EXPECT_EQ(q.size(), 2u);
}

TEST(ReadyQueueTest, RemoveAndContains) {
    ReadyQueue q;
    q.push(1, 3);
    q.push(2, 7);
    EXPECT_TRUE(q.contains(1));
    EXPECT_TRUE(q.remove(1));
    EXPECT_FALSE(q.contains(1));
    EXPECT_FALSE(q.remove(1));
    EXPECT_EQ(q.size(), 1u);
}

TEST(ReadyQueueTest, DuplicatePushThrows) {
    ReadyQueue q;
    q.push(1, 3);
    EXPECT_THROW(q.push(1, 3), std::logic_error);
}

TEST(ReadyQueueTest, EmptyPeekAndPopThrow) {
    ReadyQueue q;
    EXPECT_THROW(q.peek(), std::logic_error);
    EXPECT_THROW(q.pop(), std::logic_error);
}

TEST(ReadyQueueTest, PeekPriorityReturnsBest) {
    ReadyQueue q;
    q.push(1, 3);
    q.push(2, 7);
    EXPECT_EQ(q.peek_priority(), 7);
    EXPECT_EQ(q.size(), 2u);
    ReadyQueue empty;
    EXPECT_THROW(empty.peek_priority(), std::logic_error);
}