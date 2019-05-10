/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheObserver__h__
#define CacheObserver__h__

#include "nsIObserver.h"
#include "nsIFile.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
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
  static bool UseDiskCache() { return sUseDiskCache; }
  static bool UseMemoryCache() { return sUseMemoryCache; }
  static uint32_t MetadataMemoryLimit()  // result in kilobytes.
  {
    return sMetadataMemoryLimit;
  }
  static uint32_t MemoryCacheCapacity();  // result in kilobytes.
  static uint32_t DiskCacheCapacity()     // result in kilobytes.
  {
    return sDiskCacheCapacity;
  }
  static void SetDiskCacheCapacity(uint32_t);  // parameter in kilobytes.
  static uint32_t DiskFreeSpaceSoftLimit()     // result in kilobytes.
  {
    return sDiskFreeSpaceSoftLimit;
  }
  static uint32_t DiskFreeSpaceHardLimit()  // result in kilobytes.
  {
    return sDiskFreeSpaceHardLimit;
  }
  static bool SmartCacheSizeEnabled() { return sSmartCacheSizeEnabled; }
  static uint32_t PreloadChunkCount() { return sPreloadChunkCount; }
  static uint32_t MaxMemoryEntrySize()  // result in kilobytes.
  {
    return sMaxMemoryEntrySize;
  }
  static uint32_t MaxDiskEntrySize()  // result in kilobytes.
  {
    return sMaxDiskEntrySize;
  }
  static uint32_t MaxDiskChunksMemoryUsage(
      bool aPriority)  // result in kilobytes.
  {
    return aPriority ? sMaxDiskPriorityChunksMemoryUsage
                     : sMaxDiskChunksMemoryUsage;
  }
  static uint32_t CompressionLevel() { return sCompressionLevel; }
  static uint32_t HalfLifeSeconds() { return sHalfLifeHours * 60.0F * 60.0F; }
  static bool ClearCacheOnShutdown() {
    return sSanitizeOnShutdown && sClearCacheOnShutdown;
  }
  static bool CacheFSReported() { return sCacheFSReported; }
  static void SetCacheFSReported();
  static bool HashStatsReported() { return sHashStatsReported; }
  static void SetHashStatsReported();
  static uint32_t TelemetryReportID() { return sTelemetryReportID; }
  static void SetTelemetryReportID(uint32_t);
  static uint32_t CacheAmountWritten()  // result in kilobytes
  {
    return sCacheAmountWritten;
  }
  static void SetCacheAmountWritten(uint32_t);  // parameter in kilobytes.
  static void ParentDirOverride(nsIFile** aDir);

  static bool EntryIsTooBig(int64_t aSize, bool aUsingDisk);

  static uint32_t MaxShutdownIOLag() { return sMaxShutdownIOLag; }
  static bool IsPastShutdownIOLag();

  static bool ShuttingDown() {
    return sShutdownDemandedTime != PR_INTERVAL_NO_TIMEOUT;
  }

 private:
  static StaticRefPtr<CacheObserver> sSelf;

  void StoreDiskCacheCapacity();
  void StoreCacheFSReported();
  void StoreHashStatsReported();
  void StoreTelemetryReportID();
  void StoreCacheAmountWritten();
  void AttachToPreferences();

  static bool sUseMemoryCache;
  static bool sUseDiskCache;
  static uint32_t sMetadataMemoryLimit;
  static int32_t sMemoryCacheCapacity;
  static int32_t sAutoMemoryCacheCapacity;
  static Atomic<uint32_t, Relaxed> sDiskCacheCapacity;
  static uint32_t sDiskFreeSpaceSoftLimit;
  static uint32_t sDiskFreeSpaceHardLimit;
  static Atomic<bool, Relaxed> sSmartCacheSizeEnabled;
  static uint32_t sPreloadChunkCount;
  static int32_t sMaxMemoryEntrySize;
  static int32_t sMaxDiskEntrySize;
  static uint32_t sMaxDiskChunksMemoryUsage;
  static uint32_t sMaxDiskPriorityChunksMemoryUsage;
  static uint32_t sCompressionLevel;
  static float sHalfLifeHours;
  static bool sSanitizeOnShutdown;
  static bool sClearCacheOnShutdown;
  static bool sCacheFSReported;
  static bool sHashStatsReported;
  static Atomic<uint32_t, Relaxed> sMaxShutdownIOLag;
  static Atomic<PRIntervalTime> sShutdownDemandedTime;
  static Atomic<uint32_t, Relaxed> sTelemetryReportID;
  static Atomic<uint32_t, Relaxed> sCacheAmountWritten;

  // Non static properties, accessible via sSelf
  nsCOMPtr<nsIFile> mCacheParentDirectoryOverride;
};

}  // namespace net
}  // namespace mozilla

#endif
