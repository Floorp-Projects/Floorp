/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Queue.h"
#include "gtest/gtest.h"
#include <array>

using namespace mozilla;

namespace TestQueue {

struct Movable {
  Movable() : mDestructionCounter(nullptr) {}
  explicit Movable(uint32_t* aDestructionCounter)
      : mDestructionCounter(aDestructionCounter) {}

  ~Movable() {
    if (mDestructionCounter) {
      (*mDestructionCounter)++;
    }
  }

  Movable(Movable&& aOther) : mDestructionCounter(aOther.mDestructionCounter) {
    aOther.mDestructionCounter = nullptr;
  }

  uint32_t* mDestructionCounter;
};

template <size_t N, size_t ItemsPerPage>
void PushMovables(Queue<Movable, ItemsPerPage>& aQueue,
                  std::array<uint32_t, N>& aDestructionCounters) {
  auto oldDestructionCounters = aDestructionCounters;
  auto oldCount = aQueue.Count();
  for (uint32_t i = 0; i < N; ++i) {
    aQueue.Push(Movable(&aDestructionCounters[i]));
  }
  for (uint32_t i = 0; i < N; ++i) {
    EXPECT_EQ(aDestructionCounters[i], oldDestructionCounters[i]);
  }
  EXPECT_EQ(aQueue.Count(), oldCount + N);
  EXPECT_FALSE(aQueue.IsEmpty());
}

template <size_t N>
void ExpectCounts(const std::array<uint32_t, N>& aDestructionCounters,
                  uint32_t aExpected) {
  for (const auto& counters : aDestructionCounters) {
    EXPECT_EQ(counters, aExpected);
  }
}

TEST(Queue, Clear)
{
  std::array<uint32_t, 32> counts{0};

  Queue<Movable, 8> queue;
  PushMovables(queue, counts);
  queue.Clear();
  ExpectCounts(counts, 1);
  EXPECT_EQ(queue.Count(), 0u);
  EXPECT_TRUE(queue.IsEmpty());
}

TEST(Queue, Destroy)
{
  std::array<uint32_t, 32> counts{0};

  {
    Queue<Movable, 8> queue;
    PushMovables(queue, counts);
  }
  ExpectCounts(counts, 1u);
}

TEST(Queue, MoveConstruct)
{
  std::array<uint32_t, 32> counts{0};

  {
    Queue<Movable, 8> queue;
    PushMovables(queue, counts);

    Queue<Movable, 8> queue2(std::move(queue));
    EXPECT_EQ(queue2.Count(), 32u);
    EXPECT_FALSE(queue2.IsEmpty());
    // NOLINTNEXTLINE(bugprone-use-after-move, clang-analyzer-cplusplus.Move)
    EXPECT_EQ(queue.Count(), 0u);
    // NOLINTNEXTLINE(bugprone-use-after-move, clang-analyzer-cplusplus.Move)
    EXPECT_TRUE(queue.IsEmpty());
    ExpectCounts(counts, 0u);
  }
  ExpectCounts(counts, 1u);
}

TEST(Queue, MoveAssign)
{
  std::array<uint32_t, 32> counts{0};
  std::array<uint32_t, 32> counts2{0};

  {
    Queue<Movable, 8> queue;
    PushMovables(queue, counts);

    {
      Queue<Movable, 8> queue2;
      PushMovables(queue2, counts2);

      queue = std::move(queue2);
      ExpectCounts(counts, 1);
      ExpectCounts(counts2, 0);
      EXPECT_EQ(queue.Count(), 32u);
      EXPECT_FALSE(queue.IsEmpty());
      // NOLINTNEXTLINE(bugprone-use-after-move, clang-analyzer-cplusplus.Move)
      EXPECT_EQ(queue2.Count(), 0u);
      // NOLINTNEXTLINE(bugprone-use-after-move, clang-analyzer-cplusplus.Move)
      EXPECT_TRUE(queue2.IsEmpty());
    }
    ExpectCounts(counts, 1);
    ExpectCounts(counts2, 0);
    EXPECT_EQ(queue.Count(), 32u);
    EXPECT_FALSE(queue.IsEmpty());
  }
  ExpectCounts(counts, 1);
  ExpectCounts(counts2, 1);
}

TEST(Queue, PopOrder)
{
  std::array<uint32_t, 32> counts{0};
  Queue<Movable, 8> queue;
  PushMovables(queue, counts);

  for (auto& count : counts) {
    EXPECT_EQ(count, 0u);
    {
      Movable popped = queue.Pop();
      EXPECT_EQ(popped.mDestructionCounter, &count);
      EXPECT_EQ(count, 0u);
    }
    EXPECT_EQ(count, 1u);
  }
  EXPECT_TRUE(queue.IsEmpty());
  EXPECT_EQ(queue.Count(), 0u);
}

void DoPushPopSequence(Queue<uint32_t, 8>& aQueue, uint32_t& aInSerial,
                       uint32_t& aOutSerial, uint32_t aPush, uint32_t aPop) {
  auto initialCount = aQueue.Count();
  for (uint32_t i = 0; i < aPush; ++i) {
    aQueue.Push(aInSerial++);
  }
  EXPECT_EQ(aQueue.Count(), initialCount + aPush);
  for (uint32_t i = 0; i < aPop; ++i) {
    uint32_t popped = aQueue.Pop();
    EXPECT_EQ(popped, aOutSerial++);
  }
  EXPECT_EQ(aQueue.Count(), initialCount + aPush - aPop);
}

void PushPopPushPop(uint32_t aPush1, uint32_t aPop1, uint32_t aPush2,
                    uint32_t aPop2) {
  Queue<uint32_t, 8> queue;
  uint32_t inSerial = 0;
  uint32_t outSerial = 0;
  DoPushPopSequence(queue, inSerial, outSerial, aPush1, aPop1);
  DoPushPopSequence(queue, inSerial, outSerial, aPush2, aPop2);
}

TEST(Queue, PushPopSequence)
{
  for (uint32_t push1 = 0; push1 < 16; ++push1) {
    for (uint32_t pop1 = 0; pop1 < push1; ++pop1) {
      for (uint32_t push2 = 0; push2 < 16; ++push2) {
        for (uint32_t pop2 = 0; pop2 < push1 - pop1 + push2; ++pop2) {
          PushPopPushPop(push1, pop1, push2, pop2);
        }
      }
    }
  }
}

}  // namespace TestQueue
