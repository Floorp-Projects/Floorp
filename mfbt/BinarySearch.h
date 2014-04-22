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
 * The algorithm searches the given container 'c' over the sorted index range
 * [begin, end) for an index 'i' where 'c[i] == target'. If such an index 'i' is
 * found, BinarySearch returns 'true' and the index is returned via the outparam
 * 'matchOrInsertionPoint'. If no index is found, BinarySearch returns 'false'
 * and the outparam returns the first index in [begin, end] where 'target' can
 * be inserted to maintain sorted order.
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
BinarySearch(const Container &c, size_t begin, size_t end, T target, size_t *matchOrInsertionPoint)
{
  MOZ_ASSERT(begin <= end);

  size_t low = begin;
  size_t high = end;
  while (low != high) {
    size_t middle = low + (high - low) / 2;
    const T &middleValue = c[middle];

    MOZ_ASSERT(c[low] <= c[middle]);
    MOZ_ASSERT(c[middle] <= c[high - 1]);
    MOZ_ASSERT(c[low] <= c[high - 1]);

    if (target == middleValue) {
      *matchOrInsertionPoint = middle;
      return true;
    }

    if (target < middleValue)
      high = middle;
    else
      low = middle + 1;
  }

  *matchOrInsertionPoint = low;
  return false;
}

} // namespace mozilla

#endif // mozilla_BinarySearch_h
