/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Algorithm.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"

#include <iterator>

static constexpr bool even(int32_t n) { return !(n & 1); }
static constexpr bool odd(int32_t n) { return (n & 1); }

using namespace mozilla;

void TestAllOf() {
  using std::begin;
  using std::end;

  constexpr static int32_t arr1[3] = {1, 2, 3};
  MOZ_RELEASE_ASSERT(!AllOf(begin(arr1), end(arr1), even));
  MOZ_RELEASE_ASSERT(!AllOf(begin(arr1), end(arr1), odd));
  static_assert(!AllOf(arr1, arr1 + ArrayLength(arr1), even), "1-1");
  static_assert(!AllOf(arr1, arr1 + ArrayLength(arr1), odd), "1-2");

  constexpr static int32_t arr2[3] = {1, 3, 5};
  MOZ_RELEASE_ASSERT(!AllOf(begin(arr2), end(arr2), even));
  MOZ_RELEASE_ASSERT(AllOf(begin(arr2), end(arr2), odd));
  static_assert(!AllOf(arr2, arr2 + ArrayLength(arr2), even), "2-1");
  static_assert(AllOf(arr2, arr2 + ArrayLength(arr2), odd), "2-2");

  constexpr static int32_t arr3[3] = {2, 4, 6};
  MOZ_RELEASE_ASSERT(AllOf(begin(arr3), end(arr3), even));
  MOZ_RELEASE_ASSERT(!AllOf(begin(arr3), end(arr3), odd));
  static_assert(AllOf(arr3, arr3 + ArrayLength(arr3), even), "3-1");
  static_assert(!AllOf(arr3, arr3 + ArrayLength(arr3), odd), "3-2");
}

void TestAnyOf() {
  using std::begin;
  using std::end;

  // The Android NDK's STL doesn't support `constexpr` `std::array::begin`, see
  // bug 1677484. Hence using a raw array here.
  constexpr int32_t arr1[1] = {0};
  static_assert(!AnyOf(arr1, arr1, even));
  static_assert(!AnyOf(arr1, arr1, odd));

  constexpr int32_t arr2[] = {1};
  static_assert(!AnyOf(begin(arr2), end(arr2), even));
  static_assert(AnyOf(begin(arr2), end(arr2), odd));

  constexpr int32_t arr3[] = {2};
  static_assert(AnyOf(begin(arr3), end(arr3), even));
  static_assert(!AnyOf(begin(arr3), end(arr3), odd));

  constexpr int32_t arr4[] = {1, 2};
  static_assert(AnyOf(begin(arr4), end(arr4), even));
  static_assert(AnyOf(begin(arr4), end(arr4), odd));
}

int main() {
  TestAllOf();
  TestAnyOf();
  return 0;
}
