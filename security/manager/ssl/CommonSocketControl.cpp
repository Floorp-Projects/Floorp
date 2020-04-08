/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CommonSocketControl.h"
#include "SharedCertVerifier.h"
#include "nsNSSComponent.h"
#include "sslt.h"
#include "ssl.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS_INHERITED(CommonSocketControl, TransportSecurityInfo,
                            nsISSLSocketControl)

CommonSocketControl::CommonSocketControl(uint32_t aProviderFlags)
    : mHandshakeCompleted(false),
      mJoined(false),
      mSentClientCert(false),
      mFailedVerification(false),
      mSSLVersionUsed(nsISSLSocketControl::SSL_VERSION_UNKNOWN),
      mProviderFlags(aProviderFlags) {}

NS_IMETHODIMP
CommonSocketControl::GetNotificationCallbacks(
    nsIInterfaceRequestor** aCallbacks) {
  *aCallbacks = mCallbacks;
  NS_IF_ADDREF(*aCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
CommonSocketControl::SetNotificationCallbacks(
    nsIInterfaceRequestor* aCallbacks) {
  mCallbacks = aCallbacks;
  return NS_OK;
}

NS_IMETHODIMP
CommonSocketControl::ProxyStartSSL(void) { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
CommonSocketControl::StartTLS(void) { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
CommonSocketControl::SetNPNList(nsTArray<nsCString>& aNPNList) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CommonSocketControl::GetAlpnEarlySelection(nsACString& _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CommonSocketControl::GetEarlyDataAccepted(bool* aEarlyDataAccepted) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CommonSocketControl::DriveHandshake(void) { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
CommonSocketControl::JoinConnection(const nsACString& npnProtocol,
                                    const nsACString& hostname, int32_t port,
                                    bool* _retval) {
  nsresult rv = TestJoinConnection(npnProtocol, hostname, port, _retval);
  if (NS_SUCCEEDED(rv) && *_retval) {
    // All tests pass - this is joinable
    mJoined = true;
  }
  return rv;
}

NS_IMETHODIMP
CommonSocketControl::TestJoinConnection(const nsACString& npnProtocol,
                                        const nsACString& hostname,
                                        int32_t port, bool* _retval) {
  *_retval = false;

  // Different ports may not be joined together
  if (port != GetPort()) return NS_OK;

  // Make sure NPN has been completed and matches requested npnProtocol
  if (!mNPNCompleted || !mNegotiatedNPN.Equals(npnProtocol)) return NS_OK;

  IsAcceptableForHost(hostname, _retval);  // sets _retval
  return NS_OK;
}

NS_IMETHODIMP
CommonSocketControl::IsAcceptableForHost(const nsACString& hostname,
                                         bool* _retval) {
  NS_ENSURE_ARG(_retval);

  *_retval = false;

  // If this is the same hostname then the certicate status does not
  // need to be considered. They are joinable.
  if (hostname.Equals(GetHostName())) {
    *_retval = true;
    return NS_OK;
  }

  // Before checking the server certificate we need to make sure the
  // handshake has completed.
  if (!mHandshakeCompleted || !HasServerCert()) {
    return NS_OK;
  }

  // If the cert has error bits (e.g. it is untrusted) then do not join.
  // The value of mHaveCertErrorBits is only reliable because we know that
  // the handshake completed.
  if (mHaveCertErrorBits) {
    return NS_OK;
  }

  // If the connection is using client certificates then do not join
  // because the user decides on whether to send client certs to hosts on a
  // per-domain basis.
  if (mSentClientCert) return NS_OK;

  // Ensure that the server certificate covers the hostname that would
  // like to join this connection

  UniqueCERTCertificate nssCert;

  nsCOMPtr<nsIX509Cert> cert;
  if (NS_FAILED(GetServerCert(getter_AddRefs(cert)))) {
    return NS_OK;
  }
  if (cert) {
    nssCert.reset(cert->GetCert());
  }

  if (!nssCert) {
    return NS_OK;
  }

  // Attempt to verify the joinee's certificate using the joining hostname.
  // This ensures that any hostname-specific verification logic (e.g. key
  // pinning) is satisfied by the joinee's certificate chain.
  // This verification only uses local information; since we're on the network
  // thread, we would be blocking on ourselves if we attempted any network i/o.
  // TODO(bug 1056935): The certificate chain built by this verification may be
  // different than the certificate chain originally built during the joined
  // connection's TLS handshake. Consequently, we may report a wrong and/or
  // misleading certificate chain for HTTP transactions coalesced onto this
  // connection. This may become problematic in the future. For example,
  // if/when we begin relying on intermediate certificates being stored in the
  // securityInfo of a cached HTTPS response, that cached certificate chain may
  // actually be the wrong chain. We should consider having JoinConnection
  // return the certificate chain built here, so that the calling Necko code
  // can associate the correct certificate chain with the HTTP transactions it
  // is trying to join onto this connection.
  RefPtr<psm::SharedCertVerifier> certVerifier(psm::GetDefaultCertVerifier());
  if (!certVerifier) {
    return NS_OK;
  }
  psm::CertVerifier::Flags flags = psm::CertVerifier::FLAG_LOCAL_ONLY;
  UniqueCERTCertList unusedBuiltChain;
  mozilla::pkix::Result result =
      certVerifier->VerifySSLServerCert(nssCert, mozilla::pkix::Now(),
                                        nullptr,  // pinarg
                                        hostname, unusedBuiltChain, flags);
  if (result != mozilla::pkix::Success) {
    return NS_OK;
  }

  // All tests pass
  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP
CommonSocketControl::GetKEAUsed(int16_t* aKEAUsed) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CommonSocketControl::GetKEAKeyBits(uint32_t* aKEAKeyBits) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CommonSocketControl::GetProviderFlags(uint32_t* aProviderFlags) {
  *aProviderFlags = mProviderFlags;
  return NS_OK;
}

NS_IMETHODIMP
CommonSocketControl::GetProviderTlsFlags(uint32_t* aProviderTlsFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CommonSocketControl::GetSSLVersionUsed(int16_t* aSSLVersionUsed) {
  *aSSLVersionUsed = mSSLVersionUsed;
  return NS_OK;
}

NS_IMETHODIMP
CommonSocketControl::GetSSLVersionOffered(int16_t* aSSLVersionOffered) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CommonSocketControl::GetMACAlgorithmUsed(int16_t* aMACAlgorithmUsed) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

bool CommonSocketControl::GetDenyClientCert() { return true; }

void CommonSocketControl::SetDenyClientCert(bool aDenyClientCert) {}

NS_IMETHODIMP
CommonSocketControl::GetClientCert(nsIX509Cert** aClientCert) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CommonSocketControl::SetClientCert(nsIX509Cert* aClientCert) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CommonSocketControl::GetClientCertSent(bool* arg) {
  *arg = mSentClientCert;
  return NS_OK;
}

NS_IMETHODIMP
CommonSocketControl::GetFailedVerification(bool* arg) {
  *arg = mFailedVerification;
  return NS_OK;
}

NS_IMETHODIMP
CommonSocketControl::GetEsniTxt(nsACString& aEsniTxt) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CommonSocketControl::SetEsniTxt(const nsACString& aEsniTxt) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CommonSocketControl::GetPeerId(nsACString& aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
