/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Brian Birtles.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
