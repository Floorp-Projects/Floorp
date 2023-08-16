/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VerifySSLServerCertParent.h"

#include "cert.h"
#include "nsNSSComponent.h"
#include "secerr.h"
#include "SharedCertVerifier.h"
#include "NSSCertDBTrustDomain.h"
#include "SSLServerCertVerification.h"
#include "nsNSSIOLayer.h"
#include "nsISocketProvider.h"
#include "mozilla/Unused.h"

extern mozilla::LazyLogModule gPIPNSSLog;

using namespace mozilla::pkix;

namespace mozilla {
namespace psm {

VerifySSLServerCertParent::VerifySSLServerCertParent() {}

void VerifySSLServerCertParent::OnVerifiedSSLServerCert(
    const nsTArray<ByteArray>& aBuiltCertChain,
    uint16_t aCertificateTransparencyStatus, uint8_t aEVStatus, bool aSucceeded,
    PRErrorCode aFinalError, uint32_t aOverridableErrorCategory,
    bool aIsBuiltCertChainRootBuiltInRoot, bool aMadeOCSPRequests) {
  if (!CanSend()) {
    return;
  }

  if (aSucceeded) {
    Unused << SendOnVerifiedSSLServerCertSuccess(
        aBuiltCertChain, aCertificateTransparencyStatus, aEVStatus,
        aIsBuiltCertChainRootBuiltInRoot, aMadeOCSPRequests);
  } else {
    Unused << SendOnVerifiedSSLServerCertFailure(
        aFinalError, aOverridableErrorCategory, aMadeOCSPRequests);
  }

  Close();
}

namespace {

class IPCServerCertVerificationResult final
    : public BaseSSLServerCertVerificationResult {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(IPCServerCertVerificationResult,
                                        override)

  IPCServerCertVerificationResult(nsIEventTarget* aTarget,
                                  VerifySSLServerCertParent* aParent)
      : mTarget(aTarget), mParent(aParent) {}

  void Dispatch(nsTArray<nsTArray<uint8_t>>&& aBuiltChain,
                nsTArray<nsTArray<uint8_t>>&& aPeerCertChain,
                uint16_t aCertificateTransparencyStatus, EVStatus aEVStatus,
                bool aSucceeded, PRErrorCode aFinalError,
                nsITransportSecurityInfo::OverridableErrorCategory
                    aOverridableErrorCategory,
                bool aIsBuiltCertChainRootBuiltInRoot, uint32_t aProviderFlags,
                bool aMadeOCSPRequests) override;

 private:
  ~IPCServerCertVerificationResult() = default;

  nsCOMPtr<nsIEventTarget> mTarget;
  RefPtr<VerifySSLServerCertParent> mParent;
};

void IPCServerCertVerificationResult::Dispatch(
    nsTArray<nsTArray<uint8_t>>&& aBuiltChain,
    nsTArray<nsTArray<uint8_t>>&& aPeerCertChain,
    uint16_t aCertificateTransparencyStatus, EVStatus aEVStatus,
    bool aSucceeded, PRErrorCode aFinalError,
    nsITransportSecurityInfo::OverridableErrorCategory
        aOverridableErrorCategory,
    bool aIsBuiltCertChainRootBuiltInRoot, uint32_t aProviderFlags,
    bool aMadeOCSPRequests) {
  nsTArray<ByteArray> builtCertChain;
  if (aSucceeded) {
    for (auto& cert : aBuiltChain) {
      builtCertChain.AppendElement(ByteArray(cert));
    }
  }

  nsresult nrv = mTarget->Dispatch(
      NS_NewRunnableFunction(
          "psm::VerifySSLServerCertParent::OnVerifiedSSLServerCert",
          [parent(mParent), builtCertChain{std::move(builtCertChain)},
           aCertificateTransparencyStatus, aEVStatus, aSucceeded, aFinalError,
           aOverridableErrorCategory, aIsBuiltCertChainRootBuiltInRoot,
           aMadeOCSPRequests, aProviderFlags]() {
            if (aSucceeded &&
                !(aProviderFlags & nsISocketProvider::NO_PERMANENT_STORAGE)) {
              nsTArray<nsTArray<uint8_t>> certBytesArray;
              for (const auto& cert : builtCertChain) {
                certBytesArray.AppendElement(cert.data().Clone());
              }
              // This dispatches an event that will run when the socket thread
              // is idle.
              SaveIntermediateCerts(certBytesArray);
            }
            parent->OnVerifiedSSLServerCert(
                builtCertChain, aCertificateTransparencyStatus,
                static_cast<uint8_t>(aEVStatus), aSucceeded, aFinalError,
                static_cast<uint32_t>(aOverridableErrorCategory),
                aIsBuiltCertChainRootBuiltInRoot, aMadeOCSPRequests);
          }),
      NS_DISPATCH_NORMAL);
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(nrv));
  Unused << nrv;
}

}  // anonymous namespace

bool VerifySSLServerCertParent::Dispatch(
    nsTArray<ByteArray>&& aPeerCertChain, const nsACString& aHostName,
    const int32_t& aPort, const OriginAttributes& aOriginAttributes,
    const Maybe<ByteArray>& aStapledOCSPResponse,
    const Maybe<ByteArray>& aSctsFromTLSExtension,
    const Maybe<DelegatedCredentialInfoArg>& aDcInfo,
    const uint32_t& aProviderFlags, const uint32_t& aCertVerifierFlags) {
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("VerifySSLServerCertParent::Dispatch"));

  mBackgroundThread = GetCurrentSerialEventTarget();

  nsTArray<nsTArray<uint8_t>> peerCertBytes;
  for (auto& certBytes : aPeerCertChain) {
    nsTArray<uint8_t> bytes;
    peerCertBytes.AppendElement(std::move(certBytes.data()));
  }

  Maybe<nsTArray<uint8_t>> stapledOCSPResponse;
  if (aStapledOCSPResponse) {
    stapledOCSPResponse.emplace(aStapledOCSPResponse->data().Clone());
  }

  Maybe<nsTArray<uint8_t>> sctsFromTLSExtension;
  if (aSctsFromTLSExtension) {
    sctsFromTLSExtension.emplace(aSctsFromTLSExtension->data().Clone());
  }

  Maybe<DelegatedCredentialInfo> dcInfo;
  if (aDcInfo) {
    dcInfo.emplace();
    dcInfo->scheme = static_cast<SSLSignatureScheme>(aDcInfo->scheme());
    dcInfo->authKeyBits = aDcInfo->authKeyBits();
  }

  RefPtr<IPCServerCertVerificationResult> resultTask =
      new IPCServerCertVerificationResult(mBackgroundThread, this);
  SECStatus status = SSLServerCertVerificationJob::Dispatch(
      0, nullptr, std::move(peerCertBytes), aHostName, aPort, aOriginAttributes,
      stapledOCSPResponse, sctsFromTLSExtension, dcInfo, aProviderFlags, Now(),
      aCertVerifierFlags, resultTask);

  if (status != SECWouldBlock) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("VerifySSLServerCertParent::Dispatch - dispatch failed"));
    return false;
  }

  return true;
}

void VerifySSLServerCertParent::ActorDestroy(ActorDestroyReason aWhy) {}

VerifySSLServerCertParent::~VerifySSLServerCertParent() = default;

}  // namespace psm
}  // namespace mozilla
