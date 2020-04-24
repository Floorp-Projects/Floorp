/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VerifySSLServerCertChild.h"

#include "CertVerifier.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsNSSIOLayer.h"
#include "nsSerializationHelper.h"

extern mozilla::LazyLogModule gPIPNSSLog;

namespace mozilla {
namespace psm {

VerifySSLServerCertChild::VerifySSLServerCertChild(
    const UniqueCERTCertificate& aCert,
    SSLServerCertVerificationResult* aResultTask,
    nsTArray<nsTArray<uint8_t>>&& aPeerCertChain)
    : mCert(CERT_DupCertificate(aCert.get())),
      mResultTask(aResultTask),
      mPeerCertChain(std::move(aPeerCertChain)) {}

ipc::IPCResult VerifySSLServerCertChild::RecvOnVerifiedSSLServerCertSuccess(
    nsTArray<ByteArray>&& aBuiltCertChain,
    const uint16_t& aCertTransparencyStatus, const uint8_t& aEVStatus,
    const bool& aIsBuiltCertChainRootBuiltInRoot) {
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("[%p] VerifySSLServerCertChild::RecvOnVerifiedSSLServerCertSuccess",
           this));

  RefPtr<nsNSSCertificate> nsc = nsNSSCertificate::Create(mCert.get());
  nsTArray<nsTArray<uint8_t>> certBytesArray;
  for (auto& cert : aBuiltCertChain) {
    certBytesArray.AppendElement(std::move(cert.data()));
  }

  mResultTask->Dispatch(nsc, std::move(certBytesArray),
                        std::move(mPeerCertChain), aCertTransparencyStatus,
                        static_cast<EVStatus>(aEVStatus), true, 0, 0,
                        aIsBuiltCertChainRootBuiltInRoot);
  return IPC_OK();
}

ipc::IPCResult VerifySSLServerCertChild::RecvOnVerifiedSSLServerCertFailure(
    const uint32_t& aFinalError, const uint32_t& aCollectedErrors) {
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("[%p]VerifySSLServerCertChild::"
           "RecvOnVerifiedSSLServerCertFailure - aFinalError=%u, "
           "aCollectedErrors=%u",
           this, aFinalError, aCollectedErrors));

  RefPtr<nsNSSCertificate> nsc = nsNSSCertificate::Create(mCert.get());
  mResultTask->Dispatch(
      nsc, nsTArray<nsTArray<uint8_t>>(), std::move(mPeerCertChain),
      nsITransportSecurityInfo::CERTIFICATE_TRANSPARENCY_NOT_APPLICABLE,
      EVStatus::NotEV, false, aFinalError, aCollectedErrors, false);
  return IPC_OK();
}

SECStatus RemoteProcessCertVerification(
    const UniqueCERTCertificate& aCert,
    nsTArray<nsTArray<uint8_t>>&& aPeerCertChain, const nsACString& aHostName,
    int32_t aPort, const OriginAttributes& aOriginAttributes,
    Maybe<nsTArray<uint8_t>>& aStapledOCSPResponse,
    Maybe<nsTArray<uint8_t>>& aSctsFromTLSExtension,
    Maybe<DelegatedCredentialInfo>& aDcInfo, uint32_t aProviderFlags,
    uint32_t aCertVerifierFlags, SSLServerCertVerificationResult* aResultTask) {
  if (!aResultTask) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

  nsTArray<uint8_t> serverCertSerialized;
  serverCertSerialized.AppendElements(aCert->derCert.data, aCert->derCert.len);

  nsTArray<ByteArray> peerCertBytes;
  for (auto& certBytes : aPeerCertChain) {
    peerCertBytes.AppendElement(ByteArray(certBytes));
  }

  Maybe<ByteArray> stapledOCSPResponse;
  if (aStapledOCSPResponse) {
    stapledOCSPResponse.emplace();
    stapledOCSPResponse->data().Assign(*aStapledOCSPResponse);
  }

  Maybe<ByteArray> sctsFromTLSExtension;
  if (aSctsFromTLSExtension) {
    sctsFromTLSExtension.emplace();
    sctsFromTLSExtension->data().Assign(*aSctsFromTLSExtension);
  }

  Maybe<DelegatedCredentialInfoArg> dcInfo;
  if (aDcInfo) {
    dcInfo.emplace();
    dcInfo.ref().scheme() = static_cast<uint32_t>(aDcInfo->scheme);
    dcInfo.ref().authKeyBits() = static_cast<uint32_t>(aDcInfo->authKeyBits);
  }

  mozilla::ipc::PBackgroundChild* actorChild = mozilla::ipc::BackgroundChild::
      GetOrCreateForSocketParentBridgeForCurrentThread();
  if (!actorChild) {
    PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
    return SECFailure;
  }

  RefPtr<VerifySSLServerCertChild> authCert = new VerifySSLServerCertChild(
      aCert, aResultTask, std::move(aPeerCertChain));
  if (!actorChild->SendPVerifySSLServerCertConstructor(
          authCert, serverCertSerialized, peerCertBytes,
          PromiseFlatCString(aHostName), aPort, aOriginAttributes,
          stapledOCSPResponse, sctsFromTLSExtension, dcInfo, aProviderFlags,
          aCertVerifierFlags)) {
    PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
    return SECFailure;
  }

  PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
  return SECWouldBlock;
}

}  // namespace psm
}  // namespace mozilla
