/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpConnection_h__
#define nsHttpConnection_h__

#include "nsHttpConnectionInfo.h"
#include "nsAHttpTransaction.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsProxyRelease.h"
#include "prinrval.h"
#include "TunnelUtils.h"

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

//-----------------------------------------------------------------------------
// nsHttpConnection - represents a connection to a HTTP server (or proxy)
//
// NOTE: this objects lives on the socket thread only.  it should not be
// accessed from any other thread.
//-----------------------------------------------------------------------------

class nsHttpConnection MOZ_FINAL : public nsAHttpSegmentReader
                                 , public nsAHttpSegmentWriter
                                 , public nsIInputStreamCallback
                                 , public nsIOutputStreamCallback
                                 , public nsITransportEventSink
                                 , public nsIInterfaceRequestor
                                 , public NudgeTunnelCallback
{
    virtual ~nsHttpConnection();

public:
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
    nsresult Init(nsHttpConnectionInfo *info, uint16_t maxHangTime,
                  nsISocketTransport *, nsIAsyncInputStream *,
                  nsIAsyncOutputStream *, bool connectedTransport,
                  nsIInterfaceRequestor *, PRIntervalTime);

    // Activate causes the given transaction to be processed on this
    // connection.  It fails if there is already an existing transaction unless
    // a multiplexing protocol such as SPDY is being used
    nsresult Activate(nsAHttpTransaction *, uint32_t caps, int32_t pri);

    // Close the underlying socket transport.
    void Close(nsresult reason);

    //-------------------------------------------------------------------------
    // XXX document when these are ok to call

    bool SupportsPipelining();
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
    nsresult OnHeadersAvailable(nsAHttpTransaction *, nsHttpRequestHead *, nsHttpResponseHead *, bool *reset);
    void     CloseTransaction(nsAHttpTransaction *, nsresult reason);
    void     GetConnectionInfo(nsHttpConnectionInfo **ci) { NS_IF_ADDREF(*ci = mConnInfo); }
    nsresult TakeTransport(nsISocketTransport **,
                           nsIAsyncInputStream **,
                           nsIAsyncOutputStream **);
    void     GetSecurityInfo(nsISupports **);
    bool     IsPersistent() { return IsKeepAlive() && !mDontReuse; }
    bool     IsReused();
    void     SetIsReusedAfter(uint32_t afterMilliseconds);
    nsresult PushBack(const char *data, uint32_t length);
    nsresult ResumeSend();
    nsresult ResumeRecv();
    int64_t  MaxBytesRead() {return mMaxBytesRead;}
    uint8_t GetLastHttpResponseVersion() { return mLastHttpResponseVersion; }

    friend class nsHttpConnectionForceIO;
    nsresult ForceSend();
    nsresult ForceRecv();

    static NS_METHOD ReadFromStream(nsIInputStream *, void *, const char *,
                                    uint32_t, uint32_t, uint32_t *);

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

    nsAHttpTransaction::Classifier Classification() { return mClassification; }
    void Classify(nsAHttpTransaction::Classifier newclass)
    {
        mClassification = newclass;
    }

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

    static nsresult MakeConnectString(nsAHttpTransaction *trans,
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
            (mTrafficCount == (mTotalBytesWritten + mTotalBytesRead));
    }
    // override of nsAHttpConnection
    virtual uint32_t Version();

private:
    // Value (set in mTCPKeepaliveConfig) indicates which set of prefs to use.
    enum TCPKeepaliveConfig {
      kTCPKeepaliveDisabled = 0,
      kTCPKeepaliveShortLivedConfig,
      kTCPKeepaliveLongLivedConfig
    };

    // called to cause the underlying socket to start speaking SSL
    nsresult InitSSLParams(bool connectingToProxy, bool ProxyStartSSL);
    nsresult SetupNPNList(nsISSLSocketControl *ssl, uint32_t caps);

    nsresult OnTransactionDone(nsresult reason);
    nsresult OnSocketWritable();
    nsresult OnSocketReadable();

    nsresult SetupProxyConnect();

    PRIntervalTime IdleTime();
    bool     IsAlive();
    bool     SupportsPipelining(nsHttpResponseHead *);

    // Makes certain the SSL handshake is complete and NPN negotiation
    // has had a chance to happen
    bool     EnsureNPNComplete();
    void     SetupSSL();

    // Start the Spdy transaction handler when NPN indicates spdy/*
    void     StartSpdy(uint8_t versionLevel);

    // Directly Add a transaction to an active connection for SPDY
    nsresult AddTransaction(nsAHttpTransaction *, int32_t);

    // Used to set TCP keepalives for fast detection of dead connections during
    // an initial period, and slower detection for long-lived connections.
    nsresult StartShortLivedTCPKeepalives();
    nsresult StartLongLivedTCPKeepalives();
    nsresult DisableTCPKeepalives();

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
    nsRefPtr<nsAHttpTransaction>    mTransaction;
    nsRefPtr<TLSFilterTransaction>  mTLSFilter;

    nsRefPtr<nsHttpHandler>         mHttpHandler; // keep gHttpHandler alive

    Mutex                           mCallbacksLock;
    nsMainThreadPtrHandle<nsIInterfaceRequestor> mCallbacks;

    nsRefPtr<nsHttpConnectionInfo> mConnInfo;

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

    nsRefPtr<nsIAsyncInputStream>   mInputOverflow;

    PRIntervalTime                  mRtt;

    bool                            mConnectedTransport;
    bool                            mKeepAlive;
    bool                            mKeepAliveMask;
    bool                            mDontReuse;
    bool                            mSupportsPipelining;
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

    nsAHttpTransaction::Classifier  mClassification;

    // SPDY related
    bool                            mNPNComplete;
    bool                            mSetupSSLCalled;

    // version level in use, 0 if unused
    uint8_t                         mUsingSpdyVersion;

    nsRefPtr<ASpdySession>          mSpdySession;
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
};

}} // namespace mozilla::net

#endif // nsHttpConnection_h__
