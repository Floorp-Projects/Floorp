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
#include "nsIPrincipal.h"
#include "mozilla/LoadContextInfo.h"

using namespace mozilla;

static NS_DEFINE_CID(kCacheServiceCID, NS_CACHESERVICE_CID);

//-----------------------------------------------------------------------------
// nsApplicationCacheService
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsApplicationCacheService, nsIApplicationCacheService)

nsApplicationCacheService::nsApplicationCacheService()
{
    nsCOMPtr<nsICacheService> serv = do_GetService(kCacheServiceCID);
    mCacheService = nsCacheService::GlobalInstance();
}

NS_IMETHODIMP
nsApplicationCacheService::BuildGroupIDForInfo(
    nsIURI *aManifestURL,
    nsILoadContextInfo *aLoadContextInfo,
    nsACString &_result)
{
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
    nsIURI *aManifestURL,
    nsACString const &aOriginSuffix,
    nsACString &_result)
{
    nsresult rv;

    rv = nsOfflineCacheDevice::BuildApplicationCacheGroupID(
        aManifestURL, aOriginSuffix, _result);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

NS_IMETHODIMP
nsApplicationCacheService::CreateApplicationCache(const nsACString &group,
                                                  nsIApplicationCache **out)
{
    if (!mCacheService)
        return NS_ERROR_UNEXPECTED;

    RefPtr<nsOfflineCacheDevice> device;
    nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
    NS_ENSURE_SUCCESS(rv, rv);
    return device->CreateApplicationCache(group, out);
}

NS_IMETHODIMP
nsApplicationCacheService::CreateCustomApplicationCache(const nsACString & group,
                                                        nsIFile *profileDir,
                                                        int32_t quota,
                                                        nsIApplicationCache **out)
{
    if (!mCacheService)
        return NS_ERROR_UNEXPECTED;

    RefPtr<nsOfflineCacheDevice> device;
    nsresult rv = mCacheService->GetCustomOfflineDevice(profileDir,
                                                        quota,
                                                        getter_AddRefs(device));
    NS_ENSURE_SUCCESS(rv, rv);
    return device->CreateApplicationCache(group, out);
}

NS_IMETHODIMP
nsApplicationCacheService::GetApplicationCache(const nsACString &clientID,
                                               nsIApplicationCache **out)
{
    if (!mCacheService)
        return NS_ERROR_UNEXPECTED;

    RefPtr<nsOfflineCacheDevice> device;
    nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
    NS_ENSURE_SUCCESS(rv, rv);
    return device->GetApplicationCache(clientID, out);
}

NS_IMETHODIMP
nsApplicationCacheService::GetActiveCache(const nsACString &group,
                                          nsIApplicationCache **out)
{
    if (!mCacheService)
        return NS_ERROR_UNEXPECTED;

    RefPtr<nsOfflineCacheDevice> device;
    nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
    NS_ENSURE_SUCCESS(rv, rv);
    return device->GetActiveCache(group, out);
}

NS_IMETHODIMP
nsApplicationCacheService::DeactivateGroup(const nsACString &group)
{
    if (!mCacheService)
        return NS_ERROR_UNEXPECTED;

    RefPtr<nsOfflineCacheDevice> device;
    nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
    NS_ENSURE_SUCCESS(rv, rv);
    return device->DeactivateGroup(group);
}

NS_IMETHODIMP
nsApplicationCacheService::ChooseApplicationCache(const nsACString &key,
                                                  nsILoadContextInfo *aLoadContextInfo,
                                                  nsIApplicationCache **out)
{
    if (!mCacheService)
        return NS_ERROR_UNEXPECTED;

    RefPtr<nsOfflineCacheDevice> device;
    nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
    NS_ENSURE_SUCCESS(rv, rv);

    return device->ChooseApplicationCache(key, aLoadContextInfo, out);
}

NS_IMETHODIMP
nsApplicationCacheService::CacheOpportunistically(nsIApplicationCache* cache,
                                                  const nsACString &key)
{
    if (!mCacheService)
        return NS_ERROR_UNEXPECTED;

    RefPtr<nsOfflineCacheDevice> device;
    nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
    NS_ENSURE_SUCCESS(rv, rv);
    return device->CacheOpportunistically(cache, key);
}

NS_IMETHODIMP
nsApplicationCacheService::Evict(nsILoadContextInfo *aInfo)
{
    if (!mCacheService)
        return NS_ERROR_UNEXPECTED;

    RefPtr<nsOfflineCacheDevice> device;
    nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
    NS_ENSURE_SUCCESS(rv, rv);
    return device->Evict(aInfo);
}

NS_IMETHODIMP
nsApplicationCacheService::EvictMatchingOriginAttributes(nsAString const &aPattern)
{
    if (!mCacheService)
        return NS_ERROR_UNEXPECTED;

    RefPtr<nsOfflineCacheDevice> device;
    nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
    NS_ENSURE_SUCCESS(rv, rv);

    mozilla::OriginAttributesPattern pattern;
    if (!pattern.Init(aPattern)) {
        NS_ERROR("Could not parse OriginAttributesPattern JSON in clear-origin-attributes-data notification");
        return NS_ERROR_FAILURE;
    }

    return device->Evict(pattern);
}

NS_IMETHODIMP
nsApplicationCacheService::GetGroups(uint32_t *count,
                                     char ***keys)
{
    if (!mCacheService)
        return NS_ERROR_UNEXPECTED;

    RefPtr<nsOfflineCacheDevice> device;
    nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
    NS_ENSURE_SUCCESS(rv, rv);
    return device->GetGroups(count, keys);
}

NS_IMETHODIMP
nsApplicationCacheService::GetGroupsTimeOrdered(uint32_t *count,
                                                char ***keys)
{
    if (!mCacheService)
        return NS_ERROR_UNEXPECTED;

    RefPtr<nsOfflineCacheDevice> device;
    nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
    NS_ENSURE_SUCCESS(rv, rv);
    return device->GetGroupsTimeOrdered(count, keys);
}

//-----------------------------------------------------------------------------
// AppCacheClearDataObserver: handles clearing appcache data for app uninstall
// and clearing user data events.
//-----------------------------------------------------------------------------

namespace {

class AppCacheClearDataObserver final : public nsIObserver {
public:
    NS_DECL_ISUPPORTS

    // nsIObserver implementation.
    NS_IMETHOD
    Observe(nsISupports *aSubject, const char *aTopic, const char16_t *aData) override
    {
        MOZ_ASSERT(!nsCRT::strcmp(aTopic, "clear-origin-attributes-data"));

        nsresult rv;

        nsCOMPtr<nsIApplicationCacheService> cacheService =
            do_GetService(NS_APPLICATIONCACHESERVICE_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        return cacheService->EvictMatchingOriginAttributes(nsDependentString(aData));
    }

private:
    ~AppCacheClearDataObserver() = default;
};

NS_IMPL_ISUPPORTS(AppCacheClearDataObserver, nsIObserver)

} // namespace

// Instantiates and registers AppCacheClearDataObserver for notifications
void
nsApplicationCacheService::AppClearDataObserverInit()
{
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (observerService) {
    RefPtr<AppCacheClearDataObserver> obs = new AppCacheClearDataObserver();
    observerService->AddObserver(obs, "clear-origin-attributes-data", /*ownsWeak=*/ false);
  }
}
