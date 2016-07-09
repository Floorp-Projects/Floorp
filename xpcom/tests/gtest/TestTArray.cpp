/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTArray.h"
#include "gtest/gtest.h"

using namespace mozilla;

namespace TestTArray {

struct Copyable
{
  Copyable()
    : mDestructionCounter(nullptr)
  {
  }

  ~Copyable()
  {
    if (mDestructionCounter) {
      (*mDestructionCounter)++;
    }
  }

  Copyable(const Copyable&) = default;
  Copyable& operator=(const Copyable&) = default;

  uint32_t* mDestructionCounter;
};

struct Movable
{
  Movable()
    : mDestructionCounter(nullptr)
  {
  }

  ~Movable()
  {
    if (mDestructionCounter) {
      (*mDestructionCounter)++;
    }
  }

  Movable(Movable&& aOther)
    : mDestructionCounter(aOther.mDestructionCounter)
  {
    aOther.mDestructionCounter = nullptr;
  }

  uint32_t* mDestructionCounter;
};

} // namespace TestTArray

template<>
struct nsTArray_CopyChooser<TestTArray::Copyable>
{
  typedef nsTArray_CopyWithConstructors<TestTArray::Copyable> Type;
};

template<>
struct nsTArray_CopyChooser<TestTArray::Movable>
{
  typedef nsTArray_CopyWithConstructors<TestTArray::Movable> Type;
};

namespace TestTArray {

const nsTArray<int>& DummyArray()
{
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
const nsTArray<int>& FakeHugeArray()
{
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
  array.AppendElements(Move(temp));
  ASSERT_EQ(DummyArray(), array);
  ASSERT_TRUE(temp.IsEmpty());

  temp = DummyArray();
  array.AppendElements(Move(temp));
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
  array2.Assign(Move(array));
  ASSERT_TRUE(array.IsEmpty());
  ASSERT_EQ(DummyArray(), array2);
}

TEST(TArray, AssignmentOperatorSelfAssignment)
{
  nsTArray<int> array;
  array = DummyArray();

  array = array;
  ASSERT_EQ(DummyArray(), array);
  array = Move(array);
  ASSERT_EQ(DummyArray(), array);
}

TEST(TArray, CopyOverlappingForwards)
{
  nsTArray<Movable> array;
  const size_t rangeLength = 8;
  const size_t initialLength = 2 * rangeLength;
  array.AppendElements(initialLength);

  uint32_t destructionCounters[initialLength];
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
  nsTArray<Copyable> array;
  const size_t rangeLength = 8;
  const size_t initialLength = 2 * rangeLength;
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
  uint32_t destructionCounters[initialLength];
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

} // namespace TestTArray
