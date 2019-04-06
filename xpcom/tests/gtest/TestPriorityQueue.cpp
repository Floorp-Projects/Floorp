/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTPriorityQueue.h"
#include <stdio.h>
#include <stdlib.h>
#include "gtest/gtest.h"

template <class T, class Compare>
void CheckPopSequence(const nsTPriorityQueue<T, Compare>& aQueue,
                      const T* aExpectedSequence,
                      const uint32_t aSequenceLength) {
  nsTPriorityQueue<T, Compare> copy(aQueue);

  for (uint32_t i = 0; i < aSequenceLength; i++) {
    EXPECT_FALSE(copy.IsEmpty());

    T pop = copy.Pop();
    EXPECT_EQ(pop, aExpectedSequence[i]);
  }

  EXPECT_TRUE(copy.IsEmpty());
}

template <class A>
class MaxCompare {
 public:
  bool LessThan(const A& a, const A& b) { return a > b; }
};

TEST(PriorityQueue, Main)
{
  nsTPriorityQueue<int> queue;

  EXPECT_TRUE(queue.IsEmpty());

  queue.Push(8);
  queue.Push(6);
  queue.Push(4);
  queue.Push(2);
  queue.Push(10);
  queue.Push(6);
  EXPECT_EQ(queue.Top(), 2);
  EXPECT_EQ(queue.Length(), 6u);
  EXPECT_FALSE(queue.IsEmpty());
  int expected[] = {2, 4, 6, 6, 8, 10};
  CheckPopSequence(queue, expected, sizeof(expected) / sizeof(expected[0]));

  // copy ctor is tested by using CheckPopSequence, but check default assignment
  // operator
  nsTPriorityQueue<int> queue2;
  queue2 = queue;
  CheckPopSequence(queue2, expected, sizeof(expected) / sizeof(expected[0]));

  queue.Clear();
  EXPECT_TRUE(queue.IsEmpty());

  // try same sequence with a max heap
  nsTPriorityQueue<int, MaxCompare<int> > max_queue;
  max_queue.Push(8);
  max_queue.Push(6);
  max_queue.Push(4);
  max_queue.Push(2);
  max_queue.Push(10);
  max_queue.Push(6);
  EXPECT_EQ(max_queue.Top(), 10);
  int expected_max[] = {10, 8, 6, 6, 4, 2};
  CheckPopSequence(max_queue, expected_max,
                   sizeof(expected_max) / sizeof(expected_max[0]));
}
