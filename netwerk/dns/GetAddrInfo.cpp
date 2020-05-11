/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GetAddrInfo.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/net/DNS.h"
#include "prnetdb.h"
#include "nsHostResolver.h"
#include "nsError.h"
#include "mozilla/net/DNS.h"
#include <algorithm>
#include "prerror.h"

#include "mozilla/Logging.h"

#ifdef DNSQUERY_AVAILABLE
// There is a bug in windns.h where the type of parameter ppQueryResultsSet for
// DnsQuery_A is dependent on UNICODE being set. It should *always* be
// PDNS_RECORDA, but if UNICODE is set it is PDNS_RECORDW. To get around this
// we make sure that UNICODE is unset.
#  undef UNICODE
#  include <ws2tcpip.h>
#  undef GetAddrInfo
#  include <windns.h>
#endif

namespace mozilla {
namespace net {

static StaticRefPtr<NativeDNSResolverOverride> gOverrideService;

static LazyLogModule gGetAddrInfoLog("GetAddrInfo");
#define LOG(msg, ...) \
  MOZ_LOG(gGetAddrInfoLog, LogLevel::Debug, ("[DNS]: " msg, ##__VA_ARGS__))
#define LOG_WARNING(msg, ...) \
  MOZ_LOG(gGetAddrInfoLog, LogLevel::Warning, ("[DNS]: " msg, ##__VA_ARGS__))

#ifdef DNSQUERY_AVAILABLE
////////////////////////////
// WINDOWS IMPLEMENTATION //
////////////////////////////

// Ensure consistency of PR_* and AF_* constants to allow for legacy usage of
// PR_* constants with this API.
static_assert(PR_AF_INET == AF_INET && PR_AF_INET6 == AF_INET6 &&
                  PR_AF_UNSPEC == AF_UNSPEC,
              "PR_AF_* must match AF_*");

// If successful, returns in aResult a TTL value that is smaller or
// equal with the one already there. Gets the TTL value by calling
// to DnsQuery_A and iterating through the returned
// records to find the one with the smallest TTL value.
static MOZ_ALWAYS_INLINE nsresult _GetMinTTLForRequestType_Windows(
    const char* aHost, uint16_t aRequestType, unsigned int* aResult) {
  MOZ_ASSERT(aHost);
  MOZ_ASSERT(aResult);

  PDNS_RECORDA dnsData = nullptr;
  DNS_STATUS status = DnsQuery_A(
      aHost, aRequestType,
      (DNS_QUERY_STANDARD | DNS_QUERY_NO_NETBT | DNS_QUERY_NO_HOSTS_FILE |
       DNS_QUERY_NO_MULTICAST | DNS_QUERY_ACCEPT_TRUNCATED_RESPONSE |
       DNS_QUERY_DONT_RESET_TTL_VALUES),
      nullptr, &dnsData, nullptr);
  if (status == DNS_INFO_NO_RECORDS || status == DNS_ERROR_RCODE_NAME_ERROR ||
      !dnsData) {
    LOG("No DNS records found for %s. status=%X. aRequestType = %X\n", aHost,
        status, aRequestType);
    return NS_ERROR_FAILURE;
  } else if (status != NOERROR) {
    LOG_WARNING("DnsQuery_A failed with status %X.\n", status);
    return NS_ERROR_UNEXPECTED;
  }

  for (PDNS_RECORDA curRecord = dnsData; curRecord;
       curRecord = curRecord->pNext) {
    // Only records in the answer section are important
    if (curRecord->Flags.S.Section != DnsSectionAnswer) {
      continue;
    }

    if (curRecord->wType == aRequestType) {
      *aResult = std::min<unsigned int>(*aResult, curRecord->dwTtl);
    } else {
      LOG("Received unexpected record type %u in response for %s.\n",
          curRecord->wType, aHost);
    }
  }

  DnsFree(dnsData, DNS_FREE_TYPE::DnsFreeRecordList);
  return NS_OK;
}

static MOZ_ALWAYS_INLINE nsresult _GetTTLData_Windows(const nsACString& aHost,
                                                      uint32_t* aResult,
                                                      uint16_t aAddressFamily) {
  MOZ_ASSERT(!aHost.IsEmpty());
  MOZ_ASSERT(aResult);
  if (aAddressFamily != PR_AF_UNSPEC && aAddressFamily != PR_AF_INET &&
      aAddressFamily != PR_AF_INET6) {
    return NS_ERROR_UNEXPECTED;
  }

  // In order to avoid using ANY records which are not always implemented as a
  // "Gimme what you have" request in hostname resolvers, we should send A
  // and/or AAAA requests, based on the address family requested.
  unsigned int ttl = (unsigned int)-1;
  if (aAddressFamily == PR_AF_UNSPEC || aAddressFamily == PR_AF_INET) {
    _GetMinTTLForRequestType_Windows(aHost.BeginReading(), DNS_TYPE_A, &ttl);
  }
  if (aAddressFamily == PR_AF_UNSPEC || aAddressFamily == PR_AF_INET6) {
    _GetMinTTLForRequestType_Windows(aHost.BeginReading(), DNS_TYPE_AAAA, &ttl);
  }

  if (ttl == (unsigned int)-1) {
    LOG("No useable TTL found.");
    return NS_ERROR_FAILURE;
  }

  *aResult = ttl;
  return NS_OK;
}
#endif

////////////////////////////////////
// PORTABLE RUNTIME IMPLEMENTATION//
////////////////////////////////////

static MOZ_ALWAYS_INLINE nsresult
_GetAddrInfo_Portable(const nsACString& aCanonHost, uint16_t aAddressFamily,
                      uint16_t aFlags, AddrInfo** aAddrInfo) {
  MOZ_ASSERT(!aCanonHost.IsEmpty());
  MOZ_ASSERT(aAddrInfo);

  // We accept the same aFlags that nsHostResolver::ResolveHost accepts, but we
  // need to translate the aFlags into a form that PR_GetAddrInfoByName
  // accepts.
  int prFlags = PR_AI_ADDRCONFIG;
  if (!(aFlags & nsHostResolver::RES_CANON_NAME)) {
    prFlags |= PR_AI_NOCANONNAME;
  }

  // We need to remove IPv4 records manually because PR_GetAddrInfoByName
  // doesn't support PR_AF_INET6.
  bool disableIPv4 = aAddressFamily == PR_AF_INET6;
  if (disableIPv4) {
    aAddressFamily = PR_AF_UNSPEC;
  }

  PRAddrInfo* prai =
      PR_GetAddrInfoByName(aCanonHost.BeginReading(), aAddressFamily, prFlags);

  if (!prai) {
    return NS_ERROR_UNKNOWN_HOST;
  }

  nsAutoCString canonName;
  if (aFlags & nsHostResolver::RES_CANON_NAME) {
    canonName.Assign(PR_GetCanonNameFromAddrInfo(prai));
  }

  bool filterNameCollision =
      !(aFlags & nsHostResolver::RES_ALLOW_NAME_COLLISION);
  RefPtr<AddrInfo> ai(new AddrInfo(aCanonHost, prai, disableIPv4,
                                   filterNameCollision, canonName));
  PR_FreeAddrInfo(prai);
  if (ai->mAddresses.isEmpty()) {
    return NS_ERROR_UNKNOWN_HOST;
  }

  ai.forget(aAddrInfo);

  return NS_OK;
}

//////////////////////////////////////
// COMMON/PLATFORM INDEPENDENT CODE //
//////////////////////////////////////
nsresult GetAddrInfoInit() {
  LOG("Initializing GetAddrInfo.\n");
  return NS_OK;
}

nsresult GetAddrInfoShutdown() {
  LOG("Shutting down GetAddrInfo.\n");
  return NS_OK;
}

bool FindAddrOverride(const nsACString& aHost, uint16_t aAddressFamily,
                      uint16_t aFlags, AddrInfo** aAddrInfo) {
  RefPtr<NativeDNSResolverOverride> overrideService = gOverrideService;
  if (!overrideService) {
    return false;
  }
  AutoReadLock lock(overrideService->mLock);
  nsTArray<PRNetAddr>* overrides = overrideService->mOverrides.GetValue(aHost);
  if (!overrides) {
    return false;
  }
  nsCString* cname = nullptr;
  if (aFlags & nsHostResolver::RES_CANON_NAME) {
    cname = overrideService->mCnames.GetValue(aHost);
  }

  RefPtr<AddrInfo> ai;

  if (!cname) {
    ai = new AddrInfo(aHost, 0);
  } else {
    ai = new AddrInfo(aHost, *cname, 0);
  }

  for (const auto& ip : *overrides) {
    if (aAddressFamily != AF_UNSPEC && ip.raw.family != aAddressFamily) {
      continue;
    }
    ai->AddAddress(new NetAddrElement(&ip));
  }

  ai.forget(aAddrInfo);
  return true;
}

nsresult GetAddrInfo(const nsACString& aHost, uint16_t aAddressFamily,
                     uint16_t aFlags, AddrInfo** aAddrInfo, bool aGetTtl) {
  if (NS_WARN_IF(aHost.IsEmpty()) || NS_WARN_IF(!aAddrInfo)) {
    return NS_ERROR_NULL_POINTER;
  }

#ifdef DNSQUERY_AVAILABLE
  // The GetTTLData needs the canonical name to function properly
  if (aGetTtl) {
    aFlags |= nsHostResolver::RES_CANON_NAME;
  }
#endif

  // If there is an override for this host, then we synthetize a result.
  if (gOverrideService &&
      FindAddrOverride(aHost, aAddressFamily, aFlags, aAddrInfo)) {
    return NS_OK;
  }

  nsAutoCString host(aHost);
  if (gNativeIsLocalhost) {
    // pretend we use the given host but use IPv4 localhost instead!
    host = NS_LITERAL_CSTRING("localhost");
    aAddressFamily = PR_AF_INET;
  }

  *aAddrInfo = nullptr;
  nsresult rv = _GetAddrInfo_Portable(host, aAddressFamily, aFlags, aAddrInfo);

#ifdef DNSQUERY_AVAILABLE
  if (aGetTtl && NS_SUCCEEDED(rv)) {
    // Figure out the canonical name, or if that fails, just use the host name
    // we have.
    nsAutoCString name;
    if (*aAddrInfo != nullptr && !(*aAddrInfo)->mCanonicalName.IsEmpty()) {
      name = (*aAddrInfo)->mCanonicalName;
    } else {
      name = host;
    }

    LOG("Getting TTL for %s (cname = %s).", host.get(), name.get());
    uint32_t ttl = 0;
    nsresult ttlRv = _GetTTLData_Windows(name, &ttl, aAddressFamily);
    if (NS_SUCCEEDED(ttlRv)) {
      (*aAddrInfo)->ttl = ttl;
      LOG("Got TTL %u for %s (name = %s).", ttl, host.get(), name.get());
    } else {
      LOG_WARNING("Could not get TTL for %s (cname = %s).", host.get(),
                  name.get());
    }
  }
#endif

  return rv;
}

// static
already_AddRefed<nsINativeDNSResolverOverride>
NativeDNSResolverOverride::GetSingleton() {
  if (gOverrideService) {
    return do_AddRef(gOverrideService);
  }

  gOverrideService = new NativeDNSResolverOverride();
  ClearOnShutdown(&gOverrideService);
  return do_AddRef(gOverrideService);
}

NS_IMPL_ISUPPORTS(NativeDNSResolverOverride, nsINativeDNSResolverOverride)

NS_IMETHODIMP NativeDNSResolverOverride::AddIPOverride(
    const nsACString& aHost, const nsACString& aIPLiteral) {
  PRNetAddr tempAddr;
  // Unfortunately, PR_StringToNetAddr does not properly initialize
  // the output buffer in the case of IPv6 input. See bug 223145.
  memset(&tempAddr, 0, sizeof(PRNetAddr));

  if (PR_StringToNetAddr(nsCString(aIPLiteral).get(), &tempAddr) !=
      PR_SUCCESS) {
    return NS_ERROR_UNEXPECTED;
  }

  AutoWriteLock lock(mLock);
  auto& overrides = mOverrides.GetOrInsert(aHost);
  overrides.AppendElement(tempAddr);

  return NS_OK;
}

NS_IMETHODIMP NativeDNSResolverOverride::SetCnameOverride(
    const nsACString& aHost, const nsACString& aCNAME) {
  if (aCNAME.IsEmpty()) {
    return NS_ERROR_UNEXPECTED;
  }

  AutoWriteLock lock(mLock);
  mCnames.Put(aHost, nsCString(aCNAME));

  return NS_OK;
}

NS_IMETHODIMP NativeDNSResolverOverride::ClearHostOverride(
    const nsACString& aHost) {
  AutoWriteLock lock(mLock);
  mCnames.Remove(aHost);
  auto overrides = mOverrides.GetAndRemove(aHost);
  if (!overrides) {
    return NS_OK;
  }

  overrides->Clear();
  return NS_OK;
}

NS_IMETHODIMP NativeDNSResolverOverride::ClearOverrides() {
  AutoWriteLock lock(mLock);
  mOverrides.Clear();
  mCnames.Clear();
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
