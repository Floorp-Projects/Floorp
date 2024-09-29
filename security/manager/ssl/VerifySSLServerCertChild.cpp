/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VerifySSLServerCertChild.h"

#include "CertVerifier.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/net/SocketProcessBackgroundChild.h"
#include "mozilla/psm/PVerifySSLServerCertParent.h"
#include "mozilla/psm/PVerifySSLServerCertChild.h"
#include "nsNSSIOLayer.h"
#include "nsSerializationHelper.h"

#include "secerr.h"

extern mozilla::LazyLogModule gPIPNSSLog;

namespace mozilla {
namespace psm {

VerifySSLServerCertChild::VerifySSLServerCertChild(
    SSLServerCertVerificationResult* aResultTask,
    nsTArray<nsTArray<uint8_t>>&& aPeerCertChain, uint32_t aProviderFlags)
    : mResultTask(aResultTask),
      mPeerCertChain(std::move(aPeerCertChain)),
      mProviderFlags(aProviderFlags) {}

ipc::IPCResult VerifySSLServerCertChild::RecvOnVerifySSLServerCertFinished(
    nsTArray<ByteArray>&& aBuiltCertChain,
    const uint16_t& aCertTransparencyStatus, const EVStatus& aEVStatus,
    const bool& aSucceeded, int32_t aFinalError,
    const nsITransportSecurityInfo::OverridableErrorCategory&
        aOverridableErrorCategory,
    const bool& aIsBuiltCertChainRootBuiltInRoot,
    const bool& aMadeOCSPRequests) {
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("[%p] VerifySSLServerCertChild::RecvOnVerifySSLServerCertFinished",
           this));

  nsTArray<nsTArray<uint8_t>> certBytesArray;
  for (auto& cert : aBuiltCertChain) {
    certBytesArray.AppendElement(std::move(cert.data()));
  }

  nsresult rv = mResultTask->Dispatch(
      std::move(certBytesArray), std::move(mPeerCertChain),
      aCertTransparencyStatus, aEVStatus, aSucceeded, aFinalError,
      aOverridableErrorCategory, aIsBuiltCertChainRootBuiltInRoot,
      mProviderFlags, aMadeOCSPRequests);
  if (NS_FAILED(rv)) {
    // We can't release this off the STS thread because some parts of it are
    // not threadsafe. Just leak mResultTask.
    Unused << mResultTask.forget();
  }
  return IPC_OK();
}

SECStatus RemoteProcessCertVerification(
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

  ipc::Endpoint<PVerifySSLServerCertParent> parentEndpoint;
  ipc::Endpoint<PVerifySSLServerCertChild> childEndpoint;
  PVerifySSLServerCert::CreateEndpoints(&parentEndpoint, &childEndpoint);

  // Create a dedicated nsCString, so that our lambda below can take an
  // ownership stake in the underlying string buffer:
  nsCString hostName(aHostName);

  if (NS_FAILED(net::SocketProcessBackgroundChild::WithActor(
          "SendInitVerifySSLServerCert",
          [endpoint = std::move(parentEndpoint),
           peerCertBytes = std::move(peerCertBytes),
           hostName = std::move(hostName), port(aPort),
           originAttributes(aOriginAttributes),
           stapledOCSPResponse = std::move(stapledOCSPResponse),
           sctsFromTLSExtension = std::move(sctsFromTLSExtension),
           dcInfo = std::move(dcInfo), providerFlags(aProviderFlags),
           certVerifierFlags(aCertVerifierFlags)](
              net::SocketProcessBackgroundChild* aActor) mutable {
            Unused << aActor->SendInitVerifySSLServerCert(
                std::move(endpoint), peerCertBytes, hostName, port,
                originAttributes, stapledOCSPResponse, sctsFromTLSExtension,
                dcInfo, providerFlags, certVerifierFlags);
          }))) {
    PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
    return SECFailure;
  }

  RefPtr<VerifySSLServerCertChild> authCert = new VerifySSLServerCertChild(
      aResultTask, std::move(aPeerCertChain), aProviderFlags);
  if (!childEndpoint.Bind(authCert)) {
    PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
    return SECFailure;
  }

  PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
  return SECWouldBlock;
}

}  // namespace psm
}  // namespace mozilla
