/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SSLTokensCache.h"
#include "mozilla/Preferences.h"
#include "mozilla/Logging.h"
#include "nsIOService.h"
#include "nsNSSIOLayer.h"
#include "TransportSecurityInfo.h"
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
  bool Equals(SSLTokensCache::TokenCacheRecord* a,
              SSLTokensCache::TokenCacheRecord* b) const {
    return a->mExpirationTime == b->mExpirationTime;
  }
  bool LessThan(SSLTokensCache::TokenCacheRecord* a,
                SSLTokensCache::TokenCacheRecord* b) const {
    return a->mExpirationTime < b->mExpirationTime;
  }
};

StaticRefPtr<SSLTokensCache> SSLTokensCache::gInstance;
StaticMutex SSLTokensCache::sLock;

uint32_t SSLTokensCache::TokenCacheRecord::Size() const {
  uint32_t size = mToken.Length() + sizeof(mSessionCacheInfo.mEVStatus) +
                  sizeof(mSessionCacheInfo.mCertificateTransparencyStatus) +
                  mSessionCacheInfo.mServerCertBytes.Length();
  if (mSessionCacheInfo.mSucceededCertChainBytes) {
    for (const auto& cert : mSessionCacheInfo.mSucceededCertChainBytes.ref()) {
      size += cert.Length();
    }
  }
  return size;
}

void SSLTokensCache::TokenCacheRecord::Reset() {
  mToken.Clear();
  mExpirationTime = 0;
  mSessionCacheInfo.mEVStatus = psm::EVStatus::NotEV;
  mSessionCacheInfo.mCertificateTransparencyStatus =
      nsITransportSecurityInfo::CERTIFICATE_TRANSPARENCY_NOT_APPLICABLE;
  mSessionCacheInfo.mServerCertBytes.Clear();
  mSessionCacheInfo.mSucceededCertChainBytes.reset();
}

NS_IMPL_ISUPPORTS(SSLTokensCache, nsIMemoryReporter)

// static
nsresult SSLTokensCache::Init() {
  StaticMutexAutoLock lock(sLock);

  if (nsIOService::UseSocketProcess()) {
    if (!XRE_IsSocketProcess()) {
      return NS_OK;
    }
  } else if (!XRE_IsParentProcess()) {
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
nsresult SSLTokensCache::Put(const nsACString& aKey, const uint8_t* aToken,
                             uint32_t aTokenLen,
                             nsITransportSecurityInfo* aSecInfo) {
  StaticMutexAutoLock lock(sLock);

  LOG(("SSLTokensCache::Put [key=%s, tokenLen=%u]",
       PromiseFlatCString(aKey).get(), aTokenLen));

  if (!gInstance) {
    LOG(("  service not initialized"));
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (!aSecInfo) {
    return NS_ERROR_FAILURE;
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

  nsCOMPtr<nsIX509Cert> cert;
  aSecInfo->GetServerCert(getter_AddRefs(cert));
  if (!cert) {
    return NS_ERROR_FAILURE;
  }

  nsTArray<uint8_t> certBytes;
  nsresult rv = cert->GetRawDER(certBytes);
  if (NS_FAILED(rv)) {
    return rv;
  }

  Maybe<nsTArray<nsTArray<uint8_t>>> succeededCertChainBytes;
  nsTArray<RefPtr<nsIX509Cert>> succeededCertArray;
  rv = aSecInfo->GetSucceededCertChain(succeededCertArray);
  if (NS_FAILED(rv)) {
    return rv;
  }

  Maybe<bool> isBuiltCertChainRootBuiltInRoot;
  if (!succeededCertArray.IsEmpty()) {
    succeededCertChainBytes.emplace();
    for (const auto& cert : succeededCertArray) {
      nsTArray<uint8_t> rawCert;
      nsresult rv = cert->GetRawDER(rawCert);
      if (NS_FAILED(rv)) {
        return rv;
      }
      succeededCertChainBytes->AppendElement(std::move(rawCert));
    }

    bool builtInRoot = false;
    rv = aSecInfo->GetIsBuiltCertChainRootBuiltInRoot(&builtInRoot);
    if (NS_FAILED(rv)) {
      return rv;
    }
    isBuiltCertChainRootBuiltInRoot.emplace(builtInRoot);
  }

  bool isEV;
  rv = aSecInfo->GetIsExtendedValidation(&isEV);
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint16_t certificateTransparencyStatus;
  rv = aSecInfo->GetCertificateTransparencyStatus(
      &certificateTransparencyStatus);
  if (NS_FAILED(rv)) {
    return rv;
  }

  TokenCacheRecord* rec = nullptr;

  if (!gInstance->mTokenCacheRecords.Get(aKey, &rec)) {
    rec = new TokenCacheRecord();
    rec->mKey = aKey;
    gInstance->mTokenCacheRecords.Put(aKey, rec);
    gInstance->mExpirationArray.AppendElement(rec);
  } else {
    gInstance->mCacheSize -= rec->Size();
    rec->Reset();
  }

  rec->mExpirationTime = expirationTime;
  MOZ_ASSERT(rec->mToken.IsEmpty());
  rec->mToken.AppendElements(aToken, aTokenLen);

  rec->mSessionCacheInfo.mServerCertBytes = std::move(certBytes);

  rec->mSessionCacheInfo.mSucceededCertChainBytes =
      std::move(succeededCertChainBytes);

  if (isEV) {
    rec->mSessionCacheInfo.mEVStatus = psm::EVStatus::EV;
  }

  rec->mSessionCacheInfo.mCertificateTransparencyStatus =
      certificateTransparencyStatus;

  rec->mSessionCacheInfo.mIsBuiltCertChainRootBuiltInRoot =
      std::move(isBuiltCertChainRootBuiltInRoot);

  gInstance->mCacheSize += rec->Size();

  gInstance->LogStats();

  gInstance->EvictIfNecessary();

  return NS_OK;
}

// static
nsresult SSLTokensCache::Get(const nsACString& aKey,
                             nsTArray<uint8_t>& aToken) {
  StaticMutexAutoLock lock(sLock);

  LOG(("SSLTokensCache::Get [key=%s]", PromiseFlatCString(aKey).get()));

  if (!gInstance) {
    LOG(("  service not initialized"));
    return NS_ERROR_NOT_INITIALIZED;
  }

  TokenCacheRecord* rec = nullptr;

  if (gInstance->mTokenCacheRecords.Get(aKey, &rec)) {
    if (rec->mToken.Length()) {
      aToken = rec->mToken;
      return NS_OK;
    }
  }

  LOG(("  token not found"));
  return NS_ERROR_NOT_AVAILABLE;
}

// static
bool SSLTokensCache::GetSessionCacheInfo(const nsACString& aKey,
                                         SessionCacheInfo& aResult) {
  StaticMutexAutoLock lock(sLock);

  LOG(("SSLTokensCache::GetSessionCacheInfo [key=%s]",
       PromiseFlatCString(aKey).get()));

  if (!gInstance) {
    LOG(("  service not initialized"));
    return false;
  }

  TokenCacheRecord* rec = nullptr;

  if (gInstance->mTokenCacheRecords.Get(aKey, &rec)) {
    aResult = rec->mSessionCacheInfo;
    return true;
  }

  LOG(("  token not found"));
  return false;
}

// static
nsresult SSLTokensCache::Remove(const nsACString& aKey) {
  StaticMutexAutoLock lock(sLock);

  LOG(("SSLTokensCache::Remove [key=%s]", PromiseFlatCString(aKey).get()));

  if (!gInstance) {
    LOG(("  service not initialized"));
    return NS_ERROR_NOT_INITIALIZED;
  }

  return gInstance->RemoveLocked(aKey);
}

nsresult SSLTokensCache::RemoveLocked(const nsACString& aKey) {
  sLock.AssertCurrentThreadOwns();

  LOG(("SSLTokensCache::RemoveLocked [key=%s]",
       PromiseFlatCString(aKey).get()));

  UniquePtr<TokenCacheRecord> rec;

  if (!mTokenCacheRecords.Remove(aKey, &rec)) {
    LOG(("  token not found"));
    return NS_ERROR_NOT_AVAILABLE;
  }

  mCacheSize -= rec->Size();

  if (!mExpirationArray.RemoveElement(rec.get())) {
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
    if (NS_FAILED(RemoveLocked(mExpirationArray[0]->mKey))) {
      MOZ_ASSERT(false,
                 "mExpirationArray and mTokenCacheRecords are out of sync!");
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

  n += mTokenCacheRecords.ShallowSizeOfExcludingThis(mallocSizeOf);
  n += mExpirationArray.ShallowSizeOfExcludingThis(mallocSizeOf);

  for (uint32_t i = 0; i < mExpirationArray.Length(); ++i) {
    n += mallocSizeOf(mExpirationArray[i]);
    n += mExpirationArray[i]->mKey.SizeOfExcludingThisIfUnshared(mallocSizeOf);
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

// static
void SSLTokensCache::Clear() {
  LOG(("SSLTokensCache::Clear"));
  if (!sEnabled) {
    return;
  }

  StaticMutexAutoLock lock(sLock);
  if (!gInstance) {
    LOG(("  service not initialized"));
    return;
  }

  gInstance->mExpirationArray.Clear();
  gInstance->mTokenCacheRecords.Clear();
  gInstance->mCacheSize = 0;
}

}  // namespace net
}  // namespace mozilla
