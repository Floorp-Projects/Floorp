/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheObserver__h__
#define CacheObserver__h__

#include "nsIObserver.h"
#include "nsWeakReference.h"
#include <algorithm>

namespace mozilla {
namespace net {

class CacheObserver : public nsIObserver
                    , public nsSupportsWeakReference
{
  NS_DECL_ISUPPORTS
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
  static uint32_t const MemoryLimit() // <0.5MB,1024MB>, result in bytes.
    { return std::max(512U, std::min(1048576U, sMemoryLimit)) << 10; }
  static uint32_t const DiskCacheCapacity() // result in bytes.
    { return sDiskCacheCapacity << 10; }
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

  static bool const EntryIsTooBig(int64_t aSize, bool aUsingDisk);

private:
  static CacheObserver* sSelf;

  void AttachToPreferences();
  void SchduleAutoDelete();

  static uint32_t sUseNewCache;
  static bool sUseDiskCache;
  static bool sUseMemoryCache;
  static uint32_t sMemoryLimit;
  static uint32_t sDiskCacheCapacity;
  static uint32_t sMaxMemoryEntrySize;
  static uint32_t sMaxDiskEntrySize;
  static uint32_t sCompressionLevel;
  static uint32_t sHalfLifeHours;
  static int32_t sHalfLifeExperiment;
};

} // net
} // mozilla

#endif
