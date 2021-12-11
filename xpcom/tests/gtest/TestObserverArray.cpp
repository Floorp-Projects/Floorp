/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTObserverArray.h"
#include "gtest/gtest.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/UniquePtr.h"

using namespace mozilla;

typedef nsTObserverArray<int> IntArray;

#define DO_TEST(_type, _exp, _code)                                      \
  do {                                                                   \
    ++testNum;                                                           \
    count = 0;                                                           \
    IntArray::_type iter(arr);                                           \
    while (iter.HasMore() && count != ArrayLength(_exp)) {               \
      _code int next = iter.GetNext();                                   \
      int expected = _exp[count++];                                      \
      ASSERT_EQ(next, expected)                                          \
          << "During test " << testNum << " at position " << count - 1;  \
    }                                                                    \
    ASSERT_FALSE(iter.HasMore())                                         \
    << "During test " << testNum << ", iterator ran over";               \
    ASSERT_EQ(count, ArrayLength(_exp))                                  \
        << "During test " << testNum << ", iterator finished too early"; \
  } while (0)

// XXX Split this up into independent test cases
TEST(ObserverArray, Tests)
{
  IntArray arr;
  arr.AppendElement(3);
  arr.AppendElement(4);

  size_t count;
  int testNum = 0;

  // Basic sanity
  static int test1Expected[] = {3, 4};
  DO_TEST(ForwardIterator, test1Expected, {/* nothing */});

  // Appends
  static int test2Expected[] = {3, 4, 2};
  DO_TEST(ForwardIterator, test2Expected,
          if (count == 1) arr.AppendElement(2););
  DO_TEST(ForwardIterator, test2Expected, {/* nothing */});

  DO_TEST(EndLimitedIterator, test2Expected,
          if (count == 1) arr.AppendElement(5););

  static int test5Expected[] = {3, 4, 2, 5};
  DO_TEST(ForwardIterator, test5Expected, {/* nothing */});

  // Removals
  DO_TEST(ForwardIterator, test5Expected,
          if (count == 1) arr.RemoveElementAt(0););

  static int test7Expected[] = {4, 2, 5};
  DO_TEST(ForwardIterator, test7Expected, {/* nothing */});

  static int test8Expected[] = {4, 5};
  DO_TEST(ForwardIterator, test8Expected,
          if (count == 1) arr.RemoveElementAt(1););
  DO_TEST(ForwardIterator, test8Expected, {/* nothing */});

  arr.AppendElement(2);
  arr.AppendElementUnlessExists(6);
  static int test10Expected[] = {4, 5, 2, 6};
  DO_TEST(ForwardIterator, test10Expected, {/* nothing */});

  arr.AppendElementUnlessExists(5);
  DO_TEST(ForwardIterator, test10Expected, {/* nothing */});

  static int test12Expected[] = {4, 5, 6};
  DO_TEST(ForwardIterator, test12Expected,
          if (count == 1) arr.RemoveElementAt(2););
  DO_TEST(ForwardIterator, test12Expected, {/* nothing */});

  // Removals + Appends
  static int test14Expected[] = {4, 6, 7};
  DO_TEST(
      ForwardIterator, test14Expected, if (count == 1) {
        arr.RemoveElementAt(1);
        arr.AppendElement(7);
      });
  DO_TEST(ForwardIterator, test14Expected, {/* nothing */});

  arr.AppendElement(2);
  static int test16Expected[] = {4, 6, 7, 2};
  DO_TEST(ForwardIterator, test16Expected, {/* nothing */});

  static int test17Expected[] = {4, 7, 2};
  DO_TEST(
      EndLimitedIterator, test17Expected, if (count == 1) {
        arr.RemoveElementAt(1);
        arr.AppendElement(8);
      });

  static int test18Expected[] = {4, 7, 2, 8};
  DO_TEST(ForwardIterator, test18Expected, {/* nothing */});

  // Prepends
  arr.PrependElementUnlessExists(3);
  static int test19Expected[] = {3, 4, 7, 2, 8};
  DO_TEST(ForwardIterator, test19Expected, {/* nothing */});

  arr.PrependElementUnlessExists(7);
  DO_TEST(ForwardIterator, test19Expected, {/* nothing */});

  DO_TEST(
      ForwardIterator, test19Expected,
      if (count == 1) { arr.PrependElementUnlessExists(9); });

  static int test22Expected[] = {9, 3, 4, 7, 2, 8};
  DO_TEST(ForwardIterator, test22Expected, {});

  // BackwardIterator
  static int test23Expected[] = {8, 2, 7, 4, 3, 9};
  DO_TEST(BackwardIterator, test23Expected, );

  // Removals
  static int test24Expected[] = {8, 2, 7, 4, 9};
  DO_TEST(BackwardIterator, test24Expected,
          if (count == 1) arr.RemoveElementAt(1););

  // Appends
  DO_TEST(BackwardIterator, test24Expected,
          if (count == 1) arr.AppendElement(1););

  static int test26Expected[] = {1, 8, 2, 7, 4, 9};
  DO_TEST(BackwardIterator, test26Expected, );

  // Prepends
  static int test27Expected[] = {1, 8, 2, 7, 4, 9, 3};
  DO_TEST(BackwardIterator, test27Expected,
          if (count == 1) arr.PrependElementUnlessExists(3););

  // Removal using Iterator
  DO_TEST(BackwardIterator, test27Expected,
          // when this code runs, |GetNext()| has only been called once, so
          // this actually removes the very first element
          if (count == 1) iter.Remove(););

  static int test28Expected[] = {8, 2, 7, 4, 9, 3};
  DO_TEST(BackwardIterator, test28Expected, );

  /**
   * Note: _code is executed before the call to GetNext(), it can therefore not
   * test the case of prepending when the BackwardIterator already returned the
   * first element.
   * In that case BackwardIterator does not traverse the newly prepended Element
   */
}

TEST(ObserverArray, ForwardIterator_Remove)
{
  static const int expected[] = {3, 4};

  IntArray arr;
  arr.AppendElement(3);
  arr.AppendElement(4);

  size_t count = 0;
  for (auto iter = IntArray::ForwardIterator{arr}; iter.HasMore();) {
    const int next = iter.GetNext();
    iter.Remove();

    ASSERT_EQ(expected[count++], next);
  }
  ASSERT_EQ(2u, count);
}

TEST(ObserverArray, RangeBasedFor_Value_Forward_NonEmpty)
{
  IntArray arr;
  arr.AppendElement(3);
  arr.AppendElement(4);

  size_t iterations = 0;
  int sum = 0;
  for (int element : arr.ForwardRange()) {
    sum += element;
    ++iterations;
  }

  EXPECT_EQ(2u, iterations);
  EXPECT_EQ(7, sum);
}

TEST(ObserverArray, RangeBasedFor_Value_Forward_RemoveCurrent)
{
  IntArray arr;
  arr.AppendElement(3);
  arr.AppendElement(4);

  size_t iterations = 0;
  int sum = 0;
  for (int element : arr.ForwardRange()) {
    sum += element;
    ++iterations;
    arr.RemoveElementAt(0);
  }

  EXPECT_EQ(2u, iterations);
  EXPECT_EQ(7, sum);
}

TEST(ObserverArray, RangeBasedFor_Value_Forward_Append)
{
  IntArray arr;
  arr.AppendElement(3);
  arr.AppendElement(4);

  size_t iterations = 0;
  int sum = 0;
  for (int element : arr.ForwardRange()) {
    if (!iterations) {
      arr.AppendElement(5);
    }
    sum += element;
    ++iterations;
  }

  EXPECT_EQ(3u, iterations);
  EXPECT_EQ(12, sum);
}

TEST(ObserverArray, RangeBasedFor_Value_Forward_Prepend)
{
  IntArray arr;
  arr.AppendElement(3);
  arr.AppendElement(4);

  size_t iterations = 0;
  int sum = 0;
  for (int element : arr.ForwardRange()) {
    if (!iterations) {
      arr.InsertElementAt(0, 5);
    }
    sum += element;
    ++iterations;
  }

  EXPECT_EQ(2u, iterations);
  EXPECT_EQ(7, sum);
}

TEST(ObserverArray, RangeBasedFor_Value_Forward_Empty)
{
  IntArray arr;

  size_t iterations = 0;
  for (int element : arr.ForwardRange()) {
    (void)element;
    ++iterations;
  }

  EXPECT_EQ(0u, iterations);
}

TEST(ObserverArray, RangeBasedFor_Reference_Forward_NonEmpty)
{
  const auto arr = [] {
    nsTObserverArray<UniquePtr<int>> arr;
    arr.AppendElement(MakeUnique<int>(3));
    arr.AppendElement(MakeUnique<int>(4));
    return arr;
  }();

  size_t iterations = 0;
  int sum = 0;
  for (const UniquePtr<int>& element : arr.ForwardRange()) {
    sum += *element;
    ++iterations;
  }

  EXPECT_EQ(2u, iterations);
  EXPECT_EQ(7, sum);
}

TEST(ObserverArray, RangeBasedFor_NonConstReference_Forward_NonEmpty)
{
  nsTObserverArray<UniquePtr<int>> arr;
  arr.AppendElement(MakeUnique<int>(3));
  arr.AppendElement(MakeUnique<int>(4));

  size_t iterations = 0;
  int sum = 0;
  for (UniquePtr<int>& element : arr.ForwardRange()) {
    sum += *element;
    ++iterations;
  }

  EXPECT_EQ(2u, iterations);
  EXPECT_EQ(7, sum);
}

TEST(ObserverArray, RangeBasedFor_Value_Backward_NonEmpty)
{
  IntArray arr;
  arr.AppendElement(3);
  arr.AppendElement(4);

  size_t iterations = 0;
  int sum = 0;
  for (int element : arr.BackwardRange()) {
    sum += element;
    ++iterations;
  }

  EXPECT_EQ(2u, iterations);
  EXPECT_EQ(7, sum);
}

TEST(ObserverArray, RangeBasedFor_Value_Backward_RemoveCurrent)
{
  IntArray arr;
  arr.AppendElement(3);
  arr.AppendElement(4);

  size_t iterations = 0;
  int sum = 0;
  for (int element : arr.BackwardRange()) {
    sum += element;
    ++iterations;
    arr.RemoveElementAt(arr.Length() - 1);
  }

  EXPECT_EQ(2u, iterations);
  EXPECT_EQ(7, sum);
}

TEST(ObserverArray, RangeBasedFor_Value_Backward_Append)
{
  IntArray arr;
  arr.AppendElement(3);
  arr.AppendElement(4);

  size_t iterations = 0;
  int sum = 0;
  for (int element : arr.BackwardRange()) {
    if (!iterations) {
      arr.AppendElement(5);
    }
    sum += element;
    ++iterations;
  }

  EXPECT_EQ(2u, iterations);
  EXPECT_EQ(7, sum);
}

TEST(ObserverArray, RangeBasedFor_Value_Backward_Prepend)
{
  IntArray arr;
  arr.AppendElement(3);
  arr.AppendElement(4);

  size_t iterations = 0;
  int sum = 0;
  for (int element : arr.BackwardRange()) {
    if (!iterations) {
      arr.InsertElementAt(0, 5);
    }
    sum += element;
    ++iterations;
  }

  EXPECT_EQ(3u, iterations);
  EXPECT_EQ(12, sum);
}

TEST(ObserverArray, RangeBasedFor_Value_Backward_Empty)
{
  IntArray arr;

  size_t iterations = 0;
  for (int element : arr.BackwardRange()) {
    (void)element;
    ++iterations;
  }

  EXPECT_EQ(0u, iterations);
}

TEST(ObserverArray, RangeBasedFor_Reference_Backward_NonEmpty)
{
  const auto arr = [] {
    nsTObserverArray<UniquePtr<int>> arr;
    arr.AppendElement(MakeUnique<int>(3));
    arr.AppendElement(MakeUnique<int>(4));
    return arr;
  }();

  size_t iterations = 0;
  int sum = 0;
  for (const UniquePtr<int>& element : arr.BackwardRange()) {
    sum += *element;
    ++iterations;
  }

  EXPECT_EQ(2u, iterations);
  EXPECT_EQ(7, sum);
}

TEST(ObserverArray, RangeBasedFor_NonConstReference_Backward_NonEmpty)
{
  nsTObserverArray<UniquePtr<int>> arr;
  arr.AppendElement(MakeUnique<int>(3));
  arr.AppendElement(MakeUnique<int>(4));

  size_t iterations = 0;
  int sum = 0;
  for (UniquePtr<int>& element : arr.BackwardRange()) {
    sum += *element;
    ++iterations;
  }

  EXPECT_EQ(2u, iterations);
  EXPECT_EQ(7, sum);
}

TEST(ObserverArray, RangeBasedFor_Value_EndLimited_NonEmpty)
{
  IntArray arr;
  arr.AppendElement(3);
  arr.AppendElement(4);

  size_t iterations = 0;
  int sum = 0;
  for (int element : arr.EndLimitedRange()) {
    sum += element;
    ++iterations;
  }

  EXPECT_EQ(2u, iterations);
  EXPECT_EQ(7, sum);
}

TEST(ObserverArray, RangeBasedFor_Value_EndLimited_RemoveCurrent)
{
  IntArray arr;
  arr.AppendElement(3);
  arr.AppendElement(4);

  size_t iterations = 0;
  int sum = 0;
  for (int element : arr.EndLimitedRange()) {
    sum += element;
    ++iterations;
    arr.RemoveElementAt(0);
  }

  EXPECT_EQ(2u, iterations);
  EXPECT_EQ(7, sum);
}

TEST(ObserverArray, RangeBasedFor_Value_EndLimited_Append)
{
  IntArray arr;
  arr.AppendElement(3);
  arr.AppendElement(4);

  size_t iterations = 0;
  int sum = 0;
  for (int element : arr.EndLimitedRange()) {
    if (!iterations) {
      arr.AppendElement(5);
    }
    sum += element;
    ++iterations;
  }

  EXPECT_EQ(2u, iterations);
  EXPECT_EQ(7, sum);
}

TEST(ObserverArray, RangeBasedFor_Value_EndLimited_Prepend)
{
  IntArray arr;
  arr.AppendElement(3);
  arr.AppendElement(4);

  size_t iterations = 0;
  int sum = 0;
  for (int element : arr.EndLimitedRange()) {
    if (!iterations) {
      arr.InsertElementAt(0, 5);
    }
    sum += element;
    ++iterations;
  }

  EXPECT_EQ(2u, iterations);
  EXPECT_EQ(7, sum);
}

TEST(ObserverArray, RangeBasedFor_Value_EndLimited_Empty)
{
  IntArray arr;

  size_t iterations = 0;
  for (int element : arr.EndLimitedRange()) {
    (void)element;
    ++iterations;
  }

  EXPECT_EQ(0u, iterations);
}

TEST(ObserverArray, RangeBasedFor_Reference_EndLimited_NonEmpty)
{
  const auto arr = [] {
    nsTObserverArray<UniquePtr<int>> arr;
    arr.AppendElement(MakeUnique<int>(3));
    arr.AppendElement(MakeUnique<int>(4));
    return arr;
  }();

  size_t iterations = 0;
  int sum = 0;
  for (const UniquePtr<int>& element : arr.EndLimitedRange()) {
    sum += *element;
    ++iterations;
  }

  EXPECT_EQ(2u, iterations);
  EXPECT_EQ(7, sum);
}

TEST(ObserverArray, RangeBasedFor_NonConstReference_EndLimited_NonEmpty)
{
  nsTObserverArray<UniquePtr<int>> arr;
  arr.AppendElement(MakeUnique<int>(3));
  arr.AppendElement(MakeUnique<int>(4));

  size_t iterations = 0;
  int sum = 0;
  for (UniquePtr<int>& element : arr.EndLimitedRange()) {
    sum += *element;
    ++iterations;
  }

  EXPECT_EQ(2u, iterations);
  EXPECT_EQ(7, sum);
}

TEST(ObserverArray, RangeBasedFor_Reference_NonObserving_NonEmpty)
{
  const auto arr = [] {
    nsTObserverArray<UniquePtr<int>> arr;
    arr.AppendElement(MakeUnique<int>(3));
    arr.AppendElement(MakeUnique<int>(4));
    return arr;
  }();

  size_t iterations = 0;
  int sum = 0;
  for (const UniquePtr<int>& element : arr.NonObservingRange()) {
    sum += *element;
    ++iterations;
  }

  EXPECT_EQ(2u, iterations);
  EXPECT_EQ(7, sum);
}

// TODO add tests for EndLimitedIterator
