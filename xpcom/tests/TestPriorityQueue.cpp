/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTPriorityQueue.h"
#include <stdio.h>
#include <stdlib.h>

template<class T, class Compare>
void
CheckPopSequence(const nsTPriorityQueue<T, Compare>& aQueue,
                 const T* aExpectedSequence, const PRUint32 aSequenceLength)
{
  nsTPriorityQueue<T, Compare> copy(aQueue);

  for (PRUint32 i = 0; i < aSequenceLength; i++) {
    if (copy.IsEmpty()) {
      printf("Number of elements in the queue is too short by %d.\n",
          aSequenceLength - i);
      exit(-1);
    }

    T pop = copy.Pop();
    if (pop != aExpectedSequence[i]) {
      printf("Unexpected value in pop sequence at position %d\n", i);
      printf("  Sequence:");
      for (size_t j = 0; j < aSequenceLength; j++) {
        printf(" %d", aExpectedSequence[j]);
        if (j == i) {
          printf("**");
        }
      }
      printf("\n            ** Got %d instead\n", pop);
      exit(-1);
    }
  }

  if (!copy.IsEmpty()) {
    printf("Number of elements in the queue is too long by %d.\n",
        copy.Length());
    exit(-1);
  }
}

template<class A>
class MaxCompare {
public:
  bool LessThan(const A& a, const A& b) {
    return a > b;
  }
};

int main()
{
  nsTPriorityQueue<int> queue;

  NS_ABORT_IF_FALSE(queue.IsEmpty(), "Queue not initially empty");

  queue.Push(8);
  queue.Push(6);
  queue.Push(4);
  queue.Push(2);
  queue.Push(10);
  queue.Push(6);
  NS_ABORT_IF_FALSE(queue.Top() == 2, "Unexpected queue top");
  NS_ABORT_IF_FALSE(queue.Length() == 6, "Unexpected queue length");
  NS_ABORT_IF_FALSE(!queue.IsEmpty(), "Queue empty when populated");
  int expected[] = { 2, 4, 6, 6, 8, 10 };
  CheckPopSequence(queue, expected, sizeof(expected) / sizeof(expected[0]));

  // copy ctor is tested by using CheckPopSequence, but check default assignment
  // operator
  nsTPriorityQueue<int> queue2;
  queue2 = queue;
  CheckPopSequence(queue2, expected, sizeof(expected) / sizeof(expected[0]));

  queue.Clear();
  NS_ABORT_IF_FALSE(queue.IsEmpty(), "Queue not emptied by Clear");

  // try same sequence with a max heap
  nsTPriorityQueue<int, MaxCompare<int> > max_queue;
  max_queue.Push(8);
  max_queue.Push(6);
  max_queue.Push(4);
  max_queue.Push(2);
  max_queue.Push(10);
  max_queue.Push(6);
  NS_ABORT_IF_FALSE(max_queue.Top() == 10, "Unexpected queue top for max heap");
  int expected_max[] = { 10, 8, 6, 6, 4, 2 };
  CheckPopSequence(max_queue, expected_max,
      sizeof(expected_max) / sizeof(expected_max[0]));

  return 0;
}
