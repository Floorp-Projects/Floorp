/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MathAlgorithms.h"

using mozilla::Clamp;
using mozilla::IsPowerOfTwo;

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

int
main()
{
  TestIsPowerOfTwo();
  TestClamp();

  return 0;
}
