/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CommonSocketControl_h
#define CommonSocketControl_h

#include "CertVerifier.h"
#include "TransportSecurityInfo.h"
#include "mozilla/Maybe.h"
#include "mozilla/net/SSLTokensCache.h"
#include "nsIInterfaceRequestor.h"
#include "nsITLSSocketControl.h"
#include "nsSocketTransportService2.h"

#if defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
#  include "prthread.h"
#  define COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD() \
    MOZ_DIAGNOSTIC_ASSERT(mOwningThread == PR_GetCurrentThread())
#else
#  define COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD() \
    do {                                                  \
    } while (false)
#endif

// CommonSocketControl is the base class that implements nsITLSSocketControl.
// Various concrete TLS socket control implementations inherit from this class.
// Currently these implementations consist of NSSSocketControl (a socket
// control for NSS) and QuicSocketControl (a socket control for quic).
// NB: these classes must only be used on the socket thread (the one exception
// being tests that incidentally use CommonSocketControl on the main thread
// (and only the main thread)). This is enforced via the macro
// COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD() that should be called at the
// beginning of every function in this class and all subclasses.
class CommonSocketControl : public nsITLSSocketControl {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITLSSOCKETCONTROL

  CommonSocketControl(const nsCString& aHostName, int32_t aPort,
                      uint32_t aProviderFlags);

  // Use "errorCode" 0 to indicate success.
  virtual void SetCertVerificationResult(PRErrorCode errorCode) {
    MOZ_ASSERT_UNREACHABLE("Subclasses must override this.");
  }

  const nsACString& GetHostName() {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    return mHostName;
  }
  int32_t GetPort() {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    return mPort;
  }
  void SetMadeOCSPRequests(bool aMadeOCSPRequests) {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    mMadeOCSPRequests = aMadeOCSPRequests;
  }
  bool GetMadeOCSPRequests() {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    return mMadeOCSPRequests;
  }
  void SetUsedPrivateDNS(bool aUsedPrivateDNS) {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    mUsedPrivateDNS = aUsedPrivateDNS;
  }
  bool GetUsedPrivateDNS() {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    return mUsedPrivateDNS;
  }

  void SetServerCert(const nsCOMPtr<nsIX509Cert>& aServerCert,
                     mozilla::psm::EVStatus aEVStatus);
  already_AddRefed<nsIX509Cert> GetServerCert() {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    return do_AddRef(mServerCert);
  }
  bool HasServerCert() {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    return mServerCert != nullptr;
  }
  void SetStatusErrorBits(const nsCOMPtr<nsIX509Cert>& cert,
                          nsITransportSecurityInfo::OverridableErrorCategory
                              overridableErrorCategory);
  bool HasUserOverriddenCertificateError() {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    return mOverridableErrorCategory.isSome() &&
           *mOverridableErrorCategory !=
               nsITransportSecurityInfo::OverridableErrorCategory::ERROR_UNSET;
  }
  void SetSucceededCertChain(nsTArray<nsTArray<uint8_t>>&& certList);
  void SetFailedCertChain(nsTArray<nsTArray<uint8_t>>&& certList);
  void SetIsBuiltCertChainRootBuiltInRoot(
      bool aIsBuiltCertChainRootBuiltInRoot) {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    mIsBuiltCertChainRootBuiltInRoot = aIsBuiltCertChainRootBuiltInRoot;
  }
  void SetCertificateTransparencyStatus(
      uint16_t aCertificateTransparencyStatus) {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    mCertificateTransparencyStatus = aCertificateTransparencyStatus;
  }
  void SetOriginAttributes(const mozilla::OriginAttributes& aOriginAttributes) {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    mOriginAttributes = aOriginAttributes;
  }
  mozilla::OriginAttributes& GetOriginAttributes() {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    return mOriginAttributes;
  }

  void SetSecurityState(uint32_t aSecurityState) {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    mSecurityState = aSecurityState;
  }
  void SetResumed(bool aResumed) {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    mResumed = aResumed;
  }

  uint32_t GetProviderFlags() const {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    return mProviderFlags;
  }
  void SetSSLVersionUsed(uint16_t version) {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    mSSLVersionUsed = version;
  }
  void SetSessionCacheInfo(mozilla::net::SessionCacheInfo&& aInfo) {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    mSessionCacheInfo.reset();
    mSessionCacheInfo.emplace(std::move(aInfo));
  }
  void RebuildCertificateInfoFromSSLTokenCache();
  void SetCanceled(PRErrorCode errorCode);
  bool IsCanceled() {
    COMMON_SOCKET_CONTROL_ASSERT_ON_OWNING_THREAD();
    return mCanceled;
  }
  int32_t GetErrorCode();

 protected:
  virtual ~CommonSocketControl() = default;

  nsCString mHostName;
  int32_t mPort;
  mozilla::OriginAttributes mOriginAttributes;

  bool mCanceled;
  mozilla::Maybe<mozilla::net::SessionCacheInfo> mSessionCacheInfo;
  bool mHandshakeCompleted;
  bool mJoined;
  bool mSentClientCert;
  bool mFailedVerification;
  uint16_t mSSLVersionUsed;
  uint32_t mProviderFlags;

  // Fields used to build a TransportSecurityInfo
  uint32_t mSecurityState;
  PRErrorCode mErrorCode;
  // Peer cert chain for failed connections.
  nsTArray<RefPtr<nsIX509Cert>> mFailedCertChain;
  nsCOMPtr<nsIX509Cert> mServerCert;
  nsTArray<RefPtr<nsIX509Cert>> mSucceededCertChain;
  mozilla::Maybe<uint16_t> mCipherSuite;
  mozilla::Maybe<nsCString> mKeaGroupName;
  mozilla::Maybe<nsCString> mSignatureSchemeName;
  mozilla::Maybe<uint16_t> mProtocolVersion;
  uint16_t mCertificateTransparencyStatus;
  mozilla::Maybe<bool> mIsAcceptedEch;
  mozilla::Maybe<bool> mIsDelegatedCredential;
  mozilla::Maybe<nsITransportSecurityInfo::OverridableErrorCategory>
      mOverridableErrorCategory;
  bool mMadeOCSPRequests;
  bool mUsedPrivateDNS;
  mozilla::Maybe<bool> mIsEV;
  bool mNPNCompleted;
  nsCString mNegotiatedNPN;
  bool mResumed;
  bool mIsBuiltCertChainRootBuiltInRoot;
  nsCString mPeerId;

#if defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
  const PRThread* mOwningThread;
#endif
};

#endif  // CommonSocketControl_h
