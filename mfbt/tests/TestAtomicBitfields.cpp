/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/AtomicBitfields.h"

// This is a big macro mess, so let's summarize what's in here right up front:
//
// |TestDocumentationExample| is intended to be a copy-paste of the example
// in the macro's documentation, to make sure it's correct.
//
//
// |TestJammedWithFlags| tests using every bit of the type for bool flags.
// 64-bit isn't tested due to macro limitations.
//
//
// |TestLopsided| tests an instance with the following configuration:
//
// * a 1-bit boolean
// * an (N-1)-bit uintN_t
//
// It tests both orderings of these fields.
//
// Hopefully these are enough to cover all the nasty boundary conditions
// (that still compile).

// ==================== TestDocumentationExample ========================

struct MyType {
  MOZ_ATOMIC_BITFIELDS(mAtomicFields, 8,
                       ((bool, IsDownloaded, 1), (uint32_t, SomeData, 2),
                        (uint8_t, OtherData, 5)))

  int32_t aNormalInteger;

  explicit MyType(uint32_t aSomeData) : aNormalInteger(7) {
    SetSomeData(aSomeData);
    // Other bitfields were already default initialized to 0/false
  }
};

void TestDocumentationExample() {
  MyType val(3);

  if (!val.GetIsDownloaded()) {
    val.SetOtherData(2);
    val.SetIsDownloaded(true);
  }
}

// ====================== TestJammedWithFlags =========================

#define TIMES_8(aFunc, aSeparator, aArgs) \
  MOZ_FOR_EACH_SEPARATED(aFunc, aSeparator, aArgs, (1, 2, 3, 4, 5, 6, 7, 8))
#define TIMES_16(aFunc, aSeparator, aArgs) \
  MOZ_FOR_EACH_SEPARATED(                  \
      aFunc, aSeparator, aArgs,            \
      (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16))
#define TIMES_32(aFunc, aSeparator, aArgs)                                    \
  MOZ_FOR_EACH_SEPARATED(                                                     \
      aFunc, aSeparator, aArgs,                                               \
      (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, \
       21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32))

#define GENERATE_BOOL(aIndex) (bool, Flag##aIndex, 1)

#define CHECK_BOOL(aIndex)                    \
  MOZ_ASSERT(val.GetFlag##aIndex() == false); \
  val.SetFlag##aIndex(true);                  \
  MOZ_ASSERT(val.GetFlag##aIndex() == true);  \
  val.SetFlag##aIndex(false);                 \
  MOZ_ASSERT(val.GetFlag##aIndex() == false);

#define GENERATE_TEST_JAMMED_WITH_FLAGS(aSize)                     \
  struct JammedWithFlags##aSize {                                  \
    MOZ_ATOMIC_BITFIELDS(mAtomicFields, aSize,                     \
                         (TIMES_##aSize(GENERATE_BOOL, (, ), ()))) \
  };                                                               \
  void TestJammedWithFlags##aSize() {                              \
    JammedWithFlags##aSize val;                                    \
    TIMES_##aSize(CHECK_BOOL, (;), ());                            \
  }

#define TEST_JAMMED_WITH_FLAGS(aSize) TestJammedWithFlags##aSize();

// ========================= TestLopsided ===========================

#define GENERATE_TEST_LOPSIDED_FUNC(aSide, aSize)          \
  void TestLopsided##aSide##aSize() {                      \
    Lopsided##aSide##aSize val;                            \
    MOZ_ASSERT(val.GetHappyLittleBit() == false);          \
    MOZ_ASSERT(val.GetLargeAndInCharge() == 0);            \
    val.SetHappyLittleBit(true);                           \
    MOZ_ASSERT(val.GetHappyLittleBit() == true);           \
    MOZ_ASSERT(val.GetLargeAndInCharge() == 0);            \
    val.SetLargeAndInCharge(1);                            \
    MOZ_ASSERT(val.GetHappyLittleBit() == true);           \
    MOZ_ASSERT(val.GetLargeAndInCharge() == 1);            \
    val.SetLargeAndInCharge(0);                            \
    MOZ_ASSERT(val.GetHappyLittleBit() == true);           \
    MOZ_ASSERT(val.GetLargeAndInCharge() == 0);            \
    uint##aSize##_t size = aSize;                          \
    uint##aSize##_t int_max = (~(1ull << (size - 1))) - 1; \
    val.SetLargeAndInCharge(int_max);                      \
    MOZ_ASSERT(val.GetHappyLittleBit() == true);           \
    MOZ_ASSERT(val.GetLargeAndInCharge() == int_max);      \
    val.SetHappyLittleBit(false);                          \
    MOZ_ASSERT(val.GetHappyLittleBit() == false);          \
    MOZ_ASSERT(val.GetLargeAndInCharge() == int_max);      \
    val.SetLargeAndInCharge(int_max);                      \
    MOZ_ASSERT(val.GetHappyLittleBit() == false);          \
    MOZ_ASSERT(val.GetLargeAndInCharge() == int_max);      \
  }

#define GENERATE_TEST_LOPSIDED(aSize)                                        \
  struct LopsidedA##aSize {                                                  \
    MOZ_ATOMIC_BITFIELDS(mAtomicFields, aSize,                               \
                         ((bool, HappyLittleBit, 1),                         \
                          (uint##aSize##_t, LargeAndInCharge, ((aSize)-1)))) \
  };                                                                         \
  struct LopsidedB##aSize {                                                  \
    MOZ_ATOMIC_BITFIELDS(mAtomicFields, aSize,                               \
                         ((uint##aSize##_t, LargeAndInCharge, ((aSize)-1)),  \
                          (bool, HappyLittleBit, 1)))                        \
  };                                                                         \
  GENERATE_TEST_LOPSIDED_FUNC(A, aSize);                                     \
  GENERATE_TEST_LOPSIDED_FUNC(B, aSize);

#define TEST_LOPSIDED(aSize) \
  TestLopsidedA##aSize();    \
  TestLopsidedB##aSize();

// ==================== generate and run the tests ======================

GENERATE_TEST_JAMMED_WITH_FLAGS(8)
GENERATE_TEST_JAMMED_WITH_FLAGS(16)
GENERATE_TEST_JAMMED_WITH_FLAGS(32)
// MOZ_FOR_EACH_64 doesn't exist :(

GENERATE_TEST_LOPSIDED(8)
GENERATE_TEST_LOPSIDED(16)
GENERATE_TEST_LOPSIDED(32)
GENERATE_TEST_LOPSIDED(64)

int main() {
  TestDocumentationExample();

  TEST_JAMMED_WITH_FLAGS(8);
  TEST_JAMMED_WITH_FLAGS(16);
  TEST_JAMMED_WITH_FLAGS(32);

  TEST_LOPSIDED(8);
  TEST_LOPSIDED(16);
  TEST_LOPSIDED(32);
  TEST_LOPSIDED(64);
  return 0;
}