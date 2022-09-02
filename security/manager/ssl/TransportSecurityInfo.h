/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TransportSecurityInfo_h
#define TransportSecurityInfo_h

#include "CertVerifier.h"  // For CertificateTransparencyInfo, EVStatus
#include "ScopedNSSTypes.h"
#include "certt.h"
#include "mozilla/Assertions.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ipc/TransportSecurityInfoUtils.h"
#include "mozpkix/pkixtypes.h"
#include "nsTHashMap.h"
#include "nsIClassInfo.h"
#include "nsIObjectInputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsITransportSecurityInfo.h"
#include "nsNSSCertificate.h"
#include "nsString.h"

namespace mozilla {
namespace psm {

class TransportSecurityInfo : public nsITransportSecurityInfo,
                              public nsIInterfaceRequestor,
                              public nsISerializable,
                              public nsIClassInfo {
 protected:
  virtual ~TransportSecurityInfo() = default;

 public:
  TransportSecurityInfo();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITRANSPORTSECURITYINFO
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO

  void SetPreliminaryHandshakeInfo(const SSLChannelInfo& channelInfo,
                                   const SSLCipherSuiteInfo& cipherInfo);

  void SetSecurityState(uint32_t aState);

  inline int32_t GetErrorCode() {
    int32_t result;
    mozilla::DebugOnly<nsresult> rv = GetErrorCode(&result);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    return result;
  }

  const nsACString& GetHostName() const {
    MutexAutoLock lock(mMutex);
    return mHostName;
  }

  void SetHostName(const char* host);

  int32_t GetPort() const { return mPort; }
  void SetPort(int32_t aPort);

  const OriginAttributes& GetOriginAttributes() const {
    MutexAutoLock lock(mMutex);
    return mOriginAttributes;
  }
  const OriginAttributes& GetOriginAttributes(
      MutexAutoLock& aProofOfLock) const {
    return mOriginAttributes;
  }
  void SetOriginAttributes(const OriginAttributes& aOriginAttributes);

  void SetCanceled(PRErrorCode errorCode);
  bool IsCanceled();

  void SetStatusErrorBits(const nsCOMPtr<nsIX509Cert>& cert,
                          OverridableErrorCategory overridableErrorCategory);

  nsresult SetFailedCertChain(nsTArray<nsTArray<uint8_t>>&& certList);

  void SetServerCert(const nsCOMPtr<nsIX509Cert>& aServerCert,
                     EVStatus aEVStatus);

  nsresult SetSucceededCertChain(nsTArray<nsTArray<uint8_t>>&& certList);

  bool HasServerCert() {
    MutexAutoLock lock(mMutex);
    return mServerCert != nullptr;
  }

  static uint16_t ConvertCertificateTransparencyInfoToStatus(
      const mozilla::psm::CertificateTransparencyInfo& info);

  // Use errorCode == 0 to indicate success;
  virtual void SetCertVerificationResult(PRErrorCode errorCode){};

  void SetCertificateTransparencyStatus(
      uint16_t aCertificateTransparencyStatus) {
    MutexAutoLock lock(mMutex);
    mCertificateTransparencyStatus = aCertificateTransparencyStatus;
  }

  void SetMadeOCSPRequest(bool aMadeOCSPRequests) {
    MutexAutoLock lock(mMutex);
    mMadeOCSPRequests = aMadeOCSPRequests;
  }

  void SetUsedPrivateDNS(bool aUsedPrivateDNS) {
    MutexAutoLock lock(mMutex);
    mUsedPrivateDNS = aUsedPrivateDNS;
  }

  void SetResumed(bool aResumed);

  Atomic<OverridableErrorCategory> mOverridableErrorCategory;
  Atomic<bool> mIsEV;
  Atomic<bool> mHasIsEVStatus;

  Atomic<bool> mHaveCipherSuiteAndProtocol;

  /* mHaveCertErrrorBits is relied on to determine whether or not a SPDY
     connection is eligible for joining in nsNSSSocketInfo::JoinConnection() */
  Atomic<bool> mHaveCertErrorBits;

 private:
  // True if SetCanceled has been called (or if this was deserialized with a
  // non-zero mErrorCode, which can only be the case if SetCanceled was called
  // on the original TransportSecurityInfo).
  Atomic<bool> mCanceled;

 protected:
  mutable ::mozilla::Mutex mMutex;

  uint16_t mCipherSuite MOZ_GUARDED_BY(mMutex);
  uint16_t mProtocolVersion MOZ_GUARDED_BY(mMutex);
  uint16_t mCertificateTransparencyStatus MOZ_GUARDED_BY(mMutex);
  nsCString mKeaGroup MOZ_GUARDED_BY(mMutex);
  nsCString mSignatureSchemeName MOZ_GUARDED_BY(mMutex);

  bool mIsAcceptedEch MOZ_GUARDED_BY(mMutex);
  bool mIsDelegatedCredential MOZ_GUARDED_BY(mMutex);
  bool mMadeOCSPRequests MOZ_GUARDED_BY(mMutex);
  bool mUsedPrivateDNS MOZ_GUARDED_BY(mMutex);

  nsCOMPtr<nsIInterfaceRequestor> mCallbacks MOZ_GUARDED_BY(mMutex);
  nsTArray<RefPtr<nsIX509Cert>> mSucceededCertChain MOZ_GUARDED_BY(mMutex);
  bool mNPNCompleted MOZ_GUARDED_BY(mMutex);
  nsCString mNegotiatedNPN MOZ_GUARDED_BY(mMutex);
  bool mResumed MOZ_GUARDED_BY(mMutex);
  bool mIsBuiltCertChainRootBuiltInRoot MOZ_GUARDED_BY(mMutex);
  nsCString mPeerId MOZ_GUARDED_BY(mMutex);
  nsCString mHostName MOZ_GUARDED_BY(mMutex);
  OriginAttributes mOriginAttributes MOZ_GUARDED_BY(mMutex);

 private:
  static nsresult ReadBoolAndSetAtomicFieldHelper(nsIObjectInputStream* stream,
                                                  Atomic<bool>& atomic) {
    bool tmpBool;
    nsresult rv = stream->ReadBoolean(&tmpBool);
    if (NS_FAILED(rv)) {
      return rv;
    }
    atomic = tmpBool;
    return rv;
  }

  static nsresult ReadUint32AndSetAtomicFieldHelper(
      nsIObjectInputStream* stream, Atomic<uint32_t>& atomic) {
    uint32_t tmpInt;
    nsresult rv = stream->Read32(&tmpInt);
    if (NS_FAILED(rv)) {
      return rv;
    }
    atomic = tmpInt;
    return rv;
  }

  template <typename P>
  static bool ReadParamAtomicHelper(IPC::MessageReader* aReader,
                                    Atomic<P>& atomic) {
    P tmpStore;
    bool result = ReadParam(aReader, &tmpStore);
    if (result == false) {
      return result;
    }
    atomic = tmpStore;
    return result;
  }

  Atomic<uint32_t> mSecurityState;

  Atomic<PRErrorCode> mErrorCode;

  Atomic<int32_t> mPort;

  nsCOMPtr<nsIX509Cert> mServerCert MOZ_GUARDED_BY(mMutex);

  /* Peer cert chain for failed connections (for error reporting) */
  nsTArray<RefPtr<nsIX509Cert>> mFailedCertChain MOZ_GUARDED_BY(mMutex);

  nsresult ReadOldOverridableErrorBits(nsIObjectInputStream* aStream,
                                       MutexAutoLock& aProofOfLock);
  nsresult ReadSSLStatus(nsIObjectInputStream* aStream,
                         MutexAutoLock& aProofOfLock);

  // This function is used to read the binary that are serialized
  // by using nsIX509CertList
  nsresult ReadCertList(nsIObjectInputStream* aStream,
                        nsTArray<RefPtr<nsIX509Cert>>& aCertList,
                        MutexAutoLock& aProofOfLock);
  nsresult ReadCertificatesFromStream(nsIObjectInputStream* aStream,
                                      uint32_t aSize,
                                      nsTArray<RefPtr<nsIX509Cert>>& aCertList,
                                      MutexAutoLock& aProofOfLock);
};

class RememberCertErrorsTable {
 private:
  RememberCertErrorsTable();

  nsTHashMap<nsCStringHashKey,
             nsITransportSecurityInfo::OverridableErrorCategory>
      mErrorHosts MOZ_GUARDED_BY(mMutex);

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
