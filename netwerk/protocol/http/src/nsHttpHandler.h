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

#ifndef nsHttpHandler_h__
#define nsHttpHandler_h__

#include "nsHttp.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIProtocolProxyService.h"
#include "nsIIOService.h"
#include "nsIObserver.h"
#include "nsIProxyObjectManager.h"
#include "nsINetModuleMgr.h"
#include "nsIProxy.h"
#include "nsIStreamConverterService.h"
#include "nsICacheSession.h"
#include "nsIEventQueueService.h"
#include "nsIMIMEService.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsVoidArray.h"

class nsHttpConnection;
class nsHttpConnectionInfo;
class nsHttpHeaderArray;
class nsHttpTransaction;
class nsHttpAuthCache;
class nsIHttpChannel;
class nsIPrefBranch;

//-----------------------------------------------------------------------------
// nsHttpHandler - protocol handler for HTTP and HTTPS
//-----------------------------------------------------------------------------

class nsHttpHandler : public nsIHttpProtocolHandler
                    , public nsIObserver
                    , public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
    NS_DECL_NSIPROXIEDPROTOCOLHANDLER
    NS_DECL_NSIHTTPPROTOCOLHANDLER
    NS_DECL_NSIOBSERVER

    nsHttpHandler();
    virtual ~nsHttpHandler();

    // Implement our own create function so we can register ourselves as both
    // the HTTP and HTTPS handler.
    static NS_METHOD Create(nsISupports *outer, REFNSIID iid, void **result);

    // Returns a pointer to the one and only HTTP handler
    static nsHttpHandler *get() { return mGlobalInstance; }

    nsresult Init();
    nsresult AddStandardRequestHeaders(nsHttpHeaderArray *,
                                       PRUint8 capabilities,
                                       PRBool useProxy);
    PRBool   IsAcceptableEncoding(const char *encoding);

    const char   *UserAgent();
    nsHttpVersion DefaultVersion()     { return mHttpVersion; }
    PRUint8       ReferrerLevel()      { return mReferrerLevel; }
    PRUint16      IdleTimeout()        { return mIdleTimeout; }
    PRUint16      MaxRequestAttempts() { return mMaxRequestAttempts; }

    nsHttpAuthCache *AuthCache() { return mAuthCache; }

    // cache support
    nsresult GetCacheSession(nsCacheStoragePolicy, nsICacheSession **);
    PRUint32 GenerateUniqueID() { return ++mLastUniqueID; }
    PRUint32 SessionStartTime() { return mSessionStartTime; }

    //
    // Connection management methods:
    //
    // - the handler only owns idle connections; it does not own active
    //   connections.
    //
    // - the handler keeps a count of active connections to enforce the
    //   steady-state max-connections pref.
    // 

    // Called to kick-off a new transaction, by default the transaction
    // will be put on the pending transaction queue if it cannot be 
    // initiated at this time.  Callable from any thread.
    nsresult InitiateTransaction(nsHttpTransaction *, nsHttpConnectionInfo *);

    // Called to cancel a transaction, which may or may not be assigned to
    // a connection.  Callable from any thread.
    nsresult CancelTransaction(nsHttpTransaction *, nsresult status);

    // Called when a connection is done processing a transaction.  Callable
    // from any thread.
    nsresult ReclaimConnection(nsHttpConnection *);

    // Called when a connection has been busy with a single transaction for
    // longer than mMaxRequestDelay.
    nsresult ProcessTransactionQ();

    //
    // The HTTP handler caches pointers to specific XPCOM services, and
    // provides the following helper routines for accessing those services:
    //
    nsresult GetProxyObjectManager(nsIProxyObjectManager **);
    nsresult GetEventQueueService(nsIEventQueueService **);
    nsresult GetStreamConverterService(nsIStreamConverterService **);
    nsresult GetMimeService(nsIMIMEService **);
    nsresult GetIOService(nsIIOService** service);


    // Called by the channel before writing a request
    nsresult OnModifyRequest(nsIHttpChannel *);

    // Called by the channel once headers are available
    nsresult OnExamineResponse(nsIHttpChannel *);

private:
    //
    // Transactions that have not yet been assigned to a connection are kept
    // in a queue of nsPendingTransaction objects.  nsPendingTransaction 
    // implements nsAHttpTransactionSink to handle transaction Cancellation.
    //
    class nsPendingTransaction
    {
    public:
        nsPendingTransaction(nsHttpTransaction *, nsHttpConnectionInfo *);
       ~nsPendingTransaction();
        
        nsHttpTransaction    *Transaction()    { return mTransaction; }
        nsHttpConnectionInfo *ConnectionInfo() { return mConnectionInfo; }

        PRBool IsBusy()              { return mBusy; }
        void   SetBusy(PRBool value) { mBusy = value; }

    private:
        nsHttpTransaction    *mTransaction;
        nsHttpConnectionInfo *mConnectionInfo;
        PRPackedBool          mBusy;
    };

    //
    // Transaction queue helper methods
    //
    void     ProcessTransactionQ_Locked();
    nsresult EnqueueTransaction(nsHttpTransaction *, nsHttpConnectionInfo *);

    // Called with mConnectionLock held
    nsresult InitiateTransaction_Locked(nsHttpTransaction *,
                                        nsHttpConnectionInfo *,
                                        PRBool failIfBusy = PR_FALSE);

    nsresult RemovePendingTransaction(nsHttpTransaction *);
    PRBool   AtActiveConnectionLimit(nsHttpConnectionInfo *, PRUint8 caps);
    void     DropConnections(nsVoidArray &);

    //
    // Useragent/prefs helper methods
    //
    void     BuildUserAgent();
    void     InitUserAgentComponents();
    void     PrefsChanged(nsIPrefBranch *prefs, const char *pref = nsnull);
    void     GetPrefBranch(nsIPrefBranch **);

    nsresult SetAccept(const char *);
    nsresult SetAcceptLanguages(const char *);
    nsresult SetAcceptEncodings(const char *);
    nsresult SetAcceptCharsets(const char *);

    static PRInt32 PR_CALLBACK PrefsCallback(const char *, void *);

private:
    static nsHttpHandler *mGlobalInstance;

    // cached services
    nsCOMPtr<nsIIOService>              mIOService;
    nsCOMPtr<nsIProxyObjectManager>     mProxyMgr;
    nsCOMPtr<nsIEventQueueService>      mEventQueueService;
    nsCOMPtr<nsINetModuleMgr>           mNetModuleMgr;
    nsCOMPtr<nsIStreamConverterService> mStreamConvSvc;
    nsCOMPtr<nsIMIMEService>            mMimeService;

    // the authentication credentials cache
    nsHttpAuthCache *mAuthCache;

    //
    // prefs
    //

    PRUint8  mHttpVersion;
    PRUint8  mReferrerLevel;
    PRUint8  mCapabilities;
    PRUint8  mProxyCapabilities;

    PRUint16 mIdleTimeout;
    PRUint16 mMaxRequestAttempts;
    PRUint16 mMaxRequestDelay;

    PRUint16 mMaxConnections;
    PRUint8  mMaxConnectionsPerServer;
    PRUint8  mMaxPersistentConnectionsPerServer;

    nsCString mAccept;
    nsCString mAcceptLanguages;
    nsCString mAcceptEncodings;
    nsCString mAcceptCharsets;

    // cache support
    nsCOMPtr<nsICacheSession> mCacheSession_ANY;
    nsCOMPtr<nsICacheSession> mCacheSession_MEM;
    PRUint32                  mLastUniqueID;
    PRUint32                  mSessionStartTime;

    // connection management
    nsVoidArray mActiveConnections; // list of nsHttpConnection objects
    nsVoidArray mIdleConnections;   // list of nsHttpConnection objects
    nsVoidArray mTransactionQ;      // list of nsPendingTransaction objects
    PRLock     *mConnectionLock;    // protect connection lists

    // useragent components
    nsXPIDLCString mAppName;
    nsXPIDLCString mAppVersion;
    nsXPIDLCString mPlatform;
    nsXPIDLCString mOscpu;
    nsXPIDLCString mSecurity;
    nsXPIDLCString mLanguage;
    nsXPIDLCString mMisc;
    nsXPIDLCString mVendor;
    nsXPIDLCString mVendorSub;
    nsXPIDLCString mVendorComment;
    nsXPIDLCString mProduct;
    nsXPIDLCString mProductSub;
    nsXPIDLCString mProductComment;

    nsCString      mUserAgent;
    nsXPIDLCString mUserAgentOverride;
    PRPackedBool   mUserAgentIsDirty; // true if mUserAgent should be rebuilt

    PRPackedBool   mUseCache;
};

#endif // nsHttpHandler_h__
