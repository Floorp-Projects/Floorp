/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SSLTokensCache_h_
#define SSLTokensCache_h_

#include "CertVerifier.h"  // For EVStatus
#include "mozilla/Maybe.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StaticPtr.h"
#include "nsClassHashtable.h"
#include "nsIMemoryReporter.h"
#include "nsITransportSecurityInfo.h"
#include "nsTArray.h"
#include "nsTHashMap.h"
#include "nsXULAppAPI.h"

class CommonSocketControl;

namespace mozilla {
namespace net {

struct SessionCacheInfo {
  SessionCacheInfo Clone() const;

  psm::EVStatus mEVStatus = psm::EVStatus::NotEV;
  uint16_t mCertificateTransparencyStatus =
      nsITransportSecurityInfo::CERTIFICATE_TRANSPARENCY_NOT_APPLICABLE;
  nsTArray<uint8_t> mServerCertBytes;
  Maybe<nsTArray<nsTArray<uint8_t>>> mSucceededCertChainBytes;
  Maybe<bool> mIsBuiltCertChainRootBuiltInRoot;
  nsITransportSecurityInfo::OverridableErrorCategory mOverridableErrorCategory;
  Maybe<nsTArray<nsTArray<uint8_t>>> mFailedCertChainBytes;
};

class SSLTokensCache : public nsIMemoryReporter {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  friend class ExpirationComparator;

  static nsresult Init();
  static nsresult Shutdown();

  static nsresult Put(const nsACString& aKey, const uint8_t* aToken,
                      uint32_t aTokenLen, CommonSocketControl* aSocketControl);
  static nsresult Put(const nsACString& aKey, const uint8_t* aToken,
                      uint32_t aTokenLen, CommonSocketControl* aSocketControl,
                      PRUint32 aExpirationTime);
  static nsresult Get(const nsACString& aKey, nsTArray<uint8_t>& aToken,
                      SessionCacheInfo& aResult, uint64_t* aTokenId = nullptr);
  static nsresult Remove(const nsACString& aKey, uint64_t aId);
  static nsresult RemoveAll(const nsACString& aKey);
  static void Clear();

 private:
  SSLTokensCache();
  virtual ~SSLTokensCache();

  nsresult RemoveLocked(const nsACString& aKey, uint64_t aId);
  nsresult RemovAllLocked(const nsACString& aKey);
  nsresult GetLocked(const nsACString& aKey, nsTArray<uint8_t>& aToken,
                     SessionCacheInfo& aResult, uint64_t* aTokenId);

  void EvictIfNecessary();
  void LogStats();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  static mozilla::StaticRefPtr<SSLTokensCache> gInstance;
  static StaticMutex sLock MOZ_UNANNOTATED;
  static uint64_t sRecordId;

  uint32_t mCacheSize{0};  // Actual cache size in bytes

  class TokenCacheRecord {
   public:
    ~TokenCacheRecord();

    uint32_t Size() const;
    void Reset();

    nsCString mKey;
    PRUint32 mExpirationTime = 0;
    nsTArray<uint8_t> mToken;
    SessionCacheInfo mSessionCacheInfo;
    // An unique id to identify the record. Mostly used when we want to remove a
    // record from TokenCacheEntry.
    uint64_t mId = 0;
  };

  class TokenCacheEntry {
   public:
    uint32_t Size() const;
    // Add a record into |mRecords|. To make sure |mRecords| is sorted, we
    // iterate |mRecords| everytime to find a right place to insert the new
    // record.
    void AddRecord(UniquePtr<TokenCacheRecord>&& aRecord,
                   nsTArray<TokenCacheRecord*>& aExpirationArray);
    // This function returns the first record in |mRecords|.
    const UniquePtr<TokenCacheRecord>& Get();
    UniquePtr<TokenCacheRecord> RemoveWithId(uint64_t aId);
    uint32_t RecordCount() const { return mRecords.Length(); }
    const nsTArray<UniquePtr<TokenCacheRecord>>& Records() { return mRecords; }

   private:
    // The records in this array are ordered by the expiration time.
    nsTArray<UniquePtr<TokenCacheRecord>> mRecords;
  };

  void OnRecordDestroyed(TokenCacheRecord* aRec);

  nsClassHashtable<nsCStringHashKey, TokenCacheEntry> mTokenCacheRecords;
  nsTArray<TokenCacheRecord*> mExpirationArray;
};

}  // namespace net
}  // namespace mozilla

#endif  // SSLTokensCache_h_
