/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpConnection_h__
#define nsHttpConnection_h__

#include "nsHttpConnectionInfo.h"
#include "nsHttpResponseHead.h"
#include "nsAHttpTransaction.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsProxyRelease.h"
#include "prinrval.h"
#include "TunnelUtils.h"
#include "mozilla/Mutex.h"
#include "ARefBase.h"
#include "TimingStruct.h"

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
#define NS_HTTPCONNECTION_IID \
{ 0x1dcc863e, 0xdb90, 0x4652, {0xa1, 0xfe, 0x13, 0xfe, 0xa0, 0xb5, 0x4e, 0x46 }}

//-----------------------------------------------------------------------------
// nsHttpConnection - represents a connection to a HTTP server (or proxy)
//
// NOTE: this objects lives on the socket thread only.  it should not be
// accessed from any other thread.
//-----------------------------------------------------------------------------

class nsHttpConnection final : public nsAHttpSegmentReader
                             , public nsAHttpSegmentWriter
                             , public nsIInputStreamCallback
                             , public nsIOutputStreamCallback
                             , public nsITransportEventSink
                             , public nsIInterfaceRequestor
                             , public NudgeTunnelCallback
                             , public ARefBase
                             , public nsSupportsWeakReference
{
    virtual ~nsHttpConnection();

public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_HTTPCONNECTION_IID)
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSAHTTPSEGMENTREADER
    NS_DECL_NSAHTTPSEGMENTWRITER
    NS_DECL_NSIINPUTSTREAMCALLBACK
    NS_DECL_NSIOUTPUTSTREAMCALLBACK
    NS_DECL_NSITRANSPORTEVENTSINK
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NUDGETUNNELCALLBACK

    nsHttpConnection();

    // Initialize the connection:
    //  info        - specifies the connection parameters.
    //  maxHangTime - limits the amount of time this connection can spend on a
    //                single transaction before it should no longer be kept
    //                alive.  a value of 0xffff indicates no limit.
    MOZ_MUST_USE nsresult Init(nsHttpConnectionInfo *info, uint16_t maxHangTime,
                               nsISocketTransport *, nsIAsyncInputStream *,
                               nsIAsyncOutputStream *, bool connectedTransport,
                               nsIInterfaceRequestor *, PRIntervalTime);

    // Activate causes the given transaction to be processed on this
    // connection.  It fails if there is already an existing transaction unless
    // a multiplexing protocol such as SPDY is being used
    MOZ_MUST_USE nsresult Activate(nsAHttpTransaction *, uint32_t caps,
                                   int32_t pri);

    void SetFastOpen(bool aFastOpen);
    // Close this connection and return the transaction. The transaction is
    // restarted as well. This will only happened before connection is
    // connected.
    nsAHttpTransaction * CloseConnectionFastOpenTakesTooLongOrError(bool aCloseocketTransport);

    // Close the underlying socket transport.
    void Close(nsresult reason, bool aIsShutdown = false);

    //-------------------------------------------------------------------------
    // XXX document when these are ok to call

    bool IsKeepAlive()
    {
        return mUsingSpdyVersion || (mKeepAliveMask && mKeepAlive);
    }
    bool CanReuse();   // can this connection be reused?
    bool CanDirectlyActivate();

    // Returns time in seconds for how long connection can be reused.
    uint32_t TimeToLive();

    void DontReuse();

    bool IsProxyConnectInProgress()
    {
        return mProxyConnectInProgress;
    }

    bool LastTransactionExpectedNoContent()
    {
        return mLastTransactionExpectedNoContent;
    }

    void SetLastTransactionExpectedNoContent(bool val)
    {
        mLastTransactionExpectedNoContent = val;
    }

    bool NeedSpdyTunnel()
    {
        return mConnInfo->UsingHttpsProxy() && !mTLSFilter && mConnInfo->UsingConnect();
    }

    // A connection is forced into plaintext when it is intended to be used as a CONNECT
    // tunnel but the setup fails. The plaintext only carries the CONNECT error.
    void ForcePlainText()
    {
        mForcePlainText = true;
    }

    nsISocketTransport   *Transport()      { return mSocketTransport; }
    nsAHttpTransaction   *Transaction()    { return mTransaction; }
    nsHttpConnectionInfo *ConnectionInfo() { return mConnInfo; }

    // nsAHttpConnection compatible methods (non-virtual):
    MOZ_MUST_USE nsresult OnHeadersAvailable(nsAHttpTransaction *,
                                             nsHttpRequestHead *,
                                             nsHttpResponseHead *, bool *reset);
    void     CloseTransaction(nsAHttpTransaction *, nsresult reason,
                              bool aIsShutdown = false);
    void     GetConnectionInfo(nsHttpConnectionInfo **ci) { NS_IF_ADDREF(*ci = mConnInfo); }
    MOZ_MUST_USE nsresult TakeTransport(nsISocketTransport **,
                                        nsIAsyncInputStream **,
                                        nsIAsyncOutputStream **);
    void     GetSecurityInfo(nsISupports **);
    bool     IsPersistent() { return IsKeepAlive() && !mDontReuse; }
    bool     IsReused();
    void     SetIsReusedAfter(uint32_t afterMilliseconds);
    MOZ_MUST_USE nsresult PushBack(const char *data, uint32_t length);
    MOZ_MUST_USE nsresult ResumeSend();
    MOZ_MUST_USE nsresult ResumeRecv();
    int64_t  MaxBytesRead() {return mMaxBytesRead;}
    uint8_t GetLastHttpResponseVersion() { return mLastHttpResponseVersion; }

    friend class HttpConnectionForceIO;
    MOZ_MUST_USE nsresult ForceSend();
    MOZ_MUST_USE nsresult ForceRecv();

    static MOZ_MUST_USE nsresult ReadFromStream(nsIInputStream *, void *,
                                                const char *, uint32_t,
                                                uint32_t, uint32_t *);

    // When a persistent connection is in the connection manager idle
    // connection pool, the nsHttpConnection still reads errors and hangups
    // on the socket so that it can be proactively released if the server
    // initiates a termination. Only call on socket thread.
    void BeginIdleMonitoring();
    void EndIdleMonitoring();

    bool UsingSpdy() { return !!mUsingSpdyVersion; }
    uint8_t GetSpdyVersion() { return mUsingSpdyVersion; }
    bool EverUsedSpdy() { return mEverUsedSpdy; }
    PRIntervalTime Rtt() { return mRtt; }

    // true when connection SSL NPN phase is complete and we know
    // authoritatively whether UsingSpdy() or not.
    bool ReportedNPN() { return mReportedSpdy; }

    // When the connection is active this is called up to once every 1 second
    // return the interval (in seconds) that the connection next wants to
    // have this invoked. It might happen sooner depending on the needs of
    // other connections.
    uint32_t  ReadTimeoutTick(PRIntervalTime now);

    // For Active and Idle connections, this will be called when
    // mTCPKeepaliveTransitionTimer fires, to check if the TCP keepalive config
    // should move from short-lived (fast-detect) to long-lived.
    static void UpdateTCPKeepalive(nsITimer *aTimer, void *aClosure);

    // When the connection is active this is called every second
    void  ReadTimeoutTick();

    int64_t BytesWritten() { return mTotalBytesWritten; } // includes TLS
    int64_t ContentBytesWritten() { return mContentBytesWritten; }

    void    SetSecurityCallbacks(nsIInterfaceRequestor* aCallbacks);
    void    PrintDiagnostics(nsCString &log);

    void    SetTransactionCaps(uint32_t aCaps) { mTransactionCaps = aCaps; }

    // IsExperienced() returns true when the connection has started at least one
    // non null HTTP transaction of any version.
    bool    IsExperienced() { return mExperienced; }

    static MOZ_MUST_USE nsresult MakeConnectString(nsAHttpTransaction *trans,
                                                   nsHttpRequestHead *request,
                                                   nsACString &result);
    void    SetupSecondaryTLS();
    void    SetInSpdyTunnel(bool arg);

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
    // override of nsAHttpConnection
    virtual uint32_t Version();

    bool TestJoinConnection(const nsACString &hostname, int32_t port);
    bool JoinConnection(const nsACString &hostname, int32_t port);

    void SetFastOpenStatus(uint8_t tfoStatus);
    uint8_t GetFastOpenStatus() {
      return mFastOpenStatus;
    }

    void SetEvent(nsresult aStatus);

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
    MOZ_MUST_USE nsresult SetupNPNList(nsISSLSocketControl *ssl, uint32_t caps);

    MOZ_MUST_USE nsresult OnTransactionDone(nsresult reason);
    MOZ_MUST_USE nsresult OnSocketWritable();
    MOZ_MUST_USE nsresult OnSocketReadable();

    MOZ_MUST_USE nsresult SetupProxyConnect();

    PRIntervalTime IdleTime();
    bool     IsAlive();

    // Makes certain the SSL handshake is complete and NPN negotiation
    // has had a chance to happen
    MOZ_MUST_USE bool EnsureNPNComplete(nsresult &aOut0RTTWriteHandshakeValue,
                                        uint32_t &aOut0RTTBytesWritten);
    void     SetupSSL();

    // Start the Spdy transaction handler when NPN indicates spdy/*
    void     StartSpdy(uint8_t versionLevel);
    // Like the above, but do the bare minimum to do 0RTT data, so we can back
    // it out, if necessary
    void     Start0RTTSpdy(uint8_t versionLevel);

    // Helpers for Start*Spdy
    nsresult TryTakeSubTransactions(nsTArray<RefPtr<nsAHttpTransaction> > &list);
    nsresult MoveTransactionsToSpdy(nsresult status, nsTArray<RefPtr<nsAHttpTransaction> > &list);

    // Directly Add a transaction to an active connection for SPDY
    MOZ_MUST_USE nsresult AddTransaction(nsAHttpTransaction *, int32_t);

    // Used to set TCP keepalives for fast detection of dead connections during
    // an initial period, and slower detection for long-lived connections.
    MOZ_MUST_USE nsresult StartShortLivedTCPKeepalives();
    MOZ_MUST_USE nsresult StartLongLivedTCPKeepalives();
    MOZ_MUST_USE nsresult DisableTCPKeepalives();

private:
    nsCOMPtr<nsISocketTransport>    mSocketTransport;
    nsCOMPtr<nsIAsyncInputStream>   mSocketIn;
    nsCOMPtr<nsIAsyncOutputStream>  mSocketOut;

    nsresult                        mSocketInCondition;
    nsresult                        mSocketOutCondition;

    nsCOMPtr<nsIInputStream>        mProxyConnectStream;
    nsCOMPtr<nsIInputStream>        mRequestStream;

    // mTransaction only points to the HTTP Transaction callbacks if the
    // transaction is open, otherwise it is null.
    RefPtr<nsAHttpTransaction>    mTransaction;
    RefPtr<TLSFilterTransaction>  mTLSFilter;

    RefPtr<nsHttpHandler>         mHttpHandler; // keep gHttpHandler alive

    Mutex                           mCallbacksLock;
    nsMainThreadPtrHandle<nsIInterfaceRequestor> mCallbacks;

    RefPtr<nsHttpConnectionInfo> mConnInfo;

    PRIntervalTime                  mLastReadTime;
    PRIntervalTime                  mLastWriteTime;
    PRIntervalTime                  mMaxHangTime;    // max download time before dropping keep-alive status
    PRIntervalTime                  mIdleTimeout;    // value of keep-alive: timeout=
    PRIntervalTime                  mConsiderReusedAfterInterval;
    PRIntervalTime                  mConsiderReusedAfterEpoch;
    int64_t                         mCurrentBytesRead;   // data read per activation
    int64_t                         mMaxBytesRead;       // max read in 1 activation
    int64_t                         mTotalBytesRead;     // total data read
    int64_t                         mTotalBytesWritten;  // does not include CONNECT tunnel
    int64_t                         mContentBytesWritten;  // does not include CONNECT tunnel or TLS

    RefPtr<nsIAsyncInputStream>   mInputOverflow;

    PRIntervalTime                  mRtt;

    bool                            mConnectedTransport;
    bool                            mKeepAlive;
    bool                            mKeepAliveMask;
    bool                            mDontReuse;
    bool                            mIsReused;
    bool                            mCompletedProxyConnect;
    bool                            mLastTransactionExpectedNoContent;
    bool                            mIdleMonitoring;
    bool                            mProxyConnectInProgress;
    bool                            mExperienced;
    bool                            mInSpdyTunnel;
    bool                            mForcePlainText;

    // A snapshot of current number of transfered bytes
    int64_t                         mTrafficCount;
    bool                            mTrafficStamp; // true then the above is set

    // The number of <= HTTP/1.1 transactions performed on this connection. This
    // excludes spdy transactions.
    uint32_t                        mHttp1xTransactionCount;

    // Keep-Alive: max="mRemainingConnectionUses" provides the number of future
    // transactions (including the current one) that the server expects to allow
    // on this persistent connection.
    uint32_t                        mRemainingConnectionUses;

    // SPDY related
    bool                            mNPNComplete;
    bool                            mSetupSSLCalled;

    // version level in use, 0 if unused
    uint8_t                         mUsingSpdyVersion;

    RefPtr<ASpdySession>            mSpdySession;
    int32_t                         mPriority;
    bool                            mReportedSpdy;

    // mUsingSpdyVersion is cleared when mSpdySession is freed, this is permanent
    bool                            mEverUsedSpdy;

    // mLastHttpResponseVersion stores the last response's http version seen.
    uint8_t                         mLastHttpResponseVersion;

    // The capabailities associated with the most recent transaction
    uint32_t                        mTransactionCaps;

    bool                            mResponseTimeoutEnabled;

    // Flag to indicate connection is in inital keepalive period (fast detect).
    uint32_t                        mTCPKeepaliveConfig;
    nsCOMPtr<nsITimer>              mTCPKeepaliveTransitionTimer;

private:
    // For ForceSend()
    static void                     ForceSendIO(nsITimer *aTimer, void *aClosure);
    MOZ_MUST_USE nsresult           MaybeForceSendIO();
    bool                            mForceSendPending;
    nsCOMPtr<nsITimer>              mForceSendTimer;

    // Helper variable for 0RTT handshake;
    bool                            m0RTTChecked; // Possible 0RTT has been
                                                  // checked.
    bool                            mWaitingFor0RTTResponse; // We have are
                                                             // sending 0RTT
                                                             // data and we
                                                             // are waiting
                                                             // for the end of
                                                             // the handsake.
    int64_t                        mContentBytesWritten0RTT;
    bool                           mEarlyDataNegotiated; //Only used for telemetry
    nsCString                      mEarlyNegotiatedALPN;
    bool                           mDid0RTTSpdy;

    bool                           mFastOpen;
    uint8_t                        mFastOpenStatus;

    bool                           mForceSendDuringFastOpenPending;
    bool                           mReceivedSocketWouldBlockDuringFastOpen;
    bool                           mCheckNetworkStallsWithTFO;
    PRIntervalTime                 mLastRequestBytesSentTime;

public:
    void BootstrapTimings(TimingStruct times);
private:
    TimingStruct    mBootstrappedTimings;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsHttpConnection, NS_HTTPCONNECTION_IID)

} // namespace net
} // namespace mozilla

#endif // nsHttpConnection_h__
