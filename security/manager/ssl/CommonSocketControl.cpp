/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CommonSocketControl.h"

#include "BRNameMatchingPolicy.h"
#include "PublicKeyPinningService.h"
#include "SharedCertVerifier.h"
#include "nsNSSComponent.h"
#include "SharedSSLState.h"
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

  // An empty mSucceededCertChain means the server certificate verification
  // failed before, so don't join in this case.
  if (mSucceededCertChain.IsEmpty()) {
    return NS_OK;
  }

  // See where CheckCertHostname() is called in
  // CertVerifier::VerifySSLServerCert. We are doing the same hostname-specific
  // checks here. If any hostname-specific checks are added to
  // CertVerifier::VerifySSLServerCert we need to add them here too.
  Input serverCertInput;
  mozilla::pkix::Result rv =
      serverCertInput.Init(nssCert->derCert.data, nssCert->derCert.len);
  if (rv != Success) {
    return NS_OK;
  }

  Input hostnameInput;
  rv = hostnameInput.Init(
      BitwiseCast<const uint8_t*, const char*>(hostname.BeginReading()),
      hostname.Length());
  if (rv != Success) {
    return NS_OK;
  }

  mozilla::psm::BRNameMatchingPolicy nameMatchingPolicy(
      mIsBuiltCertChainRootBuiltInRoot
          ? mozilla::psm::PublicSSLState()->NameMatchingMode()
          : mozilla::psm::BRNameMatchingPolicy::Mode::DoNotEnforce);
  rv = CheckCertHostname(serverCertInput, hostnameInput, nameMatchingPolicy);
  if (rv != Success) {
    return NS_OK;
  }

  mozilla::psm::CertVerifier::PinningMode pinningMode =
      mozilla::psm::PublicSSLState()->PinningMode();
  if (pinningMode != mozilla::psm::CertVerifier::pinningDisabled) {
    bool chainHasValidPins;
    bool enforceTestMode =
        (pinningMode == mozilla::psm::CertVerifier::pinningEnforceTestMode);
    nsresult nsrv = mozilla::psm::PublicKeyPinningService::ChainHasValidPins(
        mSucceededCertChain, PromiseFlatCString(hostname).BeginReading(), Now(),
        enforceTestMode, GetOriginAttributes(), chainHasValidPins, nullptr);
    if (NS_FAILED(nsrv)) {
      return NS_OK;
    }

    if (!chainHasValidPins) {
      return NS_OK;
    }
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
