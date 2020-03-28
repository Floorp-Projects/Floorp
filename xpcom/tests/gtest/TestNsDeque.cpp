/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsDeque.h"
#include "nsCRT.h"
#include "mozilla/TypeTraits.h"
#include <stdio.h>
#include <functional>
#include <type_traits>
#include <utility>

/**************************************************************
  Now define the token deallocator class...
 **************************************************************/
namespace TestNsDeque {

class _Dealloc : public nsDequeFunctor {
  virtual void operator()(void* aObject) {}
};

static bool VerifyContents(const nsDeque& aDeque, const int* aContents,
                           size_t aLength) {
  for (size_t i = 0; i < aLength; ++i) {
    if (*(int*)aDeque.ObjectAt(i) != aContents[i]) {
      return false;
    }
  }
  return true;
}

class Deallocator : public nsDequeFunctor {
  virtual void operator()(void* aObject) {
    if (aObject) {
      // Set value to -1, to use in test function.
      *((int*)aObject) = -1;
    }
  }
};

class ForEachAdder : public nsDequeFunctor {
  virtual void operator()(void* aObject) {
    if (aObject) {
      sum += *(int*)aObject;
    }
  }

 private:
  int sum = 0;

 public:
  int GetSum() { return sum; }
};
}  // namespace TestNsDeque

using namespace TestNsDeque;

TEST(NsDeque, OriginalTest)
{
  const size_t size = 200;
  int ints[size];
  size_t i = 0;
  int temp;
  nsDeque theDeque(new _Dealloc);  // construct a simple one...

  // ints = [0...199]
  for (i = 0; i < size; i++) {  // initialize'em
    ints[i] = static_cast<int>(i);
  }
  // queue = [0...69]
  for (i = 0; i < 70; i++) {
    theDeque.Push(&ints[i]);
    temp = *(int*)theDeque.Peek();
    EXPECT_EQ(static_cast<int>(i), temp) << "Verify end after push #1";
    EXPECT_EQ(i + 1, theDeque.GetSize()) << "Verify size after push #1";
  }

  EXPECT_EQ(70u, theDeque.GetSize()) << "Verify overall size after pushes #1";

  // queue = [0...14]
  for (i = 1; i <= 55; i++) {
    temp = *(int*)theDeque.Pop();
    EXPECT_EQ(70 - static_cast<int>(i), temp) << "Verify end after pop # 1";
    EXPECT_EQ(70u - i, theDeque.GetSize()) << "Verify size after pop # 1";
  }
  EXPECT_EQ(15u, theDeque.GetSize()) << "Verify overall size after pops";

  // queue = [0...14,0...54]
  for (i = 0; i < 55; i++) {
    theDeque.Push(&ints[i]);
    temp = *(int*)theDeque.Peek();
    EXPECT_EQ(static_cast<int>(i), temp) << "Verify end after push #2";
    EXPECT_EQ(i + 15u + 1, theDeque.GetSize()) << "Verify size after push # 2";
  }
  EXPECT_EQ(70u, theDeque.GetSize())
      << "Verify size after end of all pushes #2";

  // queue = [0...14,0...19]
  for (i = 1; i <= 35; i++) {
    temp = *(int*)theDeque.Pop();
    EXPECT_EQ(55 - static_cast<int>(i), temp) << "Verify end after pop # 2";
    EXPECT_EQ(70u - i, theDeque.GetSize()) << "Verify size after pop #2";
  }
  EXPECT_EQ(35u, theDeque.GetSize())
      << "Verify overall size after end of all pops #2";

  // queue = [0...14,0...19,0...34]
  for (i = 0; i < 35; i++) {
    theDeque.Push(&ints[i]);
    temp = *(int*)theDeque.Peek();
    EXPECT_EQ(static_cast<int>(i), temp) << "Verify end after push # 3";
    EXPECT_EQ(35u + 1u + i, theDeque.GetSize()) << "Verify size after push #3";
  }

  // queue = [0...14,0...19]
  for (i = 0; i < 35; i++) {
    temp = *(int*)theDeque.Pop();
    EXPECT_EQ(34 - static_cast<int>(i), temp) << "Verify end after pop # 3";
  }

  // queue = [0...14]
  for (i = 0; i < 20; i++) {
    temp = *(int*)theDeque.Pop();
    EXPECT_EQ(19 - static_cast<int>(i), temp) << "Verify end after pop # 4";
  }

  // queue = []
  for (i = 0; i < 15; i++) {
    temp = *(int*)theDeque.Pop();
    EXPECT_EQ(14 - static_cast<int>(i), temp) << "Verify end after pop # 5";
  }

  EXPECT_EQ(0u, theDeque.GetSize()) << "Deque should finish empty.";
}

TEST(NsDeque, OriginalFlaw)
{
  int ints[200];
  int i = 0;
  int temp;
  nsDeque d(new _Dealloc);
  /**
   * Test 1. Origin near end, semi full, call Peek().
   * you start, mCapacity is 8
   */
  for (i = 0; i < 30; i++) ints[i] = i;

  for (i = 0; i < 6; i++) {
    d.Push(&ints[i]);
    temp = *(int*)d.Peek();
    EXPECT_EQ(i, temp) << "OriginalFlaw push #1";
  }
  EXPECT_EQ(6u, d.GetSize()) << "OriginalFlaw size check #1";

  for (i = 0; i < 4; i++) {
    temp = *(int*)d.PopFront();
    EXPECT_EQ(i, temp) << "PopFront test";
  }
  // d = [4,5]
  EXPECT_EQ(2u, d.GetSize()) << "OriginalFlaw size check #2";

  for (i = 0; i < 4; i++) {
    d.Push(&ints[6 + i]);
  }

  // d = [4...9]
  for (i = 4; i <= 9; i++) {
    temp = *(int*)d.PopFront();
    EXPECT_EQ(i, temp) << "OriginalFlaw empty check";
  }
}

TEST(NsDeque, TestObjectAt)
{
  nsDeque d;
  const int count = 10;
  int ints[count];
  for (int i = 0; i < count; i++) {
    ints[i] = i;
  }

  for (int i = 0; i < 6; i++) {
    d.Push(&ints[i]);
  }
  // d = [0...5]
  d.PopFront();
  d.PopFront();

  // d = [2..5]
  for (size_t i = 2; i <= 5; i++) {
    int t = *(int*)d.ObjectAt(i - 2);
    EXPECT_EQ(static_cast<int>(i), t) << "Verify ObjectAt()";
  }
}

TEST(NsDeque, TestPushFront)
{
  // PushFront has some interesting corner cases, primarily we're interested in
  // whether:
  // - wrapping around works properly
  // - growing works properly

  nsDeque d;

  const int kPoolSize = 10;
  const size_t kMaxSizeBeforeGrowth = 8;

  int pool[kPoolSize];
  for (int i = 0; i < kPoolSize; i++) {
    pool[i] = i;
  }

  for (size_t i = 0; i < kMaxSizeBeforeGrowth; i++) {
    d.PushFront(pool + i);
  }

  EXPECT_EQ(kMaxSizeBeforeGrowth, d.GetSize()) << "verify size";

  static const int t1[] = {7, 6, 5, 4, 3, 2, 1, 0};
  EXPECT_TRUE(VerifyContents(d, t1, kMaxSizeBeforeGrowth))
      << "verify pushfront 1";

  // Now push one more so it grows
  d.PushFront(pool + kMaxSizeBeforeGrowth);
  EXPECT_EQ(kMaxSizeBeforeGrowth + 1, d.GetSize()) << "verify size";

  static const int t2[] = {8, 7, 6, 5, 4, 3, 2, 1, 0};
  EXPECT_TRUE(VerifyContents(d, t2, kMaxSizeBeforeGrowth + 1))
      << "verify pushfront 2";

  // And one more so that it wraps again
  d.PushFront(pool + kMaxSizeBeforeGrowth + 1);
  EXPECT_EQ(kMaxSizeBeforeGrowth + 2, d.GetSize()) << "verify size";

  static const int t3[] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
  EXPECT_TRUE(VerifyContents(d, t3, kMaxSizeBeforeGrowth + 2))
      << "verify pushfront 3";
}

static void CheckIfQueueEmpty(nsDeque& d) {
  EXPECT_EQ(0u, d.GetSize()) << "Size should be 0";
  EXPECT_EQ(nullptr, d.Pop()) << "Invalid operation should return nullptr";
  EXPECT_EQ(nullptr, d.PopFront()) << "Invalid operation should return nullptr";
  EXPECT_EQ(nullptr, d.Peek()) << "Invalid operation should return nullptr";
  EXPECT_EQ(nullptr, d.PeekFront())
      << "Invalid operation should return nullptr";
  EXPECT_EQ(nullptr, d.ObjectAt(0u))
      << "Invalid operation should return nullptr";
}

TEST(NsDeque, TestEmpty)
{
  // Make sure nsDeque gives sane results if it's empty.
  nsDeque d;
  size_t numberOfEntries = 8;

  CheckIfQueueEmpty(d);

  // Fill it up and drain it.
  for (size_t i = 0; i < numberOfEntries; i++) {
    d.Push((void*)0xAA);
  }

  EXPECT_EQ(numberOfEntries, d.GetSize());

  for (size_t i = 0; i < numberOfEntries; i++) {
    (void)d.Pop();
  }

  // Now check it again.
  CheckIfQueueEmpty(d);
}

TEST(NsDeque, TestEraseMethod)
{
  nsDeque d;
  const size_t numberOfEntries = 8;

  // Fill it up before calling Erase
  for (size_t i = 0; i < numberOfEntries; i++) {
    d.Push((void*)0xAA);
  }

  // Call Erase
  d.Erase();

  // Now check it again.
  CheckIfQueueEmpty(d);
}

TEST(NsDeque, TestEraseShouldCallDeallocator)
{
  nsDeque d(new Deallocator());
  const size_t NumTestValues = 8;

  int* testArray[NumTestValues];
  for (size_t i = 0; i < NumTestValues; i++) {
    testArray[i] = new int();
    *(testArray[i]) = i;
    d.Push((void*)testArray[i]);
  }

  d.Erase();

  // Now check it again.
  CheckIfQueueEmpty(d);

  for (size_t i = 0; i < NumTestValues; i++) {
    EXPECT_EQ(-1, *(testArray[i]))
        << "Erase should call deallocator: " << *(testArray[i]);
  }
}

TEST(NsDeque, TestForEach)
{
  nsDeque d(new Deallocator());
  const size_t NumTestValues = 8;
  int sum = 0;

  int* testArray[NumTestValues];
  for (size_t i = 0; i < NumTestValues; i++) {
    testArray[i] = new int();
    *(testArray[i]) = i;
    sum += i;
    d.Push((void*)testArray[i]);
  }

  ForEachAdder adder;
  d.ForEach(adder);
  EXPECT_EQ(sum, adder.GetSum()) << "For each should iterate over values";

  d.Erase();
}

TEST(NsDeque, TestConstRangeFor)
{
  nsDeque d(new Deallocator());

  const size_t NumTestValues = 3;
  for (size_t i = 0; i < NumTestValues; ++i) {
    d.Push(new int(i + 1));
  }

  static_assert(
      std::is_same_v<nsDeque::ConstDequeIterator,
                     decltype(std::declval<const nsDeque&>().begin())>,
      "(const nsDeque).begin() should return ConstDequeIterator");
  static_assert(std::is_same_v<nsDeque::ConstDequeIterator,
                               decltype(std::declval<const nsDeque&>().end())>,
                "(const nsDeque).end() should return ConstDequeIterator");

  int sum = 0;
  for (void* ob : const_cast<const nsDeque&>(d)) {
    sum += *static_cast<int*>(ob);
  }
  EXPECT_EQ(1 + 2 + 3, sum) << "Const-range-for should iterate over values";
}

TEST(NsDeque, TestRangeFor)
{
  const size_t NumTestValues = 3;
  struct Test {
    size_t runAfterLoopCount;
    std::function<void(nsDeque&)> function;
    int expectedSum;
    const char* description;
  };
  // Note: All tests start with a deque containing 3 pointers to ints 1, 2, 3.
  Test tests[] = {
      {0, [](nsDeque& d) {}, 1 + 2 + 3, "no changes"},

      {1, [](nsDeque& d) { d.Pop(); }, 1 + 2, "Pop after 1st loop"},
      {2, [](nsDeque& d) { d.Pop(); }, 1 + 2, "Pop after 2nd loop"},
      {3, [](nsDeque& d) { d.Pop(); }, 1 + 2 + 3, "Pop after 3rd loop"},

      {1, [](nsDeque& d) { d.PopFront(); }, 1 + 3, "PopFront after 1st loop"},
      {2, [](nsDeque& d) { d.PopFront(); }, 1 + 2, "PopFront after 2nd loop"},
      {3, [](nsDeque& d) { d.PopFront(); }, 1 + 2 + 3,
       "PopFront after 3rd loop"},

      {1, [](nsDeque& d) { d.Push(new int(4)); }, 1 + 2 + 3 + 4,
       "Push after 1st loop"},
      {2, [](nsDeque& d) { d.Push(new int(4)); }, 1 + 2 + 3 + 4,
       "Push after 2nd loop"},
      {3, [](nsDeque& d) { d.Push(new int(4)); }, 1 + 2 + 3 + 4,
       "Push after 3rd loop"},
      {4, [](nsDeque& d) { d.Push(new int(4)); }, 1 + 2 + 3,
       "Push after would-be-4th loop"},

      {1, [](nsDeque& d) { d.PushFront(new int(4)); }, 1 + 1 + 2 + 3,
       "PushFront after 1st loop"},
      {2, [](nsDeque& d) { d.PushFront(new int(4)); }, 1 + 2 + 2 + 3,
       "PushFront after 2nd loop"},
      {3, [](nsDeque& d) { d.PushFront(new int(4)); }, 1 + 2 + 3 + 3,
       "PushFront after 3rd loop"},
      {4, [](nsDeque& d) { d.PushFront(new int(4)); }, 1 + 2 + 3,
       "PushFront after would-be-4th loop"},

      {1, [](nsDeque& d) { d.Erase(); }, 1, "Erase after 1st loop"},
      {2, [](nsDeque& d) { d.Erase(); }, 1 + 2, "Erase after 2nd loop"},
      {3, [](nsDeque& d) { d.Erase(); }, 1 + 2 + 3, "Erase after 3rd loop"},

      {1,
       [](nsDeque& d) {
         d.Erase();
         d.Push(new int(4));
       },
       1, "Erase after 1st loop, Push 4"},
      {1,
       [](nsDeque& d) {
         d.Erase();
         d.Push(new int(4));
         d.Push(new int(5));
       },
       1 + 5, "Erase after 1st loop, Push 4,5"},
      {2,
       [](nsDeque& d) {
         d.Erase();
         d.Push(new int(4));
       },
       1 + 2, "Erase after 2nd loop, Push 4"},
      {2,
       [](nsDeque& d) {
         d.Erase();
         d.Push(new int(4));
         d.Push(new int(5));
       },
       1 + 2, "Erase after 2nd loop, Push 4,5"},
      {2,
       [](nsDeque& d) {
         d.Erase();
         d.Push(new int(4));
         d.Push(new int(5));
         d.Push(new int(6));
       },
       1 + 2 + 6, "Erase after 2nd loop, Push 4,5,6"}};

  for (const Test& test : tests) {
    nsDeque d(new Deallocator());

    for (size_t i = 0; i < NumTestValues; ++i) {
      d.Push(new int(i + 1));
    }

    static_assert(std::is_same_v<nsDeque::ConstIterator, decltype(d.begin())>,
                  "(non-const nsDeque).begin() should return ConstIterator");
    static_assert(std::is_same_v<nsDeque::ConstIterator, decltype(d.end())>,
                  "(non-const nsDeque).end() should return ConstIterator");

    int sum = 0;
    size_t loopCount = 0;
    for (void* ob : d) {
      sum += *static_cast<int*>(ob);
      if (++loopCount == test.runAfterLoopCount) {
        test.function(d);
      }
    }
    EXPECT_EQ(test.expectedSum, sum)
        << "Range-for should iterate over values in test '" << test.description
        << "'";
  }
}
