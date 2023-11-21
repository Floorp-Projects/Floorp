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
#include "nsIUserIdleService.h"

namespace mozilla::net {

StaticRefPtr<CacheObserver> CacheObserver::sSelf;

static float const kDefaultHalfLifeHours = 24.0F;  // 24 hours
float CacheObserver::sHalfLifeHours = kDefaultHalfLifeHours;

// The default value will be overwritten as soon as the correct smart size is
// calculated by CacheFileIOManager::UpdateSmartCacheSize(). It's limited to 1GB
// just for case the size is never calculated which might in theory happen if
// GetDiskSpaceAvailable() always fails.
Atomic<uint32_t, Relaxed> CacheObserver::sSmartDiskCacheCapacity(1024 * 1024);

Atomic<PRIntervalTime> CacheObserver::sShutdownDemandedTime(
    PR_INTERVAL_NO_TIMEOUT);

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
  obs->AddObserver(sSelf, "profile-before-change", true);
  obs->AddObserver(sSelf, "xpcom-shutdown", true);
  obs->AddObserver(sSelf, "last-pb-context-exited", true);
  obs->AddObserver(sSelf, "memory-pressure", true);
  obs->AddObserver(sSelf, "browser-delayed-startup-finished", true);
  obs->AddObserver(sSelf, OBSERVER_TOPIC_IDLE_DAILY, true);

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
  mozilla::Preferences::GetComplex(
      "browser.cache.disk.parent_directory", NS_GET_IID(nsIFile),
      getter_AddRefs(mCacheParentDirectoryOverride));

  sHalfLifeHours = std::max(
      0.01F, std::min(1440.0F, mozilla::Preferences::GetFloat(
                                   "browser.cache.frecency_half_life_hours",
                                   kDefaultHalfLifeHours)));
}

// static
uint32_t CacheObserver::MemoryCacheCapacity() {
  if (StaticPrefs::browser_cache_memory_capacity() >= 0) {
    return StaticPrefs::browser_cache_memory_capacity();
  }

  // Cache of the calculated memory capacity based on the system memory size in
  // KB (C++11 guarantees local statics will be initialized once and in a
  // thread-safe way.)
  static int32_t sAutoMemoryCacheCapacity = ([] {
    uint64_t bytes = PR_GetPhysicalMemorySize();
    // If getting the physical memory failed, arbitrarily assume
    // 32 MB of RAM. We use a low default to have a reasonable
    // size on all the devices we support.
    if (bytes == 0) {
      bytes = 32 * 1024 * 1024;
    }
    // Conversion from unsigned int64_t to double doesn't work on all platforms.
    // We need to truncate the value at INT64_MAX to make sure we don't
    // overflow.
    if (bytes > INT64_MAX) {
      bytes = INT64_MAX;
    }
    uint64_t kbytes = bytes >> 10;
    double kBytesD = double(kbytes);
    double x = log(kBytesD) / log(2.0) - 14;

    int32_t capacity = 0;
    if (x > 0) {
      // 0.1 is added here for rounding
      capacity = (int32_t)(x * x / 3.0 + x + 2.0 / 3 + 0.1);
      if (capacity > 32) {
        capacity = 32;
      }
      capacity <<= 10;
    }
    return capacity;
  })();

  return sAutoMemoryCacheCapacity;
}

// static
void CacheObserver::SetSmartDiskCacheCapacity(uint32_t aCapacity) {
  sSmartDiskCacheCapacity = aCapacity;
}

// static
uint32_t CacheObserver::DiskCacheCapacity() {
  return SmartCacheSizeEnabled() ? sSmartDiskCacheCapacity
                                 : StaticPrefs::browser_cache_disk_capacity();
}

// static
void CacheObserver::ParentDirOverride(nsIFile** aDir) {
  if (NS_WARN_IF(!aDir)) return;

  *aDir = nullptr;

  if (!sSelf) return;
  if (!sSelf->mCacheParentDirectoryOverride) return;

  sSelf->mCacheParentDirectoryOverride->Clone(aDir);
}

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

  return aSize > derivedLimit;
}

// static
bool CacheObserver::IsPastShutdownIOLag() {
#ifdef DEBUG
  return false;
#else
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
#endif
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

  if (!strcmp(aTopic, "memory-pressure")) {
    RefPtr<CacheStorageService> service = CacheStorageService::Self();
    if (service) {
      service->PurgeFromMemory(nsICacheStorageService::PURGE_EVERYTHING);
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, "browser-delayed-startup-finished")) {
    CacheFileIOManager::OnDelayedStartupFinished();
    return NS_OK;
  }

  if (!strcmp(aTopic, OBSERVER_TOPIC_IDLE_DAILY)) {
    CacheFileIOManager::OnIdleDaily();
    return NS_OK;
  }

  MOZ_ASSERT(false, "Missing observer handler");
  return NS_OK;
}

}  // namespace mozilla::net
