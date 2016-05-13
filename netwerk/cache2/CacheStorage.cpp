/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "CacheStorage.h"
#include "CacheStorageService.h"
#include "CacheEntry.h"
#include "CacheObserver.h"

#include "OldWrappers.h"

#include "nsICacheEntryDoomCallback.h"

#include "nsIApplicationCache.h"
#include "nsIApplicationCacheService.h"
#include "nsIURI.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(CacheStorage, nsICacheStorage)

CacheStorage::CacheStorage(nsILoadContextInfo* aInfo,
                           bool aAllowDisk,
                           bool aLookupAppCache,
                           bool aSkipSizeCheck,
                           bool aPinning)
: mLoadContextInfo(GetLoadContextInfo(aInfo))
, mWriteToDisk(aAllowDisk)
, mLookupAppCache(aLookupAppCache)
, mSkipSizeCheck(aSkipSizeCheck)
, mPinning(aPinning)
{
}

CacheStorage::~CacheStorage()
{
}

NS_IMETHODIMP CacheStorage::AsyncOpenURI(nsIURI *aURI,
                                         const nsACString & aIdExtension,
                                         uint32_t aFlags,
                                         nsICacheEntryOpenCallback *aCallback)
{
  if (!CacheStorageService::Self())
    return NS_ERROR_NOT_INITIALIZED;

  if (MOZ_UNLIKELY(!CacheObserver::UseDiskCache()) && mWriteToDisk &&
                   !(aFlags & OPEN_INTERCEPTED)) {
    aCallback->OnCacheEntryAvailable(nullptr, false, nullptr, NS_ERROR_NOT_AVAILABLE);
    return NS_OK;
  }

  if (MOZ_UNLIKELY(!CacheObserver::UseMemoryCache()) && !mWriteToDisk &&
                   !(aFlags & OPEN_INTERCEPTED)) {
    aCallback->OnCacheEntryAvailable(nullptr, false, nullptr, NS_ERROR_NOT_AVAILABLE);
    return NS_OK;
  }

  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG(aCallback);

  nsresult rv;

  bool truncate = aFlags & nsICacheStorage::OPEN_TRUNCATE;

  nsCOMPtr<nsIURI> noRefURI;
  rv = aURI->CloneIgnoringRef(getter_AddRefs(noRefURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString asciiSpec;
  rv = noRefURI->GetAsciiSpec(asciiSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIApplicationCache> appCache;
  if (LookupAppCache()) {
    rv = ChooseApplicationCache(noRefURI, getter_AddRefs(appCache));
    NS_ENSURE_SUCCESS(rv, rv);

    if (appCache) {
      // From a chosen appcache open only as readonly
      aFlags &= ~nsICacheStorage::OPEN_TRUNCATE;
    }
  }

  if (appCache) {
    nsAutoCString scheme;
    rv = noRefURI->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);

    RefPtr<_OldCacheLoad> appCacheLoad =
      new _OldCacheLoad(scheme, asciiSpec, aCallback, appCache,
                        LoadInfo(), WriteToDisk(), aFlags);
    rv = appCacheLoad->Start();
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("CacheStorage::AsyncOpenURI loading from appcache"));
    return NS_OK;
  }

  RefPtr<CacheEntryHandle> entry;
  rv = CacheStorageService::Self()->AddStorageEntry(
    this, asciiSpec, aIdExtension,
    truncate, // replace any existing one?
    getter_AddRefs(entry));
  NS_ENSURE_SUCCESS(rv, rv);

  // May invoke the callback synchronously
  entry->Entry()->AsyncOpen(aCallback, aFlags);

  return NS_OK;
}


NS_IMETHODIMP CacheStorage::OpenTruncate(nsIURI *aURI, const nsACString & aIdExtension,
                                         nsICacheEntry **aCacheEntry)
{
  if (!CacheStorageService::Self())
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv;

  nsCOMPtr<nsIURI> noRefURI;
  rv = aURI->CloneIgnoringRef(getter_AddRefs(noRefURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString asciiSpec;
  rv = noRefURI->GetAsciiSpec(asciiSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<CacheEntryHandle> handle;
  rv = CacheStorageService::Self()->AddStorageEntry(
    this, asciiSpec, aIdExtension,
    true, // replace any existing one
    getter_AddRefs(handle));
  NS_ENSURE_SUCCESS(rv, rv);

  // Just open w/o callback, similar to nsICacheEntry.recreate().
  handle->Entry()->AsyncOpen(nullptr, OPEN_TRUNCATE);

  // Return a write handler, consumer is supposed to fill in the entry.
  RefPtr<CacheEntryHandle> writeHandle = handle->Entry()->NewWriteHandle();
  writeHandle.forget(aCacheEntry);

  return NS_OK;
}

NS_IMETHODIMP CacheStorage::Exists(nsIURI *aURI, const nsACString & aIdExtension,
                                   bool *aResult)
{
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG(aResult);

  if (!CacheStorageService::Self())
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv;

  nsCOMPtr<nsIURI> noRefURI;
  rv = aURI->CloneIgnoringRef(getter_AddRefs(noRefURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString asciiSpec;
  rv = noRefURI->GetAsciiSpec(asciiSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  return CacheStorageService::Self()->CheckStorageEntry(
    this, asciiSpec, aIdExtension, aResult);
}

NS_IMETHODIMP CacheStorage::AsyncDoomURI(nsIURI *aURI, const nsACString & aIdExtension,
                                         nsICacheEntryDoomCallback* aCallback)
{
  if (!CacheStorageService::Self())
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv;

  nsCOMPtr<nsIURI> noRefURI;
  rv = aURI->CloneIgnoringRef(getter_AddRefs(noRefURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString asciiSpec;
  rv = noRefURI->GetAsciiSpec(asciiSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CacheStorageService::Self()->DoomStorageEntry(
    this, asciiSpec, aIdExtension, aCallback);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP CacheStorage::AsyncEvictStorage(nsICacheEntryDoomCallback* aCallback)
{
  if (!CacheStorageService::Self())
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv = CacheStorageService::Self()->DoomStorageEntries(
    this, aCallback);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP CacheStorage::AsyncVisitStorage(nsICacheStorageVisitor* aVisitor,
                                              bool aVisitEntries)
{
  LOG(("CacheStorage::AsyncVisitStorage [this=%p, cb=%p, disk=%d]", this, aVisitor, (bool)mWriteToDisk));
  if (!CacheStorageService::Self())
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv = CacheStorageService::Self()->WalkStorageEntries(
    this, aVisitEntries, aVisitor);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// Internal

nsresult CacheStorage::ChooseApplicationCache(nsIURI* aURI,
                                              nsIApplicationCache** aCache)
{
  nsresult rv;

  nsCOMPtr<nsIApplicationCacheService> appCacheService =
    do_GetService(NS_APPLICATIONCACHESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString cacheKey;
  rv = aURI->GetAsciiSpec(cacheKey);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = appCacheService->ChooseApplicationCache(cacheKey, LoadInfo(), aCache);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

} // namespace net
} // namespace mozilla
