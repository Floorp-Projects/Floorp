/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpHandler_h__
#define nsHttpHandler_h__

#include "nsHttp.h"
#include "nsHttpAuthCache.h"
#include "nsHttpConnectionMgr.h"
#include "ASpdySession.h"

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"

#include "nsIHttpProtocolHandler.h"
#include "nsIObserver.h"
#include "nsISpeculativeConnect.h"
#include "nsICache.h"

class nsIHttpChannel;
class nsIPrefBranch;
class nsICancelable;
class nsICookieService;
class nsIIOService;
class nsIObserverService;
class nsISiteSecurityService;
class nsIStreamConverterService;
class nsITimer;

namespace mozilla {
namespace net {
class ATokenBucketEvent;
class EventTokenBucket;
class Tickler;
class nsHttpConnection;
class nsHttpConnectionInfo;
class nsHttpTransaction;

//-----------------------------------------------------------------------------
// nsHttpHandler - protocol handler for HTTP and HTTPS
//-----------------------------------------------------------------------------

class nsHttpHandler : public nsIHttpProtocolHandler
                    , public nsIObserver
                    , public nsSupportsWeakReference
                    , public nsISpeculativeConnect
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
    NS_DECL_NSIPROXIEDPROTOCOLHANDLER
    NS_DECL_NSIHTTPPROTOCOLHANDLER
    NS_DECL_NSIOBSERVER
    NS_DECL_NSISPECULATIVECONNECT

    nsHttpHandler();

    nsresult Init();
    nsresult AddStandardRequestHeaders(nsHttpHeaderArray *);
    nsresult AddConnectionHeader(nsHttpHeaderArray *,
                                 uint32_t capabilities);
    bool     IsAcceptableEncoding(const char *encoding);

    const nsAFlatCString &UserAgent();

    nsHttpVersion  HttpVersion()             { return mHttpVersion; }
    nsHttpVersion  ProxyHttpVersion()        { return mProxyHttpVersion; }
    uint8_t        ReferrerLevel()           { return mReferrerLevel; }
    bool           SpoofReferrerSource()     { return mSpoofReferrerSource; }
    uint8_t        ReferrerTrimmingPolicy()  { return mReferrerTrimmingPolicy; }
    uint8_t        ReferrerXOriginPolicy()   { return mReferrerXOriginPolicy; }
    bool           SendSecureXSiteReferrer() { return mSendSecureXSiteReferrer; }
    uint8_t        RedirectionLimit()        { return mRedirectionLimit; }
    PRIntervalTime IdleTimeout()             { return mIdleTimeout; }
    PRIntervalTime SpdyTimeout()             { return mSpdyTimeout; }
    PRIntervalTime ResponseTimeout() {
      return mResponseTimeoutEnabled ? mResponseTimeout : 0;
    }
    PRIntervalTime ResponseTimeoutEnabled()  { return mResponseTimeoutEnabled; }
    uint16_t       MaxRequestAttempts()      { return mMaxRequestAttempts; }
    const char    *DefaultSocketType()       { return mDefaultSocketType.get(); /* ok to return null */ }
    uint32_t       PhishyUserPassLength()    { return mPhishyUserPassLength; }
    uint8_t        GetQoSBits()              { return mQoSBits; }
    uint16_t       GetIdleSynTimeout()       { return mIdleSynTimeout; }
    bool           FastFallbackToIPv4()      { return mFastFallbackToIPv4; }
    bool           ProxyPipelining()         { return mProxyPipelining; }
    uint32_t       MaxSocketCount();
    bool           EnforceAssocReq()         { return mEnforceAssocReq; }

    bool           IsPersistentHttpsCachingEnabled() { return mEnablePersistentHttpsCaching; }
    bool           IsTelemetryEnabled() { return mTelemetryEnabled; }
    bool           AllowExperiments() { return mTelemetryEnabled && mAllowExperiments; }

    bool           IsSpdyEnabled() { return mEnableSpdy; }
    bool           IsSpdyV3Enabled() { return mSpdyV3; }
    bool           IsSpdyV31Enabled() { return mSpdyV31; }
    bool           IsHttp2DraftEnabled() { return mHttp2DraftEnabled; }
    bool           EnforceHttp2TlsProfile() { return mEnforceHttp2TlsProfile; }
    bool           CoalesceSpdy() { return mCoalesceSpdy; }
    bool           UseSpdyPersistentSettings() { return mSpdyPersistentSettings; }
    uint32_t       SpdySendingChunkSize() { return mSpdySendingChunkSize; }
    uint32_t       SpdySendBufferSize()      { return mSpdySendBufferSize; }
    uint32_t       SpdyPushAllowance()       { return mSpdyPushAllowance; }
    PRIntervalTime SpdyPingThreshold() { return mSpdyPingThreshold; }
    PRIntervalTime SpdyPingTimeout() { return mSpdyPingTimeout; }
    bool           AllowPush()   { return mAllowPush; }
    uint32_t       ConnectTimeout()  { return mConnectTimeout; }
    uint32_t       ParallelSpeculativeConnectLimit() { return mParallelSpeculativeConnectLimit; }
    bool           CriticalRequestPrioritization() { return mCriticalRequestPrioritization; }
    double         BypassCacheLockThreshold() { return mBypassCacheLockThreshold; }

    uint32_t       MaxConnectionsPerOrigin() { return mMaxPersistentConnectionsPerServer; }
    bool           UseRequestTokenBucket() { return mRequestTokenBucketEnabled; }
    uint16_t       RequestTokenBucketMinParallelism() { return mRequestTokenBucketMinParallelism; }
    uint32_t       RequestTokenBucketHz() { return mRequestTokenBucketHz; }
    uint32_t       RequestTokenBucketBurst() {return mRequestTokenBucketBurst; }

    bool           PromptTempRedirect()      { return mPromptTempRedirect; }

    // TCP Keepalive configuration values.

    // Returns true if TCP keepalive should be enabled for short-lived conns.
    bool TCPKeepaliveEnabledForShortLivedConns() {
      return mTCPKeepaliveShortLivedEnabled;
    }
    // Return time (secs) that a connection is consider short lived (for TCP
    // keepalive purposes). After this time, the connection is long-lived.
    int32_t GetTCPKeepaliveShortLivedTime() {
      return mTCPKeepaliveShortLivedTimeS;
    }
    // Returns time (secs) before first TCP keepalive probes should be sent;
    // same time used between successful keepalive probes.
    int32_t GetTCPKeepaliveShortLivedIdleTime() {
      return mTCPKeepaliveShortLivedIdleTimeS;
    }

    // Returns true if TCP keepalive should be enabled for long-lived conns.
    bool TCPKeepaliveEnabledForLongLivedConns() {
      return mTCPKeepaliveLongLivedEnabled;
    }
    // Returns time (secs) before first TCP keepalive probes should be sent;
    // same time used between successful keepalive probes.
    int32_t GetTCPKeepaliveLongLivedIdleTime() {
      return mTCPKeepaliveLongLivedIdleTimeS;
    }

    nsHttpAuthCache     *AuthCache(bool aPrivate) {
        return aPrivate ? &mPrivateAuthCache : &mAuthCache;
    }
    nsHttpConnectionMgr *ConnMgr()   { return mConnMgr; }

    // cache support
    bool UseCache() const { return mUseCache; }
    uint32_t GenerateUniqueID() { return ++mLastUniqueID; }
    uint32_t SessionStartTime() { return mSessionStartTime; }

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
    nsresult InitiateTransaction(nsHttpTransaction *trans, int32_t priority)
    {
        return mConnMgr->AddTransaction(trans, priority);
    }

    // Called to change the priority of an existing transaction that has
    // already been initiated.
    nsresult RescheduleTransaction(nsHttpTransaction *trans, int32_t priority)
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

    nsresult ProcessPendingQ()
    {
        return mConnMgr->ProcessPendingQ();
    }

    nsresult GetSocketThreadTarget(nsIEventTarget **target)
    {
        return mConnMgr->GetSocketThreadTarget(target);
    }

    nsresult SpeculativeConnect(nsHttpConnectionInfo *ci,
                                nsIInterfaceRequestor *callbacks,
                                uint32_t caps = 0)
    {
        TickleWifi(callbacks);
        return mConnMgr->SpeculativeConnect(ci, callbacks, caps);
    }

    //
    // The HTTP handler caches pointers to specific XPCOM services, and
    // provides the following helper routines for accessing those services:
    //
    nsresult GetStreamConverterService(nsIStreamConverterService **);
    nsresult GetIOService(nsIIOService** service);
    nsICookieService * GetCookieService(); // not addrefed
    nsISiteSecurityService * GetSSService();

    // callable from socket thread only
    uint32_t Get32BitsOfPseudoRandom();

    // Called by the channel synchronously during asyncOpen
    void OnOpeningRequest(nsIHttpChannel *chan)
    {
        NotifyObservers(chan, NS_HTTP_ON_OPENING_REQUEST_TOPIC);
    }

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
                               uint32_t flags);

    // Called by the channel when the response is read from the cache without
    // communicating with the server.
    void OnExamineCachedResponse(nsIHttpChannel *chan)
    {
        NotifyObservers(chan, NS_HTTP_ON_EXAMINE_CACHED_RESPONSE_TOPIC);
    }

    // Generates the host:port string for use in the Host: header as well as the
    // CONNECT line for proxies. This handles IPv6 literals correctly.
    static nsresult GenerateHostPort(const nsCString& host, int32_t port,
                                     nsACString& hostLine);

    bool GetPipelineAggressive()     { return mPipelineAggressive; }
    void GetMaxPipelineObjectSize(int64_t *outVal)
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

    SpdyInformation *SpdyInfo() { return &mSpdyInfo; }

    // returns true in between Init and Shutdown states
    bool Active() { return mHandlerActive; }

    static void GetCacheSessionNameForStoragePolicy(
            nsCacheStoragePolicy storagePolicy,
            bool isPrivate,
            uint32_t appId,
            bool inBrowser,
            nsACString& sessionName);

    // When the disk cache is responding slowly its use is suppressed
    // for 1 minute for most requests. Callable from main thread only.
    TimeStamp GetCacheSkippedUntil() { return mCacheSkippedUntil; }
    void SetCacheSkippedUntil(TimeStamp arg) { mCacheSkippedUntil = arg; }
    void ClearCacheSkippedUntil() { mCacheSkippedUntil = TimeStamp(); }

private:
    virtual ~nsHttpHandler();

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

    static void TimerCallback(nsITimer * aTimer, void * aClosure);
private:

    // cached services
    nsMainThreadPtrHandle<nsIIOService>              mIOService;
    nsMainThreadPtrHandle<nsIStreamConverterService> mStreamConvSvc;
    nsMainThreadPtrHandle<nsIObserverService>        mObserverService;
    nsMainThreadPtrHandle<nsICookieService>          mCookieService;
    nsMainThreadPtrHandle<nsISiteSecurityService>    mSSService;

    // the authentication credentials cache
    nsHttpAuthCache mAuthCache;
    nsHttpAuthCache mPrivateAuthCache;

    // the connection manager
    nsHttpConnectionMgr *mConnMgr;

    //
    // prefs
    //

    uint8_t  mHttpVersion;
    uint8_t  mProxyHttpVersion;
    uint32_t mCapabilities;
    uint8_t  mReferrerLevel;
    uint8_t  mSpoofReferrerSource;
    uint8_t  mReferrerTrimmingPolicy;
    uint8_t  mReferrerXOriginPolicy;

    bool mFastFallbackToIPv4;
    bool mProxyPipelining;
    PRIntervalTime mIdleTimeout;
    PRIntervalTime mSpdyTimeout;
    PRIntervalTime mResponseTimeout;
    bool mResponseTimeoutEnabled;

    uint16_t mMaxRequestAttempts;
    uint16_t mMaxRequestDelay;
    uint16_t mIdleSynTimeout;

    bool     mPipeliningEnabled;
    uint16_t mMaxConnections;
    uint8_t  mMaxPersistentConnectionsPerServer;
    uint8_t  mMaxPersistentConnectionsPerProxy;
    uint16_t mMaxPipelinedRequests;
    uint16_t mMaxOptimisticPipelinedRequests;
    bool     mPipelineAggressive;
    int64_t  mMaxPipelineObjectSize;
    bool     mPipelineRescheduleOnTimeout;
    PRIntervalTime mPipelineRescheduleTimeout;
    PRIntervalTime mPipelineReadTimeout;
    nsCOMPtr<nsITimer> mPipelineTestTimer;

    uint8_t  mRedirectionLimit;

    // we'll warn the user if we load an URL containing a userpass field
    // unless its length is less than this threshold.  this warning is
    // intended to protect the user against spoofing attempts that use
    // the userpass field of the URL to obscure the actual origin server.
    uint8_t  mPhishyUserPassLength;

    uint8_t  mQoSBits;

    bool mPipeliningOverSSL;
    bool mEnforceAssocReq;

    nsCString mAccept;
    nsCString mAcceptLanguages;
    nsCString mAcceptEncodings;

    nsXPIDLCString mDefaultSocketType;

    // cache support
    uint32_t                  mLastUniqueID;
    uint32_t                  mSessionStartTime;

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
    bool           mCompatFirefoxEnabled;
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

    // For broadcasting tracking preference
    bool           mDoNotTrackEnabled;
    uint8_t        mDoNotTrackValue;

    // for broadcasting safe hint;
    bool           mSafeHintEnabled;
    bool           mParentalControlEnabled;

    // Whether telemetry is reported or not
    uint32_t           mTelemetryEnabled : 1;

    // The value of network.allow-experiments
    uint32_t           mAllowExperiments : 1;

    // true in between init and shutdown states
    uint32_t           mHandlerActive : 1;

    uint32_t           mEnableSpdy : 1;
    uint32_t           mSpdyV3 : 1;
    uint32_t           mSpdyV31 : 1;
    uint32_t           mHttp2DraftEnabled : 1;
    uint32_t           mEnforceHttp2TlsProfile : 1;
    uint32_t           mCoalesceSpdy : 1;
    uint32_t           mSpdyPersistentSettings : 1;
    uint32_t           mAllowPush : 1;

    // Try to use SPDY features instead of HTTP/1.1 over SSL
    SpdyInformation    mSpdyInfo;

    uint32_t       mSpdySendingChunkSize;
    uint32_t       mSpdySendBufferSize;
    uint32_t       mSpdyPushAllowance;
    PRIntervalTime mSpdyPingThreshold;
    PRIntervalTime mSpdyPingTimeout;

    // The maximum amount of time to wait for socket transport to be
    // established. In milliseconds.
    uint32_t       mConnectTimeout;

    // The maximum amount of time the nsICacheSession lock can be held
    // before a new transaction bypasses the cache. In milliseconds.
    double         mBypassCacheLockThreshold;

    // The maximum number of current global half open sockets allowable
    // when starting a new speculative connection.
    uint32_t       mParallelSpeculativeConnectLimit;

    // For Rate Pacing of HTTP/1 requests through a netwerk/base/src/EventTokenBucket
    // Active requests <= *MinParallelism are not subject to the rate pacing
    bool           mRequestTokenBucketEnabled;
    uint16_t       mRequestTokenBucketMinParallelism;
    uint32_t       mRequestTokenBucketHz;  // EventTokenBucket HZ
    uint32_t       mRequestTokenBucketBurst; // EventTokenBucket Burst

    // Whether or not to block requests for non head js/css items (e.g. media)
    // while those elements load.
    bool           mCriticalRequestPrioritization;

    // When the disk cache is responding slowly its use is suppressed
    // for 1 minute for most requests.
    TimeStamp      mCacheSkippedUntil;

    // TCP Keepalive configuration values.

    // True if TCP keepalive is enabled for short-lived conns.
    bool mTCPKeepaliveShortLivedEnabled;
    // Time (secs) indicating how long a conn is considered short-lived.
    int32_t mTCPKeepaliveShortLivedTimeS;
    // Time (secs) before first keepalive probe; between successful probes.
    int32_t mTCPKeepaliveShortLivedIdleTimeS;

    // True if TCP keepalive is enabled for long-lived conns.
    bool mTCPKeepaliveLongLivedEnabled;
    // Time (secs) before first keepalive probe; between successful probes.
    int32_t mTCPKeepaliveLongLivedIdleTimeS;

private:
    // For Rate Pacing Certain Network Events. Only assign this pointer on
    // socket thread.
    void MakeNewRequestTokenBucket();
    nsRefPtr<EventTokenBucket> mRequestTokenBucket;

public:
    // Socket thread only
    nsresult SubmitPacedRequest(ATokenBucketEvent *event,
                                nsICancelable **cancel)
    {
        if (!mRequestTokenBucket)
            return NS_ERROR_UNEXPECTED;
        return mRequestTokenBucket->SubmitEvent(event, cancel);
    }

    // Socket thread only
    void SetRequestTokenBucket(EventTokenBucket *aTokenBucket)
    {
        mRequestTokenBucket = aTokenBucket;
    }

private:
    nsRefPtr<Tickler> mWifiTickler;
    void TickleWifi(nsIInterfaceRequestor *cb);
};

extern nsHttpHandler *gHttpHandler;

//-----------------------------------------------------------------------------
// nsHttpsHandler - thin wrapper to distinguish the HTTP handler from the
//                  HTTPS handler (even though they share the same impl).
//-----------------------------------------------------------------------------

class nsHttpsHandler : public nsIHttpProtocolHandler
                     , public nsSupportsWeakReference
                     , public nsISpeculativeConnect
{
    virtual ~nsHttpsHandler() { }
public:
    // we basically just want to override GetScheme and GetDefaultPort...
    // all other methods should be forwarded to the nsHttpHandler instance.

    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
    NS_FORWARD_NSIPROXIEDPROTOCOLHANDLER (gHttpHandler->)
    NS_FORWARD_NSIHTTPPROTOCOLHANDLER    (gHttpHandler->)
    NS_FORWARD_NSISPECULATIVECONNECT     (gHttpHandler->)

    nsHttpsHandler() { }

    nsresult Init();
};

}} // namespace mozilla::net

#endif // nsHttpHandler_h__
