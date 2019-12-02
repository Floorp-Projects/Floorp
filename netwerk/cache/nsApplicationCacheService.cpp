/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDiskCache.h"
#include "nsDiskCacheDeviceSQL.h"
#include "nsCacheService.h"
#include "nsApplicationCacheService.h"
#include "nsCRT.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIObserverService.h"
#include "mozilla/LoadContextInfo.h"

using namespace mozilla;

static NS_DEFINE_CID(kCacheServiceCID, NS_CACHESERVICE_CID);

//-----------------------------------------------------------------------------
// nsApplicationCacheService
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsApplicationCacheService, nsIApplicationCacheService)

nsApplicationCacheService::nsApplicationCacheService() {
  nsCOMPtr<nsICacheService> serv = do_GetService(kCacheServiceCID);
  mCacheService = nsCacheService::GlobalInstance();
}

NS_IMETHODIMP
nsApplicationCacheService::BuildGroupIDForInfo(
    nsIURI* aManifestURL, nsILoadContextInfo* aLoadContextInfo,
    nsACString& _result) {
  nsresult rv;

  nsAutoCString originSuffix;
  if (aLoadContextInfo) {
    aLoadContextInfo->OriginAttributesPtr()->CreateSuffix(originSuffix);
  }

  rv = nsOfflineCacheDevice::BuildApplicationCacheGroupID(
      aManifestURL, originSuffix, _result);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsApplicationCacheService::BuildGroupIDForSuffix(
    nsIURI* aManifestURL, nsACString const& aOriginSuffix,
    nsACString& _result) {
  nsresult rv;

  rv = nsOfflineCacheDevice::BuildApplicationCacheGroupID(
      aManifestURL, aOriginSuffix, _result);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsApplicationCacheService::CreateApplicationCache(const nsACString& group,
                                                  nsIApplicationCache** out) {
  if (!mCacheService) return NS_ERROR_UNEXPECTED;

  RefPtr<nsOfflineCacheDevice> device;
  nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
  NS_ENSURE_SUCCESS(rv, rv);
  return device->CreateApplicationCache(group, out);
}

NS_IMETHODIMP
nsApplicationCacheService::CreateCustomApplicationCache(
    const nsACString& group, nsIFile* profileDir, int32_t quota,
    nsIApplicationCache** out) {
  if (!mCacheService) return NS_ERROR_UNEXPECTED;

  RefPtr<nsOfflineCacheDevice> device;
  nsresult rv = mCacheService->GetCustomOfflineDevice(profileDir, quota,
                                                      getter_AddRefs(device));
  NS_ENSURE_SUCCESS(rv, rv);
  return device->CreateApplicationCache(group, out);
}

NS_IMETHODIMP
nsApplicationCacheService::GetApplicationCache(const nsACString& clientID,
                                               nsIApplicationCache** out) {
  if (!mCacheService) return NS_ERROR_UNEXPECTED;

  RefPtr<nsOfflineCacheDevice> device;
  nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
  NS_ENSURE_SUCCESS(rv, rv);
  return device->GetApplicationCache(clientID, out);
}

NS_IMETHODIMP
nsApplicationCacheService::GetActiveCache(const nsACString& group,
                                          nsIApplicationCache** out) {
  if (!mCacheService) return NS_ERROR_UNEXPECTED;

  RefPtr<nsOfflineCacheDevice> device;
  nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
  NS_ENSURE_SUCCESS(rv, rv);
  return device->GetActiveCache(group, out);
}

NS_IMETHODIMP
nsApplicationCacheService::DeactivateGroup(const nsACString& group) {
  if (!mCacheService) return NS_ERROR_UNEXPECTED;

  RefPtr<nsOfflineCacheDevice> device;
  nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
  NS_ENSURE_SUCCESS(rv, rv);
  return device->DeactivateGroup(group);
}

NS_IMETHODIMP
nsApplicationCacheService::ChooseApplicationCache(
    const nsACString& key, nsILoadContextInfo* aLoadContextInfo,
    nsIApplicationCache** out) {
  if (!mCacheService) return NS_ERROR_UNEXPECTED;

  RefPtr<nsOfflineCacheDevice> device;
  nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
  NS_ENSURE_SUCCESS(rv, rv);

  return device->ChooseApplicationCache(key, aLoadContextInfo, out);
}

NS_IMETHODIMP
nsApplicationCacheService::CacheOpportunistically(nsIApplicationCache* cache,
                                                  const nsACString& key) {
  if (!mCacheService) return NS_ERROR_UNEXPECTED;

  RefPtr<nsOfflineCacheDevice> device;
  nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
  NS_ENSURE_SUCCESS(rv, rv);
  return device->CacheOpportunistically(cache, key);
}

NS_IMETHODIMP
nsApplicationCacheService::Evict(nsILoadContextInfo* aInfo) {
  if (!mCacheService) return NS_ERROR_UNEXPECTED;

  RefPtr<nsOfflineCacheDevice> device;
  nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
  NS_ENSURE_SUCCESS(rv, rv);
  return device->Evict(aInfo);
}

NS_IMETHODIMP
nsApplicationCacheService::EvictMatchingOriginAttributes(
    nsAString const& aPattern) {
  if (!mCacheService) return NS_ERROR_UNEXPECTED;

  RefPtr<nsOfflineCacheDevice> device;
  nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
  NS_ENSURE_SUCCESS(rv, rv);

  mozilla::OriginAttributesPattern pattern;
  if (!pattern.Init(aPattern)) {
    NS_ERROR(
        "Could not parse OriginAttributesPattern JSON in "
        "clear-origin-attributes-data notification");
    return NS_ERROR_FAILURE;
  }

  return device->Evict(pattern);
}

NS_IMETHODIMP
nsApplicationCacheService::GetGroups(nsTArray<nsCString>& keys) {
  if (!mCacheService) return NS_ERROR_UNEXPECTED;

  RefPtr<nsOfflineCacheDevice> device;
  nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
  NS_ENSURE_SUCCESS(rv, rv);
  return device->GetGroups(keys);
}

NS_IMETHODIMP
nsApplicationCacheService::GetGroupsTimeOrdered(nsTArray<nsCString>& keys) {
  if (!mCacheService) return NS_ERROR_UNEXPECTED;

  RefPtr<nsOfflineCacheDevice> device;
  nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
  NS_ENSURE_SUCCESS(rv, rv);
  return device->GetGroupsTimeOrdered(keys);
}
