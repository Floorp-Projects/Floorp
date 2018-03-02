/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/WrappingOperations.h"

#include <stdint.h>

using mozilla::WrappingMultiply;
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
static_assert(WrapToSigned(uint64_t(9223372036854775808ULL)) == -9223372036854775807LL - 1,
              "works for 64-bit numbers, wraparound low end");
static_assert(WrapToSigned(uint64_t(9223372036854775808ULL + 8005552368LL)) ==
                -9223372036854775807LL - 1 + 8005552368LL,
              "works for 64-bit numbers, wraparound mid");
static_assert(WrapToSigned(uint64_t(9223372036854775808ULL + 9223372036854775807ULL)) ==
                -9223372036854775807LL - 1 + 9223372036854775807LL,
              "works for 64-bit numbers, wraparound high end");

template<typename T>
inline constexpr bool
TestEqual(T aX, T aY)
{
  return aX == aY;
}

static void
TestWrappingMultiply8()
{
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint8_t(0), uint8_t(128)),
                               uint8_t(0)),
                     "zero times anything is zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint8_t(128), uint8_t(1)),
                               uint8_t(128)),
                     "1 times anything is anything");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint8_t(2), uint8_t(128)),
                               uint8_t(0)),
                     "2 times high bit overflows, produces zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint8_t(8), uint8_t(16)),
                               uint8_t(128)),
                     "multiply that populates the high bit produces that value");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint8_t(127), uint8_t(127)),
                               uint8_t(1)),
                     "multiplying signed maxvals overflows all the way to 1");

  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int8_t(0), int8_t(-128)),
                               int8_t(0)),
                     "zero times anything is zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int8_t(-128), int8_t(1)),
                               int8_t(-128)),
                     "1 times anything is anything");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int8_t(2), int8_t(-128)),
                               int8_t(0)),
                     "2 times min overflows, produces zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int8_t(16), int8_t(24)),
                               int8_t(-128)),
                     "multiply that populates the sign bit produces minval");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int8_t(8), int8_t(16)),
                               int8_t(-128)),
                     "multiply that populates the sign bit produces minval");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int8_t(127), int8_t(127)),
                               int8_t(1)),
                     "multiplying maxvals overflows all the way to 1");
}

static void
TestWrappingMultiply16()
{
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint16_t(0), uint16_t(32768)),
                               uint16_t(0)),
                     "zero times anything is zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint16_t(32768), uint16_t(1)),
                               uint16_t(32768)),
                     "1 times anything is anything");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint16_t(2), uint16_t(32768)),
                               uint16_t(0)),
                     "2 times high bit overflows, produces zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint16_t(3), uint16_t(32768)),
                               uint16_t(-32768)),
                     "3 * 32768 - 65536 is 32768");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint16_t(64), uint16_t(512)),
                               uint16_t(32768)),
                     "multiply that populates the high bit produces that value");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint16_t(32767), uint16_t(32767)),
                               uint16_t(1)),
                    "multiplying signed maxvals overflows all the way to 1");

  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int16_t(0), int16_t(-32768)),
                               int16_t(0)),
                     "zero times anything is zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int16_t(-32768), int16_t(1)),
                               int16_t(-32768)),
                     "1 times anything is anything");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int16_t(-456), int16_t(123)),
                               int16_t(9448)),
                     "multiply opposite signs, then add 2**16 for the result");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int16_t(2), int16_t(-32768)),
                               int16_t(0)),
                     "2 times min overflows, produces zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int16_t(64), int16_t(512)),
                               int16_t(-32768)),
                     "multiply that populates the sign bit produces minval");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int16_t(32767), int16_t(32767)),
                               int16_t(1)),
                     "multiplying maxvals overflows all the way to 1");
}

static void
TestWrappingMultiply32()
{
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint32_t(0), uint32_t(2147483648)),
                               uint32_t(0)),
                    "zero times anything is zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint32_t(42), uint32_t(17)),
                               uint32_t(714)),
                     "42 * 17 is 714 without wraparound");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint32_t(2147483648), uint32_t(1)),
                               uint32_t(2147483648)),
                     "1 times anything is anything");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint32_t(2), uint32_t(2147483648)),
                               uint32_t(0)),
                     "2 times high bit overflows, produces zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint32_t(8192), uint32_t(262144)),
                               uint32_t(2147483648)),
                     "multiply that populates the high bit produces that value");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint32_t(2147483647),
                                                uint32_t(2147483647)),
                               uint32_t(1)),
                     "multiplying signed maxvals overflows all the way to 1");

  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int32_t(0), int32_t(-2147483647 - 1)),
                               int32_t(0)),
                     "zero times anything is zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int32_t(-2147483647 - 1), int32_t(1)),
                               int32_t(-2147483647 - 1)),
                     "1 times anything is anything");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int32_t(2), int32_t(-2147483647 - 1)),
                               int32_t(0)),
                     "2 times min overflows, produces zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int32_t(-7), int32_t(-9)),
                               int32_t(63)),
                     "-7 * -9 is 63, no wraparound needed");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int32_t(8192), int32_t(262144)),
                               int32_t(-2147483647 - 1)),
                     "multiply that populates the sign bit produces minval");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int32_t(2147483647), int32_t(2147483647)),
                               int32_t(1)),
                     "multiplying maxvals overflows all the way to 1");
}

static void
TestWrappingMultiply64()
{
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint64_t(0),
                                                uint64_t(9223372036854775808ULL)),
                               uint64_t(0)),
                     "zero times anything is zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint64_t(9223372036854775808ULL),
                                                uint64_t(1)),
                               uint64_t(9223372036854775808ULL)),
                     "1 times anything is anything");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint64_t(2),
                                                uint64_t(9223372036854775808ULL)),
                               uint64_t(0)),
                     "2 times high bit overflows, produces zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint64_t(131072),
                                                uint64_t(70368744177664)),
                               uint64_t(9223372036854775808ULL)),
                     "multiply that populates the high bit produces that value");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(uint64_t(9223372036854775807),
                                                uint64_t(9223372036854775807)),
                               uint64_t(1)),
                     "multiplying signed maxvals overflows all the way to 1");

  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int64_t(0), int64_t(-9223372036854775807 - 1)),
                               int64_t(0)),
                     "zero times anything is zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int64_t(-9223372036854775807 - 1),
                                                int64_t(1)),
                               int64_t(-9223372036854775807 - 1)),
                     "1 times anything is anything");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int64_t(2),
                                                int64_t(-9223372036854775807 - 1)),
                               int64_t(0)),
                     "2 times min overflows, produces zero");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int64_t(131072),
                                                int64_t(70368744177664)),
                               int64_t(-9223372036854775807 - 1)),
                     "multiply that populates the sign bit produces minval");
  MOZ_RELEASE_ASSERT(TestEqual(WrappingMultiply(int64_t(9223372036854775807),
                                                int64_t(9223372036854775807)),
                               int64_t(1)),
                     "multiplying maxvals overflows all the way to 1");
}

int
main()
{
  TestWrappingMultiply8();
  TestWrappingMultiply16();
  TestWrappingMultiply32();
  TestWrappingMultiply64();
  return 0;
}
