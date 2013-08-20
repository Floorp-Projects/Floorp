/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MathAlgorithms.h"

using mozilla::CountPopulation32;

static void
TestCountPopulation32()
{
  MOZ_ASSERT(CountPopulation32(0xFFFFFFFF) == 32);
  MOZ_ASSERT(CountPopulation32(0xF0FF1000) == 13);
  MOZ_ASSERT(CountPopulation32(0x7F8F0001) == 13);
  MOZ_ASSERT(CountPopulation32(0x3FFF0100) == 15);
  MOZ_ASSERT(CountPopulation32(0x1FF50010) == 12);
  MOZ_ASSERT(CountPopulation32(0x00800000) == 1);
  MOZ_ASSERT(CountPopulation32(0x00400000) == 1);
  MOZ_ASSERT(CountPopulation32(0x00008000) == 1);
  MOZ_ASSERT(CountPopulation32(0x00004000) == 1);
  MOZ_ASSERT(CountPopulation32(0x00000080) == 1);
  MOZ_ASSERT(CountPopulation32(0x00000040) == 1);
  MOZ_ASSERT(CountPopulation32(0x00000001) == 1);
  MOZ_ASSERT(CountPopulation32(0x00000000) == 0);
}

int main()
{
  TestCountPopulation32();
  return 0;
}
