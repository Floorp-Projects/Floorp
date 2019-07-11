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

StaticRefPtr<CacheObserver> CacheObserver::sSelf;

static float const kDefaultHalfLifeHours = 24.0F;  // 24 hours
float CacheObserver::sHalfLifeHours = kDefaultHalfLifeHours;

// Cache of the calculated memory capacity based on the system memory size in KB
int32_t CacheObserver::sAutoMemoryCacheCapacity = -1;

static uint32_t const kDefaultDiskCacheCapacity = 250 * 1024;  // 250 MB
Atomic<uint32_t, Relaxed> CacheObserver::sDiskCacheCapacity(
    kDefaultDiskCacheCapacity);

static bool kDefaultCacheFSReported = false;
bool CacheObserver::sCacheFSReported = kDefaultCacheFSReported;

static bool kDefaultHashStatsReported = false;
bool CacheObserver::sHashStatsReported = kDefaultHashStatsReported;

Atomic<PRIntervalTime> CacheObserver::sShutdownDemandedTime(
    PR_INTERVAL_NO_TIMEOUT);

static uint32_t const kDefaultTelemetryReportID = 0;
Atomic<uint32_t, Relaxed> CacheObserver::sTelemetryReportID(
    kDefaultTelemetryReportID);

static uint32_t const kDefaultCacheAmountWritten = 0;
Atomic<uint32_t, Relaxed> CacheObserver::sCacheAmountWritten(
    kDefaultCacheAmountWritten);

NS_IMPL_ISUPPORTS(CacheObserver, nsIObserver, nsISupportsWeakReference)

// static
nsresult CacheObserver::Init() {
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

  obs->AddObserver(sSelf, "prefservice:after-app-defaults", true);
  obs->AddObserver(sSelf, "profile-do-change", true);
  obs->AddObserver(sSelf, "browser-delayed-startup-finished", true);
  obs->AddObserver(sSelf, "profile-before-change", true);
  obs->AddObserver(sSelf, "xpcom-shutdown", true);
  obs->AddObserver(sSelf, "last-pb-context-exited", true);
  obs->AddObserver(sSelf, "clear-origin-attributes-data", true);
  obs->AddObserver(sSelf, "memory-pressure", true);

  return NS_OK;
}

// static
nsresult CacheObserver::Shutdown() {
  if (!sSelf) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  sSelf = nullptr;
  return NS_OK;
}

void CacheObserver::AttachToPreferences() {
  mozilla::Preferences::AddAtomicUintVarCache(&sDiskCacheCapacity,
                                              "browser.cache.disk.capacity",
                                              kDefaultDiskCacheCapacity);

  mozilla::Preferences::GetComplex(
      "browser.cache.disk.parent_directory", NS_GET_IID(nsIFile),
      getter_AddRefs(mCacheParentDirectoryOverride));

  sHalfLifeHours = std::max(
      0.01F, std::min(1440.0F, mozilla::Preferences::GetFloat(
                                   "browser.cache.frecency_half_life_hours",
                                   kDefaultHalfLifeHours)));
  mozilla::Preferences::AddAtomicUintVarCache(
      &sTelemetryReportID, "browser.cache.disk.telemetry_report_ID",
      kDefaultTelemetryReportID);

  mozilla::Preferences::AddAtomicUintVarCache(
      &sCacheAmountWritten, "browser.cache.disk.amount_written",
      kDefaultCacheAmountWritten);
}

// static
uint32_t CacheObserver::MemoryCacheCapacity() {
  if (StaticPrefs::browser_cache_memory_capacity() >= 0) {
    return StaticPrefs::browser_cache_memory_capacity();
  }

  if (sAutoMemoryCacheCapacity != -1) return sAutoMemoryCacheCapacity;

  static uint64_t bytes = PR_GetPhysicalMemorySize();
  // If getting the physical memory failed, arbitrarily assume
  // 32 MB of RAM. We use a low default to have a reasonable
  // size on all the devices we support.
  if (bytes == 0) bytes = 32 * 1024 * 1024;

  // Conversion from unsigned int64_t to double doesn't work on all platforms.
  // We need to truncate the value at INT64_MAX to make sure we don't
  // overflow.
  if (bytes > INT64_MAX) bytes = INT64_MAX;

  uint64_t kbytes = bytes >> 10;
  double kBytesD = double(kbytes);
  double x = log(kBytesD) / log(2.0) - 14;

  int32_t capacity = 0;
  if (x > 0) {
    capacity = (int32_t)(x * x / 3.0 + x + 2.0 / 3 + 0.1);  // 0.1 for rounding
    if (capacity > 32) capacity = 32;
    capacity <<= 10;
  }

  // Result is in kilobytes.
  return sAutoMemoryCacheCapacity = capacity;
}

// static
void CacheObserver::SetDiskCacheCapacity(uint32_t aCapacity) {
  sDiskCacheCapacity = aCapacity;

  if (!sSelf) {
    return;
  }

  if (NS_IsMainThread()) {
    sSelf->StoreDiskCacheCapacity();
  } else {
    nsCOMPtr<nsIRunnable> event =
        NewRunnableMethod("net::CacheObserver::StoreDiskCacheCapacity",
                          sSelf.get(), &CacheObserver::StoreDiskCacheCapacity);
    NS_DispatchToMainThread(event);
  }
}

void CacheObserver::StoreDiskCacheCapacity() {
  mozilla::Preferences::SetInt("browser.cache.disk.capacity",
                               sDiskCacheCapacity);
}

// static
void CacheObserver::SetCacheFSReported() {
  sCacheFSReported = true;

  if (!sSelf) {
    return;
  }

  if (NS_IsMainThread()) {
    sSelf->StoreCacheFSReported();
  } else {
    nsCOMPtr<nsIRunnable> event =
        NewRunnableMethod("net::CacheObserver::StoreCacheFSReported",
                          sSelf.get(), &CacheObserver::StoreCacheFSReported);
    NS_DispatchToMainThread(event);
  }
}

void CacheObserver::StoreCacheFSReported() {
  mozilla::Preferences::SetInt("browser.cache.disk.filesystem_reported",
                               sCacheFSReported);
}

// static
void CacheObserver::SetHashStatsReported() {
  sHashStatsReported = true;

  if (!sSelf) {
    return;
  }

  if (NS_IsMainThread()) {
    sSelf->StoreHashStatsReported();
  } else {
    nsCOMPtr<nsIRunnable> event =
        NewRunnableMethod("net::CacheObserver::StoreHashStatsReported",
                          sSelf.get(), &CacheObserver::StoreHashStatsReported);
    NS_DispatchToMainThread(event);
  }
}

void CacheObserver::StoreHashStatsReported() {
  mozilla::Preferences::SetInt("browser.cache.disk.hashstats_reported",
                               sHashStatsReported);
}

// static
void CacheObserver::SetTelemetryReportID(uint32_t aTelemetryReportID) {
  sTelemetryReportID = aTelemetryReportID;

  if (!sSelf) {
    return;
  }

  if (NS_IsMainThread()) {
    sSelf->StoreTelemetryReportID();
  } else {
    nsCOMPtr<nsIRunnable> event =
        NewRunnableMethod("net::CacheObserver::StoreTelemetryReportID",
                          sSelf.get(), &CacheObserver::StoreTelemetryReportID);
    NS_DispatchToMainThread(event);
  }
}

void CacheObserver::StoreTelemetryReportID() {
  mozilla::Preferences::SetInt("browser.cache.disk.telemetry_report_ID",
                               sTelemetryReportID);
}

// static
void CacheObserver::SetCacheAmountWritten(uint32_t aCacheAmountWritten) {
  sCacheAmountWritten = aCacheAmountWritten;

  if (!sSelf) {
    return;
  }

  if (NS_IsMainThread()) {
    sSelf->StoreCacheAmountWritten();
  } else {
    nsCOMPtr<nsIRunnable> event =
        NewRunnableMethod("net::CacheObserver::StoreCacheAmountWritten",
                          sSelf.get(), &CacheObserver::StoreCacheAmountWritten);
    NS_DispatchToMainThread(event);
  }
}

void CacheObserver::StoreCacheAmountWritten() {
  mozilla::Preferences::SetInt("browser.cache.disk.amount_written",
                               sCacheAmountWritten);
}

// static
void CacheObserver::ParentDirOverride(nsIFile** aDir) {
  if (NS_WARN_IF(!aDir)) return;

  *aDir = nullptr;

  if (!sSelf) return;
  if (!sSelf->mCacheParentDirectoryOverride) return;

  sSelf->mCacheParentDirectoryOverride->Clone(aDir);
}

namespace {
namespace CacheStorageEvictHelper {

nsresult ClearStorage(bool const aPrivate, bool const aAnonymous,
                      OriginAttributes& aOa) {
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

nsresult Run(OriginAttributes& aOa) {
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

}  // namespace CacheStorageEvictHelper
}  // namespace

// static
bool CacheObserver::EntryIsTooBig(int64_t aSize, bool aUsingDisk) {
  // If custom limit is set, check it.
  int64_t preferredLimit =
      aUsingDisk ? MaxDiskEntrySize() : MaxMemoryEntrySize();

  // do not convert to bytes when the limit is -1, which means no limit
  if (preferredLimit > 0) {
    preferredLimit <<= 10;
  }

  if (preferredLimit != -1 && aSize > preferredLimit) return true;

  // Otherwise (or when in the custom limit), check limit based on the global
  // limit. It's 1/8 of the respective capacity.
  int64_t derivedLimit =
      aUsingDisk ? DiskCacheCapacity() : MemoryCacheCapacity();
  derivedLimit <<= (10 - 3);

  if (aSize > derivedLimit) return true;

  return false;
}

// static
bool CacheObserver::IsPastShutdownIOLag() {
#ifdef DEBUG
  return false;
#endif

  if (sShutdownDemandedTime == PR_INTERVAL_NO_TIMEOUT ||
      MaxShutdownIOLag() == UINT32_MAX) {
    return false;
  }

  static const PRIntervalTime kMaxShutdownIOLag =
      PR_SecondsToInterval(MaxShutdownIOLag());

  if ((PR_IntervalNow() - sShutdownDemandedTime) > kMaxShutdownIOLag) {
    return true;
  }

  return false;
}

NS_IMETHODIMP
CacheObserver::Observe(nsISupports* aSubject, const char* aTopic,
                       const char16_t* aData) {
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
    CacheStorageService::CleaupCacheDirectories();
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

  if (!strcmp(aTopic, "clear-origin-attributes-data")) {
    OriginAttributes oa;
    if (!oa.Init(nsDependentString(aData))) {
      NS_ERROR(
          "Could not parse OriginAttributes JSON in "
          "clear-origin-attributes-data notification");
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

}  // namespace net
}  // namespace mozilla
