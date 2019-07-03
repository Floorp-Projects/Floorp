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
void TestArrayManipulation() {
  using namespace mozilla;
  SmallPointerArray<void> testArray;

  MOZ_RELEASE_ASSERT(testArray.Length() == 0);
  MOZ_RELEASE_ASSERT(sizeof(testArray) == 2 * sizeof(void*));
  MOZ_RELEASE_ASSERT(!testArray.Contains(PTR1));

  testArray.AppendElement(PTR1);

  MOZ_RELEASE_ASSERT(testArray.Length() == 1);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR1);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR1);
  MOZ_RELEASE_ASSERT(testArray.Contains(PTR1));

  testArray.AppendElement(PTR2);

  MOZ_RELEASE_ASSERT(testArray.Length() == 2);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR1);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR1);
  MOZ_RELEASE_ASSERT(testArray[1] == PTR2);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(1) == PTR2);
  MOZ_RELEASE_ASSERT(testArray.Contains(PTR2));

  MOZ_RELEASE_ASSERT(testArray.RemoveElement(PTR1));
  MOZ_RELEASE_ASSERT(!testArray.RemoveElement(PTR1));

  MOZ_RELEASE_ASSERT(testArray.Length() == 1);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR2);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR2);
  MOZ_RELEASE_ASSERT(!testArray.Contains(PTR1));

  testArray.AppendElement(PTR1);

  MOZ_RELEASE_ASSERT(testArray.Length() == 2);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR2);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR2);
  MOZ_RELEASE_ASSERT(testArray[1] == PTR1);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(1) == PTR1);
  MOZ_RELEASE_ASSERT(testArray.Contains(PTR1));

  testArray.AppendElement(PTR3);

  MOZ_RELEASE_ASSERT(testArray.Length() == 3);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR2);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR2);
  MOZ_RELEASE_ASSERT(testArray[1] == PTR1);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(1) == PTR1);
  MOZ_RELEASE_ASSERT(testArray[2] == PTR3);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(2) == PTR3);
  MOZ_RELEASE_ASSERT(testArray.Contains(PTR3));

  MOZ_RELEASE_ASSERT(testArray.RemoveElement(PTR1));

  MOZ_RELEASE_ASSERT(testArray.Length() == 2);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR2);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR2);
  MOZ_RELEASE_ASSERT(testArray[1] == PTR3);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(1) == PTR3);

  MOZ_RELEASE_ASSERT(testArray.RemoveElement(PTR2));

  MOZ_RELEASE_ASSERT(testArray.Length() == 1);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR3);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR3);

  MOZ_RELEASE_ASSERT(testArray.RemoveElement(PTR3));

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

  MOZ_RELEASE_ASSERT(testArray.RemoveElement(PTR2));

  MOZ_RELEASE_ASSERT(testArray.Length() == 1);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR1);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR1);

  MOZ_RELEASE_ASSERT(!testArray.RemoveElement(PTR3));

  MOZ_RELEASE_ASSERT(testArray.Length() == 1);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR1);
  MOZ_RELEASE_ASSERT(testArray.ElementAt(0) == PTR1);
}

void TestRangeBasedLoops() {
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

void TestMove() {
  using namespace mozilla;

  SmallPointerArray<void> testArray;
  testArray.AppendElement(PTR1);
  testArray.AppendElement(PTR2);

  SmallPointerArray<void> moved = std::move(testArray);

  MOZ_RELEASE_ASSERT(testArray.IsEmpty());
  MOZ_RELEASE_ASSERT(moved.Length() == 2);
  MOZ_RELEASE_ASSERT(moved[0] == PTR1);
  MOZ_RELEASE_ASSERT(moved[1] == PTR2);

  // Heap case.
  moved.AppendElement(PTR3);

  SmallPointerArray<void> another = std::move(moved);

  MOZ_RELEASE_ASSERT(testArray.IsEmpty());
  MOZ_RELEASE_ASSERT(moved.IsEmpty());
  MOZ_RELEASE_ASSERT(another.Length() == 3);
  MOZ_RELEASE_ASSERT(another[0] == PTR1);
  MOZ_RELEASE_ASSERT(another[1] == PTR2);
  MOZ_RELEASE_ASSERT(another[2] == PTR3);

  // Move assignment.
  testArray = std::move(another);

  MOZ_RELEASE_ASSERT(moved.IsEmpty());
  MOZ_RELEASE_ASSERT(another.IsEmpty());
  MOZ_RELEASE_ASSERT(testArray.Length() == 3);
  MOZ_RELEASE_ASSERT(testArray[0] == PTR1);
  MOZ_RELEASE_ASSERT(testArray[1] == PTR2);
  MOZ_RELEASE_ASSERT(testArray[2] == PTR3);
}

int main() {
  TestArrayManipulation();
  TestRangeBasedLoops();
  TestMove();
  return 0;
}
