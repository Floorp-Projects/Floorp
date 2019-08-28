/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TransportSecurityInfo_h
#define TransportSecurityInfo_h

#include "CertVerifier.h"  // For CertificateTransparencyInfo
#include "ScopedNSSTypes.h"
#include "certt.h"
#include "mozilla/Assertions.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "nsDataHashtable.h"
#include "nsIClassInfo.h"
#include "nsIInterfaceRequestor.h"
#include "nsITransportSecurityInfo.h"
#include "nsNSSCertificate.h"
#include "nsString.h"
#include "mozpkix/pkixtypes.h"

namespace mozilla {
namespace psm {

enum class EVStatus {
  NotEV = 0,
  EV = 1,
};

class TransportSecurityInfo : public nsITransportSecurityInfo,
                              public nsIInterfaceRequestor,
                              public nsISerializable,
                              public nsIClassInfo {
 protected:
  virtual ~TransportSecurityInfo() {}

 public:
  TransportSecurityInfo();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITRANSPORTSECURITYINFO
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO

  void SetSecurityState(uint32_t aState);

  inline int32_t GetErrorCode() {
    int32_t result;
    mozilla::DebugOnly<nsresult> rv = GetErrorCode(&result);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    return result;
  }

  const nsACString& GetHostName() const { return mHostName; }

  void SetHostName(const char* host);

  int32_t GetPort() const { return mPort; }
  void SetPort(int32_t aPort);

  const OriginAttributes& GetOriginAttributes() const {
    return mOriginAttributes;
  }
  void SetOriginAttributes(const OriginAttributes& aOriginAttributes);

  void SetCanceled(PRErrorCode errorCode);
  bool IsCanceled();

  void SetStatusErrorBits(nsNSSCertificate* cert, uint32_t collected_errors);

  nsresult SetFailedCertChain(UniqueCERTCertList certList);

  void SetServerCert(nsNSSCertificate* aServerCert, EVStatus aEVStatus);

  nsresult SetSucceededCertChain(mozilla::UniqueCERTCertList certList);

  bool HasServerCert() { return mServerCert != nullptr; }

  void SetCertificateTransparencyInfo(
      const mozilla::psm::CertificateTransparencyInfo& info);

  uint16_t mCipherSuite;
  uint16_t mProtocolVersion;
  uint16_t mCertificateTransparencyStatus;
  nsCString mKeaGroup;
  nsCString mSignatureSchemeName;

  bool mIsDomainMismatch;
  bool mIsNotValidAtThisTime;
  bool mIsUntrusted;
  bool mIsEV;

  bool mHasIsEVStatus;
  bool mHaveCipherSuiteAndProtocol;

  /* mHaveCertErrrorBits is relied on to determine whether or not a SPDY
     connection is eligible for joining in nsNSSSocketInfo::JoinConnection() */
  bool mHaveCertErrorBits;

 private:
  // True if SetCanceled has been called (or if this was deserialized with a
  // non-zero mErrorCode, which can only be the case if SetCanceled was called
  // on the original TransportSecurityInfo).
  Atomic<bool> mCanceled;

  mutable ::mozilla::Mutex mMutex;

 protected:
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;

 private:
  uint32_t mSecurityState;

  PRErrorCode mErrorCode;

  int32_t mPort;
  nsCString mHostName;
  OriginAttributes mOriginAttributes;

  nsCOMPtr<nsIX509Cert> mServerCert;
  nsCOMPtr<nsIX509CertList> mSucceededCertChain;

  /* Peer cert chain for failed connections (for error reporting) */
  nsCOMPtr<nsIX509CertList> mFailedCertChain;

  nsresult ReadSSLStatus(nsIObjectInputStream* aStream);
};

class RememberCertErrorsTable {
 private:
  RememberCertErrorsTable();

  struct CertStateBits {
    bool mIsDomainMismatch;
    bool mIsNotValidAtThisTime;
    bool mIsUntrusted;
  };
  nsDataHashtable<nsCStringHashKey, CertStateBits> mErrorHosts;

 public:
  void RememberCertHasError(TransportSecurityInfo* infoObject,
                            SECStatus certVerificationResult);
  void LookupCertErrorBits(TransportSecurityInfo* infoObject);

  static void Init() { sInstance = new RememberCertErrorsTable(); }

  static RememberCertErrorsTable& GetInstance() {
    MOZ_ASSERT(sInstance);
    return *sInstance;
  }

  static void Cleanup() {
    delete sInstance;
    sInstance = nullptr;
  }

 private:
  Mutex mMutex;

  static RememberCertErrorsTable* sInstance;
};

}  // namespace psm
}  // namespace mozilla

// 16786594-0296-4471-8096-8f84497ca428
#define TRANSPORTSECURITYINFO_CID                    \
  {                                                  \
    0x16786594, 0x0296, 0x4471, {                    \
      0x80, 0x96, 0x8f, 0x84, 0x49, 0x7c, 0xa4, 0x28 \
    }                                                \
  }

#endif  // TransportSecurityInfo_h
