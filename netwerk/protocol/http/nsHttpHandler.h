/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpHandler_h__
#define nsHttpHandler_h__

#include "nsHttp.h"
#include "nsHttpAuthCache.h"
#include "nsHttpConnection.h"
#include "nsHttpConnectionMgr.h"
#include "ASpdySession.h"

#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"

#include "nsIHttpProtocolHandler.h"
#include "nsIProtocolProxyService.h"
#include "nsIIOService.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIStreamConverterService.h"
#include "nsICacheSession.h"
#include "nsICookieService.h"
#include "nsIIDNService.h"
#include "nsITimer.h"
#include "nsIStrictTransportSecurityService.h"
#include "nsISpeculativeConnect.h"

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
                    , public nsISpeculativeConnect
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
    NS_DECL_NSIPROXIEDPROTOCOLHANDLER
    NS_DECL_NSIHTTPPROTOCOLHANDLER
    NS_DECL_NSIOBSERVER
    NS_DECL_NSISPECULATIVECONNECT

    nsHttpHandler();
    virtual ~nsHttpHandler();

    nsresult Init();
    nsresult AddStandardRequestHeaders(nsHttpHeaderArray *,
                                       PRUint8 capabilities,
                                       bool useProxy);
    bool     IsAcceptableEncoding(const char *encoding);

    const nsAFlatCString &UserAgent();

    nsHttpVersion  HttpVersion()             { return mHttpVersion; }
    nsHttpVersion  ProxyHttpVersion()        { return mProxyHttpVersion; }
    PRUint8        ReferrerLevel()           { return mReferrerLevel; }
    bool           SendSecureXSiteReferrer() { return mSendSecureXSiteReferrer; }
    PRUint8        RedirectionLimit()        { return mRedirectionLimit; }
    PRIntervalTime IdleTimeout()             { return mIdleTimeout; }
    PRIntervalTime SpdyTimeout()             { return mSpdyTimeout; }
    PRUint16       MaxRequestAttempts()      { return mMaxRequestAttempts; }
    const char    *DefaultSocketType()       { return mDefaultSocketType.get(); /* ok to return null */ }
    nsIIDNService *IDNConverter()            { return mIDNConverter; }
    PRUint32       PhishyUserPassLength()    { return mPhishyUserPassLength; }
    PRUint8        GetQoSBits()              { return mQoSBits; }
    PRUint16       GetIdleSynTimeout()       { return mIdleSynTimeout; }
    bool           FastFallbackToIPv4()      { return mFastFallbackToIPv4; }
    PRUint32       MaxSocketCount();
    bool           EnforceAssocReq()         { return mEnforceAssocReq; }

    bool           IsPersistentHttpsCachingEnabled() { return mEnablePersistentHttpsCaching; }
    bool           IsTelemetryEnabled() { return mTelemetryEnabled; }
    bool           AllowExperiments() { return mTelemetryEnabled && mAllowExperiments; }

    bool           IsSpdyEnabled() { return mEnableSpdy; }
    bool           IsSpdyV2Enabled() { return mSpdyV2; }
    bool           IsSpdyV3Enabled() { return mSpdyV3; }
    bool           CoalesceSpdy() { return mCoalesceSpdy; }
    bool           UseAlternateProtocol() { return mUseAlternateProtocol; }
    PRUint32       SpdySendingChunkSize() { return mSpdySendingChunkSize; }
    PRIntervalTime SpdyPingThreshold() { return mSpdyPingThreshold; }
    PRIntervalTime SpdyPingTimeout() { return mSpdyPingTimeout; }

    bool           PromptTempRedirect()      { return mPromptTempRedirect; }

    nsHttpAuthCache     *AuthCache() { return &mAuthCache; }
    nsHttpConnectionMgr *ConnMgr()   { return mConnMgr; }

    // cache support
    bool UseCache() const { return mUseCache; }
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

    nsresult SpeculativeConnect(nsHttpConnectionInfo *ci,
                                nsIInterfaceRequestor *callbacks,
                                nsIEventTarget *target)
    {
        return mConnMgr->SpeculativeConnect(ci, callbacks, target);
    }

    //
    // The HTTP handler caches pointers to specific XPCOM services, and
    // provides the following helper routines for accessing those services:
    //
    nsresult GetStreamConverterService(nsIStreamConverterService **);
    nsresult GetIOService(nsIIOService** service);
    nsICookieService * GetCookieService(); // not addrefed
    nsIStrictTransportSecurityService * GetSTSService();

    // callable from socket thread only
    PRUint32 Get32BitsOfPseudoRandom();

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

    bool GetPipelineAggressive()     { return mPipelineAggressive; }
    void GetMaxPipelineObjectSize(PRInt64 *outVal)
    {
        *outVal = mMaxPipelineObjectSize;
    }

    bool GetPipelineEnabled()
    {
        return mCapabilities & NS_HTTP_ALLOW_PIPELINING;
    }

    bool GetPipelineRescheduleOnTimeout()
    {
        return mPipelineRescheduleOnTimeout;
    }

    PRIntervalTime GetPipelineRescheduleTimeout()
    {
        return mPipelineRescheduleTimeout;
    }
    
    PRIntervalTime GetPipelineTimeout()   { return mPipelineReadTimeout; }

    mozilla::net::SpdyInformation *SpdyInfo() { return &mSpdyInfo; }

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

    bool mFastFallbackToIPv4;

    PRIntervalTime mIdleTimeout;
    PRIntervalTime mSpdyTimeout;

    PRUint16 mMaxRequestAttempts;
    PRUint16 mMaxRequestDelay;
    PRUint16 mIdleSynTimeout;

    PRUint16 mMaxConnections;
    PRUint8  mMaxConnectionsPerServer;
    PRUint8  mMaxPersistentConnectionsPerServer;
    PRUint8  mMaxPersistentConnectionsPerProxy;
    PRUint16 mMaxPipelinedRequests;
    PRUint16 mMaxOptimisticPipelinedRequests;
    bool     mPipelineAggressive;
    PRInt64  mMaxPipelineObjectSize;
    bool     mPipelineRescheduleOnTimeout;
    PRIntervalTime mPipelineRescheduleTimeout;
    PRIntervalTime mPipelineReadTimeout;

    PRUint8  mRedirectionLimit;

    // we'll warn the user if we load an URL containing a userpass field
    // unless its length is less than this threshold.  this warning is
    // intended to protect the user against spoofing attempts that use
    // the userpass field of the URL to obscure the actual origin server.
    PRUint8  mPhishyUserPassLength;

    PRUint8  mQoSBits;

    bool mPipeliningOverSSL;
    bool mEnforceAssocReq;

    nsCString mAccept;
    nsCString mAcceptLanguages;
    nsCString mAcceptEncodings;

    nsXPIDLCString mDefaultSocketType;

    // cache support
    PRUint32                  mLastUniqueID;
    PRUint32                  mSessionStartTime;

    // useragent components
    nsCString      mLegacyAppName;
    nsCString      mLegacyAppVersion;
    nsCString      mPlatform;
    nsCString      mOscpu;
    nsCString      mMisc;
    nsCString      mProduct;
    nsXPIDLCString mProductSub;
    nsXPIDLCString mAppName;
    nsXPIDLCString mAppVersion;
    nsCString      mCompatFirefox;
    nsXPIDLCString mCompatDevice;

    nsCString      mUserAgent;
    nsXPIDLCString mUserAgentOverride;
    bool           mUserAgentIsDirty; // true if mUserAgent should be rebuilt

    bool           mUseCache;

    bool           mPromptTempRedirect;
    // mSendSecureXSiteReferrer: default is false, 
    // if true allow referrer headers between secure non-matching hosts
    bool           mSendSecureXSiteReferrer;

    // Persistent HTTPS caching flag
    bool           mEnablePersistentHttpsCaching;

    // For broadcasting the preference to not be tracked
    bool           mDoNotTrackEnabled;
    
    // Whether telemetry is reported or not
    bool           mTelemetryEnabled;

    // The value of network.allow-experiments
    bool           mAllowExperiments;

    // Try to use SPDY features instead of HTTP/1.1 over SSL
    mozilla::net::SpdyInformation mSpdyInfo;
    bool           mEnableSpdy;
    bool           mSpdyV2;
    bool           mSpdyV3;
    bool           mCoalesceSpdy;
    bool           mUseAlternateProtocol;
    PRUint32       mSpdySendingChunkSize;
    PRIntervalTime mSpdyPingThreshold;
    PRIntervalTime mSpdyPingTimeout;
};

//-----------------------------------------------------------------------------

extern nsHttpHandler *gHttpHandler;

//-----------------------------------------------------------------------------
// nsHttpsHandler - thin wrapper to distinguish the HTTP handler from the
//                  HTTPS handler (even though they share the same impl).
//-----------------------------------------------------------------------------

class nsHttpsHandler : public nsIHttpProtocolHandler
                     , public nsSupportsWeakReference
                     , public nsISpeculativeConnect
{
public:
    // we basically just want to override GetScheme and GetDefaultPort...
    // all other methods should be forwarded to the nsHttpHandler instance.
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
    NS_FORWARD_NSIPROXIEDPROTOCOLHANDLER (gHttpHandler->)
    NS_FORWARD_NSIHTTPPROTOCOLHANDLER    (gHttpHandler->)
    NS_FORWARD_NSISPECULATIVECONNECT     (gHttpHandler->)

    nsHttpsHandler() { }
    virtual ~nsHttpsHandler() { }

    nsresult Init();
};

#endif // nsHttpHandler_h__
