/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "AppCacheStorage.h"
#include "CacheStorageService.h"

#include "OldWrappers.h"

#include "nsICacheEntryDoomCallback.h"

#include "nsCacheService.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheService.h"
#include "nsIURI.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

AppCacheStorage::AppCacheStorage(nsILoadContextInfo* aInfo,
                                 nsIApplicationCache* aAppCache)
    : CacheStorage(aInfo, true /* disk */, false /* lookup app cache */,
                   false /* skip size check */, false /* pin */),
      mAppCache(aAppCache) {}

AppCacheStorage::~AppCacheStorage() {
  ProxyReleaseMainThread("AppCacheStorage::mAppCache", mAppCache);
}

NS_IMETHODIMP AppCacheStorage::AsyncOpenURI(
    nsIURI* aURI, const nsACString& aIdExtension, uint32_t aFlags,
    nsICacheEntryOpenCallback* aCallback) {
  if (!CacheStorageService::Self()) return NS_ERROR_NOT_INITIALIZED;

  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG(aCallback);

  nsresult rv;

  nsCOMPtr<nsIApplicationCache> appCache = mAppCache;

  if (!appCache) {
    rv = ChooseApplicationCache(aURI, getter_AddRefs(appCache));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!appCache) {
    LOG(
        ("AppCacheStorage::AsyncOpenURI entry not found in any appcache, "
         "giving up"));
    aCallback->OnCacheEntryAvailable(nullptr, false, nullptr,
                                     NS_ERROR_CACHE_KEY_NOT_FOUND);
    return NS_OK;
  }

  nsCOMPtr<nsIURI> noRefURI;
  rv = NS_GetURIWithoutRef(aURI, getter_AddRefs(noRefURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString cacheKey;
  rv = noRefURI->GetAsciiSpec(cacheKey);
  NS_ENSURE_SUCCESS(rv, rv);

  // This is the only way how to recognize appcache data by the anonymous
  // flag.  There is no way to switch to e.g. a different session, because
  // there is just a single session for an appcache version (identified
  // by the client id).
  if (LoadInfo()->IsAnonymous()) {
    cacheKey = NS_LITERAL_CSTRING("anon&") + cacheKey;
  }

  nsAutoCString scheme;
  rv = noRefURI->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<_OldCacheLoad> appCacheLoad = new _OldCacheLoad(
      scheme, cacheKey, aCallback, appCache, LoadInfo(), WriteToDisk(), aFlags);
  rv = appCacheLoad->Start();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP AppCacheStorage::OpenTruncate(nsIURI* aURI,
                                            const nsACString& aIdExtension,
                                            nsICacheEntry** aCacheEntry) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP AppCacheStorage::Exists(nsIURI* aURI,
                                      const nsACString& aIdExtension,
                                      bool* aResult) {
  *aResult = false;
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP AppCacheStorage::AsyncDoomURI(
    nsIURI* aURI, const nsACString& aIdExtension,
    nsICacheEntryDoomCallback* aCallback) {
  if (!CacheStorageService::Self()) return NS_ERROR_NOT_INITIALIZED;

  if (!mAppCache) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<_OldStorage> old = new _OldStorage(LoadInfo(), WriteToDisk(),
                                            LookupAppCache(), true, mAppCache);
  return old->AsyncDoomURI(aURI, aIdExtension, aCallback);
}

NS_IMETHODIMP AppCacheStorage::AsyncEvictStorage(
    nsICacheEntryDoomCallback* aCallback) {
  if (!CacheStorageService::Self()) return NS_ERROR_NOT_INITIALIZED;

  nsresult rv;

  if (!mAppCache) {
    // Discard everything under this storage context
    nsCOMPtr<nsIApplicationCacheService> appCacheService =
        do_GetService(NS_APPLICATIONCACHESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = appCacheService->Evict(LoadInfo());
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // Discard the group
    RefPtr<_OldStorage> old = new _OldStorage(
        LoadInfo(), WriteToDisk(), LookupAppCache(), true, mAppCache);
    rv = old->AsyncEvictStorage(aCallback);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  if (aCallback) aCallback->OnCacheEntryDoomed(NS_OK);

  return NS_OK;
}

NS_IMETHODIMP AppCacheStorage::AsyncVisitStorage(
    nsICacheStorageVisitor* aVisitor, bool aVisitEntries) {
  if (!CacheStorageService::Self()) return NS_ERROR_NOT_INITIALIZED;

  LOG(("AppCacheStorage::AsyncVisitStorage [this=%p, cb=%p]", this, aVisitor));

  nsresult rv;

  nsCOMPtr<nsICacheService> serv =
      do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<_OldVisitCallbackWrapper> cb = new _OldVisitCallbackWrapper(
      "offline", aVisitor, aVisitEntries, LoadInfo());
  rv = nsCacheService::GlobalInstance()->VisitEntriesInternal(cb);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP AppCacheStorage::GetCacheIndexEntryAttrs(
    nsIURI* aURI, const nsACString& aIdExtension, bool* aHasAltData,
    uint32_t* aSizeInKB) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace net
}  // namespace mozilla
