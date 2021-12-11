/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SSLTokensCache_h_
#define SSLTokensCache_h_

#include "nsIMemoryReporter.h"
#include "nsClassHashtable.h"
#include "nsTArray.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StaticPtr.h"
#include "nsXULAppAPI.h"
#include "TransportSecurityInfo.h"
#include "CertVerifier.h"  // For EVStatus

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
};

class SSLTokensCache : public nsIMemoryReporter {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  friend class ExpirationComparator;

  static nsresult Init();
  static nsresult Shutdown();

  static nsresult Put(const nsACString& aKey, const uint8_t* aToken,
                      uint32_t aTokenLen, nsITransportSecurityInfo* aSecInfo);
  static nsresult Put(const nsACString& aKey, const uint8_t* aToken,
                      uint32_t aTokenLen, nsITransportSecurityInfo* aSecInfo,
                      PRUint32 aExpirationTime);
  static nsresult Get(const nsACString& aKey, nsTArray<uint8_t>& aToken);
  static bool GetSessionCacheInfo(const nsACString& aKey,
                                  SessionCacheInfo& aResult);
  static nsresult Remove(const nsACString& aKey);
  static void Clear();

 private:
  SSLTokensCache();
  virtual ~SSLTokensCache();

  nsresult RemoveLocked(const nsACString& aKey);

  void EvictIfNecessary();
  void LogStats();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  static mozilla::StaticRefPtr<SSLTokensCache> gInstance;
  static StaticMutex sLock;

  uint32_t mCacheSize{0};  // Actual cache size in bytes

  class TokenCacheRecord {
   public:
    uint32_t Size() const;
    void Reset();

    nsCString mKey;
    PRUint32 mExpirationTime = 0;
    nsTArray<uint8_t> mToken;
    SessionCacheInfo mSessionCacheInfo;
  };

  nsClassHashtable<nsCStringHashKey, TokenCacheRecord> mTokenCacheRecords;
  nsTArray<TokenCacheRecord*> mExpirationArray;
};

}  // namespace net
}  // namespace mozilla

#endif  // SSLTokensCache_h_
