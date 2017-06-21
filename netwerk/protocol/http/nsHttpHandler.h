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

class nsIHttpChannel;
class nsIPrefBranch;
class nsICancelable;
class nsICookieService;
class nsIIOService;
class nsIRequestContextService;
class nsISiteSecurityService;
class nsIStreamConverterService;


namespace mozilla {
namespace net {

bool OnSocketThread();

class ATokenBucketEvent;
class EventTokenBucket;
class Tickler;
class nsHttpConnection;
class nsHttpConnectionInfo;
class nsHttpTransaction;
class AltSvcMapping;

enum FrameCheckLevel {
    FRAMECHECK_LAX,
    FRAMECHECK_BARELY,
    FRAMECHECK_STRICT
};

//-----------------------------------------------------------------------------
// nsHttpHandler - protocol handler for HTTP and HTTPS
//-----------------------------------------------------------------------------

class nsHttpHandler final : public nsIHttpProtocolHandler
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

    MOZ_MUST_USE nsresult Init();
    MOZ_MUST_USE nsresult AddStandardRequestHeaders(nsHttpRequestHead *,
                                                    bool isSecure);
    MOZ_MUST_USE nsresult AddConnectionHeader(nsHttpRequestHead *,
                                              uint32_t capabilities);
    bool     IsAcceptableEncoding(const char *encoding, bool isSecure);

    const nsAFlatCString &UserAgent();

    nsHttpVersion  HttpVersion()             { return mHttpVersion; }
    nsHttpVersion  ProxyHttpVersion()        { return mProxyHttpVersion; }
    uint8_t        ReferrerLevel()           { return mReferrerLevel; }
    bool           SpoofReferrerSource()     { return mSpoofReferrerSource; }
    bool           HideOnionReferrerSource() { return mHideOnionReferrerSource; }
    uint8_t        ReferrerTrimmingPolicy()  { return mReferrerTrimmingPolicy; }
    uint8_t        ReferrerXOriginTrimmingPolicy() {
        return mReferrerXOriginTrimmingPolicy;
    }
    uint8_t        ReferrerXOriginPolicy()   { return mReferrerXOriginPolicy; }
    uint8_t        RedirectionLimit()        { return mRedirectionLimit; }
    PRIntervalTime IdleTimeout()             { return mIdleTimeout; }
    PRIntervalTime SpdyTimeout()             { return mSpdyTimeout; }
    PRIntervalTime ResponseTimeout() {
      return mResponseTimeoutEnabled ? mResponseTimeout : 0;
    }
    PRIntervalTime ResponseTimeoutEnabled()  { return mResponseTimeoutEnabled; }
    uint32_t       NetworkChangedTimeout()   { return mNetworkChangedTimeout; }
    uint16_t       MaxRequestAttempts()      { return mMaxRequestAttempts; }
    const char    *DefaultSocketType()       { return mDefaultSocketType.get(); /* ok to return null */ }
    uint32_t       PhishyUserPassLength()    { return mPhishyUserPassLength; }
    uint8_t        GetQoSBits()              { return mQoSBits; }
    uint16_t       GetIdleSynTimeout()       { return mIdleSynTimeout; }
    bool           FastFallbackToIPv4()      { return mFastFallbackToIPv4; }
    uint32_t       MaxSocketCount();
    bool           EnforceAssocReq()         { return mEnforceAssocReq; }

    bool           IsPersistentHttpsCachingEnabled() { return mEnablePersistentHttpsCaching; }
    bool           IsTelemetryEnabled() { return mTelemetryEnabled; }
    bool           AllowExperiments() { return mTelemetryEnabled && mAllowExperiments; }

    bool           IsSpdyEnabled() { return mEnableSpdy; }
    bool           IsHttp2Enabled() { return mHttp2Enabled; }
    bool           EnforceHttp2TlsProfile() { return mEnforceHttp2TlsProfile; }
    bool           CoalesceSpdy() { return mCoalesceSpdy; }
    bool           UseSpdyPersistentSettings() { return mSpdyPersistentSettings; }
    uint32_t       SpdySendingChunkSize() { return mSpdySendingChunkSize; }
    uint32_t       SpdySendBufferSize()      { return mSpdySendBufferSize; }
    uint32_t       SpdyPushAllowance()       { return mSpdyPushAllowance; }
    uint32_t       SpdyPullAllowance()       { return mSpdyPullAllowance; }
    uint32_t       DefaultSpdyConcurrent()   { return mDefaultSpdyConcurrent; }
    PRIntervalTime SpdyPingThreshold() { return mSpdyPingThreshold; }
    PRIntervalTime SpdyPingTimeout() { return mSpdyPingTimeout; }
    bool           AllowPush()   { return mAllowPush; }
    bool           AllowAltSvc() { return mEnableAltSvc; }
    bool           AllowAltSvcOE() { return mEnableAltSvcOE; }
    bool           AllowOriginExtension() { return mEnableOriginExtension; }
    uint32_t       ConnectTimeout()  { return mConnectTimeout; }
    uint32_t       ParallelSpeculativeConnectLimit() { return mParallelSpeculativeConnectLimit; }
    bool           CriticalRequestPrioritization() { return mCriticalRequestPrioritization; }
    bool           UseH2Deps() { return mUseH2Deps; }

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

    bool UseFastOpen()
    {
        return mUseFastOpen && mFastOpenSupported &&
               mFastOpenConsecutiveFailureCounter < mFastOpenConsecutiveFailureLimit;
    }
    // If one of tcp connections return PR_NOT_TCP_SOCKET_ERROR while trying
    // fast open, it means that Fast Open is turned off so we will not try again
    // until a restart. This is only on Linux.
    // For windows 10 we can only check whether a version of windows support
    // Fast Open at run time, so if we get error PR_NOT_IMPLEMENTED_ERROR it
    // means that Fast Open is not supported and we will set mFastOpenSupported
    // to false.
    void SetFastOpenNotSupported() { mFastOpenSupported = false; }

    void IncrementFastOpenConsecutiveFailureCounter();

    void ResetFastOpenConsecutiveFailureCounter()
    {
        mFastOpenConsecutiveFailureCounter = 0;
    }

    // returns the HTTP framing check level preference, as controlled with
    // network.http.enforce-framing.http1 and network.http.enforce-framing.soft
    FrameCheckLevel GetEnforceH1Framing() { return mEnforceH1Framing; }

    nsHttpAuthCache     *AuthCache(bool aPrivate) {
        return aPrivate ? &mPrivateAuthCache : &mAuthCache;
    }
    nsHttpConnectionMgr *ConnMgr()   { return mConnMgr; }

    // cache support
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
    MOZ_MUST_USE nsresult InitiateTransaction(nsHttpTransaction *trans,
                                              int32_t priority)
    {
        return mConnMgr->AddTransaction(trans, priority);
    }

    // Called to change the priority of an existing transaction that has
    // already been initiated.
    MOZ_MUST_USE nsresult RescheduleTransaction(nsHttpTransaction *trans,
                                                int32_t priority)
    {
        return mConnMgr->RescheduleTransaction(trans, priority);
    }

    void UpdateClassOfServiceOnTransaction(nsHttpTransaction *trans,
                                           uint32_t classOfService)
    {
        mConnMgr->UpdateClassOfServiceOnTransaction(trans, classOfService);
    }

    // Called to cancel a transaction, which may or may not be assigned to
    // a connection.  Callable from any thread.
    MOZ_MUST_USE nsresult CancelTransaction(nsHttpTransaction *trans,
                                            nsresult reason)
    {
        return mConnMgr->CancelTransaction(trans, reason);
    }

    // Called when a connection is done processing a transaction.  Callable
    // from any thread.
    MOZ_MUST_USE nsresult ReclaimConnection(nsHttpConnection *conn)
    {
        return mConnMgr->ReclaimConnection(conn);
    }

    MOZ_MUST_USE nsresult ProcessPendingQ(nsHttpConnectionInfo *cinfo)
    {
        return mConnMgr->ProcessPendingQ(cinfo);
    }

    MOZ_MUST_USE nsresult ProcessPendingQ()
    {
        return mConnMgr->ProcessPendingQ();
    }

    MOZ_MUST_USE nsresult GetSocketThreadTarget(nsIEventTarget **target)
    {
        return mConnMgr->GetSocketThreadTarget(target);
    }

    MOZ_MUST_USE nsresult SpeculativeConnect(nsHttpConnectionInfo *ci,
                                             nsIInterfaceRequestor *callbacks,
                                             uint32_t caps = 0)
    {
        TickleWifi(callbacks);
        return mConnMgr->SpeculativeConnect(ci, callbacks, caps);
    }

    // Alternate Services Maps are main thread only
    void UpdateAltServiceMapping(AltSvcMapping *map,
                                 nsProxyInfo *proxyInfo,
                                 nsIInterfaceRequestor *callbacks,
                                 uint32_t caps,
                                 const OriginAttributes &originAttributes)
    {
        mConnMgr->UpdateAltServiceMapping(map, proxyInfo, callbacks, caps,
                                          originAttributes);
    }

    already_AddRefed<AltSvcMapping> GetAltServiceMapping(const nsACString &scheme,
                                                         const nsACString &host,
                                                         int32_t port, bool pb,
                                                         const OriginAttributes &originAttributes)
    {
        return mConnMgr->GetAltServiceMapping(scheme, host, port, pb, originAttributes);
    }

    //
    // The HTTP handler caches pointers to specific XPCOM services, and
    // provides the following helper routines for accessing those services:
    //
    MOZ_MUST_USE nsresult GetStreamConverterService(nsIStreamConverterService **);
    MOZ_MUST_USE nsresult GetIOService(nsIIOService** service);
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

    // Called by the channel and cached in the loadGroup
    void OnUserAgentRequest(nsIHttpChannel *chan)
    {
      NotifyObservers(chan, NS_HTTP_ON_USERAGENT_REQUEST_TOPIC);
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
    MOZ_MUST_USE nsresult AsyncOnChannelRedirect(nsIChannel* oldChan,
                                                 nsIChannel* newChan,
                                                 uint32_t flags);

    // Called by the channel when the response is read from the cache without
    // communicating with the server.
    void OnExamineCachedResponse(nsIHttpChannel *chan)
    {
        NotifyObservers(chan, NS_HTTP_ON_EXAMINE_CACHED_RESPONSE_TOPIC);
    }

    // Generates the host:port string for use in the Host: header as well as the
    // CONNECT line for proxies. This handles IPv6 literals correctly.
    static MOZ_MUST_USE nsresult GenerateHostPort(const nsCString& host,
                                                  int32_t port,
                                                  nsACString& hostLine);


    SpdyInformation *SpdyInfo() { return &mSpdyInfo; }
    bool IsH2MandatorySuiteEnabled() { return mH2MandatorySuiteEnabled; }

    // returns true in between Init and Shutdown states
    bool Active() { return mHandlerActive; }

    nsIRequestContextService *GetRequestContextService()
    {
        return mRequestContextService.get();
    }

    void ShutdownConnectionManager();

    bool KeepEmptyResponseHeadersAsEmtpyString() const
    {
        return mKeepEmptyResponseHeadersAsEmtpyString;
    }

    uint32_t DefaultHpackBuffer() const
    {
        return mDefaultHpackBuffer;
    }

    uint32_t MaxHttpResponseHeaderSize() const
    {
        return mMaxHttpResponseHeaderSize;
    }

    float FocusedWindowTransactionRatio() const
    {
        return mFocusedWindowTransactionRatio;
    }

private:
    virtual ~nsHttpHandler();

    //
    // Useragent/prefs helper methods
    //
    void     BuildUserAgent();
    void     InitUserAgentComponents();
    void     PrefsChanged(nsIPrefBranch *prefs, const char *pref);

    MOZ_MUST_USE nsresult SetAccept(const char *);
    MOZ_MUST_USE nsresult SetAcceptLanguages();
    MOZ_MUST_USE nsresult SetAcceptEncodings(const char *, bool mIsSecure);

    MOZ_MUST_USE nsresult InitConnectionMgr();

    void     NotifyObservers(nsIHttpChannel *chan, const char *event);

    void SetFastOpenOSSupport();

    void EnsureUAOverridesInit();
private:

    // cached services
    nsMainThreadPtrHandle<nsIIOService>              mIOService;
    nsMainThreadPtrHandle<nsIStreamConverterService> mStreamConvSvc;
    nsMainThreadPtrHandle<nsICookieService>          mCookieService;
    nsMainThreadPtrHandle<nsISiteSecurityService>    mSSService;

    // the authentication credentials cache
    nsHttpAuthCache mAuthCache;
    nsHttpAuthCache mPrivateAuthCache;

    // the connection manager
    RefPtr<nsHttpConnectionMgr> mConnMgr;

    //
    // prefs
    //

    uint8_t  mHttpVersion;
    uint8_t  mProxyHttpVersion;
    uint32_t mCapabilities;
    uint8_t  mReferrerLevel;
    uint8_t  mSpoofReferrerSource;
    uint8_t  mHideOnionReferrerSource;
    uint8_t  mReferrerTrimmingPolicy;
    uint8_t  mReferrerXOriginTrimmingPolicy;
    uint8_t  mReferrerXOriginPolicy;

    bool mFastFallbackToIPv4;
    PRIntervalTime mIdleTimeout;
    PRIntervalTime mSpdyTimeout;
    PRIntervalTime mResponseTimeout;
    bool mResponseTimeoutEnabled;
    uint32_t mNetworkChangedTimeout; // milliseconds
    uint16_t mMaxRequestAttempts;
    uint16_t mMaxRequestDelay;
    uint16_t mIdleSynTimeout;

    bool     mH2MandatorySuiteEnabled;
    uint16_t mMaxUrgentExcessiveConns;
    uint16_t mMaxConnections;
    uint8_t  mMaxPersistentConnectionsPerServer;
    uint8_t  mMaxPersistentConnectionsPerProxy;

    bool mThrottleEnabled;
    uint32_t mThrottleSuspendFor;
    uint32_t mThrottleResumeFor;
    uint32_t mThrottleResumeIn;

    uint8_t  mRedirectionLimit;

    // we'll warn the user if we load an URL containing a userpass field
    // unless its length is less than this threshold.  this warning is
    // intended to protect the user against spoofing attempts that use
    // the userpass field of the URL to obscure the actual origin server.
    uint8_t  mPhishyUserPassLength;

    uint8_t  mQoSBits;

    bool mEnforceAssocReq;

    nsCString mAccept;
    nsCString mAcceptLanguages;
    nsCString mHttpAcceptEncodings;
    nsCString mHttpsAcceptEncodings;

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
    nsCString      mDeviceModelId;

    nsCString      mUserAgent;
    nsCString      mSpoofedUserAgent;
    nsXPIDLCString mUserAgentOverride;
    bool           mUserAgentIsDirty; // true if mUserAgent should be rebuilt
    bool           mAcceptLanguagesIsDirty;


    bool           mPromptTempRedirect;

    // Persistent HTTPS caching flag
    bool           mEnablePersistentHttpsCaching;

    // For broadcasting tracking preference
    bool           mDoNotTrackEnabled;

    // for broadcasting safe hint;
    bool           mSafeHintEnabled;
    bool           mParentalControlEnabled;

    // true in between init and shutdown states
    Atomic<bool, Relaxed> mHandlerActive;

    // Whether telemetry is reported or not
    uint32_t           mTelemetryEnabled : 1;

    // The value of network.allow-experiments
    uint32_t           mAllowExperiments : 1;

    // The value of 'hidden' network.http.debug-observations : 1;
    uint32_t           mDebugObservations : 1;

    uint32_t           mEnableSpdy : 1;
    uint32_t           mHttp2Enabled : 1;
    uint32_t           mUseH2Deps : 1;
    uint32_t           mEnforceHttp2TlsProfile : 1;
    uint32_t           mCoalesceSpdy : 1;
    uint32_t           mSpdyPersistentSettings : 1;
    uint32_t           mAllowPush : 1;
    uint32_t           mEnableAltSvc : 1;
    uint32_t           mEnableAltSvcOE : 1;
    uint32_t           mEnableOriginExtension : 1;

    // Try to use SPDY features instead of HTTP/1.1 over SSL
    SpdyInformation    mSpdyInfo;

    uint32_t       mSpdySendingChunkSize;
    uint32_t       mSpdySendBufferSize;
    uint32_t       mSpdyPushAllowance;
    uint32_t       mSpdyPullAllowance;
    uint32_t       mDefaultSpdyConcurrent;
    PRIntervalTime mSpdyPingThreshold;
    PRIntervalTime mSpdyPingTimeout;

    // The maximum amount of time to wait for socket transport to be
    // established. In milliseconds.
    uint32_t       mConnectTimeout;

    // The maximum number of current global half open sockets allowable
    // when starting a new speculative connection.
    uint32_t       mParallelSpeculativeConnectLimit;

    // For Rate Pacing of HTTP/1 requests through a netwerk/base/EventTokenBucket
    // Active requests <= *MinParallelism are not subject to the rate pacing
    bool           mRequestTokenBucketEnabled;
    uint16_t       mRequestTokenBucketMinParallelism;
    uint32_t       mRequestTokenBucketHz;  // EventTokenBucket HZ
    uint32_t       mRequestTokenBucketBurst; // EventTokenBucket Burst

    // Whether or not to block requests for non head js/css items (e.g. media)
    // while those elements load.
    bool           mCriticalRequestPrioritization;

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

    // if true, generate NS_ERROR_PARTIAL_TRANSFER for h1 responses with
    // incorrect content lengths or malformed chunked encodings
    FrameCheckLevel mEnforceH1Framing;

    nsCOMPtr<nsIRequestContextService> mRequestContextService;

    // If it is set to false, headers with empty value will not appear in the
    // header array - behavior as it used to be. If it is true: empty headers
    // coming from the network will exits in header array as empty string.
    // Call SetHeader with an empty value will still delete the header.
    // (Bug 6699259)
    bool mKeepEmptyResponseHeadersAsEmtpyString;

    // The default size (in bytes) of the HPACK decompressor table.
    uint32_t mDefaultHpackBuffer;

    // The max size (in bytes) for received Http response header.
    uint32_t mMaxHttpResponseHeaderSize;

    // The ratio for dispatching transactions from the focused window.
    float mFocusedWindowTransactionRatio;

    Atomic<bool, Relaxed> mUseFastOpen;
    Atomic<bool, Relaxed> mFastOpenSupported;
    uint32_t mFastOpenConsecutiveFailureLimit;
    uint32_t mFastOpenConsecutiveFailureCounter;

private:
    // For Rate Pacing Certain Network Events. Only assign this pointer on
    // socket thread.
    void MakeNewRequestTokenBucket();
    RefPtr<EventTokenBucket> mRequestTokenBucket;

public:
    // Socket thread only
    MOZ_MUST_USE nsresult SubmitPacedRequest(ATokenBucketEvent *event,
                                             nsICancelable **cancel)
    {
        MOZ_ASSERT(OnSocketThread(), "not on socket thread");
        if (!mRequestTokenBucket) {
            return NS_ERROR_NOT_AVAILABLE;
        }
        return mRequestTokenBucket->SubmitEvent(event, cancel);
    }

    // Socket thread only
    void SetRequestTokenBucket(EventTokenBucket *aTokenBucket)
    {
        MOZ_ASSERT(OnSocketThread(), "not on socket thread");
        mRequestTokenBucket = aTokenBucket;
    }

    void StopRequestTokenBucket()
    {
        MOZ_ASSERT(OnSocketThread(), "not on socket thread");
        if (mRequestTokenBucket) {
            mRequestTokenBucket->Stop();
            mRequestTokenBucket = nullptr;
        }
    }

private:
    RefPtr<Tickler> mWifiTickler;
    void TickleWifi(nsIInterfaceRequestor *cb);

private:
    MOZ_MUST_USE nsresult
    SpeculativeConnectInternal(nsIURI *aURI,
                               nsIPrincipal *aPrincipal,
                               nsIInterfaceRequestor *aCallbacks,
                               bool anonymous);

    // State for generating channelIds
    uint32_t mProcessId;
    uint32_t mNextChannelId;

public:
    MOZ_MUST_USE nsresult NewChannelId(uint64_t& channelId);
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

    MOZ_MUST_USE nsresult Init();
};

} // namespace net
} // namespace mozilla

#endif // nsHttpHandler_h__
