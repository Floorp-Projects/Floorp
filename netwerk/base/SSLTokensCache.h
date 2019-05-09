/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SSLTokensCache_h_
#define SSLTokensCache_h_

#include "nsIMemoryReporter.h"
#include "nsClassHashtable.h"
#include "nsTArray.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace net {

class SSLTokensCache : public nsIMemoryReporter {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  friend class ExpirationComparator;

  static nsresult Init();
  static nsresult Shutdown();

  static bool IsEnabled() { return sEnabled; }

  static nsresult Put(const nsACString& aHost, const uint8_t* aToken,
                      uint32_t aTokenLen);
  static nsresult Get(const nsACString& aHost, nsTArray<uint8_t>& aToken);
  static nsresult Remove(const nsACString& aHost);

 private:
  SSLTokensCache();
  virtual ~SSLTokensCache();

  nsresult RemoveLocked(const nsACString& aHost);

  void InitPrefs();
  void EvictIfNecessary();
  void LogStats();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  static mozilla::StaticRefPtr<SSLTokensCache> gInstance;
  static StaticMutex sLock;

  static Atomic<bool, Relaxed> sEnabled;
  // Capacity of the cache in kilobytes
  static Atomic<uint32_t, Relaxed> sCapacity;

  uint32_t mCacheSize;  // Actual cache size in bytes

  class HostRecord {
   public:
    nsCString mHost;
    PRUint32 mExpirationTime;
    nsTArray<uint8_t> mToken;
  };

  nsClassHashtable<nsCStringHashKey, HostRecord> mHostRecs;
  nsTArray<HostRecord*> mExpirationArray;
};

}  // namespace net
}  // namespace mozilla

#endif  // SSLTokensCache_h_
