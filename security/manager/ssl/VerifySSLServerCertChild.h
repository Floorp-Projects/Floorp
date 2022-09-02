/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_psm_VerifySSLServerCertChild_h__
#define mozilla_psm_VerifySSLServerCertChild_h__

#include "mozilla/psm/PVerifySSLServerCertChild.h"

#include "SSLServerCertVerification.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "seccomon.h"

namespace mozilla {
namespace psm {

class DelegatedCredentialInfo;

// This class implements the socket process part of the server certificate
// verification IPC protocol.
class VerifySSLServerCertChild : public PVerifySSLServerCertChild {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VerifySSLServerCertChild, override);

  explicit VerifySSLServerCertChild(
      SSLServerCertVerificationResult* aResultTask,
      nsTArray<nsTArray<uint8_t>>&& aPeerCertChain, uint32_t aProviderFlags);

  ipc::IPCResult RecvOnVerifiedSSLServerCertSuccess(
      nsTArray<ByteArray>&& aBuiltCertChain,
      const uint16_t& aCertTransparencyStatus, const uint8_t& aEVStatus,
      const bool& aIsBuiltCertChainRootBuiltInRoot,
      const bool& aMadeOCSPRequests);

  ipc::IPCResult RecvOnVerifiedSSLServerCertFailure(
      const int32_t& aFinalError, const uint32_t& aOverridableErrorCategory,
      const bool& aMadeOCSPRequests);

 private:
  ~VerifySSLServerCertChild() = default;

  RefPtr<SSLServerCertVerificationResult> mResultTask;
  nsTArray<nsTArray<uint8_t>> mPeerCertChain;
  uint32_t mProviderFlags;
};

SECStatus RemoteProcessCertVerification(
    nsTArray<nsTArray<uint8_t>>&& aPeerCertChain, const nsACString& aHostName,
    int32_t aPort, const OriginAttributes& aOriginAttributes,
    Maybe<nsTArray<uint8_t>>& aStapledOCSPResponse,
    Maybe<nsTArray<uint8_t>>& aSctsFromTLSExtension,
    Maybe<DelegatedCredentialInfo>& aDcInfo, uint32_t aProviderFlags,
    uint32_t aCertVerifierFlags, SSLServerCertVerificationResult* aResultTask);

}  // namespace psm
}  // namespace mozilla

#endif
