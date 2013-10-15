/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "CacheStorage.h"
#include "CacheStorageService.h"
#include "CacheEntry.h"

#include "OldWrappers.h"

#include "nsICacheEntryDoomCallback.h"

#include "nsIApplicationCache.h"
#include "nsIApplicationCacheService.h"
#include "nsIURI.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS1(CacheStorage, nsICacheStorage)

CacheStorage::CacheStorage(nsILoadContextInfo* aInfo,
                           bool aAllowDisk,
                           bool aLookupAppCache)
: mLoadContextInfo(GetLoadContextInfo(aInfo))
, mWriteToDisk(aAllowDisk)
, mLookupAppCache(aLookupAppCache)
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

  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG(aCallback);

  nsresult rv;

  bool truncate = aFlags & nsICacheStorage::OPEN_TRUNCATE;

  nsCOMPtr<nsIURI> noRefURI;
  rv = aURI->CloneIgnoringRef(getter_AddRefs(noRefURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIApplicationCache> appCache;
  if (LookupAppCache()) {
    MOZ_ASSERT(!truncate);

    rv = ChooseApplicationCache(noRefURI, getter_AddRefs(appCache));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (appCache) {
    nsAutoCString cacheKey;
    rv = noRefURI->GetAsciiSpec(cacheKey);
    NS_ENSURE_SUCCESS(rv, rv);

    nsRefPtr<_OldCacheLoad> appCacheLoad =
      new _OldCacheLoad(cacheKey, aCallback, appCache,
                        LoadInfo(), WriteToDisk(), aFlags);
    rv = appCacheLoad->Start();
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("CacheStorage::AsyncOpenURI loading from appcache"));
    return NS_OK;
  }

  nsRefPtr<CacheEntry> entry;
  rv = CacheStorageService::Self()->AddStorageEntry(
    this, noRefURI, aIdExtension,
    true, // create always
    truncate, // replace any existing one?
    getter_AddRefs(entry));
  NS_ENSURE_SUCCESS(rv, rv);

  // May invoke the callback synchronously
  entry->AsyncOpen(aCallback, aFlags);

  return NS_OK;
}

NS_IMETHODIMP CacheStorage::AsyncDoomURI(nsIURI *aURI, const nsACString & aIdExtension,
                                         nsICacheEntryDoomCallback* aCallback)
{
  if (!CacheStorageService::Self())
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv = CacheStorageService::Self()->DoomStorageEntry(
    this, aURI, aIdExtension, aCallback);
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

} // net
} // mozilla
