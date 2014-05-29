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

class CacheObserver : public nsIObserver
                    , public nsSupportsWeakReference
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  virtual ~CacheObserver() {}

  static nsresult Init();
  static nsresult Shutdown();
  static CacheObserver* Self() { return sSelf; }

  // Access to preferences
  static bool const UseNewCache();
  static bool const UseDiskCache()
    { return sUseDiskCache; }
  static bool const UseMemoryCache()
    { return sUseMemoryCache; }
  static uint32_t const MetadataMemoryLimit() // result in bytes.
    { return sMetadataMemoryLimit << 10; }
  static uint32_t const MemoryCacheCapacity(); // result in bytes.
  static uint32_t const DiskCacheCapacity() // result in bytes.
    { return sDiskCacheCapacity << 10; }
  static void SetDiskCacheCapacity(uint32_t); // parameter in bytes.
  static bool const SmartCacheSizeEnabled()
    { return sSmartCacheSizeEnabled; }
  static uint32_t const PreloadChunkCount()
    { return sPreloadChunkCount; }
  static uint32_t const MaxMemoryEntrySize() // result in bytes.
    { return sMaxMemoryEntrySize << 10; }
  static uint32_t const MaxDiskEntrySize() // result in bytes.
    { return sMaxDiskEntrySize << 10; }
  static uint32_t const CompressionLevel()
    { return sCompressionLevel; }
  static uint32_t const HalfLifeSeconds()
    { return sHalfLifeHours * 60 * 60; }
  static int32_t const HalfLifeExperiment()
    { return sHalfLifeExperiment; }
  static bool const ClearCacheOnShutdown()
    { return sSanitizeOnShutdown && sClearCacheOnShutdown; }
  static void ParentDirOverride(nsIFile ** aDir);

  static bool const EntryIsTooBig(int64_t aSize, bool aUsingDisk);

private:
  static CacheObserver* sSelf;

  void StoreDiskCacheCapacity();
  void AttachToPreferences();

  static uint32_t sUseNewCache;
  static bool sUseMemoryCache;
  static bool sUseDiskCache;
  static uint32_t sMetadataMemoryLimit;
  static int32_t sMemoryCacheCapacity;
  static int32_t sAutoMemoryCacheCapacity;
  static uint32_t sDiskCacheCapacity;
  static bool sSmartCacheSizeEnabled;
  static uint32_t sPreloadChunkCount;
  static uint32_t sMaxMemoryEntrySize;
  static uint32_t sMaxDiskEntrySize;
  static uint32_t sCompressionLevel;
  static uint32_t sHalfLifeHours;
  static int32_t sHalfLifeExperiment;
  static bool sSanitizeOnShutdown;
  static bool sClearCacheOnShutdown;

  // Non static properties, accessible via sSelf
  nsCOMPtr<nsIFile> mCacheParentDirectoryOverride;
};

} // net
} // mozilla

#endif
