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
#include "prinrval.h"
#include "ASpdySession.h"
#include "mozilla/TimeStamp.h"

#include "nsIStreamListener.h"
#include "nsISocketTransport.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsIEventTarget.h"

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
    nsresult Init(nsHttpConnectionInfo *info, PRUint16 maxHangTime,
                  nsISocketTransport *, nsIAsyncInputStream *,
                  nsIAsyncOutputStream *, nsIInterfaceRequestor *,
                  nsIEventTarget *, PRIntervalTime);

    // Activate causes the given transaction to be processed on this
    // connection.  It fails if there is already an existing transaction unless
    // a multiplexing protocol such as SPDY is being used
    nsresult Activate(nsAHttpTransaction *, PRUint8 caps, PRInt32 pri);

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
    PRUint32 TimeToLive();

    void     DontReuse();
    void     DropTransport() { DontReuse(); mSocketTransport = 0; }

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
    void     SetIsReusedAfter(PRUint32 afterMilliseconds);
    void     SetIdleTimeout(PRIntervalTime val) {mIdleTimeout = val;}
    nsresult PushBack(const char *data, PRUint32 length);
    nsresult ResumeSend();
    nsresult ResumeRecv();
    PRInt64  MaxBytesRead() {return mMaxBytesRead;}

    static NS_METHOD ReadFromStream(nsIInputStream *, void *, const char *,
                                    PRUint32, PRUint32, PRUint32 *);

    // When a persistent connection is in the connection manager idle 
    // connection pool, the nsHttpConnection still reads errors and hangups
    // on the socket so that it can be proactively released if the server
    // initiates a termination. Only call on socket thread.
    void BeginIdleMonitoring();
    void EndIdleMonitoring();

    bool UsingSpdy() { return !!mUsingSpdyVersion; }
    bool EverUsedSpdy() { return mEverUsedSpdy; }

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

    PRInt64 BytesWritten() { return mTotalBytesWritten; }

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
    void     SetupNPN(PRUint8 caps);

    // Inform the connection manager of any SPDY Alternate-Protocol
    // redirections
    void     HandleAlternateProtocol(nsHttpResponseHead *);

    // Start the Spdy transaction handler when NPN indicates spdy/*
    void     StartSpdy(PRUint8 versionLevel);

    // Directly Add a transaction to an active connection for SPDY
    nsresult AddTransaction(nsAHttpTransaction *, PRInt32);

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

    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
    nsCOMPtr<nsIEventTarget>        mCallbackTarget;

    nsRefPtr<nsHttpConnectionInfo> mConnInfo;

    PRIntervalTime                  mLastReadTime;
    PRIntervalTime                  mMaxHangTime;    // max download time before dropping keep-alive status
    PRIntervalTime                  mIdleTimeout;    // value of keep-alive: timeout=
    PRIntervalTime                  mConsiderReusedAfterInterval;
    PRIntervalTime                  mConsiderReusedAfterEpoch;
    PRInt64                         mCurrentBytesRead;   // data read per activation
    PRInt64                         mMaxBytesRead;       // max read in 1 activation
    PRInt64                         mTotalBytesRead;     // total data read
    PRInt64                         mTotalBytesWritten;  // does not include CONNECT tunnel

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

    // The number of <= HTTP/1.1 transactions performed on this connection. This
    // excludes spdy transactions.
    PRUint32                        mHttp1xTransactionCount;

    // Keep-Alive: max="mRemainingConnectionUses" provides the number of future
    // transactions (including the current one) that the server expects to allow
    // on this persistent connection.
    PRUint32                        mRemainingConnectionUses;

    nsAHttpTransaction::Classifier  mClassification;

    // SPDY related
    bool                            mNPNComplete;
    bool                            mSetupNPNCalled;

    // version level in use, 0 if unused
    PRUint8                         mUsingSpdyVersion;

    nsRefPtr<mozilla::net::ASpdySession> mSpdySession;
    PRInt32                         mPriority;
    bool                            mReportedSpdy;

    // mUsingSpdyVersion is cleared when mSpdySession is freed, this is permanent
    bool                            mEverUsedSpdy;
};

#endif // nsHttpConnection_h__
