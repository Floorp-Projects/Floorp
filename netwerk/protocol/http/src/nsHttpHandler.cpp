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
 *   Gagan Saksena <gagan@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Adrian Havill <havill@redhat.com>
 *   Gervase Markham <gerv@gerv.net>
 *   Bradley Baetz <bbaetz@netscape.com>
 */

#include "nsHttp.h"
#include "nsHttpHandler.h"
#include "nsHttpChannel.h"
#include "nsHttpConnection.h"
#include "nsHttpResponseHead.h"
#include "nsHttpTransaction.h"
#include "nsHttpAuthCache.h"
#include "nsIHttpChannel.h"
#include "nsIHttpNotify.h"
#include "nsIURL.h"
#include "nsICacheService.h"
#include "nsICategoryManager.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "nsINetModRegEntry.h"
#include "nsICacheService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranchInternal.h"
#include "nsIPrefLocalizedString.h"
#include "nsPrintfCString.h"
#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "nsAutoLock.h"
#include "prprf.h"
#include "nsReadableUtils.h"

#if defined(XP_UNIX) || defined(XP_BEOS)
#include <sys/utsname.h>
#endif

#if defined(XP_PC) && !defined(XP_OS2)
#include <windows.h>
#endif

#if defined(XP_MAC)
#include <Gestalt.h>
#endif

#ifdef DEBUG
// defined by the socket transport service while active
extern PRThread *NS_SOCKET_THREAD;
#endif

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kCategoryManagerCID, NS_CATEGORYMANAGER_CID);
static NS_DEFINE_CID(kNetModuleMgrCID, NS_NETMODULEMGR_CID);
static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_CID(kCacheServiceCID, NS_CACHESERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kMimeServiceCID, NS_MIMESERVICE_CID);

#define UA_PREF_PREFIX          "general.useragent."
#define UA_APPNAME              "Mozilla"
#define UA_APPVERSION           "5.0"
#define UA_APPSECURITY_FALLBACK "N"

#define HTTP_PREF_PREFIX        "network.http."
#define INTL_ACCEPT_LANGUAGES   "intl.accept_languages"
#define INTL_ACCEPT_CHARSET     "intl.charset.default"

#define UA_PREF(_pref) UA_PREF_PREFIX _pref
#define HTTP_PREF(_pref) HTTP_PREF_PREFIX _pref

//-----------------------------------------------------------------------------
// nsHttpHandler <public>
//-----------------------------------------------------------------------------

nsHttpHandler *nsHttpHandler::mGlobalInstance = 0;

nsHttpHandler::nsHttpHandler()
    : mAuthCache(nsnull)
    , mHttpVersion(NS_HTTP_VERSION_1_1)
    , mReferrerLevel(0xff) // by default we always send a referrer
    , mCapabilities(NS_HTTP_ALLOW_KEEPALIVE)
    , mProxyCapabilities(NS_HTTP_ALLOW_KEEPALIVE)
    , mIdleTimeout(10)
    , mMaxRequestAttempts(10)
    , mMaxRequestDelay(10)
    , mMaxConnections(24)
    , mMaxConnectionsPerServer(8)
    , mMaxPersistentConnectionsPerServer(2)
    , mActiveConnections(0)
    , mIdleConnections(0)
    , mTransactionQ(0)
    , mUserAgentIsDirty(PR_TRUE)
    , mUseCache(PR_TRUE)
{
    NS_INIT_ISUPPORTS();

#if defined(PR_LOGGING)
    gHttpLog = PR_NewLogModule("nsHttp");
#endif

    LOG(("Creating nsHttpHandler [this=%x].\n", this));

    NS_ASSERTION(!mGlobalInstance, "HTTP handler already created!");
    mGlobalInstance = this;
}

nsHttpHandler::~nsHttpHandler()
{
    LOG(("Deleting nsHttpHandler [this=%x]\n", this));

    nsHttp::DestroyAtomTable();

    nsCOMPtr<nsIPrefBranch> prefBranch;
    GetPrefBranch(getter_AddRefs(prefBranch));
    if (prefBranch) {
        nsCOMPtr<nsIPrefBranchInternal> pbi = do_QueryInterface(prefBranch);
        if (pbi) {
            pbi->RemoveObserver(HTTP_PREF_PREFIX, this);
            pbi->RemoveObserver(UA_PREF_PREFIX, this);
            pbi->RemoveObserver(INTL_ACCEPT_LANGUAGES, this); 
            pbi->RemoveObserver(INTL_ACCEPT_CHARSET, this);
        }
    }

    LOG(("dropping active connections...\n"));
    DropConnections(mActiveConnections);

    LOG(("dropping idle connections...\n"));
    DropConnections(mIdleConnections);

    if (mAuthCache) {
        delete mAuthCache;
        mAuthCache = nsnull;
    }

    if (mConnectionLock) {
        PR_DestroyLock(mConnectionLock);
        mConnectionLock = nsnull;
    }

    mGlobalInstance = nsnull;
}

NS_METHOD
nsHttpHandler::Create(nsISupports *outer, REFNSIID iid, void **result)
{
    nsresult rv;
    if (outer)
        return NS_ERROR_NO_AGGREGATION;
    nsHttpHandler *handler = get();
    if (!handler) {
        // create the one any only instance of nsHttpHandler
        NS_NEWXPCOM(handler, nsHttpHandler);
        if (!handler)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(handler);
        rv = handler->Init();
        if (NS_FAILED(rv)) {
            LOG(("nsHttpHandler::Init failed [rv=%x]\n", rv));
            NS_RELEASE(handler);
            return rv;
        }
    }
    else
        NS_ADDREF(handler);
    rv = handler->QueryInterface(iid, result);
    NS_RELEASE(handler);
    return rv;
}

nsresult
nsHttpHandler::Init()
{
    nsresult rv = NS_OK;

    LOG(("nsHttpHandler::Init\n"));

    mIOService = do_GetService(kIOServiceCID, &rv);
    if (NS_FAILED(rv)) {
        NS_WARNING("unable to continue without io service");
        return rv;
    }

    mConnectionLock = PR_NewLock();
    if (!mConnectionLock)
        return NS_ERROR_OUT_OF_MEMORY;

    InitUserAgentComponents();

    // monitor some preference changes
    nsCOMPtr<nsIPrefBranch> prefBranch;
    GetPrefBranch(getter_AddRefs(prefBranch));
    if (prefBranch) {
        nsCOMPtr<nsIPrefBranchInternal> pbi = do_QueryInterface(prefBranch);
        if (pbi) {
            pbi->AddObserver(HTTP_PREF_PREFIX, this);
            pbi->AddObserver(UA_PREF_PREFIX, this);
            pbi->AddObserver(INTL_ACCEPT_LANGUAGES, this); 
            pbi->AddObserver(INTL_ACCEPT_CHARSET, this);
        }
        PrefsChanged(prefBranch);
    }

#if DEBUG
    // dump user agent prefs
    LOG(("> app-name = %s\n", mAppName.get()));
    LOG(("> app-version = %s\n", mAppVersion.get()));
    LOG(("> platform = %s\n", mPlatform.get()));
    LOG(("> oscpu = %s\n", mOscpu.get()));
    LOG(("> security = %s\n", mSecurity.get()));
    LOG(("> language = %s\n", mLanguage.get()));
    LOG(("> misc = %s\n", mMisc.get()));
    LOG(("> vendor = %s\n", mVendor.get()));
    LOG(("> vendor-sub = %s\n", mVendorSub.get()));
    LOG(("> vendor-comment = %s\n", mVendorComment.get()));
    LOG(("> product = %s\n", mProduct.get()));
    LOG(("> product-sub = %s\n", mProductSub.get()));
    LOG(("> product-comment = %s\n", mProductComment.get()));
    LOG(("> user-agent = %s\n", UserAgent()));
#endif

    mSessionStartTime = NowInSeconds();

    mAuthCache = new nsHttpAuthCache();
    if (!mAuthCache)
        return NS_ERROR_OUT_OF_MEMORY;
    rv = mAuthCache->Init();
    if (NS_FAILED(rv)) return rv;

    // Startup the http category
    // Bring alive the objects in the http-protocol-startup category
    NS_CreateServicesFromCategory(NS_HTTP_STARTUP_CATEGORY,
                                  NS_STATIC_CAST(nsISupports*,NS_STATIC_CAST(void*,this)),
                                  NS_HTTP_STARTUP_TOPIC);
    
    nsCOMPtr<nsIObserverService> observerSvc =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
    if (observerSvc) {
        observerSvc->AddObserver(this, NS_LITERAL_STRING("profile-before-change").get());
        observerSvc->AddObserver(this, NS_LITERAL_STRING("session-logout").get());
    }
    return NS_OK;
}

nsresult
nsHttpHandler::AddStandardRequestHeaders(nsHttpHeaderArray *request,
                                         PRUint8 caps,
                                         PRBool useProxy)
{
    nsresult rv;

    LOG(("nsHttpHandler::AddStandardRequestHeaders\n"));

    // Add the "User-Agent" header
    rv = request->SetHeader(nsHttp::User_Agent, UserAgent());
    if (NS_FAILED(rv)) return rv;

    // MIME based content negotiation lives!
    // Add the "Accept" header
    rv = request->SetHeader(nsHttp::Accept, mAccept.get());
    if (NS_FAILED(rv)) return rv;

    // Add the "Accept-Language" header
    if (!mAcceptLanguages.IsEmpty()) {
        // Add the "Accept-Language" header
        rv = request->SetHeader(nsHttp::Accept_Language, mAcceptLanguages.get());
        if (NS_FAILED(rv)) return rv;
    }

    // Add the "Accept-Encoding" header
    rv = request->SetHeader(nsHttp::Accept_Encoding, mAcceptEncodings.get());
    if (NS_FAILED(rv)) return rv;

    // Add the "Accept-Charset" header
    rv = request->SetHeader(nsHttp::Accept_Charset, mAcceptCharsets.get());
    if (NS_FAILED(rv)) return rv;

    // RFC2616 section 19.6.2 states that the "Connection: keep-alive"
    // and "Keep-alive" request headers should not be sent by HTTP/1.1
    // user-agents.  Otherwise, problems with proxy servers (especially
    // transparent proxies) can result.
    //
    // However, we need to send something so that we can use keepalive
    // with HTTP/1.0 servers/proxies. We use "Proxy-Connection:" when 
    // we're talking to an http proxy, and "Connection:" otherwise
    
    const char* connectionType = "close";
    if (caps & NS_HTTP_ALLOW_KEEPALIVE) {
        char buf[32];
        
        PR_snprintf(buf, sizeof(buf), "%u", (PRUintn) mIdleTimeout);
        
        rv = request->SetHeader(nsHttp::Keep_Alive, buf);
        if (NS_FAILED(rv)) return rv;
        
        connectionType = "keep-alive";
    } else if (useProxy) {
        // Bug 92006
        request->SetHeader(nsHttp::Connection, "close");
    }

    const nsHttpAtom &header =
        useProxy ? nsHttp::Proxy_Connection : nsHttp::Connection;
    return request->SetHeader(header, connectionType);
}

PRBool
nsHttpHandler::IsAcceptableEncoding(const char *enc)
{
    if (!enc)
        return PR_FALSE;

    // HTTP 1.1 allows servers to send x-gzip and x-compress instead
    // of gzip and compress, for example.  So, we'll always strip off
    // an "x-" prefix before matching the encoding to one we claim
    // to accept.
    if (!PL_strncasecmp(enc, "x-", 2))
        enc += 2;
    
    return PL_strcasestr(mAcceptEncodings, enc) != nsnull;
}

nsresult
nsHttpHandler::GetCacheSession(nsCacheStoragePolicy storagePolicy,
                               nsICacheSession **result)
{
    nsresult rv;

    // Skip cache if disabled in preferences
    if (!mUseCache)
        return NS_ERROR_NOT_AVAILABLE;

    if (!mCacheSession_ANY) {
        nsCOMPtr<nsICacheService> serv = do_GetService(kCacheServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = serv->CreateSession("HTTP",
                                 nsICache::STORE_ANYWHERE,
                                 nsICache::STREAM_BASED,
                                 getter_AddRefs(mCacheSession_ANY));
        if (NS_FAILED(rv)) return rv;

        rv = mCacheSession_ANY->SetDoomEntriesIfExpired(PR_FALSE);
        if (NS_FAILED(rv)) return rv;

        rv = serv->CreateSession("HTTP-memory-only",
                                 nsICache::STORE_IN_MEMORY,
                                 nsICache::STREAM_BASED,
                                 getter_AddRefs(mCacheSession_MEM));
        if (NS_FAILED(rv)) return rv;

        rv = mCacheSession_MEM->SetDoomEntriesIfExpired(PR_FALSE);
        if (NS_FAILED(rv)) return rv;
    }

    if (storagePolicy == nsICache::STORE_IN_MEMORY)
        NS_ADDREF(*result = mCacheSession_MEM);
    else
        NS_ADDREF(*result = mCacheSession_ANY);

    return NS_OK;
}

// may be called from any thread
nsresult
nsHttpHandler::InitiateTransaction(nsHttpTransaction *trans,
                                   nsHttpConnectionInfo *ci)
{
    LOG(("nsHttpHandler::InitiateTransaction\n"));

    NS_ENSURE_ARG_POINTER(trans);
    NS_ENSURE_ARG_POINTER(ci);

    nsAutoLock lock(mConnectionLock);

    return InitiateTransaction_Locked(trans, ci);
}

// called from the socket thread
nsresult
nsHttpHandler::ReclaimConnection(nsHttpConnection *conn)
{
    NS_ENSURE_ARG_POINTER(conn);
#ifdef DEBUG
    NS_PRECONDITION(PR_GetCurrentThread() == NS_SOCKET_THREAD, "wrong thread");
#endif

    PRBool reusable = conn->CanReuse();

    LOG(("nsHttpHandler::ReclaimConnection [conn=%x(%s:%d) keep-alive=%d]\n",
        conn, conn->ConnectionInfo()->Host(), conn->ConnectionInfo()->Port(), reusable));

    nsAutoLock lock(mConnectionLock);

    // remove connection from the active connection list
    mActiveConnections.RemoveElement(conn);

    if (reusable) {
        LOG(("adding connection to idle list [conn=%x]\n", conn));
        // hold onto this connection in the idle list.  we push it
        // to the end of the list so as to ensure that we'll visit
        // older connections first before getting to this one.
        mIdleConnections.AppendElement(conn);
    }
    else {
        LOG(("closing connection: connection can't be reused\n"));
        NS_RELEASE(conn);
    }

    LOG(("active connection count is now %u\n", mActiveConnections.Count()));

    // process the pending transaction queue...
    if (mTransactionQ.Count() > 0)
        ProcessTransactionQ_Locked();

    return NS_OK;
}

// called from the socket thread (see nsHttpConnection::OnDataAvailable)
nsresult
nsHttpHandler::ProcessTransactionQ()
{
    LOG(("nsHttpHandler::ProcessTransactionQ\n"));
#ifdef DEBUG
    NS_PRECONDITION(PR_GetCurrentThread() == NS_SOCKET_THREAD, "wrong thread");
#endif

    nsAutoLock lock(mConnectionLock);

    // conn is no longer keep-alive, so we may be able to initiate
    // a pending transaction to the same host.
    if (mTransactionQ.Count() > 0)
        ProcessTransactionQ_Locked();

    return NS_OK;
}

// called from any thread, by the implementation of nsHttpTransaction::Cancel
nsresult
nsHttpHandler::CancelTransaction(nsHttpTransaction *trans, nsresult status)
{
    nsAHttpConnection *conn;

    LOG(("nsHttpHandler::CancelTransaction [trans=%x status=%x]\n",
        trans, status));

    NS_ENSURE_ARG_POINTER(trans);

    // we need to be inside the connection lock in order to know whether
    // or not this transaction has an associated connection.  otherwise,
    // we'd have a race condition (see bug 85822).
    {
        nsAutoLock lock(mConnectionLock);

        conn = trans->Connection();
        if (conn)
            NS_ADDREF(conn); // make sure the connection stays around.
        else
            RemovePendingTransaction(trans);
    }

    if (conn) {
        conn->OnTransactionComplete(trans, status);
        NS_RELEASE(conn);
    }
    else
        trans->OnStopTransaction(status);

    return NS_OK;
}

nsresult
nsHttpHandler::GetProxyObjectManager(nsIProxyObjectManager **result)
{
    if (!mProxyMgr) {
        nsresult rv;
        mProxyMgr = do_GetService(NS_XPCOMPROXY_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;
    }
    *result = mProxyMgr;
    NS_ADDREF(*result);
    return NS_OK;
}

nsresult
nsHttpHandler::GetEventQueueService(nsIEventQueueService **result)
{
    if (!mEventQueueService) {
        nsresult rv;
        mEventQueueService = do_GetService(kEventQueueServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;
    }
    *result = mEventQueueService;
    NS_ADDREF(*result);
    return NS_OK;
}

nsresult
nsHttpHandler::GetStreamConverterService(nsIStreamConverterService **result)
{
    if (!mStreamConvSvc) {
        nsresult rv;
        mStreamConvSvc = do_GetService(kStreamConverterServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;
    }
    *result = mStreamConvSvc;
    NS_ADDREF(*result);
    return NS_OK;
}

nsresult
nsHttpHandler::GetMimeService(nsIMIMEService **result)
{
    if (!mMimeService) {
        nsresult rv;
        mMimeService = do_GetService(kMimeServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;
    }
    *result = mMimeService;
    NS_ADDREF(*result);
    return NS_OK;
}

nsresult 
nsHttpHandler::GetIOService(nsIIOService** result)
{
    NS_ADDREF(*result = mIOService);
    return NS_OK;
}


nsresult
nsHttpHandler::OnModifyRequest(nsIHttpChannel *chan)
{
    nsresult rv;

    LOG(("nsHttpHandler::OnModifyRequest [chan=%x]\n", chan));

    if (!mNetModuleMgr) {
        mNetModuleMgr = do_GetService(kNetModuleMgrCID, &rv);
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsISimpleEnumerator> modules;
    rv = mNetModuleMgr->EnumerateModules(
            NS_NETWORK_MODULE_MANAGER_HTTP_REQUEST_CONTRACTID,
            getter_AddRefs(modules));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISupports> sup;

    // notify each module...
    while (NS_SUCCEEDED(modules->GetNext(getter_AddRefs(sup)))) {
        nsCOMPtr<nsINetModRegEntry> entry = do_QueryInterface(sup, &rv);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsINetNotify> netNotify;
        rv = entry->GetSyncProxy(getter_AddRefs(netNotify));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIHttpNotify> httpNotify = do_QueryInterface(netNotify, &rv);
        if (NS_FAILED(rv)) return rv;

        // fire off the notification, ignore the return code.
        httpNotify->OnModifyRequest(chan);
    }
    
    return NS_OK;
}

nsresult
nsHttpHandler::OnExamineResponse(nsIHttpChannel *chan)
{
    nsresult rv;

    LOG(("nsHttpHandler::OnExamineResponse [chan=%x]\n", chan));

    if (!mNetModuleMgr) {
        mNetModuleMgr = do_GetService(kNetModuleMgrCID, &rv);
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsISimpleEnumerator> modules;
    rv = mNetModuleMgr->EnumerateModules(
            NS_NETWORK_MODULE_MANAGER_HTTP_RESPONSE_CONTRACTID,
            getter_AddRefs(modules));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISupports> sup;
    nsCOMPtr<nsINetModRegEntry> entry;
    nsCOMPtr<nsINetNotify> netNotify;
    nsCOMPtr<nsIHttpNotify> httpNotify;

    // notify each module...
    while (NS_SUCCEEDED(modules->GetNext(getter_AddRefs(sup)))) {
        entry = do_QueryInterface(sup, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = entry->GetSyncProxy(getter_AddRefs(netNotify));
        if (NS_FAILED(rv)) return rv;

        httpNotify = do_QueryInterface(netNotify, &rv);
        if (NS_FAILED(rv)) return rv;

        // fire off the notification, ignore the return code.
        httpNotify->OnExamineResponse(chan);
    }
    
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler <private>
//-----------------------------------------------------------------------------

const char *
nsHttpHandler::UserAgent()
{
    if (mUserAgentOverride) {
        LOG(("using general.useragent.override : %s\n", mUserAgentOverride.get()));
        return mUserAgentOverride.get();
    }

    if (mUserAgentIsDirty) {
        BuildUserAgent();
        mUserAgentIsDirty = PR_FALSE;
    }

    return mUserAgent.get();
}

// called with the connection lock held
void
nsHttpHandler::ProcessTransactionQ_Locked()
{
    LOG(("nsHttpHandler::ProcessTransactionQ_Locked\n"));

    nsPendingTransaction *pt = nsnull;

    PRInt32 i;
    for (i=0; (i < mTransactionQ.Count()) &&
              (mActiveConnections.Count() < PRInt32(mMaxConnections)); ++i) {

        pt = (nsPendingTransaction *) mTransactionQ[i];

        // skip over a busy pending transaction
        if (pt->IsBusy())
            continue;

        // the connection lock is released during InitiateTransaction_Locked, so
        // the transaction queue could get modified.  mark this pending transaction
        // as busy to cause it to be skipped if this function happens to recurse.
        pt->SetBusy(PR_TRUE);

        // try to initiate this transaction... if it fails then we'll just skip
        // over this pending transaction and try the next.
        nsresult rv = InitiateTransaction_Locked(pt->Transaction(),
                                                 pt->ConnectionInfo(),
                                                 PR_TRUE);

        if (NS_SUCCEEDED(rv)) {
            mTransactionQ.RemoveElementAt(i);
            delete pt;
            i--;
        }
        else {
            LOG(("InitiateTransaction_Locked failed [rv=%x]\n", rv));
            pt->SetBusy(PR_FALSE);
        }
    }
}

// called with the connection lock held
nsresult
nsHttpHandler::EnqueueTransaction(nsHttpTransaction *trans,
                                  nsHttpConnectionInfo *ci)
{
    LOG(("nsHttpHandler::EnqueueTransaction [trans=%x]\n", trans));

    nsPendingTransaction *pt = new nsPendingTransaction(trans, ci);
    if (!pt)
        return NS_ERROR_OUT_OF_MEMORY;

    mTransactionQ.AppendElement(pt);

    LOG((">> transaction queue contains %u elements\n", mTransactionQ.Count()));
    return NS_OK;
}

// called with the connection lock held
nsresult
nsHttpHandler::InitiateTransaction_Locked(nsHttpTransaction *trans,
                                          nsHttpConnectionInfo *ci,
                                          PRBool failIfBusy)
{
    nsresult rv;
    PRUint8 caps = trans->Capabilities();

    LOG(("nsHttpHandler::InitiateTransaction_Locked [failIfBusy=%d]\n", failIfBusy));

    if (AtActiveConnectionLimit(ci, caps)) {
        LOG((">> unable to perform the transaction at this time [trans=%x]\n", trans));
        return failIfBusy ? NS_ERROR_FAILURE : EnqueueTransaction(trans, ci);
    }

    nsHttpConnection *conn = nsnull;

    if (caps & NS_HTTP_ALLOW_KEEPALIVE) {
        // search the idle connection list
        PRInt32 i;
        for (i=0; i<mIdleConnections.Count(); ++i) {
            conn = (nsHttpConnection *) mIdleConnections[i];

            LOG((">> comparing against idle connection [conn=%x host=%s:%d]\n",
                conn, conn->ConnectionInfo()->Host(), conn->ConnectionInfo()->Port()));

            // we check if the connection can be reused before even checking if it
            // is a "matching" connection.  this is how we keep the idle connection
            // list fresh.  we could alternatively use some sort of timer for this.
            if (!conn->CanReuse()) {
                LOG(("   dropping stale connection: [conn=%x]\n", conn));
                mIdleConnections.RemoveElementAt(i);
                i--;
                NS_RELEASE(conn);
            }
            else if (conn->ConnectionInfo()->Equals(ci)) {
                LOG(("   reusing connection [conn=%x]\n", conn));
                mIdleConnections.RemoveElementAt(i);
                i--;
                break;
            }
            conn = nsnull;
        }
    }

    if (!conn) {
        LOG((">> creating new connection...\n"));
        NS_NEWXPCOM(conn, nsHttpConnection);
        if (!conn)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(conn);

        rv = conn->Init(ci, mMaxRequestDelay);
        if (NS_FAILED(rv)) {
            NS_RELEASE(conn);
            return rv;
        }
    }
    else {
        // Update the connectionInfo (bug 94038)
        conn->ConnectionInfo()->SetOriginServer(ci->Host(), ci->Port());
    }

    // assign the connection to the transaction.
    trans->SetConnection(conn);

    // consider this connection active, even though it may fail.
    mActiveConnections.AppendElement(conn);
    
    // we must not hold the connection lock while making this call
    // as it could lead to deadlocks.
    PR_Unlock(mConnectionLock);
    rv = conn->SetTransaction(trans, trans->Capabilities());
    PR_Lock(mConnectionLock);

    if (NS_FAILED(rv)) {
        LOG(("nsHttpConnection::SetTransaction failed [rv=%x]\n", rv));
        // the connection may already have been removed from the 
        // active connection list.
        if (mActiveConnections.RemoveElement(conn))
            NS_RELEASE(conn);
    }

    return rv;
}

// called with the connection lock held
nsresult
nsHttpHandler::RemovePendingTransaction(nsHttpTransaction *trans)
{
    LOG(("nsHttpHandler::RemovePendingTransaction [trans=%x]\n", trans));

    NS_ENSURE_ARG_POINTER(trans);

    nsPendingTransaction *pt = nsnull;
    PRInt32 i;
    for (i=0; i<mTransactionQ.Count(); ++i) {
        pt = (nsPendingTransaction *) mTransactionQ[i];

        if (pt->Transaction() == trans) {
            mTransactionQ.RemoveElementAt(i);
            delete pt;
            return NS_OK;
        }
    }

    NS_WARNING("transaction not in pending queue");
    return NS_ERROR_NOT_AVAILABLE;
}

// we're at the active connection limit if any one of the following conditions is true:
//  (1) at max-connections
//  (2) keep-alive enabled and at max-persistent-connections-per-host
//  (3) keep-alive disabled and at max-connections-per-host
PRBool
nsHttpHandler::AtActiveConnectionLimit(nsHttpConnectionInfo *ci, PRUint8 caps)
{
    LOG(("nsHttpHandler::AtActiveConnectionLimit [host=%s:%d caps=%x]\n",
        ci->Host(), ci->Port(), caps));

    // use >= just to be safe
    if (mActiveConnections.Count() >= mMaxConnections)
        return PR_TRUE;

    nsHttpConnection *conn;
    PRUint8 totalCount = 0, persistentCount = 0;
    PRInt32 i;
    for (i=0; i<mActiveConnections.Count(); ++i) {
        conn = NS_STATIC_CAST(nsHttpConnection *, mActiveConnections[i]); 
        LOG((">> comparing against active connection [conn=%x host=%s:%d]\n", 
            conn, conn->ConnectionInfo()->Host(), conn->ConnectionInfo()->Port()));
        if (conn->ConnectionInfo()->Equals(ci)) {
            totalCount++;
            if (conn->IsKeepAlive())
                persistentCount++;
        }
    }

    LOG(("   total-count=%u, persistent-count=%u\n",
        PRUint32(totalCount), PRUint32(persistentCount)));

    // use >= just to be safe
    return (totalCount >= mMaxConnectionsPerServer) ||
               ((caps & NS_HTTP_ALLOW_KEEPALIVE) &&
                (persistentCount >= mMaxPersistentConnectionsPerServer));
}

void
nsHttpHandler::DropConnections(nsVoidArray &connections)
{
    nsHttpConnection *conn;
    PRInt32 i;
    for (i=0; i<connections.Count(); ++i) {
        conn = (nsHttpConnection *) connections[i];
        NS_RELEASE(conn);
    }
    connections.Clear();
}

void
nsHttpHandler::BuildUserAgent()
{
    LOG(("nsHttpHandler::BuildUserAgent\n"));

    NS_ASSERTION(mAppName &&
                 mAppVersion &&
                 mPlatform &&
                 mSecurity &&
                 mOscpu,
                 "HTTP cannot send practical requests without this much");

    // Application portion
    mUserAgent.Assign(mAppName);
    mUserAgent += '/';
    mUserAgent += mAppVersion;
    mUserAgent += ' ';

    // Application comment
    mUserAgent += '(';
    mUserAgent += mPlatform;
    mUserAgent += "; ";
    mUserAgent += mSecurity;
    mUserAgent += "; ";
    mUserAgent += mOscpu;
    if (mLanguage) {
        mUserAgent += "; ";
        mUserAgent += mLanguage;
    }
    if (mMisc) {
        mUserAgent += "; ";
        mUserAgent += mMisc;
    }
    mUserAgent += ')';

    // Product portion
    if (mProduct) {
        mUserAgent += ' ';
        mUserAgent += mProduct;
        if (mProductSub) {
            mUserAgent += '/';
            mUserAgent += mProductSub;
        }
        if (mProductComment) {
            mUserAgent += " (";
            mUserAgent += mProductComment;
            mUserAgent += ')';
        }
    }

    // Vendor portion
    if (mVendor) {
        mUserAgent += ' ';
        mUserAgent += mVendor;
        if (mVendorSub) {
            mUserAgent += '/';
            mUserAgent += mVendorSub;
        }
        if (mVendorComment) {
            mUserAgent += " (";
            mUserAgent += mVendorComment;
            mUserAgent += ')';
        }
    }
}

void
nsHttpHandler::InitUserAgentComponents()
{

    // Gather Application name and Version.
    mAppName.Adopt(nsCRT::strdup(UA_APPNAME));
    mAppVersion.Adopt(nsCRT::strdup(UA_APPVERSION));

      // Gather platform.
    mPlatform.Adopt(nsCRT::strdup(
#if defined(XP_OS2)
    "OS/2"
#elif defined(XP_PC)
    "Windows"
#elif defined(RHAPSODY)
    "Macintosh"
#elif defined (XP_UNIX)
    "X11"
#elif defined(XP_BEOS)
    "BeOS"
#elif defined(XP_MAC)
    "Macintosh"
#endif
    ));

    // Gather OS/CPU.
#if defined(XP_OS2)
    ULONG os2ver = 0;
    DosQuerySysInfo(QSV_VERSION_MINOR, QSV_VERSION_MINOR,
                    &os2ver, sizeof(os2ver));
    if (os2ver == 11)
        mOscpu.Adopt(nsCRT::strdup("2.11"));
    else if (os2ver == 30)
        mOscpu.Adopt(nsCRT::strdup("Warp 3"));
    else if (os2ver == 40)
        mOscpu.Adopt(nsCRT::strdup("Warp 4"));
    else if (os2ver == 45)
        mOscpu.Adopt(nsCRT::strdup("Warp 4.5"));

#elif defined(XP_PC)
    OSVERSIONINFO info = { sizeof OSVERSIONINFO };
    if (GetVersionEx(&info)) {
        if (info.dwPlatformId == VER_PLATFORM_WIN32_NT) {
            if (info.dwMajorVersion      == 3)
                mOscpu.Adopt(nsCRT::strdup("WinNT3.51"));
            else if (info.dwMajorVersion == 4)
                mOscpu.Adopt(nsCRT::strdup("WinNT4.0"));
            else {
                char *buf = PR_smprintf("Windows NT %ld.%ld",
                                        info.dwMajorVersion,
                                        info.dwMinorVersion);
                if (buf) {
                    mOscpu.Adopt(nsCRT::strdup(buf));
                    PR_smprintf_free(buf);
                }
            }
        } else if (info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS &&
                   info.dwMajorVersion == 4) {
            if (info.dwMinorVersion == 90)
                mOscpu.Adopt(nsCRT::strdup("Win 9x 4.90"));  // Windows Me
            else if (info.dwMinorVersion > 0)
                mOscpu.Adopt(nsCRT::strdup("Win98"));
            else
                mOscpu.Adopt(nsCRT::strdup("Win95"));
        } else {
            char *buf = PR_smprintf("Windows %ld.%ld",
                                    info.dwMajorVersion,
                                    info.dwMinorVersion);
            if (buf) {
                mOscpu.Adopt(nsCRT::strdup(buf));
                PR_smprintf_free(buf);
            }
        }
    }
#elif defined (XP_UNIX) || defined (XP_BEOS)
    struct utsname name;
    
    int ret = uname(&name);
    if (ret >= 0) {
        nsCString buf;  
        buf =  (char*)name.sysname;
        buf += ' ';
        buf += (char*)name.machine;
        mOscpu.Adopt(ToNewCString(buf));
    }
#elif defined (XP_MAC)
    long version;
    if (::Gestalt(gestaltSystemVersion, &version) == noErr && version >= 0x00001000)
        mOscpu.Adopt(nsCRT::strdup("PPC Mac OS X"));
    else
        mOscpu.Adopt(nsCRT::strdup("PPC"));
#endif

    mUserAgentIsDirty = PR_TRUE;
}

void
nsHttpHandler::PrefsChanged(nsIPrefBranch *prefs, const char *pref)
{
    nsresult rv = NS_OK;
    PRInt32 val;

    LOG(("nsHttpHandler::PrefsChanged [pref=%x]\n", pref));

#define PREF_CHANGED(p) ((pref == nsnull) || !PL_strcmp(pref, p))

    //
    // UA components
    //

    // Gather vendor values.
    if (PREF_CHANGED(UA_PREF("vendor"))) {
        prefs->GetCharPref(UA_PREF("vendor"),
            getter_Copies(mVendor));
        mUserAgentIsDirty = PR_TRUE;
    }
    if (PREF_CHANGED(UA_PREF("vendorSub"))) {
        prefs->GetCharPref(UA_PREF("vendorSub"),
            getter_Copies(mVendorSub));
        mUserAgentIsDirty = PR_TRUE;
    }
    if (PREF_CHANGED(UA_PREF("vendorComment"))) {
        prefs->GetCharPref(UA_PREF("vendorComment"),
            getter_Copies(mVendorComment));
        mUserAgentIsDirty = PR_TRUE;
    }

    // Gather product values.
    if (PREF_CHANGED(UA_PREF("product"))) {
        prefs->GetCharPref(UA_PREF_PREFIX "product",
            getter_Copies(mProduct));
        mUserAgentIsDirty = PR_TRUE;
    }
    if (PREF_CHANGED(UA_PREF("productSub"))) {
        prefs->GetCharPref(UA_PREF("productSub"),
            getter_Copies(mProductSub));
        mUserAgentIsDirty = PR_TRUE;
    }
    if (PREF_CHANGED(UA_PREF("productComment"))) {
        prefs->GetCharPref(UA_PREF("productComment"),
            getter_Copies(mProductComment));
        mUserAgentIsDirty = PR_TRUE;
    }

    // Gather misc value.
    if (PREF_CHANGED(UA_PREF("misc"))) {
        prefs->GetCharPref(UA_PREF("misc"),
            getter_Copies(mMisc));
        mUserAgentIsDirty = PR_TRUE;
    }

    // Get Security level supported
    if (PREF_CHANGED(UA_PREF("security"))) {
        prefs->GetCharPref(UA_PREF("security"), getter_Copies(mSecurity));
        if (!mSecurity)
            mSecurity.Adopt(nsCRT::strdup(UA_APPSECURITY_FALLBACK));
        mUserAgentIsDirty = PR_TRUE;
    }

    // Gather locale.
    if (PREF_CHANGED(UA_PREF("locale"))) {
        nsCOMPtr<nsIPrefLocalizedString> pls;
        prefs->GetComplexValue(UA_PREF("locale"),
                                NS_GET_IID(nsIPrefLocalizedString),
                                getter_AddRefs(pls));
        if (pls) {
            nsXPIDLString uval;
            pls->ToString(getter_Copies(uval));
            if (uval)
                mLanguage.Adopt(ToNewUTF8String(nsDependentString(uval)));
        } 
        mUserAgentIsDirty = PR_TRUE;
    }

    // general.useragent.override
    if (PREF_CHANGED(UA_PREF("override"))) {
        prefs->GetCharPref(UA_PREF("override"),
                            getter_Copies(mUserAgentOverride));
        mUserAgentIsDirty = PR_TRUE;
    }

    // general.useragent.misc
    if (PREF_CHANGED(UA_PREF("misc"))) {
        prefs->GetCharPref(UA_PREF("misc"), getter_Copies(mMisc));
        mUserAgentIsDirty = PR_TRUE;
    }

    //
    // HTTP options
    //

    if (PREF_CHANGED(HTTP_PREF("keep-alive.timeout"))) {
        rv = prefs->GetIntPref(HTTP_PREF("keep-alive.timeout"), &val);
        if (NS_SUCCEEDED(rv))
            mIdleTimeout = (PRUint16) CLAMP(val, 1, 0xffff);
    }

    if (PREF_CHANGED(HTTP_PREF("request.max-attempts"))) {
        rv = prefs->GetIntPref(HTTP_PREF("request.max-attempts"), &val);
        if (NS_SUCCEEDED(rv))
            mMaxRequestAttempts = (PRUint16) CLAMP(val, 1, 0xffff);
    }

    if (PREF_CHANGED(HTTP_PREF("request.max-start-delay"))) {
        rv = prefs->GetIntPref(HTTP_PREF("request.max-start-delay"), &val);
        if (NS_SUCCEEDED(rv))
            mMaxRequestDelay = (PRUint16) CLAMP(val, 0, 0xffff);
    }

    if (PREF_CHANGED(HTTP_PREF("max-connections"))) {
        rv = prefs->GetIntPref(HTTP_PREF("max-connections"), &val);
        if (NS_SUCCEEDED(rv))
            mMaxConnections = (PRUint16) CLAMP(val, 1, 0xffff);
    }

    if (PREF_CHANGED(HTTP_PREF("max-connections-per-server"))) {
        rv = prefs->GetIntPref(HTTP_PREF("max-connections-per-server"), &val);
        if (NS_SUCCEEDED(rv))
            mMaxConnectionsPerServer = (PRUint8) CLAMP(val, 1, 0xff);
    }

    if (PREF_CHANGED(HTTP_PREF("max-persistent-connections-per-server"))) {
        rv = prefs->GetIntPref(HTTP_PREF("max-persistent-connections-per-server"), &val);
        if (NS_SUCCEEDED(rv))
            mMaxPersistentConnectionsPerServer = (PRUint8) CLAMP(val, 1, 0xff);
    }

    if (PREF_CHANGED(HTTP_PREF("sendRefererHeader"))) {
        rv = prefs->GetIntPref(HTTP_PREF("sendRefererHeader"), (PRInt32 *) &val);
        if (NS_SUCCEEDED(rv))
            mReferrerLevel = (PRUint8) CLAMP(val, 0, 0xff);
    }

    if (PREF_CHANGED(HTTP_PREF("version"))) {
        nsXPIDLCString httpVersion;
        prefs->GetCharPref(HTTP_PREF("version"), getter_Copies(httpVersion));
	
        if (httpVersion) {
            if (!PL_strcmp(httpVersion, "1.1"))
                mHttpVersion = NS_HTTP_VERSION_1_1;
            else if (!PL_strcmp(httpVersion, "0.9"))
                mHttpVersion = NS_HTTP_VERSION_0_9;
            else
                mHttpVersion = NS_HTTP_VERSION_1_0;
        }

        if (mHttpVersion == NS_HTTP_VERSION_1_1) {
            mCapabilities = NS_HTTP_ALLOW_KEEPALIVE;
            mProxyCapabilities = NS_HTTP_ALLOW_KEEPALIVE;
        }
        else {
            mCapabilities = 0;
            mProxyCapabilities = 0;
        }
    }

    PRBool cVar = PR_FALSE;

    if (PREF_CHANGED(HTTP_PREF("keep-alive"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("keep-alive"), &cVar);
        if (NS_SUCCEEDED(rv)) {
            if (cVar)
                mCapabilities |= NS_HTTP_ALLOW_KEEPALIVE;
            else
                mCapabilities &= ~NS_HTTP_ALLOW_KEEPALIVE;
        }
    }

    if (PREF_CHANGED(HTTP_PREF("proxy.keep-alive"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("proxy.keep-alive"), &cVar);
        if (NS_SUCCEEDED(rv)) {
            if (cVar)
                mProxyCapabilities |= NS_HTTP_ALLOW_KEEPALIVE;
            else
                mProxyCapabilities &= ~NS_HTTP_ALLOW_KEEPALIVE;
        }
    }

    if (PREF_CHANGED(HTTP_PREF("pipelining"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("pipelining"), &cVar);
        if (NS_SUCCEEDED(rv)) {
            if (cVar)
                mCapabilities |=  NS_HTTP_ALLOW_PIPELINING;
            else
                mCapabilities &= ~NS_HTTP_ALLOW_PIPELINING;
        }
    }

    /*
    mPipelineMaxRequests  = DEFAULT_PIPELINE_MAX_REQUESTS;
    rv = prefs->GetIntPref("network.http.pipelining.maxrequests", &mPipelineMaxRequests );
    */

    if (PREF_CHANGED(HTTP_PREF("proxy.pipelining"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("proxy.pipelining"), &cVar);
        if (NS_SUCCEEDED(rv)) {
            if (cVar)
                mProxyCapabilities |=  NS_HTTP_ALLOW_PIPELINING;
            else
                mProxyCapabilities &= ~NS_HTTP_ALLOW_PIPELINING;
        }
    }

    /*
    if (bChangedAll || PL_strcmp(pref, "network.http.connect.timeout") == 0)
        prefs->GetIntPref("network.http.connect.timeout", &mConnectTimeout);

    if (bChangedAll || PL_strcmp(pref, "network.http.request.timeout") == 0)
        prefs->GetIntPref("network.http.request.timeout", &mRequestTimeout);
    */

    if (PREF_CHANGED(HTTP_PREF("accept.default"))) {
        nsXPIDLCString accept;
        rv = prefs->GetCharPref(HTTP_PREF("accept.default"),
                                  getter_Copies(accept));
        if (NS_SUCCEEDED(rv))
            SetAccept(accept);
    }
    
    if (PREF_CHANGED(HTTP_PREF("accept-encoding"))) {
        nsXPIDLCString acceptEncodings;
        rv = prefs->GetCharPref(HTTP_PREF("accept-encoding"),
                                  getter_Copies(acceptEncodings));
        if (NS_SUCCEEDED(rv))
            SetAcceptEncodings(acceptEncodings);
    }

    if (PREF_CHANGED(HTTP_PREF("use-cache"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("use-cache"), &cVar);
        if (NS_SUCCEEDED(rv)) {
            mUseCache = cVar;
            if (!mUseCache) {
                // release our references to the cache
                mCacheSession_ANY = 0;
                mCacheSession_MEM = 0;
            }
        }
    }

    //
    // INTL options
    //

    if (PREF_CHANGED(INTL_ACCEPT_LANGUAGES)) {
        nsCOMPtr<nsIPrefLocalizedString> pls;
        prefs->GetComplexValue(INTL_ACCEPT_LANGUAGES,
                                NS_GET_IID(nsIPrefLocalizedString),
                                getter_AddRefs(pls));
        if (pls) {
            nsXPIDLString uval;
            pls->ToString(getter_Copies(uval));
            if (uval)
                SetAcceptLanguages(NS_ConvertUCS2toUTF8(uval).get());
        } 
    }

    if (PREF_CHANGED(INTL_ACCEPT_CHARSET)) {
        nsCOMPtr<nsIPrefLocalizedString> pls;
        prefs->GetComplexValue(INTL_ACCEPT_CHARSET,
                                NS_GET_IID(nsIPrefLocalizedString),
                                getter_AddRefs(pls));
        if (pls) {
            nsXPIDLString uval;
            pls->ToString(getter_Copies(uval));
            if (uval)
                SetAcceptCharsets(NS_ConvertUCS2toUTF8(uval).get());
        } 
    }

#undef PREF_CHANGED
}

void
nsHttpHandler::GetPrefBranch(nsIPrefBranch **result)
{
    *result = nsnull;
    nsCOMPtr<nsIPrefService> prefService =
        do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefService)
        prefService->GetBranch(nsnull, result);
}

/**
 *  Allocates a C string into that contains a ISO 639 language list
 *  notated with HTTP "q" values for output with a HTTP Accept-Language
 *  header. Previous q values will be stripped because the order of
 *  the langs imply the q value. The q values are calculated by dividing
 *  1.0 amongst the number of languages present.
 *
 *  Ex: passing: "en, ja"
 *      returns: "en, ja;q=0.50"
 *
 *      passing: "en, ja, fr_CA"
 *      returns: "en, ja;q=0.66, fr_CA;q=0.33"
 */
static nsresult
PrepareAcceptLanguages(const char *i_AcceptLanguages, nsACString &o_AcceptLanguages)
{
    if (!i_AcceptLanguages)
        return NS_OK;

    PRUint32 n, size, wrote;
    double q, dec;
    char *p, *p2, *token, *q_Accept, *o_Accept;
    const char *comma;
    PRInt32 available;


    o_Accept = nsCRT::strdup(i_AcceptLanguages);
    if (nsnull == o_Accept)
        return NS_ERROR_OUT_OF_MEMORY;
    for (p = o_Accept, n = size = 0; '\0' != *p; p++) {
        if (*p == ',') n++;
            size++;
    }

    available = size + ++n * 11 + 1;
    q_Accept = new char[available];
    if ((char *) 0 == q_Accept)
        return nsnull;
    *q_Accept = '\0';
    q = 1.0;
    dec = q / (double) n;
    n = 0;
    p2 = q_Accept;
    for (token = nsCRT::strtok(o_Accept, ",", &p);
         token != (char *) 0;
         token = nsCRT::strtok(p, ",", &p))
    {
        while (*token == ' ' || *token == '\x9') token++;
        char* trim;
        trim = PL_strpbrk(token, "; \x9");
        if (trim != (char*)0)  // remove "; q=..." if present
            *trim = '\0';

        if (*token != '\0') {
            comma = n++ != 0 ? ", " : ""; // delimiter if not first item
            if (q < 0.9995)
                wrote = PR_snprintf(p2, available, "%s%s;q=0.%02u", comma, token, (unsigned) (q * 100.0));
            else
                wrote = PR_snprintf(p2, available, "%s%s", comma, token);
            q -= dec;
            p2 += wrote;
            available -= wrote;
            NS_ASSERTION(available > 0, "allocated string not long enough");
        }
    }
    nsCRT::free(o_Accept);

    o_AcceptLanguages.Assign((const char *) q_Accept);
    delete [] q_Accept;

    return NS_OK;
}

nsresult
nsHttpHandler::SetAcceptLanguages(const char *aAcceptLanguages) 
{
    nsCString buf;
    nsresult rv = PrepareAcceptLanguages(aAcceptLanguages, buf);
    if (NS_SUCCEEDED(rv))
        mAcceptLanguages.Assign(buf.get());
    return rv;
}

/**
 *  Allocates a C string into that contains a character set/encoding list
 *  notated with HTTP "q" values for output with a HTTP Accept-Charset
 *  header. If the UTF-8 character set is not present, it will be added.
 *  If a wildcard catch-all is not present, it will be added. If more than
 *  one charset is set (as of 2001-02-07, only one is used), they will be
 *  comma delimited and with q values set for each charset in decending order.
 *
 *  Ex: passing: "euc-jp"
 *      returns: "euc-jp, utf-8;q=0.667, *;q=0.667"
 *
 *      passing: "UTF-8"
 *      returns: "UTF-8, *"
 */
static nsresult
PrepareAcceptCharsets(const char *i_AcceptCharset, nsACString &o_AcceptCharset)
{
    PRUint32 n, size, wrote;
    PRInt32 available;
    double q, dec;
    char *p, *p2, *token, *q_Accept, *o_Accept;
    const char *acceptable, *comma;
    PRBool add_utf = PR_FALSE;
    PRBool add_asterick = PR_FALSE;

    if (!i_AcceptCharset)
        acceptable = "";
    else
        acceptable = i_AcceptCharset;
    o_Accept = nsCRT::strdup(acceptable);
    if (nsnull == o_Accept)
        return NS_ERROR_OUT_OF_MEMORY;
    for (p = o_Accept, n = size = 0; '\0' != *p; p++) {
        if (*p == ',') n++;
            size++;
    }

    // only add "utf-8" and "*" to the list if they aren't
    // already specified.

    if (PL_strcasestr(acceptable, "utf-8") == NULL) {
        n++;
        add_utf = PR_TRUE;
    }
    if (PL_strstr(acceptable, "*") == NULL) {
        n++;
        add_asterick = PR_TRUE;
    }

    available = size + ++n * 11 + 1;
    q_Accept = new char[available];
    if ((char *) 0 == q_Accept)
        return NS_ERROR_OUT_OF_MEMORY;
    *q_Accept = '\0';
    q = 1.0;
    dec = q / (double) n;
    n = 0;
    p2 = q_Accept;
    for (token = nsCRT::strtok(o_Accept, ",", &p);
         token != (char *) 0;
         token = nsCRT::strtok(p, ",", &p)) {
        while (*token == ' ' || *token == '\x9') token++;
        char* trim;
        trim = PL_strpbrk(token, "; \x9");
        if (trim != (char*)0)  // remove "; q=..." if present
            *trim = '\0';

        if (*token != '\0') {
            comma = n++ != 0 ? ", " : ""; // delimiter if not first item
            if (q < 0.9995)
                wrote = PR_snprintf(p2, available, "%s%s;q=0.%02u", comma, token, (unsigned) (q * 100.0));
            else
                wrote = PR_snprintf(p2, available, "%s%s", comma, token);
            q -= dec;
            p2 += wrote;
            available -= wrote;
            NS_ASSERTION(available > 0, "allocated string not long enough");
        }
    }
    if (add_utf) {
        comma = n++ != 0 ? ", " : ""; // delimiter if not first item
        if (q < 0.9995)
            wrote = PR_snprintf(p2, available, "%sutf-8;q=0.%02u", comma, (unsigned) (q * 100.0));
        else
            wrote = PR_snprintf(p2, available, "%sutf-8", comma);
        q -= dec;
        p2 += wrote;
        available -= wrote;
        NS_ASSERTION(available > 0, "allocated string not long enough");
    }
    if (add_asterick) {
        comma = n++ != 0 ? ", " : ""; // delimiter if not first item

        // keep q of "*" equal to the lowest q value
        // in the event of a tie between the q of "*" and a non-wildcard
        // the non-wildcard always receives preference.

        q += dec;
        if (q < 0.9995) {
            wrote = PR_snprintf(p2, available, "%s*;q=0.%02u", comma, (unsigned) (q * 100.0));
        }
        else
            wrote = PR_snprintf(p2, available, "%s*", comma);
        available -= wrote;
        p2 += wrote;
        NS_ASSERTION(available > 0, "allocated string not long enough");
    }
    nsCRT::free(o_Accept);

    // change alloc from C++ new/delete to nsCRT::strdup's way
    o_AcceptCharset.Assign(q_Accept);
#if defined DEBUG_havill
    printf("Accept-Charset: %s\n", q_Accept);
#endif
    delete [] q_Accept;
    return NS_OK;
}

nsresult
nsHttpHandler::SetAcceptCharsets(const char *aAcceptCharsets) 
{
    nsCString buf;
    nsresult rv = PrepareAcceptCharsets(aAcceptCharsets, buf);
    if (NS_SUCCEEDED(rv))
        mAcceptCharsets.Assign(buf.get());
    return rv;
}

nsresult
nsHttpHandler::SetAccept(const char *aAccept) 
{
    mAccept = aAccept;
    return NS_OK;
}

nsresult
nsHttpHandler::SetAcceptEncodings(const char *aAcceptEncodings) 
{
    mAcceptEncodings = aAcceptEncodings;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS5(nsHttpHandler,
                              nsIHttpProtocolHandler,
                              nsIProxiedProtocolHandler,
                              nsIProtocolHandler,
                              nsIObserver,
                              nsISupportsWeakReference)

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIProtocolHandler
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpHandler::GetScheme(char **aScheme)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpHandler::GetDefaultPort(PRInt32 *aDefaultPort)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpHandler::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_STD | ALLOWS_PROXY | ALLOWS_PROXY_HTTP;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **aURI)
{
    nsresult rv = NS_OK;

    LOG(("nsHttpHandler::NewURI\n"));

    nsCOMPtr<nsIStandardURL> url = do_CreateInstance(kStandardURLCID, &rv);
    if (NS_FAILED(rv)) return rv;

    // XXX need to choose the default port based on the scheme
    rv = url->Init(nsIStandardURL::URLTYPE_AUTHORITY, 80, aSpec, aBaseURI);
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface(url, aURI);
}

NS_IMETHODIMP
nsHttpHandler::NewChannel(nsIURI *uri, nsIChannel **result)
{
    LOG(("nsHttpHandler::NewChannel\n"));

    NS_ENSURE_ARG_POINTER(uri);
    NS_ENSURE_ARG_POINTER(result);

    PRBool isHttp = PR_FALSE, isHttps = PR_FALSE;

    // Verify that we have been given a valid scheme
    nsresult rv = uri->SchemeIs("http", &isHttp);
    if (NS_FAILED(rv)) return rv;
    if (!isHttp) {
        rv = uri->SchemeIs("https", &isHttps);
        if (NS_FAILED(rv)) return rv;
        if (!isHttps) {
            NS_WARNING("Invalid URI scheme");
            return NS_ERROR_UNEXPECTED;
        }
    }
    
    rv = NewProxiedChannel(uri,
                           nsnull,
                           result);
    return rv;
}

NS_IMETHODIMP 
nsHttpHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIProxiedProtocolHandler
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpHandler::NewProxiedChannel(nsIURI *uri,
                                 nsIProxyInfo* proxyInfo,
                                 nsIChannel **result)
{
    nsHttpChannel *httpChannel = nsnull;

    LOG(("nsHttpHandler::NewProxiedChannel [proxyInfo=%p]\n",
        proxyInfo));

    NS_NEWXPCOM(httpChannel, nsHttpChannel);
    if (!httpChannel)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(httpChannel);

    nsresult rv = httpChannel->Init(uri,
                                    mCapabilities,
                                    proxyInfo);

    if (NS_SUCCEEDED(rv))
        rv = httpChannel->
                QueryInterface(NS_GET_IID(nsIChannel), (void **) result);

    NS_RELEASE(httpChannel);
    return rv;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIHttpProtocolHandler
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpHandler::GetUserAgent(char **aUserAgent)
{
    return DupString(UserAgent(), aUserAgent);
}

NS_IMETHODIMP
nsHttpHandler::GetAppName(char **aAppName)
{
    return DupString(mAppName, aAppName);
}

NS_IMETHODIMP
nsHttpHandler::GetAppVersion(char **aAppVersion)
{
    return DupString(mAppVersion, aAppVersion);
}

NS_IMETHODIMP
nsHttpHandler::GetVendor(char **aVendor)
{
    return DupString(mVendor, aVendor);
}
NS_IMETHODIMP
nsHttpHandler::SetVendor(const char *aVendor)
{
    mVendor.Adopt(aVendor ? nsCRT::strdup(aVendor) : 0);
    mUserAgentIsDirty = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetVendorSub(char **aVendorSub)
{
    return DupString(mVendorSub, aVendorSub);
}
NS_IMETHODIMP
nsHttpHandler::SetVendorSub(const char *aVendorSub)
{
    mVendorSub.Adopt(aVendorSub ? nsCRT::strdup(aVendorSub) : 0);
    mUserAgentIsDirty = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetVendorComment(char **aVendorComment)
{
    return DupString(mVendorComment, aVendorComment);
}
NS_IMETHODIMP
nsHttpHandler::SetVendorComment(const char *aVendorComment)
{
    mVendorComment.Adopt(aVendorComment ? nsCRT::strdup(aVendorComment) : 0);
    mUserAgentIsDirty = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetProduct(char **aProduct)
{
    return DupString(mProduct, aProduct);
}
NS_IMETHODIMP
nsHttpHandler::SetProduct(const char *aProduct)
{
    mProduct.Adopt(aProduct ? nsCRT::strdup(aProduct) : 0);
    mUserAgentIsDirty = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetProductSub(char **aProductSub)
{
    return DupString(mProductSub, aProductSub);
}
NS_IMETHODIMP
nsHttpHandler::SetProductSub(const char *aProductSub)
{
    mProductSub.Adopt(aProductSub ? nsCRT::strdup(aProductSub) : 0);
    mUserAgentIsDirty = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetProductComment(char **aProductComment)
{
    return DupString(mProductComment, aProductComment);
}
NS_IMETHODIMP
nsHttpHandler::SetProductComment(const char *aProductComment)
{
    mProductComment.Adopt(aProductComment ? nsCRT::strdup(aProductComment) : 0);
    mUserAgentIsDirty = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetPlatform(char **aPlatform)
{
    return DupString(mPlatform, aPlatform);
}

NS_IMETHODIMP
nsHttpHandler::GetOscpu(char **aOscpu)
{
    return DupString(mOscpu, aOscpu);
}

NS_IMETHODIMP
nsHttpHandler::GetLanguage(char **aLanguage)
{
    return DupString(mLanguage, aLanguage);
}
NS_IMETHODIMP
nsHttpHandler::SetLanguage(const char *aLanguage)
{
    mLanguage.Adopt(aLanguage ? nsCRT::strdup(aLanguage) : 0);
    mUserAgentIsDirty = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetMisc(char **aMisc)
{
    return DupString(mMisc, aMisc);
}
NS_IMETHODIMP
nsHttpHandler::SetMisc(const char *aMisc)
{
    mMisc.Adopt(aMisc ? nsCRT::strdup(aMisc) : 0);
    mUserAgentIsDirty = PR_TRUE;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpHandler::Observe(nsISupports *subject,
                       const PRUnichar *topic,
                       const PRUnichar *data)
{
    if (!nsCRT::strcmp(topic, NS_LITERAL_STRING("profile-before-change").get()) ||
        !nsCRT::strcmp(topic, NS_LITERAL_STRING("session-logout").get())) {
        // clear cache of all authentication credentials.
        if (mAuthCache)
            mAuthCache->ClearAll();

        // need to reset the session start time since cache validation may
        // depend on this value.
        mSessionStartTime = NowInSeconds();
    }
    else if (!nsCRT::strcmp(topic, NS_LITERAL_STRING("nsPref:changed").get())) {
        nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(subject);
        if (prefBranch)
            PrefsChanged(prefBranch, NS_ConvertUCS2toUTF8(data).get());
    }
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsPendingTransaction
//-----------------------------------------------------------------------------

nsHttpHandler::
nsPendingTransaction::nsPendingTransaction(nsHttpTransaction *trans,
                                           nsHttpConnectionInfo *ci)
    : mTransaction(trans)
    , mConnectionInfo(ci)
    , mBusy(0)
{
    LOG(("Creating nsPendingTransaction @%x\n", this));

    NS_PRECONDITION(mTransaction, "null transaction");
    NS_PRECONDITION(mConnectionInfo, "null connection info");

    NS_ADDREF(mTransaction);
    NS_ADDREF(mConnectionInfo);
}

nsHttpHandler::
nsPendingTransaction::~nsPendingTransaction()
{
    LOG(("Destroying nsPendingTransaction @%x\n", this));
 
    NS_RELEASE(mTransaction);
    NS_RELEASE(mConnectionInfo);
}
