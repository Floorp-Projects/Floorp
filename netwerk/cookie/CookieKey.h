/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieKey_h
#define mozilla_net_CookieKey_h

#include "nsHashKeys.h"
#include "nsTHashtable.h"

namespace mozilla {
namespace net {

class CookieKey : public PLDHashEntryHdr {
 public:
  typedef const CookieKey& KeyType;
  typedef const CookieKey* KeyTypePointer;

  CookieKey() = default;

  CookieKey(const nsACString& baseDomain, const OriginAttributes& attrs)
      : mBaseDomain(baseDomain), mOriginAttributes(attrs) {}

  explicit CookieKey(KeyTypePointer other)
      : mBaseDomain(other->mBaseDomain),
        mOriginAttributes(other->mOriginAttributes) {}

  CookieKey(CookieKey&& other) = default;
  CookieKey& operator=(CookieKey&&) = default;

  bool KeyEquals(KeyTypePointer other) const {
    return mBaseDomain == other->mBaseDomain &&
           mOriginAttributes == other->mOriginAttributes;
  }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }

  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    nsAutoCString temp(aKey->mBaseDomain);
    temp.Append('#');
    nsAutoCString suffix;
    aKey->mOriginAttributes.CreateSuffix(suffix);
    temp.Append(suffix);
    return mozilla::HashString(temp);
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return mBaseDomain.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  enum { ALLOW_MEMMOVE = true };

  nsCString mBaseDomain;
  OriginAttributes mOriginAttributes;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieKey_h
