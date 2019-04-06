/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTArray.h"
#include "gtest/gtest.h"
#include "mozilla/ArrayUtils.h"

using namespace mozilla;

namespace TestTArray {

struct Copyable {
  Copyable() : mDestructionCounter(nullptr) {}

  ~Copyable() {
    if (mDestructionCounter) {
      (*mDestructionCounter)++;
    }
  }

  Copyable(const Copyable&) = default;
  Copyable& operator=(const Copyable&) = default;

  uint32_t* mDestructionCounter;
};

struct Movable {
  Movable() : mDestructionCounter(nullptr) {}

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

}  // namespace TestTArray

template <>
struct nsTArray_CopyChooser<TestTArray::Copyable> {
  typedef nsTArray_CopyWithConstructors<TestTArray::Copyable> Type;
};

template <>
struct nsTArray_CopyChooser<TestTArray::Movable> {
  typedef nsTArray_CopyWithConstructors<TestTArray::Movable> Type;
};

namespace TestTArray {

static const nsTArray<int>& DummyArray() {
  static nsTArray<int> sArray;
  if (sArray.IsEmpty()) {
    const int data[] = {4, 1, 2, 8};
    sArray.AppendElements(data, ArrayLength(data));
  }
  return sArray;
}

// This returns an invalid nsTArray with a huge length in order to test that
// fallible operations actually fail.
#ifdef DEBUG
static const nsTArray<int>& FakeHugeArray() {
  static nsTArray<int> sArray;
  if (sArray.IsEmpty()) {
    sArray.AppendElement();
    ((nsTArrayHeader*)sArray.DebugGetHeader())->mLength = UINT32_MAX;
  }
  return sArray;
}
#endif

TEST(TArray, AppendElementsRvalue)
{
  nsTArray<int> array;

  nsTArray<int> temp(DummyArray());
  array.AppendElements(std::move(temp));
  ASSERT_EQ(DummyArray(), array);
  ASSERT_TRUE(temp.IsEmpty());

  temp = DummyArray();
  array.AppendElements(std::move(temp));
  nsTArray<int> expected;
  expected.AppendElements(DummyArray());
  expected.AppendElements(DummyArray());
  ASSERT_EQ(expected, array);
  ASSERT_TRUE(temp.IsEmpty());
}

TEST(TArray, Assign)
{
  nsTArray<int> array;
  array.Assign(DummyArray());
  ASSERT_EQ(DummyArray(), array);

  ASSERT_TRUE(array.Assign(DummyArray(), fallible));
  ASSERT_EQ(DummyArray(), array);

#ifdef DEBUG
  ASSERT_FALSE(array.Assign(FakeHugeArray(), fallible));
#endif

  nsTArray<int> array2;
  array2.Assign(std::move(array));
  ASSERT_TRUE(array.IsEmpty());
  ASSERT_EQ(DummyArray(), array2);
}

TEST(TArray, AssignmentOperatorSelfAssignment)
{
  nsTArray<int> array;
  array = DummyArray();

  array = *&array;
  ASSERT_EQ(DummyArray(), array);

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wself-move"
#endif
  array = std::move(array);  // self-move
  ASSERT_EQ(DummyArray(), array);
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif
}

TEST(TArray, CopyOverlappingForwards)
{
  const size_t rangeLength = 8;
  const size_t initialLength = 2 * rangeLength;
  uint32_t destructionCounters[initialLength];
  nsTArray<Movable> array;
  array.AppendElements(initialLength);

  for (uint32_t i = 0; i < initialLength; ++i) {
    destructionCounters[i] = 0;
  }
  for (uint32_t i = 0; i < initialLength; ++i) {
    array[i].mDestructionCounter = &destructionCounters[i];
  }

  const size_t removedLength = rangeLength / 2;
  array.RemoveElementsAt(0, removedLength);

  for (uint32_t i = 0; i < removedLength; ++i) {
    ASSERT_EQ(destructionCounters[i], 1u);
  }
  for (uint32_t i = removedLength; i < initialLength; ++i) {
    ASSERT_EQ(destructionCounters[i], 0u);
  }
}

// The code to copy overlapping regions had a bug in that it wouldn't correctly
// destroy all over the source elements being copied.
TEST(TArray, CopyOverlappingBackwards)
{
  const size_t rangeLength = 8;
  const size_t initialLength = 2 * rangeLength;
  uint32_t destructionCounters[initialLength];
  nsTArray<Copyable> array;
  array.SetCapacity(3 * rangeLength);
  array.AppendElements(initialLength);
  // To tickle the bug, we need to copy a source region:
  //
  //   ..XXXXX..
  //
  // such that it overlaps the destination region:
  //
  //   ....XXXXX
  //
  // so we are forced to copy back-to-front to ensure correct behavior.
  // The easiest way to do that is to call InsertElementsAt, which will force
  // the desired kind of shift.
  for (uint32_t i = 0; i < initialLength; ++i) {
    destructionCounters[i] = 0;
  }
  for (uint32_t i = 0; i < initialLength; ++i) {
    array[i].mDestructionCounter = &destructionCounters[i];
  }

  array.InsertElementsAt(0, rangeLength);

  for (uint32_t i = 0; i < initialLength; ++i) {
    ASSERT_EQ(destructionCounters[i], 1u);
  }
}

TEST(TArray, UnorderedRemoveElements)
{
  // When removing an element from the end of the array, it can be removed in
  // place, by destroying it and decrementing the length.
  //
  // [ 1, 2, 3 ] => [ 1, 2 ]
  //         ^
  {
    nsTArray<int> array{1, 2, 3};
    array.UnorderedRemoveElementAt(2);

    nsTArray<int> goal{1, 2};
    ASSERT_EQ(array, goal);
  }

  // When removing any other single element, it is removed by swapping it with
  // the last element, and then decrementing the length as before.
  //
  // [ 1, 2, 3, 4, 5, 6 ]  => [ 1, 6, 3, 4, 5 ]
  //      ^
  {
    nsTArray<int> array{1, 2, 3, 4, 5, 6};
    array.UnorderedRemoveElementAt(1);

    nsTArray<int> goal{1, 6, 3, 4, 5};
    ASSERT_EQ(array, goal);
  }

  // This method also supports efficiently removing a range of elements. If they
  // are at the end, then they can all be removed like in the one element case.
  //
  // [ 1, 2, 3, 4, 5, 6 ] => [ 1, 2 ]
  //         ^--------^
  {
    nsTArray<int> array{1, 2, 3, 4, 5, 6};
    array.UnorderedRemoveElementsAt(2, 4);

    nsTArray<int> goal{1, 2};
    ASSERT_EQ(array, goal);
  }

  // If more elements are removed than exist after the removed section, the
  // remaining elements will be shifted down like in a normal removal.
  //
  // [ 1, 2, 3, 4, 5, 6, 7, 8 ] => [ 1, 2, 7, 8 ]
  //         ^--------^
  {
    nsTArray<int> array{1, 2, 3, 4, 5, 6, 7, 8};
    array.UnorderedRemoveElementsAt(2, 4);

    nsTArray<int> goal{1, 2, 7, 8};
    ASSERT_EQ(array, goal);
  }

  // And if fewer elements are removed than exist after the removed section,
  // elements will be moved from the end of the array to fill the vacated space.
  //
  // [ 1, 2, 3, 4, 5, 6, 7, 8 ] => [ 1, 7, 8, 4, 5, 6 ]
  //      ^--^
  {
    nsTArray<int> array{1, 2, 3, 4, 5, 6, 7, 8};
    array.UnorderedRemoveElementsAt(1, 2);

    nsTArray<int> goal{1, 7, 8, 4, 5, 6};
    ASSERT_EQ(array, goal);
  }

  // We should do the right thing if we drain the entire array.
  {
    nsTArray<int> array{1, 2, 3, 4, 5};
    array.UnorderedRemoveElementsAt(0, 5);

    nsTArray<int> goal{};
    ASSERT_EQ(array, goal);
  }

  {
    nsTArray<int> array{1};
    array.UnorderedRemoveElementAt(0);

    nsTArray<int> goal{};
    ASSERT_EQ(array, goal);
  }

  // We should do the right thing if we remove the same number of elements that
  // we have remaining.
  {
    nsTArray<int> array{1, 2, 3, 4, 5, 6};
    array.UnorderedRemoveElementsAt(2, 2);

    nsTArray<int> goal{1, 2, 5, 6};
    ASSERT_EQ(array, goal);
  }

  {
    nsTArray<int> array{1, 2, 3};
    array.UnorderedRemoveElementAt(1);

    nsTArray<int> goal{1, 3};
    ASSERT_EQ(array, goal);
  }

  // We should be able to remove elements from the front without issue.
  {
    nsTArray<int> array{1, 2, 3, 4, 5, 6};
    array.UnorderedRemoveElementsAt(0, 2);

    nsTArray<int> goal{5, 6, 3, 4};
    ASSERT_EQ(array, goal);
  }

  {
    nsTArray<int> array{1, 2, 3, 4};
    array.UnorderedRemoveElementAt(0);

    nsTArray<int> goal{4, 2, 3};
    ASSERT_EQ(array, goal);
  }
}

TEST(TArray, RemoveFromEnd)
{
  {
    nsTArray<int> array{1, 2, 3, 4};
    ASSERT_EQ(array.PopLastElement(), 4);
    array.RemoveLastElement();
    ASSERT_EQ(array.PopLastElement(), 2);
    array.RemoveLastElement();
    ASSERT_TRUE(array.IsEmpty());
  }
}

}  // namespace TestTArray
