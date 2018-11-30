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
constexpr bool AllOf(Iter aFirst, Iter aLast, Pred aPred) {
  for (; aFirst != aLast; ++aFirst) {
    if (!aPred(*aFirst)) {
      return false;
    }
  }
  return true;
}

}  // namespace mozilla

#endif  // mozilla_Algorithm_h
