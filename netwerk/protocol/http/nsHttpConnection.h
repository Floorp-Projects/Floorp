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
#include "prlock.h"

#include "nsIStreamListener.h"
#include "nsISocketTransport.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIInterfaceRequestor.h"

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

    // Returns time in seconds for how long connection can be reused.
    PRUint32 TimeToLive();

    void     DontReuse()   { mKeepAliveMask = PR_FALSE;
                             mKeepAlive = PR_FALSE;
                             mIdleTimeout = 0; }
    void     DropTransport() { DontReuse(); mSocketTransport = 0; }

    nsAHttpTransaction   *Transaction()    { return mTransaction; }
    nsHttpConnectionInfo *ConnectionInfo() { return mConnInfo; }

    // nsAHttpConnection compatible methods (non-virtual):
    nsresult OnHeadersAvailable(nsAHttpTransaction *, nsHttpRequestHead *, nsHttpResponseHead *, PRBool *reset);
    void     CloseTransaction(nsAHttpTransaction *, nsresult reason);
    void     GetConnectionInfo(nsHttpConnectionInfo **ci) { NS_IF_ADDREF(*ci = mConnInfo); }
    void     GetSecurityInfo(nsISupports **);
    PRBool   IsPersistent() { return IsKeepAlive(); }
    PRBool   IsReused() { return mIsReused; }
    nsresult PushBack(const char *data, PRUint32 length) { NS_NOTREACHED("PushBack"); return NS_ERROR_UNEXPECTED; }
    nsresult ResumeSend();
    nsresult ResumeRecv();

    static NS_METHOD ReadFromStream(nsIInputStream *, void *, const char *,
                                    PRUint32, PRUint32, PRUint32 *);

private:
    // called to cause the underlying socket to start speaking SSL
    nsresult ProxyStartSSL();

    nsresult CreateTransport(PRUint8 caps);
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

    PRUint32                        mLastReadTime;
    PRUint16                        mMaxHangTime;    // max download time before dropping keep-alive status
    PRUint16                        mIdleTimeout;    // value of keep-alive: timeout=

    PRPackedBool                    mKeepAlive;
    PRPackedBool                    mKeepAliveMask;
    PRPackedBool                    mSupportsPipelining;
    PRPackedBool                    mIsReused;
    PRPackedBool                    mCompletedSSLConnect;
};

#endif // nsHttpConnection_h__
