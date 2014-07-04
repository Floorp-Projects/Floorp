/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BinarySearch_h
#define mozilla_BinarySearch_h

#include "mozilla/Assertions.h"

#include <stddef.h>

namespace mozilla {

/*
 * The algorithm searches the given container |aContainer| over the sorted
 * index range [aBegin, aEnd) for an index |i| where |aContainer[i] == aTarget|.
 * If such an index |i| is found, BinarySearch returns |true| and the index is
 * returned via the outparam |aMatchOrInsertionPoint|. If no index is found,
 * BinarySearch returns |false| and the outparam returns the first index in
 * [aBegin, aEnd] where |aTarget| can be inserted to maintain sorted order.
 *
 * Example:
 *
 *   Vector<int> sortedInts = ...
 *
 *   size_t match;
 *   if (BinarySearch(sortedInts, 0, sortedInts.length(), 13, &match))
 *     printf("found 13 at %lu\n", match);
 */

template <typename Container, typename T>
bool
BinarySearch(const Container& aContainer, size_t aBegin, size_t aEnd,
             T aTarget, size_t* aMatchOrInsertionPoint)
{
  MOZ_ASSERT(aBegin <= aEnd);

  size_t low = aBegin;
  size_t high = aEnd;
  while (low != high) {
    size_t middle = low + (high - low) / 2;

    // Allow any intermediate type so long as it provides a suitable ordering
    // relation.
    const auto& middleValue = aContainer[middle];

    MOZ_ASSERT(aContainer[low] <= aContainer[middle]);
    MOZ_ASSERT(aContainer[middle] <= aContainer[high - 1]);
    MOZ_ASSERT(aContainer[low] <= aContainer[high - 1]);

    if (aTarget == middleValue) {
      *aMatchOrInsertionPoint = middle;
      return true;
    }

    if (aTarget < middleValue) {
      high = middle;
    } else {
      low = middle + 1;
    }
  }

  *aMatchOrInsertionPoint = low;
  return false;
}

} // namespace mozilla

#endif // mozilla_BinarySearch_h
