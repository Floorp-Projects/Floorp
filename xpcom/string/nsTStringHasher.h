/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTStringHasher_h___
#define nsTStringHasher_h___

#include "mozilla/HashTable.h"  // mozilla::{DefaultHasher, HashNumber, HashString}

namespace mozilla {

template <typename T>
struct DefaultHasher<nsTString<T>> {
  using Key = nsTString<T>;
  using Lookup = nsTString<T>;

  static mozilla::HashNumber hash(const Lookup& aLookup) {
    return mozilla::HashString(aLookup.get());
  }

  static bool match(const Key& aKey, const Lookup& aLookup) {
    return aKey.Equals(aLookup);
  }
};

}  // namespace mozilla

#endif  // !defined(nsTStringHasher_h___)
