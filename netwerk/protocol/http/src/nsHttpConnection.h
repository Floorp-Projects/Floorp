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
#include "nsIStreamListener.h"
#include "nsIStreamProvider.h"
#include "nsISocketTransport.h"
#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIEventQueue.h"
#include "nsIInputStream.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "prlock.h"

class nsHttpHandler;

//-----------------------------------------------------------------------------
// nsHttpConnection - represents a connection to a HTTP server (or proxy)
//-----------------------------------------------------------------------------

class nsHttpConnection : public nsAHttpConnection
                       , public nsIStreamListener
                       , public nsIStreamProvider
                       , public nsIProgressEventSink
                       , public nsIInterfaceRequestor
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSISTREAMPROVIDER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIPROGRESSEVENTSINK

    nsHttpConnection();
    virtual ~nsHttpConnection();

    // Initialize the connection:
    //  info        - specifies the connection parameters.
    //  maxHangTime - limits the amount of time this connection can spend on a
    //                single transaction before it should no longer be kept 
    //                alive.  a value of 0xffff indicates no limit.
    nsresult Init(nsHttpConnectionInfo *info, PRUint16 maxHangTime);

    // SetTransaction causes the given transaction to be processed on this
    // connection.  It fails if there is already an existing transaction.
    nsresult SetTransaction(nsAHttpTransaction *, PRUint8 capabilities);

    // called to cause the underlying socket to start speaking SSL
    nsresult ProxyStepUp();

    PRBool   IsKeepAlive() { return mKeepAliveMask && mKeepAlive; }
    PRBool   CanReuse();   // can this connection be reused?
    void     DontReuse()   { mKeepAliveMask = PR_FALSE;
                             mKeepAlive = PR_FALSE;
                             mIdleTimeout = 0; }

    nsAHttpTransaction   *Transaction()    { return mTransaction; }
    nsHttpConnectionInfo *ConnectionInfo() { return mConnectionInfo; }

    // nsAHttpConnection methods:
    nsresult OnHeadersAvailable(nsAHttpTransaction *, nsHttpResponseHead *, PRBool *reset);
    nsresult OnTransactionComplete(nsAHttpTransaction *, nsresult status);
    nsresult OnSuspend();
    nsresult OnResume();
    void     GetConnectionInfo(nsHttpConnectionInfo **ci) { NS_IF_ADDREF(*ci = mConnectionInfo); }
    void     DropTransaction(nsAHttpTransaction *);
    PRBool   IsPersistent() { return IsKeepAlive(); }
    nsresult PushBack(char *data, PRUint32 length) { return NS_OK; }

private:
    nsresult ActivateConnection();
    nsresult CreateTransport();

    nsresult SetupSSLProxyConnect();

    PRBool   IsAlive();

private:
    nsCOMPtr<nsISocketTransport>    mSocketTransport;
    nsCOMPtr<nsIRequest>            mWriteRequest;
    nsCOMPtr<nsIRequest>            mReadRequest;

    nsCOMPtr<nsIInputStream>        mSSLProxyConnectStream;

    nsAHttpTransaction             *mTransaction;    // hard ref
    nsHttpConnectionInfo           *mConnectionInfo; // hard ref

    PRLock                         *mLock;

    PRUint32                        mReadStartTime;  // time of OnStartRequest
    PRUint32                        mLastActiveTime;
    PRUint16                        mMaxHangTime;    // max download time before dropping keep-alive status
    PRUint16                        mIdleTimeout;    // value of keep-alive: timeout=

    PRPackedBool                    mKeepAlive;
    PRPackedBool                    mKeepAliveMask;
    PRPackedBool                    mWriteDone;
    PRPackedBool                    mReadDone;
};

#endif // nsHttpConnection_h__
