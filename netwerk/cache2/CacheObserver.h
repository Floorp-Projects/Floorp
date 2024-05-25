/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheObserver__h__
#define CacheObserver__h__

#include "nsIObserver.h"
#include "nsIFile.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StaticPtr.h"
#include <algorithm>

namespace mozilla {
namespace net {

class CacheObserver : public nsIObserver, public nsSupportsWeakReference {
  virtual ~CacheObserver() = default;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static nsresult Init();
  static nsresult Shutdown();
  static CacheObserver* Self() { return sSelf; }

  // Access to preferences
  static bool UseDiskCache() {
    return StaticPrefs::browser_cache_disk_enable();
  }
  static bool UseMemoryCache() {
    return StaticPrefs::browser_cache_memory_enable();
  }
  static uint32_t MetadataMemoryLimit()  // result in kilobytes.
  {
    return StaticPrefs::browser_cache_disk_metadata_memory_limit();
  }
  static uint32_t MemoryCacheCapacity();            // result in kilobytes.
  static uint32_t DiskCacheCapacity();              // result in kilobytes.
  static void SetSmartDiskCacheCapacity(uint32_t);  // parameter in kilobytes.
  static uint32_t DiskFreeSpaceSoftLimit()          // result in kilobytes.
  {
    return StaticPrefs::browser_cache_disk_free_space_soft_limit();
  }
  static uint32_t DiskFreeSpaceHardLimit()  // result in kilobytes.
  {
    return StaticPrefs::browser_cache_disk_free_space_hard_limit();
  }
  static bool SmartCacheSizeEnabled() {
    return StaticPrefs::browser_cache_disk_smart_size_enabled();
  }
  static uint32_t PreloadChunkCount() {
    return StaticPrefs::browser_cache_disk_preload_chunk_count();
  }
  static uint32_t MaxMemoryEntrySize()  // result in kilobytes.
  {
    return StaticPrefs::browser_cache_memory_max_entry_size();
  }
  static uint32_t MaxDiskEntrySize()  // result in kilobytes.
  {
    return StaticPrefs::browser_cache_disk_max_entry_size();
  }
  static uint32_t MaxDiskChunksMemoryUsage(
      bool aPriority)  // result in kilobytes.
  {
    return aPriority
               ? StaticPrefs::
                     browser_cache_disk_max_priority_chunks_memory_usage()
               : StaticPrefs::browser_cache_disk_max_chunks_memory_usage();
  }
  static uint32_t HalfLifeSeconds() { return sHalfLifeHours * 60.0F * 60.0F; }
  static bool ClearCacheOnShutdown() {
    if (!StaticPrefs::privacy_sanitize_sanitizeOnShutdown()) {
      return false;
    }
    if (StaticPrefs::privacy_sanitize_useOldClearHistoryDialog()) {
      return StaticPrefs::privacy_clearOnShutdown_cache();
    }
    // use the new cache clearing pref for the new clear history dialog
    return StaticPrefs::privacy_clearOnShutdown_v2_cache();
  }
  static void ParentDirOverride(nsIFile** aDir);

  static bool EntryIsTooBig(int64_t aSize, bool aUsingDisk);

  static uint32_t MaxShutdownIOLag() {
    return StaticPrefs::browser_cache_max_shutdown_io_lag();
  }
  static bool IsPastShutdownIOLag();

  static bool ShuttingDown() {
    return sShutdownDemandedTime != PR_INTERVAL_NO_TIMEOUT;
  }

 private:
  static StaticRefPtr<CacheObserver> sSelf;

  void AttachToPreferences();

  static int32_t sAutoMemoryCacheCapacity;
  static Atomic<uint32_t, Relaxed> sSmartDiskCacheCapacity;
  static float sHalfLifeHours;
  static Atomic<PRIntervalTime> sShutdownDemandedTime;

  // Non static properties, accessible via sSelf
  nsCOMPtr<nsIFile> mCacheParentDirectoryOverride;
};

}  // namespace net
}  // namespace mozilla

#endif
