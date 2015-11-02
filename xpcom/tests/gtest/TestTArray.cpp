/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTArray.h"
#include "gtest/gtest.h"

using namespace mozilla;

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

} // namespace TestTArray
