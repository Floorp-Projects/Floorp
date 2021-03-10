/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef netwerk_dns_GetAddrInfo_h
#define netwerk_dns_GetAddrInfo_h

#include "nsError.h"
#include "nscore.h"
#include "nsINativeDNSResolverOverride.h"
#include "nsHashKeys.h"
#include "nsTHashMap.h"
#include "mozilla/RWLock.h"
#include "nsTArray.h"
#include "prio.h"

#if defined(XP_WIN)
#  define DNSQUERY_AVAILABLE 1
#else
#  undef DNSQUERY_AVAILABLE
#endif

namespace mozilla {
namespace net {

class AddrInfo;

/**
 * Look up a host by name. Mostly equivalent to getaddrinfo(host, NULL, ...) of
 * RFC 3493.
 *
 * @param aHost[in] Character string defining the host name of interest
 * @param aAddressFamily[in] May be AF_INET, AF_INET6, or AF_UNSPEC.
 * @param aFlags[in] May be either PR_AI_ADDRCONFIG or
 *     PR_AI_ADDRCONFIG | PR_AI_NOCANONNAME. Include PR_AI_NOCANONNAME to
 *     suppress the determination of the canonical name corresponding to
 *     hostname (PR_AI_NOCANONNAME will be ignored if the TTL is retrieved).
 * @param aAddrInfo[out] Will point to the results of the host lookup, or be
 *     null if the lookup failed.
 * @param aGetTtl[in] If true, the TTL will be retrieved if DNS provides the
 *     answers..
 */
nsresult GetAddrInfo(const nsACString& aHost, uint16_t aAddressFamily,
                     uint16_t aFlags, AddrInfo** aAddrInfo, bool aGetTtl);

/**
 * Initialize the GetAddrInfo module.
 *
 * GetAddrInfoShutdown() should be called for every time this function is
 * called.
 */
nsresult GetAddrInfoInit();

/**
 * Shutdown the GetAddrInfo module.
 *
 * This function should be called for every time GetAddrInfoInit() is called.
 * An assertion may throw (but is not guarenteed) if this function is called
 * too many times.
 */
nsresult GetAddrInfoShutdown();

class NativeDNSResolverOverride : public nsINativeDNSResolverOverride {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINATIVEDNSRESOLVEROVERRIDE
 public:
  NativeDNSResolverOverride() : mLock("NativeDNSResolverOverride") {}

  static already_AddRefed<nsINativeDNSResolverOverride> GetSingleton();

 private:
  virtual ~NativeDNSResolverOverride() = default;
  mozilla::RWLock mLock;

  nsTHashMap<nsCStringHashKey, nsTArray<PRNetAddr>> mOverrides;
  nsTHashMap<nsCStringHashKey, nsCString> mCnames;

  friend bool FindAddrOverride(const nsACString& aHost, uint16_t aAddressFamily,
                               uint16_t aFlags, AddrInfo** aAddrInfo);
};

}  // namespace net
}  // namespace mozilla

#endif  // netwerk_dns_GetAddrInfo_h
