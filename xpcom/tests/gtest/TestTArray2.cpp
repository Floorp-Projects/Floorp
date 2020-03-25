/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Unused.h"

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include "nsTArray.h"
#include "nsString.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsXPCOM.h"
#include "nsIFile.h"

#include "gtest/gtest.h"

using namespace mozilla;

namespace TestTArray {

// Define this so we can use test_basic_array in test_comptr_array
template <class T>
inline bool operator<(const nsCOMPtr<T>& lhs, const nsCOMPtr<T>& rhs) {
  return lhs.get() < rhs.get();
}

//----

template <class ElementType>
static bool test_basic_array(ElementType* data, size_t dataLen,
                             const ElementType& extra) {
  nsTArray<ElementType> ary;
  const nsTArray<ElementType>& cary = ary;

  ary.AppendElements(data, dataLen);
  if (ary.Length() != dataLen) {
    return false;
  }
  if (!(ary == ary)) {
    return false;
  }
  size_t i;
  for (i = 0; i < ary.Length(); ++i) {
    if (ary[i] != data[i]) return false;
  }
  for (i = 0; i < ary.Length(); ++i) {
    if (ary.SafeElementAt(i, extra) != data[i]) return false;
  }
  if (ary.SafeElementAt(ary.Length(), extra) != extra ||
      ary.SafeElementAt(ary.Length() * 10, extra) != extra)
    return false;
  // ensure sort results in ascending order
  ary.Sort();
  size_t j = 0, k = ary.IndexOfFirstElementGt(extra);
  if (k != 0 && ary[k - 1] == extra) return false;
  for (i = 0; i < ary.Length(); ++i) {
    k = ary.IndexOfFirstElementGt(ary[i]);
    if (k == 0 || ary[k - 1] != ary[i]) return false;
    if (k < j) return false;
    j = k;
  }
  for (i = ary.Length(); --i;) {
    if (ary[i] < ary[i - 1]) return false;
    if (ary[i] == ary[i - 1]) ary.RemoveElementAt(i);
  }
  if (!(ary == ary)) {
    return false;
  }
  for (i = 0; i < ary.Length(); ++i) {
    if (ary.BinaryIndexOf(ary[i]) != i) return false;
  }
  if (ary.BinaryIndexOf(extra) != ary.NoIndex) return false;
  size_t oldLen = ary.Length();
  ary.RemoveElement(data[dataLen / 2]);
  if (ary.Length() != (oldLen - 1)) return false;
  if (!(ary == ary)) return false;

  if (ary.ApplyIf(
          extra, []() { return true; }, []() { return false; }))
    return false;
  if (ary.ApplyIf(
          extra, [](size_t) { return true; }, []() { return false; }))
    return false;
  // On a non-const array, ApplyIf's first lambda may use either const or non-
  // const element types.
  if (ary.ApplyIf(
          extra, [](ElementType&) { return true; }, []() { return false; }))
    return false;
  if (ary.ApplyIf(
          extra, [](const ElementType&) { return true; },
          []() { return false; }))
    return false;
  if (ary.ApplyIf(
          extra, [](size_t, ElementType&) { return true; },
          []() { return false; }))
    return false;
  if (ary.ApplyIf(
          extra, [](size_t, const ElementType&) { return true; },
          []() { return false; }))
    return false;

  if (cary.ApplyIf(
          extra, []() { return true; }, []() { return false; }))
    if (cary.ApplyIf(
            extra, [](size_t) { return true; }, []() { return false; }))
      // On a const array, ApplyIf's first lambda must only use const element
      // types.
      if (cary.ApplyIf(
              extra, [](const ElementType&) { return true; },
              []() { return false; }))
        if (cary.ApplyIf(
                extra, [](size_t, const ElementType&) { return true; },
                []() { return false; }))
          return false;

  size_t index = ary.Length() / 2;
  if (!ary.InsertElementAt(index, extra)) return false;
  if (!(ary == ary)) return false;
  if (ary[index] != extra) return false;
  if (ary.IndexOf(extra) == ary.NoIndex) return false;
  if (ary.LastIndexOf(extra) == ary.NoIndex) return false;
  // ensure proper searching
  if (ary.IndexOf(extra) > ary.LastIndexOf(extra)) return false;
  if (ary.IndexOf(extra, index) != ary.LastIndexOf(extra, index)) return false;
  if (!ary.ApplyIf(
          extra,
          [&](size_t i, const ElementType& e) {
            return i == index && e == extra;
          },
          []() { return false; }))
    return false;
  if (!cary.ApplyIf(
          extra,
          [&](size_t i, const ElementType& e) {
            return i == index && e == extra;
          },
          []() { return false; }))
    return false;

  nsTArray<ElementType> copy(ary);
  if (!(ary == copy)) return false;
  for (i = 0; i < copy.Length(); ++i) {
    if (ary[i] != copy[i]) return false;
  }
  if (!ary.AppendElements(copy)) return false;
  size_t cap = ary.Capacity();
  ary.RemoveElementsAt(copy.Length(), copy.Length());
  ary.Compact();
  if (ary.Capacity() == cap) return false;

  ary.Clear();
  if (ary.IndexOf(extra) != ary.NoIndex) return false;
  if (ary.LastIndexOf(extra) != ary.NoIndex) return false;
  if (ary.ApplyIf(
          extra, []() { return true; }, []() { return false; }))
    return false;
  if (cary.ApplyIf(
          extra, []() { return true; }, []() { return false; }))
    return false;

  ary.Clear();
  if (!ary.IsEmpty() || ary.Elements() == nullptr) return false;
  if (!(ary == nsTArray<ElementType>())) return false;
  if (ary == copy) return false;
  if (ary.SafeElementAt(0, extra) != extra ||
      ary.SafeElementAt(10, extra) != extra)
    return false;

  ary = copy;
  if (!(ary == copy)) return false;
  for (i = 0; i < copy.Length(); ++i) {
    if (ary[i] != copy[i]) return false;
  }

  if (!ary.InsertElementsAt(0, copy)) return false;
  if (ary == copy) return false;
  ary.RemoveElementsAt(0, copy.Length());
  for (i = 0; i < copy.Length(); ++i) {
    if (ary[i] != copy[i]) return false;
  }

  // These shouldn't crash!
  nsTArray<ElementType> empty;
  ary.AppendElements(reinterpret_cast<ElementType*>(0), 0);
  ary.AppendElements(empty);

  // See bug 324981
  ary.RemoveElement(extra);
  ary.RemoveElement(extra);

  return true;
}

TEST(TArray, test_int_array)
{
  int data[] = {4, 6, 8, 2, 4, 1, 5, 7, 3};
  ASSERT_TRUE(test_basic_array(data, ArrayLength(data), int(14)));
}

TEST(TArray, test_int64_array)
{
  int64_t data[] = {4, 6, 8, 2, 4, 1, 5, 7, 3};
  ASSERT_TRUE(test_basic_array(data, ArrayLength(data), int64_t(14)));
}

TEST(TArray, test_char_array)
{
  char data[] = {4, 6, 8, 2, 4, 1, 5, 7, 3};
  ASSERT_TRUE(test_basic_array(data, ArrayLength(data), char(14)));
}

TEST(TArray, test_uint32_array)
{
  uint32_t data[] = {4, 6, 8, 2, 4, 1, 5, 7, 3};
  ASSERT_TRUE(test_basic_array(data, ArrayLength(data), uint32_t(14)));
}

//----

class Object {
 public:
  Object() : mNum(0) {}
  Object(const char* str, uint32_t num) : mStr(str), mNum(num) {}
  Object(const Object& other) = default;
  ~Object() = default;

  Object& operator=(const Object& other) = default;

  bool operator==(const Object& other) const {
    return mStr == other.mStr && mNum == other.mNum;
  }

  bool operator<(const Object& other) const {
    // sort based on mStr only
    return mStr.Compare(other.mStr.get()) < 0;
  }

  const char* Str() const { return mStr.get(); }
  uint32_t Num() const { return mNum; }

 private:
  nsCString mStr;
  uint32_t mNum;
};

TEST(TArray, test_object_array)
{
  nsTArray<Object> objArray;
  const char kdata[] = "hello world";
  size_t i;
  for (i = 0; i < ArrayLength(kdata); ++i) {
    char x[] = {kdata[i], '\0'};
    ASSERT_TRUE(objArray.AppendElement(Object(x, i)));
  }
  for (i = 0; i < ArrayLength(kdata); ++i) {
    ASSERT_EQ(objArray[i].Str()[0], kdata[i]);
    ASSERT_EQ(objArray[i].Num(), i);
  }
  objArray.Sort();
  const char ksorted[] = "\0 dehllloorw";
  for (i = 0; i < ArrayLength(kdata) - 1; ++i) {
    ASSERT_EQ(objArray[i].Str()[0], ksorted[i]);
  }
}

class Countable {
  static int sCount;

 public:
  Countable() { sCount++; }

  Countable(const Countable& aOther) { sCount++; }

  static int Count() { return sCount; }
};

class Moveable {
  static int sCount;

 public:
  Moveable() { sCount++; }

  Moveable(const Moveable& aOther) { sCount++; }

  Moveable(Moveable&& aOther) {
    // Do not increment sCount
  }

  static int Count() { return sCount; }
};

class MoveOnly_RelocateUsingMemutils {
 public:
  MoveOnly_RelocateUsingMemutils() = default;

  MoveOnly_RelocateUsingMemutils(const MoveOnly_RelocateUsingMemutils&) =
      delete;
  MoveOnly_RelocateUsingMemutils(MoveOnly_RelocateUsingMemutils&&) = default;

  MoveOnly_RelocateUsingMemutils& operator=(
      const MoveOnly_RelocateUsingMemutils&) = delete;
  MoveOnly_RelocateUsingMemutils& operator=(MoveOnly_RelocateUsingMemutils&&) =
      default;
};

static_assert(
    std::is_move_constructible_v<nsTArray<MoveOnly_RelocateUsingMemutils>>);
static_assert(
    std::is_move_assignable_v<nsTArray<MoveOnly_RelocateUsingMemutils>>);
static_assert(
    !std::is_copy_constructible_v<nsTArray<MoveOnly_RelocateUsingMemutils>>);
static_assert(
    !std::is_copy_assignable_v<nsTArray<MoveOnly_RelocateUsingMemutils>>);

class MoveOnly_RelocateUsingMoveConstructor {
 public:
  MoveOnly_RelocateUsingMoveConstructor() = default;

  MoveOnly_RelocateUsingMoveConstructor(
      const MoveOnly_RelocateUsingMoveConstructor&) = delete;
  MoveOnly_RelocateUsingMoveConstructor(
      MoveOnly_RelocateUsingMoveConstructor&&) = default;

  MoveOnly_RelocateUsingMoveConstructor& operator=(
      const MoveOnly_RelocateUsingMoveConstructor&) = delete;
  MoveOnly_RelocateUsingMoveConstructor& operator=(
      MoveOnly_RelocateUsingMoveConstructor&&) = default;
};
}  // namespace TestTArray

MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(
    TestTArray::MoveOnly_RelocateUsingMoveConstructor)

namespace TestTArray {
static_assert(std::is_move_constructible_v<
              nsTArray<MoveOnly_RelocateUsingMoveConstructor>>);
static_assert(
    std::is_move_assignable_v<nsTArray<MoveOnly_RelocateUsingMoveConstructor>>);
static_assert(!std::is_copy_constructible_v<
              nsTArray<MoveOnly_RelocateUsingMoveConstructor>>);
static_assert(!std::is_copy_assignable_v<
              nsTArray<MoveOnly_RelocateUsingMoveConstructor>>);
}  // namespace TestTArray

namespace TestTArray {

/* static */
int Countable::sCount = 0;
/* static */
int Moveable::sCount = 0;

static nsTArray<int> returns_by_value() {
  nsTArray<int> result;
  return result;
}

TEST(TArray, test_return_by_value)
{
  nsTArray<int> result = returns_by_value();
  ASSERT_TRUE(true);  // This is just a compilation test.
}

TEST(TArray, test_move_array)
{
  nsTArray<Countable> countableArray;
  uint32_t i;
  for (i = 0; i < 4; ++i) {
    ASSERT_TRUE(countableArray.AppendElement(Countable()));
  }

  ASSERT_EQ(Countable::Count(), 8);

  const nsTArray<Countable>& constRefCountableArray = countableArray;

  ASSERT_EQ(Countable::Count(), 8);

  nsTArray<Countable> copyCountableArray(constRefCountableArray);

  ASSERT_EQ(Countable::Count(), 12);

  nsTArray<Countable>&& moveRefCountableArray = std::move(countableArray);
  moveRefCountableArray.Length();  // Make compilers happy.

  ASSERT_EQ(Countable::Count(), 12);

  nsTArray<Countable> movedCountableArray(std::move(countableArray));

  ASSERT_EQ(Countable::Count(), 12);

  // Test ctor
  FallibleTArray<Countable> differentAllocatorCountableArray(
      std::move(copyCountableArray));
  // operator=
  copyCountableArray = std::move(differentAllocatorCountableArray);
  differentAllocatorCountableArray = std::move(copyCountableArray);
  // And the other ctor
  nsTArray<Countable> copyCountableArray2(
      std::move(differentAllocatorCountableArray));
  // with auto
  AutoTArray<Countable, 3> autoCountableArray(std::move(copyCountableArray2));
  // operator=
  copyCountableArray2 = std::move(autoCountableArray);
  // Mix with FallibleTArray
  FallibleTArray<Countable> differentAllocatorCountableArray2(
      std::move(copyCountableArray2));
  AutoTArray<Countable, 4> autoCountableArray2(
      std::move(differentAllocatorCountableArray2));
  differentAllocatorCountableArray2 = std::move(autoCountableArray2);

  ASSERT_EQ(Countable::Count(), 12);

  nsTArray<Moveable> moveableArray;
  for (i = 0; i < 4; ++i) {
    ASSERT_TRUE(moveableArray.AppendElement(Moveable()));
  }

  ASSERT_EQ(Moveable::Count(), 4);

  const nsTArray<Moveable>& constRefMoveableArray = moveableArray;

  ASSERT_EQ(Moveable::Count(), 4);

  nsTArray<Moveable> copyMoveableArray(constRefMoveableArray);

  ASSERT_EQ(Moveable::Count(), 8);

  nsTArray<Moveable>&& moveRefMoveableArray = std::move(moveableArray);
  moveRefMoveableArray.Length();  // Make compilers happy.

  ASSERT_EQ(Moveable::Count(), 8);

  nsTArray<Moveable> movedMoveableArray(std::move(moveableArray));

  ASSERT_EQ(Moveable::Count(), 8);

  // Test ctor
  FallibleTArray<Moveable> differentAllocatorMoveableArray(
      std::move(copyMoveableArray));
  // operator=
  copyMoveableArray = std::move(differentAllocatorMoveableArray);
  differentAllocatorMoveableArray = std::move(copyMoveableArray);
  // And the other ctor
  nsTArray<Moveable> copyMoveableArray2(
      std::move(differentAllocatorMoveableArray));
  // with auto
  AutoTArray<Moveable, 3> autoMoveableArray(std::move(copyMoveableArray2));
  // operator=
  copyMoveableArray2 = std::move(autoMoveableArray);
  // Mix with FallibleTArray
  FallibleTArray<Moveable> differentAllocatorMoveableArray2(
      std::move(copyMoveableArray2));
  AutoTArray<Moveable, 4> autoMoveableArray2(
      std::move(differentAllocatorMoveableArray2));
  differentAllocatorMoveableArray2 = std::move(autoMoveableArray2);

  ASSERT_EQ(Moveable::Count(), 8);

  AutoTArray<Moveable, 8> moveableAutoArray;
  for (uint32_t i = 0; i < 4; ++i) {
    ASSERT_TRUE(moveableAutoArray.AppendElement(Moveable()));
  }

  ASSERT_EQ(Moveable::Count(), 12);

  const AutoTArray<Moveable, 8>& constRefMoveableAutoArray = moveableAutoArray;

  ASSERT_EQ(Moveable::Count(), 12);

  AutoTArray<Moveable, 8> copyMoveableAutoArray(constRefMoveableAutoArray);

  ASSERT_EQ(Moveable::Count(), 16);

  AutoTArray<Moveable, 8> movedMoveableAutoArray(std::move(moveableAutoArray));

  ASSERT_EQ(Moveable::Count(), 16);
}

template <typename TypeParam>
class TArray_MoveOnlyTest : public ::testing::Test {};

TYPED_TEST_CASE_P(TArray_MoveOnlyTest);

static constexpr size_t kMoveOnlyTestArrayLength = 4;

template <typename ArrayType>
static auto MakeMoveOnlyArray() {
  ArrayType moveOnlyArray;
  for (size_t i = 0; i < kMoveOnlyTestArrayLength; ++i) {
    EXPECT_TRUE(
        moveOnlyArray.AppendElement(typename ArrayType::elem_type(), fallible));
  }
  return moveOnlyArray;
}

TYPED_TEST_P(TArray_MoveOnlyTest, nsTArray_MoveConstruct) {
  auto moveOnlyArray = MakeMoveOnlyArray<nsTArray<TypeParam>>();
  nsTArray<TypeParam> movedMoveOnlyArray(std::move(moveOnlyArray));

  ASSERT_EQ(0u, moveOnlyArray.Length());
  ASSERT_EQ(kMoveOnlyTestArrayLength, movedMoveOnlyArray.Length());
}

TYPED_TEST_P(TArray_MoveOnlyTest, nsTArray_MoveAssign) {
  auto moveOnlyArray = MakeMoveOnlyArray<nsTArray<TypeParam>>();
  nsTArray<TypeParam> movedMoveOnlyArray;
  movedMoveOnlyArray = std::move(moveOnlyArray);

  ASSERT_EQ(0u, moveOnlyArray.Length());
  ASSERT_EQ(kMoveOnlyTestArrayLength, movedMoveOnlyArray.Length());
}

TYPED_TEST_P(TArray_MoveOnlyTest, nsTArray_MoveReAssign) {
  nsTArray<TypeParam> movedMoveOnlyArray;
  movedMoveOnlyArray = MakeMoveOnlyArray<nsTArray<TypeParam>>();
  // Re-assign, to check that move-assign does not only work on an empty array.
  movedMoveOnlyArray = MakeMoveOnlyArray<nsTArray<TypeParam>>();

  ASSERT_EQ(kMoveOnlyTestArrayLength, movedMoveOnlyArray.Length());
}

TYPED_TEST_P(TArray_MoveOnlyTest, nsTArray_to_FallibleTArray_MoveConstruct) {
  auto moveOnlyArray = MakeMoveOnlyArray<nsTArray<TypeParam>>();
  FallibleTArray<TypeParam> differentAllocatorMoveOnlyArray(
      std::move(moveOnlyArray));

  ASSERT_EQ(0u, moveOnlyArray.Length());
  ASSERT_EQ(kMoveOnlyTestArrayLength, differentAllocatorMoveOnlyArray.Length());
}

TYPED_TEST_P(TArray_MoveOnlyTest, nsTArray_to_FallibleTArray_MoveAssign) {
  auto moveOnlyArray = MakeMoveOnlyArray<nsTArray<TypeParam>>();
  FallibleTArray<TypeParam> differentAllocatorMoveOnlyArray;
  differentAllocatorMoveOnlyArray = std::move(moveOnlyArray);

  ASSERT_EQ(0u, moveOnlyArray.Length());
  ASSERT_EQ(kMoveOnlyTestArrayLength, differentAllocatorMoveOnlyArray.Length());
}

TYPED_TEST_P(TArray_MoveOnlyTest, FallibleTArray_to_nsTArray_MoveConstruct) {
  auto moveOnlyArray = MakeMoveOnlyArray<FallibleTArray<TypeParam>>();
  nsTArray<TypeParam> differentAllocatorMoveOnlyArray(std::move(moveOnlyArray));

  ASSERT_EQ(0u, moveOnlyArray.Length());
  ASSERT_EQ(kMoveOnlyTestArrayLength, differentAllocatorMoveOnlyArray.Length());
}

TYPED_TEST_P(TArray_MoveOnlyTest, FallibleTArray_to_nsTArray_MoveAssign) {
  auto moveOnlyArray = MakeMoveOnlyArray<FallibleTArray<TypeParam>>();
  nsTArray<TypeParam> differentAllocatorMoveOnlyArray;
  differentAllocatorMoveOnlyArray = std::move(moveOnlyArray);

  ASSERT_EQ(0u, moveOnlyArray.Length());
  ASSERT_EQ(kMoveOnlyTestArrayLength, differentAllocatorMoveOnlyArray.Length());
}

TYPED_TEST_P(TArray_MoveOnlyTest, AutoTArray_AutoStorage_MoveConstruct) {
  auto moveOnlyArray =
      MakeMoveOnlyArray<AutoTArray<TypeParam, kMoveOnlyTestArrayLength>>();
  AutoTArray<TypeParam, kMoveOnlyTestArrayLength> autoMoveOnlyArray(
      std::move(moveOnlyArray));

  ASSERT_EQ(0u, moveOnlyArray.Length());
  ASSERT_EQ(kMoveOnlyTestArrayLength, autoMoveOnlyArray.Length());
}

TYPED_TEST_P(TArray_MoveOnlyTest, AutoTArray_AutoStorage_MoveAssign) {
  auto moveOnlyArray =
      MakeMoveOnlyArray<AutoTArray<TypeParam, kMoveOnlyTestArrayLength>>();
  AutoTArray<TypeParam, kMoveOnlyTestArrayLength> autoMoveOnlyArray;
  autoMoveOnlyArray = std::move(moveOnlyArray);

  ASSERT_EQ(0u, moveOnlyArray.Length());
  ASSERT_EQ(kMoveOnlyTestArrayLength, autoMoveOnlyArray.Length());
}

TYPED_TEST_P(TArray_MoveOnlyTest,
             nsTArray_to_AutoTArray_AutoStorage_MoveConstruct) {
  auto moveOnlyArray = MakeMoveOnlyArray<nsTArray<TypeParam>>();
  AutoTArray<TypeParam, kMoveOnlyTestArrayLength> autoMoveOnlyArray(
      std::move(moveOnlyArray));

  ASSERT_EQ(0u, moveOnlyArray.Length());
  ASSERT_EQ(kMoveOnlyTestArrayLength, autoMoveOnlyArray.Length());
}

TYPED_TEST_P(TArray_MoveOnlyTest,
             nsTArray_to_AutoTArray_AutoStorage_MoveAssign) {
  auto moveOnlyArray = MakeMoveOnlyArray<nsTArray<TypeParam>>();
  AutoTArray<TypeParam, kMoveOnlyTestArrayLength> autoMoveOnlyArray;
  autoMoveOnlyArray = std::move(moveOnlyArray);

  ASSERT_EQ(0u, moveOnlyArray.Length());
  ASSERT_EQ(kMoveOnlyTestArrayLength, autoMoveOnlyArray.Length());
}

TYPED_TEST_P(TArray_MoveOnlyTest,
             nsTArray_to_AutoTArray_HeapStorage_MoveConstruct) {
  auto moveOnlyArray = MakeMoveOnlyArray<nsTArray<TypeParam>>();
  AutoTArray<TypeParam, kMoveOnlyTestArrayLength - 1> autoMoveOnlyArray(
      std::move(moveOnlyArray));

  ASSERT_EQ(0u, moveOnlyArray.Length());
  ASSERT_EQ(kMoveOnlyTestArrayLength, autoMoveOnlyArray.Length());
}

TYPED_TEST_P(TArray_MoveOnlyTest,
             nsTArray_to_AutoTArray_HeapStorage_MoveAssign) {
  auto moveOnlyArray = MakeMoveOnlyArray<nsTArray<TypeParam>>();
  AutoTArray<TypeParam, kMoveOnlyTestArrayLength - 1> autoMoveOnlyArray;
  autoMoveOnlyArray = std::move(moveOnlyArray);

  ASSERT_EQ(0u, moveOnlyArray.Length());
  ASSERT_EQ(kMoveOnlyTestArrayLength, autoMoveOnlyArray.Length());
}

TYPED_TEST_P(TArray_MoveOnlyTest,
             FallibleTArray_to_AutoTArray_HeapStorage_MoveConstruct) {
  auto moveOnlyArray = MakeMoveOnlyArray<FallibleTArray<TypeParam>>();
  AutoTArray<TypeParam, 4> autoMoveOnlyArray(std::move(moveOnlyArray));

  ASSERT_EQ(0u, moveOnlyArray.Length());
  ASSERT_EQ(kMoveOnlyTestArrayLength, autoMoveOnlyArray.Length());
}

TYPED_TEST_P(TArray_MoveOnlyTest,
             FallibleTArray_to_AutoTArray_HeapStorage_MoveAssign) {
  auto moveOnlyArray = MakeMoveOnlyArray<FallibleTArray<TypeParam>>();
  AutoTArray<TypeParam, 4> autoMoveOnlyArray;
  autoMoveOnlyArray = std::move(moveOnlyArray);

  ASSERT_EQ(0u, moveOnlyArray.Length());
  ASSERT_EQ(kMoveOnlyTestArrayLength, autoMoveOnlyArray.Length());
}

REGISTER_TYPED_TEST_CASE_P(
    TArray_MoveOnlyTest, nsTArray_MoveConstruct, nsTArray_MoveAssign,
    nsTArray_MoveReAssign, nsTArray_to_FallibleTArray_MoveConstruct,
    nsTArray_to_FallibleTArray_MoveAssign,
    FallibleTArray_to_nsTArray_MoveConstruct,
    FallibleTArray_to_nsTArray_MoveAssign, AutoTArray_AutoStorage_MoveConstruct,
    AutoTArray_AutoStorage_MoveAssign,
    nsTArray_to_AutoTArray_AutoStorage_MoveConstruct,
    nsTArray_to_AutoTArray_AutoStorage_MoveAssign,
    nsTArray_to_AutoTArray_HeapStorage_MoveConstruct,
    nsTArray_to_AutoTArray_HeapStorage_MoveAssign,
    FallibleTArray_to_AutoTArray_HeapStorage_MoveConstruct,
    FallibleTArray_to_AutoTArray_HeapStorage_MoveAssign);

using BothMoveOnlyTypes =
    ::testing::Types<MoveOnly_RelocateUsingMemutils,
                     MoveOnly_RelocateUsingMoveConstructor>;
INSTANTIATE_TYPED_TEST_CASE_P(InstantiationOf, TArray_MoveOnlyTest,
                              BothMoveOnlyTypes);

//----

TEST(TArray, test_string_array)
{
  nsTArray<nsCString> strArray;
  const char kdata[] = "hello world";
  size_t i;
  for (i = 0; i < ArrayLength(kdata); ++i) {
    nsCString str;
    str.Assign(kdata[i]);
    ASSERT_TRUE(strArray.AppendElement(str));
  }
  for (i = 0; i < ArrayLength(kdata); ++i) {
    ASSERT_EQ(strArray[i].CharAt(0), kdata[i]);
  }

  const char kextra[] = "foo bar";
  size_t oldLen = strArray.Length();
  ASSERT_TRUE(strArray.AppendElement(kextra));
  strArray.RemoveElement(kextra);
  ASSERT_EQ(oldLen, strArray.Length());

  ASSERT_EQ(strArray.IndexOf("e"), size_t(1));
  ASSERT_TRUE(strArray.ApplyIf(
      "e", [](size_t i, nsCString& s) { return i == 1 && s == "e"; },
      []() { return false; }));

  strArray.Sort();
  const char ksorted[] = "\0 dehllloorw";
  for (i = ArrayLength(kdata); i--;) {
    ASSERT_EQ(strArray[i].CharAt(0), ksorted[i]);
    if (i > 0 && strArray[i] == strArray[i - 1]) strArray.RemoveElementAt(i);
  }
  for (i = 0; i < strArray.Length(); ++i) {
    ASSERT_EQ(strArray.BinaryIndexOf(strArray[i]), i);
  }
  auto no_index = strArray.NoIndex;  // Fixes gtest compilation error
  ASSERT_EQ(strArray.BinaryIndexOf(EmptyCString()), no_index);

  nsCString rawArray[MOZ_ARRAY_LENGTH(kdata) - 1];
  for (i = 0; i < ArrayLength(rawArray); ++i)
    rawArray[i].Assign(kdata + i);  // substrings of kdata

  ASSERT_TRUE(
      test_basic_array(rawArray, ArrayLength(rawArray), nsCString("foopy")));
}

//----

typedef nsCOMPtr<nsIFile> FilePointer;

class nsFileNameComparator {
 public:
  bool Equals(const FilePointer& a, const char* b) const {
    nsAutoCString name;
    a->GetNativeLeafName(name);
    return name.Equals(b);
  }
};

TEST(TArray, test_comptr_array)
{
  FilePointer tmpDir;
  NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tmpDir));
  ASSERT_TRUE(tmpDir);
  const char* kNames[] = {"foo.txt", "bar.html", "baz.gif"};
  nsTArray<FilePointer> fileArray;
  size_t i;
  for (i = 0; i < ArrayLength(kNames); ++i) {
    FilePointer f;
    tmpDir->Clone(getter_AddRefs(f));
    ASSERT_TRUE(f);
    ASSERT_FALSE(NS_FAILED(f->AppendNative(nsDependentCString(kNames[i]))));
    fileArray.AppendElement(f);
  }

  ASSERT_EQ(fileArray.IndexOf(kNames[1], 0, nsFileNameComparator()), size_t(1));
  ASSERT_TRUE(fileArray.ApplyIf(
      kNames[1], 0, nsFileNameComparator(), [](size_t i) { return i == 1; },
      []() { return false; }));

  // It's unclear what 'operator<' means for nsCOMPtr, but whatever...
  ASSERT_TRUE(
      test_basic_array(fileArray.Elements(), fileArray.Length(), tmpDir));
}

//----

class RefcountedObject {
 public:
  RefcountedObject() : rc(0) {}
  void AddRef() { ++rc; }
  void Release() {
    if (--rc == 0) delete this;
  }
  ~RefcountedObject() = default;

 private:
  int32_t rc;
};

TEST(TArray, test_refptr_array)
{
  nsTArray<RefPtr<RefcountedObject>> objArray;

  RefcountedObject* a = new RefcountedObject();
  a->AddRef();
  RefcountedObject* b = new RefcountedObject();
  b->AddRef();
  RefcountedObject* c = new RefcountedObject();
  c->AddRef();

  objArray.AppendElement(a);
  objArray.AppendElement(b);
  objArray.AppendElement(c);

  ASSERT_EQ(objArray.IndexOf(b), size_t(1));
  ASSERT_TRUE(objArray.ApplyIf(
      b,
      [&](size_t i, RefPtr<RefcountedObject>& r) { return i == 1 && r == b; },
      []() { return false; }));

  a->Release();
  b->Release();
  c->Release();
}

//----

TEST(TArray, test_ptrarray)
{
  nsTArray<uint32_t*> ary;
  ASSERT_EQ(ary.SafeElementAt(0), nullptr);
  ASSERT_EQ(ary.SafeElementAt(1000), nullptr);

  uint32_t a = 10;
  ary.AppendElement(&a);
  ASSERT_EQ(*ary[0], a);
  ASSERT_EQ(*ary.SafeElementAt(0), a);

  nsTArray<const uint32_t*> cary;
  ASSERT_EQ(cary.SafeElementAt(0), nullptr);
  ASSERT_EQ(cary.SafeElementAt(1000), nullptr);

  const uint32_t b = 14;
  cary.AppendElement(&a);
  cary.AppendElement(&b);
  ASSERT_EQ(*cary[0], a);
  ASSERT_EQ(*cary[1], b);
  ASSERT_EQ(*cary.SafeElementAt(0), a);
  ASSERT_EQ(*cary.SafeElementAt(1), b);
}

//----

// This test relies too heavily on the existence of DebugGetHeader to be
// useful in non-debug builds.
#ifdef DEBUG
TEST(TArray, test_autoarray)
{
  uint32_t data[] = {4, 6, 8, 2, 4, 1, 5, 7, 3};
  AutoTArray<uint32_t, MOZ_ARRAY_LENGTH(data)> array;

  void* hdr = array.DebugGetHeader();
  ASSERT_NE(hdr, nsTArray<uint32_t>().DebugGetHeader());
  ASSERT_NE(hdr,
            (AutoTArray<uint32_t, MOZ_ARRAY_LENGTH(data)>().DebugGetHeader()));

  array.AppendElement(1u);
  ASSERT_EQ(hdr, array.DebugGetHeader());

  array.RemoveElement(1u);
  array.AppendElements(data, ArrayLength(data));
  ASSERT_EQ(hdr, array.DebugGetHeader());

  array.AppendElement(2u);
  ASSERT_NE(hdr, array.DebugGetHeader());

  array.Clear();
  array.Compact();
  ASSERT_EQ(hdr, array.DebugGetHeader());
  array.AppendElements(data, ArrayLength(data));
  ASSERT_EQ(hdr, array.DebugGetHeader());

  nsTArray<uint32_t> array2;
  void* emptyHdr = array2.DebugGetHeader();
  array.SwapElements(array2);
  ASSERT_NE(emptyHdr, array.DebugGetHeader());
  ASSERT_NE(hdr, array2.DebugGetHeader());
  size_t i;
  for (i = 0; i < ArrayLength(data); ++i) {
    ASSERT_EQ(array2[i], data[i]);
  }
  ASSERT_TRUE(array.IsEmpty());

  array.Compact();
  array.AppendElements(data, ArrayLength(data));
  uint32_t data3[] = {5, 7, 11};
  AutoTArray<uint32_t, MOZ_ARRAY_LENGTH(data3)> array3;
  array3.AppendElements(data3, ArrayLength(data3));
  array.SwapElements(array3);
  for (i = 0; i < ArrayLength(data); ++i) {
    ASSERT_EQ(array3[i], data[i]);
  }
  for (i = 0; i < ArrayLength(data3); ++i) {
    ASSERT_EQ(array[i], data3[i]);
  }
}
#endif

//----

// IndexOf used to potentially scan beyond the end of the array.  Test for
// this incorrect behavior by adding a value (5), removing it, then seeing
// if IndexOf finds it.
TEST(TArray, test_indexof)
{
  nsTArray<int> array;
  array.AppendElement(0);
  // add and remove the 5
  array.AppendElement(5);
  array.RemoveElementAt(1);
  // we should not find the 5!
  auto no_index = array.NoIndex;  // Fixes gtest compilation error.
  ASSERT_EQ(array.IndexOf(5, 1), no_index);
  ASSERT_FALSE(array.ApplyIf(
      5, 1, []() { return true; }, []() { return false; }));
}

//----

template <class Array>
static bool is_heap(const Array& ary, size_t len) {
  size_t index = 1;
  while (index < len) {
    if (ary[index] > ary[(index - 1) >> 1]) return false;
    index++;
  }
  return true;
}

//----

// An array |arr| is using its auto buffer if |&arr < arr.Elements()| and
// |arr.Elements() - &arr| is small.

#define IS_USING_AUTO(arr)                            \
  ((uintptr_t) & (arr) < (uintptr_t)arr.Elements() && \
   ((ptrdiff_t)arr.Elements() - (ptrdiff_t)&arr) <= 16)

#define CHECK_IS_USING_AUTO(arr)     \
  do {                               \
    ASSERT_TRUE(IS_USING_AUTO(arr)); \
  } while (0)

#define CHECK_NOT_USING_AUTO(arr)     \
  do {                                \
    ASSERT_FALSE(IS_USING_AUTO(arr)); \
  } while (0)

#define CHECK_USES_SHARED_EMPTY_HDR(arr)          \
  do {                                            \
    nsTArray<int> _empty;                         \
    ASSERT_EQ(_empty.Elements(), arr.Elements()); \
  } while (0)

#define CHECK_EQ_INT(actual, expected) \
  do {                                 \
    ASSERT_EQ((actual), (expected));   \
  } while (0)

#define CHECK_ARRAY(arr, data)                               \
  do {                                                       \
    CHECK_EQ_INT((arr).Length(), (size_t)ArrayLength(data)); \
    for (size_t _i = 0; _i < ArrayLength(data); _i++) {      \
      CHECK_EQ_INT((arr)[_i], (data)[_i]);                   \
    }                                                        \
  } while (0)

TEST(TArray, test_swap)
{
  // Test nsTArray::SwapElements.  Unfortunately there are many cases.
  int data1[] = {8, 6, 7, 5};
  int data2[] = {3, 0, 9};

  // Swap two auto arrays.
  {
    AutoTArray<int, 8> a;
    AutoTArray<int, 6> b;

    a.AppendElements(data1, ArrayLength(data1));
    b.AppendElements(data2, ArrayLength(data2));
    CHECK_IS_USING_AUTO(a);
    CHECK_IS_USING_AUTO(b);

    a.SwapElements(b);

    CHECK_IS_USING_AUTO(a);
    CHECK_IS_USING_AUTO(b);
    CHECK_ARRAY(a, data2);
    CHECK_ARRAY(b, data1);
  }

  // Swap two auto arrays -- one whose data lives on the heap, the other whose
  // data lives on the stack -- which each fits into the other's auto storage.
  {
    AutoTArray<int, 3> a;
    AutoTArray<int, 3> b;

    a.AppendElements(data1, ArrayLength(data1));
    a.RemoveElementAt(3);
    b.AppendElements(data2, ArrayLength(data2));

    // Here and elsewhere, we assert that if we start with an auto array
    // capable of storing N elements, we store N+1 elements into the array, and
    // then we remove one element, that array is still not using its auto
    // buffer.
    //
    // This isn't at all required by the TArray API. It would be fine if, when
    // we shrink back to N elements, the TArray frees its heap storage and goes
    // back to using its stack storage.  But we assert here as a check that the
    // test does what we expect.  If the TArray implementation changes, just
    // change the failing assertions.
    CHECK_NOT_USING_AUTO(a);

    // This check had better not change, though.
    CHECK_IS_USING_AUTO(b);

    a.SwapElements(b);

    CHECK_IS_USING_AUTO(b);
    CHECK_ARRAY(a, data2);
    int expectedB[] = {8, 6, 7};
    CHECK_ARRAY(b, expectedB);
  }

  // Swap two auto arrays which are using heap storage such that one fits into
  // the other's auto storage, but the other needs to stay on the heap.
  {
    AutoTArray<int, 3> a;
    AutoTArray<int, 2> b;
    a.AppendElements(data1, ArrayLength(data1));
    a.RemoveElementAt(3);

    b.AppendElements(data2, ArrayLength(data2));
    b.RemoveElementAt(2);

    CHECK_NOT_USING_AUTO(a);
    CHECK_NOT_USING_AUTO(b);

    a.SwapElements(b);

    CHECK_NOT_USING_AUTO(b);

    int expected1[] = {3, 0};
    int expected2[] = {8, 6, 7};

    CHECK_ARRAY(a, expected1);
    CHECK_ARRAY(b, expected2);
  }

  // Swap two arrays, neither of which fits into the other's auto-storage.
  {
    AutoTArray<int, 1> a;
    AutoTArray<int, 3> b;

    a.AppendElements(data1, ArrayLength(data1));
    b.AppendElements(data2, ArrayLength(data2));

    a.SwapElements(b);

    CHECK_ARRAY(a, data2);
    CHECK_ARRAY(b, data1);
  }

  // Swap an empty nsTArray with a non-empty AutoTArray.
  {
    nsTArray<int> a;
    AutoTArray<int, 3> b;

    b.AppendElements(data2, ArrayLength(data2));
    CHECK_IS_USING_AUTO(b);

    a.SwapElements(b);

    CHECK_ARRAY(a, data2);
    CHECK_EQ_INT(b.Length(), size_t(0));
    CHECK_IS_USING_AUTO(b);
  }

  // Swap two big auto arrays.
  {
    const unsigned size = 8192;
    AutoTArray<unsigned, size> a;
    AutoTArray<unsigned, size> b;

    for (unsigned i = 0; i < size; i++) {
      a.AppendElement(i);
      b.AppendElement(i + 1);
    }

    CHECK_IS_USING_AUTO(a);
    CHECK_IS_USING_AUTO(b);

    a.SwapElements(b);

    CHECK_IS_USING_AUTO(a);
    CHECK_IS_USING_AUTO(b);

    CHECK_EQ_INT(a.Length(), size_t(size));
    CHECK_EQ_INT(b.Length(), size_t(size));

    for (unsigned i = 0; i < size; i++) {
      CHECK_EQ_INT(a[i], i + 1);
      CHECK_EQ_INT(b[i], i);
    }
  }

  // Swap two arrays and make sure that their capacities don't increase
  // unnecessarily.
  {
    nsTArray<int> a;
    nsTArray<int> b;
    b.AppendElements(data2, ArrayLength(data2));

    CHECK_EQ_INT(a.Capacity(), size_t(0));
    size_t bCapacity = b.Capacity();

    a.SwapElements(b);

    // Make sure that we didn't increase the capacity of either array.
    CHECK_ARRAY(a, data2);
    CHECK_EQ_INT(b.Length(), size_t(0));
    CHECK_EQ_INT(b.Capacity(), size_t(0));
    CHECK_EQ_INT(a.Capacity(), bCapacity);
  }

  // Swap an auto array with a TArray, then clear the auto array and make sure
  // it doesn't forget the fact that it has an auto buffer.
  {
    nsTArray<int> a;
    AutoTArray<int, 3> b;

    a.AppendElements(data1, ArrayLength(data1));

    a.SwapElements(b);

    CHECK_EQ_INT(a.Length(), size_t(0));
    CHECK_ARRAY(b, data1);

    b.Clear();

    CHECK_USES_SHARED_EMPTY_HDR(a);
    CHECK_IS_USING_AUTO(b);
  }

  // Same thing as the previous test, but with more auto arrays.
  {
    AutoTArray<int, 16> a;
    AutoTArray<int, 3> b;

    a.AppendElements(data1, ArrayLength(data1));

    a.SwapElements(b);

    CHECK_EQ_INT(a.Length(), size_t(0));
    CHECK_ARRAY(b, data1);

    b.Clear();

    CHECK_IS_USING_AUTO(a);
    CHECK_IS_USING_AUTO(b);
  }

  // Swap an empty nsTArray and an empty AutoTArray.
  {
    AutoTArray<int, 8> a;
    nsTArray<int> b;

    a.SwapElements(b);

    CHECK_IS_USING_AUTO(a);
    CHECK_NOT_USING_AUTO(b);
    CHECK_EQ_INT(a.Length(), size_t(0));
    CHECK_EQ_INT(b.Length(), size_t(0));
  }

  // Swap empty auto array with non-empty AutoTArray using malloc'ed storage.
  // I promise, all these tests have a point.
  {
    AutoTArray<int, 2> a;
    AutoTArray<int, 1> b;

    a.AppendElements(data1, ArrayLength(data1));

    a.SwapElements(b);

    CHECK_IS_USING_AUTO(a);
    CHECK_NOT_USING_AUTO(b);
    CHECK_ARRAY(b, data1);
    CHECK_EQ_INT(a.Length(), size_t(0));
  }
}

// Bug 1171296: Disabled on andoid due to crashes.
#if !defined(ANDROID)
TEST(TArray, test_fallible)
{
  // Test that FallibleTArray works properly; that is, it never OOMs, but
  // instead eventually returns false.
  //
  // This test is only meaningful on 32-bit systems.  On a 64-bit system, we
  // might never OOM.
  if (sizeof(void*) > 4) {
    ASSERT_TRUE(true);
    return;
  }

  // Allocate a bunch of 128MB arrays.  Larger allocations will fail on some
  // platforms without actually hitting OOM.
  //
  // 36 * 128MB > 4GB, so we should definitely OOM by the 36th array.
  const unsigned numArrays = 36;
  FallibleTArray<char> arrays[numArrays];
  bool oomed = false;
  for (size_t i = 0; i < numArrays; i++) {
    // SetCapacity allocates the requested capacity + a header, and we want to
    // avoid allocating more than 128MB overall because of the size padding it
    // will cause, which depends on allocator behavior, so use 128MB - an
    // arbitrary size larger than the array header, so that chances are good
    // that allocations will always be 128MB.
    bool success = arrays[i].SetCapacity(128 * 1024 * 1024 - 1024, fallible);
    if (!success) {
      // We got our OOM.  Check that it didn't come too early.
      oomed = true;
#  ifdef XP_WIN
      // 32-bit Windows sometimes OOMs on the 6th, 7th, or 8th.  To keep the
      // test green, choose the lower of those: the important thing here is
      // that some allocations fail and some succeed.  We're not too
      // concerned about how many iterations it takes.
      const size_t kOOMIterations = 6;
#  else
      const size_t kOOMIterations = 8;
#  endif
      ASSERT_GE(i, kOOMIterations)
          << "Got OOM on iteration " << i << ". Too early!";
    }
  }

  ASSERT_TRUE(oomed)
  << "Didn't OOM or crash?  nsTArray::SetCapacity"
     "must be lying.";
}
#endif

TEST(TArray, test_conversion_operator)
{
  FallibleTArray<int> f;
  const FallibleTArray<int> fconst;

  nsTArray<int> t;
  const nsTArray<int> tconst;
  AutoTArray<int, 8> tauto;
  const AutoTArray<int, 8> tautoconst;

#define CHECK_ARRAY_CAST(type)                  \
  do {                                          \
    const type<int>& z1 = f;                    \
    ASSERT_EQ((void*)&z1, (void*)&f);           \
    const type<int>& z2 = fconst;               \
    ASSERT_EQ((void*)&z2, (void*)&fconst);      \
    const type<int>& z9 = t;                    \
    ASSERT_EQ((void*)&z9, (void*)&t);           \
    const type<int>& z10 = tconst;              \
    ASSERT_EQ((void*)&z10, (void*)&tconst);     \
    const type<int>& z11 = tauto;               \
    ASSERT_EQ((void*)&z11, (void*)&tauto);      \
    const type<int>& z12 = tautoconst;          \
    ASSERT_EQ((void*)&z12, (void*)&tautoconst); \
  } while (0)

  CHECK_ARRAY_CAST(FallibleTArray);
  CHECK_ARRAY_CAST(nsTArray);

#undef CHECK_ARRAY_CAST
}

template <class T>
struct BufAccessor : public T {
  void* GetHdr() { return T::mHdr; }
};

TEST(TArray, test_SetLengthAndRetainStorage_no_ctor)
{
  // 1050 because sizeof(int)*1050 is more than a page typically.
  const int N = 1050;
  FallibleTArray<int> f;

  nsTArray<int> t;
  AutoTArray<int, N> tauto;

#define LPAREN (
#define RPAREN )
#define FOR_EACH(pre, post) \
  do {                      \
    pre f post;             \
    pre t post;             \
    pre tauto post;         \
  } while (0)

  // Setup test arrays.
  FOR_EACH(; Unused <<, .SetLength(N, fallible));
  for (int n = 0; n < N; ++n) {
    FOR_EACH(;, [n] = n);
  }

  void* initial_Hdrs[] = {
      static_cast<BufAccessor<FallibleTArray<int>>&>(f).GetHdr(),
      static_cast<BufAccessor<nsTArray<int>>&>(t).GetHdr(),
      static_cast<BufAccessor<AutoTArray<int, N>>&>(tauto).GetHdr(), nullptr};

  // SetLengthAndRetainStorage(n), should NOT overwrite memory when T hasn't
  // a default constructor.
  FOR_EACH(;, .SetLengthAndRetainStorage(8));
  FOR_EACH(;, .SetLengthAndRetainStorage(12));
  for (int n = 0; n < 12; ++n) {
    ASSERT_EQ(f[n], n);
    ASSERT_EQ(t[n], n);
    ASSERT_EQ(tauto[n], n);
  }
  FOR_EACH(;, .SetLengthAndRetainStorage(0));
  FOR_EACH(;, .SetLengthAndRetainStorage(N));
  for (int n = 0; n < N; ++n) {
    ASSERT_EQ(f[n], n);
    ASSERT_EQ(t[n], n);
    ASSERT_EQ(tauto[n], n);
  }

  void* current_Hdrs[] = {
      static_cast<BufAccessor<FallibleTArray<int>>&>(f).GetHdr(),
      static_cast<BufAccessor<nsTArray<int>>&>(t).GetHdr(),
      static_cast<BufAccessor<AutoTArray<int, N>>&>(tauto).GetHdr(), nullptr};

  // SetLengthAndRetainStorage(n) should NOT have reallocated the internal
  // memory.
  ASSERT_EQ(sizeof(initial_Hdrs), sizeof(current_Hdrs));
  for (size_t n = 0; n < sizeof(current_Hdrs) / sizeof(current_Hdrs[0]); ++n) {
    ASSERT_EQ(current_Hdrs[n], initial_Hdrs[n]);
  }

#undef FOR_EACH
#undef LPAREN
#undef RPAREN
}

template <typename Comparator>
bool TestCompareMethods(const Comparator& aComp) {
  nsTArray<int> ary({57, 4, 16, 17, 3, 5, 96, 12});

  ary.Sort(aComp);

  const int sorted[] = {3, 4, 5, 12, 16, 17, 57, 96};
  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(sorted); i++) {
    if (sorted[i] != ary[i]) {
      return false;
    }
  }

  if (!ary.ContainsSorted(5, aComp)) {
    return false;
  }
  if (ary.ContainsSorted(42, aComp)) {
    return false;
  }

  if (ary.BinaryIndexOf(16, aComp) != 4) {
    return false;
  }

  return true;
}

struct IntComparator {
  bool Equals(int aLeft, int aRight) const { return aLeft == aRight; }

  bool LessThan(int aLeft, int aRight) const { return aLeft < aRight; }
};

TEST(TArray, test_comparator_objects)
{
  ASSERT_TRUE(TestCompareMethods(IntComparator()));
  ASSERT_TRUE(
      TestCompareMethods([](int aLeft, int aRight) { return aLeft - aRight; }));
}

struct Big {
  uint64_t size[40] = {};
};

TEST(TArray, test_AutoTArray_SwapElements)
{
  AutoTArray<Big, 40> oneArray;
  AutoTArray<Big, 40> another;

  for (size_t i = 0; i < 8; ++i) {
    oneArray.AppendElement(Big());
  }
  oneArray[0].size[10] = 1;
  for (size_t i = 0; i < 9; ++i) {
    another.AppendElement(Big());
  }
  oneArray.SwapElements(another);

  ASSERT_EQ(oneArray.Length(), 9u);
  ASSERT_EQ(another.Length(), 8u);

  ASSERT_EQ(oneArray[0].size[10], 0u);
  ASSERT_EQ(another[0].size[10], 1u);
}

}  // namespace TestTArray
