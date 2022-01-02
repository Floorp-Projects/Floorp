/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/WrappingOperations.h"

#include <stdint.h>

using mozilla::WrappingAdd;
using mozilla::WrappingMultiply;
using mozilla::WrappingSubtract;
using mozilla::WrapToSigned;

// NOTE: In places below |-FOO_MAX - 1| is used instead of |-FOO_MIN| because
//       in C++ numeric literals are full expressions -- the |-| in a negative
//       number is technically separate.  So with most compilers that limit
//       |int| to the signed 32-bit range, something like |-2147483648| is
//       operator-() applied to an *unsigned* expression.  And MSVC, at least,
//       warns when you do that.  (The operation is well-defined, but it likely
//       doesn't do what was intended.)  So we do the usual workaround for this
//       (see your local copy of <stdint.h> for a likely demo of this), writing
//       it out by negating the max value and subtracting 1.

static_assert(WrapToSigned(uint8_t(17)) == 17,
              "no wraparound should work, 8-bit");
static_assert(WrapToSigned(uint8_t(128)) == -128,
              "works for 8-bit numbers, wraparound low end");
static_assert(WrapToSigned(uint8_t(128 + 7)) == -128 + 7,
              "works for 8-bit numbers, wraparound mid");
static_assert(WrapToSigned(uint8_t(128 + 127)) == -128 + 127,
              "works for 8-bit numbers, wraparound high end");

static_assert(WrapToSigned(uint16_t(12345)) == 12345,
              "no wraparound should work, 16-bit");
static_assert(WrapToSigned(uint16_t(32768)) == -32768,
              "works for 16-bit numbers, wraparound low end");
static_assert(WrapToSigned(uint16_t(32768 + 42)) == -32768 + 42,
              "works for 16-bit numbers, wraparound mid");
static_assert(WrapToSigned(uint16_t(32768 + 32767)) == -32768 + 32767,
              "works for 16-bit numbers, wraparound high end");

static_assert(WrapToSigned(uint32_t(8675309)) == 8675309,
              "no wraparound should work, 32-bit");
static_assert(WrapToSigned(uint32_t(2147483648)) == -2147483647 - 1,
              "works for 32-bit numbers, wraparound low end");
static_assert(WrapToSigned(uint32_t(2147483648 + 42)) == -2147483647 - 1 + 42,
              "works for 32-bit numbers, wraparound mid");
static_assert(WrapToSigned(uint32_t(2147483648 + 2147483647)) ==
                  -2147483647 - 1 + 2147483647,
              "works for 32-bit numbers, wraparound high end");

static_assert(WrapToSigned(uint64_t(4152739164)) == 4152739164,
              "no wraparound should work, 64-bit");
static_assert(WrapToSigned(uint64_t(9223372036854775808ULL)) ==
                  -9223372036854775807LL - 1,
              "works for 64-bit numbers, wraparound low end");
static_assert(WrapToSigned(uint64_t(9223372036854775808ULL + 8005552368LL)) ==
                  -9223372036854775807LL - 1 + 8005552368LL,
              "works for 64-bit numbers, wraparound mid");
static_assert(WrapToSigned(uint64_t(9223372036854775808ULL +
                                    9223372036854775807ULL)) ==
                  -9223372036854775807LL - 1 + 9223372036854775807LL,
              "works for 64-bit numbers, wraparound high end");

template <typename T>
inline constexpr bool TestEqual(T aX, T aY) {
  return aX == aY;
}

static void TestWrappingAdd8() {
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(uint8_t(0), uint8_t(128)), uint8_t(128)),
      "zero plus anything is anything");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(uint8_t(17), uint8_t(42)), uint8_t(59)),
      "17 + 42 == 59");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(uint8_t(255), uint8_t(1)), uint8_t(0)),
      "all bits plus one overflows to zero");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(uint8_t(128), uint8_t(127)), uint8_t(255)),
      "high bit plus all lower bits is all bits");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(uint8_t(128), uint8_t(193)), uint8_t(65)),
      "128 + 193 is 256 + 65");

  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(int8_t(0), int8_t(-128)), int8_t(-128)),
      "zero plus anything is anything");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(int8_t(123), int8_t(8)), int8_t(-125)),
      "overflow to negative");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(int8_t(5), int8_t(-123)), int8_t(-118)),
      "5 - 123 is -118");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(int8_t(-85), int8_t(-73)), int8_t(98)),
      "underflow to positive");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(int8_t(-128), int8_t(127)), int8_t(-1)),
      "high bit plus all lower bits is -1");
}

static void TestWrappingAdd16() {
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(uint16_t(0), uint16_t(32768)), uint16_t(32768)),
      "zero plus anything is anything");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(uint16_t(24389), uint16_t(2682)), uint16_t(27071)),
      "24389 + 2682 == 27071");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(uint16_t(65535), uint16_t(1)), uint16_t(0)),
      "all bits plus one overflows to zero");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(uint16_t(32768), uint16_t(32767)), uint16_t(65535)),
      "high bit plus all lower bits is all bits");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(uint16_t(32768), uint16_t(47582)), uint16_t(14814)),
      "32768 + 47582 is 65536 + 14814");

  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(int16_t(0), int16_t(-32768)), int16_t(-32768)),
      "zero plus anything is anything");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(int16_t(32765), int16_t(8)), int16_t(-32763)),
      "overflow to negative");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(int16_t(5), int16_t(-28933)), int16_t(-28928)),
      "5 - 28933 is -28928");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(int16_t(-23892), int16_t(-12893)), int16_t(28751)),
      "underflow to positive");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(int16_t(-32768), int16_t(32767)), int16_t(-1)),
      "high bit plus all lower bits is -1");
}

static void TestWrappingAdd32() {
  MOZ_RELEASE_ASSERT(TestEqual(WrappingAdd(uint32_t(0), uint32_t(2147483648)),
                               uint32_t(2147483648)),
                     "zero plus anything is anything");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(uint32_t(1398742328), uint32_t(714192829)),
                uint32_t(2112935157)),
      "1398742328 + 714192829 == 2112935157");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(uint32_t(4294967295), uint32_t(1)), uint32_t(0)),
      "all bits plus one overflows to zero");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(uint32_t(2147483648), uint32_t(2147483647)),
                uint32_t(4294967295)),
      "high bit plus all lower bits is all bits");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(uint32_t(2147483648), uint32_t(3146492712)),
                uint32_t(999009064)),
      "2147483648 + 3146492712 is 4294967296 + 999009064");

  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(int32_t(0), int32_t(-2147483647 - 1)),
                int32_t(-2147483647 - 1)),
      "zero plus anything is anything");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingAdd(int32_t(2147483645), int32_t(8)),
                               int32_t(-2147483643)),
                     "overflow to negative");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingAdd(int32_t(257), int32_t(-23947248)),
                               int32_t(-23946991)),
                     "257 - 23947248 is -23946991");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(int32_t(-2147483220), int32_t(-12893)),
                int32_t(2147471183)),
      "underflow to positive");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(int32_t(-32768), int32_t(32767)), int32_t(-1)),
      "high bit plus all lower bits is -1");
}

static void TestWrappingAdd64() {
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(uint64_t(0), uint64_t(9223372036854775808ULL)),
                uint64_t(9223372036854775808ULL)),
      "zero plus anything is anything");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(uint64_t(70368744177664), uint64_t(3740873592)),
                uint64_t(70372485051256)),
      "70368744177664 + 3740873592 == 70372485051256");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(uint64_t(18446744073709551615ULL), uint64_t(1)),
                uint64_t(0)),
      "all bits plus one overflows to zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingAdd(uint64_t(9223372036854775808ULL),
                                           uint64_t(9223372036854775807ULL)),
                               uint64_t(18446744073709551615ULL)),
                     "high bit plus all lower bits is all bits");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingAdd(uint64_t(14552598638644786479ULL),
                                           uint64_t(3894174382537247221ULL)),
                               uint64_t(28947472482084)),
                     "9223372036854775808 + 3146492712 is 18446744073709551616 "
                     "+ 28947472482084");

  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(int64_t(0), int64_t(-9223372036854775807LL - 1)),
                int64_t(-9223372036854775807LL - 1)),
      "zero plus anything is anything");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(int64_t(9223372036854775802LL), int64_t(8)),
                int64_t(-9223372036854775806LL)),
      "overflow to negative");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingAdd(int64_t(37482739294298742LL),
                            int64_t(-437843573929483498LL)),
                int64_t(-400360834635184756LL)),
      "37482739294298742 - 437843573929483498 is -400360834635184756");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingAdd(int64_t(-9127837934058953374LL),
                                           int64_t(-4173572032144775807LL)),
                               int64_t(5145334107505822435L)),
                     "underflow to positive");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingAdd(int64_t(-9223372036854775807LL - 1),
                                           int64_t(9223372036854775807LL)),
                               int64_t(-1)),
                     "high bit plus all lower bits is -1");
}

static void TestWrappingAdd() {
  TestWrappingAdd8();
  TestWrappingAdd16();
  TestWrappingAdd32();
  TestWrappingAdd64();
}

static void TestWrappingSubtract8() {
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(uint8_t(0), uint8_t(128)), uint8_t(128)),
      "zero minus half is half");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(uint8_t(17), uint8_t(42)), uint8_t(231)),
      "17 - 42 == -25 added to 256 is 231");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(uint8_t(0), uint8_t(1)), uint8_t(255)),
      "zero underflows to all bits");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(uint8_t(128), uint8_t(127)), uint8_t(1)),
      "128 - 127 == 1");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(uint8_t(128), uint8_t(193)), uint8_t(191)),
      "128 - 193 is -65 so -65 + 256 == 191");

  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int8_t(0), int8_t(-128)), int8_t(-128)),
      "zero minus high bit wraps to high bit");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int8_t(-126), int8_t(4)), int8_t(126)),
      "underflow to positive");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int8_t(5), int8_t(-123)), int8_t(-128)),
      "overflow to negative");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int8_t(-85), int8_t(-73)), int8_t(-12)),
      "negative minus smaller negative");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int8_t(-128), int8_t(127)), int8_t(1)),
      "underflow to 1");
}

static void TestWrappingSubtract16() {
  MOZ_RELEASE_ASSERT(TestEqual(WrappingSubtract(uint16_t(0), uint16_t(32768)),
                               uint16_t(32768)),
                     "zero minus half is half");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(uint16_t(24389), uint16_t(2682)),
                uint16_t(21707)),
      "24389 - 2682 == 21707");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(uint16_t(0), uint16_t(1)), uint16_t(65535)),
      "zero underflows to all bits");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(uint16_t(32768), uint16_t(32767)),
                uint16_t(1)),
      "high bit minus all lower bits is one");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(uint16_t(32768), uint16_t(47582)),
                uint16_t(50722)),
      "32768 - 47582 + 65536 is 50722");

  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int16_t(0), int16_t(-32768)), int16_t(-32768)),
      "zero minus high bit wraps to high bit");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int16_t(-32766), int16_t(4)), int16_t(32766)),
      "underflow to positive");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int16_t(5), int16_t(-28933)), int16_t(28938)),
      "5 - -28933 is 28938");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int16_t(-23892), int16_t(-12893)),
                int16_t(-10999)),
      "negative minus smaller negative");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int16_t(-32768), int16_t(32767)), int16_t(1)),
      "underflow to 1");
}

static void TestWrappingSubtract32() {
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(uint32_t(0), uint32_t(2147483648)),
                uint32_t(2147483648)),
      "zero minus half is half");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(uint32_t(1398742328), uint32_t(714192829)),
                uint32_t(684549499)),
      "1398742328 - 714192829 == 684549499");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingSubtract(uint32_t(0), uint32_t(1)),
                               uint32_t(4294967295)),
                     "zero underflows to all bits");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(uint32_t(2147483648), uint32_t(2147483647)),
                uint32_t(1)),
      "high bit minus all lower bits is one");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(uint32_t(2147483648), uint32_t(3146492712)),
                uint32_t(3295958232)),
      "2147483648 - 3146492712 + 4294967296 is 3295958232");

  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int32_t(0), int32_t(-2147483647 - 1)),
                int32_t(-2147483647 - 1)),
      "zero minus high bit wraps to high bit");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int32_t(-2147483646), int32_t(4)),
                int32_t(2147483646)),
      "underflow to positive");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int32_t(257), int32_t(-23947248)),
                int32_t(23947505)),
      "257 - -23947248 is 23947505");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int32_t(-2147483220), int32_t(-12893)),
                int32_t(-2147470327)),
      "negative minus smaller negative");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int32_t(-2147483647 - 1), int32_t(2147483647)),
                int32_t(1)),
      "underflow to 1");
}

static void TestWrappingSubtract64() {
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(uint64_t(0), uint64_t(9223372036854775808ULL)),
                uint64_t(9223372036854775808ULL)),
      "zero minus half is half");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingSubtract(uint64_t(70368744177664),
                                                uint64_t(3740873592)),
                               uint64_t(70365003304072)),
                     "70368744177664 - 3740873592 == 70365003304072");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingSubtract(uint64_t(0), uint64_t(1)),
                               uint64_t(18446744073709551615ULL)),
                     "zero underflows to all bits");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(uint64_t(9223372036854775808ULL),
                                 uint64_t(9223372036854775807ULL)),
                uint64_t(1)),
      "high bit minus all lower bits is one");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(uint64_t(14552598638644786479ULL),
                                 uint64_t(3894174382537247221ULL)),
                uint64_t(10658424256107539258ULL)),
      "14552598638644786479 - 39763621533397112216 is 10658424256107539258L");

  MOZ_RELEASE_ASSERT(
      TestEqual(
          WrappingSubtract(int64_t(0), int64_t(-9223372036854775807LL - 1)),
          int64_t(-9223372036854775807LL - 1)),
      "zero minus high bit wraps to high bit");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int64_t(-9223372036854775802LL), int64_t(8)),
                int64_t(9223372036854775806LL)),
      "overflow to negative");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int64_t(37482739294298742LL),
                                 int64_t(-437843573929483498LL)),
                int64_t(475326313223782240)),
      "37482739294298742 - -437843573929483498 is 475326313223782240");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int64_t(-9127837934058953374LL),
                                 int64_t(-4173572032144775807LL)),
                int64_t(-4954265901914177567LL)),
      "negative minus smaller negative");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingSubtract(int64_t(-9223372036854775807LL - 1),
                                 int64_t(9223372036854775807LL)),
                int64_t(1)),
      "underflow to 1");
}

static void TestWrappingSubtract() {
  TestWrappingSubtract8();
  TestWrappingSubtract16();
  TestWrappingSubtract32();
  TestWrappingSubtract64();
}

static void TestWrappingMultiply8() {
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint8_t(0), uint8_t(128)), uint8_t(0)),
      "zero times anything is zero");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint8_t(128), uint8_t(1)), uint8_t(128)),
      "1 times anything is anything");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint8_t(2), uint8_t(128)), uint8_t(0)),
      "2 times high bit overflows, produces zero");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint8_t(8), uint8_t(16)), uint8_t(128)),
      "multiply that populates the high bit produces that value");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint8_t(127), uint8_t(127)), uint8_t(1)),
      "multiplying signed maxvals overflows all the way to 1");

  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int8_t(0), int8_t(-128)), int8_t(0)),
      "zero times anything is zero");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int8_t(-128), int8_t(1)), int8_t(-128)),
      "1 times anything is anything");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int8_t(2), int8_t(-128)), int8_t(0)),
      "2 times min overflows, produces zero");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int8_t(16), int8_t(24)), int8_t(-128)),
      "multiply that populates the sign bit produces minval");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int8_t(8), int8_t(16)), int8_t(-128)),
      "multiply that populates the sign bit produces minval");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int8_t(127), int8_t(127)), int8_t(1)),
      "multiplying maxvals overflows all the way to 1");
}

static void TestWrappingMultiply16() {
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint16_t(0), uint16_t(32768)), uint16_t(0)),
      "zero times anything is zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint16_t(32768), uint16_t(1)),
                               uint16_t(32768)),
                     "1 times anything is anything");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint16_t(2), uint16_t(32768)), uint16_t(0)),
      "2 times high bit overflows, produces zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint16_t(3), uint16_t(32768)),
                               uint16_t(-32768)),
                     "3 * 32768 - 65536 is 32768");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint16_t(64), uint16_t(512)), uint16_t(32768)),
      "multiply that populates the high bit produces that value");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint16_t(32767), uint16_t(32767)),
                uint16_t(1)),
      "multiplying signed maxvals overflows all the way to 1");

  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int16_t(0), int16_t(-32768)), int16_t(0)),
      "zero times anything is zero");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int16_t(-32768), int16_t(1)), int16_t(-32768)),
      "1 times anything is anything");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int16_t(-456), int16_t(123)), int16_t(9448)),
      "multiply opposite signs, then add 2**16 for the result");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int16_t(2), int16_t(-32768)), int16_t(0)),
      "2 times min overflows, produces zero");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int16_t(64), int16_t(512)), int16_t(-32768)),
      "multiply that populates the sign bit produces minval");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int16_t(32767), int16_t(32767)), int16_t(1)),
      "multiplying maxvals overflows all the way to 1");
}

static void TestWrappingMultiply32() {
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint32_t(0), uint32_t(2147483648)),
                uint32_t(0)),
      "zero times anything is zero");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint32_t(42), uint32_t(17)), uint32_t(714)),
      "42 * 17 is 714 without wraparound");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint32_t(2147483648), uint32_t(1)),
                uint32_t(2147483648)),
      "1 times anything is anything");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint32_t(2), uint32_t(2147483648)),
                uint32_t(0)),
      "2 times high bit overflows, produces zero");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint32_t(8192), uint32_t(262144)),
                uint32_t(2147483648)),
      "multiply that populates the high bit produces that value");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint32_t(2147483647), uint32_t(2147483647)),
                uint32_t(1)),
      "multiplying signed maxvals overflows all the way to 1");

  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int32_t(0), int32_t(-2147483647 - 1)),
                int32_t(0)),
      "zero times anything is zero");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int32_t(-2147483647 - 1), int32_t(1)),
                int32_t(-2147483647 - 1)),
      "1 times anything is anything");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int32_t(2), int32_t(-2147483647 - 1)),
                int32_t(0)),
      "2 times min overflows, produces zero");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int32_t(-7), int32_t(-9)), int32_t(63)),
      "-7 * -9 is 63, no wraparound needed");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int32_t(8192), int32_t(262144)),
                               int32_t(-2147483647 - 1)),
                     "multiply that populates the sign bit produces minval");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int32_t(2147483647), int32_t(2147483647)),
                int32_t(1)),
      "multiplying maxvals overflows all the way to 1");
}

static void TestWrappingMultiply64() {
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint64_t(0), uint64_t(9223372036854775808ULL)),
                uint64_t(0)),
      "zero times anything is zero");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint64_t(9223372036854775808ULL), uint64_t(1)),
                uint64_t(9223372036854775808ULL)),
      "1 times anything is anything");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint64_t(2), uint64_t(9223372036854775808ULL)),
                uint64_t(0)),
      "2 times high bit overflows, produces zero");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(uint64_t(131072), uint64_t(70368744177664)),
                uint64_t(9223372036854775808ULL)),
      "multiply that populates the high bit produces that value");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint64_t(9223372036854775807),
                                                uint64_t(9223372036854775807)),
                               uint64_t(1)),
                     "multiplying signed maxvals overflows all the way to 1");

  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int64_t(0), int64_t(-9223372036854775807 - 1)),
                int64_t(0)),
      "zero times anything is zero");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int64_t(-9223372036854775807 - 1), int64_t(1)),
                int64_t(-9223372036854775807 - 1)),
      "1 times anything is anything");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int64_t(2), int64_t(-9223372036854775807 - 1)),
                int64_t(0)),
      "2 times min overflows, produces zero");
  MOZ_RELEASE_ASSERT(
      TestEqual(WrappingMultiply(int64_t(131072), int64_t(70368744177664)),
                int64_t(-9223372036854775807 - 1)),
      "multiply that populates the sign bit produces minval");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int64_t(9223372036854775807),
                                                int64_t(9223372036854775807)),
                               int64_t(1)),
                     "multiplying maxvals overflows all the way to 1");
}

static void TestWrappingMultiply() {
  TestWrappingMultiply8();
  TestWrappingMultiply16();
  TestWrappingMultiply32();
  TestWrappingMultiply64();
}

int main() {
  TestWrappingAdd();
  TestWrappingSubtract();
  TestWrappingMultiply();
  return 0;
}
