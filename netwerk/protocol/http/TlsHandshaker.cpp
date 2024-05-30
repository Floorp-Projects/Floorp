/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "TlsHandshaker.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsHttpConnection.h"
#include "nsHttpConnectionInfo.h"
#include "nsHttpHandler.h"
#include "nsITLSSocketControl.h"

#define TLS_EARLY_DATA_NOT_AVAILABLE 0
#define TLS_EARLY_DATA_AVAILABLE_BUT_NOT_USED 1
#define TLS_EARLY_DATA_AVAILABLE_AND_USED 2

namespace mozilla::net {

NS_IMPL_ISUPPORTS(TlsHandshaker, nsITlsHandshakeCallbackListener)

TlsHandshaker::TlsHandshaker(nsHttpConnectionInfo* aInfo,
                             nsHttpConnection* aOwner)
    : mConnInfo(aInfo), mOwner(aOwner) {
  LOG(("TlsHandshaker ctor %p", this));
}

TlsHandshaker::~TlsHandshaker() { LOG(("TlsHandshaker dtor %p", this)); }

NS_IMETHODIMP
TlsHandshaker::CertVerificationDone() {
  LOG(("TlsHandshaker::CertVerificationDone mOwner=%p", mOwner.get()));
  if (mOwner) {
    Unused << mOwner->ResumeSend();
  }
  return NS_OK;
}

NS_IMETHODIMP
TlsHandshaker::ClientAuthCertificateSelected() {
  LOG(("TlsHandshaker::ClientAuthCertificateSelected mOwner=%p", mOwner.get()));
  if (mOwner) {
    Unused << mOwner->ResumeSend();
  }
  return NS_OK;
}

NS_IMETHODIMP
TlsHandshaker::HandshakeDone() {
  LOG(("TlsHandshaker::HandshakeDone mOwner=%p", mOwner.get()));
  if (mOwner) {
    mTlsHandshakeComplitionPending = true;

    // HandshakeDone needs to be dispatched so that it is not called inside
    // nss locks.
    RefPtr<TlsHandshaker> self(this);
    NS_DispatchToCurrentThread(NS_NewRunnableFunction(
        "TlsHandshaker::HandshakeDoneInternal", [self{std::move(self)}]() {
          if (self->mTlsHandshakeComplitionPending && self->mOwner) {
            self->mOwner->HandshakeDoneInternal();
            self->mTlsHandshakeComplitionPending = false;
          }
        }));
  }
  return NS_OK;
}

void TlsHandshaker::SetupSSL(bool aInSpdyTunnel, bool aForcePlainText) {
  if (!mOwner) {
    return;
  }

  LOG1(("TlsHandshaker::SetupSSL %p caps=0x%X %s\n", mOwner.get(),
        mOwner->TransactionCaps(), mConnInfo->HashKey().get()));

  if (mSetupSSLCalled) {  // do only once
    return;
  }
  mSetupSSLCalled = true;

  if (mNPNComplete) {
    return;
  }

  // we flip this back to false if SetNPNList succeeds at the end
  // of this function
  mNPNComplete = true;

  if (!mConnInfo->FirstHopSSL() || aForcePlainText) {
    return;
  }

  // if we are connected to the proxy with TLS, start the TLS
  // flow immediately without waiting for a CONNECT sequence.
  DebugOnly<nsresult> rv{};
  if (aInSpdyTunnel) {
    rv = InitSSLParams(false, true);
  } else {
    bool usingHttpsProxy = mConnInfo->UsingHttpsProxy();
    rv = InitSSLParams(usingHttpsProxy, usingHttpsProxy);
  }
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

nsresult TlsHandshaker::InitSSLParams(bool connectingToProxy,
                                      bool proxyStartSSL) {
  LOG(("TlsHandshaker::InitSSLParams [mOwner=%p] connectingToProxy=%d\n",
       mOwner.get(), connectingToProxy));
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (!mOwner) {
    return NS_ERROR_ABORT;
  }

  nsCOMPtr<nsITLSSocketControl> ssl;
  mOwner->GetTLSSocketControl(getter_AddRefs(ssl));
  if (!ssl) {
    return NS_ERROR_FAILURE;
  }

  // If proxy is use or 0RTT is excluded for a origin, don't use early-data.
  if (mConnInfo->UsingProxy() || gHttpHandler->Is0RttTcpExcluded(mConnInfo)) {
    ssl->DisableEarlyData();
  }

  if (proxyStartSSL) {
    nsresult rv = ssl->ProxyStartSSL();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (NS_SUCCEEDED(
          SetupNPNList(ssl, mOwner->TransactionCaps(), connectingToProxy)) &&
      NS_SUCCEEDED(ssl->SetHandshakeCallbackListener(this))) {
    LOG(("InitSSLParams Setting up SPDY Negotiation OK mOwner=%p",
         mOwner.get()));
    mNPNComplete = false;
  }

  return NS_OK;
}

// The naming of NPN is historical - this function creates the basic
// offer list for both NPN and ALPN. ALPN validation callbacks are made
// now before the handshake is complete, and NPN validation callbacks
// are made during the handshake.
nsresult TlsHandshaker::SetupNPNList(nsITLSSocketControl* ssl, uint32_t caps,
                                     bool connectingToProxy) {
  nsTArray<nsCString> protocolArray;

  // The first protocol is used as the fallback if none of the
  // protocols supported overlap with the server's list.
  // When using ALPN the advertised preferences are protocolArray indicies
  // {1, .., N, 0} in decreasing order.
  // For NPN, In the case of overlap, matching priority is driven by
  // the order of the server's advertisement - with index 0 used when
  // there is no match.
  protocolArray.AppendElement("http/1.1"_ns);

  if (StaticPrefs::network_http_http2_enabled() &&
      (connectingToProxy || !(caps & NS_HTTP_DISALLOW_SPDY)) &&
      !(connectingToProxy && (caps & NS_HTTP_DISALLOW_HTTP2_PROXY))) {
    LOG(("nsHttpConnection::SetupSSL Allow SPDY NPN selection"));
    const SpdyInformation* info = gHttpHandler->SpdyInfo();
    if (info->ALPNCallbacks(ssl)) {
      protocolArray.AppendElement(info->VersionString);
    }
  } else {
    LOG(("nsHttpConnection::SetupSSL Disallow SPDY NPN selection"));
  }

  nsresult rv = ssl->SetNPNList(protocolArray);
  LOG(("TlsHandshaker::SetupNPNList %p %" PRIx32 "\n", mOwner.get(),
       static_cast<uint32_t>(rv)));
  return rv;
}

// Checks if TLS handshake is needed and it is responsible to move it forward.
bool TlsHandshaker::EnsureNPNComplete() {
  if (!mOwner) {
    mNPNComplete = true;
    return true;
  }

  nsCOMPtr<nsISocketTransport> transport = mOwner->Transport();
  MOZ_ASSERT(transport);
  if (!transport) {
    // this cannot happen
    mNPNComplete = true;
    return true;
  }

  if (mNPNComplete) {
    return true;
  }

  if (mTlsHandshakeComplitionPending) {
    return false;
  }

  nsCOMPtr<nsITLSSocketControl> ssl;
  mOwner->GetTLSSocketControl(getter_AddRefs(ssl));
  if (!ssl) {
    FinishNPNSetup(false, false);
    return true;
  }

  if (!m0RTTChecked) {
    // We reuse m0RTTChecked. We want to send this status only once.
    RefPtr<nsAHttpTransaction> transaction = mOwner->Transaction();
    nsCOMPtr<nsISocketTransport> transport = mOwner->Transport();
    if (transaction && transport) {
      transaction->OnTransportStatus(transport,
                                     NS_NET_STATUS_TLS_HANDSHAKE_STARTING, 0);
    }
  }

  LOG(("TlsHandshaker::EnsureNPNComplete [mOwner=%p] drive TLS handshake",
       mOwner.get()));
  nsresult rv = ssl->DriveHandshake();
  if (NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK) {
    FinishNPNSetup(false, true);
    return true;
  }

  Check0RttEnabled(ssl);
  return false;
}

void TlsHandshaker::EarlyDataDone() {
  if (mEarlyDataState == EarlyData::USED) {
    mEarlyDataState = EarlyData::DONE_USED;
  } else if (mEarlyDataState == EarlyData::CANNOT_BE_USED) {
    mEarlyDataState = EarlyData::DONE_CANNOT_BE_USED;
  } else if (mEarlyDataState == EarlyData::NOT_AVAILABLE) {
    mEarlyDataState = EarlyData::DONE_NOT_AVAILABLE;
  }
}

void TlsHandshaker::FinishNPNSetup(bool handshakeSucceeded,
                                   bool hasSecurityInfo) {
  LOG(("TlsHandshaker::FinishNPNSetup mOwner=%p", mOwner.get()));
  mNPNComplete = true;

  mOwner->PostProcessNPNSetup(handshakeSucceeded, hasSecurityInfo,
                              EarlyDataUsed());
  EarlyDataDone();
}

void TlsHandshaker::Check0RttEnabled(nsITLSSocketControl* ssl) {
  if (!mOwner) {
    return;
  }

  if (m0RTTChecked) {
    return;
  }

  m0RTTChecked = true;

  if (mConnInfo->UsingProxy()) {
    return;
  }

  // There is no ALPN info (yet!). We need to consider doing 0RTT. We
  // will do so if there is ALPN information from a previous session
  // (AlpnEarlySelection), we are using HTTP/1, and the request data can
  // be safely retried.
  if (NS_FAILED(ssl->GetAlpnEarlySelection(mEarlyNegotiatedALPN))) {
    LOG1(
        ("TlsHandshaker::Check0RttEnabled %p - "
         "early selected alpn not available",
         mOwner.get()));
  } else {
    mOwner->ChangeConnectionState(ConnectionState::ZERORTT);
    LOG1(
        ("TlsHandshaker::Check0RttEnabled %p -"
         "early selected alpn: %s",
         mOwner.get(), mEarlyNegotiatedALPN.get()));
    const SpdyInformation* info = gHttpHandler->SpdyInfo();
    if (!mEarlyNegotiatedALPN.Equals(info->VersionString)) {
      // This is the HTTP/1 case.
      // Check if early-data is allowed for this transaction.
      RefPtr<nsAHttpTransaction> transaction = mOwner->Transaction();
      if (transaction && transaction->Do0RTT()) {
        LOG(
            ("TlsHandshaker::Check0RttEnabled [mOwner=%p] - We "
             "can do 0RTT (http/1)!",
             mOwner.get()));
        mEarlyDataState = EarlyData::USED;
      } else {
        mEarlyDataState = EarlyData::CANNOT_BE_USED;
        // Poll for read now. Polling for write will cause us to busy wait.
        // When the handshake is done the polling flags will be set correctly.
        Unused << mOwner->ResumeRecv();
      }
    } else {
      // We have h2, we can at least 0-RTT the preamble and opening
      // SETTINGS, etc, and maybe some of the first request
      LOG(
          ("TlsHandshaker::Check0RttEnabled [mOwner=%p] - Starting "
           "0RTT for h2!",
           mOwner.get()));
      mEarlyDataState = EarlyData::USED;
      mOwner->Start0RTTSpdy(info->Version);
    }
  }
}

void TlsHandshaker::EarlyDataTelemetry(int16_t tlsVersion,
                                       bool earlyDataAccepted,
                                       int64_t aContentBytesWritten0RTT) {
  // Send the 0RTT telemetry only for tls1.3
  if (tlsVersion > nsITLSSocketControl::TLS_VERSION_1_2) {
    Telemetry::Accumulate(Telemetry::TLS_EARLY_DATA_NEGOTIATED,
                          (mEarlyDataState == EarlyData::NOT_AVAILABLE)
                              ? TLS_EARLY_DATA_NOT_AVAILABLE
                              : ((mEarlyDataState == EarlyData::USED)
                                     ? TLS_EARLY_DATA_AVAILABLE_AND_USED
                                     : TLS_EARLY_DATA_AVAILABLE_BUT_NOT_USED));
    if (EarlyDataUsed()) {
      Telemetry::Accumulate(Telemetry::TLS_EARLY_DATA_ACCEPTED,
                            earlyDataAccepted);
    }
    if (earlyDataAccepted) {
      Telemetry::Accumulate(Telemetry::TLS_EARLY_DATA_BYTES_WRITTEN,
                            aContentBytesWritten0RTT);
    }
  }
}

}  // namespace mozilla::net
