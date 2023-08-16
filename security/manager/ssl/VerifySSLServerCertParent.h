/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_psm_VerifySSLServerCertParent_h__
#define mozilla_psm_VerifySSLServerCertParent_h__

#include "mozilla/psm/PVerifySSLServerCertParent.h"
#include "mozpkix/Time.h"
#include "ScopedNSSTypes.h"
#include "SharedCertVerifier.h"

namespace mozilla {
namespace psm {

// This class implements the main process side of the server certificate
// verification for socket process.
// SSLServerCertVerificationJob::Dispatch is called in
// VerifySSLServerCertParent::Dispatch with IPCServerCertVerificationResult and
// the result of the certificate verification will be sent to the socket process
// via IPC.
class VerifySSLServerCertParent : public PVerifySSLServerCertParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VerifySSLServerCertParent, override)

  VerifySSLServerCertParent();

  bool Dispatch(nsTArray<ByteArray>&& aPeerCertChain,
                const nsACString& aHostName, const int32_t& aPort,
                const OriginAttributes& aOriginAttributes,
                const Maybe<ByteArray>& aStapledOCSPResponse,
                const Maybe<ByteArray>& aSctsFromTLSExtension,
                const Maybe<DelegatedCredentialInfoArg>& aDcInfo,
                const uint32_t& aProviderFlags,
                const uint32_t& aCertVerifierFlags);

  void OnVerifiedSSLServerCert(const nsTArray<ByteArray>& aBuiltCertChain,
                               uint16_t aCertificateTransparencyStatus,
                               uint8_t aEVStatus, bool aSucceeded,
                               PRErrorCode aFinalError,
                               uint32_t aOverridableErrorCategory,
                               bool aIsBuiltCertChainRootBuiltInRoot,
                               bool aMadeOCSPRequests);

 private:
  virtual ~VerifySSLServerCertParent();

  // PVerifySSLServerCertParent
  void ActorDestroy(ActorDestroyReason aWhy) override;

  nsCOMPtr<nsISerialEventTarget> mBackgroundThread;
};

}  // namespace psm
}  // namespace mozilla

#endif  // mozilla_psm_VerifySSLServerCertParent_h__
