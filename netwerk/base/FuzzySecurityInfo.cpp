/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FuzzySecurityInfo.h"
#include "mozilla/Logging.h"
#include "mozilla/OriginAttributes.h"
#include "nsThreadManager.h"

namespace mozilla {
namespace net {

FuzzySecurityInfo::FuzzySecurityInfo() {}

FuzzySecurityInfo::~FuzzySecurityInfo() {}

NS_IMPL_ISUPPORTS(FuzzySecurityInfo, nsITransportSecurityInfo,
                  nsIInterfaceRequestor, nsISSLSocketControl)

NS_IMETHODIMP
FuzzySecurityInfo::GetErrorCode(int32_t* state) {
  *state = 0;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetSecurityState(uint32_t* state) {
  *state = 0;
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
  NS_ENSURE_ARG_POINTER(aServerCert);
  // This method is called by nsHttpChannel::ProcessSSLInformation()
  // in order to display certain information in the console.
  // Returning NULL is okay here and handled by the caller.
  *aServerCert = NULL;
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
  *aSecretKeyLength = 4096;
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
  NS_ENSURE_ARG_POINTER(aCertificateTransparencyStatus);
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetIsDomainMismatch(bool* aIsDomainMismatch) {
  NS_ENSURE_ARG_POINTER(aIsDomainMismatch);
  *aIsDomainMismatch = false;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetIsNotValidAtThisTime(bool* aIsNotValidAtThisTime) {
  NS_ENSURE_ARG_POINTER(aIsNotValidAtThisTime);
  *aIsNotValidAtThisTime = false;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetIsUntrusted(bool* aIsUntrusted) {
  NS_ENSURE_ARG_POINTER(aIsUntrusted);
  *aIsUntrusted = false;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetIsExtendedValidation(bool* aIsEV) {
  NS_ENSURE_ARG_POINTER(aIsEV);
  *aIsEV = true;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetIsDelegatedCredential(bool* aIsDelegCred) {
  NS_ENSURE_ARG_POINTER(aIsDelegCred);
  *aIsDelegCred = false;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetInterface(const nsIID& uuid, void** result) {
  if (!NS_IsMainThread()) {
    MOZ_CRASH("FuzzySecurityInfo::GetInterface called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsresult rv = NS_ERROR_NO_INTERFACE;
  if (mCallbacks) {
    rv = mCallbacks->GetInterface(uuid, result);
  }
  return rv;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetNotificationCallbacks(
    nsIInterfaceRequestor** aCallbacks) {
  nsCOMPtr<nsIInterfaceRequestor> ir(mCallbacks);
  ir.forget(aCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::SetNotificationCallbacks(nsIInterfaceRequestor* aCallbacks) {
  mCallbacks = aCallbacks;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetProviderFlags(uint32_t* aProviderFlags) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetProviderTlsFlags(uint32_t* aProviderTlsFlags) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetKEAUsed(int16_t* aKea) {
  // Can be ssl_kea_dh or ssl_kea_ecdh for HTTP2
  *aKea = ssl_kea_ecdh;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetKEAKeyBits(uint32_t* aKeyBits) {
  // Must be >= 224 for ecdh and >= 2048 for dh when using HTTP2
  *aKeyBits = 256;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetSSLVersionUsed(int16_t* aSSLVersionUsed) {
  // Must be >= TLS 1.2 for HTTP2
  *aSSLVersionUsed = nsISSLSocketControl::TLS_VERSION_1_2;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetSSLVersionOffered(int16_t* aSSLVersionOffered) {
  *aSSLVersionOffered = nsISSLSocketControl::TLS_VERSION_1_2;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetMACAlgorithmUsed(int16_t* aMac) {
  // The only valid choice for HTTP2 is SSL_MAC_AEAD
  *aMac = nsISSLSocketControl::SSL_MAC_AEAD;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetClientCert(nsIX509Cert** aClientCert) {
  NS_ENSURE_ARG_POINTER(aClientCert);
  *aClientCert = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::SetClientCert(nsIX509Cert* aClientCert) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

bool FuzzySecurityInfo::GetDenyClientCert() { return false; }

void FuzzySecurityInfo::SetDenyClientCert(bool aDenyClientCert) {
  // Called by mozilla::net::nsHttpConnection::StartSpdy
}

NS_IMETHODIMP
FuzzySecurityInfo::GetClientCertSent(bool* arg) {
  *arg = false;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetFailedVerification(bool* arg) {
  *arg = false;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetNegotiatedNPN(nsACString& aNegotiatedNPN) {
  aNegotiatedNPN = "h2";
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetAlpnEarlySelection(nsACString& aAlpnSelected) {
  // TODO: For now we don't support early selection
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetEarlyDataAccepted(bool* aAccepted) {
  *aAccepted = false;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetResumed(bool* aResumed) {
  *aResumed = false;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::DriveHandshake() { return NS_OK; }

NS_IMETHODIMP
FuzzySecurityInfo::IsAcceptableForHost(const nsACString& hostname,
                                       bool* _retval) {
  NS_ENSURE_ARG(_retval);
  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::TestJoinConnection(const nsACString& npnProtocol,
                                      const nsACString& hostname, int32_t port,
                                      bool* _retval) {
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::JoinConnection(const nsACString& npnProtocol,
                                  const nsACString& hostname, int32_t port,
                                  bool* _retval) {
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::ProxyStartSSL() { return NS_OK; }

NS_IMETHODIMP
FuzzySecurityInfo::StartTLS() { return NS_OK; }

NS_IMETHODIMP
FuzzySecurityInfo::SetNPNList(nsTArray<nsCString>& protocolArray) {
  return NS_OK;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetEsniTxt(nsACString& aEsniTxt) { return NS_OK; }

NS_IMETHODIMP
FuzzySecurityInfo::SetEsniTxt(const nsACString& aEsniTxt) {
  MOZ_CRASH("Unused");
  return NS_OK;
}

void FuzzySecurityInfo::SerializeToIPC(IPC::Message* aMsg) {
  MOZ_CRASH("Unused");
}

bool FuzzySecurityInfo::DeserializeFromIPC(const IPC::Message* aMsg,
                                           PickleIterator* aIter) {
  MOZ_CRASH("Unused");
  return false;
}

NS_IMETHODIMP
FuzzySecurityInfo::GetPeerId(nsACString& aResult) {
  aResult.Assign(EmptyCString());
  return NS_OK;
}

NS_IMETHODIMP FuzzySecurityInfo::SetIsBuiltCertChainRootBuiltInRoot(
    bool aIsBuiltInRoot) {
  return NS_OK;
}

NS_IMETHODIMP FuzzySecurityInfo::GetIsBuiltCertChainRootBuiltInRoot(
    bool* aIsBuiltInRoot) {
  *aIsBuiltInRoot = false;
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
