/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/SmallPointerArray.h"

#define PTR1 (void*)0x4
#define PTR2 (void*)0x5
#define PTR3 (void*)0x6

// We explicitly test sizes up to 3 here, as that is when SmallPointerArray<>
// switches to the storage method used for larger arrays.
void TestArrayManipulation()
{
  using namespace mozilla;
  SmallPointerArray<void> testArray;

  MOZ_RELEASE_ASSERT(testArray.Length() == 0);
  MOZ_RELEASE_ASSERT(sizeof(testArray) == 2 * sizeof(void*));

  testArray.AppendElement(PTR1);

  MOZ_RELEASE_ASSERT(testArray.Length() == 1);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR1);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR1);

  testArray.AppendElement(PTR2);

  MOZ_RELEASE_ASSERT(testArray.Length() == 2);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR1);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR1);
  MOZ_RELEASE_ASSERT(testArray[1] == PTR2);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(1) == PTR2);

  testArray.RemoveElement(PTR1);

  MOZ_RELEASE_ASSERT(testArray.Length() == 1);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR2);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR2);

  testArray.AppendElement(PTR1);

  MOZ_RELEASE_ASSERT(testArray.Length() == 2);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR2);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR2);
  MOZ_RELEASE_ASSERT(testArray[1] == PTR1);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(1) == PTR1);

  testArray.AppendElement(PTR3);

  MOZ_RELEASE_ASSERT(testArray.Length() == 3);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR2);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR2);
  MOZ_RELEASE_ASSERT(testArray[1] == PTR1);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(1) == PTR1);
  MOZ_RELEASE_ASSERT(testArray[2] == PTR3);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(2) == PTR3);

  testArray.RemoveElement(PTR1);

  MOZ_RELEASE_ASSERT(testArray.Length() == 2);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR2);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR2);
  MOZ_RELEASE_ASSERT(testArray[1] == PTR3);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(1) == PTR3);

  testArray.RemoveElement(PTR2);

  MOZ_RELEASE_ASSERT(testArray.Length() == 1);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR3);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR3);

  testArray.RemoveElement(PTR3);

  MOZ_RELEASE_ASSERT(testArray.Length() == 0);

  testArray.Clear();

  MOZ_RELEASE_ASSERT(testArray.Length() == 0);

  testArray.AppendElement(PTR1);

  MOZ_RELEASE_ASSERT(testArray.Length() == 1);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR1);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR1);

  testArray.AppendElement(PTR2);

  MOZ_RELEASE_ASSERT(testArray.Length() == 2);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR1);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR1);
  MOZ_RELEASE_ASSERT(testArray[1] == PTR2);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(1) == PTR2);

  testArray.RemoveElement(PTR2);

  MOZ_RELEASE_ASSERT(testArray.Length() == 1);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR1);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR1);

  testArray.RemoveElement(PTR3);

  MOZ_RELEASE_ASSERT(testArray.Length() == 1);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR1);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR1);
}

void TestRangeBasedLoops()
{
  using namespace mozilla;
  SmallPointerArray<void> testArray;
  void* verification[3];
  uint32_t entries = 0;

  for (void* test : testArray) {
    verification[entries++] = test;
  }

  MOZ_RELEASE_ASSERT(entries == 0);

  testArray.AppendElement(PTR1);

  for (void* test : testArray) {
    verification[entries++] = test;
  }

  MOZ_RELEASE_ASSERT(entries == 1);
  MOZ_RELEASE_ASSERT(verification[0] == PTR1);

  entries = 0;

  testArray.AppendElement(PTR2);

  for (void* test : testArray) {
    verification[entries++] = test;
  }

  MOZ_RELEASE_ASSERT(entries == 2);
  MOZ_RELEASE_ASSERT(verification[0] == PTR1);
  MOZ_RELEASE_ASSERT(verification[1] == PTR2);

  entries = 0;

  testArray.RemoveElement(PTR1);

  for (void* test : testArray) {
    verification[entries++] = test;
  }

  MOZ_RELEASE_ASSERT(entries == 1);
  MOZ_RELEASE_ASSERT(verification[0] == PTR2);

  entries = 0;

  testArray.AppendElement(PTR1);
  testArray.AppendElement(PTR3);

  for (void* test : testArray) {
    verification[entries++] = test;
  }

  MOZ_RELEASE_ASSERT(entries == 3);
  MOZ_RELEASE_ASSERT(verification[0] == PTR2);
  MOZ_RELEASE_ASSERT(verification[1] == PTR1);
  MOZ_RELEASE_ASSERT(verification[2] == PTR3);

  entries = 0;

  testArray.RemoveElement(PTR1);
  testArray.RemoveElement(PTR2);
  testArray.RemoveElement(PTR3);

  for (void* test : testArray) {
    verification[entries++] = test;
  }

  MOZ_RELEASE_ASSERT(entries == 0);

  testArray.Clear();

  for (void* test : testArray) {
    verification[entries++] = test;
  }

  MOZ_RELEASE_ASSERT(entries == 0);
}

int
main()
{
  TestArrayManipulation();
  TestRangeBasedLoops();
  return 0;
}
