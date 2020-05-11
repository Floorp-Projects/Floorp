/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsURIHashKey_h__
#define nsURIHashKey_h__

#include <utility>

#include "PLDHashTable.h"
#include "mozilla/Unused.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsIURI.h"

/**
 * Hashtable key class to use with nsTHashtable/nsBaseHashtable
 */
class nsURIHashKey : public PLDHashEntryHdr {
 public:
  typedef nsIURI* KeyType;
  typedef const nsIURI* KeyTypePointer;

  nsURIHashKey() { MOZ_COUNT_CTOR(nsURIHashKey); }
  explicit nsURIHashKey(const nsIURI* aKey) : mKey(const_cast<nsIURI*>(aKey)) {
    MOZ_COUNT_CTOR(nsURIHashKey);
  }
  nsURIHashKey(nsURIHashKey&& toMove)
      : PLDHashEntryHdr(std::move(toMove)), mKey(std::move(toMove.mKey)) {
    MOZ_COUNT_CTOR(nsURIHashKey);
  }
  MOZ_COUNTED_DTOR(nsURIHashKey)

  nsURIHashKey& operator=(const nsURIHashKey& aOther) {
    mKey = aOther.mKey;
    return *this;
  }

  nsIURI* GetKey() const { return mKey; }

  bool KeyEquals(const nsIURI* aKey) const {
    bool eq;
    if (!mKey) {
      return !aKey;
    }
    if (NS_SUCCEEDED(mKey->Equals(const_cast<nsIURI*>(aKey), &eq))) {
      return eq;
    }
    return false;
  }

  static const nsIURI* KeyToPointer(nsIURI* aKey) { return aKey; }
  static PLDHashNumber HashKey(const nsIURI* aKey) {
    if (!aKey) {
      // If the key is null, return hash for empty string.
      return mozilla::HashString(EmptyCString());
    }
    nsAutoCString spec;
    // If GetSpec() fails, ignoring the failure and proceeding with an
    // empty |spec| seems like the best thing to do.
    mozilla::Unused << const_cast<nsIURI*>(aKey)->GetSpec(spec);
    return mozilla::HashString(spec);
  }

  enum { ALLOW_MEMMOVE = true };

 protected:
  nsCOMPtr<nsIURI> mKey;
};

#endif  // nsURIHashKey_h__
