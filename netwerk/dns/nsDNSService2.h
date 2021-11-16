/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDNSService2_h__
#define nsDNSService2_h__

#include "nsClassHashtable.h"
#include "nsPIDNSService.h"
#include "nsIIDNService.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsHostResolver.h"
#include "nsString.h"
#include "nsTHashSet.h"
#include "nsHashKeys.h"
#include "mozilla/Mutex.h"
#include "mozilla/Attributes.h"
#include "TRRService.h"

class nsAuthSSPI;

class nsDNSService final : public nsPIDNSService,
                           public nsIObserver,
                           public nsIMemoryReporter {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSPIDNSSERVICE
  NS_DECL_NSIDNSSERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMEMORYREPORTER

  nsDNSService() = default;

  static already_AddRefed<nsIDNSService> GetXPCOMSingleton();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  bool GetOffline() const;

 protected:
  friend class nsAuthSSPI;

  nsresult DeprecatedSyncResolve(
      const nsACString& aHostname, uint32_t flags,
      const mozilla::OriginAttributes& aOriginAttributes,
      nsIDNSRecord** result);

 private:
  ~nsDNSService() = default;

  nsresult ReadPrefs(const char* name);
  static already_AddRefed<nsDNSService> GetSingleton();

  uint16_t GetAFForLookup(const nsACString& host, uint32_t flags);

  nsresult PreprocessHostname(bool aLocalDomain, const nsACString& aInput,
                              nsIIDNService* aIDN, nsACString& aACE);

  nsresult AsyncResolveInternal(
      const nsACString& aHostname, uint16_t type, uint32_t flags,
      nsIDNSResolverInfo* aResolver, nsIDNSListener* aListener,
      nsIEventTarget* target_,
      const mozilla::OriginAttributes& aOriginAttributes,
      nsICancelable** result);

  nsresult CancelAsyncResolveInternal(
      const nsACString& aHostname, uint16_t aType, uint32_t aFlags,
      nsIDNSResolverInfo* aResolver, nsIDNSListener* aListener,
      nsresult aReason, const mozilla::OriginAttributes& aOriginAttributes);

  nsresult ResolveInternal(const nsACString& aHostname, uint32_t flags,
                           const mozilla::OriginAttributes& aOriginAttributes,
                           nsIDNSRecord** result);

  bool DNSForbiddenByActiveProxy(const nsACString& aHostname, uint32_t flags);

  // Locks the mutex and returns an addreffed resolver. May return null.
  already_AddRefed<nsHostResolver> GetResolverLocked();

  RefPtr<nsHostResolver> mResolver;
  nsCOMPtr<nsIIDNService> mIDN;

  // mLock protects access to mResolver, mLocalDomains, mIPv4OnlyDomains and
  // mFailedSVCDomainNames
  mozilla::Mutex mLock{"nsDNSServer.mLock"};

  // mIPv4OnlyDomains is a comma-separated list of domains for which only
  // IPv4 DNS lookups are performed. This allows the user to disable IPv6 on
  // a per-domain basis and work around broken DNS servers. See bug 68796.
  nsCString mIPv4OnlyDomains;
  nsCString mForceResolve;
  bool mDisableIPv6 = false;
  bool mDisablePrefetch = false;
  bool mBlockDotOnion = false;
  bool mNotifyResolution = false;
  bool mOfflineLocalhost = false;
  bool mForceResolveOn = false;
  nsTHashSet<nsCString> mLocalDomains;
  RefPtr<mozilla::net::TRRService> mTrrService;
  mozilla::Atomic<bool, mozilla::Relaxed> mHasSocksProxy{false};

  uint32_t mResCacheEntries = 0;
  uint32_t mResCacheExpiration = 0;
  uint32_t mResCacheGrace = 0;
  bool mResolverPrefsUpdated = false;
  bool mODoHActivated = false;
  nsClassHashtable<nsCStringHashKey, nsTArray<nsCString>> mFailedSVCDomainNames;
};

already_AddRefed<nsIDNSService> GetOrInitDNSService();

#endif  // nsDNSService2_h__
