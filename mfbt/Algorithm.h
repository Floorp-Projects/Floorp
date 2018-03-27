/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A polyfill for `<algorithm>`. */

#ifndef mozilla_Algorithm_h
#define mozilla_Algorithm_h

namespace mozilla {

// Returns true if all elements in the range [aFirst, aLast)
// satisfy the predicate aPred.
template <class Iter, class Pred>
bool AllOf(Iter aFirst, Iter aLast, Pred aPred)
{
  for (; aFirst != aLast; ++aFirst) {
    if (!aPred(*aFirst)) {
      return false;
    }
  }
  return true;
}

// This is a `constexpr` alternative to AllOf. It should
// only be used for compile-time computation because it uses recursion.
// XXX: once support for GCC 4.9 is dropped, this function should be removed
// and AllOf should be made `constexpr`.
// XXX: This implementation requires RandomAccessIterator.
template <class Iter, class Pred>
constexpr bool ConstExprAllOf(Iter aFirst, Iter aLast, Pred aPred)
{
  return aFirst == aLast ? true :
    aLast - aFirst == 1 ? aPred(*aFirst) :
    ConstExprAllOf(aFirst, aFirst + (aLast - aFirst) / 2, aPred) &&
    ConstExprAllOf(aFirst + (aLast - aFirst) / 2, aLast, aPred);
}

} // namespace mozilla

#endif // mozilla_Algorithm_h
