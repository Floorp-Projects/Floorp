/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/Vector.h"

#include <cstdlib>

using mozilla::ArrayLength;
using mozilla::BinarySearch;
using mozilla::BinarySearchIf;
using mozilla::Vector;

#define A(a) MOZ_RELEASE_ASSERT(a)

struct Person {
  int mAge;
  int mId;
  Person(int aAge, int aId) : mAge(aAge), mId(aId) {}
};

struct GetAge {
  Vector<Person>& mV;
  explicit GetAge(Vector<Person>& aV) : mV(aV) {}
  int operator[](size_t index) const { return mV[index].mAge; }
};

struct RangeFinder {
  const int mLower, mUpper;
  RangeFinder(int lower, int upper) : mLower(lower), mUpper(upper) {}
  int operator()(int val) const {
    if (val >= mUpper) return -1;
    if (val < mLower) return 1;
    return 0;
  }
};

static void TestBinarySearch() {
  size_t m;

  Vector<int> v1;
  MOZ_RELEASE_ASSERT(v1.append(2));
  MOZ_RELEASE_ASSERT(v1.append(4));
  MOZ_RELEASE_ASSERT(v1.append(6));
  MOZ_RELEASE_ASSERT(v1.append(8));

  MOZ_RELEASE_ASSERT(!BinarySearch(v1, 0, v1.length(), 1, &m) && m == 0);
  MOZ_RELEASE_ASSERT(BinarySearch(v1, 0, v1.length(), 2, &m) && m == 0);
  MOZ_RELEASE_ASSERT(!BinarySearch(v1, 0, v1.length(), 3, &m) && m == 1);
  MOZ_RELEASE_ASSERT(BinarySearch(v1, 0, v1.length(), 4, &m) && m == 1);
  MOZ_RELEASE_ASSERT(!BinarySearch(v1, 0, v1.length(), 5, &m) && m == 2);
  MOZ_RELEASE_ASSERT(BinarySearch(v1, 0, v1.length(), 6, &m) && m == 2);
  MOZ_RELEASE_ASSERT(!BinarySearch(v1, 0, v1.length(), 7, &m) && m == 3);
  MOZ_RELEASE_ASSERT(BinarySearch(v1, 0, v1.length(), 8, &m) && m == 3);
  MOZ_RELEASE_ASSERT(!BinarySearch(v1, 0, v1.length(), 9, &m) && m == 4);

  MOZ_RELEASE_ASSERT(!BinarySearch(v1, 1, 3, 1, &m) && m == 1);
  MOZ_RELEASE_ASSERT(!BinarySearch(v1, 1, 3, 2, &m) && m == 1);
  MOZ_RELEASE_ASSERT(!BinarySearch(v1, 1, 3, 3, &m) && m == 1);
  MOZ_RELEASE_ASSERT(BinarySearch(v1, 1, 3, 4, &m) && m == 1);
  MOZ_RELEASE_ASSERT(!BinarySearch(v1, 1, 3, 5, &m) && m == 2);
  MOZ_RELEASE_ASSERT(BinarySearch(v1, 1, 3, 6, &m) && m == 2);
  MOZ_RELEASE_ASSERT(!BinarySearch(v1, 1, 3, 7, &m) && m == 3);
  MOZ_RELEASE_ASSERT(!BinarySearch(v1, 1, 3, 8, &m) && m == 3);
  MOZ_RELEASE_ASSERT(!BinarySearch(v1, 1, 3, 9, &m) && m == 3);

  MOZ_RELEASE_ASSERT(!BinarySearch(v1, 0, 0, 0, &m) && m == 0);
  MOZ_RELEASE_ASSERT(!BinarySearch(v1, 0, 0, 9, &m) && m == 0);

  Vector<int> v2;
  MOZ_RELEASE_ASSERT(!BinarySearch(v2, 0, 0, 0, &m) && m == 0);
  MOZ_RELEASE_ASSERT(!BinarySearch(v2, 0, 0, 9, &m) && m == 0);

  Vector<Person> v3;
  MOZ_RELEASE_ASSERT(v3.append(Person(2, 42)));
  MOZ_RELEASE_ASSERT(v3.append(Person(4, 13)));
  MOZ_RELEASE_ASSERT(v3.append(Person(6, 360)));

  A(!BinarySearch(GetAge(v3), 0, v3.length(), 1, &m) && m == 0);
  A(BinarySearch(GetAge(v3), 0, v3.length(), 2, &m) && m == 0);
  A(!BinarySearch(GetAge(v3), 0, v3.length(), 3, &m) && m == 1);
  A(BinarySearch(GetAge(v3), 0, v3.length(), 4, &m) && m == 1);
  A(!BinarySearch(GetAge(v3), 0, v3.length(), 5, &m) && m == 2);
  A(BinarySearch(GetAge(v3), 0, v3.length(), 6, &m) && m == 2);
  A(!BinarySearch(GetAge(v3), 0, v3.length(), 7, &m) && m == 3);
}

static void TestBinarySearchIf() {
  const int v1[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  const size_t len = ArrayLength(v1);
  size_t m;

  A(BinarySearchIf(v1, 0, len, RangeFinder(2, 3), &m) && m == 2);
  A(!BinarySearchIf(v1, 0, len, RangeFinder(-5, -2), &m) && m == 0);
  A(BinarySearchIf(v1, 0, len, RangeFinder(3, 5), &m) && m >= 3 && m < 5);
  A(!BinarySearchIf(v1, 0, len, RangeFinder(10, 12), &m) && m == 10);
}

static void TestEqualRange() {
  struct CompareN {
    int mVal;
    explicit CompareN(int n) : mVal(n) {}
    int operator()(int aVal) const { return mVal - aVal; }
  };

  constexpr int kMaxNumber = 100;
  constexpr int kMaxRepeat = 2;

  Vector<int> sortedArray;
  MOZ_RELEASE_ASSERT(sortedArray.reserve(kMaxNumber * kMaxRepeat));

  // Make a sorted array by appending the loop counter [0, kMaxRepeat] times
  // in each iteration.  The array will be something like:
  //   [0, 0, 1, 1, 2, 2, 8, 9, ..., kMaxNumber]
  for (int i = 0; i <= kMaxNumber; ++i) {
    int repeat = rand() % (kMaxRepeat + 1);
    for (int j = 0; j < repeat; ++j) {
      MOZ_RELEASE_ASSERT(sortedArray.emplaceBack(i));
    }
  }

  for (int i = -1; i < kMaxNumber + 1; ++i) {
    auto bounds = EqualRange(sortedArray, 0, sortedArray.length(), CompareN(i));

    MOZ_RELEASE_ASSERT(bounds.first <= sortedArray.length());
    MOZ_RELEASE_ASSERT(bounds.second <= sortedArray.length());
    MOZ_RELEASE_ASSERT(bounds.first <= bounds.second);

    if (bounds.first == 0) {
      MOZ_RELEASE_ASSERT(sortedArray[0] >= i);
    } else if (bounds.first == sortedArray.length()) {
      MOZ_RELEASE_ASSERT(sortedArray[sortedArray.length() - 1] < i);
    } else {
      MOZ_RELEASE_ASSERT(sortedArray[bounds.first - 1] < i);
      MOZ_RELEASE_ASSERT(sortedArray[bounds.first] >= i);
    }

    if (bounds.second == 0) {
      MOZ_RELEASE_ASSERT(sortedArray[0] > i);
    } else if (bounds.second == sortedArray.length()) {
      MOZ_RELEASE_ASSERT(sortedArray[sortedArray.length() - 1] <= i);
    } else {
      MOZ_RELEASE_ASSERT(sortedArray[bounds.second - 1] <= i);
      MOZ_RELEASE_ASSERT(sortedArray[bounds.second] > i);
    }
  }
}

int main() {
  TestBinarySearch();
  TestBinarySearchIf();
  TestEqualRange();
  return 0;
}
