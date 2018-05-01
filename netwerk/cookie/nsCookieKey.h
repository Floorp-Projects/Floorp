/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_nsCookieKey_h
#define mozilla_net_nsCookieKey_h
#include "nsHashKeys.h"
#include "nsTHashtable.h"

namespace mozilla {
namespace net {
class nsCookieKey : public PLDHashEntryHdr
{
public:
  typedef const nsCookieKey& KeyType;
  typedef const nsCookieKey* KeyTypePointer;

  nsCookieKey() = default;

  nsCookieKey(const nsCString &baseDomain, const OriginAttributes &attrs)
    : mBaseDomain(baseDomain)
    , mOriginAttributes(attrs)
  {}

  explicit nsCookieKey(KeyTypePointer other)
    : mBaseDomain(other->mBaseDomain)
    , mOriginAttributes(other->mOriginAttributes)
  {}

  nsCookieKey(KeyType other)
    : mBaseDomain(other.mBaseDomain)
    , mOriginAttributes(other.mOriginAttributes)
  {}

  bool KeyEquals(KeyTypePointer other) const
  {
    return mBaseDomain == other->mBaseDomain &&
           mOriginAttributes == other->mOriginAttributes;
  }

  static KeyTypePointer KeyToPointer(KeyType aKey)
  {
    return &aKey;
  }

  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    nsAutoCString temp(aKey->mBaseDomain);
    temp.Append('#');
    nsAutoCString suffix;
    aKey->mOriginAttributes.CreateSuffix(suffix);
    temp.Append(suffix);
    return mozilla::HashString(temp);
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return mBaseDomain.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  enum { ALLOW_MEMMOVE = true };

  nsCString        mBaseDomain;
  OriginAttributes mOriginAttributes;
};

} // net
} // mozilla
#endif // mozilla_net_nsCookieKey_h
