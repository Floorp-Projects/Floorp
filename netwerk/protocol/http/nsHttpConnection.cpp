/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

// Log on level :5, instead of default :4.
#undef LOG
#define LOG(args) LOG5(args)
#undef LOG_ENABLED
#define LOG_ENABLED() LOG5_ENABLED()

#define TLS_EARLY_DATA_NOT_AVAILABLE 0
#define TLS_EARLY_DATA_AVAILABLE_BUT_NOT_USED 1
#define TLS_EARLY_DATA_AVAILABLE_AND_USED 2

#include "ASpdySession.h"
#include "mozilla/ChaosMode.h"
#include "mozilla/Telemetry.h"
#include "nsHttpConnection.h"
#include "nsHttpHandler.h"
#include "nsHttpRequestHead.h"
#include "nsHttpResponseHead.h"
#include "nsIClassOfService.h"
#include "nsIOService.h"
#include "nsISocketTransport.h"
#include "nsSocketTransportService2.h"
#include "nsISSLSocketControl.h"
#include "nsISupportsPriority.h"
#include "nsITransportSecurityInfo.h"
#include "nsCRT.h"
#include "nsPreloadedStream.h"
#include "nsProxyRelease.h"
#include "nsSocketTransport2.h"
#include "nsStringStream.h"
#include "nsITransportSecurityInfo.h"
#include "mozpkix/pkixnss.h"
#include "sslt.h"
#include "NSSErrorsService.h"
#include "TunnelUtils.h"

namespace mozilla {
namespace net {

enum TlsHandshakeResult : uint32_t {
  EchConfigSuccessful = 0,
  EchConfigFailed,
  NoEchConfigSuccessful,
  NoEchConfigFailed,
};

//-----------------------------------------------------------------------------
// nsHttpConnection <public>
//-----------------------------------------------------------------------------

nsHttpConnection::nsHttpConnection()
    : mSocketInCondition(NS_ERROR_NOT_INITIALIZED),
      mSocketOutCondition(NS_ERROR_NOT_INITIALIZED),
      mHttpHandler(gHttpHandler),
      mLastReadTime(0),
      mLastWriteTime(0),
      mMaxHangTime(0),
      mConsiderReusedAfterInterval(0),
      mConsiderReusedAfterEpoch(0),
      mCurrentBytesRead(0),
      mMaxBytesRead(0),
      mTotalBytesRead(0),
      mContentBytesWritten(0),
      mUrgentStartPreferred(false),
      mUrgentStartPreferredKnown(false),
      mConnectedTransport(false),
      mKeepAlive(true)  // assume to keep-alive by default
      ,
      mKeepAliveMask(true),
      mDontReuse(false),
      mIsReused(false),
      mCompletedProxyConnect(false),
      mLastTransactionExpectedNoContent(false),
      mIdleMonitoring(false),
      mProxyConnectInProgress(false),
      mInSpdyTunnel(false),
      mForcePlainText(false),
      mTrafficCount(0),
      mTrafficStamp(false),
      mHttp1xTransactionCount(0),
      mRemainingConnectionUses(0xffffffff),
      mNPNComplete(false),
      mSetupSSLCalled(false),
      mUsingSpdyVersion(SpdyVersion::NONE),
      mPriority(nsISupportsPriority::PRIORITY_NORMAL),
      mReportedSpdy(false),
      mEverUsedSpdy(false),
      mLastHttpResponseVersion(HttpVersion::v1_1),
      mDefaultTimeoutFactor(1),
      mResponseTimeoutEnabled(false),
      mTCPKeepaliveConfig(kTCPKeepaliveDisabled),
      mForceSendPending(false),
      m0RTTChecked(false),
      mWaitingFor0RTTResponse(false),
      mContentBytesWritten0RTT(0),
      mEarlyDataNegotiated(false),
      mDid0RTTSpdy(false) {
  LOG(("Creating nsHttpConnection @%p\n", this));

  // the default timeout is for when this connection has not yet processed a
  // transaction
  static const PRIntervalTime k5Sec = PR_SecondsToInterval(5);
  mIdleTimeout = (k5Sec < gHttpHandler->IdleTimeout())
                     ? k5Sec
                     : gHttpHandler->IdleTimeout();

  mThroughCaptivePortal = gHttpHandler->GetThroughCaptivePortal();
}

nsHttpConnection::~nsHttpConnection() {
  LOG(("Destroying nsHttpConnection @%p\n", this));

  if (!mEverUsedSpdy) {
    LOG(("nsHttpConnection %p performed %d HTTP/1.x transactions\n", this,
         mHttp1xTransactionCount));
    Telemetry::Accumulate(Telemetry::HTTP_REQUEST_PER_CONN,
                          mHttp1xTransactionCount);
    nsHttpConnectionInfo* ci = nullptr;
    if (mTransaction) {
      ci = mTransaction->ConnectionInfo();
    }
    if (!ci) {
      ci = mConnInfo;
    }

    MOZ_ASSERT(ci);
    if (ci->GetIsTrrServiceChannel()) {
      Telemetry::Accumulate(Telemetry::DNS_TRR_REQUEST_PER_CONN,
                            mHttp1xTransactionCount);
    }
  }

  if (mTotalBytesRead) {
    uint32_t totalKBRead = static_cast<uint32_t>(mTotalBytesRead >> 10);
    LOG(("nsHttpConnection %p read %dkb on connection spdy=%d\n", this,
         totalKBRead, mEverUsedSpdy));
    Telemetry::Accumulate(mEverUsedSpdy ? Telemetry::SPDY_KBREAD_PER_CONN2
                                        : Telemetry::HTTP_KBREAD_PER_CONN2,
                          totalKBRead);
  }

  if (mThroughCaptivePortal) {
    if (mTotalBytesRead || mTotalBytesWritten) {
      auto total =
          Clamp<uint32_t>((mTotalBytesRead >> 10) + (mTotalBytesWritten >> 10),
                          0, std::numeric_limits<uint32_t>::max());
      Telemetry::ScalarAdd(
          Telemetry::ScalarID::NETWORKING_DATA_TRANSFERRED_CAPTIVE_PORTAL,
          total);
    }

    Telemetry::ScalarAdd(
        Telemetry::ScalarID::NETWORKING_HTTP_CONNECTIONS_CAPTIVE_PORTAL, 1);
  }

  if (mForceSendTimer) {
    mForceSendTimer->Cancel();
    mForceSendTimer = nullptr;
  }
}

nsresult nsHttpConnection::Init(
    nsHttpConnectionInfo* info, uint16_t maxHangTime,
    nsISocketTransport* transport, nsIAsyncInputStream* instream,
    nsIAsyncOutputStream* outstream, bool connectedTransport, nsresult status,
    nsIInterfaceRequestor* callbacks, PRIntervalTime rtt) {
  LOG1(("nsHttpConnection::Init this=%p sockettransport=%p", this, transport));
  NS_ENSURE_ARG_POINTER(info);
  NS_ENSURE_TRUE(!mConnInfo, NS_ERROR_ALREADY_INITIALIZED);
  MOZ_ASSERT(NS_SUCCEEDED(status) || !connectedTransport);

  mConnectedTransport = connectedTransport;
  mConnInfo = info;
  MOZ_ASSERT(mConnInfo);

  mLastWriteTime = mLastReadTime = PR_IntervalNow();
  mRtt = rtt;
  mMaxHangTime = PR_SecondsToInterval(maxHangTime);

  mSocketTransport = transport;
  mSocketIn = instream;
  mSocketOut = outstream;

  // See explanation for non-strictness of this operation in
  // SetSecurityCallbacks.
  mCallbacks = new nsMainThreadPtrHolder<nsIInterfaceRequestor>(
      "nsHttpConnection::mCallbacks", callbacks, false);

  mErrorBeforeConnect = status;
  if (NS_SUCCEEDED(mErrorBeforeConnect)) {
    mSocketTransport->SetEventSink(this, nullptr);
    mSocketTransport->SetSecurityCallbacks(this);
  }

  return NS_OK;
}

nsresult nsHttpConnection::TryTakeSubTransactions(
    nsTArray<RefPtr<nsAHttpTransaction> >& list) {
  nsresult rv = mTransaction->TakeSubTransactions(list);

  if (rv == NS_ERROR_ALREADY_OPENED) {
    // Has the interface for TakeSubTransactions() changed?
    LOG(
        ("TakeSubTransactions somehow called after "
         "nsAHttpTransaction began processing\n"));
    MOZ_ASSERT(false,
               "TakeSubTransactions somehow called after "
               "nsAHttpTransaction began processing");
    mTransaction->Close(NS_ERROR_ABORT);
    return rv;
  }

  if (NS_FAILED(rv) && rv != NS_ERROR_NOT_IMPLEMENTED) {
    // Has the interface for TakeSubTransactions() changed?
    LOG(("unexpected rv from nnsAHttpTransaction::TakeSubTransactions()"));
    MOZ_ASSERT(false,
               "unexpected result from "
               "nsAHttpTransaction::TakeSubTransactions()");
    mTransaction->Close(NS_ERROR_ABORT);
    return rv;
  }

  return rv;
}

nsresult nsHttpConnection::MoveTransactionsToSpdy(
    nsresult status, nsTArray<RefPtr<nsAHttpTransaction> >& list) {
  if (NS_FAILED(status)) {  // includes NS_ERROR_NOT_IMPLEMENTED
    MOZ_ASSERT(list.IsEmpty(), "sub transaction list not empty");

    // This is ok - treat mTransaction as a single real request.
    // Wrap the old http transaction into the new spdy session
    // as the first stream.
    LOG(
        ("nsHttpConnection::MoveTransactionsToSpdy moves single transaction %p "
         "into SpdySession %p\n",
         mTransaction.get(), mSpdySession.get()));
    nsresult rv = AddTransaction(mTransaction, mPriority);
    if (NS_FAILED(rv)) {
      return rv;
    }
  } else {
    int32_t count = list.Length();

    LOG(
        ("nsHttpConnection::MoveTransactionsToSpdy moving transaction list "
         "len=%d "
         "into SpdySession %p\n",
         count, mSpdySession.get()));

    if (!count) {
      mTransaction->Close(NS_ERROR_ABORT);
      return NS_ERROR_ABORT;
    }

    for (int32_t index = 0; index < count; ++index) {
      nsresult rv = AddTransaction(list[index], mPriority);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  return NS_OK;
}

void nsHttpConnection::Start0RTTSpdy(SpdyVersion spdyVersion) {
  LOG(("nsHttpConnection::Start0RTTSpdy [this=%p]", this));

  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  mDid0RTTSpdy = true;
  mUsingSpdyVersion = spdyVersion;
  mSpdySession =
      ASpdySession::NewSpdySession(spdyVersion, mSocketTransport, true);

  nsTArray<RefPtr<nsAHttpTransaction> > list;
  nsresult rv = TryTakeSubTransactions(list);
  if (NS_FAILED(rv) && rv != NS_ERROR_NOT_IMPLEMENTED) {
    LOG(
        ("nsHttpConnection::Start0RTTSpdy [this=%p] failed taking "
         "subtransactions rv=%" PRIx32,
         this, static_cast<uint32_t>(rv)));
    return;
  }

  rv = MoveTransactionsToSpdy(rv, list);
  if (NS_FAILED(rv)) {
    LOG(
        ("nsHttpConnection::Start0RTTSpdy [this=%p] failed moving "
         "transactions rv=%" PRIx32,
         this, static_cast<uint32_t>(rv)));
    return;
  }

  mTransaction = mSpdySession;
}

void nsHttpConnection::StartSpdy(nsISSLSocketControl* sslControl,
                                 SpdyVersion spdyVersion) {
  LOG(("nsHttpConnection::StartSpdy [this=%p, mDid0RTTSpdy=%d]\n", this,
       mDid0RTTSpdy));

  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(!mSpdySession || mDid0RTTSpdy);

  mUsingSpdyVersion = spdyVersion;
  mEverUsedSpdy = true;
  if (sslControl) {
    sslControl->SetDenyClientCert(true);
  }

  if (!mDid0RTTSpdy) {
    mSpdySession =
        ASpdySession::NewSpdySession(spdyVersion, mSocketTransport, false);
  }

  if (!mReportedSpdy) {
    mReportedSpdy = true;
    gHttpHandler->ConnMgr()->ReportSpdyConnection(this, true);
  }

  // Setting the connection as reused allows some transactions that fail
  // with NS_ERROR_NET_RESET to be restarted and SPDY uses that code
  // to handle clean rejections (such as those that arrived after
  // a server goaway was generated).
  mIsReused = true;

  // If mTransaction is a muxed object it might represent
  // several requests. If so, we need to unpack that and
  // pack them all into a new spdy session.

  nsTArray<RefPtr<nsAHttpTransaction> > list;
  nsresult status = NS_OK;
  if (!mDid0RTTSpdy) {
    status = TryTakeSubTransactions(list);

    if (NS_FAILED(status) && status != NS_ERROR_NOT_IMPLEMENTED) {
      return;
    }
  }

  if (NeedSpdyTunnel()) {
    LOG3(
        ("nsHttpConnection::StartSpdy %p Connecting To a HTTP/2 "
         "Proxy and Need Connect",
         this));
    MOZ_ASSERT(mProxyConnectStream);

    mProxyConnectStream = nullptr;
    mCompletedProxyConnect = true;
    mProxyConnectInProgress = false;
  }

  nsresult rv = NS_OK;
  bool spdyProxy = mConnInfo->UsingHttpsProxy() && !mTLSFilter;
  if (spdyProxy) {
    RefPtr<nsHttpConnectionInfo> wildCardProxyCi;
    rv = mConnInfo->CreateWildCard(getter_AddRefs(wildCardProxyCi));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    gHttpHandler->ConnMgr()->MoveToWildCardConnEntry(mConnInfo, wildCardProxyCi,
                                                     this);
    mConnInfo = wildCardProxyCi;
    MOZ_ASSERT(mConnInfo);
  }

  if (!mDid0RTTSpdy) {
    rv = MoveTransactionsToSpdy(status, list);
    if (NS_FAILED(rv)) {
      return;
    }
  }

  // Disable TCP Keepalives - use SPDY ping instead.
  rv = DisableTCPKeepalives();
  if (NS_FAILED(rv)) {
    LOG(
        ("nsHttpConnection::StartSpdy [%p] DisableTCPKeepalives failed "
         "rv[0x%" PRIx32 "]",
         this, static_cast<uint32_t>(rv)));
  }

  mIdleTimeout = gHttpHandler->SpdyTimeout() * mDefaultTimeoutFactor;

  if (!mTLSFilter) {
    mTransaction = mSpdySession;
  } else {
    rv = mTLSFilter->SetProxiedTransaction(mSpdySession);
    if (NS_FAILED(rv)) {
      LOG(
          ("nsHttpConnection::StartSpdy [%p] SetProxiedTransaction failed"
           " rv[0x%x]",
           this, static_cast<uint32_t>(rv)));
    }
  }
  if (mDontReuse) {
    mSpdySession->DontReuse();
  }
}

bool nsHttpConnection::EnsureNPNComplete(nsresult& aOut0RTTWriteHandshakeValue,
                                         uint32_t& aOut0RTTBytesWritten) {
  // If for some reason the components to check on NPN aren't available,
  // this function will just return true to continue on and disable SPDY

  aOut0RTTWriteHandshakeValue = NS_OK;
  aOut0RTTBytesWritten = 0;

  MOZ_ASSERT(mSocketTransport);
  if (!mSocketTransport) {
    // this cannot happen
    mNPNComplete = true;
    return true;
  }

  if (mNPNComplete) {
    return true;
  }

  nsresult rv = NS_OK;
  nsCOMPtr<nsISupports> securityInfo;
  nsCOMPtr<nsITransportSecurityInfo> info;
  nsCOMPtr<nsISSLSocketControl> ssl;
  nsAutoCString negotiatedNPN;
  // This is needed for telemetry
  bool handshakeSucceeded = false;

  GetSecurityInfo(getter_AddRefs(securityInfo));
  if (!securityInfo) {
    goto npnComplete;
  }

  ssl = do_QueryInterface(securityInfo, &rv);
  if (NS_FAILED(rv)) goto npnComplete;

  info = do_QueryInterface(securityInfo, &rv);
  if (NS_FAILED(rv)) goto npnComplete;

  if (!m0RTTChecked) {
    // We reuse m0RTTChecked. We want to send this status only once.
    mTransaction->OnTransportStatus(mSocketTransport,
                                    NS_NET_STATUS_TLS_HANDSHAKE_STARTING, 0);
  }

  rv = info->GetNegotiatedNPN(negotiatedNPN);
  if (!m0RTTChecked && (rv == NS_ERROR_NOT_CONNECTED) &&
      !mConnInfo->UsingProxy()) {
    // There is no ALPN info (yet!). We need to consider doing 0RTT. We
    // will do so if there is ALPN information from a previous session
    // (AlpnEarlySelection), we are using HTTP/1, and the request data can
    // be safely retried.
    m0RTTChecked = true;
    nsresult rvEarlyAlpn = ssl->GetAlpnEarlySelection(mEarlyNegotiatedALPN);
    if (NS_FAILED(rvEarlyAlpn)) {
      // if ssl->DriveHandshake() has never been called the value
      // for AlpnEarlySelection is still not set. So call it here and
      // check again.
      LOG1(
          ("nsHttpConnection::EnsureNPNComplete %p - "
           "early selected alpn not available, we will try one more time.",
           this));
      // Let's do DriveHandshake again.
      rv = ssl->DriveHandshake();
      if (NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK) {
        goto npnComplete;
      }

      // Check NegotiatedNPN first.
      rv = info->GetNegotiatedNPN(negotiatedNPN);
      if (rv == NS_ERROR_NOT_CONNECTED) {
        rvEarlyAlpn = ssl->GetAlpnEarlySelection(mEarlyNegotiatedALPN);
      }
    }

    if (NS_FAILED(rvEarlyAlpn)) {
      LOG1(
          ("nsHttpConnection::EnsureNPNComplete %p - "
           "early selected alpn not available",
           this));
      mEarlyDataNegotiated = false;
    } else {
      LOG1(
          ("nsHttpConnection::EnsureNPNComplete %p -"
           "early selected alpn: %s",
           this, mEarlyNegotiatedALPN.get()));
      uint32_t infoIndex;
      const SpdyInformation* info = gHttpHandler->SpdyInfo();
      if (NS_FAILED(info->GetNPNIndex(mEarlyNegotiatedALPN, &infoIndex))) {
        // This is the HTTP/1 case.
        // Check if early-data is allowed for this transaction.
        if (mTransaction->Do0RTT()) {
          LOG(
              ("nsHttpConnection::EnsureNPNComplete [this=%p] - We "
               "can do 0RTT (http/1)!",
               this));
          mWaitingFor0RTTResponse = true;
        }
      } else {
        // We have h2, we can at least 0-RTT the preamble and opening
        // SETTINGS, etc, and maybe some of the first request
        LOG(
            ("nsHttpConnection::EnsureNPNComplete [this=%p] - Starting "
             "0RTT for h2!",
             this));
        mWaitingFor0RTTResponse = true;
        Start0RTTSpdy(info->Version[infoIndex]);
      }
      mEarlyDataNegotiated = true;
    }
  }

  if (rv == NS_ERROR_NOT_CONNECTED) {
    if (mWaitingFor0RTTResponse) {
      aOut0RTTWriteHandshakeValue = mTransaction->ReadSegments(
          this, nsIOService::gDefaultSegmentSize, &aOut0RTTBytesWritten);
      if (NS_FAILED(aOut0RTTWriteHandshakeValue) &&
          aOut0RTTWriteHandshakeValue != NS_BASE_STREAM_WOULD_BLOCK) {
        goto npnComplete;
      }
      LOG(
          ("nsHttpConnection::EnsureNPNComplete [this=%p] - written %d "
           "bytes during 0RTT",
           this, aOut0RTTBytesWritten));
      mContentBytesWritten0RTT += aOut0RTTBytesWritten;
    }

    rv = ssl->DriveHandshake();
    if (NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK) {
      goto npnComplete;
    }

    return false;
  }

  if (NS_SUCCEEDED(rv)) {
    LOG1(("nsHttpConnection::EnsureNPNComplete %p [%s] negotiated to '%s'%s\n",
          this, mConnInfo->HashKey().get(), negotiatedNPN.get(),
          mTLSFilter ? " [Double Tunnel]" : ""));

    handshakeSucceeded = true;

    int16_t tlsVersion;
    ssl->GetSSLVersionUsed(&tlsVersion);
    mConnInfo->SetLessThanTls13(
        (tlsVersion < nsISSLSocketControl::TLS_VERSION_1_3) &&
        (tlsVersion != nsISSLSocketControl::SSL_VERSION_UNKNOWN));

    bool earlyDataAccepted = false;
    if (mWaitingFor0RTTResponse) {
      // Check if early data has been accepted.
      nsresult rvEarlyData = ssl->GetEarlyDataAccepted(&earlyDataAccepted);
      LOG(
          ("nsHttpConnection::EnsureNPNComplete [this=%p] - early data "
           "that was sent during 0RTT %s been accepted [rv=%" PRIx32 "].",
           this, earlyDataAccepted ? "has" : "has not",
           static_cast<uint32_t>(rv)));

      if (NS_FAILED(rvEarlyData) ||
          NS_FAILED(mTransaction->Finish0RTT(
              !earlyDataAccepted, negotiatedNPN != mEarlyNegotiatedALPN))) {
        LOG(
            ("nsHttpConection::EnsureNPNComplete [this=%p] closing transaction "
             "%p",
             this, mTransaction.get()));
        mTransaction->Close(NS_ERROR_NET_RESET);
        goto npnComplete;
      }
    }

    // Send the 0RTT telemetry only for tls1.3
    if (tlsVersion > nsISSLSocketControl::TLS_VERSION_1_2) {
      Telemetry::Accumulate(
          Telemetry::TLS_EARLY_DATA_NEGOTIATED,
          (!mEarlyDataNegotiated)
              ? TLS_EARLY_DATA_NOT_AVAILABLE
              : ((mWaitingFor0RTTResponse)
                     ? TLS_EARLY_DATA_AVAILABLE_AND_USED
                     : TLS_EARLY_DATA_AVAILABLE_BUT_NOT_USED));
      if (mWaitingFor0RTTResponse) {
        Telemetry::Accumulate(Telemetry::TLS_EARLY_DATA_ACCEPTED,
                              earlyDataAccepted);
      }
      if (earlyDataAccepted) {
        Telemetry::Accumulate(Telemetry::TLS_EARLY_DATA_BYTES_WRITTEN,
                              mContentBytesWritten0RTT);
      }
    }
    mWaitingFor0RTTResponse = false;

    if (!earlyDataAccepted) {
      LOG(
          ("nsHttpConnection::EnsureNPNComplete [this=%p] early data not "
           "accepted",
           this));
      if (mTransaction->QueryNullTransaction() &&
          (mBootstrappedTimings.secureConnectionStart.IsNull() ||
           mBootstrappedTimings.tcpConnectEnd.IsNull())) {
        mBootstrappedTimings.secureConnectionStart =
            mTransaction->QueryNullTransaction()->GetSecureConnectionStart();
        mBootstrappedTimings.tcpConnectEnd =
            mTransaction->QueryNullTransaction()->GetTcpConnectEnd();
      }
      uint32_t infoIndex;
      const SpdyInformation* info = gHttpHandler->SpdyInfo();
      if (NS_SUCCEEDED(info->GetNPNIndex(negotiatedNPN, &infoIndex))) {
        StartSpdy(ssl, info->Version[infoIndex]);
      }
    } else {
      LOG(("nsHttpConnection::EnsureNPNComplete [this=%p] - %" PRId64 " bytes "
           "has been sent during 0RTT.",
           this, mContentBytesWritten0RTT));
      mContentBytesWritten = mContentBytesWritten0RTT;
      if (mSpdySession) {
        // We had already started 0RTT-spdy, now we need to fully set up
        // spdy, since we know we're sticking with it.
        LOG(
            ("nsHttpConnection::EnsureNPNComplete [this=%p] - finishing "
             "StartSpdy for 0rtt spdy session %p",
             this, mSpdySession.get()));
        StartSpdy(ssl, mSpdySession->SpdyVersion());
      }
    }

    Telemetry::Accumulate(Telemetry::SPDY_NPN_CONNECT, UsingSpdy());
  }

npnComplete:
  LOG(("nsHttpConnection::EnsureNPNComplete [this=%p] setting complete to true",
       this));
  mNPNComplete = true;

  mTransaction->OnTransportStatus(mSocketTransport,
                                  NS_NET_STATUS_TLS_HANDSHAKE_ENDED, 0);

  // this is happening after the bootstrap was originally written to. so update
  // it.
  if (mTransaction->QueryNullTransaction() &&
      (mBootstrappedTimings.secureConnectionStart.IsNull() ||
       mBootstrappedTimings.tcpConnectEnd.IsNull())) {
    mBootstrappedTimings.secureConnectionStart =
        mTransaction->QueryNullTransaction()->GetSecureConnectionStart();
    mBootstrappedTimings.tcpConnectEnd =
        mTransaction->QueryNullTransaction()->GetTcpConnectEnd();
  }

  if (securityInfo) {
    mBootstrappedTimings.connectEnd = TimeStamp::Now();
  }

  if (mWaitingFor0RTTResponse) {
    // Didn't get 0RTT OK, back out of the "attempting 0RTT" state
    mWaitingFor0RTTResponse = false;
    LOG(("nsHttpConnection::EnsureNPNComplete [this=%p] 0rtt failed", this));
    if (NS_FAILED(mTransaction->Finish0RTT(
            true, negotiatedNPN != mEarlyNegotiatedALPN))) {
      mTransaction->Close(NS_ERROR_NET_RESET);
    }
    mContentBytesWritten0RTT = 0;
  }

  if (mDid0RTTSpdy && negotiatedNPN != mEarlyNegotiatedALPN) {
    // Reset the work done by Start0RTTSpdy
    LOG((
        "nsHttpConnection::EnsureNPNComplete [this=%p] resetting Start0RTTSpdy",
        this));
    mUsingSpdyVersion = SpdyVersion::NONE;
    mTransaction = nullptr;
    mSpdySession = nullptr;
    // We have to reset this here, just in case we end up starting spdy again,
    // so it can actually do everything it needs to do.
    mDid0RTTSpdy = false;
  }

  if (ssl) {
    // Telemetry for tls failure rate with and without esni;
    bool echConfigUsed = false;
    mSocketTransport->GetEchConfigUsed(&echConfigUsed);
    TlsHandshakeResult result =
        echConfigUsed
            ? (handshakeSucceeded ? TlsHandshakeResult::EchConfigSuccessful
                                  : TlsHandshakeResult::EchConfigFailed)
            : (handshakeSucceeded ? TlsHandshakeResult::NoEchConfigSuccessful
                                  : TlsHandshakeResult::NoEchConfigFailed);
    Telemetry::Accumulate(Telemetry::ECHCONFIG_SUCCESS_RATE, result);
  }

  if (rv == psm::GetXPCOMFromNSSError(
                mozilla::pkix::MOZILLA_PKIX_ERROR_MITM_DETECTED)) {
    gSocketTransportService->SetNotTrustedMitmDetected();
  }
  return true;
}

nsresult nsHttpConnection::OnTunnelNudged(TLSFilterTransaction* trans) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("nsHttpConnection::OnTunnelNudged %p\n", this));
  if (trans != mTLSFilter) {
    return NS_OK;
  }
  LOG(("nsHttpConnection::OnTunnelNudged %p Calling OnSocketWritable\n", this));
  return OnSocketWritable();
}

// called on the socket thread
nsresult nsHttpConnection::Activate(nsAHttpTransaction* trans, uint32_t caps,
                                    int32_t pri) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG1(("nsHttpConnection::Activate [this=%p trans=%p caps=%x]\n", this, trans,
        caps));

  if (!mExperienced && !trans->IsNullTransaction()) {
    if (mNPNComplete) {
      mExperienced = true;
    }
    if (mBootstrappedTimingsSet) {
      mBootstrappedTimingsSet = false;
      nsHttpTransaction* hTrans = trans->QueryHttpTransaction();
      if (hTrans) {
        hTrans->BootstrapTimings(mBootstrappedTimings);
        SetUrgentStartPreferred(hTrans->ClassOfService() &
                                nsIClassOfService::UrgentStart);
      }
    }
    mBootstrappedTimings = TimingStruct();
  }

  if (caps & NS_HTTP_LARGE_KEEPALIVE) {
    mDefaultTimeoutFactor = 10;  // don't ever lower
  }

  mTransactionCaps = caps;
  mPriority = pri;
  if (mTransaction && (mUsingSpdyVersion != SpdyVersion::NONE)) {
    return AddTransaction(trans, pri);
  }

  NS_ENSURE_ARG_POINTER(trans);
  NS_ENSURE_TRUE(!mTransaction, NS_ERROR_IN_PROGRESS);

  // reset the read timers to wash away any idle time
  mLastWriteTime = mLastReadTime = PR_IntervalNow();

  // Connection failures are Activated() just like regular transacions.
  // If we don't have a confirmation of a connected socket then test it
  // with a write() to get relevant error code.
  if (NS_FAILED(mErrorBeforeConnect)) {
    mSocketOutCondition = mErrorBeforeConnect;
    mTransaction = trans;
    CloseTransaction(mTransaction, mSocketOutCondition);
    return mSocketOutCondition;
  }

  if (!mConnectedTransport) {
    uint32_t count;
    mSocketOutCondition = NS_ERROR_FAILURE;
    if (mSocketOut) {
      mSocketOutCondition = mSocketOut->Write("", 0, &count);
    }
    if (NS_FAILED(mSocketOutCondition) &&
        mSocketOutCondition != NS_BASE_STREAM_WOULD_BLOCK) {
      LOG(("nsHttpConnection::Activate [this=%p] Bad Socket %" PRIx32 "\n",
           this, static_cast<uint32_t>(mSocketOutCondition)));
      mSocketOut->AsyncWait(nullptr, 0, 0, nullptr);
      mTransaction = trans;
      CloseTransaction(mTransaction, mSocketOutCondition);
      return mSocketOutCondition;
    }
  }

  // Update security callbacks
  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  trans->GetSecurityCallbacks(getter_AddRefs(callbacks));
  SetSecurityCallbacks(callbacks);
  SetupSSL();

  // take ownership of the transaction
  mTransaction = trans;

  MOZ_ASSERT(!mIdleMonitoring, "Activating a connection with an Idle Monitor");
  mIdleMonitoring = false;

  // set mKeepAlive according to what will be requested
  mKeepAliveMask = mKeepAlive = (caps & NS_HTTP_ALLOW_KEEPALIVE);

  // need to handle HTTP CONNECT tunnels if this is the first time if
  // we are tunneling through a proxy
  nsresult rv = NS_OK;
  if (mTransaction->ConnectionInfo()->UsingConnect() &&
      !mCompletedProxyConnect) {
    rv = SetupProxyConnect();
    if (NS_FAILED(rv)) goto failed_activation;
    mProxyConnectInProgress = true;
  }

  // Clear the per activation counter
  mCurrentBytesRead = 0;

  // The overflow state is not needed between activations
  mInputOverflow = nullptr;

  mResponseTimeoutEnabled = gHttpHandler->ResponseTimeoutEnabled() &&
                            mTransaction->ResponseTimeout() > 0 &&
                            mTransaction->ResponseTimeoutEnabled();

  rv = StartShortLivedTCPKeepalives();
  if (NS_FAILED(rv)) {
    LOG(
        ("nsHttpConnection::Activate [%p] "
         "StartShortLivedTCPKeepalives failed rv[0x%" PRIx32 "]",
         this, static_cast<uint32_t>(rv)));
  }

  if (mTLSFilter) {
    RefPtr<NullHttpTransaction> baseTrans(do_QueryReferent(mWeakTrans));
    rv = mTLSFilter->SetProxiedTransaction(trans, baseTrans);
    NS_ENSURE_SUCCESS(rv, rv);
    if (mTransaction->ConnectionInfo()->UsingConnect()) {
      SpdyConnectTransaction* trans =
          baseTrans ? baseTrans->QuerySpdyConnectTransaction() : nullptr;
      if (trans && !trans->IsWebsocket()) {
        // If we are here, the tunnel is already established. Let the
        // transaction know that proxy connect is successful.
        mTransaction->OnProxyConnectComplete(200);
      }
    }
    mTransaction = mTLSFilter;
  }

  trans->OnActivated();

  rv = OnOutputStreamReady(mSocketOut);

failed_activation:
  if (NS_FAILED(rv)) {
    mTransaction = nullptr;
  }

  return rv;
}

void nsHttpConnection::SetupSSL() {
  LOG1(("nsHttpConnection::SetupSSL %p caps=0x%X %s\n", this, mTransactionCaps,
        mConnInfo->HashKey().get()));

  if (mSetupSSLCalled)  // do only once
    return;
  mSetupSSLCalled = true;

  if (mNPNComplete) return;

  // we flip this back to false if SetNPNList succeeds at the end
  // of this function
  mNPNComplete = true;

  if (!mConnInfo->FirstHopSSL() || mForcePlainText) {
    return;
  }

  // if we are connected to the proxy with TLS, start the TLS
  // flow immediately without waiting for a CONNECT sequence.
  DebugOnly<nsresult> rv;
  if (mInSpdyTunnel) {
    rv = InitSSLParams(false, true);
  } else {
    bool usingHttpsProxy = mConnInfo->UsingHttpsProxy();
    rv = InitSSLParams(usingHttpsProxy, usingHttpsProxy);
  }
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

// The naming of NPN is historical - this function creates the basic
// offer list for both NPN and ALPN. ALPN validation callbacks are made
// now before the handshake is complete, and NPN validation callbacks
// are made during the handshake.
nsresult nsHttpConnection::SetupNPNList(nsISSLSocketControl* ssl,
                                        uint32_t caps) {
  nsTArray<nsCString> protocolArray;

  nsCString npnToken = mConnInfo->GetNPNToken();
  if (npnToken.IsEmpty()) {
    // The first protocol is used as the fallback if none of the
    // protocols supported overlap with the server's list.
    // When using ALPN the advertised preferences are protocolArray indicies
    // {1, .., N, 0} in decreasing order.
    // For NPN, In the case of overlap, matching priority is driven by
    // the order of the server's advertisement - with index 0 used when
    // there is no match.
    protocolArray.AppendElement("http/1.1"_ns);

    if (gHttpHandler->IsSpdyEnabled() && !(caps & NS_HTTP_DISALLOW_SPDY)) {
      LOG(("nsHttpConnection::SetupSSL Allow SPDY NPN selection"));
      const SpdyInformation* info = gHttpHandler->SpdyInfo();
      for (uint32_t index = SpdyInformation::kCount; index > 0; --index) {
        if (info->ProtocolEnabled(index - 1) &&
            info->ALPNCallbacks[index - 1](ssl)) {
          protocolArray.AppendElement(info->VersionString[index - 1]);
        }
      }
    }
  } else {
    LOG(("nsHttpConnection::SetupSSL limiting NPN selection to %s",
         npnToken.get()));
    protocolArray.AppendElement(npnToken);
  }

  nsresult rv = ssl->SetNPNList(protocolArray);
  LOG(("nsHttpConnection::SetupNPNList %p %" PRIx32 "\n", this,
       static_cast<uint32_t>(rv)));
  return rv;
}

nsresult nsHttpConnection::AddTransaction(nsAHttpTransaction* httpTransaction,
                                          int32_t priority) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(mSpdySession && (mUsingSpdyVersion != SpdyVersion::NONE),
             "AddTransaction to live http connection without spdy/quic");

  // If this is a wild card nshttpconnection (i.e. a spdy proxy) then
  // it is important to start the stream using the specific connection
  // info of the transaction to ensure it is routed on the right tunnel

  nsHttpConnectionInfo* transCI = httpTransaction->ConnectionInfo();

  bool needTunnel = transCI->UsingHttpsProxy();
  needTunnel = needTunnel && !mTLSFilter;
  needTunnel = needTunnel && transCI->UsingConnect();
  needTunnel = needTunnel && httpTransaction->QueryHttpTransaction();

  // Let the transaction know that the tunnel is already established and we
  // don't need to setup the tunnel again.
  if (transCI->UsingConnect() && mEverUsedSpdy && mTLSFilter) {
    httpTransaction->OnProxyConnectComplete(200);
  }

  bool isWebsocket = false;
  nsHttpTransaction* trans = httpTransaction->QueryHttpTransaction();
  if (trans) {
    isWebsocket = trans->IsWebsocketUpgrade();
    MOZ_ASSERT(!isWebsocket || !needTunnel, "Websocket and tunnel?!");
  }

  LOG(("nsHttpConnection::AddTransaction [this=%p] for %s%s", this,
       mSpdySession ? "SPDY" : "QUIC",
       needTunnel ? " over tunnel" : (isWebsocket ? " websocket" : "")));

  if (mSpdySession) {
    if (!mSpdySession->AddStream(httpTransaction, priority, needTunnel,
                                 isWebsocket, mCallbacks)) {
      MOZ_ASSERT(false);  // this cannot happen!
      httpTransaction->Close(NS_ERROR_ABORT);
      return NS_ERROR_FAILURE;
    }
  }

  Unused << ResumeSend();
  return NS_OK;
}

void nsHttpConnection::Close(nsresult reason, bool aIsShutdown) {
  LOG(("nsHttpConnection::Close [this=%p reason=%" PRIx32 "]\n", this,
       static_cast<uint32_t>(reason)));

  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  // Ensure TCP keepalive timer is stopped.
  if (mTCPKeepaliveTransitionTimer) {
    mTCPKeepaliveTransitionTimer->Cancel();
    mTCPKeepaliveTransitionTimer = nullptr;
  }
  if (mForceSendTimer) {
    mForceSendTimer->Cancel();
    mForceSendTimer = nullptr;
  }

  if (!mTrafficCategory.IsEmpty()) {
    HttpTrafficAnalyzer* hta = gHttpHandler->GetHttpTrafficAnalyzer();
    if (hta) {
      hta->IncrementHttpConnection(std::move(mTrafficCategory));
      MOZ_ASSERT(mTrafficCategory.IsEmpty());
    }
  }

  if (NS_FAILED(reason)) {
    if (mIdleMonitoring) EndIdleMonitoring();

    mTLSFilter = nullptr;

    // The connection and security errors clear out alt-svc mappings
    // in case any previously validated ones are now invalid
    if (((reason == NS_ERROR_NET_RESET) ||
         (NS_ERROR_GET_MODULE(reason) == NS_ERROR_MODULE_SECURITY)) &&
        mConnInfo && !(mTransactionCaps & NS_HTTP_ERROR_SOFTLY)) {
      gHttpHandler->ClearHostMapping(mConnInfo);
    }

    if (mSocketTransport) {
      mSocketTransport->SetEventSink(nullptr, nullptr);

      // If there are bytes sitting in the input queue then read them
      // into a junk buffer to avoid generating a tcp rst by closing a
      // socket with data pending. TLS is a classic case of this where
      // a Alert record might be superfulous to a clean HTTP/SPDY shutdown.
      // Never block to do this and limit it to a small amount of data.
      // During shutdown just be fast!
      if (mSocketIn && !aIsShutdown) {
        char buffer[4000];
        uint32_t count, total = 0;
        nsresult rv;
        do {
          rv = mSocketIn->Read(buffer, 4000, &count);
          if (NS_SUCCEEDED(rv)) total += count;
        } while (NS_SUCCEEDED(rv) && count > 0 && total < 64000);
        LOG(("nsHttpConnection::Close drained %d bytes\n", total));
      }

      mSocketTransport->SetSecurityCallbacks(nullptr);
      mSocketTransport->Close(reason);
      if (mSocketOut) mSocketOut->AsyncWait(nullptr, 0, 0, nullptr);
    }
    mKeepAlive = false;
  }
}

// called on the socket thread
nsresult nsHttpConnection::InitSSLParams(bool connectingToProxy,
                                         bool proxyStartSSL) {
  LOG(("nsHttpConnection::InitSSLParams [this=%p] connectingToProxy=%d\n", this,
       connectingToProxy));
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  nsresult rv;
  nsCOMPtr<nsISupports> securityInfo;
  GetSecurityInfo(getter_AddRefs(securityInfo));
  if (!securityInfo) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISSLSocketControl> ssl = do_QueryInterface(securityInfo, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (proxyStartSSL) {
    rv = ssl->ProxyStartSSL();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (NS_SUCCEEDED(SetupNPNList(ssl, mTransactionCaps))) {
    LOG(("InitSSLParams Setting up SPDY Negotiation OK"));
    mNPNComplete = false;
  }

  return NS_OK;
}

void nsHttpConnection::DontReuse() {
  LOG(("nsHttpConnection::DontReuse %p spdysession=%p\n", this,
       mSpdySession.get()));
  mKeepAliveMask = false;
  mKeepAlive = false;
  mDontReuse = true;
  mIdleTimeout = 0;
  if (mSpdySession) {
    mSpdySession->DontReuse();
  }
}

bool nsHttpConnection::TestJoinConnection(const nsACString& hostname,
                                          int32_t port) {
  if (mSpdySession && CanDirectlyActivate()) {
    return mSpdySession->TestJoinConnection(hostname, port);
  }

  return false;
}

bool nsHttpConnection::JoinConnection(const nsACString& hostname,
                                      int32_t port) {
  if (mSpdySession && CanDirectlyActivate()) {
    return mSpdySession->JoinConnection(hostname, port);
  }

  return false;
}

bool nsHttpConnection::CanReuse() {
  if (mDontReuse || !mRemainingConnectionUses) {
    return false;
  }

  if ((mTransaction ? (mTransaction->IsDone() ? 0U : 1U) : 0U) >=
      mRemainingConnectionUses) {
    return false;
  }

  bool canReuse;
  if (mSpdySession) {
    canReuse = mSpdySession->CanReuse();
  } else {
    canReuse = IsKeepAlive();
  }

  canReuse = canReuse && (IdleTime() < mIdleTimeout) && IsAlive();

  // An idle persistent connection should not have data waiting to be read
  // before a request is sent. Data here is likely a 408 timeout response
  // which we would deal with later on through the restart logic, but that
  // path is more expensive than just closing the socket now.

  uint64_t dataSize;
  if (canReuse && mSocketIn && (mUsingSpdyVersion == SpdyVersion::NONE) &&
      mHttp1xTransactionCount &&
      NS_SUCCEEDED(mSocketIn->Available(&dataSize)) && dataSize) {
    LOG(
        ("nsHttpConnection::CanReuse %p %s"
         "Socket not reusable because read data pending (%" PRIu64 ") on it.\n",
         this, mConnInfo->Origin(), dataSize));
    canReuse = false;
  }
  return canReuse;
}

bool nsHttpConnection::CanDirectlyActivate() {
  // return true if a new transaction can be addded to ths connection at any
  // time through Activate(). In practice this means this is a healthy SPDY
  // connection with room for more concurrent streams.

  return UsingSpdy() && CanReuse() && mSpdySession &&
         mSpdySession->RoomForMoreStreams();
}

PRIntervalTime nsHttpConnection::IdleTime() {
  return mSpdySession ? mSpdySession->IdleTime()
                      : (PR_IntervalNow() - mLastReadTime);
}

// returns the number of seconds left before the allowable idle period
// expires, or 0 if the period has already expied.
uint32_t nsHttpConnection::TimeToLive() {
  LOG(("nsHttpConnection::TTL: %p %s idle %d timeout %d\n", this,
       mConnInfo->Origin(), IdleTime(), mIdleTimeout));

  if (IdleTime() >= mIdleTimeout) {
    return 0;
  }

  uint32_t timeToLive = PR_IntervalToSeconds(mIdleTimeout - IdleTime());

  // a positive amount of time can be rounded to 0. Because 0 is used
  // as the expiration signal, round all values from 0 to 1 up to 1.
  if (!timeToLive) {
    timeToLive = 1;
  }
  return timeToLive;
}

bool nsHttpConnection::IsAlive() {
  if (!mSocketTransport || !mConnectedTransport) return false;

  // SocketTransport::IsAlive can run the SSL state machine, so make sure
  // the NPN options are set before that happens.
  SetupSSL();

  bool alive;
  nsresult rv = mSocketTransport->IsAlive(&alive);
  if (NS_FAILED(rv)) alive = false;

//#define TEST_RESTART_LOGIC
#ifdef TEST_RESTART_LOGIC
  if (!alive) {
    LOG(("pretending socket is still alive to test restart logic\n"));
    alive = true;
  }
#endif

  return alive;
}

void nsHttpConnection::SetUrgentStartPreferred(bool urgent) {
  if (mExperienced && !mUrgentStartPreferredKnown) {
    // Set only according the first ever dispatched non-null transaction
    mUrgentStartPreferredKnown = true;
    mUrgentStartPreferred = urgent;
    LOG(("nsHttpConnection::SetUrgentStartPreferred [this=%p urgent=%d]", this,
         urgent));
  }
}

//----------------------------------------------------------------------------
// nsHttpConnection::nsAHttpConnection compatible methods
//----------------------------------------------------------------------------

nsresult nsHttpConnection::OnHeadersAvailable(nsAHttpTransaction* trans,
                                              nsHttpRequestHead* requestHead,
                                              nsHttpResponseHead* responseHead,
                                              bool* reset) {
  LOG(
      ("nsHttpConnection::OnHeadersAvailable [this=%p trans=%p "
       "response-head=%p]\n",
       this, trans, responseHead));

  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  NS_ENSURE_ARG_POINTER(trans);
  MOZ_ASSERT(responseHead, "No response head?");

  if (mInSpdyTunnel) {
    DebugOnly<nsresult> rv =
        responseHead->SetHeader(nsHttp::X_Firefox_Spdy_Proxy, "true"_ns);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  // we won't change our keep-alive policy unless the server has explicitly
  // told us to do so.

  // inspect the connection headers for keep-alive info provided the
  // transaction completed successfully. In the case of a non-sensical close
  // and keep-alive favor the close out of conservatism.

  bool explicitKeepAlive = false;
  bool explicitClose =
      responseHead->HasHeaderValue(nsHttp::Connection, "close") ||
      responseHead->HasHeaderValue(nsHttp::Proxy_Connection, "close");
  if (!explicitClose)
    explicitKeepAlive =
        responseHead->HasHeaderValue(nsHttp::Connection, "keep-alive") ||
        responseHead->HasHeaderValue(nsHttp::Proxy_Connection, "keep-alive");

  // deal with 408 Server Timeouts
  uint16_t responseStatus = responseHead->Status();
  static const PRIntervalTime k1000ms = PR_MillisecondsToInterval(1000);
  if (responseStatus == 408) {
    // If this error could be due to a persistent connection reuse then
    // we pass an error code of NS_ERROR_NET_RESET to
    // trigger the transaction 'restart' mechanism.  We tell it to reset its
    // response headers so that it will be ready to receive the new response.
    if (mIsReused && ((PR_IntervalNow() - mLastWriteTime) < k1000ms)) {
      Close(NS_ERROR_NET_RESET);
      *reset = true;
      return NS_OK;
    }

    // timeouts that are not caused by persistent connection reuse should
    // not be retried for browser compatibility reasons. bug 907800. The
    // server driven close is implicit in the 408.
    explicitClose = true;
    explicitKeepAlive = false;
  }

  if ((responseHead->Version() < HttpVersion::v1_1) ||
      (requestHead->Version() < HttpVersion::v1_1)) {
    // HTTP/1.0 connections are by default NOT persistent
    if (explicitKeepAlive)
      mKeepAlive = true;
    else
      mKeepAlive = false;
  } else {
    // HTTP/1.1 connections are by default persistent
    mKeepAlive = !explicitClose;
  }
  mKeepAliveMask = mKeepAlive;

  // if this connection is persistent, then the server may send a "Keep-Alive"
  // header specifying the maximum number of times the connection can be
  // reused as well as the maximum amount of time the connection can be idle
  // before the server will close it.  we ignore the max reuse count, because
  // a "keep-alive" connection is by definition capable of being reused, and
  // we only care about being able to reuse it once.  if a timeout is not
  // specified then we use our advertized timeout value.
  bool foundKeepAliveMax = false;
  if (mKeepAlive) {
    nsAutoCString keepAlive;
    Unused << responseHead->GetHeader(nsHttp::Keep_Alive, keepAlive);

    if (mUsingSpdyVersion == SpdyVersion::NONE) {
      const char* cp = nsCRT::strcasestr(keepAlive.get(), "timeout=");
      if (cp)
        mIdleTimeout = PR_SecondsToInterval((uint32_t)atoi(cp + 8));
      else
        mIdleTimeout = gHttpHandler->IdleTimeout() * mDefaultTimeoutFactor;

      cp = nsCRT::strcasestr(keepAlive.get(), "max=");
      if (cp) {
        int maxUses = atoi(cp + 4);
        if (maxUses > 0) {
          foundKeepAliveMax = true;
          mRemainingConnectionUses = static_cast<uint32_t>(maxUses);
        }
      }
    }

    LOG(("Connection can be reused [this=%p idle-timeout=%usec]\n", this,
         PR_IntervalToSeconds(mIdleTimeout)));
  }

  if (!foundKeepAliveMax && mRemainingConnectionUses &&
      (mUsingSpdyVersion == SpdyVersion::NONE)) {
    --mRemainingConnectionUses;
  }

  // If we're doing a proxy connect, we need to check whether or not
  // it was successful.  If so, we have to reset the transaction and step-up
  // the socket connection if using SSL. Finally, we have to wake up the
  // socket write request.
  bool itWasProxyConnect = !!mProxyConnectStream;
  if (mProxyConnectStream) {
    MOZ_ASSERT(mUsingSpdyVersion == SpdyVersion::NONE,
               "SPDY NPN Complete while using proxy connect stream");
    mProxyConnectStream = nullptr;
    bool isHttps = mTransaction ? mTransaction->ConnectionInfo()->EndToEndSSL()
                                : mConnInfo->EndToEndSSL();
    bool onlyConnect = mTransactionCaps & NS_HTTP_CONNECT_ONLY;

    mTransaction->OnProxyConnectComplete(responseStatus);
    if (responseStatus == 200) {
      LOG(("proxy CONNECT succeeded! endtoendssl=%d onlyconnect=%d\n", isHttps,
           onlyConnect));
      // If we're only connecting, we don't need to reset the transaction
      // state. We need to upgrade the socket now without doing the actual
      // http request.
      if (!onlyConnect) {
        *reset = true;
      }
      nsresult rv;
      // CONNECT only flag doesn't do the tls setup. https here only
      // ensures a proxy tunnel was used not that tls is setup.
      if (isHttps) {
        if (!onlyConnect) {
          if (mConnInfo->UsingHttpsProxy()) {
            LOG(("%p new TLSFilterTransaction %s %d\n", this,
                 mConnInfo->Origin(), mConnInfo->OriginPort()));
            SetupSecondaryTLS();
          }

          rv = InitSSLParams(false, true);
          LOG(("InitSSLParams [rv=%" PRIx32 "]\n", static_cast<uint32_t>(rv)));
        } else {
          // We have an https protocol but the CONNECT only flag was
          // specified. The consumer only wants a raw socket to the
          // proxy. We have to mark this as complete to finish the
          // transaction and be upgraded. OnSocketReadable() uses this
          // to detect an inactive tunnel and blocks completion.
          mNPNComplete = true;
        }
      }
      mCompletedProxyConnect = true;
      mProxyConnectInProgress = false;
      rv = mSocketOut->AsyncWait(this, 0, 0, nullptr);
      // XXX what if this fails -- need to handle this error
      MOZ_ASSERT(NS_SUCCEEDED(rv), "mSocketOut->AsyncWait failed");
    } else {
      LOG(("proxy CONNECT failed! endtoendssl=%d onlyconnect=%d\n", isHttps,
           onlyConnect));
      mTransaction->SetProxyConnectFailed();
    }
  }

  nsAutoCString upgradeReq;
  bool hasUpgradeReq =
      NS_SUCCEEDED(requestHead->GetHeader(nsHttp::Upgrade, upgradeReq));
  // Don't use persistent connection for Upgrade unless there's an auth failure:
  // some proxies expect to see auth response on persistent connection.
  // Also allow persistent conn for h2, as we don't want to waste connections
  // for multiplexed upgrades.
  if (!itWasProxyConnect && hasUpgradeReq && responseStatus != 401 &&
      responseStatus != 407 && !mSpdySession) {
    LOG(("HTTP Upgrade in play - disable keepalive for http/1.x\n"));
    DontReuse();
  }

  if (responseStatus == 101) {
    nsAutoCString upgradeResp;
    bool hasUpgradeResp =
        NS_SUCCEEDED(responseHead->GetHeader(nsHttp::Upgrade, upgradeResp));
    if (!hasUpgradeReq || !hasUpgradeResp ||
        !nsHttp::FindToken(upgradeResp.get(), upgradeReq.get(),
                           HTTP_HEADER_VALUE_SEPS)) {
      LOG(("HTTP 101 Upgrade header mismatch req = %s, resp = %s\n",
           upgradeReq.get(),
           !upgradeResp.IsEmpty() ? upgradeResp.get()
                                  : "RESPONSE's nsHttp::Upgrade is empty"));
      Close(NS_ERROR_ABORT);
    } else {
      LOG(("HTTP Upgrade Response to %s\n", upgradeResp.get()));
    }
  }

  mLastHttpResponseVersion = responseHead->Version();

  return NS_OK;
}

bool nsHttpConnection::IsReused() {
  if (mIsReused) return true;
  if (!mConsiderReusedAfterInterval) return false;

  // ReusedAfter allows a socket to be consider reused only after a certain
  // interval of time has passed
  return (PR_IntervalNow() - mConsiderReusedAfterEpoch) >=
         mConsiderReusedAfterInterval;
}

void nsHttpConnection::SetIsReusedAfter(uint32_t afterMilliseconds) {
  mConsiderReusedAfterEpoch = PR_IntervalNow();
  mConsiderReusedAfterInterval = PR_MillisecondsToInterval(afterMilliseconds);
}

nsresult nsHttpConnection::TakeTransport(nsISocketTransport** aTransport,
                                         nsIAsyncInputStream** aInputStream,
                                         nsIAsyncOutputStream** aOutputStream) {
  if (mUsingSpdyVersion != SpdyVersion::NONE) return NS_ERROR_FAILURE;
  if (mTransaction && !mTransaction->IsDone()) return NS_ERROR_IN_PROGRESS;
  if (!(mSocketTransport && mSocketIn && mSocketOut))
    return NS_ERROR_NOT_INITIALIZED;

  if (mInputOverflow) mSocketIn = mInputOverflow.forget();

  // Change TCP Keepalive frequency to long-lived if currently short-lived.
  if (mTCPKeepaliveConfig == kTCPKeepaliveShortLivedConfig) {
    if (mTCPKeepaliveTransitionTimer) {
      mTCPKeepaliveTransitionTimer->Cancel();
      mTCPKeepaliveTransitionTimer = nullptr;
    }
    nsresult rv = StartLongLivedTCPKeepalives();
    LOG(
        ("nsHttpConnection::TakeTransport [%p] calling "
         "StartLongLivedTCPKeepalives",
         this));
    if (NS_FAILED(rv)) {
      LOG(
          ("nsHttpConnection::TakeTransport [%p] "
           "StartLongLivedTCPKeepalives failed rv[0x%" PRIx32 "]",
           this, static_cast<uint32_t>(rv)));
    }
  }

  mSocketTransport->SetSecurityCallbacks(nullptr);
  mSocketTransport->SetEventSink(nullptr, nullptr);

  // The nsHttpConnection will go away soon, so if there is a TLS Filter
  // being used (e.g. for wss CONNECT tunnel from a proxy connected to
  // via https) that filter needs to take direct control of the
  // streams
  if (mTLSFilter) {
    nsCOMPtr<nsIAsyncInputStream> ref1(mSocketIn);
    nsCOMPtr<nsIAsyncOutputStream> ref2(mSocketOut);
    mTLSFilter->newIODriver(ref1, ref2, getter_AddRefs(mSocketIn),
                            getter_AddRefs(mSocketOut));
    mTLSFilter = nullptr;
  }

  mSocketTransport.forget(aTransport);
  mSocketIn.forget(aInputStream);
  mSocketOut.forget(aOutputStream);

  return NS_OK;
}

uint32_t nsHttpConnection::ReadTimeoutTick(PRIntervalTime now) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  // make sure timer didn't tick before Activate()
  if (!mTransaction) return UINT32_MAX;

  // Spdy implements some timeout handling using the SPDY ping frame.
  if (mSpdySession) {
    return mSpdySession->ReadTimeoutTick(now);
  }

  uint32_t nextTickAfter = UINT32_MAX;
  // Timeout if the response is taking too long to arrive.
  if (mResponseTimeoutEnabled) {
    NS_WARNING_ASSERTION(
        gHttpHandler->ResponseTimeoutEnabled(),
        "Timing out a response, but response timeout is disabled!");

    PRIntervalTime initialResponseDelta = now - mLastWriteTime;

    if (initialResponseDelta > mTransaction->ResponseTimeout()) {
      LOG(("canceling transaction: no response for %ums: timeout is %dms\n",
           PR_IntervalToMilliseconds(initialResponseDelta),
           PR_IntervalToMilliseconds(mTransaction->ResponseTimeout())));

      mResponseTimeoutEnabled = false;

      // This will also close the connection
      CloseTransaction(mTransaction, NS_ERROR_NET_TIMEOUT);
      return UINT32_MAX;
    }
    nextTickAfter = PR_IntervalToSeconds(mTransaction->ResponseTimeout()) -
                    PR_IntervalToSeconds(initialResponseDelta);
    nextTickAfter = std::max(nextTickAfter, 1U);
  }

  if (!mNPNComplete) {
    // We can reuse mLastWriteTime here, because it is set when the
    // connection is activated and only change when a transaction
    // succesfullu write to the socket and this can only happen after
    // the TLS handshake is done.
    PRIntervalTime initialTLSDelta = now - mLastWriteTime;
    if (initialTLSDelta >
        PR_MillisecondsToInterval(gHttpHandler->TLSHandshakeTimeout())) {
      LOG(
          ("canceling transaction: tls handshake takes too long: tls handshake "
           "last %ums, timeout is %dms.",
           PR_IntervalToMilliseconds(initialTLSDelta),
           gHttpHandler->TLSHandshakeTimeout()));

      // This will also close the connection
      CloseTransaction(mTransaction, NS_ERROR_NET_TIMEOUT);
      return UINT32_MAX;
    }
  }

  return nextTickAfter;
}

void nsHttpConnection::UpdateTCPKeepalive(nsITimer* aTimer, void* aClosure) {
  MOZ_ASSERT(aTimer);
  MOZ_ASSERT(aClosure);

  nsHttpConnection* self = static_cast<nsHttpConnection*>(aClosure);

  if (NS_WARN_IF(self->mUsingSpdyVersion != SpdyVersion::NONE)) {
    return;
  }

  // Do not reduce keepalive probe frequency for idle connections.
  if (self->mIdleMonitoring) {
    return;
  }

  nsresult rv = self->StartLongLivedTCPKeepalives();
  if (NS_FAILED(rv)) {
    LOG(
        ("nsHttpConnection::UpdateTCPKeepalive [%p] "
         "StartLongLivedTCPKeepalives failed rv[0x%" PRIx32 "]",
         self, static_cast<uint32_t>(rv)));
  }
}

void nsHttpConnection::GetSecurityInfo(nsISupports** secinfo) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  LOG(("nsHttpConnection::GetSecurityInfo trans=%p tlsfilter=%p socket=%p\n",
       mTransaction.get(), mTLSFilter.get(), mSocketTransport.get()));

  if (mTransaction &&
      NS_SUCCEEDED(mTransaction->GetTransactionSecurityInfo(secinfo))) {
    return;
  }

  if (mTLSFilter &&
      NS_SUCCEEDED(mTLSFilter->GetTransactionSecurityInfo(secinfo))) {
    return;
  }

  if (mSocketTransport &&
      NS_SUCCEEDED(mSocketTransport->GetSecurityInfo(secinfo))) {
    return;
  }

  *secinfo = nullptr;
}

nsresult nsHttpConnection::PushBack(const char* data, uint32_t length) {
  LOG(("nsHttpConnection::PushBack [this=%p, length=%d]\n", this, length));

  if (mInputOverflow) {
    NS_ERROR("nsHttpConnection::PushBack only one buffer supported");
    return NS_ERROR_UNEXPECTED;
  }

  mInputOverflow = new nsPreloadedStream(mSocketIn, data, length);
  return NS_OK;
}

class HttpConnectionForceIO : public Runnable {
 public:
  HttpConnectionForceIO(nsHttpConnection* aConn, bool doRecv)
      : Runnable("net::HttpConnectionForceIO"), mConn(aConn), mDoRecv(doRecv) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(OnSocketThread(), "not on socket thread");

    if (mDoRecv) {
      if (!mConn->mSocketIn) return NS_OK;
      return mConn->OnInputStreamReady(mConn->mSocketIn);
    }

    MOZ_ASSERT(mConn->mForceSendPending);
    mConn->mForceSendPending = false;

    if (!mConn->mSocketOut) {
      return NS_OK;
    }
    return mConn->OnOutputStreamReady(mConn->mSocketOut);
  }

 private:
  RefPtr<nsHttpConnection> mConn;
  bool mDoRecv;
};

nsresult nsHttpConnection::ResumeSend() {
  LOG(("nsHttpConnection::ResumeSend [this=%p]\n", this));

  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (mSocketOut) {
    return mSocketOut->AsyncWait(this, 0, 0, nullptr);
  }

  MOZ_ASSERT_UNREACHABLE("no socket output stream");
  return NS_ERROR_UNEXPECTED;
}

nsresult nsHttpConnection::ResumeRecv() {
  LOG(("nsHttpConnection::ResumeRecv [this=%p]\n", this));

  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  // the mLastReadTime timestamp is used for finding slowish readers
  // and can be pretty sensitive. For that reason we actually reset it
  // when we ask to read (resume recv()) so that when we get called back
  // with actual read data in OnSocketReadable() we are only measuring
  // the latency between those two acts and not all the processing that
  // may get done before the ResumeRecv() call
  mLastReadTime = PR_IntervalNow();

  if (mSocketIn) {
    if (!mTLSFilter || !mTLSFilter->HasDataToRecv() || NS_FAILED(ForceRecv())) {
      return mSocketIn->AsyncWait(this, 0, 0, nullptr);
    }
    return NS_OK;
  }

  MOZ_ASSERT_UNREACHABLE("no socket input stream");
  return NS_ERROR_UNEXPECTED;
}

void nsHttpConnection::ForceSendIO(nsITimer* aTimer, void* aClosure) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  nsHttpConnection* self = static_cast<nsHttpConnection*>(aClosure);
  MOZ_ASSERT(aTimer == self->mForceSendTimer);
  self->mForceSendTimer = nullptr;
  NS_DispatchToCurrentThread(new HttpConnectionForceIO(self, false));
}

nsresult nsHttpConnection::MaybeForceSendIO() {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  // due to bug 1213084 sometimes real I/O events do not get serviced when
  // NSPR derived I/O events are ready and this can cause a deadlock with
  // https over https proxying. Normally we would expect the write callback to
  // be invoked before this timer goes off, but set it at the old windows
  // tick interval (kForceDelay) as a backup for those circumstances.
  static const uint32_t kForceDelay = 17;  // ms

  if (mForceSendPending) {
    return NS_OK;
  }
  MOZ_ASSERT(!mForceSendTimer);
  mForceSendPending = true;
  return NS_NewTimerWithFuncCallback(getter_AddRefs(mForceSendTimer),
                                     nsHttpConnection::ForceSendIO, this,
                                     kForceDelay, nsITimer::TYPE_ONE_SHOT,
                                     "net::nsHttpConnection::MaybeForceSendIO");
}

// trigger an asynchronous read
nsresult nsHttpConnection::ForceRecv() {
  LOG(("nsHttpConnection::ForceRecv [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  return NS_DispatchToCurrentThread(new HttpConnectionForceIO(this, true));
}

// trigger an asynchronous write
nsresult nsHttpConnection::ForceSend() {
  LOG(("nsHttpConnection::ForceSend [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (mTLSFilter) {
    return mTLSFilter->NudgeTunnel(this);
  }
  return MaybeForceSendIO();
}

void nsHttpConnection::BeginIdleMonitoring() {
  LOG(("nsHttpConnection::BeginIdleMonitoring [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(!mTransaction, "BeginIdleMonitoring() while active");
  MOZ_ASSERT(mUsingSpdyVersion == SpdyVersion::NONE,
             "Idle monitoring of spdy not allowed");

  LOG(("Entering Idle Monitoring Mode [this=%p]", this));
  mIdleMonitoring = true;
  if (mSocketIn) mSocketIn->AsyncWait(this, 0, 0, nullptr);
}

void nsHttpConnection::EndIdleMonitoring() {
  LOG(("nsHttpConnection::EndIdleMonitoring [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(!mTransaction, "EndIdleMonitoring() while active");

  if (mIdleMonitoring) {
    LOG(("Leaving Idle Monitoring Mode [this=%p]", this));
    mIdleMonitoring = false;
    if (mSocketIn) mSocketIn->AsyncWait(nullptr, 0, 0, nullptr);
  }
}

HttpVersion nsHttpConnection::Version() {
  if (mUsingSpdyVersion != SpdyVersion::NONE) {
    return HttpVersion::v2_0;
  }
  return mLastHttpResponseVersion;
}

//-----------------------------------------------------------------------------
// nsHttpConnection <private>
//-----------------------------------------------------------------------------

void nsHttpConnection::CloseTransaction(nsAHttpTransaction* trans,
                                        nsresult reason, bool aIsShutdown) {
  LOG(("nsHttpConnection::CloseTransaction[this=%p trans=%p reason=%" PRIx32
       "]\n",
       this, trans, static_cast<uint32_t>(reason)));

  MOZ_ASSERT((trans == mTransaction) ||
             (mTLSFilter && !mTLSFilter->Transaction()) ||
             (mTLSFilter && mTLSFilter->Transaction() == trans));
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (mCurrentBytesRead > mMaxBytesRead) mMaxBytesRead = mCurrentBytesRead;

  // mask this error code because its not a real error.
  if (reason == NS_BASE_STREAM_CLOSED) reason = NS_OK;

  if (mUsingSpdyVersion != SpdyVersion::NONE) {
    DontReuse();
    // if !mSpdySession then mUsingSpdyVersion must be false for canreuse()
    mSpdySession->SetCleanShutdown(aIsShutdown);
    mUsingSpdyVersion = SpdyVersion::NONE;
    mSpdySession = nullptr;
  }

  if (!mTransaction && mTLSFilter) {
    // In case of a race when the transaction is being closed before the tunnel
    // is established we need to carry closing status on the proxied
    // transaction.
    // Not doing this leads to use of this closed connection to activate the
    // not closed transaction what will likely lead to a use of a closed ssl
    // socket and may cause a crash because of an unexpected use.
    //
    // There can possibly be two states: the actual transaction is still hanging
    // of off the filter, or has not even been assigned on it yet.  In the
    // latter case we simply must close the transaction given to us via the
    // argument.
    if (!mTLSFilter->Transaction()) {
      if (trans) {
        LOG(("  closing transaction directly"));
        trans->Close(reason);
      }
    } else {
      LOG(("  closing transactin hanging of off mTLSFilter"));
      mTLSFilter->Close(reason);
    }
  }

  if (mTransaction) {
    LOG(("  closing associated mTransaction"));
    mHttp1xTransactionCount += mTransaction->Http1xTransactionCount();

    mTransaction->Close(reason);
    mTransaction = nullptr;
  }

  {
    MutexAutoLock lock(mCallbacksLock);
    mCallbacks = nullptr;
  }

  if (NS_FAILED(reason) && (reason != NS_BINDING_RETARGETED)) {
    Close(reason, aIsShutdown);
  }

  // flag the connection as reused here for convenience sake.  certainly
  // it might be going away instead ;-)
  mIsReused = true;
}

nsresult nsHttpConnection::ReadFromStream(nsIInputStream* input, void* closure,
                                          const char* buf, uint32_t offset,
                                          uint32_t count, uint32_t* countRead) {
  // thunk for nsIInputStream instance
  nsHttpConnection* conn = (nsHttpConnection*)closure;
  return conn->OnReadSegment(buf, count, countRead);
}

bool nsHttpConnection::CheckCanWrite0RTTData() {
  MOZ_ASSERT(mWaitingFor0RTTResponse);
  nsCOMPtr<nsISupports> securityInfo;
  GetSecurityInfo(getter_AddRefs(securityInfo));
  if (!securityInfo) {
    return false;
  }
  nsCOMPtr<nsITransportSecurityInfo> info;
  info = do_QueryInterface(securityInfo);
  if (!info) {
    return false;
  }
  nsAutoCString negotiatedNPN;
  // If the following code fails means that the handshake is not done
  // yet, so continue writing 0RTT data.
  nsresult rv = info->GetNegotiatedNPN(negotiatedNPN);
  if (NS_FAILED(rv)) {
    return true;
  }
  nsCOMPtr<nsISSLSocketControl> ssl;
  ssl = do_QueryInterface(securityInfo);
  if (!ssl) {
    return false;
  }
  bool earlyDataAccepted = false;
  rv = ssl->GetEarlyDataAccepted(&earlyDataAccepted);
  // If 0RTT data is accepted we can continue writing data,
  // if it is reject stop writing more data.
  return NS_SUCCEEDED(rv) && earlyDataAccepted;
}

nsresult nsHttpConnection::OnReadSegment(const char* buf, uint32_t count,
                                         uint32_t* countRead) {
  LOG(("nsHttpConnection::OnReadSegment [this=%p]\n", this));
  if (count == 0) {
    // some ReadSegments implementations will erroneously call the writer
    // to consume 0 bytes worth of data.  we must protect against this case
    // or else we'd end up closing the socket prematurely.
    NS_ERROR("bad ReadSegments implementation");
    return NS_ERROR_FAILURE;  // stop iterating
  }

  // If we are waiting for 0RTT Response, check maybe nss has finished
  // handshake already.
  // IsAlive() calls drive the handshake and that may cause nss and necko
  // to be out of sync.
  if (mWaitingFor0RTTResponse && !CheckCanWrite0RTTData()) {
    LOG(
        ("nsHttpConnection::OnReadSegment Do not write any data, wait"
         " for EnsureNPNComplete to be called [this=%p]",
         this));
    *countRead = 0;
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  nsresult rv = mSocketOut->Write(buf, count, countRead);
  if (NS_FAILED(rv))
    mSocketOutCondition = rv;
  else if (*countRead == 0)
    mSocketOutCondition = NS_BASE_STREAM_CLOSED;
  else {
    mLastWriteTime = PR_IntervalNow();
    mSocketOutCondition = NS_OK;  // reset condition
    if (!mProxyConnectInProgress) mTotalBytesWritten += *countRead;
  }

  return mSocketOutCondition;
}

nsresult nsHttpConnection::OnSocketWritable() {
  LOG(("nsHttpConnection::OnSocketWritable [this=%p] host=%s\n", this,
       mConnInfo->Origin()));

  nsresult rv;
  uint32_t transactionBytes;
  bool again = true;

  // Prevent STS thread from being blocked by single OnOutputStreamReady
  // callback.
  const uint32_t maxWriteAttempts = 128;
  uint32_t writeAttempts = 0;

  if (mTransactionCaps & NS_HTTP_CONNECT_ONLY) {
    if (!mCompletedProxyConnect && !mProxyConnectStream) {
      // A CONNECT has been requested for this connection but will never
      // be performed. This should never happen.
      MOZ_ASSERT(false, "proxy connect will never happen");
      LOG(("return failure because proxy connect will never happen\n"));
      return NS_ERROR_FAILURE;
    }

    if (mCompletedProxyConnect) {
      // Don't need to check this each write attempt since it is only
      // updated after OnSocketWritable completes.
      // We've already done primary tls (if needed) and sent our CONNECT.
      // If we're doing a CONNECT only request there's no need to write
      // the http transaction or do the SSL handshake here.
      LOG(("return ok because proxy connect successful\n"));
      return NS_OK;
    }
  }

  do {
    ++writeAttempts;
    rv = mSocketOutCondition = NS_OK;
    transactionBytes = 0;

    // The SSL handshake must be completed before the
    // transaction->readsegments() processing can proceed because we need to
    // know how to format the request differently for http/1, http/2, spdy,
    // etc.. and that is negotiated with NPN/ALPN in the SSL handshake.

    if (mConnInfo->UsingHttpsProxy() &&
        !EnsureNPNComplete(rv, transactionBytes)) {
      MOZ_ASSERT(!transactionBytes);
      mSocketOutCondition = NS_BASE_STREAM_WOULD_BLOCK;
    } else if (mProxyConnectStream) {
      // If we're need an HTTP/1 CONNECT tunnel through a proxy
      // send it before doing the SSL handshake
      LOG(("  writing CONNECT request stream\n"));
      rv = mProxyConnectStream->ReadSegments(ReadFromStream, this,
                                             nsIOService::gDefaultSegmentSize,
                                             &transactionBytes);
    } else if (!EnsureNPNComplete(rv, transactionBytes)) {
      if (NS_SUCCEEDED(rv) && !transactionBytes &&
          NS_SUCCEEDED(mSocketOutCondition)) {
        mSocketOutCondition = NS_BASE_STREAM_WOULD_BLOCK;
      }
    } else if (!mTransaction) {
      rv = NS_ERROR_FAILURE;
      LOG(("  No Transaction In OnSocketWritable\n"));
    } else if (NS_SUCCEEDED(rv)) {
      // for non spdy sessions let the connection manager know
      if (!mReportedSpdy) {
        mReportedSpdy = true;
        MOZ_ASSERT(!mEverUsedSpdy);
        gHttpHandler->ConnMgr()->ReportSpdyConnection(this, false);
      }

      LOG(("  writing transaction request stream\n"));
      mProxyConnectInProgress = false;
      rv = mTransaction->ReadSegmentsAgain(
          this, nsIOService::gDefaultSegmentSize, &transactionBytes, &again);
      mContentBytesWritten += transactionBytes;
    }

    LOG(
        ("nsHttpConnection::OnSocketWritable %p "
         "ReadSegments returned [rv=%" PRIx32 " read=%u "
         "sock-cond=%" PRIx32 " again=%d]\n",
         this, static_cast<uint32_t>(rv), transactionBytes,
         static_cast<uint32_t>(mSocketOutCondition), again));

    // XXX some streams return NS_BASE_STREAM_CLOSED to indicate EOF.
    if (rv == NS_BASE_STREAM_CLOSED && !mTransaction->IsDone()) {
      rv = NS_OK;
      transactionBytes = 0;
    }

    if (!again && mWaitingFor0RTTResponse) {
      // Continue waiting;
      rv = mSocketOut->AsyncWait(this, 0, 0, nullptr);
    }
    if (NS_FAILED(rv)) {
      // if the transaction didn't want to write any more data, then
      // wait for the transaction to call ResumeSend.
      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        rv = NS_OK;
        if (mWaitingFor0RTTResponse) {
          // Continue waiting;
          rv = mSocketOut->AsyncWait(this, 0, 0, nullptr);
        }
      }
      again = false;
    } else if (NS_FAILED(mSocketOutCondition)) {
      if (mSocketOutCondition == NS_BASE_STREAM_WOULD_BLOCK) {
        if (mTLSFilter) {
          LOG(("  blocked tunnel (handshake?)\n"));
          rv = mTLSFilter->NudgeTunnel(this);
        } else {
          rv = mSocketOut->AsyncWait(this, 0, 0, nullptr);  // continue writing
        }
      } else {
        rv = mSocketOutCondition;
      }
      again = false;
    } else if (!transactionBytes) {
      rv = NS_OK;

      if (mWaitingFor0RTTResponse) {
        // Wait for tls handshake to finish or waiting for connect.
        rv = mSocketOut->AsyncWait(this, 0, 0, nullptr);
      } else if (mTransaction) {  // in case the ReadSegments stack called
                                  // CloseTransaction()
        //
        // at this point we've written out the entire transaction, and now we
        // must wait for the server's response.  we manufacture a status message
        // here to reflect the fact that we are waiting.  this message will be
        // trumped (overwritten) if the server responds quickly.
        //
        mTransaction->OnTransportStatus(mSocketTransport,
                                        NS_NET_STATUS_WAITING_FOR, 0);

        rv = ResumeRecv();  // start reading
      }
      again = false;
    } else if (writeAttempts >= maxWriteAttempts) {
      LOG(("  yield for other transactions\n"));
      rv = mSocketOut->AsyncWait(this, 0, 0, nullptr);  // continue writing
      again = false;
    }
    // write more to the socket until error or end-of-request...
  } while (again && gHttpHandler->Active());

  return rv;
}

nsresult nsHttpConnection::OnWriteSegment(char* buf, uint32_t count,
                                          uint32_t* countWritten) {
  if (count == 0) {
    // some WriteSegments implementations will erroneously call the reader
    // to provide 0 bytes worth of data.  we must protect against this case
    // or else we'd end up closing the socket prematurely.
    NS_ERROR("bad WriteSegments implementation");
    return NS_ERROR_FAILURE;  // stop iterating
  }

  if (ChaosMode::isActive(ChaosFeature::IOAmounts) &&
      ChaosMode::randomUint32LessThan(2)) {
    // read 1...count bytes
    count = ChaosMode::randomUint32LessThan(count) + 1;
  }

  nsresult rv = mSocketIn->Read(buf, count, countWritten);
  if (NS_FAILED(rv))
    mSocketInCondition = rv;
  else if (*countWritten == 0)
    mSocketInCondition = NS_BASE_STREAM_CLOSED;
  else
    mSocketInCondition = NS_OK;  // reset condition

  return mSocketInCondition;
}

nsresult nsHttpConnection::OnSocketReadable() {
  LOG(("nsHttpConnection::OnSocketReadable [this=%p]\n", this));

  PRIntervalTime now = PR_IntervalNow();
  PRIntervalTime delta = now - mLastReadTime;

  // Reset mResponseTimeoutEnabled to stop response timeout checks.
  mResponseTimeoutEnabled = false;

  if ((mTransactionCaps & NS_HTTP_CONNECT_ONLY) && !mCompletedProxyConnect &&
      !mProxyConnectStream) {
    // A CONNECT has been requested for this connection but will never
    // be performed. This should never happen.
    MOZ_ASSERT(false, "proxy connect will never happen");
    LOG(("return failure because proxy connect will never happen\n"));
    return NS_ERROR_FAILURE;
  }

  if (mKeepAliveMask && (delta >= mMaxHangTime)) {
    LOG(("max hang time exceeded!\n"));
    // give the handler a chance to create a new persistent connection to
    // this host if we've been busy for too long.
    mKeepAliveMask = false;
    Unused << gHttpHandler->ProcessPendingQ(mConnInfo);
  }

  // Reduce the estimate of the time since last read by up to 1 RTT to
  // accommodate exhausted sender TCP congestion windows or minor I/O delays.
  mLastReadTime = now;

  nsresult rv;
  uint32_t n;
  bool again = true;

  do {
    if (!mProxyConnectInProgress && !mNPNComplete) {
      // Unless we are setting up a tunnel via CONNECT, prevent reading
      // from the socket until the results of NPN
      // negotiation are known (which is determined from the write path).
      // If the server speaks SPDY it is likely the readable data here is
      // a spdy settings frame and without NPN it would be misinterpreted
      // as HTTP/*

      LOG(
          ("nsHttpConnection::OnSocketReadable %p return due to inactive "
           "tunnel setup but incomplete NPN state\n",
           this));
      rv = NS_OK;
      break;
    }

    mSocketInCondition = NS_OK;
    rv = mTransaction->WriteSegmentsAgain(
        this, nsIOService::gDefaultSegmentSize, &n, &again);
    LOG(("nsHttpConnection::OnSocketReadable %p trans->ws rv=%" PRIx32
         " n=%d socketin=%" PRIx32 "\n",
         this, static_cast<uint32_t>(rv), n,
         static_cast<uint32_t>(mSocketInCondition)));
    if (NS_FAILED(rv)) {
      // if the transaction didn't want to take any more data, then
      // wait for the transaction to call ResumeRecv.
      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        rv = NS_OK;
      }
      again = false;
    } else {
      mCurrentBytesRead += n;
      mTotalBytesRead += n;
      if (NS_FAILED(mSocketInCondition)) {
        // continue waiting for the socket if necessary...
        if (mSocketInCondition == NS_BASE_STREAM_WOULD_BLOCK) {
          rv = ResumeRecv();
        } else {
          rv = mSocketInCondition;
        }
        again = false;
      }
    }
    // read more from the socket until error...
  } while (again && gHttpHandler->Active());

  return rv;
}

void nsHttpConnection::SetupSecondaryTLS(
    nsAHttpTransaction* aSpdyConnectTransaction) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(!mTLSFilter);
  LOG(
      ("nsHttpConnection %p SetupSecondaryTLS %s %d "
       "aSpdyConnectTransaction=%p\n",
       this, mConnInfo->Origin(), mConnInfo->OriginPort(),
       aSpdyConnectTransaction));

  nsHttpConnectionInfo* ci = nullptr;
  if (mTransaction) {
    ci = mTransaction->ConnectionInfo();
  }
  if (!ci) {
    ci = mConnInfo;
  }
  MOZ_ASSERT(ci);

  mTLSFilter = new TLSFilterTransaction(mTransaction, ci->Origin(),
                                        ci->OriginPort(), this, this);

  if (mTransaction) {
    mTransaction = mTLSFilter;
  }
  mWeakTrans = do_GetWeakReference(aSpdyConnectTransaction);
}

void nsHttpConnection::SetInSpdyTunnel(bool arg) {
  MOZ_ASSERT(mTLSFilter);
  mInSpdyTunnel = arg;

  // don't setup another tunnel :)
  mProxyConnectStream = nullptr;
  mCompletedProxyConnect = true;
  mProxyConnectInProgress = false;
}

// static
nsresult nsHttpConnection::MakeConnectString(nsAHttpTransaction* trans,
                                             nsHttpRequestHead* request,
                                             nsACString& result, bool h2ws) {
  result.Truncate();
  if (!trans->ConnectionInfo()) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  DebugOnly<nsresult> rv;

  rv = nsHttpHandler::GenerateHostPort(
      nsDependentCString(trans->ConnectionInfo()->Origin()),
      trans->ConnectionInfo()->OriginPort(), result);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // CONNECT host:port HTTP/1.1
  request->SetMethod("CONNECT"_ns);
  request->SetVersion(gHttpHandler->HttpVersion());
  if (h2ws) {
    // HTTP/2 websocket CONNECT forms need the full request URI
    nsAutoCString requestURI;
    trans->RequestHead()->RequestURI(requestURI);
    request->SetRequestURI(requestURI);

    request->SetHTTPS(trans->RequestHead()->IsHTTPS());
  } else {
    request->SetRequestURI(result);
  }
  rv = request->SetHeader(nsHttp::User_Agent, gHttpHandler->UserAgent());
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // a CONNECT is always persistent
  rv = request->SetHeader(nsHttp::Proxy_Connection, "keep-alive"_ns);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  rv = request->SetHeader(nsHttp::Connection, "keep-alive"_ns);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // all HTTP/1.1 requests must include a Host header (even though it
  // may seem redundant in this case; see bug 82388).
  rv = request->SetHeader(nsHttp::Host, result);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  nsAutoCString val;
  if (NS_SUCCEEDED(
          trans->RequestHead()->GetHeader(nsHttp::Proxy_Authorization, val))) {
    // we don't know for sure if this authorization is intended for the
    // SSL proxy, so we add it just in case.
    rv = request->SetHeader(nsHttp::Proxy_Authorization, val);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  if ((trans->Caps() & NS_HTTP_CONNECT_ONLY) &&
      NS_SUCCEEDED(trans->RequestHead()->GetHeader(nsHttp::Upgrade, val))) {
    // rfc7639 proposes using the ALPN header to indicate the protocol used
    // in CONNECT when not used for TLS. The protocol is stored in Upgrade.
    // We have to copy this header here since a new HEAD request is created
    // for the CONNECT.
    rv = request->SetHeader("ALPN"_ns, val);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  result.Truncate();
  request->Flatten(result, false);

  if (LOG1_ENABLED()) {
    LOG(("nsHttpConnection::MakeConnectString for transaction=%p [",
         trans->QueryHttpTransaction()));
    LogHeaders(result.BeginReading());
    LOG(("]"));
  }

  result.AppendLiteral("\r\n");
  return NS_OK;
}

nsresult nsHttpConnection::SetupProxyConnect() {
  LOG(("nsHttpConnection::SetupProxyConnect [this=%p]\n", this));
  NS_ENSURE_TRUE(!mProxyConnectStream, NS_ERROR_ALREADY_INITIALIZED);
  MOZ_ASSERT(mUsingSpdyVersion == SpdyVersion::NONE,
             "SPDY NPN Complete while using proxy connect stream");

  nsAutoCString buf;
  nsHttpRequestHead request;
  nsresult rv = MakeConnectString(mTransaction, &request, buf, false);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_NewCStringInputStream(getter_AddRefs(mProxyConnectStream),
                                  std::move(buf));
}

nsresult nsHttpConnection::StartShortLivedTCPKeepalives() {
  if (mUsingSpdyVersion != SpdyVersion::NONE) {
    return NS_OK;
  }
  MOZ_ASSERT(mSocketTransport);
  if (!mSocketTransport) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = NS_OK;
  int32_t idleTimeS = -1;
  int32_t retryIntervalS = -1;
  if (gHttpHandler->TCPKeepaliveEnabledForShortLivedConns()) {
    // Set the idle time.
    idleTimeS = gHttpHandler->GetTCPKeepaliveShortLivedIdleTime();
    LOG(
        ("nsHttpConnection::StartShortLivedTCPKeepalives[%p] "
         "idle time[%ds].",
         this, idleTimeS));

    retryIntervalS = std::max<int32_t>((int32_t)PR_IntervalToSeconds(mRtt), 1);
    rv = mSocketTransport->SetKeepaliveVals(idleTimeS, retryIntervalS);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = mSocketTransport->SetKeepaliveEnabled(true);
    mTCPKeepaliveConfig = kTCPKeepaliveShortLivedConfig;
  } else {
    rv = mSocketTransport->SetKeepaliveEnabled(false);
    mTCPKeepaliveConfig = kTCPKeepaliveDisabled;
  }
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Start a timer to move to long-lived keepalive config.
  if (!mTCPKeepaliveTransitionTimer) {
    mTCPKeepaliveTransitionTimer = NS_NewTimer();
  }

  if (mTCPKeepaliveTransitionTimer) {
    int32_t time = gHttpHandler->GetTCPKeepaliveShortLivedTime();

    // Adjust |time| to ensure a full set of keepalive probes can be sent
    // at the end of the short-lived phase.
    if (gHttpHandler->TCPKeepaliveEnabledForShortLivedConns()) {
      if (NS_WARN_IF(!gSocketTransportService)) {
        return NS_ERROR_NOT_INITIALIZED;
      }
      int32_t probeCount = -1;
      rv = gSocketTransportService->GetKeepaliveProbeCount(&probeCount);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (NS_WARN_IF(probeCount <= 0)) {
        return NS_ERROR_UNEXPECTED;
      }
      // Add time for final keepalive probes, and 2 seconds for a buffer.
      time += ((probeCount)*retryIntervalS) - (time % idleTimeS) + 2;
    }
    mTCPKeepaliveTransitionTimer->InitWithNamedFuncCallback(
        nsHttpConnection::UpdateTCPKeepalive, this, (uint32_t)time * 1000,
        nsITimer::TYPE_ONE_SHOT,
        "net::nsHttpConnection::StartShortLivedTCPKeepalives");
  } else {
    NS_WARNING(
        "nsHttpConnection::StartShortLivedTCPKeepalives failed to "
        "create timer.");
  }

  return NS_OK;
}

nsresult nsHttpConnection::StartLongLivedTCPKeepalives() {
  MOZ_ASSERT(mUsingSpdyVersion == SpdyVersion::NONE,
             "Don't use TCP Keepalive with SPDY!");
  if (NS_WARN_IF(mUsingSpdyVersion != SpdyVersion::NONE)) {
    return NS_OK;
  }
  MOZ_ASSERT(mSocketTransport);
  if (!mSocketTransport) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = NS_OK;
  if (gHttpHandler->TCPKeepaliveEnabledForLongLivedConns()) {
    // Increase the idle time.
    int32_t idleTimeS = gHttpHandler->GetTCPKeepaliveLongLivedIdleTime();
    LOG(("nsHttpConnection::StartLongLivedTCPKeepalives[%p] idle time[%ds]",
         this, idleTimeS));

    int32_t retryIntervalS =
        std::max<int32_t>((int32_t)PR_IntervalToSeconds(mRtt), 1);
    rv = mSocketTransport->SetKeepaliveVals(idleTimeS, retryIntervalS);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Ensure keepalive is enabled, if current status is disabled.
    if (mTCPKeepaliveConfig == kTCPKeepaliveDisabled) {
      rv = mSocketTransport->SetKeepaliveEnabled(true);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    mTCPKeepaliveConfig = kTCPKeepaliveLongLivedConfig;
  } else {
    rv = mSocketTransport->SetKeepaliveEnabled(false);
    mTCPKeepaliveConfig = kTCPKeepaliveDisabled;
  }

  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
}

nsresult nsHttpConnection::DisableTCPKeepalives() {
  MOZ_ASSERT(mSocketTransport);
  if (!mSocketTransport) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  LOG(("nsHttpConnection::DisableTCPKeepalives [%p]", this));
  if (mTCPKeepaliveConfig != kTCPKeepaliveDisabled) {
    nsresult rv = mSocketTransport->SetKeepaliveEnabled(false);
    if (NS_FAILED(rv)) {
      return rv;
    }
    mTCPKeepaliveConfig = kTCPKeepaliveDisabled;
  }
  if (mTCPKeepaliveTransitionTimer) {
    mTCPKeepaliveTransitionTimer->Cancel();
    mTCPKeepaliveTransitionTimer = nullptr;
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpConnection::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ADDREF(nsHttpConnection)
NS_IMPL_RELEASE(nsHttpConnection)

NS_INTERFACE_MAP_BEGIN(nsHttpConnection)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamCallback)
  NS_INTERFACE_MAP_ENTRY(nsIOutputStreamCallback)
  NS_INTERFACE_MAP_ENTRY(nsITransportEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(HttpConnectionBase)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(nsHttpConnection)
NS_INTERFACE_MAP_END

//-----------------------------------------------------------------------------
// nsHttpConnection::nsIInputStreamCallback
//-----------------------------------------------------------------------------

// called on the socket transport thread
NS_IMETHODIMP
nsHttpConnection::OnInputStreamReady(nsIAsyncInputStream* in) {
  MOZ_ASSERT(in == mSocketIn, "unexpected stream");
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (mIdleMonitoring) {
    MOZ_ASSERT(!mTransaction, "Idle Input Event While Active");

    // The only read event that is protocol compliant for an idle connection
    // is an EOF, which we check for with CanReuse(). If the data is
    // something else then just ignore it and suspend checking for EOF -
    // our normal timers or protocol stack are the place to deal with
    // any exception logic.

    if (!CanReuse()) {
      LOG(("Server initiated close of idle conn %p\n", this));
      Unused << gHttpHandler->ConnMgr()->CloseIdleConnection(this);
      return NS_OK;
    }

    LOG(("Input data on idle conn %p, but not closing yet\n", this));
    return NS_OK;
  }

  // if the transaction was dropped...
  if (!mTransaction) {
    LOG(("  no transaction; ignoring event\n"));
    return NS_OK;
  }

  nsresult rv = OnSocketReadable();
  if (NS_FAILED(rv)) CloseTransaction(mTransaction, rv);

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpConnection::nsIOutputStreamCallback
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpConnection::OnOutputStreamReady(nsIAsyncOutputStream* out) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(out == mSocketOut, "unexpected socket");
  // if the transaction was dropped...
  if (!mTransaction) {
    LOG(("  no transaction; ignoring event\n"));
    return NS_OK;
  }

  nsresult rv = OnSocketWritable();
  if (NS_FAILED(rv)) CloseTransaction(mTransaction, rv);

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpConnection::nsITransportEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpConnection::OnTransportStatus(nsITransport* trans, nsresult status,
                                    int64_t progress, int64_t progressMax) {
  if (mTransaction) mTransaction->OnTransportStatus(trans, status, progress);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpConnection::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

// not called on the socket transport thread
NS_IMETHODIMP
nsHttpConnection::GetInterface(const nsIID& iid, void** result) {
  // NOTE: This function is only called on the UI thread via sync proxy from
  //       the socket transport thread.  If that weren't the case, then we'd
  //       have to worry about the possibility of mTransaction going away
  //       part-way through this function call.  See CloseTransaction.

  // NOTE - there is a bug here, the call to getinterface is proxied off the
  // nss thread, not the ui thread as the above comment says. So there is
  // indeed a chance of mTransaction going away. bug 615342

  MOZ_ASSERT(!OnSocketThread(), "on socket thread");

  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  {
    MutexAutoLock lock(mCallbacksLock);
    callbacks = mCallbacks;
  }
  if (callbacks) return callbacks->GetInterface(iid, result);
  return NS_ERROR_NO_INTERFACE;
}

void nsHttpConnection::CheckForTraffic(bool check) {
  if (check) {
    LOG((" CheckForTraffic conn %p\n", this));
    if (mSpdySession) {
      if (PR_IntervalToMilliseconds(IdleTime()) >= 500) {
        // Send a ping to verify it is still alive if it has been idle
        // more than half a second, the network changed events are
        // rate-limited to one per 1000 ms.
        LOG((" SendPing\n"));
        mSpdySession->SendPing();
      } else {
        LOG((" SendPing skipped due to network activity\n"));
      }
    } else {
      // If not SPDY, Store snapshot amount of data right now
      mTrafficCount = mTotalBytesWritten + mTotalBytesRead;
      mTrafficStamp = true;
    }
  } else {
    // mark it as not checked
    mTrafficStamp = false;
  }
}

void nsHttpConnection::SetEvent(nsresult aStatus) {
  switch (aStatus) {
    case NS_NET_STATUS_RESOLVING_HOST:
      mBootstrappedTimings.domainLookupStart = TimeStamp::Now();
      break;
    case NS_NET_STATUS_RESOLVED_HOST:
      mBootstrappedTimings.domainLookupEnd = TimeStamp::Now();
      break;
    case NS_NET_STATUS_CONNECTING_TO:
      mBootstrappedTimings.connectStart = TimeStamp::Now();
      break;
    case NS_NET_STATUS_CONNECTED_TO: {
      TimeStamp tnow = TimeStamp::Now();
      mBootstrappedTimings.tcpConnectEnd = tnow;
      mBootstrappedTimings.connectEnd = tnow;
      mBootstrappedTimings.secureConnectionStart = tnow;
      break;
    }
    case NS_NET_STATUS_TLS_HANDSHAKE_STARTING:
      mBootstrappedTimings.secureConnectionStart = TimeStamp::Now();
      break;
    case NS_NET_STATUS_TLS_HANDSHAKE_ENDED:
      mBootstrappedTimings.connectEnd = TimeStamp::Now();
      break;
    default:
      break;
  }
}

bool nsHttpConnection::NoClientCertAuth() const {
  if (!mSocketTransport) {
    return false;
  }

  nsCOMPtr<nsISupports> secInfo;
  mSocketTransport->GetSecurityInfo(getter_AddRefs(secInfo));
  if (!secInfo) {
    return false;
  }

  nsCOMPtr<nsISSLSocketControl> ssc(do_QueryInterface(secInfo));
  if (!ssc) {
    return false;
  }

  return !ssc->GetClientCertSent();
}

bool nsHttpConnection::CanAcceptWebsocket() {
  if (!UsingSpdy()) {
    return true;
  }

  return mSpdySession->CanAcceptWebsocket();
}

bool nsHttpConnection::IsProxyConnectInProgress() {
  return mProxyConnectInProgress;
}

bool nsHttpConnection::LastTransactionExpectedNoContent() {
  return mLastTransactionExpectedNoContent;
}

void nsHttpConnection::SetLastTransactionExpectedNoContent(bool val) {
  mLastTransactionExpectedNoContent = val;
}

bool nsHttpConnection::IsPersistent() { return IsKeepAlive() && !mDontReuse; }

nsAHttpTransaction* nsHttpConnection::Transaction() { return mTransaction; }

nsresult nsHttpConnection::GetSelfAddr(NetAddr* addr) {
  if (!mSocketTransport) {
    return NS_ERROR_FAILURE;
  } else {
    return mSocketTransport->GetSelfAddr(addr);
  }
}

nsresult nsHttpConnection::GetPeerAddr(NetAddr* addr) {
  if (!mSocketTransport) {
    return NS_ERROR_FAILURE;
  } else {
    return mSocketTransport->GetPeerAddr(addr);
  }
}

bool nsHttpConnection::ResolvedByTRR() {
  bool val = false;
  if (mSocketTransport) {
    mSocketTransport->ResolvedByTRR(&val);
  }
  return val;
}

bool nsHttpConnection::GetEchConfigUsed() {
  bool val = false;
  if (mSocketTransport) {
    mSocketTransport->GetEchConfigUsed(&val);
  }
  return val;
}

}  // namespace net
}  // namespace mozilla
