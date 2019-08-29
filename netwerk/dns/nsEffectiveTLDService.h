//* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EffectiveTLDService_h
#define EffectiveTLDService_h

#include "nsIEffectiveTLDService.h"

#include "mozilla/AutoMemMap.h"
#include "mozilla/Attributes.h"
#include "mozilla/Dafsa.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/MruCache.h"
#include "mozilla/RWLock.h"

#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsString.h"

class nsIIDNService;

class nsEffectiveTLDService final : public nsIEffectiveTLDService,
                                    public nsIObserver,
                                    public nsIMemoryReporter {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEFFECTIVETLDSERVICE
  NS_DECL_NSIMEMORYREPORTER
  NS_DECL_NSIOBSERVER

  nsEffectiveTLDService();
  nsresult Init();

  static nsEffectiveTLDService* GetInstance();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

 private:
  nsresult GetBaseDomainInternal(nsCString& aHostname, int32_t aAdditionalParts,
                                 nsACString& aBaseDomain);
  nsresult NormalizeHostname(nsCString& aHostname);
  ~nsEffectiveTLDService();

  nsCOMPtr<nsIIDNService> mIDNService;

  // The DAFSA provides a compact encoding of the rather large eTLD list.
  mozilla::Maybe<mozilla::Dafsa> mGraph;

  // Memory map used for a new updated dafsa
  mozilla::loader::AutoMemMap mDafsaMap;

  // Lock for mGraph and mDafsaMap
  mozilla::RWLock mGraphLock;

  // Note that the cache entries here can record entries that were cached
  // successfully or unsuccessfully.  mResult must be checked before using an
  // entry.  If it's a success error code, the cache entry is valid and can be
  // used.
  struct TLDCacheEntry {
    nsCString mHost;
    nsCString mBaseDomain;
    nsresult mResult;
  };

  // We use a small most recently used cache to compensate for DAFSA lookups
  // being slightly slower than a binary search on a larger table of strings.
  //
  // We first check the cache for a matching result and avoid a DAFSA lookup
  // if a match is found. Otherwise we lookup the domain in the DAFSA and then
  // cache the result. During standard browsing the same domains are repeatedly
  // fed into |GetBaseDomainInternal| so this ends up being an effective
  // mitigation getting about a 99% hit rate with four tabs open.
  struct TldCache
      : public mozilla::MruCache<nsACString, TLDCacheEntry, TldCache> {
    static mozilla::HashNumber Hash(const nsACString& aKey) {
      return mozilla::HashString(aKey);
    }
    static bool Match(const nsACString& aKey, const TLDCacheEntry& aVal) {
      return aKey == aVal.mHost;
    }
  };

  TldCache mMruTable;
};

#endif  // EffectiveTLDService_h
