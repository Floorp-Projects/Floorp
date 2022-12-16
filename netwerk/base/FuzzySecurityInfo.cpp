/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FuzzySecurityInfo.h"

#include "nsIWebProgressListener.h"
#include "nsString.h"

namespace mozilla {
namespace net {

FuzzySecurityInfo::FuzzySecurityInfo() {}

FuzzySecurityInfo::~FuzzySecurityInfo() {}

NS_IMPL_ISUPPORTS(FuzzySecurityInfo, nsITransportSecurityInfo)

NS_IMETHODIMP
FuzzySecurityInfo::GetSecurityState(uint32_t* state) {
  *state = nsIWebProgressListener::STATE_IS_SECURE;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetErrorCode(int32_t* state) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetErrorCodeString(nsAString& aErrorString) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetFailedCertChain(
    nsTArray<RefPtr<nsIX509Cert>>& aFailedCertChain) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetServerCert(nsIX509Cert** aServerCert) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetSucceededCertChain(
    nsTArray<RefPtr<nsIX509Cert>>& aSucceededCertChain) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetCipherName(nsACString& aCipherName) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetKeyLength(uint32_t* aKeyLength) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetSecretKeyLength(uint32_t* aSecretKeyLength) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetKeaGroupName(nsACString& aKeaGroup) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetSignatureSchemeName(nsACString& aSignatureScheme) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetProtocolVersion(uint16_t* aProtocolVersion) {
  NS_ENSURE_ARG_POINTER(aProtocolVersion);
  // Must be >= TLS 1.2 for HTTP2
  *aProtocolVersion = nsITransportSecurityInfo::TLS_VERSION_1_2;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetCertificateTransparencyStatus(
    uint16_t* aCertificateTransparencyStatus) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetIsDelegatedCredential(bool* aIsDelegCred) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetIsAcceptedEch(bool* aIsAcceptedEch) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetOverridableErrorCategory(
    OverridableErrorCategory* aOverridableErrorCode) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetMadeOCSPRequests(bool* aMadeOCSPRequests) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetUsedPrivateDNS(bool* aUsedPrivateDNS) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetIsExtendedValidation(bool* aIsEV) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::ToString(nsACString& aResult) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

void FuzzySecurityInfo::SerializeToIPC(IPC::MessageWriter* aWriter) {
  MOZ_CRASH("Unused");
}

NS_IMETHODIMP
FuzzySecurityInfo::GetNegotiatedNPN(nsACString& aNegotiatedNPN) {
  aNegotiatedNPN.Assign("h2");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetResumed(bool* aResumed) {
  *aResumed = false;
  return NS_OK;
}

NS_IMETHODIMP FuzzySecurityInfo::GetIsBuiltCertChainRootBuiltInRoot(
    bool* aIsBuiltInRoot) {
  *aIsBuiltInRoot = false;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetPeerId(nsACString& aResult) {
  aResult.Assign(""_ns);
  return NS_OK;
}

}  // namespace net

}  // namespace mozilla
