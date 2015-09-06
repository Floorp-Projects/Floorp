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
  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(0u));
  MOZ_RELEASE_ASSERT( IsPowerOfTwo(1u));
  MOZ_RELEASE_ASSERT( IsPowerOfTwo(2u));
  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(3u));
  MOZ_RELEASE_ASSERT( IsPowerOfTwo(4u));
  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(5u));
  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(6u));
  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(7u));
  MOZ_RELEASE_ASSERT( IsPowerOfTwo(8u));
  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(9u));

  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(uint8_t(UINT8_MAX/2)));     // 127, 0x7f
  MOZ_RELEASE_ASSERT( IsPowerOfTwo(uint8_t(UINT8_MAX/2 + 1))); // 128, 0x80
  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(uint8_t(UINT8_MAX/2 + 2))); // 129, 0x81
  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(uint8_t(UINT8_MAX - 1)));   // 254, 0xfe
  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(uint8_t(UINT8_MAX)));       // 255, 0xff

  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(uint16_t(UINT16_MAX/2)));     // 0x7fff
  MOZ_RELEASE_ASSERT( IsPowerOfTwo(uint16_t(UINT16_MAX/2 + 1))); // 0x8000
  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(uint16_t(UINT16_MAX/2 + 2))); // 0x8001
  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(uint16_t(UINT16_MAX - 1)));   // 0xfffe
  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(uint16_t(UINT16_MAX)));       // 0xffff

  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(uint32_t(UINT32_MAX/2)));
  MOZ_RELEASE_ASSERT( IsPowerOfTwo(uint32_t(UINT32_MAX/2 + 1)));
  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(uint32_t(UINT32_MAX/2 + 2)));
  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(uint32_t(UINT32_MAX - 1)));
  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(uint32_t(UINT32_MAX)));

  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(uint64_t(UINT64_MAX/2)));
  MOZ_RELEASE_ASSERT( IsPowerOfTwo(uint64_t(UINT64_MAX/2 + 1)));
  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(uint64_t(UINT64_MAX/2 + 2)));
  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(uint64_t(UINT64_MAX - 1)));
  MOZ_RELEASE_ASSERT(!IsPowerOfTwo(uint64_t(UINT64_MAX)));
}

int
main()
{
  TestIsPowerOfTwo();
  TestClamp();

  return 0;
}
