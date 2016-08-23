/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheObserver.h"

#include "CacheStorageService.h"
#include "CacheFileIOManager.h"
#include "LoadContextInfo.h"
#include "nsICacheStorage.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "mozilla/TimeStamp.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/net/NeckoCommon.h"
#include "prsystem.h"
#include <time.h>
#include <math.h>

namespace mozilla {
namespace net {

CacheObserver* CacheObserver::sSelf = nullptr;

static uint32_t const kDefaultUseNewCache = 1; // Use the new cache by default
uint32_t CacheObserver::sUseNewCache = kDefaultUseNewCache;

static bool sUseNewCacheTemp = false; // Temp trigger to not lose early adopters

static int32_t const kAutoDeleteCacheVersion = -1; // Auto-delete off by default
static int32_t sAutoDeleteCacheVersion = kAutoDeleteCacheVersion;

static int32_t const kDefaultHalfLifeExperiment = -1; // Disabled
int32_t CacheObserver::sHalfLifeExperiment = kDefaultHalfLifeExperiment;

static float const kDefaultHalfLifeHours = 1.0F; // 1 hour
float CacheObserver::sHalfLifeHours = kDefaultHalfLifeHours;

static bool const kDefaultUseDiskCache = true;
bool CacheObserver::sUseDiskCache = kDefaultUseDiskCache;

static bool const kDefaultUseMemoryCache = true;
bool CacheObserver::sUseMemoryCache = kDefaultUseMemoryCache;

static uint32_t const kDefaultMetadataMemoryLimit = 250; // 0.25 MB
uint32_t CacheObserver::sMetadataMemoryLimit = kDefaultMetadataMemoryLimit;

static int32_t const kDefaultMemoryCacheCapacity = -1; // autodetect
int32_t CacheObserver::sMemoryCacheCapacity = kDefaultMemoryCacheCapacity;
// Cache of the calculated memory capacity based on the system memory size
int32_t CacheObserver::sAutoMemoryCacheCapacity = -1;

static uint32_t const kDefaultDiskCacheCapacity = 250 * 1024; // 250 MB
Atomic<uint32_t,Relaxed> CacheObserver::sDiskCacheCapacity
                                           (kDefaultDiskCacheCapacity);

static uint32_t const kDefaultDiskFreeSpaceSoftLimit = 5 * 1024; // 5MB
uint32_t CacheObserver::sDiskFreeSpaceSoftLimit = kDefaultDiskFreeSpaceSoftLimit;

static uint32_t const kDefaultDiskFreeSpaceHardLimit = 1024; // 1MB
uint32_t CacheObserver::sDiskFreeSpaceHardLimit = kDefaultDiskFreeSpaceHardLimit;

static bool const kDefaultSmartCacheSizeEnabled = false;
bool CacheObserver::sSmartCacheSizeEnabled = kDefaultSmartCacheSizeEnabled;

static uint32_t const kDefaultPreloadChunkCount = 4;
uint32_t CacheObserver::sPreloadChunkCount = kDefaultPreloadChunkCount;

static int32_t const kDefaultMaxMemoryEntrySize = 4 * 1024; // 4 MB
int32_t CacheObserver::sMaxMemoryEntrySize = kDefaultMaxMemoryEntrySize;

static int32_t const kDefaultMaxDiskEntrySize = 50 * 1024; // 50 MB
int32_t CacheObserver::sMaxDiskEntrySize = kDefaultMaxDiskEntrySize;

static uint32_t const kDefaultMaxDiskChunksMemoryUsage = 10 * 1024; // 10MB
uint32_t CacheObserver::sMaxDiskChunksMemoryUsage = kDefaultMaxDiskChunksMemoryUsage;

static uint32_t const kDefaultMaxDiskPriorityChunksMemoryUsage = 10 * 1024; // 10MB
uint32_t CacheObserver::sMaxDiskPriorityChunksMemoryUsage = kDefaultMaxDiskPriorityChunksMemoryUsage;

static uint32_t const kDefaultCompressionLevel = 1;
uint32_t CacheObserver::sCompressionLevel = kDefaultCompressionLevel;

static bool kDefaultSanitizeOnShutdown = false;
bool CacheObserver::sSanitizeOnShutdown = kDefaultSanitizeOnShutdown;

static bool kDefaultClearCacheOnShutdown = false;
bool CacheObserver::sClearCacheOnShutdown = kDefaultClearCacheOnShutdown;

static bool kDefaultCacheFSReported = false;
bool CacheObserver::sCacheFSReported = kDefaultCacheFSReported;

static bool kDefaultHashStatsReported = false;
bool CacheObserver::sHashStatsReported = kDefaultHashStatsReported;

static uint32_t const kDefaultMaxShutdownIOLag = 2; // seconds
Atomic<uint32_t, Relaxed> CacheObserver::sMaxShutdownIOLag(kDefaultMaxShutdownIOLag);

Atomic<PRIntervalTime> CacheObserver::sShutdownDemandedTime(PR_INTERVAL_NO_TIMEOUT);

NS_IMPL_ISUPPORTS(CacheObserver,
                  nsIObserver,
                  nsISupportsWeakReference)

// static
nsresult
CacheObserver::Init()
{
  if (IsNeckoChild()) {
    return NS_OK;
  }

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
  obs->AddObserver(sSelf, "browser-delayed-startup-finished", true);
  obs->AddObserver(sSelf, "profile-before-change", true);
  obs->AddObserver(sSelf, "xpcom-shutdown", true);
  obs->AddObserver(sSelf, "last-pb-context-exited", true);
  obs->AddObserver(sSelf, "clear-origin-data", true);
  obs->AddObserver(sSelf, "memory-pressure", true);

  return NS_OK;
}

// static
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
    &sUseNewCacheTemp, "browser.cache.use_new_backend_temp", false);

  mozilla::Preferences::AddBoolVarCache(
    &sUseDiskCache, "browser.cache.disk.enable", kDefaultUseDiskCache);
  mozilla::Preferences::AddBoolVarCache(
    &sUseMemoryCache, "browser.cache.memory.enable", kDefaultUseMemoryCache);

  mozilla::Preferences::AddUintVarCache(
    &sMetadataMemoryLimit, "browser.cache.disk.metadata_memory_limit", kDefaultMetadataMemoryLimit);

  mozilla::Preferences::AddAtomicUintVarCache(
    &sDiskCacheCapacity, "browser.cache.disk.capacity", kDefaultDiskCacheCapacity);
  mozilla::Preferences::AddBoolVarCache(
    &sSmartCacheSizeEnabled, "browser.cache.disk.smart_size.enabled", kDefaultSmartCacheSizeEnabled);
  mozilla::Preferences::AddIntVarCache(
    &sMemoryCacheCapacity, "browser.cache.memory.capacity", kDefaultMemoryCacheCapacity);

  mozilla::Preferences::AddUintVarCache(
    &sDiskFreeSpaceSoftLimit, "browser.cache.disk.free_space_soft_limit", kDefaultDiskFreeSpaceSoftLimit);
  mozilla::Preferences::AddUintVarCache(
    &sDiskFreeSpaceHardLimit, "browser.cache.disk.free_space_hard_limit", kDefaultDiskFreeSpaceHardLimit);

  mozilla::Preferences::AddUintVarCache(
    &sPreloadChunkCount, "browser.cache.disk.preload_chunk_count", kDefaultPreloadChunkCount);

  mozilla::Preferences::AddIntVarCache(
    &sMaxDiskEntrySize, "browser.cache.disk.max_entry_size", kDefaultMaxDiskEntrySize);
  mozilla::Preferences::AddIntVarCache(
    &sMaxMemoryEntrySize, "browser.cache.memory.max_entry_size", kDefaultMaxMemoryEntrySize);

  mozilla::Preferences::AddUintVarCache(
    &sMaxDiskChunksMemoryUsage, "browser.cache.disk.max_chunks_memory_usage", kDefaultMaxDiskChunksMemoryUsage);
  mozilla::Preferences::AddUintVarCache(
    &sMaxDiskPriorityChunksMemoryUsage, "browser.cache.disk.max_priority_chunks_memory_usage", kDefaultMaxDiskPriorityChunksMemoryUsage);

  // http://mxr.mozilla.org/mozilla-central/source/netwerk/cache/nsCacheEntryDescriptor.cpp#367
  mozilla::Preferences::AddUintVarCache(
    &sCompressionLevel, "browser.cache.compression_level", kDefaultCompressionLevel);

  mozilla::Preferences::GetComplex(
    "browser.cache.disk.parent_directory", NS_GET_IID(nsIFile),
    getter_AddRefs(mCacheParentDirectoryOverride));

  // First check the default value.  If it is at -1, the experient
  // is turned off.  If it is at 0, then use the user pref value
  // instead.
  sHalfLifeExperiment = mozilla::Preferences::GetDefaultInt(
    "browser.cache.frecency_experiment", kDefaultHalfLifeExperiment);

  if (sHalfLifeExperiment == 0) {
    // Default preferences indicate we want to run the experiment,
    // hence read the user value.
    sHalfLifeExperiment = mozilla::Preferences::GetInt(
      "browser.cache.frecency_experiment", sHalfLifeExperiment);
  }

  if (sHalfLifeExperiment == 0) {
    // The experiment has not yet been initialized but is engaged, do
    // the initialization now.
    srand(time(NULL));
    sHalfLifeExperiment = (rand() % 4) + 1;
    // Store the experiemnt value, since we need it not to change between
    // browser sessions.
    mozilla::Preferences::SetInt(
      "browser.cache.frecency_experiment", sHalfLifeExperiment);
  }

  switch (sHalfLifeExperiment) {
  case 1: // The experiment is engaged
    sHalfLifeHours = 0.083F; // ~5 mintues
    break;
  case 2:
    sHalfLifeHours = 0.25F; // 15 mintues
    break;
  case 3:
    sHalfLifeHours = 1.0F;
    break;
  case 4:
    sHalfLifeHours = 6.0F;
    break;

  case -1:
  default: // The experiment is off or broken
    sHalfLifeExperiment = -1;
    sHalfLifeHours = std::max(0.01F, std::min(1440.0F, mozilla::Preferences::GetFloat(
      "browser.cache.frecency_half_life_hours", kDefaultHalfLifeHours)));
    break;
  }

  mozilla::Preferences::AddBoolVarCache(
    &sSanitizeOnShutdown, "privacy.sanitize.sanitizeOnShutdown", kDefaultSanitizeOnShutdown);
  mozilla::Preferences::AddBoolVarCache(
    &sClearCacheOnShutdown, "privacy.clearOnShutdown.cache", kDefaultClearCacheOnShutdown);

  mozilla::Preferences::AddAtomicUintVarCache(
    &sMaxShutdownIOLag, "browser.cache.max_shutdown_io_lag", kDefaultMaxShutdownIOLag);
}

// static
uint32_t CacheObserver::MemoryCacheCapacity()
{
  if (sMemoryCacheCapacity >= 0)
    return sMemoryCacheCapacity << 10;

  if (sAutoMemoryCacheCapacity != -1)
    return sAutoMemoryCacheCapacity;

  static uint64_t bytes = PR_GetPhysicalMemorySize();
  // If getting the physical memory failed, arbitrarily assume
  // 32 MB of RAM. We use a low default to have a reasonable
  // size on all the devices we support.
  if (bytes == 0)
    bytes = 32 * 1024 * 1024;

  // Conversion from unsigned int64_t to double doesn't work on all platforms.
  // We need to truncate the value at INT64_MAX to make sure we don't
  // overflow.
  if (bytes > INT64_MAX)
    bytes = INT64_MAX;

  uint64_t kbytes = bytes >> 10;
  double kBytesD = double(kbytes);
  double x = log(kBytesD)/log(2.0) - 14;

  int32_t capacity = 0;
  if (x > 0) {
    capacity = (int32_t)(x * x / 3.0 + x + 2.0 / 3 + 0.1); // 0.1 for rounding
    if (capacity > 32)
      capacity = 32;
    capacity <<= 20;
  }

  // Result is in bytes.
  return sAutoMemoryCacheCapacity = capacity;
}

// static
bool CacheObserver::UseNewCache()
{
  uint32_t useNewCache = sUseNewCache;

  if (sUseNewCacheTemp)
    useNewCache = 1;

  switch (useNewCache) {
    case 0: // use the old cache backend
      return false;

    case 1: // use the new cache backend
      return true;
  }

  return true;
}

// static
void
CacheObserver::SetDiskCacheCapacity(uint32_t aCapacity)
{
  sDiskCacheCapacity = aCapacity >> 10;

  if (!sSelf) {
    return;
  }

  if (NS_IsMainThread()) {
    sSelf->StoreDiskCacheCapacity();
  } else {
    nsCOMPtr<nsIRunnable> event =
      NewRunnableMethod(sSelf, &CacheObserver::StoreDiskCacheCapacity);
    NS_DispatchToMainThread(event);
  }
}

void
CacheObserver::StoreDiskCacheCapacity()
{
  mozilla::Preferences::SetInt("browser.cache.disk.capacity",
                               sDiskCacheCapacity);
}

// static
void
CacheObserver::SetCacheFSReported()
{
  sCacheFSReported = true;

  if (!sSelf) {
    return;
  }

  if (NS_IsMainThread()) {
    sSelf->StoreCacheFSReported();
  } else {
    nsCOMPtr<nsIRunnable> event =
      NewRunnableMethod(sSelf, &CacheObserver::StoreCacheFSReported);
    NS_DispatchToMainThread(event);
  }
}

void
CacheObserver::StoreCacheFSReported()
{
  mozilla::Preferences::SetInt("browser.cache.disk.filesystem_reported",
                               sCacheFSReported);
}

// static
void
CacheObserver::SetHashStatsReported()
{
  sHashStatsReported = true;

  if (!sSelf) {
    return;
  }

  if (NS_IsMainThread()) {
    sSelf->StoreHashStatsReported();
  } else {
    nsCOMPtr<nsIRunnable> event =
      NewRunnableMethod(sSelf, &CacheObserver::StoreHashStatsReported);
    NS_DispatchToMainThread(event);
  }
}

void
CacheObserver::StoreHashStatsReported()
{
  mozilla::Preferences::SetInt("browser.cache.disk.hashstats_reported",
                               sHashStatsReported);
}

// static
void CacheObserver::ParentDirOverride(nsIFile** aDir)
{
  if (NS_WARN_IF(!aDir))
    return;

  *aDir = nullptr;

  if (!sSelf)
    return;
  if (!sSelf->mCacheParentDirectoryOverride)
    return;

  sSelf->mCacheParentDirectoryOverride->Clone(aDir);
}

namespace {
namespace CacheStorageEvictHelper {

nsresult ClearStorage(bool const aPrivate,
                      bool const aAnonymous,
                      NeckoOriginAttributes &aOa)
{
  nsresult rv;

  aOa.SyncAttributesWithPrivateBrowsing(aPrivate);
  RefPtr<LoadContextInfo> info = GetLoadContextInfo(aAnonymous, aOa);

  nsCOMPtr<nsICacheStorage> storage;
  RefPtr<CacheStorageService> service = CacheStorageService::Self();
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

  return NS_OK;
}

nsresult Run(NeckoOriginAttributes &aOa)
{
  nsresult rv;

  // Clear all [private X anonymous] combinations
  rv = ClearStorage(false, false, aOa);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = ClearStorage(false, true, aOa);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = ClearStorage(true, false, aOa);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = ClearStorage(true, true, aOa);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

} // CacheStorageEvictHelper
} // anon

// static
bool CacheObserver::EntryIsTooBig(int64_t aSize, bool aUsingDisk)
{
  // If custom limit is set, check it.
  int64_t preferredLimit = aUsingDisk ? sMaxDiskEntrySize : sMaxMemoryEntrySize;

  // do not convert to bytes when the limit is -1, which means no limit
  if (preferredLimit > 0) {
    preferredLimit <<= 10;
  }

  if (preferredLimit != -1 && aSize > preferredLimit)
    return true;

  // Otherwise (or when in the custom limit), check limit based on the global
  // limit.  It's 1/8 (>> 3) of the respective capacity.
  int64_t derivedLimit = aUsingDisk
    ? (static_cast<int64_t>(DiskCacheCapacity() >> 3))
    : (static_cast<int64_t>(MemoryCacheCapacity() >> 3));

  if (aSize > derivedLimit)
    return true;

  return false;
}

// static
bool CacheObserver::IsPastShutdownIOLag()
{
#ifdef DEBUG
  return false;
#endif

  if (sShutdownDemandedTime == PR_INTERVAL_NO_TIMEOUT ||
      sMaxShutdownIOLag == UINT32_MAX) {
    return false;
  }

  static const PRIntervalTime kMaxShutdownIOLag =
    PR_SecondsToInterval(sMaxShutdownIOLag);

  if ((PR_IntervalNow() - sShutdownDemandedTime) > kMaxShutdownIOLag) {
    return true;
  }

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

  if (!strcmp(aTopic, "browser-delayed-startup-finished")) {
    uint32_t activeVersion = UseNewCache() ? 1 : 0;
    CacheStorageService::CleaupCacheDirectories(sAutoDeleteCacheVersion, activeVersion);
    return NS_OK;
  }

  if (!strcmp(aTopic, "profile-change-net-teardown") ||
      !strcmp(aTopic, "profile-before-change") ||
      !strcmp(aTopic, "xpcom-shutdown")) {
    if (sShutdownDemandedTime == PR_INTERVAL_NO_TIMEOUT) {
      sShutdownDemandedTime = PR_IntervalNow();
    }

    RefPtr<CacheStorageService> service = CacheStorageService::Self();
    if (service) {
      service->Shutdown();
    }

    CacheFileIOManager::Shutdown();
    return NS_OK;
  }

  if (!strcmp(aTopic, "last-pb-context-exited")) {
    RefPtr<CacheStorageService> service = CacheStorageService::Self();
    if (service) {
      service->DropPrivateBrowsingEntries();
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, "clear-origin-data")) {
    NeckoOriginAttributes oa;
    if (!oa.Init(nsDependentString(aData))) {
      NS_ERROR("Could not parse NeckoOriginAttributes JSON in clear-origin-data notification");
      return NS_OK;
    }

    nsresult rv = CacheStorageEvictHelper::Run(oa);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  if (!strcmp(aTopic, "memory-pressure")) {
    RefPtr<CacheStorageService> service = CacheStorageService::Self();
    if (service)
      service->PurgeFromMemory(nsICacheStorageService::PURGE_EVERYTHING);

    return NS_OK;
  }

  MOZ_ASSERT(false, "Missing observer handler");
  return NS_OK;
}

} // namespace net
} // namespace mozilla
