/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsHttpConnection_h__
#define nsHttpConnection_h__

#include "nsHttp.h"
#include "nsHttpConnectionInfo.h"
#include "nsAHttpConnection.h"
#include "nsAHttpTransaction.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "prinrval.h"
#include "SpdySession.h"

#include "nsIStreamListener.h"
#include "nsISocketTransport.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsIEventTarget.h"

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
                  nsIEventTarget *);

    // Activate causes the given transaction to be processed on this
    // connection.  It fails if there is already an existing transaction unless
    // a multiplexing protocol such as SPDY is being used
    nsresult Activate(nsAHttpTransaction *, PRUint8 caps, PRInt32 pri);

    // Close the underlying socket transport.
    void Close(nsresult reason);

    //-------------------------------------------------------------------------
    // XXX document when these are ok to call

    bool     SupportsPipelining() { return mSupportsPipelining; }
    bool     IsKeepAlive() { return mUsingSpdy ||
                                    (mKeepAliveMask && mKeepAlive); }
    bool     CanReuse();   // can this connection be reused?
    bool     CanDirectlyActivate();

    // Returns time in seconds for how long connection can be reused.
    PRUint32 TimeToLive();

    void     DontReuse();
    void     DropTransport() { DontReuse(); mSocketTransport = 0; }

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

    bool UsingSpdy() { return mUsingSpdy; }

    // When the connection is active this is called every 15 seconds
    void  ReadTimeoutTick(PRIntervalTime now);

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

    // Start the Spdy transaction handler when NPN indicates spdy/2
    void     StartSpdy();

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

    PRUint32                        mLastReadTime;
    PRIntervalTime                  mMaxHangTime;    // max download time before dropping keep-alive status
    PRIntervalTime                  mIdleTimeout;    // value of keep-alive: timeout=
    PRIntervalTime                  mConsiderReusedAfterInterval;
    PRIntervalTime                  mConsiderReusedAfterEpoch;
    PRInt64                         mCurrentBytesRead;   // data read per activation
    PRInt64                         mMaxBytesRead;       // max read in 1 activation
    PRInt64                         mTotalBytesRead;     // total data read

    nsRefPtr<nsIAsyncInputStream>   mInputOverflow;

    bool                            mKeepAlive;
    bool                            mKeepAliveMask;
    bool                            mSupportsPipelining;
    bool                            mIsReused;
    bool                            mCompletedProxyConnect;
    bool                            mLastTransactionExpectedNoContent;
    bool                            mIdleMonitoring;

    // The number of <= HTTP/1.1 transactions performed on this connection. This
    // excludes spdy transactions.
    PRUint32                        mHttp1xTransactionCount;

    // SPDY related
    bool                            mNPNComplete;
    bool                            mSetupNPNCalled;
    bool                            mUsingSpdy;
    nsRefPtr<mozilla::net::SpdySession> mSpdySession;
    PRInt32                         mPriority;
    bool                            mReportedSpdy;

    // mUsingSpdy is cleared when mSpdySession is freed, this is permanent
    bool                            mEverUsedSpdy;
};

#endif // nsHttpConnection_h__
