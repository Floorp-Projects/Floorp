/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheObserver.h"

#include "CacheStorageService.h"
#include "CacheFileIOManager.h"
#include "LoadContextInfo.h"
#include "nsICacheStorage.h"
#include "nsIObserverService.h"
#include "mozIApplicationClearPrivateDataParams.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "nsServiceManagerUtils.h"
#include <time.h>

namespace mozilla {
namespace net {

CacheObserver* CacheObserver::sSelf = nullptr;

static uint32_t const kDefaultMemoryLimit = 50 * 1024; // 50 MB
uint32_t CacheObserver::sMemoryLimit = kDefaultMemoryLimit;

static uint32_t const kDefaultUseNewCache = 0; // Don't use the new cache by default
uint32_t CacheObserver::sUseNewCache = kDefaultUseNewCache;
static int32_t const kAutoDeleteCacheVersion = -1; // Auto-delete off by default
static int32_t sAutoDeleteCacheVersion = kAutoDeleteCacheVersion;

static int32_t const kDefaultHalfLifeExperiment = -1; // Disabled
int32_t CacheObserver::sHalfLifeExperiment = kDefaultHalfLifeExperiment;

static uint32_t const kDefaultHalfLifeHours = 6; // 6 hours
uint32_t CacheObserver::sHalfLifeHours = kDefaultHalfLifeHours;

static bool const kDefaultUseDiskCache = true;
bool CacheObserver::sUseDiskCache = kDefaultUseDiskCache;

static bool const kDefaultUseMemoryCache = true;
bool CacheObserver::sUseMemoryCache = kDefaultUseMemoryCache;

static uint32_t const kDefaultDiskCacheCapacity = 250 * 1024; // 250 MB
uint32_t CacheObserver::sDiskCacheCapacity = kDefaultDiskCacheCapacity;

static uint32_t const kDefaultMaxMemoryEntrySize = 4 * 1024; // 4 MB
uint32_t CacheObserver::sMaxMemoryEntrySize = kDefaultMaxMemoryEntrySize;

static uint32_t const kDefaultMaxDiskEntrySize = 50 * 1024; // 50 MB
uint32_t CacheObserver::sMaxDiskEntrySize = kDefaultMaxDiskEntrySize;

static uint32_t const kDefaultCompressionLevel = 1;
uint32_t CacheObserver::sCompressionLevel = kDefaultCompressionLevel;

NS_IMPL_ISUPPORTS2(CacheObserver,
                   nsIObserver,
                   nsISupportsWeakReference)

nsresult
CacheObserver::Init()
{
  if (sSelf) {
    return NS_OK;
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return NS_ERROR_UNEXPECTED;
  }

  sSelf = new CacheObserver();
  NS_ADDREF(sSelf);

  obs->AddObserver(sSelf, "prefservice:after-app-defaults", true);
  obs->AddObserver(sSelf, "profile-do-change", true);
  obs->AddObserver(sSelf, "sessionstore-windows-restored", true);
  obs->AddObserver(sSelf, "profile-before-change", true);
  obs->AddObserver(sSelf, "xpcom-shutdown", true);
  obs->AddObserver(sSelf, "last-pb-context-exited", true);
  obs->AddObserver(sSelf, "webapps-clear-data", true);
  obs->AddObserver(sSelf, "memory-pressure", true);

  return NS_OK;
}

nsresult
CacheObserver::Shutdown()
{
  if (!sSelf) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_RELEASE(sSelf);
  return NS_OK;
}

void
CacheObserver::AttachToPreferences()
{
  sAutoDeleteCacheVersion = mozilla::Preferences::GetInt(
    "browser.cache.auto_delete_cache_version", kAutoDeleteCacheVersion);

  mozilla::Preferences::AddUintVarCache(
    &sUseNewCache, "browser.cache.use_new_backend", kDefaultUseNewCache);

  mozilla::Preferences::AddBoolVarCache(
    &sUseDiskCache, "browser.cache.disk.enable", kDefaultUseDiskCache);
  mozilla::Preferences::AddBoolVarCache(
    &sUseMemoryCache, "browser.cache.memory.enable", kDefaultUseMemoryCache);

  mozilla::Preferences::AddUintVarCache(
    &sMemoryLimit, "browser.cache.memory_limit", kDefaultMemoryLimit);

  mozilla::Preferences::AddUintVarCache(
    &sDiskCacheCapacity, "browser.cache.disk.capacity", kDefaultDiskCacheCapacity);

  mozilla::Preferences::AddUintVarCache(
    &sMaxMemoryEntrySize, "browser.cache.memory.max_entry_size", kDefaultMaxMemoryEntrySize);
  mozilla::Preferences::AddUintVarCache(
    &sMaxDiskEntrySize, "browser.cache.disk.max_entry_size", kDefaultMaxDiskEntrySize);

  // http://mxr.mozilla.org/mozilla-central/source/netwerk/cache/nsCacheEntryDescriptor.cpp#367
  mozilla::Preferences::AddUintVarCache(
    &sCompressionLevel, "browser.cache.compression_level", kDefaultCompressionLevel);

  sHalfLifeExperiment = mozilla::Preferences::GetInt(
    "browser.cache.frecency_experiment", kDefaultHalfLifeExperiment);

  if (sHalfLifeExperiment == 0) {
    // The experiment has not yet been initialized, do it now
    // Store the experiemnt value, since we need it not to change between
    // browser sessions.
    srand(time(NULL));
    sHalfLifeExperiment = (rand() % 4) + 1;
    mozilla::Preferences::SetInt(
      "browser.cache.frecency_experiment", sHalfLifeExperiment);
  }

  switch (sHalfLifeExperiment) {
  case 1: // The experiment is engaged
    sHalfLifeHours = 6;
    break;
  case 2:
    sHalfLifeHours = 24;
    break;
  case 3:
    sHalfLifeHours = 7 * 24;
    break;
  case 4:
    sHalfLifeHours = 50 * 24;
    break;

  case -1:
  default: // The experiment is off or broken
    sHalfLifeExperiment = -1;
    sHalfLifeHours = std::max(1U, std::min(1440U, mozilla::Preferences::GetUint(
      "browser.cache.frecency_half_life_hours", kDefaultHalfLifeHours)));
    break;
  }
}

void CacheObserver::SchduleAutoDelete()
{
  // Auto-delete not set
  if (sAutoDeleteCacheVersion == -1)
    return;

  // Don't autodelete the same version of the cache user has setup
  // to use.
  int32_t activeVersion = UseNewCache() ? 1 : 0;
  if (sAutoDeleteCacheVersion == activeVersion)
    return;

  CacheStorageService::WipeCacheDirectory(sAutoDeleteCacheVersion);
}

// static
bool const CacheObserver::UseNewCache()
{
  switch (sUseNewCache) {
    case 0: // use the old cache backend
      return false;

    case 1: // use the new cache backend
      return true;
  }

  return true;
}

namespace { // anon

class CacheStorageEvictHelper
{
public:
  nsresult Run(mozIApplicationClearPrivateDataParams* aParams);

private:
  uint32_t mAppId;
  nsresult ClearStorage(bool const aPrivate,
                        bool const aInBrowser,
                        bool const aAnonymous);
};

nsresult
CacheStorageEvictHelper::Run(mozIApplicationClearPrivateDataParams* aParams)
{
  nsresult rv;

  rv = aParams->GetAppId(&mAppId);
  NS_ENSURE_SUCCESS(rv, rv);

  bool aBrowserOnly;
  rv = aParams->GetBrowserOnly(&aBrowserOnly);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(mAppId != nsILoadContextInfo::UNKNOWN_APP_ID);

  // Clear all [private X anonymous] combinations
  rv = ClearStorage(false, aBrowserOnly, false);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = ClearStorage(false, aBrowserOnly, true);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = ClearStorage(true, aBrowserOnly, false);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = ClearStorage(true, aBrowserOnly, true);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CacheStorageEvictHelper::ClearStorage(bool const aPrivate,
                                      bool const aInBrowser,
                                      bool const aAnonymous)
{
  nsresult rv;

  nsRefPtr<LoadContextInfo> info = GetLoadContextInfo(
    aPrivate, mAppId, aInBrowser, aAnonymous);

  nsCOMPtr<nsICacheStorage> storage;
  nsRefPtr<CacheStorageService> service = CacheStorageService::Self();
  NS_ENSURE_TRUE(service, NS_ERROR_FAILURE);

  // Clear disk storage
  rv = service->DiskCacheStorage(info, false, getter_AddRefs(storage));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = storage->AsyncEvictStorage(nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  // Clear memory storage
  rv = service->MemoryCacheStorage(info, getter_AddRefs(storage));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = storage->AsyncEvictStorage(nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aInBrowser) {
    rv = ClearStorage(aPrivate, true, aAnonymous);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

} // anon

// static
bool const CacheObserver::EntryIsTooBig(int64_t aSize, bool aUsingDisk)
{
  // If custom limit is set, check it.
  int64_t preferredLimit = aUsingDisk
    ? static_cast<int64_t>(sMaxDiskEntrySize) << 10
    : static_cast<int64_t>(sMaxMemoryEntrySize) << 10;

  if (preferredLimit != -1 && aSize > preferredLimit)
    return true;

  // Otherwise (or when in the custom limit), check limit
  // based on the global limit.
  int64_t derivedLimit = aUsingDisk
    ? (static_cast<int64_t>(sDiskCacheCapacity) << 7) // << 7 == * 1024 / 8
    : (static_cast<int64_t>(sMemoryLimit) << 7);

  if (aSize > derivedLimit)
    return true;

  return false;
}

NS_IMETHODIMP
CacheObserver::Observe(nsISupports* aSubject,
                       const char* aTopic,
                       const char16_t* aData)
{
  if (!strcmp(aTopic, "prefservice:after-app-defaults")) {
    CacheFileIOManager::Init();
    return NS_OK;
  }

  if (!strcmp(aTopic, "profile-do-change")) {
    AttachToPreferences();
    CacheFileIOManager::Init();
    CacheFileIOManager::OnProfile();
    return NS_OK;
  }

  if (!strcmp(aTopic, "sessionstore-windows-restored")) {
    SchduleAutoDelete();
    return NS_OK;
  }

  if (!strcmp(aTopic, "profile-before-change")) {
    nsRefPtr<CacheStorageService> service = CacheStorageService::Self();
    if (service)
      service->Shutdown();

    return NS_OK;
  }

  if (!strcmp(aTopic, "xpcom-shutdown")) {
    nsRefPtr<CacheStorageService> service = CacheStorageService::Self();
    if (service)
      service->Shutdown();

    CacheFileIOManager::Shutdown();
    return NS_OK;
  }

  if (!strcmp(aTopic, "last-pb-context-exited")) {
    nsRefPtr<CacheStorageService> service = CacheStorageService::Self();
    if (service)
      service->DropPrivateBrowsingEntries();

    return NS_OK;
  }

  if (!strcmp(aTopic, "webapps-clear-data")) {
    nsCOMPtr<mozIApplicationClearPrivateDataParams> params =
            do_QueryInterface(aSubject);
    if (!params) {
      NS_ERROR("'webapps-clear-data' notification's subject should be a mozIApplicationClearPrivateDataParams");
      return NS_ERROR_UNEXPECTED;
    }

    CacheStorageEvictHelper helper;
    nsresult rv = helper.Run(params);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  if (!strcmp(aTopic, "memory-pressure")) {
    nsRefPtr<CacheStorageService> service = CacheStorageService::Self();
    if (service)
      service->PurgeFromMemory(nsICacheStorageService::PURGE_EVERYTHING);

    return NS_OK;
  }

  MOZ_ASSERT(false, "Missing observer handler");
  return NS_OK;
}

} // net
} // mozilla
