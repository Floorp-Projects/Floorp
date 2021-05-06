/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This service reads a file of rules describing TLD-like domain names.  For a
// complete description of the expected file format and parsing rules, see
// http://wiki.mozilla.org/Gecko:Effective_TLD_Service

#include "mozilla/ArrayUtils.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/TextUtils.h"

#include "MainThreadUtils.h"
#include "nsCRT.h"
#include "nsEffectiveTLDService.h"
#include "nsIFile.h"
#include "nsIIDNService.h"
#include "nsIObserverService.h"
#include "nsIURI.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/net/DNS.h"

namespace etld_dafsa {

// Generated file that includes kDafsa
#include "etld_data.inc"

}  // namespace etld_dafsa

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsEffectiveTLDService, nsIEffectiveTLDService,
                  nsIMemoryReporter, nsIObserver)

// ----------------------------------------------------------------------

static nsEffectiveTLDService* gService = nullptr;

nsEffectiveTLDService::nsEffectiveTLDService()
    : mIDNService(), mGraphLock("nsEffectiveTLDService::mGraph") {
  mGraph.emplace(etld_dafsa::kDafsa);
}

nsresult nsEffectiveTLDService::Init() {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->AddObserver(this, "public-suffix-list-updated", false);

  if (gService) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  nsresult rv;
  mIDNService = do_GetService(NS_IDNSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  gService = this;
  RegisterWeakMemoryReporter(this);

  return NS_OK;
}

NS_IMETHODIMP nsEffectiveTLDService::Observe(nsISupports* aSubject,
                                             const char* aTopic,
                                             const char16_t* aData) {
  /**
   * Signal sent from netwerk/dns/PublicSuffixList.jsm
   * aSubject is the nsIFile object for dafsa.bin
   * aData is the absolute path to the dafsa.bin file (not used)
   */
  if (aSubject && (nsCRT::strcmp(aTopic, "public-suffix-list-updated") == 0)) {
    nsCOMPtr<nsIFile> mDafsaBinFile(do_QueryInterface(aSubject));
    NS_ENSURE_TRUE(mDafsaBinFile, NS_ERROR_ILLEGAL_VALUE);

    AutoWriteLock lock(mGraphLock);
    // Reset mGraph with kDafsa in case reassigning to mDafsaMap fails
    mGraph.reset();
    mGraph.emplace(etld_dafsa::kDafsa);

    mDafsaMap.reset();
    mMruTable.Clear();

    MOZ_TRY(mDafsaMap.init(mDafsaBinFile));

    size_t size = mDafsaMap.size();
    const uint8_t* remoteDafsaPtr = mDafsaMap.get<uint8_t>().get();

    auto remoteDafsa = mozilla::Span(remoteDafsaPtr, size);

    mGraph.reset();
    mGraph.emplace(remoteDafsa);
  }
  return NS_OK;
}

nsEffectiveTLDService::~nsEffectiveTLDService() {
  UnregisterWeakMemoryReporter(this);
  if (mIDNService) {
    // Only clear gService if Init() finished successfully.
    gService = nullptr;
  }
}

// static
nsEffectiveTLDService* nsEffectiveTLDService::GetInstance() {
  if (gService) {
    return gService;
  }
  nsCOMPtr<nsIEffectiveTLDService> tldService =
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  if (!tldService) {
    return nullptr;
  }
  MOZ_ASSERT(
      gService,
      "gService must have been initialized in nsEffectiveTLDService::Init");
  return gService;
}

MOZ_DEFINE_MALLOC_SIZE_OF(EffectiveTLDServiceMallocSizeOf)

// The amount of heap memory measured here is tiny. It used to be bigger when
// nsEffectiveTLDService used a separate hash table instead of binary search.
// Nonetheless, we keep this code here in anticipation of bug 1083971 which will
// change ETLDEntries::entries to a heap-allocated array modifiable at runtime.
NS_IMETHODIMP
nsEffectiveTLDService::CollectReports(nsIHandleReportCallback* aHandleReport,
                                      nsISupports* aData, bool aAnonymize) {
  MOZ_COLLECT_REPORT("explicit/network/effective-TLD-service", KIND_HEAP,
                     UNITS_BYTES,
                     SizeOfIncludingThis(EffectiveTLDServiceMallocSizeOf),
                     "Memory used by the effective TLD service.");

  return NS_OK;
}

size_t nsEffectiveTLDService::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) {
  size_t n = aMallocSizeOf(this);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mIDNService

  return n;
}

// External function for dealing with URI's correctly.
// Pulls out the host portion from an nsIURI, and calls through to
// GetPublicSuffixFromHost().
NS_IMETHODIMP
nsEffectiveTLDService::GetPublicSuffix(nsIURI* aURI,
                                       nsACString& aPublicSuffix) {
  NS_ENSURE_ARG_POINTER(aURI);

  nsAutoCString host;
  nsresult rv = NS_GetInnermostURIHost(aURI, host);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return GetBaseDomainInternal(host, 0, false, aPublicSuffix);
}

NS_IMETHODIMP
nsEffectiveTLDService::GetKnownPublicSuffix(nsIURI* aURI,
                                            nsACString& aPublicSuffix) {
  NS_ENSURE_ARG_POINTER(aURI);

  nsAutoCString host;
  nsresult rv = NS_GetInnermostURIHost(aURI, host);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return GetBaseDomainInternal(host, 0, true, aPublicSuffix);
}

// External function for dealing with URI's correctly.
// Pulls out the host portion from an nsIURI, and calls through to
// GetBaseDomainFromHost().
NS_IMETHODIMP
nsEffectiveTLDService::GetBaseDomain(nsIURI* aURI, uint32_t aAdditionalParts,
                                     nsACString& aBaseDomain) {
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_TRUE(((int32_t)aAdditionalParts) >= 0, NS_ERROR_INVALID_ARG);

  nsAutoCString host;
  nsresult rv = NS_GetInnermostURIHost(aURI, host);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return GetBaseDomainInternal(host, aAdditionalParts + 1, false, aBaseDomain);
}

// External function for dealing with a host string directly: finds the public
// suffix (e.g. co.uk) for the given hostname. See GetBaseDomainInternal().
NS_IMETHODIMP
nsEffectiveTLDService::GetPublicSuffixFromHost(const nsACString& aHostname,
                                               nsACString& aPublicSuffix) {
  // Create a mutable copy of the hostname and normalize it to ACE.
  // This will fail if the hostname includes invalid characters.
  nsAutoCString normHostname(aHostname);
  nsresult rv = NormalizeHostname(normHostname);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return GetBaseDomainInternal(normHostname, 0, false, aPublicSuffix);
}

NS_IMETHODIMP
nsEffectiveTLDService::GetKnownPublicSuffixFromHost(const nsACString& aHostname,
                                                    nsACString& aPublicSuffix) {
  // Create a mutable copy of the hostname and normalize it to ACE.
  // This will fail if the hostname includes invalid characters.
  nsAutoCString normHostname(aHostname);
  nsresult rv = NormalizeHostname(normHostname);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return GetBaseDomainInternal(normHostname, 0, true, aPublicSuffix);
}

// External function for dealing with a host string directly: finds the base
// domain (e.g. www.co.uk) for the given hostname and number of subdomain parts
// requested. See GetBaseDomainInternal().
NS_IMETHODIMP
nsEffectiveTLDService::GetBaseDomainFromHost(const nsACString& aHostname,
                                             uint32_t aAdditionalParts,
                                             nsACString& aBaseDomain) {
  NS_ENSURE_TRUE(((int32_t)aAdditionalParts) >= 0, NS_ERROR_INVALID_ARG);

  // Create a mutable copy of the hostname and normalize it to ACE.
  // This will fail if the hostname includes invalid characters.
  nsAutoCString normHostname(aHostname);
  nsresult rv = NormalizeHostname(normHostname);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return GetBaseDomainInternal(normHostname, aAdditionalParts + 1, false,
                               aBaseDomain);
}

NS_IMETHODIMP
nsEffectiveTLDService::GetNextSubDomain(const nsACString& aHostname,
                                        nsACString& aBaseDomain) {
  // Create a mutable copy of the hostname and normalize it to ACE.
  // This will fail if the hostname includes invalid characters.
  nsAutoCString normHostname(aHostname);
  nsresult rv = NormalizeHostname(normHostname);
  NS_ENSURE_SUCCESS(rv, rv);

  return GetBaseDomainInternal(normHostname, -1, false, aBaseDomain);
}

// Finds the base domain for a host, with requested number of additional parts.
// This will fail, generating an error, if the host is an IPv4/IPv6 address,
// if more subdomain parts are requested than are available, or if the hostname
// includes characters that are not valid in a URL. Normalization is performed
// on the host string and the result will be in UTF8.
nsresult nsEffectiveTLDService::GetBaseDomainInternal(
    nsCString& aHostname, int32_t aAdditionalParts, bool aOnlyKnownPublicSuffix,
    nsACString& aBaseDomain) {
  const int kExceptionRule = 1;
  const int kWildcardRule = 2;

  if (aHostname.IsEmpty()) {
    return NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS;
  }

  // chomp any trailing dot, and keep track of it for later
  bool trailingDot = aHostname.Last() == '.';
  if (trailingDot) {
    aHostname.Truncate(aHostname.Length() - 1);
  }

  // check the edge cases of the host being '.' or having a second trailing '.',
  // since subsequent checks won't catch it.
  if (aHostname.IsEmpty() || aHostname.Last() == '.') {
    return NS_ERROR_INVALID_ARG;
  }

  // Lookup in the cache if this is a normal query. This is restricted to
  // main thread-only as the cache is not thread-safe.
  Maybe<TldCache::Entry> entry;
  if (aAdditionalParts == 1 && NS_IsMainThread()) {
    auto p = mMruTable.Lookup(aHostname);
    if (p) {
      if (NS_FAILED(p.Data().mResult)) {
        return p.Data().mResult;
      }

      // There was a match, just return the cached value.
      aBaseDomain = p.Data().mBaseDomain;
      if (trailingDot) {
        aBaseDomain.Append('.');
      }

      return NS_OK;
    }

    entry = Some(p);
  }

  // Check if we're dealing with an IPv4/IPv6 hostname, and return
  if (mozilla::net::HostIsIPLiteral(aHostname)) {
    // Update the MRU table if in use.
    if (entry) {
      entry->Set(TLDCacheEntry{aHostname, ""_ns, NS_ERROR_HOST_IS_IP_ADDRESS});
    }

    return NS_ERROR_HOST_IS_IP_ADDRESS;
  }

  // Walk up the domain tree, most specific to least specific,
  // looking for matches at each level.  Note that a given level may
  // have multiple attributes (e.g. IsWild() and IsNormal()).
  const char* prevDomain = nullptr;
  const char* currDomain = aHostname.get();
  const char* nextDot = strchr(currDomain, '.');
  const char* end = currDomain + aHostname.Length();
  // Default value of *eTLD is currDomain as set in the while loop below
  const char* eTLD = nullptr;
  bool hasKnownPublicSuffix = false;
  while (true) {
    // sanity check the string we're about to look up: it should not begin
    // with a '.'; this would mean the hostname began with a '.' or had an
    // embedded '..' sequence.
    if (*currDomain == '.') {
      // Update the MRU table if in use.
      if (entry) {
        entry->Set(TLDCacheEntry{aHostname, ""_ns, NS_ERROR_INVALID_ARG});
      }

      return NS_ERROR_INVALID_ARG;
    }

    int result;
    {
      AutoReadLock lock(mGraphLock);
      // Perform the lookup.
      result = mGraph->Lookup(Substring(currDomain, end));
    }
    if (result != Dafsa::kKeyNotFound) {
      hasKnownPublicSuffix = true;
      if (result == kWildcardRule && prevDomain) {
        // wildcard rules imply an eTLD one level inferior to the match.
        eTLD = prevDomain;
        break;
      }
      if (result != kExceptionRule || !nextDot) {
        // specific match, or we've hit the top domain level
        eTLD = currDomain;
        break;
      }
      if (result == kExceptionRule) {
        // exception rules imply an eTLD one level superior to the match.
        eTLD = nextDot + 1;
        break;
      }
    }

    if (!nextDot) {
      // we've hit the top domain level; use it by default.
      eTLD = currDomain;
      break;
    }

    prevDomain = currDomain;
    currDomain = nextDot + 1;
    nextDot = strchr(currDomain, '.');
  }

  if (aOnlyKnownPublicSuffix && !hasKnownPublicSuffix) {
    aBaseDomain.Truncate();
    return NS_OK;
  }

  const char *begin, *iter;
  if (aAdditionalParts < 0) {
    NS_ASSERTION(aAdditionalParts == -1,
                 "aAdditionalParts can't be negative and different from -1");

    for (iter = aHostname.get(); iter != eTLD && *iter != '.'; iter++) {
      ;
    }

    if (iter != eTLD) {
      iter++;
    }
    if (iter != eTLD) {
      aAdditionalParts = 0;
    }
  } else {
    // count off the number of requested domains.
    begin = aHostname.get();
    iter = eTLD;

    while (true) {
      if (iter == begin) {
        break;
      }

      if (*(--iter) == '.' && aAdditionalParts-- == 0) {
        ++iter;
        ++aAdditionalParts;
        break;
      }
    }
  }

  if (aAdditionalParts != 0) {
    // Update the MRU table if in use.
    if (entry) {
      entry->Set(
          TLDCacheEntry{aHostname, ""_ns, NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS});
    }

    return NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS;
  }

  aBaseDomain = Substring(iter, end);

  // Update the MRU table if in use.
  if (entry) {
    entry->Set(TLDCacheEntry{aHostname, nsCString(aBaseDomain), NS_OK});
  }

  // add on the trailing dot, if applicable
  if (trailingDot) {
    aBaseDomain.Append('.');
  }

  return NS_OK;
}

// Normalizes the given hostname, component by component.  ASCII/ACE
// components are lower-cased, and UTF-8 components are normalized per
// RFC 3454 and converted to ACE.
nsresult nsEffectiveTLDService::NormalizeHostname(nsCString& aHostname) {
  if (!IsAscii(aHostname)) {
    nsresult rv = mIDNService->ConvertUTF8toACE(aHostname, aHostname);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  ToLowerCase(aHostname);
  return NS_OK;
}

NS_IMETHODIMP
nsEffectiveTLDService::HasRootDomain(const nsACString& aInput,
                                     const nsACString& aHost, bool* aResult) {
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_FAILURE;
  }

  *aResult = false;

  // If the strings are the same, we obviously have a match.
  if (aInput == aHost) {
    *aResult = true;
    return NS_OK;
  }

  // If aHost is not found, we know we do not have it as a root domain.
  int32_t index = nsAutoCString(aInput).Find(aHost.BeginReading());
  if (index == kNotFound) {
    return NS_OK;
  }

  // Otherwise, we have aHost as our root domain iff the index of aHost is
  // aHost.length subtracted from our length and (since we do not have an
  // exact match) the character before the index is a dot or slash.
  *aResult = index > 0 && (uint32_t)index == aInput.Length() - aHost.Length() &&
             (aInput[index - 1] == '.' || aInput[index - 1] == '/');
  return NS_OK;
}
