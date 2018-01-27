/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MathAlgorithms.h"

#include <stdint.h>

using mozilla::Clamp;
using mozilla::IsPowerOfTwo;
using mozilla::WrapToSigned;

static void
TestClamp()
{
  MOZ_RELEASE_ASSERT(Clamp(0, 0, 0) == 0);
  MOZ_RELEASE_ASSERT(Clamp(1, 0, 0) == 0);
  MOZ_RELEASE_ASSERT(Clamp(-1, 0, 0) == 0);

  MOZ_RELEASE_ASSERT(Clamp(0, 1, 1) == 1);
  MOZ_RELEASE_ASSERT(Clamp(0, 1, 2) == 1);

  MOZ_RELEASE_ASSERT(Clamp(0, -1, -1) == -1);
  MOZ_RELEASE_ASSERT(Clamp(0, -2, -1) == -1);

  MOZ_RELEASE_ASSERT(Clamp(0, 1, 3) == 1);
  MOZ_RELEASE_ASSERT(Clamp(1, 1, 3) == 1);
  MOZ_RELEASE_ASSERT(Clamp(2, 1, 3) == 2);
  MOZ_RELEASE_ASSERT(Clamp(3, 1, 3) == 3);
  MOZ_RELEASE_ASSERT(Clamp(4, 1, 3) == 3);
  MOZ_RELEASE_ASSERT(Clamp(5, 1, 3) == 3);

  MOZ_RELEASE_ASSERT(Clamp<uint8_t>(UINT8_MAX, 0, UINT8_MAX) == UINT8_MAX);
  MOZ_RELEASE_ASSERT(Clamp<uint8_t>(0, 0, UINT8_MAX) == 0);

  MOZ_RELEASE_ASSERT(Clamp<int8_t>(INT8_MIN, INT8_MIN, INT8_MAX) == INT8_MIN);
  MOZ_RELEASE_ASSERT(Clamp<int8_t>(INT8_MIN, 0, INT8_MAX) == 0);
  MOZ_RELEASE_ASSERT(Clamp<int8_t>(INT8_MAX, INT8_MIN, INT8_MAX) == INT8_MAX);
  MOZ_RELEASE_ASSERT(Clamp<int8_t>(INT8_MAX, INT8_MIN, 0) == 0);
}

static void
TestIsPowerOfTwo()
{
  static_assert(!IsPowerOfTwo(0u), "0 isn't a power of two");
  static_assert(IsPowerOfTwo(1u), "1 is a power of two");
  static_assert(IsPowerOfTwo(2u), "2 is a power of two");
  static_assert(!IsPowerOfTwo(3u), "3 isn't a power of two");
  static_assert(IsPowerOfTwo(4u), "4 is a power of two");
  static_assert(!IsPowerOfTwo(5u), "5 isn't a power of two");
  static_assert(!IsPowerOfTwo(6u), "6 isn't a power of two");
  static_assert(!IsPowerOfTwo(7u), "7 isn't a power of two");
  static_assert(IsPowerOfTwo(8u), "8 is a power of two");
  static_assert(!IsPowerOfTwo(9u), "9 isn't a power of two");

  static_assert(!IsPowerOfTwo(uint8_t(UINT8_MAX/2)), "127, 0x7f isn't a power of two");
  static_assert(IsPowerOfTwo(uint8_t(UINT8_MAX/2 + 1)), "128, 0x80 is a power of two");
  static_assert(!IsPowerOfTwo(uint8_t(UINT8_MAX/2 + 2)), "129, 0x81 isn't a power of two");
  static_assert(!IsPowerOfTwo(uint8_t(UINT8_MAX - 1)), "254, 0xfe isn't a power of two");
  static_assert(!IsPowerOfTwo(uint8_t(UINT8_MAX)), "255, 0xff isn't a power of two");

  static_assert(!IsPowerOfTwo(uint16_t(UINT16_MAX/2)), "0x7fff isn't a power of two");
  static_assert(IsPowerOfTwo(uint16_t(UINT16_MAX/2 + 1)), "0x8000 is a power of two");
  static_assert(!IsPowerOfTwo(uint16_t(UINT16_MAX/2 + 2)), "0x8001 isn't a power of two");
  static_assert(!IsPowerOfTwo(uint16_t(UINT16_MAX - 1)), "0xfffe isn't a power of two");
  static_assert(!IsPowerOfTwo(uint16_t(UINT16_MAX)), "0xffff isn't a power of two");

  static_assert(!IsPowerOfTwo(uint32_t(UINT32_MAX/2)), "0x7fffffff isn't a power of two");
  static_assert(IsPowerOfTwo(uint32_t(UINT32_MAX/2 + 1)), "0x80000000 is a power of two");
  static_assert(!IsPowerOfTwo(uint32_t(UINT32_MAX/2 + 2)), "0x80000001 isn't a power of two");
  static_assert(!IsPowerOfTwo(uint32_t(UINT32_MAX - 1)), "0xfffffffe isn't a power of two");
  static_assert(!IsPowerOfTwo(uint32_t(UINT32_MAX)), "0xffffffff isn't a power of two");

  static_assert(!IsPowerOfTwo(uint64_t(UINT64_MAX/2)), "0x7fffffffffffffff isn't a power of two");
  static_assert(IsPowerOfTwo(uint64_t(UINT64_MAX/2 + 1)), "0x8000000000000000 is a power of two");
  static_assert(!IsPowerOfTwo(uint64_t(UINT64_MAX/2 + 2)), "0x8000000000000001 isn't a power of two");
  static_assert(!IsPowerOfTwo(uint64_t(UINT64_MAX - 1)), "0xfffffffffffffffe isn't a power of two");
  static_assert(!IsPowerOfTwo(uint64_t(UINT64_MAX)), "0xffffffffffffffff isn't a power of two");
}

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

int
main()
{
  TestIsPowerOfTwo();
  TestClamp();

  return 0;
}
