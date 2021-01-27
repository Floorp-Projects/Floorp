/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuicSocketControl.h"

#include "Http3Session.h"
#include "SharedCertVerifier.h"
#include "nsISocketProvider.h"
#include "nsIWebProgressListener.h"
#include "nsNSSComponent.h"
#include "nsWeakReference.h"
#include "nsSocketTransportService2.h"
#include "nsThreadUtils.h"
#include "sslt.h"
#include "ssl.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS_INHERITED(QuicSocketControl, TransportSecurityInfo,
                            nsISSLSocketControl, QuicSocketControl)

QuicSocketControl::QuicSocketControl(uint32_t aProviderFlags)
    : CommonSocketControl(aProviderFlags) {}

void QuicSocketControl::SetCertVerificationResult(PRErrorCode errorCode) {
  if (errorCode) {
    mFailedVerification = true;
    SetCanceled(errorCode);
  }

  if (OnSocketThread()) {
    CallAuthenticated();
  } else {
    DebugOnly<nsresult> rv = gSocketTransportService->Dispatch(
        NewRunnableMethod("QuicSocketControl::CallAuthenticated", this,
                          &QuicSocketControl::CallAuthenticated),
        NS_DISPATCH_NORMAL);
  }
}

NS_IMETHODIMP
QuicSocketControl::GetSSLVersionOffered(int16_t* aSSLVersionOffered) {
  *aSSLVersionOffered = nsISSLSocketControl::TLS_VERSION_1_3;
  return NS_OK;
}

void QuicSocketControl::CallAuthenticated() {
  RefPtr<Http3Session> http3Session = do_QueryReferent(mHttp3Session);
  if (http3Session) {
    http3Session->Authenticated(GetErrorCode());
  }
  mHttp3Session = nullptr;
}

void QuicSocketControl::SetAuthenticationCallback(Http3Session* aHttp3Session) {
  mHttp3Session = do_GetWeakReference(
      static_cast<nsISupportsWeakReference*>(aHttp3Session));
}

void QuicSocketControl::HandshakeCompleted() {
  psm::RememberCertErrorsTable::GetInstance().LookupCertErrorBits(this);

  uint32_t state = nsIWebProgressListener::STATE_IS_SECURE;

  MutexAutoLock lock(mMutex);

  // If we're here, the TLS handshake has succeeded. Thus if any of these
  // booleans are true, the user has added an override for a certificate error.
  if (mIsDomainMismatch || mIsUntrusted || mIsNotValidAtThisTime) {
    state |= nsIWebProgressListener::STATE_CERT_USER_OVERRIDDEN;
  }

  SetSecurityState(state);
  mHandshakeCompleted = true;
}

void QuicSocketControl::SetNegotiatedNPN(const nsACString& aValue) {
  MutexAutoLock lock(mMutex);
  mNegotiatedNPN = aValue;
  mNPNCompleted = true;
}

void QuicSocketControl::SetInfo(uint16_t aCipherSuite,
                                uint16_t aProtocolVersion, uint16_t aKeaGroup,
                                uint16_t aSignatureScheme) {
  SSLCipherSuiteInfo cipherInfo;
  if (SSL_GetCipherSuiteInfo(aCipherSuite, &cipherInfo, sizeof cipherInfo) ==
      SECSuccess) {
    MutexAutoLock lock(mMutex);
    mHaveCipherSuiteAndProtocol = true;
    mCipherSuite = aCipherSuite;
    mProtocolVersion = aProtocolVersion & 0xFF;
    mKeaGroup = getKeaGroupName(aKeaGroup);
    mSignatureSchemeName = getSignatureName(aSignatureScheme);
  }
}

NS_IMETHODIMP QuicSocketControl::GetPeerId(nsACString& aResult) {
  if (!mPeerId.IsEmpty()) {
    aResult.Assign(mPeerId);
    return NS_OK;
  }

  if (mProviderFlags &
      nsISocketProvider::ANONYMOUS_CONNECT) {  // See bug 466080
    mPeerId.AppendLiteral("anon:");
  }
  if (mProviderFlags & nsISocketProvider::NO_PERMANENT_STORAGE) {
    mPeerId.AppendLiteral("private:");
  }
  if (mProviderFlags & nsISocketProvider::BE_CONSERVATIVE) {
    mPeerId.AppendLiteral("beConservative:");
  }

  mPeerId.Append(GetHostName());
  mPeerId.Append(':');
  mPeerId.AppendInt(GetPort());
  nsAutoCString suffix;
  GetOriginAttributes().CreateSuffix(suffix);
  mPeerId.Append(suffix);

  aResult.Assign(mPeerId);
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
