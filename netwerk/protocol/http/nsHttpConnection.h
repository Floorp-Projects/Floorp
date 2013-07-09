/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpConnection_h__
#define nsHttpConnection_h__

#include "nsHttp.h"
#include "nsHttpConnectionInfo.h"
#include "nsAHttpTransaction.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsProxyRelease.h"
#include "prinrval.h"
#include "ASpdySession.h"
#include "mozilla/TimeStamp.h"

#include "nsIStreamListener.h"
#include "nsISocketTransport.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIInterfaceRequestor.h"

class nsHttpRequestHead;
class nsHttpResponseHead;

//-----------------------------------------------------------------------------
// nsHttpConnection - represents a connection to a HTTP server (or proxy)
//
// NOTE: this objects lives on the socket thread only.  it should not be
// accessed from any other thread.
//-----------------------------------------------------------------------------

class nsHttpConnection : public nsAHttpSegmentReader
                       , public nsAHttpSegmentWriter
                       , public nsIInputStreamCallback
                       , public nsIOutputStreamCallback
                       , public nsITransportEventSink
                       , public nsIInterfaceRequestor
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSAHTTPSEGMENTREADER
    NS_DECL_NSAHTTPSEGMENTWRITER
    NS_DECL_NSIINPUTSTREAMCALLBACK
    NS_DECL_NSIOUTPUTSTREAMCALLBACK
    NS_DECL_NSITRANSPORTEVENTSINK
    NS_DECL_NSIINTERFACEREQUESTOR

    nsHttpConnection();
    virtual ~nsHttpConnection();

    // Initialize the connection:
    //  info        - specifies the connection parameters.
    //  maxHangTime - limits the amount of time this connection can spend on a
    //                single transaction before it should no longer be kept
    //                alive.  a value of 0xffff indicates no limit.
    nsresult Init(nsHttpConnectionInfo *info, uint16_t maxHangTime,
                  nsISocketTransport *, nsIAsyncInputStream *,
                  nsIAsyncOutputStream *, nsIInterfaceRequestor *,
                  PRIntervalTime);

    // Activate causes the given transaction to be processed on this
    // connection.  It fails if there is already an existing transaction unless
    // a multiplexing protocol such as SPDY is being used
    nsresult Activate(nsAHttpTransaction *, uint32_t caps, int32_t pri);

    // Close the underlying socket transport.
    void Close(nsresult reason);

    //-------------------------------------------------------------------------
    // XXX document when these are ok to call

    bool     SupportsPipelining();
    bool     IsKeepAlive() { return mUsingSpdyVersion ||
                                    (mKeepAliveMask && mKeepAlive); }
    bool     CanReuse();   // can this connection be reused?
    bool     CanDirectlyActivate();

    // Returns time in seconds for how long connection can be reused.
    uint32_t TimeToLive();

    void     DontReuse();

    bool     IsProxyConnectInProgress()
    {
        return mProxyConnectInProgress;
    }

    bool     LastTransactionExpectedNoContent()
    {
        return mLastTransactionExpectedNoContent;
    }

    void     SetLastTransactionExpectedNoContent(bool val)
    {
        mLastTransactionExpectedNoContent = val;
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
    bool     IsPersistent() { return IsKeepAlive(); }
    bool     IsReused();
    void     SetIsReusedAfter(uint32_t afterMilliseconds);
    void     SetIdleTimeout(PRIntervalTime val) {mIdleTimeout = val;}
    nsresult PushBack(const char *data, uint32_t length);
    nsresult ResumeSend();
    nsresult ResumeRecv();
    int64_t  MaxBytesRead() {return mMaxBytesRead;}
    uint8_t GetLastHttpResponseVersion() { return mLastHttpResponseVersion; }

    friend class nsHttpConnectionForceRecv;
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

    // When the connection is active this is called every 1 second
    void  ReadTimeoutTick(PRIntervalTime now);

    nsAHttpTransaction::Classifier Classification() { return mClassification; }
    void Classify(nsAHttpTransaction::Classifier newclass)
    {
        mClassification = newclass;
    }

    // When the connection is active this is called every second
    void  ReadTimeoutTick();

    int64_t BytesWritten() { return mTotalBytesWritten; }

    void    SetSecurityCallbacks(nsIInterfaceRequestor* aCallbacks);
    void    PrintDiagnostics(nsCString &log);

    void    SetTransactionCaps(uint32_t aCaps) { mTransactionCaps = aCaps; }

    // IsExperienced() returns true when the connection has started at least one
    // non null HTTP transaction of any version.
    bool    IsExperienced() { return mExperienced; }

private:
    // called to cause the underlying socket to start speaking SSL
    nsresult ProxyStartSSL();

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
    void     SetupSSL(uint32_t caps);

    // Start the Spdy transaction handler when NPN indicates spdy/*
    void     StartSpdy(uint8_t versionLevel);

    // Directly Add a transaction to an active connection for SPDY
    nsresult AddTransaction(nsAHttpTransaction *, int32_t);

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

    mozilla::Mutex                  mCallbacksLock;
    nsMainThreadPtrHandle<nsIInterfaceRequestor> mCallbacks;

    nsRefPtr<nsHttpConnectionInfo> mConnInfo;

    PRIntervalTime                  mLastReadTime;
    PRIntervalTime                  mMaxHangTime;    // max download time before dropping keep-alive status
    PRIntervalTime                  mIdleTimeout;    // value of keep-alive: timeout=
    PRIntervalTime                  mConsiderReusedAfterInterval;
    PRIntervalTime                  mConsiderReusedAfterEpoch;
    int64_t                         mCurrentBytesRead;   // data read per activation
    int64_t                         mMaxBytesRead;       // max read in 1 activation
    int64_t                         mTotalBytesRead;     // total data read
    int64_t                         mTotalBytesWritten;  // does not include CONNECT tunnel

    nsRefPtr<nsIAsyncInputStream>   mInputOverflow;

    PRIntervalTime                  mRtt;

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

    nsRefPtr<mozilla::net::ASpdySession> mSpdySession;
    int32_t                         mPriority;
    bool                            mReportedSpdy;

    // mUsingSpdyVersion is cleared when mSpdySession is freed, this is permanent
    bool                            mEverUsedSpdy;

    // mLastHttpResponseVersion stores the last response's http version seen.
    uint8_t                         mLastHttpResponseVersion;

    // The capabailities associated with the most recent transaction
    uint32_t                        mTransactionCaps;
};

#endif // nsHttpConnection_h__
