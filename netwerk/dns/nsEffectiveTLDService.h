//* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EffectiveTLDService_h
#define EffectiveTLDService_h

#include "nsIEffectiveTLDService.h"

#include "nsHashKeys.h"
#include "nsIMemoryReporter.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"
#include "mozilla/Dafsa.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/MruCache.h"

class nsIIDNService;

class nsEffectiveTLDService final
  : public nsIEffectiveTLDService
  , public nsIMemoryReporter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEFFECTIVETLDSERVICE
  NS_DECL_NSIMEMORYREPORTER

  nsEffectiveTLDService();
  nsresult Init();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

private:
  nsresult GetBaseDomainInternal(nsCString &aHostname, int32_t aAdditionalParts, nsACString &aBaseDomain);
  nsresult NormalizeHostname(nsCString &aHostname);
  ~nsEffectiveTLDService();

  nsCOMPtr<nsIIDNService>     mIDNService;

  // The DAFSA provides a compact encoding of the rather large eTLD list.
  mozilla::Dafsa mGraph;

  struct TLDCacheEntry
  {
    nsCString mHost;
    nsCString mBaseDomain;
  };

  // We use a small most recently used cache to compensate for DAFSA lookups
  // being slightly slower than a binary search on a larger table of strings.
  //
  // We first check the cache for a matching result and avoid a DAFSA lookup
  // if a match is found. Otherwise we lookup the domain in the DAFSA and then
  // cache the result. During standard browsing the same domains are repeatedly
  // fed into |GetBaseDomainInternal| so this ends up being an effective
  // mitigation getting about a 99% hit rate with four tabs open.
  struct TldCache :
    public mozilla::MruCache<nsACString, TLDCacheEntry, TldCache>
  {
    static mozilla::HashNumber Hash(const nsACString& aKey)
    {
      return mozilla::HashString(aKey);
    }
    static bool Match(const nsACString& aKey, const TLDCacheEntry& aVal)
    {
      return aKey == aVal.mHost;
    }
  };

  TldCache mMruTable;
};

#endif // EffectiveTLDService_h
