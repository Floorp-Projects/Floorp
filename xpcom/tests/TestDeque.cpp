/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"
#include "nsDeque.h"
#include "nsCRT.h"
#include <stdio.h>

/**************************************************************
  Now define the token deallocator class...
 **************************************************************/
class _TestDeque {
public:
  int Test();
private:
  int OriginalTest();
  int OriginalFlaw();
  int AssignFlaw();
  int TestRemove();
  int TestPushFront();
  int TestEmpty();
};

class _Dealloc: public nsDequeFunctor {
  virtual void* operator()(void* aObject) {
    return 0;
  }
};

#define TEST(aCondition, aMsg) \
  if (!(aCondition)) { fail("TestDeque: "#aMsg); return 1; }


/**
 * conduct automated self test for this class
 *
 * @param
 * @return
 */
int _TestDeque::Test() {
  /* the old deque should have failed a bunch of these tests */
  int results=0;
  results+=OriginalTest();
  results+=OriginalFlaw();
  results+=AssignFlaw();
  results+=TestRemove();
  results+=TestPushFront();
  results+=TestEmpty();
  return results;
}

int _TestDeque::OriginalTest() {
  const int size = 200;
  int ints[size];
  int i=0;
  int temp;
  nsDeque theDeque(new _Dealloc); //construct a simple one...
 
  // ints = [0...199]
  for (i=0;i<size;i++) { //initialize'em
    ints[i]=i;
  }
  // queue = [0...69]
  for (i=0;i<70;i++) {
    theDeque.Push(&ints[i]);
    temp=*(int*)theDeque.Peek();
    TEST(temp == i, "Verify end after push #1");
    TEST(theDeque.GetSize() == i + 1, "Verify size after push #1");
  }
  TEST(theDeque.GetSize() == 70, "Verify overall size after pushes #1");
  // queue = [0...14]
  for (i=1;i<=55;i++) {
    temp=*(int*)theDeque.Pop();
    TEST(temp == 70-i, "Verify end after pop # 1");
    TEST(theDeque.GetSize() == 70 - i, "Verify size after pop # 1");
  }
  TEST(theDeque.GetSize() == 15, "Verify overall size after pops");

  // queue = [0...14,0...54]
  for (i=0;i<55;i++) {
    theDeque.Push(&ints[i]);
    temp=*(int*)theDeque.Peek();
    TEST(temp == i, "Verify end after push #2");
    TEST(theDeque.GetSize() == i + 15 + 1, "Verify size after push # 2");
  }
  TEST(theDeque.GetSize() == 70, "Verify size after end of all pushes #2");

  // queue = [0...14,0...19]
  for (i=1;i<=35;i++) {
    temp=*(int*)theDeque.Pop();
    TEST(temp == 55-i, "Verify end after pop # 2");
    TEST(theDeque.GetSize() == 70 - i, "Verify size after pop #2");
  }
  TEST(theDeque.GetSize() == 35, "Verify overall size after end of all pops #2");

  // queue = [0...14,0...19,0...34]
  for (i=0;i<35;i++) {
    theDeque.Push(&ints[i]);
    temp = *(int*)theDeque.Peek();
    TEST(temp == i, "Verify end after push # 3");
    TEST(theDeque.GetSize() == 35 + 1 + i, "Verify size after push #3");
  }

  // queue = [0...14,0...19]
  for (i=0;i<35;i++) {
    temp=*(int*)theDeque.Pop();
    TEST(temp == 34 - i, "Verify end after pop # 3");
  }

  // queue = [0...14]
  for (i=0;i<20;i++) {
    temp=*(int*)theDeque.Pop();
    TEST(temp == 19 - i, "Verify end after pop # 4");
  }

  // queue = []
  for (i=0;i<15;i++) {
    temp=*(int*)theDeque.Pop();
    TEST(temp == 14 - i, "Verify end after pop # 5");
  }

  TEST(theDeque.GetSize() == 0, "Deque should finish empty.");

  return 0;
}

int _TestDeque::OriginalFlaw() {
  int ints[200];
  int i=0;
  int temp;
  nsDeque d(new _Dealloc);
  /**
   * Test 1. Origin near end, semi full, call Peek().
   * you start, mCapacity is 8
   */
  printf("fill array\n");
  for (i=0; i<30; i++)
    ints[i]=i;

  for (i=0; i<6; i++) {
    d.Push(&ints[i]);
    temp = *(int*)d.Peek();
    TEST(temp == i, "OriginalFlaw push #1");
  }
  TEST(d.GetSize() == 6, "OriginalFlaw size check #1");

  for (i=0; i<4; i++) {
    temp=*(int*)d.PopFront();
    TEST(temp == i, "PopFront test");
  }
  // d = [4,5]
  TEST(d.GetSize() == 2, "OriginalFlaw size check #2");

  for (i=0; i<4; i++) {
    d.Push(&ints[6 + i]);
  }
  // d = [4...9]

  for (i=4; i<=9; i++) {
    temp=*(int*)d.PopFront();
    TEST(temp == i, "OriginalFlaw empty check");
  }

  return 0;
}

int _TestDeque::AssignFlaw() {
  nsDeque src(new _Dealloc),dest(new _Dealloc);
  return 0;
}

static bool VerifyContents(const nsDeque& aDeque, const int* aContents, int aLength) {
  for (int i=0; i<aLength; ++i) {
    if (*(int*)aDeque.ObjectAt(i) != aContents[i]) {
      return false;
    }
  }
  return true;
}

int _TestDeque::TestRemove() {
  nsDeque d;
  const int count = 10;
  int ints[count];
  for (int i=0; i<count; i++) {
    ints[i] = i;
  }

  for (int i=0; i<6; i++) {
    d.Push(&ints[i]);
  }
  // d = [0...5]
  d.PopFront();
  d.PopFront();

  // d = [2,5]
  for (int i=2; i<=5; i++) {
    int t = *(int*)d.ObjectAt(i-2);
    TEST(t == i, "Verify ObjectAt()");
  }

  d.RemoveObjectAt(1);
  // d == [2,4,5]
  static const int t1[] = {2,4,5};
  TEST(VerifyContents(d, t1, 3), "verify contents t1");

  d.PushFront(&ints[1]);
  d.PushFront(&ints[0]);
  d.PushFront(&ints[7]);
  d.PushFront(&ints[6]);
  //  d == [6,7,0,1,2,4,5] // (0==mOrigin)
  static const int t2[] = {6,7,0,1,2,4,5};
  TEST(VerifyContents(d, t2, 7), "verify contents t2");

  d.RemoveObjectAt(1);
  //  d == [6,0,1,2,4,5] // (1==mOrigin)
  static const int t3[] = {6,0,1,2,4,5};
  TEST(VerifyContents(d, t3, 6), "verify contents t3");

  d.RemoveObjectAt(5);
  //  d == [6,0,1,2,4] // (1==mOrigin)
  static const int t4[] = {6,0,1,2,4};
  TEST(VerifyContents(d, t4, 5), "verify contents t4");

  d.RemoveObjectAt(0);
  //  d == [0,1,2,4] // (2==mOrigin)
  static const int t5[] = {0,1,2,4};
  TEST(VerifyContents(d, t5, 4), "verify contents t5");


  return 0;
}

int _TestDeque::TestPushFront() {
  // PushFront has some interesting corner cases, primarily we're interested in whether:
  // - wrapping around works properly
  // - growing works properly

  nsDeque d;

  const int kPoolSize = 10;
  const int kMaxSizeBeforeGrowth = 8;

  int pool[kPoolSize];
  for (int i = 0; i < kPoolSize; i++) {
    pool[i] = i;
  }

  for (int i = 0; i < kMaxSizeBeforeGrowth; i++) {
    d.PushFront(pool + i);
  }

  TEST(d.GetSize() == kMaxSizeBeforeGrowth, "verify size");

  static const int t1[] = {7,6,5,4,3,2,1,0};
  TEST(VerifyContents(d, t1, kMaxSizeBeforeGrowth), "verify pushfront 1");

  // Now push one more so it grows
  d.PushFront(pool + kMaxSizeBeforeGrowth);
  TEST(d.GetSize() == kMaxSizeBeforeGrowth + 1, "verify size");

  static const int t2[] = {8,7,6,5,4,3,2,1,0};
  TEST(VerifyContents(d, t2, kMaxSizeBeforeGrowth + 1), "verify pushfront 2");

  // And one more so that it wraps again
  d.PushFront(pool + kMaxSizeBeforeGrowth + 1);
  TEST(d.GetSize() == kMaxSizeBeforeGrowth + 2, "verify size");

  static const int t3[] = {9,8,7,6,5,4,3,2,1,0};
  TEST(VerifyContents(d, t3, kMaxSizeBeforeGrowth + 2), "verify pushfront 3");

  return 0;
}

int _TestDeque::TestEmpty() {
  // Make sure nsDeque gives sane results if it's empty.
  nsDeque d;

  TEST(d.GetSize() == 0, "Size should be 0");
  TEST(d.Pop() == nullptr, "Invalid operation should return nullptr");
  TEST(d.PopFront() == nullptr, "Invalid operation should return nullptr");
  TEST(d.Peek() == nullptr, "Invalid operation should return nullptr");
  TEST(d.PeekFront() == nullptr, "Invalid operation should return nullptr");
  TEST(d.ObjectAt(0) == nullptr, "Invalid operation should return nullptr");
  TEST(d.Last() == nullptr, "Invalid operation should return nullptr");

  // Fill it up and drain it.
  for (size_t i = 0; i < 8; i++) {
    d.Push((void*)0xAA);
  }

  for (size_t i = 0; i < 8; i++) {
    (void)d.Pop();
  }

  // Now check it again.
  TEST(d.GetSize() == 0, "Size should be 0");
  TEST(d.Pop() == nullptr, "Invalid operation should return nullptr");
  TEST(d.PopFront() == nullptr, "Invalid operation should return nullptr");
  TEST(d.Peek() == nullptr, "Invalid operation should return nullptr");
  TEST(d.PeekFront() == nullptr, "Invalid operation should return nullptr");
  TEST(d.ObjectAt(0) == nullptr, "Invalid operation should return nullptr");
  TEST(d.Last() == nullptr, "Invalid operation should return nullptr");

  return 0;
}

int main (void) {
  ScopedXPCOM xpcom("TestDeque");
  NS_ENSURE_FALSE(xpcom.failed(), 1);

  _TestDeque test;
  int result = test.Test();
  TEST(result == 0, "All tests pass");
  return 0;
}
