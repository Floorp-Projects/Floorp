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

#ifndef nsHttpHandler_h__
#define nsHttpHandler_h__

#include "nsHttp.h"
#include "nsHttpAuthCache.h"
#include "nsHttpConnection.h"
#include "nsHttpConnectionMgr.h"

#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"

#include "nsIHttpProtocolHandler.h"
#include "nsIProtocolProxyService.h"
#include "nsIIOService.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIProxyObjectManager.h"
#include "nsIPrivateBrowsingService.h"
#include "nsIStreamConverterService.h"
#include "nsICacheSession.h"
#include "nsICookieService.h"
#include "nsIIDNService.h"
#include "nsITimer.h"
#include "nsIStrictTransportSecurityService.h"

class nsHttpConnectionInfo;
class nsHttpHeaderArray;
class nsHttpTransaction;
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
    PRUint32       PhishyUserPassLength()    { return mPhishyUserPassLength; }
    PRUint8        GetQoSBits()              { return mQoSBits; }
    PRUint16       GetIdleSynTimeout()       { return mIdleSynTimeout; }
    
    PRBool         IsPersistentHttpsCachingEnabled() { return mEnablePersistentHttpsCaching; }

    PRBool         PromptTempRedirect()      { return mPromptTempRedirect; }

    nsHttpAuthCache     *AuthCache() { return &mAuthCache; }
    nsHttpConnectionMgr *ConnMgr()   { return mConnMgr; }

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
    nsresult InitiateTransaction(nsHttpTransaction *trans, PRInt32 priority)
    {
        return mConnMgr->AddTransaction(trans, priority);
    }

    // Called to change the priority of an existing transaction that has
    // already been initiated.
    nsresult RescheduleTransaction(nsHttpTransaction *trans, PRInt32 priority)
    {
        return mConnMgr->RescheduleTransaction(trans, priority);
    }

    // Called to cancel a transaction, which may or may not be assigned to
    // a connection.  Callable from any thread.
    nsresult CancelTransaction(nsHttpTransaction *trans, nsresult reason)
    {
        return mConnMgr->CancelTransaction(trans, reason);
    }

    // Called when a connection is done processing a transaction.  Callable
    // from any thread.
    nsresult ReclaimConnection(nsHttpConnection *conn)
    {
        return mConnMgr->ReclaimConnection(conn);
    }

    nsresult ProcessPendingQ(nsHttpConnectionInfo *cinfo)
    {
        return mConnMgr->ProcessPendingQ(cinfo);
    }

    nsresult GetSocketThreadTarget(nsIEventTarget **target)
    {
        return mConnMgr->GetSocketThreadTarget(target);
    }

    // for anything that wants to know if we're in private browsing mode.
    PRBool InPrivateBrowsingMode();

    //
    // The HTTP handler caches pointers to specific XPCOM services, and
    // provides the following helper routines for accessing those services:
    //
    nsresult GetStreamConverterService(nsIStreamConverterService **);
    nsresult GetIOService(nsIIOService** service);
    nsICookieService * GetCookieService(); // not addrefed
    nsIStrictTransportSecurityService * GetSTSService();

    // Called by the channel before writing a request
    void OnModifyRequest(nsIHttpChannel *chan)
    {
        NotifyObservers(chan, NS_HTTP_ON_MODIFY_REQUEST_TOPIC);
    }

    // Called by the channel once headers are available
    void OnExamineResponse(nsIHttpChannel *chan)
    {
        NotifyObservers(chan, NS_HTTP_ON_EXAMINE_RESPONSE_TOPIC);
    }

    // Called by the channel once headers have been merged with cached headers
    void OnExamineMergedResponse(nsIHttpChannel *chan)
    {
        NotifyObservers(chan, NS_HTTP_ON_EXAMINE_MERGED_RESPONSE_TOPIC);
    }

    // Called by channels before a redirect happens. This notifies both the
    // channel's and the global redirect observers.
    nsresult AsyncOnChannelRedirect(nsIChannel* oldChan, nsIChannel* newChan,
                               PRUint32 flags);

    // Called by the channel when the response is read from the cache without
    // communicating with the server.
    void OnExamineCachedResponse(nsIHttpChannel *chan)
    {
        NotifyObservers(chan, NS_HTTP_ON_EXAMINE_CACHED_RESPONSE_TOPIC);
    }

    // Generates the host:port string for use in the Host: header as well as the
    // CONNECT line for proxies. This handles IPv6 literals correctly.
    static nsresult GenerateHostPort(const nsCString& host, PRInt32 port,
                                     nsCString& hostLine);

private:

    //
    // Useragent/prefs helper methods
    //
    void     BuildUserAgent();
    void     InitUserAgentComponents();
    void     PrefsChanged(nsIPrefBranch *prefs, const char *pref);

    nsresult SetAccept(const char *);
    nsresult SetAcceptLanguages(const char *);
    nsresult SetAcceptEncodings(const char *);
    nsresult SetAcceptCharsets(const char *);

    nsresult InitConnectionMgr();

    void     NotifyObservers(nsIHttpChannel *chan, const char *event);

private:

    // cached services
    nsCOMPtr<nsIIOService>              mIOService;
    nsCOMPtr<nsIStreamConverterService> mStreamConvSvc;
    nsCOMPtr<nsIObserverService>        mObserverService;
    nsCOMPtr<nsICookieService>          mCookieService;
    nsCOMPtr<nsIIDNService>             mIDNConverter;
    nsCOMPtr<nsIStrictTransportSecurityService> mSTSService;

    // the authentication credentials cache
    nsHttpAuthCache mAuthCache;

    // the connection manager
    nsHttpConnectionMgr *mConnMgr;

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
    PRUint16 mIdleSynTimeout;

    PRUint16 mMaxConnections;
    PRUint8  mMaxConnectionsPerServer;
    PRUint8  mMaxPersistentConnectionsPerServer;
    PRUint8  mMaxPersistentConnectionsPerProxy;
    PRUint8  mMaxPipelinedRequests;

    PRUint8  mRedirectionLimit;

    // we'll warn the user if we load an URL containing a userpass field
    // unless its length is less than this threshold.  this warning is
    // intended to protect the user against spoofing attempts that use
    // the userpass field of the URL to obscure the actual origin server.
    PRUint8  mPhishyUserPassLength;

    PRUint8  mQoSBits;

    PRPackedBool mPipeliningOverSSL;

    // cached value of whether or not the browser is in private browsing mode.
    enum {
        PRIVATE_BROWSING_OFF = PR_FALSE,
        PRIVATE_BROWSING_ON = PR_TRUE,
        PRIVATE_BROWSING_UNKNOWN = 2
    } mInPrivateBrowsingMode;

    nsCString mAccept;
    nsCString mAcceptLanguages;
    nsCString mAcceptEncodings;
    nsCString mAcceptCharsets;

    nsXPIDLCString mDefaultSocketType;

    // cache support
    PRUint32                  mLastUniqueID;
    PRUint32                  mSessionStartTime;

    // useragent components
    nsCString      mLegacyAppName;
    nsCString      mLegacyAppVersion;
    nsCString      mPlatform;
    nsCString      mOscpu;
    nsCString      mLanguage;
    nsCString      mMisc;
    nsCString      mProduct;
    nsXPIDLCString mProductSub;
    nsXPIDLCString mAppName;
    nsXPIDLCString mAppVersion;
    nsCString      mCompatFirefox;

    nsCString      mUserAgent;
    nsXPIDLCString mUserAgentOverride;
    PRPackedBool   mUserAgentIsDirty; // true if mUserAgent should be rebuilt

    PRPackedBool   mUseCache;

    PRPackedBool   mPromptTempRedirect;
    // mSendSecureXSiteReferrer: default is false, 
    // if true allow referrer headers between secure non-matching hosts
    PRPackedBool   mSendSecureXSiteReferrer;

    // Persistent HTTPS caching flag
    PRPackedBool   mEnablePersistentHttpsCaching;

    // For broadcasting the preference to not be tracked
    PRPackedBool   mDoNotTrackEnabled;
};

//-----------------------------------------------------------------------------

extern nsHttpHandler *gHttpHandler;

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
    NS_FORWARD_NSIPROXIEDPROTOCOLHANDLER (gHttpHandler->)
    NS_FORWARD_NSIHTTPPROTOCOLHANDLER    (gHttpHandler->)

    nsHttpsHandler() { }
    virtual ~nsHttpsHandler() { }

    nsresult Init();
};

#endif // nsHttpHandler_h__
