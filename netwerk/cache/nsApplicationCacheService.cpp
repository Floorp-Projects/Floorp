/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDiskCache.h"
#include "nsDiskCacheDeviceSQL.h"
#include "nsCacheService.h"
#include "nsApplicationCacheService.h"

#include "nsNetUtil.h"

using namespace mozilla;

static NS_DEFINE_CID(kCacheServiceCID, NS_CACHESERVICE_CID);

NS_IMPL_ISUPPORTS1(nsApplicationCacheService, nsIApplicationCacheService)

nsApplicationCacheService::nsApplicationCacheService()
{
    nsCOMPtr<nsICacheService> serv = do_GetService(kCacheServiceCID);
    mCacheService = nsCacheService::GlobalInstance();
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
                                                        nsILocalFile *profileDir,
                                                        PRInt32 quota,
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
                                                  nsIApplicationCache **out)
{
    if (!mCacheService)
        return NS_ERROR_UNEXPECTED;

    nsRefPtr<nsOfflineCacheDevice> device;
    nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
    NS_ENSURE_SUCCESS(rv, rv);
    return device->ChooseApplicationCache(key, out);
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
nsApplicationCacheService::GetGroups(PRUint32 *count,
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
nsApplicationCacheService::GetGroupsTimeOrdered(PRUint32 *count,
                                                char ***keys)
{
    if (!mCacheService)
        return NS_ERROR_UNEXPECTED;

    nsRefPtr<nsOfflineCacheDevice> device;
    nsresult rv = mCacheService->GetOfflineDevice(getter_AddRefs(device));
    NS_ENSURE_SUCCESS(rv, rv);
    return device->GetGroupsTimeOrdered(count, keys);
}
