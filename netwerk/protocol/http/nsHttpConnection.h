/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpConnection_h__
#define nsHttpConnection_h__

#include <functional>
#include "HttpConnectionBase.h"
#include "nsHttpConnectionInfo.h"
#include "nsHttpResponseHead.h"
#include "nsAHttpTransaction.h"
#include "nsCOMPtr.h"
#include "nsProxyRelease.h"
#include "prinrval.h"
#include "mozilla/Mutex.h"
#include "ARefBase.h"
#include "TimingStruct.h"
#include "HttpTrafficAnalyzer.h"
#include "TlsHandshaker.h"

#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsISocketTransport.h"
#include "nsISupportsPriority.h"
#include "nsITimer.h"
#include "nsITlsHandshakeListener.h"

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
                               public nsIInterfaceRequestor {
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

  nsHttpConnection();

  // Initialize the connection:
  //  info        - specifies the connection parameters.
  //  maxHangTime - limits the amount of time this connection can spend on a
  //                single transaction before it should no longer be kept
  //                alive.  a value of 0xffff indicates no limit.
  [[nodiscard]] virtual nsresult Init(nsHttpConnectionInfo* info,
                                      uint16_t maxHangTime, nsISocketTransport*,
                                      nsIAsyncInputStream*,
                                      nsIAsyncOutputStream*,
                                      bool connectedTransport, nsresult status,
                                      nsIInterfaceRequestor*, PRIntervalTime,
                                      bool forWebSocket);

  //-------------------------------------------------------------------------
  // XXX document when these are ok to call

  bool IsKeepAlive() {
    return (mUsingSpdyVersion != SpdyVersion::NONE) ||
           (mKeepAliveMask && mKeepAlive);
  }

  // Returns time in seconds for how long connection can be reused.
  uint32_t TimeToLive();

  bool NeedSpdyTunnel() {
    return mConnInfo->UsingHttpsProxy() && !mHasTLSTransportLayer &&
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
  friend class TlsHandshaker;

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

  void SetupSecondaryTLS(
      nsAHttpTransaction* aHttp2ConnectTransaction = nullptr);
  void SetInSpdyTunnel();

  // Check active connections for traffic (or not). SPDY connections send a
  // ping, ordinary HTTP connections get some time to get traffic to be
  // considered alive.
  void CheckForTraffic(bool check);

  // NoTraffic() returns true if there's been no traffic on the (non-spdy)
  // connection since CheckForTraffic() was called.
  bool NoTraffic() {
    return mTrafficStamp &&
           (mTrafficCount == (mTotalBytesWritten + mTotalBytesRead));
  }

  // Return true when the socket this connection is using has not been
  // authenticated using a client certificate.  Before SSL negotiation
  // has finished this returns false.
  bool NoClientCertAuth() const override;

  bool CanAcceptWebsocket() override;

  int64_t BytesWritten() override { return mTotalBytesWritten; }

  nsISocketTransport* Transport() override { return mSocketTransport; }

  nsresult GetSelfAddr(NetAddr* addr) override;
  nsresult GetPeerAddr(NetAddr* addr) override;
  bool ResolvedByTRR() override;
  bool GetEchConfigUsed() override;

  bool IsForWebSocket() { return mForWebSocket; }

  // The following functions are related to setting up a tunnel.
  [[nodiscard]] static nsresult MakeConnectString(
      nsAHttpTransaction* trans, nsHttpRequestHead* request, nsACString& result,
      bool h2ws, bool aShouldResistFingerprinting);
  [[nodiscard]] static nsresult ReadFromStream(nsIInputStream*, void*,
                                               const char*, uint32_t, uint32_t,
                                               uint32_t*);

  nsresult CreateTunnelStream(nsAHttpTransaction* httpTransaction,
                              nsHttpConnection** aHttpConnection);

  bool RequestDone() { return mRequestDone; }

 private:
  enum HttpConnectionState {
    UNINITIALIZED,
    SETTING_UP_TUNNEL,
    REQUEST,
  } mState{HttpConnectionState::UNINITIALIZED};
  void ChangeState(HttpConnectionState newState);

  // Tunnel retated functions:
  bool TunnelSetupInProgress() { return mState == SETTING_UP_TUNNEL; }
  void SetTunnelSetupDone();
  nsresult CheckTunnelIsNeeded();
  nsresult SetupProxyConnectStream();
  nsresult SendConnectRequest(void* closure, uint32_t* transactionBytes);

  void HandleTunnelResponse(uint16_t responseStatus, bool* reset);
  void HandleWebSocketResponse(nsHttpRequestHead* requestHead,
                               nsHttpResponseHead* responseHead,
                               uint16_t responseStatus);

  // Value (set in mTCPKeepaliveConfig) indicates which set of prefs to use.
  enum TCPKeepaliveConfig {
    kTCPKeepaliveDisabled = 0,
    kTCPKeepaliveShortLivedConfig,
    kTCPKeepaliveLongLivedConfig
  };

  [[nodiscard]] nsresult OnTransactionDone(nsresult reason);
  [[nodiscard]] nsresult OnSocketWritable();
  [[nodiscard]] nsresult OnSocketReadable();

  PRIntervalTime IdleTime();
  bool IsAlive();

  // Start the Spdy transaction handler when NPN indicates spdy/*
  void StartSpdy(nsISSLSocketControl* ssl, SpdyVersion spdyVersion);
  // Like the above, but do the bare minimum to do 0RTT data, so we can back
  // it out, if necessary
  void Start0RTTSpdy(SpdyVersion spdyVersion);

  // Helpers for Start*Spdy
  nsresult TryTakeSubTransactions(nsTArray<RefPtr<nsAHttpTransaction> >& list);
  nsresult MoveTransactionsToSpdy(nsresult status,
                                  nsTArray<RefPtr<nsAHttpTransaction> >& list);

  // Directly Add a transaction to an active connection for SPDY
  [[nodiscard]] nsresult AddTransaction(nsAHttpTransaction*, int32_t);

  // Used to set TCP keepalives for fast detection of dead connections during
  // an initial period, and slower detection for long-lived connections.
  [[nodiscard]] nsresult StartShortLivedTCPKeepalives();
  [[nodiscard]] nsresult StartLongLivedTCPKeepalives();
  [[nodiscard]] nsresult DisableTCPKeepalives();

  bool CheckCanWrite0RTTData();
  void PostProcessNPNSetup(bool handshakeSucceeded, bool hasSecurityInfo,
                           bool earlyDataUsed);
  void Reset0RttForSpdy();
  void HandshakeDoneInternal();
  uint32_t TransactionCaps() const { return mTransactionCaps; }

 private:
  // mTransaction only points to the HTTP Transaction callbacks if the
  // transaction is open, otherwise it is null.
  RefPtr<nsAHttpTransaction> mTransaction;

  RefPtr<TlsHandshaker> mTlsHandshaker;

  nsCOMPtr<nsIAsyncInputStream> mSocketIn;
  nsCOMPtr<nsIAsyncOutputStream> mSocketOut;

  nsresult mSocketInCondition{NS_ERROR_NOT_INITIALIZED};
  nsresult mSocketOutCondition{NS_ERROR_NOT_INITIALIZED};

  nsWeakPtr mWeakTrans;  // Http2ConnectTransaction *

  RefPtr<nsHttpHandler> mHttpHandler;  // keep gHttpHandler alive

  PRIntervalTime mLastReadTime{0};
  PRIntervalTime mLastWriteTime{0};
  // max download time before dropping keep-alive status
  PRIntervalTime mMaxHangTime{0};
  PRIntervalTime mIdleTimeout;  // value of keep-alive: timeout=
  PRIntervalTime mConsiderReusedAfterInterval{0};
  PRIntervalTime mConsiderReusedAfterEpoch{0};
  int64_t mCurrentBytesRead{0};     // data read per activation
  int64_t mMaxBytesRead{0};         // max read in 1 activation
  int64_t mTotalBytesRead{0};       // total data read
  int64_t mContentBytesWritten{0};  // does not include CONNECT tunnel or TLS

  RefPtr<nsIAsyncInputStream> mInputOverflow;

  // Whether the first non-null transaction dispatched on this connection was
  // urgent-start or not
  bool mUrgentStartPreferred{false};
  // A flag to prevent reset of mUrgentStartPreferred by subsequent transactions
  bool mUrgentStartPreferredKnown{false};
  bool mConnectedTransport{false};
  // assume to keep-alive by default
  bool mKeepAlive{true};
  bool mKeepAliveMask{true};
  bool mDontReuse{false};
  bool mIsReused{false};
  bool mLastTransactionExpectedNoContent{false};
  bool mIdleMonitoring{false};
  bool mInSpdyTunnel{false};
  bool mForcePlainText{false};

  // A snapshot of current number of transfered bytes
  int64_t mTrafficCount{0};
  bool mTrafficStamp{false};  // true then the above is set

  // The number of <= HTTP/1.1 transactions performed on this connection. This
  // excludes spdy transactions.
  uint32_t mHttp1xTransactionCount{0};

  // Keep-Alive: max="mRemainingConnectionUses" provides the number of future
  // transactions (including the current one) that the server expects to allow
  // on this persistent connection.
  uint32_t mRemainingConnectionUses{0xffffffff};

  // version level in use, 0 if unused
  SpdyVersion mUsingSpdyVersion{SpdyVersion::NONE};

  RefPtr<ASpdySession> mSpdySession;
  int32_t mPriority{nsISupportsPriority::PRIORITY_NORMAL};
  bool mReportedSpdy{false};

  // mUsingSpdyVersion is cleared when mSpdySession is freed, this is permanent
  bool mEverUsedSpdy{false};

  // mLastHttpResponseVersion stores the last response's http version seen.
  HttpVersion mLastHttpResponseVersion{HttpVersion::v1_1};

  // If a large keepalive has been requested for any trans,
  // scale the default by this factor
  uint32_t mDefaultTimeoutFactor{1};

  bool mResponseTimeoutEnabled{false};

  // Flag to indicate connection is in inital keepalive period (fast detect).
  uint32_t mTCPKeepaliveConfig{kTCPKeepaliveDisabled};
  nsCOMPtr<nsITimer> mTCPKeepaliveTransitionTimer;

 private:
  // For ForceSend()
  static void ForceSendIO(nsITimer* aTimer, void* aClosure);
  [[nodiscard]] nsresult MaybeForceSendIO();
  bool mForceSendPending{false};
  nsCOMPtr<nsITimer> mForceSendTimer;

  int64_t mContentBytesWritten0RTT{0};
  bool mDid0RTTSpdy{false};

  nsresult mErrorBeforeConnect = NS_OK;

  nsCOMPtr<nsISocketTransport> mSocketTransport;

  bool mForWebSocket{false};

  std::function<void()> mContinueHandshakeDone{nullptr};

 private:
  bool mThroughCaptivePortal;
  int64_t mTotalBytesWritten = 0;  // does not include CONNECT tunnel

  nsCOMPtr<nsIInputStream> mProxyConnectStream;

  bool mRequestDone{false};
  bool mHasTLSTransportLayer{false};
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsHttpConnection, NS_HTTPCONNECTION_IID)

}  // namespace net
}  // namespace mozilla

#endif  // nsHttpConnection_h__
