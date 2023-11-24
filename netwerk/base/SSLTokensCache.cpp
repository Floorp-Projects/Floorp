/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SSLTokensCache.h"

#include "CertVerifier.h"
#include "CommonSocketControl.h"
#include "TransportSecurityInfo.h"
#include "mozilla/ArrayAlgorithm.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "nsIOService.h"
#include "ssl.h"
#include "sslexp.h"

namespace mozilla {
namespace net {

static LazyLogModule gSSLTokensCacheLog("SSLTokensCache");
#undef LOG
#define LOG(args) MOZ_LOG(gSSLTokensCacheLog, mozilla::LogLevel::Debug, args)
#undef LOG5_ENABLED
#define LOG5_ENABLED() \
  MOZ_LOG_TEST(mozilla::net::gSSLTokensCacheLog, mozilla::LogLevel::Verbose)

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

SessionCacheInfo SessionCacheInfo::Clone() const {
  SessionCacheInfo result;
  result.mEVStatus = mEVStatus;
  result.mCertificateTransparencyStatus = mCertificateTransparencyStatus;
  result.mServerCertBytes = mServerCertBytes.Clone();
  result.mSucceededCertChainBytes =
      mSucceededCertChainBytes
          ? Some(TransformIntoNewArray(
                *mSucceededCertChainBytes,
                [](const auto& element) { return element.Clone(); }))
          : Nothing();
  result.mIsBuiltCertChainRootBuiltInRoot = mIsBuiltCertChainRootBuiltInRoot;
  result.mOverridableErrorCategory = mOverridableErrorCategory;
  result.mFailedCertChainBytes =
      mFailedCertChainBytes
          ? Some(TransformIntoNewArray(
                *mFailedCertChainBytes,
                [](const auto& element) { return element.Clone(); }))
          : Nothing();
  return result;
}

StaticRefPtr<SSLTokensCache> SSLTokensCache::gInstance;
StaticMutex SSLTokensCache::sLock;
uint64_t SSLTokensCache::sRecordId = 0;

SSLTokensCache::TokenCacheRecord::~TokenCacheRecord() {
  if (!gInstance) {
    return;
  }

  gInstance->OnRecordDestroyed(this);
}

uint32_t SSLTokensCache::TokenCacheRecord::Size() const {
  uint32_t size = mToken.Length() + sizeof(mSessionCacheInfo.mEVStatus) +
                  sizeof(mSessionCacheInfo.mCertificateTransparencyStatus) +
                  mSessionCacheInfo.mServerCertBytes.Length() +
                  sizeof(mSessionCacheInfo.mIsBuiltCertChainRootBuiltInRoot) +
                  sizeof(mSessionCacheInfo.mOverridableErrorCategory);
  if (mSessionCacheInfo.mSucceededCertChainBytes) {
    for (const auto& cert : mSessionCacheInfo.mSucceededCertChainBytes.ref()) {
      size += cert.Length();
    }
  }
  if (mSessionCacheInfo.mFailedCertChainBytes) {
    for (const auto& cert : mSessionCacheInfo.mFailedCertChainBytes.ref()) {
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
  mSessionCacheInfo.mIsBuiltCertChainRootBuiltInRoot.reset();
  mSessionCacheInfo.mOverridableErrorCategory =
      nsITransportSecurityInfo::OverridableErrorCategory::ERROR_UNSET;
  mSessionCacheInfo.mFailedCertChainBytes.reset();
}

uint32_t SSLTokensCache::TokenCacheEntry::Size() const {
  uint32_t size = 0;
  for (const auto& rec : mRecords) {
    size += rec->Size();
  }
  return size;
}

void SSLTokensCache::TokenCacheEntry::AddRecord(
    UniquePtr<SSLTokensCache::TokenCacheRecord>&& aRecord,
    nsTArray<TokenCacheRecord*>& aExpirationArray) {
  if (mRecords.Length() ==
      StaticPrefs::network_ssl_tokens_cache_records_per_entry()) {
    aExpirationArray.RemoveElement(mRecords[0].get());
    mRecords.RemoveElementAt(0);
  }

  aExpirationArray.AppendElement(aRecord.get());
  for (int32_t i = mRecords.Length() - 1; i >= 0; --i) {
    if (aRecord->mExpirationTime > mRecords[i]->mExpirationTime) {
      mRecords.InsertElementAt(i + 1, std::move(aRecord));
      return;
    }
  }
  mRecords.InsertElementAt(0, std::move(aRecord));
}

UniquePtr<SSLTokensCache::TokenCacheRecord>
SSLTokensCache::TokenCacheEntry::RemoveWithId(uint64_t aId) {
  for (int32_t i = mRecords.Length() - 1; i >= 0; --i) {
    if (mRecords[i]->mId == aId) {
      UniquePtr<TokenCacheRecord> record = std::move(mRecords[i]);
      mRecords.RemoveElementAt(i);
      return record;
    }
  }
  return nullptr;
}

const UniquePtr<SSLTokensCache::TokenCacheRecord>&
SSLTokensCache::TokenCacheEntry::Get() {
  return mRecords[0];
}

NS_IMPL_ISUPPORTS(SSLTokensCache, nsIMemoryReporter)

// static
nsresult SSLTokensCache::Init() {
  StaticMutexAutoLock lock(sLock);

  // SSLTokensCache should be only used in parent process and socket process.
  // Ideally, parent process should not use this when socket process is enabled.
  // However, some xpcsehll tests may need to create and use sockets directly,
  // so we still allow to use this in parent process no matter socket process is
  // enabled or not.
  if (!(XRE_IsSocketProcess() || XRE_IsParentProcess())) {
    return NS_OK;
  }

  MOZ_ASSERT(!gInstance);

  gInstance = new SSLTokensCache();

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

SSLTokensCache::SSLTokensCache() { LOG(("SSLTokensCache::SSLTokensCache")); }

SSLTokensCache::~SSLTokensCache() { LOG(("SSLTokensCache::~SSLTokensCache")); }

// static
nsresult SSLTokensCache::Put(const nsACString& aKey, const uint8_t* aToken,
                             uint32_t aTokenLen,
                             CommonSocketControl* aSocketControl) {
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

  return Put(aKey, aToken, aTokenLen, aSocketControl, expirationTime);
}

// static
nsresult SSLTokensCache::Put(const nsACString& aKey, const uint8_t* aToken,
                             uint32_t aTokenLen,
                             CommonSocketControl* aSocketControl,
                             PRUint32 aExpirationTime) {
  StaticMutexAutoLock lock(sLock);

  LOG(("SSLTokensCache::Put [key=%s, tokenLen=%u]",
       PromiseFlatCString(aKey).get(), aTokenLen));

  if (!gInstance) {
    LOG(("  service not initialized"));
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (!aSocketControl) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsITransportSecurityInfo> securityInfo;
  nsresult rv = aSocketControl->GetSecurityInfo(getter_AddRefs(securityInfo));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIX509Cert> cert;
  securityInfo->GetServerCert(getter_AddRefs(cert));
  if (!cert) {
    return NS_ERROR_FAILURE;
  }

  nsTArray<uint8_t> certBytes;
  rv = cert->GetRawDER(certBytes);
  if (NS_FAILED(rv)) {
    return rv;
  }

  Maybe<nsTArray<nsTArray<uint8_t>>> succeededCertChainBytes;
  nsTArray<RefPtr<nsIX509Cert>> succeededCertArray;
  rv = securityInfo->GetSucceededCertChain(succeededCertArray);
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
    rv = securityInfo->GetIsBuiltCertChainRootBuiltInRoot(&builtInRoot);
    if (NS_FAILED(rv)) {
      return rv;
    }
    isBuiltCertChainRootBuiltInRoot.emplace(builtInRoot);
  }

  bool isEV;
  rv = securityInfo->GetIsExtendedValidation(&isEV);
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint16_t certificateTransparencyStatus;
  rv = securityInfo->GetCertificateTransparencyStatus(
      &certificateTransparencyStatus);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsITransportSecurityInfo::OverridableErrorCategory overridableErrorCategory;
  rv = securityInfo->GetOverridableErrorCategory(&overridableErrorCategory);
  if (NS_FAILED(rv)) {
    return rv;
  }

  Maybe<nsTArray<nsTArray<uint8_t>>> failedCertChainBytes;
  nsTArray<RefPtr<nsIX509Cert>> failedCertArray;
  rv = securityInfo->GetFailedCertChain(failedCertArray);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!failedCertArray.IsEmpty()) {
    failedCertChainBytes.emplace();
    for (const auto& cert : failedCertArray) {
      nsTArray<uint8_t> rawCert;
      nsresult rv = cert->GetRawDER(rawCert);
      if (NS_FAILED(rv)) {
        return rv;
      }
      failedCertChainBytes->AppendElement(std::move(rawCert));
    }
  }

  auto makeUniqueRecord = [&]() {
    auto rec = MakeUnique<TokenCacheRecord>();
    rec->mKey = aKey;
    rec->mExpirationTime = aExpirationTime;
    MOZ_ASSERT(rec->mToken.IsEmpty());
    rec->mToken.AppendElements(aToken, aTokenLen);
    rec->mId = ++sRecordId;
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
    rec->mSessionCacheInfo.mOverridableErrorCategory = overridableErrorCategory;
    rec->mSessionCacheInfo.mFailedCertChainBytes =
        std::move(failedCertChainBytes);
    return rec;
  };

  TokenCacheEntry* const cacheEntry =
      gInstance->mTokenCacheRecords.WithEntryHandle(aKey, [&](auto&& entry) {
        if (!entry) {
          auto rec = makeUniqueRecord();
          auto cacheEntry = MakeUnique<TokenCacheEntry>();
          cacheEntry->AddRecord(std::move(rec), gInstance->mExpirationArray);
          entry.Insert(std::move(cacheEntry));
        } else {
          // To make sure the cache size is synced, we take away the size of
          // whole entry and add it back later.
          gInstance->mCacheSize -= entry.Data()->Size();
          entry.Data()->AddRecord(makeUniqueRecord(),
                                  gInstance->mExpirationArray);
        }

        return entry->get();
      });

  gInstance->mCacheSize += cacheEntry->Size();

  gInstance->LogStats();

  gInstance->EvictIfNecessary();

  return NS_OK;
}

// static
nsresult SSLTokensCache::Get(const nsACString& aKey, nsTArray<uint8_t>& aToken,
                             SessionCacheInfo& aResult, uint64_t* aTokenId) {
  StaticMutexAutoLock lock(sLock);

  LOG(("SSLTokensCache::Get [key=%s]", PromiseFlatCString(aKey).get()));

  if (!gInstance) {
    LOG(("  service not initialized"));
    return NS_ERROR_NOT_INITIALIZED;
  }

  return gInstance->GetLocked(aKey, aToken, aResult, aTokenId);
}

nsresult SSLTokensCache::GetLocked(const nsACString& aKey,
                                   nsTArray<uint8_t>& aToken,
                                   SessionCacheInfo& aResult,
                                   uint64_t* aTokenId) {
  sLock.AssertCurrentThreadOwns();

  TokenCacheEntry* cacheEntry = nullptr;

  if (mTokenCacheRecords.Get(aKey, &cacheEntry)) {
    if (cacheEntry->RecordCount() == 0) {
      MOZ_ASSERT(false, "Found a cacheEntry with no records");
      mTokenCacheRecords.Remove(aKey);
      return NS_ERROR_NOT_AVAILABLE;
    }

    const UniquePtr<TokenCacheRecord>& rec = cacheEntry->Get();
    aToken = rec->mToken.Clone();
    aResult = rec->mSessionCacheInfo.Clone();
    if (aTokenId) {
      *aTokenId = rec->mId;
    }
    mCacheSize -= rec->Size();
    cacheEntry->RemoveWithId(rec->mId);
    if (cacheEntry->RecordCount() == 0) {
      mTokenCacheRecords.Remove(aKey);
    }
    return NS_OK;
  }

  LOG(("  token not found"));
  return NS_ERROR_NOT_AVAILABLE;
}

// static
nsresult SSLTokensCache::Remove(const nsACString& aKey, uint64_t aId) {
  StaticMutexAutoLock lock(sLock);

  LOG(("SSLTokensCache::Remove [key=%s]", PromiseFlatCString(aKey).get()));

  if (!gInstance) {
    LOG(("  service not initialized"));
    return NS_ERROR_NOT_INITIALIZED;
  }

  return gInstance->RemoveLocked(aKey, aId);
}

nsresult SSLTokensCache::RemoveLocked(const nsACString& aKey, uint64_t aId) {
  sLock.AssertCurrentThreadOwns();

  LOG(("SSLTokensCache::RemoveLocked [key=%s, id=%" PRIu64 "]",
       PromiseFlatCString(aKey).get(), aId));

  TokenCacheEntry* cacheEntry;
  if (!mTokenCacheRecords.Get(aKey, &cacheEntry)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  UniquePtr<TokenCacheRecord> rec = cacheEntry->RemoveWithId(aId);
  if (!rec) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mCacheSize -= rec->Size();
  if (cacheEntry->RecordCount() == 0) {
    mTokenCacheRecords.Remove(aKey);
  }

  // Release the record immediately, so mExpirationArray can be also updated.
  rec = nullptr;

  LogStats();

  return NS_OK;
}

// static
nsresult SSLTokensCache::RemoveAll(const nsACString& aKey) {
  StaticMutexAutoLock lock(sLock);

  LOG(("SSLTokensCache::RemoveAll [key=%s]", PromiseFlatCString(aKey).get()));

  if (!gInstance) {
    LOG(("  service not initialized"));
    return NS_ERROR_NOT_INITIALIZED;
  }

  return gInstance->RemovAllLocked(aKey);
}

nsresult SSLTokensCache::RemovAllLocked(const nsACString& aKey) {
  sLock.AssertCurrentThreadOwns();

  LOG(("SSLTokensCache::RemovAllLocked [key=%s]",
       PromiseFlatCString(aKey).get()));

  UniquePtr<TokenCacheEntry> cacheEntry;
  if (!mTokenCacheRecords.Remove(aKey, &cacheEntry)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mCacheSize -= cacheEntry->Size();
  cacheEntry = nullptr;

  LogStats();

  return NS_OK;
}

void SSLTokensCache::OnRecordDestroyed(TokenCacheRecord* aRec) {
  mExpirationArray.RemoveElement(aRec);
}

void SSLTokensCache::EvictIfNecessary() {
  // kilobytes to bytes
  uint32_t capacity = StaticPrefs::network_ssl_tokens_cache_capacity() << 10;
  if (mCacheSize <= capacity) {
    return;
  }

  LOG(("SSLTokensCache::EvictIfNecessary - evicting"));

  mExpirationArray.Sort(ExpirationComparator());

  while (mCacheSize > capacity && mExpirationArray.Length() > 0) {
    DebugOnly<nsresult> rv =
        RemoveLocked(mExpirationArray[0]->mKey, mExpirationArray[0]->mId);
    MOZ_ASSERT(NS_SUCCEEDED(rv),
               "mExpirationArray and mTokenCacheRecords are out of sync!");
  }
}

void SSLTokensCache::LogStats() {
  if (!LOG5_ENABLED()) {
    return;
  }
  LOG(("SSLTokensCache::LogStats [count=%zu, cacheSize=%u]",
       mExpirationArray.Length(), mCacheSize));
  for (const auto& ent : mTokenCacheRecords.Values()) {
    const UniquePtr<TokenCacheRecord>& rec = ent->Get();
    LOG(("key=%s count=%d", rec->mKey.get(), ent->RecordCount()));
  }
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
