/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Definitions for nsAtom-related hash keys */

#ifndef nsAtomHashKeys_h
#define nsAtomHashKeys_h

#include "nsAtom.h"
#include "nsHashKeys.h"

// TODO(emilio): Consider removing this and specializing AddToHash instead
// once bug 1849386 is fixed.

// Hash keys suitable for use in nsTHashTable.
struct nsAtomHashKey : public nsRefPtrHashKey<nsAtom> {
  using nsRefPtrHashKey::nsRefPtrHashKey;
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return MOZ_LIKELY(aKey) ? aKey->hash() : 0;
  }
};

struct nsWeakAtomHashKey : public nsPtrHashKey<nsAtom> {
  using nsPtrHashKey::nsPtrHashKey;
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return nsAtomHashKey::HashKey(aKey);
  }
};

// Hash keys suitable for use in mozilla::HashMap.
namespace mozilla {

struct AtomHashKey {
  RefPtr<nsAtom> mKey;

  explicit AtomHashKey(nsAtom* aAtom) : mKey(aAtom) {}

  using Lookup = nsAtom*;

  static HashNumber hash(const Lookup& aKey) { return aKey->hash(); }
  static bool match(const AtomHashKey& aFirst, const Lookup& aSecond) {
    return aFirst.mKey == aSecond;
  }
};

}  // namespace mozilla

#endif  // nsAtomHashKeys_h
