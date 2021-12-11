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
    StoreSomeData(aSomeData);
    // Other bitfields were already default initialized to 0/false
  }
};

void TestDocumentationExample() {
  MyType val(3);

  if (!val.LoadIsDownloaded()) {
    val.StoreOtherData(2);
    val.StoreIsDownloaded(true);
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

#define CHECK_BOOL(aIndex)                     \
  MOZ_ASSERT(val.LoadFlag##aIndex() == false); \
  val.StoreFlag##aIndex(true);                 \
  MOZ_ASSERT(val.LoadFlag##aIndex() == true);  \
  val.StoreFlag##aIndex(false);                \
  MOZ_ASSERT(val.LoadFlag##aIndex() == false);

#define GENERATE_TEST_JAMMED_WITH_FLAGS(aSize) \
  void TestJammedWithFlags##aSize() {          \
    JammedWithFlags##aSize val;                \
    TIMES_##aSize(CHECK_BOOL, (;), ());        \
  }

#define TEST_JAMMED_WITH_FLAGS(aSize) TestJammedWithFlags##aSize();

// ========================= TestLopsided ===========================

#define GENERATE_TEST_LOPSIDED_FUNC(aSide, aSize)          \
  void TestLopsided##aSide##aSize() {                      \
    Lopsided##aSide##aSize val;                            \
    MOZ_ASSERT(val.LoadHappyLittleBit() == false);         \
    MOZ_ASSERT(val.LoadLargeAndInCharge() == 0);           \
    val.StoreHappyLittleBit(true);                         \
    MOZ_ASSERT(val.LoadHappyLittleBit() == true);          \
    MOZ_ASSERT(val.LoadLargeAndInCharge() == 0);           \
    val.StoreLargeAndInCharge(1);                          \
    MOZ_ASSERT(val.LoadHappyLittleBit() == true);          \
    MOZ_ASSERT(val.LoadLargeAndInCharge() == 1);           \
    val.StoreLargeAndInCharge(0);                          \
    MOZ_ASSERT(val.LoadHappyLittleBit() == true);          \
    MOZ_ASSERT(val.LoadLargeAndInCharge() == 0);           \
    uint##aSize##_t size = aSize;                          \
    uint##aSize##_t int_max = (~(1ull << (size - 1))) - 1; \
    val.StoreLargeAndInCharge(int_max);                    \
    MOZ_ASSERT(val.LoadHappyLittleBit() == true);          \
    MOZ_ASSERT(val.LoadLargeAndInCharge() == int_max);     \
    val.StoreHappyLittleBit(false);                        \
    MOZ_ASSERT(val.LoadHappyLittleBit() == false);         \
    MOZ_ASSERT(val.LoadLargeAndInCharge() == int_max);     \
    val.StoreLargeAndInCharge(int_max);                    \
    MOZ_ASSERT(val.LoadHappyLittleBit() == false);         \
    MOZ_ASSERT(val.LoadLargeAndInCharge() == int_max);     \
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

// There's an unknown bug in clang-cl-9 (used for win64-ccov) that makes
// generating these with the TIMES_N macro not work. So these are written out
// explicitly to unbork CI.
struct JammedWithFlags8 {
  MOZ_ATOMIC_BITFIELDS(mAtomicFields, 8,
                       ((bool, Flag1, 1), (bool, Flag2, 1), (bool, Flag3, 1),
                        (bool, Flag4, 1), (bool, Flag5, 1), (bool, Flag6, 1),
                        (bool, Flag7, 1), (bool, Flag8, 1)))
};

struct JammedWithFlags16 {
  MOZ_ATOMIC_BITFIELDS(mAtomicFields, 16,
                       ((bool, Flag1, 1), (bool, Flag2, 1), (bool, Flag3, 1),
                        (bool, Flag4, 1), (bool, Flag5, 1), (bool, Flag6, 1),
                        (bool, Flag7, 1), (bool, Flag8, 1), (bool, Flag9, 1),
                        (bool, Flag10, 1), (bool, Flag11, 1), (bool, Flag12, 1),
                        (bool, Flag13, 1), (bool, Flag14, 1), (bool, Flag15, 1),
                        (bool, Flag16, 1)))
};

struct JammedWithFlags32 {
  MOZ_ATOMIC_BITFIELDS(mAtomicFields, 32,
                       ((bool, Flag1, 1), (bool, Flag2, 1), (bool, Flag3, 1),
                        (bool, Flag4, 1), (bool, Flag5, 1), (bool, Flag6, 1),
                        (bool, Flag7, 1), (bool, Flag8, 1), (bool, Flag9, 1),
                        (bool, Flag10, 1), (bool, Flag11, 1), (bool, Flag12, 1),
                        (bool, Flag13, 1), (bool, Flag14, 1), (bool, Flag15, 1),
                        (bool, Flag16, 1), (bool, Flag17, 1), (bool, Flag18, 1),
                        (bool, Flag19, 1), (bool, Flag20, 1), (bool, Flag21, 1),
                        (bool, Flag22, 1), (bool, Flag23, 1), (bool, Flag24, 1),
                        (bool, Flag25, 1), (bool, Flag26, 1), (bool, Flag27, 1),
                        (bool, Flag28, 1), (bool, Flag29, 1), (bool, Flag30, 1),
                        (bool, Flag31, 1), (bool, Flag32, 1)))
};

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
