/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpConnection_h__
#define nsHttpConnection_h__

#include "HttpConnectionBase.h"
#include "nsHttpConnectionInfo.h"
#include "nsHttpResponseHead.h"
#include "nsAHttpTransaction.h"
#include "nsCOMPtr.h"
#include "nsProxyRelease.h"
#include "prinrval.h"
#include "TunnelUtils.h"
#include "mozilla/Mutex.h"
#include "ARefBase.h"
#include "TimingStruct.h"
#include "HttpTrafficAnalyzer.h"

#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsITimer.h"

class nsISocketTransport;
class nsISSLSocketControl;

namespace mozilla {
namespace net {

class nsHttpHandler;
class ASpdySession;

// 1dcc863e-db90-4652-a1fe-13fea0b54e46
#define NS_HTTPCONNECTION_IID                        \
  {                                                  \
    0x1dcc863e, 0xdb90, 0x4652, {                    \
      0xa1, 0xfe, 0x13, 0xfe, 0xa0, 0xb5, 0x4e, 0x46 \
    }                                                \
  }

//-----------------------------------------------------------------------------
// nsHttpConnection - represents a connection to a HTTP server (or proxy)
//
// NOTE: this objects lives on the socket thread only.  it should not be
// accessed from any other thread.
//-----------------------------------------------------------------------------

class nsHttpConnection final : public HttpConnectionBase,
                               public nsAHttpSegmentReader,
                               public nsAHttpSegmentWriter,
                               public nsIInputStreamCallback,
                               public nsIOutputStreamCallback,
                               public nsITransportEventSink,
                               public nsIInterfaceRequestor,
                               public NudgeTunnelCallback {
 private:
  virtual ~nsHttpConnection();

 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_HTTPCONNECTION_IID)
  NS_DECL_HTTPCONNECTIONBASE
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSAHTTPSEGMENTREADER
  NS_DECL_NSAHTTPSEGMENTWRITER
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIOUTPUTSTREAMCALLBACK
  NS_DECL_NSITRANSPORTEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NUDGETUNNELCALLBACK

  nsHttpConnection();

  void SetFastOpen(bool aFastOpen);
  // Close this connection and return the transaction. The transaction is
  // restarted as well. This will only happened before connection is
  // connected.
  nsAHttpTransaction* CloseConnectionFastOpenTakesTooLongOrError(
      bool aCloseocketTransport);

  //-------------------------------------------------------------------------
  // XXX document when these are ok to call

  bool IsKeepAlive() {
    return (mUsingSpdyVersion != SpdyVersion::NONE) ||
           (mKeepAliveMask && mKeepAlive);
  }

  // Returns time in seconds for how long connection can be reused.
  uint32_t TimeToLive();

  bool NeedSpdyTunnel() {
    return mConnInfo->UsingHttpsProxy() && !mTLSFilter &&
           mConnInfo->UsingConnect();
  }

  // A connection is forced into plaintext when it is intended to be used as a
  // CONNECT tunnel but the setup fails. The plaintext only carries the CONNECT
  // error.
  void ForcePlainText() { mForcePlainText = true; }

  bool IsUrgentStartPreferred() const {
    return mUrgentStartPreferredKnown && mUrgentStartPreferred;
  }
  void SetUrgentStartPreferred(bool urgent);

  void SetIsReusedAfter(uint32_t afterMilliseconds);

  int64_t MaxBytesRead() { return mMaxBytesRead; }
  HttpVersion GetLastHttpResponseVersion() { return mLastHttpResponseVersion; }

  friend class HttpConnectionForceIO;

  static MOZ_MUST_USE nsresult ReadFromStream(nsIInputStream*, void*,
                                              const char*, uint32_t, uint32_t,
                                              uint32_t*);

  // When a persistent connection is in the connection manager idle
  // connection pool, the nsHttpConnection still reads errors and hangups
  // on the socket so that it can be proactively released if the server
  // initiates a termination. Only call on socket thread.
  void BeginIdleMonitoring();
  void EndIdleMonitoring();

  bool UsingSpdy() override { return (mUsingSpdyVersion != SpdyVersion::NONE); }
  SpdyVersion GetSpdyVersion() { return mUsingSpdyVersion; }
  bool EverUsedSpdy() { return mEverUsedSpdy; }
  bool UsingHttp3() override { return false; }

  // true when connection SSL NPN phase is complete and we know
  // authoritatively whether UsingSpdy() or not.
  bool ReportedNPN() { return mReportedSpdy; }

  // When the connection is active this is called up to once every 1 second
  // return the interval (in seconds) that the connection next wants to
  // have this invoked. It might happen sooner depending on the needs of
  // other connections.
  uint32_t ReadTimeoutTick(PRIntervalTime now);

  // For Active and Idle connections, this will be called when
  // mTCPKeepaliveTransitionTimer fires, to check if the TCP keepalive config
  // should move from short-lived (fast-detect) to long-lived.
  static void UpdateTCPKeepalive(nsITimer* aTimer, void* aClosure);

  // When the connection is active this is called every second
  void ReadTimeoutTick();

  int64_t ContentBytesWritten() { return mContentBytesWritten; }

  static MOZ_MUST_USE nsresult MakeConnectString(nsAHttpTransaction* trans,
                                                 nsHttpRequestHead* request,
                                                 nsACString& result, bool h2ws);
  void SetupSecondaryTLS(nsAHttpTransaction* aSpdyConnectTransaction = nullptr);
  void SetInSpdyTunnel(bool arg);

  // Check active connections for traffic (or not). SPDY connections send a
  // ping, ordinary HTTP connections get some time to get traffic to be
  // considered alive.
  void CheckForTraffic(bool check);

  // NoTraffic() returns true if there's been no traffic on the (non-spdy)
  // connection since CheckForTraffic() was called.
  bool NoTraffic() {
    return mTrafficStamp &&
           (mTrafficCount == (mTotalBytesWritten + mTotalBytesRead)) &&
           !mFastOpen;
  }

  void SetFastOpenStatus(uint8_t tfoStatus);
  uint8_t GetFastOpenStatus() { return mFastOpenStatus; }

  // Return true when the socket this connection is using has not been
  // authenticated using a client certificate.  Before SSL negotiation
  // has finished this returns false.
  bool NoClientCertAuth() const override;

  bool CanAcceptWebsocket() override;

 private:
  // Value (set in mTCPKeepaliveConfig) indicates which set of prefs to use.
  enum TCPKeepaliveConfig {
    kTCPKeepaliveDisabled = 0,
    kTCPKeepaliveShortLivedConfig,
    kTCPKeepaliveLongLivedConfig
  };

  // called to cause the underlying socket to start speaking SSL
  MOZ_MUST_USE nsresult InitSSLParams(bool connectingToProxy,
                                      bool ProxyStartSSL);
  MOZ_MUST_USE nsresult SetupNPNList(nsISSLSocketControl* ssl, uint32_t caps);

  MOZ_MUST_USE nsresult OnTransactionDone(nsresult reason);
  MOZ_MUST_USE nsresult OnSocketWritable();
  MOZ_MUST_USE nsresult OnSocketReadable();

  MOZ_MUST_USE nsresult SetupProxyConnect();

  PRIntervalTime IdleTime();
  bool IsAlive();

  // Makes certain the SSL handshake is complete and NPN negotiation
  // has had a chance to happen
  MOZ_MUST_USE bool EnsureNPNComplete(nsresult& aOut0RTTWriteHandshakeValue,
                                      uint32_t& aOut0RTTBytesWritten);

  void SetupSSL();

  // Start the Spdy transaction handler when NPN indicates spdy/*
  void StartSpdy(nsISSLSocketControl* ssl, SpdyVersion versionLevel);
  // Like the above, but do the bare minimum to do 0RTT data, so we can back
  // it out, if necessary
  void Start0RTTSpdy(SpdyVersion versionLevel);

  // Helpers for Start*Spdy
  nsresult TryTakeSubTransactions(nsTArray<RefPtr<nsAHttpTransaction> >& list);
  nsresult MoveTransactionsToSpdy(nsresult status,
                                  nsTArray<RefPtr<nsAHttpTransaction> >& list);

  // Directly Add a transaction to an active connection for SPDY
  MOZ_MUST_USE nsresult AddTransaction(nsAHttpTransaction*, int32_t);

  // Used to set TCP keepalives for fast detection of dead connections during
  // an initial period, and slower detection for long-lived connections.
  MOZ_MUST_USE nsresult StartShortLivedTCPKeepalives();
  MOZ_MUST_USE nsresult StartLongLivedTCPKeepalives();
  MOZ_MUST_USE nsresult DisableTCPKeepalives();

 private:
  // mTransaction only points to the HTTP Transaction callbacks if the
  // transaction is open, otherwise it is null.
  RefPtr<nsAHttpTransaction> mTransaction;

  nsCOMPtr<nsIAsyncInputStream> mSocketIn;
  nsCOMPtr<nsIAsyncOutputStream> mSocketOut;

  nsresult mSocketInCondition;
  nsresult mSocketOutCondition;

  nsCOMPtr<nsIInputStream> mProxyConnectStream;
  nsCOMPtr<nsIInputStream> mRequestStream;

  RefPtr<TLSFilterTransaction> mTLSFilter;
  nsWeakPtr mWeakTrans;  // SpdyConnectTransaction *

  RefPtr<nsHttpHandler> mHttpHandler;  // keep gHttpHandler alive

  PRIntervalTime mLastReadTime;
  PRIntervalTime mLastWriteTime;
  PRIntervalTime
      mMaxHangTime;  // max download time before dropping keep-alive status
  PRIntervalTime mIdleTimeout;  // value of keep-alive: timeout=
  PRIntervalTime mConsiderReusedAfterInterval;
  PRIntervalTime mConsiderReusedAfterEpoch;
  int64_t mCurrentBytesRead;     // data read per activation
  int64_t mMaxBytesRead;         // max read in 1 activation
  int64_t mTotalBytesRead;       // total data read
  int64_t mContentBytesWritten;  // does not include CONNECT tunnel or TLS

  RefPtr<nsIAsyncInputStream> mInputOverflow;

  // Whether the first non-null transaction dispatched on this connection was
  // urgent-start or not
  bool mUrgentStartPreferred;
  // A flag to prevent reset of mUrgentStartPreferred by subsequent transactions
  bool mUrgentStartPreferredKnown;
  bool mConnectedTransport;
  bool mKeepAlive;
  bool mKeepAliveMask;
  bool mDontReuse;
  bool mIsReused;
  bool mCompletedProxyConnect;
  bool mLastTransactionExpectedNoContent;
  bool mIdleMonitoring;
  bool mProxyConnectInProgress;
  bool mInSpdyTunnel;
  bool mForcePlainText;

  // A snapshot of current number of transfered bytes
  int64_t mTrafficCount;
  bool mTrafficStamp;  // true then the above is set

  // The number of <= HTTP/1.1 transactions performed on this connection. This
  // excludes spdy transactions.
  uint32_t mHttp1xTransactionCount;

  // Keep-Alive: max="mRemainingConnectionUses" provides the number of future
  // transactions (including the current one) that the server expects to allow
  // on this persistent connection.
  uint32_t mRemainingConnectionUses;

  // SPDY related
  bool mNPNComplete;
  bool mSetupSSLCalled;

  // version level in use, 0 if unused
  SpdyVersion mUsingSpdyVersion;

  RefPtr<ASpdySession> mSpdySession;
  int32_t mPriority;
  bool mReportedSpdy;

  // mUsingSpdyVersion is cleared when mSpdySession is freed, this is permanent
  bool mEverUsedSpdy;

  // mLastHttpResponseVersion stores the last response's http version seen.
  HttpVersion mLastHttpResponseVersion;

  // If a large keepalive has been requested for any trans,
  // scale the default by this factor
  uint32_t mDefaultTimeoutFactor;

  bool mResponseTimeoutEnabled;

  // Flag to indicate connection is in inital keepalive period (fast detect).
  uint32_t mTCPKeepaliveConfig;
  nsCOMPtr<nsITimer> mTCPKeepaliveTransitionTimer;

 private:
  // For ForceSend()
  static void ForceSendIO(nsITimer* aTimer, void* aClosure);
  MOZ_MUST_USE nsresult MaybeForceSendIO();
  bool mForceSendPending;
  nsCOMPtr<nsITimer> mForceSendTimer;

  // Helper variable for 0RTT handshake;
  bool m0RTTChecked;             // Possible 0RTT has been
                                 // checked.
  bool mWaitingFor0RTTResponse;  // We have are
                                 // sending 0RTT
                                 // data and we
                                 // are waiting
                                 // for the end of
                                 // the handsake.
  int64_t mContentBytesWritten0RTT;
  bool mEarlyDataNegotiated;  // Only used for telemetry
  nsCString mEarlyNegotiatedALPN;
  bool mDid0RTTSpdy;

  bool mFastOpen;
  uint8_t mFastOpenStatus;

  bool mForceSendDuringFastOpenPending;
  bool mReceivedSocketWouldBlockDuringFastOpen;
  bool mCheckNetworkStallsWithTFO;
  PRIntervalTime mLastRequestBytesSentTime;

 private:
  bool mThroughCaptivePortal;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsHttpConnection, NS_HTTPCONNECTION_IID)

}  // namespace net
}  // namespace mozilla

#endif  // nsHttpConnection_h__
