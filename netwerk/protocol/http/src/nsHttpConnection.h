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
#include "nsIStreamListener.h"
#include "nsIStreamProvider.h"
#include "nsISocketTransport.h"
#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIEventQueue.h"
#include "nsIInputStream.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "plstr.h"
#include "prclist.h"
#include "nsCRT.h"
#include "prlock.h"

class nsHttpHandler;
class nsHttpConnectionInfo;
class nsHttpTransaction;

//-----------------------------------------------------------------------------
// nsHttpConnection - represents a connection to a HTTP server (or proxy)
//-----------------------------------------------------------------------------

class nsHttpConnection : public nsIStreamListener
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
    nsresult SetTransaction(nsHttpTransaction *);

    // called by the transaction to inform the connection that all of the
    // headers are available.
    nsresult OnHeadersAvailable(nsHttpTransaction *, PRBool *reset);

    // called by the transaction to inform the connection that it is done.
    nsresult OnTransactionComplete(nsHttpTransaction *, nsresult status);

    // called by the transaction to suspend/resume a read-in-progress
    nsresult Suspend();
    nsresult Resume();

    // called to cause the underlying socket to start speaking SSL
    nsresult ProxyStepUp();

    PRBool   IsKeepAlive() { return mKeepAliveMask && mKeepAlive; }
    PRBool   CanReuse();   // can this connection be reused?
    void     DontReuse()   { mKeepAliveMask = PR_FALSE;
                             mKeepAlive = PR_FALSE;
                             mIdleTimeout = 0; }

    void DropTransaction();
    void ReportProgress(PRUint32 progress, PRInt32 progressMax);

    nsHttpTransaction    *Transaction()    { return mTransaction; }
    nsHttpConnectionInfo *ConnectionInfo() { return mConnectionInfo; }
    nsIEventQueue        *ConsumerEventQ() { return mConsumerEventQ; }

private:
    nsresult ActivateConnection();
    nsresult CreateTransport();

    // proxy the release of the transaction
    nsresult ProxyReleaseTransaction(nsHttpTransaction *);

    nsresult SetupSSLProxyConnect();

    PRBool   IsAlive();

private:
    nsCOMPtr<nsISocketTransport>    mSocketTransport;
    nsCOMPtr<nsIRequest>            mWriteRequest;
    nsCOMPtr<nsIRequest>            mReadRequest;

    nsCOMPtr<nsIProgressEventSink>  mProgressSink;
    nsCOMPtr<nsIEventQueue>         mConsumerEventQ;

    nsCOMPtr<nsIInputStream>        mSSLProxyConnectStream;

    nsHttpTransaction              *mTransaction;    // hard ref
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

//-----------------------------------------------------------------------------
// nsHttpConnectionInfo - holds the properties of a connection
//-----------------------------------------------------------------------------

class nsHttpConnectionInfo : public nsISupports
{
public:
    NS_DECL_ISUPPORTS

    nsHttpConnectionInfo(const char *host, PRInt32 port=-1,
                         const char *proxyHost=0, PRInt32 proxyPort=-1,
                         const char *proxyType=0, PRBool usingSSL=0)
        : mProxyPort(proxyPort)
        , mUsingSSL(usingSSL)
    {
        LOG(("Creating nsHttpConnectionInfo @%x\n", this));

        NS_INIT_ISUPPORTS();

        if (proxyHost)
          mProxyHost.Adopt(nsCRT::strdup(proxyHost));
        if (proxyType)
          mProxyType.Adopt(nsCRT::strdup(proxyType));

        SetOriginServer(host, port);
    }

    nsresult SetOriginServer(const char* host, PRInt32 port)
    {
        if (host)
            mHost.Adopt(nsCRT::strdup(host));
        mPort = port == -1 ? DefaultPort(): port;
        
        return NS_OK;
    }
    
    virtual ~nsHttpConnectionInfo()
    {
        LOG(("Destroying nsHttpConnectionInfo @%x\n", this));
    }

    // Compare this connection info to another...
    // Two connections are 'equal' if they end up talking the same
    // protocol to the same server. This is needed to properly manage
    // persistent connections to proxies
    PRBool Equals(const nsHttpConnectionInfo *info)
    {
        // if its a host independent proxy, then compare the proxy
        // servers.
        if ((info->mProxyHost.get() || mProxyHost.get()) &&
            !mUsingSSL &&
            PL_strcasecmp(mProxyType, "socks") != 0 &&
            PL_strcasecmp(mProxyType, "socks4") != 0) {
            return (!PL_strcasecmp(info->mProxyHost, mProxyHost) &&
                    !PL_strcasecmp(info->mProxyType, mProxyType) &&
                    info->mProxyPort == mProxyPort &&
                    info->mUsingSSL == mUsingSSL);
        }

        // otherwise, just check the hosts
        return (!PL_strcasecmp(info->mHost, mHost) &&
                info->mPort == mPort &&
                info->mUsingSSL == mUsingSSL);

    }

    const char *Host()      { return mHost; }
    PRInt32     Port()      { return mPort; }
    const char *ProxyHost() { return mProxyHost; }
    PRInt32     ProxyPort() { return mProxyPort; }
    const char *ProxyType() { return mProxyType; }
    PRBool      UsingSSL()  { return mUsingSSL; }

    PRInt32     DefaultPort() { return mUsingSSL ? 443 : 80; }
            
private:
    nsXPIDLCString     mHost;
    PRInt32            mPort;
    nsXPIDLCString     mProxyHost;
    PRInt32            mProxyPort;
    nsXPIDLCString     mProxyType;
    PRPackedBool       mUsingSSL;
};

#endif // nsHttpConnection_h__
