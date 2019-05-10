/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SSLTokensCache.h"
#include "mozilla/Preferences.h"
#include "mozilla/Logging.h"
#include "ssl.h"
#include "sslexp.h"

namespace mozilla {
namespace net {

static bool const kDefaultEnabled = false;
Atomic<bool, Relaxed> SSLTokensCache::sEnabled(kDefaultEnabled);

static uint32_t const kDefaultCapacity = 2048;  // 2MB
Atomic<uint32_t, Relaxed> SSLTokensCache::sCapacity(kDefaultCapacity);

static LazyLogModule gSSLTokensCacheLog("SSLTokensCache");
#undef LOG
#define LOG(args) MOZ_LOG(gSSLTokensCacheLog, mozilla::LogLevel::Debug, args)

class ExpirationComparator {
 public:
  bool Equals(SSLTokensCache::HostRecord* a,
              SSLTokensCache::HostRecord* b) const {
    return a->mExpirationTime == b->mExpirationTime;
  }
  bool LessThan(SSLTokensCache::HostRecord* a,
                SSLTokensCache::HostRecord* b) const {
    return a->mExpirationTime < b->mExpirationTime;
  }
};

StaticRefPtr<SSLTokensCache> SSLTokensCache::gInstance;
StaticMutex SSLTokensCache::sLock;

NS_IMPL_ISUPPORTS(SSLTokensCache, nsIMemoryReporter)

// static
nsresult SSLTokensCache::Init() {
  StaticMutexAutoLock lock(sLock);

  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return NS_OK;
  }

  MOZ_ASSERT(!gInstance);

  gInstance = new SSLTokensCache();
  gInstance->InitPrefs();

  RegisterWeakMemoryReporter(gInstance);

  return NS_OK;
}

// static
nsresult SSLTokensCache::Shutdown() {
  StaticMutexAutoLock lock(sLock);

  if (!gInstance) {
    return NS_ERROR_UNEXPECTED;
  }

  UnregisterWeakMemoryReporter(gInstance);

  gInstance = nullptr;

  return NS_OK;
}

SSLTokensCache::SSLTokensCache() : mCacheSize(0) {
  LOG(("SSLTokensCache::SSLTokensCache"));
}

SSLTokensCache::~SSLTokensCache() { LOG(("SSLTokensCache::~SSLTokensCache")); }

// static
nsresult SSLTokensCache::Put(const nsACString& aHost, const uint8_t* aToken,
                             uint32_t aTokenLen) {
  StaticMutexAutoLock lock(sLock);

  LOG(("SSLTokensCache::Put [host=%s, tokenLen=%u]",
       PromiseFlatCString(aHost).get(), aTokenLen));

  if (!gInstance) {
    LOG(("  service not initialized"));
    return NS_ERROR_NOT_INITIALIZED;
  }

  PRUint32 expirationTime;
  SSLResumptionTokenInfo tokenInfo;
  if (SSL_GetResumptionTokenInfo(aToken, aTokenLen, &tokenInfo,
                                 sizeof(tokenInfo)) != SECSuccess) {
    LOG(("  cannot get expiration time from the token, NSS error %d",
         PORT_GetError()));
    return NS_ERROR_FAILURE;
  }
  expirationTime = tokenInfo.expirationTime;
  SSL_DestroyResumptionTokenInfo(&tokenInfo);

  HostRecord* rec = nullptr;

  if (!gInstance->mHostRecs.Get(aHost, &rec)) {
    rec = new HostRecord();
    rec->mHost = aHost;
    gInstance->mHostRecs.Put(aHost, rec);
    gInstance->mExpirationArray.AppendElement(rec);
  } else {
    gInstance->mCacheSize -= rec->mToken.Length();
    rec->mToken.Clear();
  }

  rec->mExpirationTime = expirationTime;
  MOZ_ASSERT(rec->mToken.IsEmpty());
  rec->mToken.AppendElements(aToken, aTokenLen);
  gInstance->mCacheSize += rec->mToken.Length();

  gInstance->LogStats();

  gInstance->EvictIfNecessary();

  return NS_OK;
}

// static
nsresult SSLTokensCache::Get(const nsACString& aHost,
                             nsTArray<uint8_t>& aToken) {
  StaticMutexAutoLock lock(sLock);

  LOG(("SSLTokensCache::Get [host=%s]", PromiseFlatCString(aHost).get()));

  if (!gInstance) {
    LOG(("  service not initialized"));
    return NS_ERROR_NOT_INITIALIZED;
  }

  HostRecord* rec = nullptr;

  if (gInstance->mHostRecs.Get(aHost, &rec)) {
    if (rec->mToken.Length()) {
      aToken = rec->mToken;
      return NS_OK;
    }
  }

  LOG(("  token not found"));
  return NS_ERROR_NOT_AVAILABLE;
}

// static
nsresult SSLTokensCache::Remove(const nsACString& aHost) {
  StaticMutexAutoLock lock(sLock);

  LOG(("SSLTokensCache::Remove [host=%s]", PromiseFlatCString(aHost).get()));

  if (!gInstance) {
    LOG(("  service not initialized"));
    return NS_ERROR_NOT_INITIALIZED;
  }

  return gInstance->RemoveLocked(aHost);
}

nsresult SSLTokensCache::RemoveLocked(const nsACString& aHost) {
  sLock.AssertCurrentThreadOwns();

  LOG(("SSLTokensCache::RemoveLocked [host=%s]",
       PromiseFlatCString(aHost).get()));

  nsAutoPtr<HostRecord> rec;

  if (!mHostRecs.Remove(aHost, &rec)) {
    LOG(("  token not found"));
    return NS_ERROR_NOT_AVAILABLE;
  }

  mCacheSize -= rec->mToken.Length();

  if (!mExpirationArray.RemoveElement(rec)) {
    MOZ_ASSERT(false, "token not found in mExpirationArray");
  }

  LogStats();

  return NS_OK;
}

void SSLTokensCache::InitPrefs() {
  Preferences::AddAtomicBoolVarCache(
      &sEnabled, "network.ssl_tokens_cache_enabled", kDefaultEnabled);
  Preferences::AddAtomicUintVarCache(
      &sCapacity, "network.ssl_tokens_cache_capacity", kDefaultCapacity);
}

void SSLTokensCache::EvictIfNecessary() {
  uint32_t capacity = sCapacity << 10;  // kilobytes to bytes
  if (mCacheSize <= capacity) {
    return;
  }

  LOG(("SSLTokensCache::EvictIfNecessary - evicting"));

  mExpirationArray.Sort(ExpirationComparator());

  while (mCacheSize > capacity && mExpirationArray.Length() > 0) {
    if (NS_FAILED(RemoveLocked(mExpirationArray[0]->mHost))) {
      MOZ_ASSERT(false, "mExpirationArray and mHostRecs are out of sync!");
      mExpirationArray.RemoveElementAt(0);
    }
  }
}

void SSLTokensCache::LogStats() {
  LOG(("SSLTokensCache::LogStats [count=%zu, cacheSize=%u]",
       mExpirationArray.Length(), mCacheSize));
}

size_t SSLTokensCache::SizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  size_t n = mallocSizeOf(this);

  n += mHostRecs.ShallowSizeOfExcludingThis(mallocSizeOf);
  n += mExpirationArray.ShallowSizeOfExcludingThis(mallocSizeOf);

  for (uint32_t i = 0; i < mExpirationArray.Length(); ++i) {
    n += mallocSizeOf(mExpirationArray[i]);
    n += mExpirationArray[i]->mHost.SizeOfExcludingThisIfUnshared(mallocSizeOf);
    n += mExpirationArray[i]->mToken.ShallowSizeOfExcludingThis(mallocSizeOf);
  }

  return n;
}

MOZ_DEFINE_MALLOC_SIZE_OF(SSLTokensCacheMallocSizeOf)

NS_IMETHODIMP
SSLTokensCache::CollectReports(nsIHandleReportCallback* aHandleReport,
                               nsISupports* aData, bool aAnonymize) {
  StaticMutexAutoLock lock(sLock);

  MOZ_COLLECT_REPORT("explicit/network/ssl-tokens-cache", KIND_HEAP,
                     UNITS_BYTES,
                     SizeOfIncludingThis(SSLTokensCacheMallocSizeOf),
                     "Memory used for the SSL tokens cache.");

  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
