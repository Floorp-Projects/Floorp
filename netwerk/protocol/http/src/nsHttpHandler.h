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
#include "nsHttpPipeline.h"
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
#include "nsIIDNService.h"
#include "nsITimer.h"

class nsHttpConnection;
class nsHttpConnectionInfo;
class nsHttpHeaderArray;
class nsHttpTransaction;
class nsHttpAuthCache;
class nsAHttpTransaction;
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

    // Returns a pointer to the one and only HTTP handler
    static nsHttpHandler *get() { return mGlobalInstance; }

    nsresult Init();
    nsresult AddStandardRequestHeaders(nsHttpHeaderArray *,
                                       PRUint8 capabilities,
                                       PRBool useProxy);
    PRBool   IsAcceptableEncoding(const char *encoding);

    const nsAFlatCString &UserAgent();

    nsHttpVersion  HttpVersion()             { return mHttpVersion; }
    nsHttpVersion  ProxyHttpVersion()        { return mProxyHttpVersion; }
    PRUint8        ReferrerLevel()           { return mReferrerLevel; }
    PRBool         SendSecureXSiteReferrer() { return mSendSecureXSiteReferrer; }
    PRUint8        RedirectionLimit()        { return mRedirectionLimit; }
    PRUint16       IdleTimeout()             { return mIdleTimeout; }
    PRUint16       MaxRequestAttempts()      { return mMaxRequestAttempts; }
    const char    *DefaultSocketType()       { return mDefaultSocketType.get(); /* ok to return null */ }
    nsIIDNService *IDNConverter()            { return mIDNConverter; }

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

    nsresult PurgeDeadConnections();

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

public: /* internal */
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
    // Data structure used to hold information used during the construction of
    // a transaction pipeline.
    //
    class nsPipelineEnqueueState
    {
    public:
        nsPipelineEnqueueState() : mPipeline(nsnull) {}
       ~nsPipelineEnqueueState() { Cleanup(); }

        nsresult Init(nsHttpTransaction *);
        nsresult AppendTransaction(nsPendingTransaction *);
        void     Cleanup();
        
        nsAHttpTransaction   *Transaction() { return mPipeline; }
        PRUint8               TransactionCaps() { return NS_HTTP_ALLOW_KEEPALIVE | NS_HTTP_ALLOW_PIPELINING; }
        PRBool                HasPipeline() { return (mPipeline != nsnull); }
        nsPendingTransaction *GetAppendedTrans(PRInt32 i) { return (nsPendingTransaction *) mAppendedTrans[i]; }
        PRInt32               NumAppendedTrans() { return mAppendedTrans.Count(); }
        void                  DropAppendedTrans() { mAppendedTrans.Clear(); }

    private:
        nsHttpPipeline *mPipeline;
        nsAutoVoidArray mAppendedTrans;
    };

private:

    //
    // Connection management methods
    //
    nsresult GetConnection_Locked(nsHttpConnectionInfo *, PRUint8 caps, nsHttpConnection **);
    nsresult EnqueueTransaction_Locked(nsHttpTransaction *, nsHttpConnectionInfo *);
    nsresult DispatchTransaction_Locked(nsAHttpTransaction *, PRUint8 caps, nsHttpConnection *); // unlocked on return
    void     ProcessTransactionQ_Locked(); // unlocked on return
    PRBool   AtActiveConnectionLimit_Locked(nsHttpConnectionInfo *, PRUint8 caps);
    nsresult RemovePendingTransaction_Locked(nsHttpTransaction *);

    void     DropConnections(nsVoidArray &);

    //
    // Pipelining methods - these may fail silently, and that's ok!
    //
    PRBool   BuildPipeline_Locked(nsPipelineEnqueueState &,
                                  nsHttpTransaction *,
                                  nsHttpConnectionInfo *);
    void     PipelineFailed_Locked(nsPipelineEnqueueState &);

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

    // timer callback for cleansing the idle connection list
    static void DeadConnectionCleanupCB(nsITimer *, void *);

private:
    static nsHttpHandler *mGlobalInstance;

    // cached services
    nsCOMPtr<nsIIOService>              mIOService;
    nsCOMPtr<nsIProxyObjectManager>     mProxyMgr;
    nsCOMPtr<nsIEventQueueService>      mEventQueueService;
    nsCOMPtr<nsINetModuleMgr>           mNetModuleMgr;
    nsCOMPtr<nsIStreamConverterService> mStreamConvSvc;
    nsCOMPtr<nsIMIMEService>            mMimeService;
    nsCOMPtr<nsIIDNService>             mIDNConverter;
    nsCOMPtr<nsITimer>                  mTimer;

    // the authentication credentials cache
    nsHttpAuthCache *mAuthCache;

    //
    // prefs
    //

    PRUint8  mHttpVersion;
    PRUint8  mProxyHttpVersion;
    PRUint8  mCapabilities;
    PRUint8  mProxyCapabilities;
    PRUint8  mReferrerLevel;

    PRUint16 mIdleTimeout;
    PRUint16 mMaxRequestAttempts;
    PRUint16 mMaxRequestDelay;

    PRUint16 mMaxConnections;
    PRUint8  mMaxConnectionsPerServer;
    PRUint8  mMaxPersistentConnectionsPerServer;
    PRUint8  mMaxPersistentConnectionsPerProxy;
    PRUint8  mMaxPipelinedRequests;

    PRUint8  mRedirectionLimit;

    nsCString mAccept;
    nsCString mAcceptLanguages;
    nsCString mAcceptEncodings;
    nsCString mAcceptCharsets;

    nsXPIDLCString mDefaultSocketType;

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
    // mSendSecureXSiteReferrer: default is false, 
    // if true allow referrer headers between secure non-matching hosts
    PRPackedBool   mSendSecureXSiteReferrer;
};

//-----------------------------------------------------------------------------
// nsHttpsHandler - thin wrapper to distinguish the HTTP handler from the
//                  HTTPS handler (even though they share the same impl).
//-----------------------------------------------------------------------------

class nsHttpsHandler : public nsIHttpProtocolHandler
                     , public nsSupportsWeakReference
{
public:
    // we basically just want to override GetScheme and GetDefaultPort...
    // all other methods should be forwarded to the nsHttpHandler instance.
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
    NS_FORWARD_NSIPROXIEDPROTOCOLHANDLER (nsHttpHandler::get()->)
    NS_FORWARD_NSIHTTPPROTOCOLHANDLER    (nsHttpHandler::get()->)

    nsHttpsHandler() { }
    virtual ~nsHttpsHandler() { }

    nsresult Init();
};

#endif // nsHttpHandler_h__
