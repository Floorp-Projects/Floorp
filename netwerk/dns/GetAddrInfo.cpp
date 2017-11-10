/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GetAddrInfo.h"
#include "mozilla/net/DNS.h"
#include "prnetdb.h"
#include "nsHostResolver.h"
#include "nsError.h"
#include "mozilla/Mutex.h"
#include "nsAutoPtr.h"
#include "mozilla/StaticPtr.h"
#include "MainThreadUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/net/DNS.h"
#include <algorithm>
#include "prerror.h"

#include "mozilla/Logging.h"

#if DNSQUERY_AVAILABLE
// There is a bug in windns.h where the type of parameter ppQueryResultsSet for
// DnsQuery_A is dependent on UNICODE being set. It should *always* be
// PDNS_RECORDA, but if UNICODE is set it is PDNS_RECORDW. To get around this
// we make sure that UNICODE is unset.
#undef UNICODE
#include <ws2tcpip.h>
#undef GetAddrInfo
#include <windns.h>
#endif

namespace mozilla {
namespace net {

static LazyLogModule gGetAddrInfoLog("GetAddrInfo");
#define LOG(msg, ...) \
  MOZ_LOG(gGetAddrInfoLog, LogLevel::Debug, ("[DNS]: " msg, ##__VA_ARGS__))
#define LOG_WARNING(msg, ...) \
  MOZ_LOG(gGetAddrInfoLog, LogLevel::Warning, ("[DNS]: " msg, ##__VA_ARGS__))

#if DNSQUERY_AVAILABLE
////////////////////////////
// WINDOWS IMPLEMENTATION //
////////////////////////////

// Ensure consistency of PR_* and AF_* constants to allow for legacy usage of
// PR_* constants with this API.
static_assert(PR_AF_INET == AF_INET && PR_AF_INET6 == AF_INET6
    && PR_AF_UNSPEC == AF_UNSPEC, "PR_AF_* must match AF_*");

// We intentionally leak this mutex. This is because we can run into a
// situation where the worker threads are still running until the process
// is actually fully shut down, and at any time one of those worker
// threads can access gDnsapiInfoLock.
static OffTheBooksMutex* gDnsapiInfoLock = nullptr;

struct DnsapiInfo
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DnsapiInfo);

  HMODULE mLibrary;
  decltype(&DnsQuery_A) mDnsQueryFunc;
  decltype(&DnsFree) mDnsFreeFunc;

private:
  // This will either be called during shutdown of the GetAddrInfo module, or
  // when a worker thread is done doing a lookup (ie: within
  // _GetAddrInfo_Windows). Note that the lock must be held when this is
  // called.
  ~DnsapiInfo()
  {
    if (gDnsapiInfoLock) {
      gDnsapiInfoLock->AssertCurrentThreadOwns();
    } else {
      MOZ_ASSERT_UNREACHABLE("No mutex available during GetAddrInfo "
                             "shutdown.");
      return;
    }

    LOG("Freeing Dnsapi.dll");
    MOZ_ASSERT(mLibrary);
    DebugOnly<BOOL> rv = FreeLibrary(mLibrary);
    NS_WARNING_ASSERTION(rv, "Failed to free Dnsapi.dll.");
  }
};

static StaticRefPtr<DnsapiInfo> gDnsapiInfo;

static MOZ_ALWAYS_INLINE nsresult
_GetAddrInfoInit_Windows()
{
  // This is necessary to ensure strict thread safety because if two threads
  // run this function at the same time they can potentially create two
  // mutexes.
  MOZ_ASSERT(NS_IsMainThread(),
             "Do not initialize GetAddrInfo off main thread!");

  if (!gDnsapiInfoLock) {
    gDnsapiInfoLock = new OffTheBooksMutex("GetAddrInfo.cpp::gDnsapiInfoLock");
  }
  OffTheBooksMutexAutoLock lock(*gDnsapiInfoLock);

  if (gDnsapiInfo) {
    MOZ_ASSERT_UNREACHABLE("GetAddrInfo is being initialized multiple times!");
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  HMODULE library = LoadLibraryA("Dnsapi.dll");
  if (NS_WARN_IF(!library)) {
    return NS_ERROR_FAILURE;
  }

  FARPROC dnsQueryFunc = GetProcAddress(library, "DnsQuery_A");
  FARPROC dnsFreeFunc = GetProcAddress(library, "DnsFree");
  if (NS_WARN_IF(!dnsQueryFunc) || NS_WARN_IF(!dnsFreeFunc)) {
    DebugOnly<BOOL> rv = FreeLibrary(library);
    NS_WARNING_ASSERTION(rv, "Failed to free Dnsapi.dll.");
    return NS_ERROR_FAILURE;
  }

  DnsapiInfo* info = new DnsapiInfo;
  info->mLibrary = library;
  info->mDnsQueryFunc = (decltype(info->mDnsQueryFunc)) dnsQueryFunc;
  info->mDnsFreeFunc = (decltype(info->mDnsFreeFunc)) dnsFreeFunc;
  gDnsapiInfo = info;

  return NS_OK;
}

static MOZ_ALWAYS_INLINE nsresult
_GetAddrInfoShutdown_Windows()
{
  OffTheBooksMutexAutoLock lock(*gDnsapiInfoLock);

  if (NS_WARN_IF(!gDnsapiInfo) || NS_WARN_IF(!gDnsapiInfoLock)) {
    MOZ_ASSERT_UNREACHABLE("GetAddrInfo not initialized!");
    return NS_ERROR_NOT_INITIALIZED;
  }

  gDnsapiInfo = nullptr;

  return NS_OK;
}

// If successful, returns in aResult a TTL value that is smaller or
// equal with the one already there. Gets the TTL value by calling
// to dnsapi->mDnsQueryFunc and iterating through the returned
// records to find the one with the smallest TTL value.
static MOZ_ALWAYS_INLINE nsresult
_GetMinTTLForRequestType_Windows(DnsapiInfo * dnsapi, const char* aHost,
                                 uint16_t aRequestType, unsigned int* aResult)
{
  MOZ_ASSERT(dnsapi);
  MOZ_ASSERT(aHost);
  MOZ_ASSERT(aResult);

  PDNS_RECORDA dnsData = nullptr;
  DNS_STATUS status = dnsapi->mDnsQueryFunc(
    aHost,
    aRequestType,
    (DNS_QUERY_STANDARD | DNS_QUERY_NO_NETBT | DNS_QUERY_NO_HOSTS_FILE
      | DNS_QUERY_NO_MULTICAST | DNS_QUERY_ACCEPT_TRUNCATED_RESPONSE
      | DNS_QUERY_DONT_RESET_TTL_VALUES),
    nullptr,
    &dnsData,
    nullptr);
  if (status == DNS_INFO_NO_RECORDS || status == DNS_ERROR_RCODE_NAME_ERROR
      || !dnsData) {
    LOG("No DNS records found for %s. status=%X. aRequestType = %X\n",
        aHost, status, aRequestType);
    return NS_ERROR_FAILURE;
  } else if (status != NOERROR) {
    LOG_WARNING("DnsQuery_A failed with status %X.\n", status);
    return NS_ERROR_UNEXPECTED;
  }

  for (PDNS_RECORDA curRecord = dnsData; curRecord; curRecord = curRecord->pNext) {
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

  dnsapi->mDnsFreeFunc(dnsData, DNS_FREE_TYPE::DnsFreeRecordList);
  return NS_OK;
}

static MOZ_ALWAYS_INLINE nsresult
_GetTTLData_Windows(const char* aHost, uint32_t* aResult, uint16_t aAddressFamily)
{
  MOZ_ASSERT(aHost);
  MOZ_ASSERT(aResult);
  if (aAddressFamily != PR_AF_UNSPEC &&
      aAddressFamily != PR_AF_INET &&
      aAddressFamily != PR_AF_INET6) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<DnsapiInfo> dnsapi = nullptr;
  {
    OffTheBooksMutexAutoLock lock(*gDnsapiInfoLock);
    dnsapi = gDnsapiInfo;
  }

  if (!dnsapi) {
    LOG_WARNING("GetAddrInfo has been shutdown or has not been initialized.");
    return NS_ERROR_NOT_INITIALIZED;
  }

  // In order to avoid using ANY records which are not always implemented as a
  // "Gimme what you have" request in hostname resolvers, we should send A
  // and/or AAAA requests, based on the address family requested.
  unsigned int ttl = (unsigned int)-1;
  if (aAddressFamily == PR_AF_UNSPEC || aAddressFamily == PR_AF_INET) {
    _GetMinTTLForRequestType_Windows(dnsapi, aHost, DNS_TYPE_A, &ttl);
  }
  if (aAddressFamily == PR_AF_UNSPEC || aAddressFamily == PR_AF_INET6) {
    _GetMinTTLForRequestType_Windows(dnsapi, aHost, DNS_TYPE_AAAA, &ttl);
  }

  {
    // dnsapi's destructor is not thread-safe, so we release explicitly here
    OffTheBooksMutexAutoLock lock(*gDnsapiInfoLock);
    dnsapi = nullptr;
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
_GetAddrInfo_Portable(const char* aCanonHost, uint16_t aAddressFamily,
                      uint16_t aFlags, const char* aNetworkInterface,
                      AddrInfo** aAddrInfo)
{
  MOZ_ASSERT(aCanonHost);
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

  PRAddrInfo* prai = PR_GetAddrInfoByName(aCanonHost, aAddressFamily, prFlags);

  if (!prai) {
    return NS_ERROR_UNKNOWN_HOST;
  }

  const char* canonName = nullptr;
  if (aFlags & nsHostResolver::RES_CANON_NAME) {
    canonName = PR_GetCanonNameFromAddrInfo(prai);
  }

  bool filterNameCollision = !(aFlags & nsHostResolver::RES_ALLOW_NAME_COLLISION);
  nsAutoPtr<AddrInfo> ai(new AddrInfo(aCanonHost, prai, disableIPv4,
                                      filterNameCollision, canonName));
  PR_FreeAddrInfo(prai);
  if (ai->mAddresses.isEmpty()) {
    return NS_ERROR_UNKNOWN_HOST;
  }

  *aAddrInfo = ai.forget();

  return NS_OK;
}

//////////////////////////////////////
// COMMON/PLATFORM INDEPENDENT CODE //
//////////////////////////////////////
nsresult
GetAddrInfoInit() {
  LOG("Initializing GetAddrInfo.\n");

#if DNSQUERY_AVAILABLE
  return _GetAddrInfoInit_Windows();
#else
  return NS_OK;
#endif
}

nsresult
GetAddrInfoShutdown() {
  LOG("Shutting down GetAddrInfo.\n");

#if DNSQUERY_AVAILABLE
  return _GetAddrInfoShutdown_Windows();
#else
  return NS_OK;
#endif
}

nsresult
GetAddrInfo(const char* aHost, uint16_t aAddressFamily, uint16_t aFlags,
            const char* aNetworkInterface, AddrInfo** aAddrInfo, bool aGetTtl)
{
  if (NS_WARN_IF(!aHost) || NS_WARN_IF(!aAddrInfo)) {
    return NS_ERROR_NULL_POINTER;
  }

#if DNSQUERY_AVAILABLE
  // The GetTTLData needs the canonical name to function properly
  if (aGetTtl) {
    aFlags |= nsHostResolver::RES_CANON_NAME;
  }
#endif

  *aAddrInfo = nullptr;
  nsresult rv = _GetAddrInfo_Portable(aHost, aAddressFamily, aFlags,
                                      aNetworkInterface, aAddrInfo);

#if DNSQUERY_AVAILABLE
  if (aGetTtl && NS_SUCCEEDED(rv)) {
    // Figure out the canonical name, or if that fails, just use the host name
    // we have.
    const char *name = nullptr;
    if (*aAddrInfo != nullptr && (*aAddrInfo)->mCanonicalName) {
      name = (*aAddrInfo)->mCanonicalName;
    } else {
      name = aHost;
    }

    LOG("Getting TTL for %s (cname = %s).", aHost, name);
    uint32_t ttl = 0;
    nsresult ttlRv = _GetTTLData_Windows(name, &ttl, aAddressFamily);
    if (NS_SUCCEEDED(ttlRv)) {
      (*aAddrInfo)->ttl = ttl;
      LOG("Got TTL %u for %s (name = %s).", ttl, aHost, name);
    } else {
      LOG_WARNING("Could not get TTL for %s (cname = %s).", aHost, name);
    }
  }
#endif

  return rv;
}

} // namespace net
} // namespace mozilla
