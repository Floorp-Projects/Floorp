/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#ifndef nsHttpConnection_h__
#define nsHttpConnection_h__

#include "nsHttp.h"
#include "nsHttpConnectionInfo.h"
#include "nsAHttpConnection.h"
#include "nsAHttpTransaction.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "prlock.h"

#include "nsIStreamListener.h"
#include "nsISocketTransport.h"
#include "nsIEventQueue.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIInterfaceRequestor.h"

//-----------------------------------------------------------------------------
// nsHttpConnection - represents a connection to a HTTP server (or proxy)
//
// NOTE: this objects lives on the socket thread only.  it should not be
// accessed from any other thread.
//-----------------------------------------------------------------------------

class nsHttpConnection : public nsAHttpConnection
                       , public nsAHttpSegmentReader
                       , public nsAHttpSegmentWriter
                       , public nsIInputStreamNotify
                       , public nsIOutputStreamNotify
                       , public nsITransportEventSink
                       , public nsIInterfaceRequestor
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAMNOTIFY
    NS_DECL_NSIOUTPUTSTREAMNOTIFY
    NS_DECL_NSITRANSPORTEVENTSINK
    NS_DECL_NSIINTERFACEREQUESTOR

    nsHttpConnection();
    virtual ~nsHttpConnection();

    // Initialize the connection:
    //  info        - specifies the connection parameters.
    //  maxHangTime - limits the amount of time this connection can spend on a
    //                single transaction before it should no longer be kept 
    //                alive.  a value of 0xffff indicates no limit.
    nsresult Init(nsHttpConnectionInfo *info, PRUint16 maxHangTime);

    // Activate causes the given transaction to be processed on this
    // connection.  It fails if there is already an existing transaction.
    nsresult Activate(nsAHttpTransaction *, PRUint8 caps);

    // Close the underlying socket transport.
    void Close(nsresult reason);

    //-------------------------------------------------------------------------
    // XXX document when these are ok to call

    PRBool   SupportsPipelining() { return mSupportsPipelining; }
    PRBool   IsKeepAlive() { return mKeepAliveMask && mKeepAlive; }
    PRBool   CanReuse();   // can this connection be reused?
    void     DontReuse()   { mKeepAliveMask = PR_FALSE;
                             mKeepAlive = PR_FALSE;
                             mIdleTimeout = 0; }
    void     DropTransport() { DontReuse(); mSocketTransport = 0; }

    nsAHttpTransaction   *Transaction()    { return mTransaction; }
    nsHttpConnectionInfo *ConnectionInfo() { return mConnInfo; }

    // nsAHttpConnection methods:
    nsresult OnHeadersAvailable(nsAHttpTransaction *, nsHttpRequestHead *, nsHttpResponseHead *, PRBool *reset);
    void     CloseTransaction(nsAHttpTransaction *, nsresult reason);
    void     GetConnectionInfo(nsHttpConnectionInfo **ci) { NS_IF_ADDREF(*ci = mConnInfo); }
    void     GetSecurityInfo(nsISupports **);
    PRBool   IsPersistent() { return IsKeepAlive(); }
    nsresult PushBack(const char *data, PRUint32 length) { NS_NOTREACHED("PushBack"); return NS_ERROR_UNEXPECTED; }
    nsresult ResumeSend();
    nsresult ResumeRecv();

    // nsAHttpSegmentReader methods:
    nsresult OnReadSegment(const char *, PRUint32, PRUint32 *);

    // nsAHttpSegmentWriter methods:
    nsresult OnWriteSegment(char *, PRUint32, PRUint32 *);
 
    static NS_METHOD ReadFromStream(nsIInputStream *, void *, const char *,
                                    PRUint32, PRUint32, PRUint32 *);

private:
    // called to cause the underlying socket to start speaking SSL
    nsresult ProxyStartSSL();

    nsresult CreateTransport();
    nsresult OnTransactionDone(nsresult reason);
    nsresult OnSocketWritable();
    nsresult OnSocketReadable();

    nsresult SetupSSLProxyConnect();

    PRBool   IsAlive();
    PRBool   SupportsPipelining(nsHttpResponseHead *);
    
private:
    nsCOMPtr<nsISocketTransport>    mSocketTransport;
    nsCOMPtr<nsIAsyncInputStream>   mSocketIn;
    nsCOMPtr<nsIAsyncOutputStream>  mSocketOut;

    nsresult                        mSocketInCondition;
    nsresult                        mSocketOutCondition;

    nsCOMPtr<nsIInputStream>        mSSLProxyConnectStream;
    nsCOMPtr<nsIInputStream>        mRequestStream;

    nsAHttpTransaction             *mTransaction; // hard ref
    nsHttpConnectionInfo           *mConnInfo;    // hard ref

    PRLock                         *mLock;
    PRInt32                         mSuspendCount;

    PRUint32                        mLastReadTime;
    PRUint16                        mMaxHangTime;    // max download time before dropping keep-alive status
    PRUint16                        mIdleTimeout;    // value of keep-alive: timeout=

    nsresult                        mTransactionStatus;
    PRPackedBool                    mTransactionDone;

    PRPackedBool                    mKeepAlive;
    PRPackedBool                    mKeepAliveMask;
    PRPackedBool                    mSupportsPipelining;
};

#endif // nsHttpConnection_h__
