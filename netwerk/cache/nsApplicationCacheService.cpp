/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDiskCache.h"
#include "nsDiskCacheDeviceSQL.h"
#include "nsCacheService.h"
#include "nsApplicationCacheService.h"
#include "nsCRT.h"
#include "nsNetUtil.h"
#include "nsIObserverService.h"
#include "nsILoadContextInfo.h"

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

nsApplicationCacheService::~nsApplicationCacheService()
{
}

NS_IMETHODIMP
nsApplicationCacheService::BuildGroupID(nsIURI *aManifestURL,
                                        nsILoadContextInfo *aLoadContextInfo,
                                        nsACString &_result)
{
    nsresult rv;

    uint32_t appId = NECKO_NO_APP_ID;
    bool isInBrowserElement = false;

    if (aLoadContextInfo) {
        appId = aLoadContextInfo->AppId();
        isInBrowserElement = aLoadContextInfo->IsInBrowserElement();
    }

    rv = nsOfflineCacheDevice::BuildApplicationCacheGroupID(
        aManifestURL, appId, isInBrowserElement, _result);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

NS_IMETHODIMP
nsApplicationCacheService::BuildGroupIDForApp(nsIURI *aManifestURL,
                                              uint32_t aAppId,
                                              bool aIsInBrowser,
                                              nsACString &_result)
{
    nsresult rv = nsOfflineCacheDevice::BuildApplicationCacheGroupID(
        aManifestURL, aAppId, aIsInBrowser, _result);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

NS_IMETHODIMP
nsApplicationCacheService::CreateApplicationCache(const nsACString &group,
                                                  nsIApplicationCache **out)
{
    if (!mCacheService)
        return NS_ERROR_UNEXPECTED;

    nsRefPtr<nsOfflineCacheDevice> device;
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

    nsRefPtr<nsOfflineCacheDevice> device;
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

    nsRefPtr<nsOfflineCacheDevice> device;
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

    nsRefPtr<nsOfflineCacheDevice> device;
    nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
    NS_ENSURE_SUCCESS(rv, rv);
    return device->GetActiveCache(group, out);
}

NS_IMETHODIMP
nsApplicationCacheService::DeactivateGroup(const nsACString &group)
{
    if (!mCacheService)
        return NS_ERROR_UNEXPECTED;

    nsRefPtr<nsOfflineCacheDevice> device;
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

    nsRefPtr<nsOfflineCacheDevice> device;
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

    nsRefPtr<nsOfflineCacheDevice> device;
    nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
    NS_ENSURE_SUCCESS(rv, rv);
    return device->CacheOpportunistically(cache, key);
}

NS_IMETHODIMP
nsApplicationCacheService::DiscardByAppId(int32_t appID, bool isInBrowser)
{
    if (!mCacheService)
        return NS_ERROR_UNEXPECTED;

    nsRefPtr<nsOfflineCacheDevice> device;
    nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
    NS_ENSURE_SUCCESS(rv, rv);
    return device->DiscardByAppId(appID, isInBrowser);
}

NS_IMETHODIMP
nsApplicationCacheService::GetGroups(uint32_t *count,
                                     char ***keys)
{
    if (!mCacheService)
        return NS_ERROR_UNEXPECTED;

    nsRefPtr<nsOfflineCacheDevice> device;
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

    nsRefPtr<nsOfflineCacheDevice> device;
    nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
    NS_ENSURE_SUCCESS(rv, rv);
    return device->GetGroupsTimeOrdered(count, keys);
}

//-----------------------------------------------------------------------------
// AppCacheClearDataObserver: handles clearing appcache data for app uninstall
// and clearing user data events.
//-----------------------------------------------------------------------------

namespace {

class AppCacheClearDataObserver MOZ_FINAL : public nsIObserver {
public:
    NS_DECL_ISUPPORTS

    // nsIObserver implementation.
    NS_IMETHODIMP
    Observe(nsISupports *aSubject, const char *aTopic, const char16_t *aData)
    {
        MOZ_ASSERT(!nsCRT::strcmp(aTopic, TOPIC_WEB_APP_CLEAR_DATA));

        uint32_t appId = NECKO_UNKNOWN_APP_ID;
        bool browserOnly = false;
        nsresult rv = NS_GetAppInfoFromClearDataNotification(aSubject, &appId,
                                                             &browserOnly);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIApplicationCacheService> cacheService =
            do_GetService(NS_APPLICATIONCACHESERVICE_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        return cacheService->DiscardByAppId(appId, browserOnly);
    }

private:
    ~AppCacheClearDataObserver() {}
};

NS_IMPL_ISUPPORTS(AppCacheClearDataObserver, nsIObserver)

} // anonymous namespace

// Instantiates and registers AppCacheClearDataObserver for notifications
void
nsApplicationCacheService::AppClearDataObserverInit()
{
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1");
    if (observerService) {
        nsRefPtr<AppCacheClearDataObserver> obs
            = new AppCacheClearDataObserver();
        observerService->AddObserver(obs, TOPIC_WEB_APP_CLEAR_DATA,
                                     /*holdsWeak=*/ false);
    }
}

