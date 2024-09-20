/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _SSLSERVERCERTVERIFICATION_H
#define _SSLSERVERCERTVERIFICATION_H

#include "CertVerifier.h"
#include "CommonSocketControl.h"
#include "ScopedNSSTypes.h"
#include "mozilla/Maybe.h"
#include "mozpkix/pkix.h"
#include "nsITransportSecurityInfo.h"
#include "nsIX509Cert.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "prerror.h"
#include "prio.h"
#include "seccomon.h"
#include "secoidt.h"

using namespace mozilla::pkix;

namespace mozilla {
namespace psm {

enum class EVStatus : uint8_t;

SECStatus AuthCertificateHook(void* arg, PRFileDesc* fd, PRBool checkSig,
                              PRBool isServer);

// This function triggers the certificate verification. The verification is
// asynchronous and the info object will be notified when the verification has
// completed via SetCertVerificationResult.
SECStatus AuthCertificateHookWithInfo(
    CommonSocketControl* socketControl, const nsACString& aHostName,
    const void* aPtrForLogging, nsTArray<nsTArray<uint8_t>>&& peerCertChain,
    Maybe<nsTArray<nsTArray<uint8_t>>>& stapledOCSPResponses,
    Maybe<nsTArray<uint8_t>>& sctsFromTLSExtension, uint32_t providerFlags);

// Base class for dispatching the certificate verification result.
class BaseSSLServerCertVerificationResult {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  [[nodiscard]] virtual nsresult Dispatch(
      nsTArray<nsTArray<uint8_t>>&& aBuiltChain,
      nsTArray<nsTArray<uint8_t>>&& aPeerCertChain,
      uint16_t aCertificateTransparencyStatus, EVStatus aEVStatus,
      bool aSucceeded, PRErrorCode aFinalError,
      nsITransportSecurityInfo::OverridableErrorCategory
          aOverridableErrorCategory,
      bool aIsBuiltCertChainRootBuiltInRoot, uint32_t aProviderFlags,
      bool aMadeOCSPRequests) = 0;
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

  explicit SSLServerCertVerificationResult(CommonSocketControl* socketControl);

  [[nodiscard]] nsresult Dispatch(
      nsTArray<nsTArray<uint8_t>>&& aBuiltChain,
      nsTArray<nsTArray<uint8_t>>&& aPeerCertChain,
      uint16_t aCertificateTransparencyStatus, EVStatus aEVStatus,
      bool aSucceeded, PRErrorCode aFinalError,
      nsITransportSecurityInfo::OverridableErrorCategory
          aOverridableErrorCategory,
      bool aIsBuiltCertChainRootBuiltInRoot, uint32_t aProviderFlags,
      bool aMadeOCSPRequests) override;

 private:
  ~SSLServerCertVerificationResult() = default;

  RefPtr<CommonSocketControl> mSocketControl;
  nsTArray<nsTArray<uint8_t>> mBuiltChain;
  nsTArray<nsTArray<uint8_t>> mPeerCertChain;
  uint16_t mCertificateTransparencyStatus;
  EVStatus mEVStatus;
  bool mSucceeded;
  PRErrorCode mFinalError;
  nsITransportSecurityInfo::OverridableErrorCategory mOverridableErrorCategory;
  bool mIsBuiltCertChainRootBuiltInRoot;
  uint32_t mProviderFlags;
  bool mMadeOCSPRequests;
};

class SSLServerCertVerificationJob : public Runnable {
 public:
  SSLServerCertVerificationJob(const SSLServerCertVerificationJob&) = delete;

  // Must be called only on the socket transport thread
  static SECStatus Dispatch(uint64_t addrForLogging, void* aPinArg,
                            nsTArray<nsTArray<uint8_t>>&& peerCertChain,
                            const nsACString& aHostName, int32_t aPort,
                            const OriginAttributes& aOriginAttributes,
                            Maybe<nsTArray<uint8_t>>& stapledOCSPResponse,
                            Maybe<nsTArray<uint8_t>>& sctsFromTLSExtension,
                            Maybe<DelegatedCredentialInfo>& dcInfo,
                            uint32_t providerFlags, Time time,
                            uint32_t certVerifierFlags,
                            BaseSSLServerCertVerificationResult* aResultTask);

 private:
  NS_DECL_NSIRUNNABLE

  // Must be called only on the socket transport thread
  SSLServerCertVerificationJob(uint64_t addrForLogging, void* aPinArg,
                               nsTArray<nsTArray<uint8_t>>&& peerCertChain,
                               const nsACString& aHostName, int32_t aPort,
                               const OriginAttributes& aOriginAttributes,
                               Maybe<nsTArray<uint8_t>>& stapledOCSPResponse,
                               Maybe<nsTArray<uint8_t>>& sctsFromTLSExtension,
                               Maybe<DelegatedCredentialInfo>& dcInfo,
                               uint32_t providerFlags, Time time,
                               uint32_t certVerifierFlags,
                               BaseSSLServerCertVerificationResult* aResultTask)
      : Runnable("psm::SSLServerCertVerificationJob"),
        mAddrForLogging(addrForLogging),
        mPinArg(aPinArg),
        mPeerCertChain(std::move(peerCertChain)),
        mHostName(aHostName),
        mPort(aPort),
        mOriginAttributes(aOriginAttributes),
        mProviderFlags(providerFlags),
        mCertVerifierFlags(certVerifierFlags),
        mTime(time),
        mStapledOCSPResponse(std::move(stapledOCSPResponse)),
        mSCTsFromTLSExtension(std::move(sctsFromTLSExtension)),
        mDCInfo(std::move(dcInfo)),
        mResultTask(aResultTask) {}

  uint64_t mAddrForLogging;
  void* mPinArg;
  nsTArray<nsTArray<uint8_t>> mPeerCertChain;
  nsCString mHostName;
  int32_t mPort;
  OriginAttributes mOriginAttributes;
  const uint32_t mProviderFlags;
  const uint32_t mCertVerifierFlags;
  const Time mTime;
  Maybe<nsTArray<uint8_t>> mStapledOCSPResponse;
  Maybe<nsTArray<uint8_t>> mSCTsFromTLSExtension;
  Maybe<DelegatedCredentialInfo> mDCInfo;
  RefPtr<BaseSSLServerCertVerificationResult> mResultTask;
};

}  // namespace psm
}  // namespace mozilla

#endif
