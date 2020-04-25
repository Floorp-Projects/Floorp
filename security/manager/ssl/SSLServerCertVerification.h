/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _SSLSERVERCERTVERIFICATION_H
#define _SSLSERVERCERTVERIFICATION_H

#include "CertVerifier.h"
#include "ScopedNSSTypes.h"
#include "mozilla/Maybe.h"
#include "mozpkix/pkix.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "prerror.h"
#include "prio.h"
#include "seccomon.h"
#include "secoidt.h"

class nsNSSCertificate;

using namespace mozilla::pkix;

namespace mozilla {
namespace psm {

class TransportSecurityInfo;
enum class EVStatus : uint8_t;

SECStatus AuthCertificateHook(void* arg, PRFileDesc* fd, PRBool checkSig,
                              PRBool isServer);

// This function triggers the certificate verification. The verification is
// asynchronous and the info object will be notified when the verification has
// completed via SetCertVerificationResult.
SECStatus AuthCertificateHookWithInfo(
    TransportSecurityInfo* infoObject, const void* aPtrForLogging,
    nsTArray<nsTArray<uint8_t>>&& peerCertChain,
    Maybe<nsTArray<nsTArray<uint8_t>>>& stapledOCSPResponses,
    Maybe<nsTArray<uint8_t>>& sctsFromTLSExtension, uint32_t providerFlags);

// Base class for dispatching the certificate verification result.
class BaseSSLServerCertVerificationResult {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual void Dispatch(nsNSSCertificate* aCert,
                        nsTArray<nsTArray<uint8_t>>&& aBuiltChain,
                        nsTArray<nsTArray<uint8_t>>&& aPeerCertChain,
                        uint16_t aCertificateTransparencyStatus,
                        EVStatus aEVStatus, bool aSucceeded,
                        PRErrorCode aFinalError, uint32_t aCollectedErrors,
                        bool aIsBuiltCertChainRootBuiltInRoot) = 0;
};

// Dispatched to the STS thread to notify the infoObject of the verification
// result.
//
// This will cause the PR_Poll in the STS thread to return, so things work
// correctly even if the STS thread is blocked polling (only) on the file
// descriptor that is waiting for this result.
class SSLServerCertVerificationResult final
    : public BaseSSLServerCertVerificationResult,
      public Runnable {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE

  explicit SSLServerCertVerificationResult(TransportSecurityInfo* infoObject);

  void Dispatch(nsNSSCertificate* aCert,
                nsTArray<nsTArray<uint8_t>>&& aBuiltChain,
                nsTArray<nsTArray<uint8_t>>&& aPeerCertChain,
                uint16_t aCertificateTransparencyStatus, EVStatus aEVStatus,
                bool aSucceeded, PRErrorCode aFinalError,
                uint32_t aCollectedErrors,
                bool aIsBuiltCertChainRootBuiltInRoot) override;

 private:
  ~SSLServerCertVerificationResult() = default;

  const RefPtr<TransportSecurityInfo> mInfoObject;
  RefPtr<nsNSSCertificate> mCert;
  nsTArray<nsTArray<uint8_t>> mBuiltChain;
  nsTArray<nsTArray<uint8_t>> mPeerCertChain;
  uint16_t mCertificateTransparencyStatus;
  EVStatus mEVStatus;
  bool mSucceeded;
  PRErrorCode mFinalError;
  uint32_t mCollectedErrors;
  bool mIsBuiltCertChainRootBuiltInRoot;
};

class SSLServerCertVerificationJob : public Runnable {
 public:
  // Must be called only on the socket transport thread
  static SECStatus Dispatch(uint64_t addrForLogging, void* aPinArg,
                            const UniqueCERTCertificate& serverCert,
                            nsTArray<nsTArray<uint8_t>>&& peerCertChain,
                            const nsACString& aHostName, int32_t aPort,
                            const OriginAttributes& aOriginAttributes,
                            Maybe<nsTArray<uint8_t>>& stapledOCSPResponse,
                            Maybe<nsTArray<uint8_t>>& sctsFromTLSExtension,
                            Maybe<DelegatedCredentialInfo>& dcInfo,
                            uint32_t providerFlags, Time time, PRTime prtime,
                            uint32_t certVerifierFlags,
                            BaseSSLServerCertVerificationResult* aResultTask);

 private:
  NS_DECL_NSIRUNNABLE

  // Must be called only on the socket transport thread
  SSLServerCertVerificationJob(uint64_t addrForLogging, void* aPinArg,
                               const UniqueCERTCertificate& cert,
                               nsTArray<nsTArray<uint8_t>>&& peerCertChain,
                               const nsACString& aHostName, int32_t aPort,
                               const OriginAttributes& aOriginAttributes,
                               Maybe<nsTArray<uint8_t>>& stapledOCSPResponse,
                               Maybe<nsTArray<uint8_t>>& sctsFromTLSExtension,
                               Maybe<DelegatedCredentialInfo>& dcInfo,
                               uint32_t providerFlags, Time time, PRTime prtime,
                               uint32_t certVerifierFlags,
                               BaseSSLServerCertVerificationResult* aResultTask)
      : Runnable("psm::SSLServerCertVerificationJob"),
        mAddrForLogging(addrForLogging),
        mPinArg(aPinArg),
        mCert(CERT_DupCertificate(cert.get())),
        mPeerCertChain(std::move(peerCertChain)),
        mHostName(aHostName),
        mPort(aPort),
        mOriginAttributes(aOriginAttributes),
        mProviderFlags(providerFlags),
        mCertVerifierFlags(certVerifierFlags),
        mTime(time),
        mPRTime(prtime),
        mStapledOCSPResponse(std::move(stapledOCSPResponse)),
        mSCTsFromTLSExtension(std::move(sctsFromTLSExtension)),
        mDCInfo(std::move(dcInfo)),
        mResultTask(aResultTask) {}

  uint64_t mAddrForLogging;
  void* mPinArg;
  const UniqueCERTCertificate mCert;
  nsTArray<nsTArray<uint8_t>> mPeerCertChain;
  nsCString mHostName;
  int32_t mPort;
  OriginAttributes mOriginAttributes;
  const uint32_t mProviderFlags;
  const uint32_t mCertVerifierFlags;
  const Time mTime;
  const PRTime mPRTime;
  Maybe<nsTArray<uint8_t>> mStapledOCSPResponse;
  Maybe<nsTArray<uint8_t>> mSCTsFromTLSExtension;
  Maybe<DelegatedCredentialInfo> mDCInfo;
  RefPtr<BaseSSLServerCertVerificationResult> mResultTask;
};

}  // namespace psm
}  // namespace mozilla

#endif
