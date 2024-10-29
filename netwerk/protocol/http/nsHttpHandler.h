/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpHandler_h__
#define nsHttpHandler_h__

#include <functional>

#include "nsHttp.h"
#include "nsHttpAuthCache.h"
#include "nsHttpConnectionMgr.h"
#include "AlternateServices.h"
#include "ASpdySession.h"
#include "HttpTrafficAnalyzer.h"

#include "mozilla/Mutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TimeStamp.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"

#include "nsIHttpProtocolHandler.h"
#include "nsIObserver.h"
#include "nsISpeculativeConnect.h"
#include "nsTHashMap.h"
#include "nsTHashSet.h"

#ifdef DEBUG
#  include "nsIOService.h"
#endif

// XXX These includes can be replaced by forward declarations by moving the On*
// method implementations to the cpp file
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsSocketTransportService2.h"

class nsIHttpActivityDistributor;
class nsIHttpUpgradeListener;
class nsIPrefBranch;
class nsICancelable;
class nsICookieService;
class nsIIOService;
class nsIRequestContextService;
class nsISiteSecurityService;
class nsIStreamConverterService;

namespace mozilla::net {

class ATokenBucketEvent;
class EventTokenBucket;
class Tickler;
class nsHttpConnection;
class nsHttpConnectionInfo;
class HttpBaseChannel;
class HttpHandlerInitArgs;
class HttpTransactionShell;
class AltSvcMapping;
class DNSUtils;
class TRRServiceChannel;
class SocketProcessChild;

/*
 * FRAMECHECK_LAX - no check
 * FRAMECHECK_BARELY - allows:
 *                     1) that chunk-encoding does not have the last 0-size
 *                     chunk. So, if a chunked-encoded transfer ends on exactly
 *                     a chunk boundary we consider that fine. This will allows
 *                     us to accept buggy servers that do not send the last
 *                     chunk. It will make us not detect a certain amount of
 *                     cut-offs.
 *                     2) When receiving a gzipped response, we consider a
 *                     gzip stream that doesn't end fine according to the gzip
 *                     decompressing state machine to be a partial transfer.
 *                     If a gzipped transfer ends fine according to the
 *                     decompressor, we do not check for size unalignments.
 *                     This allows to allow HTTP gzipped responses where the
 *                     Content-Length is not the same as the actual contents.
 *                     3) When receiving HTTP that isn't
 *                     content-encoded/compressed (like in case 2) and not
 *                     chunked (like in case 1), perform the size comparison
 *                     between Content-Length: and the actual size received
 *                     and consider a mismatch to mean a
 *                     NS_ERROR_NET_PARTIAL_TRANSFER error.
 * FRAMECHECK_STRICT_CHUNKED - This is the same as FRAMECHECK_BARELY only we
 *                             enforce that the last 0-size chunk is received
 *                             in case 1).
 * FRAMECHECK_STRICT - we also do not allow case 2) and 3) from
 *                     FRAMECHECK_BARELY.
 */
enum FrameCheckLevel {
  FRAMECHECK_LAX,
  FRAMECHECK_BARELY,
  FRAMECHECK_STRICT_CHUNKED,
  FRAMECHECK_STRICT
};

//-----------------------------------------------------------------------------
// nsHttpHandler - protocol handler for HTTP and HTTPS
//-----------------------------------------------------------------------------

class nsHttpHandler final : public nsIHttpProtocolHandler,
                            public nsIObserver,
                            public nsSupportsWeakReference,
                            public nsISpeculativeConnect {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER
  NS_DECL_NSIPROXIEDPROTOCOLHANDLER
  NS_DECL_NSIHTTPPROTOCOLHANDLER
  NS_DECL_NSIOBSERVER
  NS_DECL_NSISPECULATIVECONNECT

  static already_AddRefed<nsHttpHandler> GetInstance();

  [[nodiscard]] nsresult AddStandardRequestHeaders(
      nsHttpRequestHead*, bool isSecure,
      ExtContentPolicyType aContentPolicyType,
      bool aShouldResistFingerprinting);
  [[nodiscard]] nsresult AddConnectionHeader(nsHttpRequestHead*, uint32_t caps);
  bool IsAcceptableEncoding(const char* encoding, bool isSecure);

  const nsCString& UserAgent(bool aShouldResistFingerprinting);

  enum HttpVersion HttpVersion() { return mHttpVersion; }
  enum HttpVersion ProxyHttpVersion() { return mProxyHttpVersion; }
  uint8_t RedirectionLimit() { return mRedirectionLimit; }
  PRIntervalTime IdleTimeout() { return mIdleTimeout; }
  PRIntervalTime SpdyTimeout() { return mSpdyTimeout; }
  PRIntervalTime ResponseTimeout() {
    return mResponseTimeoutEnabled ? mResponseTimeout : 0;
  }
  PRIntervalTime ResponseTimeoutEnabled() { return mResponseTimeoutEnabled; }
  uint32_t NetworkChangedTimeout() { return mNetworkChangedTimeout; }
  uint16_t MaxRequestAttempts() { return mMaxRequestAttempts; }
  const nsCString& DefaultSocketType() { return mDefaultSocketType; }
  uint32_t PhishyUserPassLength() { return mPhishyUserPassLength; }
  uint8_t GetQoSBits() { return mQoSBits; }
  uint16_t GetIdleSynTimeout() { return mIdleSynTimeout; }
  uint16_t GetFallbackSynTimeout() { return mFallbackSynTimeout; }
  bool FastFallbackToIPv4() { return mFastFallbackToIPv4; }
  uint32_t MaxSocketCount();
  bool EnforceAssocReq() { return mEnforceAssocReq; }

  bool IsPersistentHttpsCachingEnabled() {
    return mEnablePersistentHttpsCaching;
  }

  uint32_t SpdySendingChunkSize() { return mSpdySendingChunkSize; }
  uint32_t SpdySendBufferSize() { return mSpdySendBufferSize; }
  uint32_t SpdyPushAllowance() { return mSpdyPushAllowance; }
  uint32_t SpdyPullAllowance() { return mSpdyPullAllowance; }
  uint32_t DefaultSpdyConcurrent() { return mDefaultSpdyConcurrent; }
  PRIntervalTime SpdyPingThreshold() { return mSpdyPingThreshold; }
  PRIntervalTime SpdyPingTimeout() { return mSpdyPingTimeout; }
  bool AllowAltSvc() { return mEnableAltSvc; }
  bool AllowAltSvcOE() { return mEnableAltSvcOE; }
  uint32_t ConnectTimeout() { return mConnectTimeout; }
  uint32_t TLSHandshakeTimeout() { return mTLSHandshakeTimeout; }
  uint32_t ParallelSpeculativeConnectLimit() {
    return mParallelSpeculativeConnectLimit;
  }
  bool CriticalRequestPrioritization() {
    return mCriticalRequestPrioritization;
  }

  uint32_t MaxConnectionsPerOrigin() {
    return mMaxPersistentConnectionsPerServer;
  }
  bool UseRequestTokenBucket() { return mRequestTokenBucketEnabled; }
  uint16_t RequestTokenBucketMinParallelism() {
    return mRequestTokenBucketMinParallelism;
  }
  uint32_t RequestTokenBucketHz() { return mRequestTokenBucketHz; }
  uint32_t RequestTokenBucketBurst() { return mRequestTokenBucketBurst; }

  bool PromptTempRedirect() { return mPromptTempRedirect; }
  bool IsUrgentStartEnabled() { return mUrgentStartEnabled; }
  bool IsTailBlockingEnabled() { return mTailBlockingEnabled; }
  uint32_t TailBlockingDelayQuantum(bool aAfterDOMContentLoaded) {
    return aAfterDOMContentLoaded ? mTailDelayQuantumAfterDCL
                                  : mTailDelayQuantum;
  }
  uint32_t TailBlockingDelayMax() { return mTailDelayMax; }
  uint32_t TailBlockingTotalMax() { return mTailTotalMax; }

  uint32_t ThrottlingReadLimit() {
    return mThrottleVersion == 1 ? 0 : mThrottleReadLimit;
  }
  int32_t SendWindowSize() { return mSendWindowSize * 1024; }

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

  // returns the HTTP framing check level preference, as controlled with
  // network.http.enforce-framing.http1 and network.http.enforce-framing.soft
  FrameCheckLevel GetEnforceH1Framing() { return mEnforceH1Framing; }

  nsHttpAuthCache* AuthCache(bool aPrivate) {
    return aPrivate ? &mPrivateAuthCache : &mAuthCache;
  }
  nsHttpConnectionMgr* ConnMgr() {
    MOZ_ASSERT_IF(nsIOService::UseSocketProcess(), XRE_IsSocketProcess());
    return mConnMgr->AsHttpConnectionMgr();
  }

  AltSvcCache* AltServiceCache() const {
    MOZ_ASSERT(XRE_IsParentProcess());
    return mAltSvcCache.get();
  }

  void ClearHostMapping(nsHttpConnectionInfo* aConnInfo);

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
  [[nodiscard]] nsresult InitiateTransaction(HttpTransactionShell* trans,
                                             int32_t priority);

  // This function is also called to kick-off a new transaction. But the new
  // transaction will take a sticky connection from |transWithStickyConn|
  // and reuse it.
  [[nodiscard]] nsresult InitiateTransactionWithStickyConn(
      HttpTransactionShell* trans, int32_t priority,
      HttpTransactionShell* transWithStickyConn);

  // Called to change the priority of an existing transaction that has
  // already been initiated.
  [[nodiscard]] nsresult RescheduleTransaction(HttpTransactionShell* trans,
                                               int32_t priority);

  void UpdateClassOfServiceOnTransaction(HttpTransactionShell* trans,
                                         const ClassOfService& classOfService);

  // Called to cancel a transaction, which may or may not be assigned to
  // a connection.  Callable from any thread.
  [[nodiscard]] nsresult CancelTransaction(HttpTransactionShell* trans,
                                           nsresult reason);

  // Called when a connection is done processing a transaction.  Callable
  // from any thread.
  [[nodiscard]] nsresult ReclaimConnection(HttpConnectionBase* conn) {
    return mConnMgr->ReclaimConnection(conn);
  }

  [[nodiscard]] nsresult ProcessPendingQ(nsHttpConnectionInfo* cinfo) {
    return mConnMgr->ProcessPendingQ(cinfo);
  }

  [[nodiscard]] nsresult ProcessPendingQ() {
    return mConnMgr->ProcessPendingQ();
  }

  [[nodiscard]] nsresult GetSocketThreadTarget(nsIEventTarget** target) {
    return mConnMgr->GetSocketThreadTarget(target);
  }

  [[nodiscard]] nsresult SpeculativeConnect(nsHttpConnectionInfo* ci,
                                            nsIInterfaceRequestor* callbacks,
                                            uint32_t caps = 0,
                                            bool aFetchHTTPSRR = false) {
    TickleWifi(callbacks);
    RefPtr<nsHttpConnectionInfo> clone = ci->Clone();
    return mConnMgr->SpeculativeConnect(clone, callbacks, caps, nullptr,
                                        aFetchHTTPSRR | EchConfigEnabled());
  }

  [[nodiscard]] nsresult SpeculativeConnect(nsHttpConnectionInfo* ci,
                                            nsIInterfaceRequestor* callbacks,
                                            uint32_t caps,
                                            SpeculativeTransaction* aTrans) {
    RefPtr<nsHttpConnectionInfo> clone = ci->Clone();
    return mConnMgr->SpeculativeConnect(clone, callbacks, caps, aTrans);
  }

  // Alternate Services Maps are main thread only
  void UpdateAltServiceMapping(AltSvcMapping* map, nsProxyInfo* proxyInfo,
                               nsIInterfaceRequestor* callbacks, uint32_t caps,
                               const OriginAttributes& originAttributes) {
    mAltSvcCache->UpdateAltServiceMapping(map, proxyInfo, callbacks, caps,
                                          originAttributes);
  }

  void UpdateAltServiceMappingWithoutValidation(
      AltSvcMapping* map, nsProxyInfo* proxyInfo,
      nsIInterfaceRequestor* callbacks, uint32_t caps,
      const OriginAttributes& originAttributes) {
    mAltSvcCache->UpdateAltServiceMappingWithoutValidation(
        map, proxyInfo, callbacks, caps, originAttributes);
  }

  already_AddRefed<AltSvcMapping> GetAltServiceMapping(
      const nsACString& scheme, const nsACString& host, int32_t port, bool pb,
      const OriginAttributes& originAttributes, bool aHttp2Allowed,
      bool aHttp3Allowed) {
    return mAltSvcCache->GetAltServiceMapping(
        scheme, host, port, pb, originAttributes, aHttp2Allowed, aHttp3Allowed);
  }

  //
  // The HTTP handler caches pointers to specific XPCOM services, and
  // provides the following helper routines for accessing those services:
  //
  [[nodiscard]] nsresult GetIOService(nsIIOService** result);
  nsICookieService* GetCookieService();  // not addrefed
  nsISiteSecurityService* GetSSService();

  // Called by the channel synchronously during asyncOpen
  void OnFailedOpeningRequest(nsIHttpChannel* chan) {
    NotifyObservers(chan, NS_HTTP_ON_FAILED_OPENING_REQUEST_TOPIC);
  }

  // Called by the channel synchronously during asyncOpen
  void OnOpeningRequest(nsIHttpChannel* chan) {
    NotifyObservers(chan, NS_HTTP_ON_OPENING_REQUEST_TOPIC);
  }

  void OnOpeningDocumentRequest(nsIIdentChannel* chan) {
    NotifyObservers(chan, NS_DOCUMENT_ON_OPENING_REQUEST_TOPIC);
  }

  // Called by the channel before writing a request
  void OnModifyRequest(nsIHttpChannel* chan) {
    NotifyObservers(chan, NS_HTTP_ON_MODIFY_REQUEST_TOPIC);
  }

  // Same as OnModifyRequest but before cookie headers are written.
  void OnModifyRequestBeforeCookies(nsIHttpChannel* chan) {
    NotifyObservers(chan, NS_HTTP_ON_MODIFY_REQUEST_BEFORE_COOKIES_TOPIC);
  }

  void OnModifyDocumentRequest(nsIIdentChannel* chan) {
    NotifyObservers(chan, NS_DOCUMENT_ON_MODIFY_REQUEST_TOPIC);
  }

  // Called by the channel before calling onStopRequest
  void OnBeforeStopRequest(nsIHttpChannel* chan) {
    NotifyObservers(chan, NS_HTTP_ON_BEFORE_STOP_REQUEST_TOPIC);
  }

  // Called by the channel after calling onStopRequest
  void OnStopRequest(nsIHttpChannel* chan) {
    NotifyObservers(chan, NS_HTTP_ON_STOP_REQUEST_TOPIC);
  }

  // Called by the channel before setting up the transaction
  void OnBeforeConnect(nsIHttpChannel* chan) {
    NotifyObservers(chan, NS_HTTP_ON_BEFORE_CONNECT_TOPIC);
  }

  // Called by the channel once headers are available
  void OnExamineResponse(nsIHttpChannel* chan) {
    NotifyObservers(chan, NS_HTTP_ON_EXAMINE_RESPONSE_TOPIC);
  }

  // Called by the channel once headers have been merged with cached headers
  void OnExamineMergedResponse(nsIHttpChannel* chan) {
    NotifyObservers(chan, NS_HTTP_ON_EXAMINE_MERGED_RESPONSE_TOPIC);
  }

  // Called by the channel once it made background cache revalidation
  void OnBackgroundRevalidation(nsIHttpChannel* chan) {
    NotifyObservers(chan, NS_HTTP_ON_BACKGROUND_REVALIDATION);
  }

  // Called by channels before a redirect happens. This notifies both the
  // channel's and the global redirect observers.
  [[nodiscard]] nsresult AsyncOnChannelRedirect(
      nsIChannel* oldChan, nsIChannel* newChan, uint32_t flags,
      nsIEventTarget* mainThreadEventTarget = nullptr);

  // Called by the channel when the response is read from the cache without
  // communicating with the server.
  void OnExamineCachedResponse(nsIHttpChannel* chan) {
    NotifyObservers(chan, NS_HTTP_ON_EXAMINE_CACHED_RESPONSE_TOPIC);
  }

  // Called by the channel when the transaction pump is suspended because of
  // trying to get credentials asynchronously.
  void OnTransactionSuspendedDueToAuthentication(nsIHttpChannel* chan) {
    NotifyObservers(chan, "http-on-transaction-suspended-authentication");
  }

  // Generates the host:port string for use in the Host: header as well as the
  // CONNECT line for proxies. This handles IPv6 literals correctly.
  [[nodiscard]] static nsresult GenerateHostPort(const nsCString& host,
                                                 int32_t port,
                                                 nsACString& hostLine);

  static uint8_t UrgencyFromCoSFlags(uint32_t cos,
                                     int32_t aSupportsPriority = 0);

  SpdyInformation* SpdyInfo() { return &mSpdyInfo; }
  bool IsH2MandatorySuiteEnabled() { return mH2MandatorySuiteEnabled; }

  // returns true in between Init and Shutdown states
  bool Active() { return mHandlerActive; }

  nsIRequestContextService* GetRequestContextService() {
    return mRequestContextService.get();
  }

  void ShutdownConnectionManager();

  uint32_t DefaultHpackBuffer() const { return mDefaultHpackBuffer; }

  static bool IsHttp3Enabled();
  bool IsHttp3VersionSupported(const nsACString& version);

  static bool IsHttp3SupportedByServer(nsHttpResponseHead* aResponseHead);
  uint32_t DefaultQpackTableSize() const { return mQpackTableSize; }
  uint16_t DefaultHttp3MaxBlockedStreams() const {
    return (uint16_t)mHttp3MaxBlockedStreams;
  }

  const nsCString& Http3QlogDir();

  float FocusedWindowTransactionRatio() const {
    return mFocusedWindowTransactionRatio;
  }

  bool ActiveTabPriority() const { return mActiveTabPriority; }

  // Called when an optimization feature affecting active vs background tab load
  // took place.  Called only on the parent process and only updates
  // mLastActiveTabLoadOptimizationHit timestamp to now.
  void NotifyActiveTabLoadOptimization();
  TimeStamp GetLastActiveTabLoadOptimizationHit();
  void SetLastActiveTabLoadOptimizationHit(TimeStamp const& when);
  bool IsBeforeLastActiveTabLoadOptimization(TimeStamp const& when);

  HttpTrafficAnalyzer* GetHttpTrafficAnalyzer();

  bool GetThroughCaptivePortal() { return mThroughCaptivePortal; }

  nsresult CompleteUpgrade(HttpTransactionShell* aTrans,
                           nsIHttpUpgradeListener* aUpgradeListener);

  nsresult DoShiftReloadConnectionCleanupWithConnInfo(
      nsHttpConnectionInfo* aCI);

  void MaybeAddAltSvcForTesting(nsIURI* aUri, const nsACString& aUsername,
                                bool aPrivateBrowsing,
                                nsIInterfaceRequestor* aCallbacks,
                                const OriginAttributes& aOriginAttributes);

  bool EchConfigEnabled(bool aIsHttp3 = false) const;
  // When EchConfig is enabled and all records with echConfig are failed, this
  // functon indicate whether we can fallback to the origin server.
  // In the case an HTTPS RRSet contains some RRs with echConfig and some
  // without, we always fallback to the origin one.
  bool FallbackToOriginIfConfigsAreECHAndAllFailed() const;

  // So we can ensure that this is done during process preallocation to
  // avoid first-use overhead
  static void PresetAcceptLanguages();

  bool HttpActivityDistributorActivated();
  void ObserveHttpActivityWithArgs(const HttpActivityArgs& aArgs,
                                   uint32_t aActivityType,
                                   uint32_t aActivitySubtype, PRTime aTimestamp,
                                   uint64_t aExtraSizeData,
                                   const nsACString& aExtraStringData);

 private:
  nsHttpHandler();

  virtual ~nsHttpHandler();

  [[nodiscard]] nsresult Init();

  //
  // Useragent/prefs helper methods
  //
  void BuildUserAgent();
  void InitUserAgentComponents();
#ifdef XP_MACOSX
  void InitMSAuthorities();
#endif
  static void PrefsChanged(const char* pref, void* self);
  void PrefsChanged(const char* pref);

  [[nodiscard]] nsresult SetAcceptLanguages();
  [[nodiscard]] nsresult SetAcceptEncodings(const char*, bool mIsSecure);

  [[nodiscard]] nsresult InitConnectionMgr();

  void NotifyObservers(nsIChannel* chan, const char* event);

  friend class SocketProcessChild;
  void SetHttpHandlerInitArgs(const HttpHandlerInitArgs& aArgs);
  void SetDeviceModelId(const nsACString& aModelId);

  // We only allow DNSUtils and TRRServiceChannel itself to create
  // TRRServiceChannel.
  friend class TRRServiceChannel;
  friend class DNSUtils;
  nsresult CreateTRRServiceChannel(nsIURI* uri, nsIProxyInfo* givenProxyInfo,
                                   uint32_t proxyResolveFlags, nsIURI* proxyURI,
                                   nsILoadInfo* aLoadInfo, nsIChannel** result);
  nsresult SetupChannelInternal(HttpBaseChannel* aChannel, nsIURI* uri,
                                nsIProxyInfo* givenProxyInfo,
                                uint32_t proxyResolveFlags, nsIURI* proxyURI,
                                nsILoadInfo* aLoadInfo, nsIChannel** result);

 private:
  // cached services
  nsMainThreadPtrHandle<nsIIOService> mIOService;
  nsMainThreadPtrHandle<nsICookieService> mCookieService;
  nsMainThreadPtrHandle<nsISiteSecurityService> mSSService;

  // the authentication credentials cache
  nsHttpAuthCache mAuthCache;
  nsHttpAuthCache mPrivateAuthCache;

  // the connection manager
  RefPtr<HttpConnectionMgrShell> mConnMgr;

  UniquePtr<AltSvcCache> mAltSvcCache;

  //
  // prefs
  //

  enum HttpVersion mHttpVersion { HttpVersion::v1_1 };
  enum HttpVersion mProxyHttpVersion { HttpVersion::v1_1 };
  uint32_t mCapabilities{NS_HTTP_ALLOW_KEEPALIVE};

  bool mFastFallbackToIPv4{false};
  PRIntervalTime mIdleTimeout;
  PRIntervalTime mSpdyTimeout;
  PRIntervalTime mResponseTimeout;
  Atomic<bool, Relaxed> mResponseTimeoutEnabled{false};
  uint32_t mNetworkChangedTimeout{5000};  // milliseconds
  uint16_t mMaxRequestAttempts{6};
  uint16_t mMaxRequestDelay{10};
  uint16_t mIdleSynTimeout{250};
  uint16_t mFallbackSynTimeout{5};  // seconds

  bool mH2MandatorySuiteEnabled{false};
  uint16_t mMaxUrgentExcessiveConns{3};
  uint16_t mMaxConnections{24};
  uint8_t mMaxPersistentConnectionsPerServer{2};
  uint8_t mMaxPersistentConnectionsPerProxy{4};

  bool mThrottleEnabled{true};
  uint32_t mThrottleVersion{2};
  uint32_t mThrottleSuspendFor{3000};
  uint32_t mThrottleResumeFor{200};
  uint32_t mThrottleReadLimit{8000};
  uint32_t mThrottleReadInterval{500};
  uint32_t mThrottleHoldTime{600};
  uint32_t mThrottleMaxTime{3000};

  int32_t mSendWindowSize{1024};

  bool mUrgentStartEnabled{true};
  bool mTailBlockingEnabled{true};
  uint32_t mTailDelayQuantum{600};
  uint32_t mTailDelayQuantumAfterDCL{100};
  uint32_t mTailDelayMax{6000};
  uint32_t mTailTotalMax{0};

  uint8_t mRedirectionLimit{10};

  bool mBeConservativeForProxy{true};

  // we'll warn the user if we load an URL containing a userpass field
  // unless its length is less than this threshold.  this warning is
  // intended to protect the user against spoofing attempts that use
  // the userpass field of the URL to obscure the actual origin server.
  uint8_t mPhishyUserPassLength{1};

  uint8_t mQoSBits{0x00};

  bool mEnforceAssocReq{false};

  nsCString mImageAcceptHeader;
  nsCString mDocumentAcceptHeader;

  nsCString mAcceptLanguages;
  nsCString mHttpAcceptEncodings;
  nsCString mHttpsAcceptEncodings;

  nsCString mDefaultSocketType;

  // cache support
  uint32_t mLastUniqueID;
  Atomic<uint32_t, Relaxed> mSessionStartTime{0};

  // useragent components
  nsCString mLegacyAppName{"Mozilla"};
  nsCString mLegacyAppVersion{"5.0"};
  nsCString mPlatform;
  nsCString mOscpu;
  nsCString mMisc;
  nsCString mProduct{"Gecko"};
  nsCString mProductSub;
  nsCString mAppName;
  nsCString mAppVersion;
  nsCString mCompatFirefox;
  bool mCompatFirefoxEnabled{false};
  nsCString mCompatDevice;
  nsCString mDeviceModelId;

  nsCString mUserAgent;
  nsCString mSpoofedUserAgent;
  nsCString mUserAgentOverride;
  bool mUserAgentIsDirty{true};  // true if mUserAgent should be rebuilt
  bool mAcceptLanguagesIsDirty{true};

  bool mPromptTempRedirect{true};

  // Persistent HTTPS caching flag
  bool mEnablePersistentHttpsCaching{false};

  // for broadcasting safe hint;
  bool mSafeHintEnabled{false};
  bool mParentalControlEnabled{false};

  // true in between init and shutdown states
  Atomic<bool, Relaxed> mHandlerActive{false};

  // The value of 'hidden' network.http.debug-observations : 1;
  uint32_t mDebugObservations : 1;

  uint32_t mEnableAltSvc : 1;
  uint32_t mEnableAltSvcOE : 1;

  // Try to use SPDY features instead of HTTP/1.1 over SSL
  SpdyInformation mSpdyInfo;

  uint32_t mSpdySendingChunkSize{ASpdySession::kSendingChunkSize};
  uint32_t mSpdySendBufferSize{ASpdySession::kTCPSendBufferSize};
  uint32_t mSpdyPushAllowance{
      ASpdySession::kInitialPushAllowance};  // match default pref
  uint32_t mSpdyPullAllowance{ASpdySession::kInitialRwin};
  uint32_t mDefaultSpdyConcurrent{ASpdySession::kDefaultMaxConcurrent};
  PRIntervalTime mSpdyPingThreshold;
  PRIntervalTime mSpdyPingTimeout;

  // The maximum amount of time to wait for socket transport to be
  // established. In milliseconds.
  uint32_t mConnectTimeout{90000};

  // The maximum amount of time to wait for a tls handshake to be
  // established. In milliseconds.
  uint32_t mTLSHandshakeTimeout{30000};

  // The maximum number of current global half open sockets allowable
  // when starting a new speculative connection.
  uint32_t mParallelSpeculativeConnectLimit{6};

  // For Rate Pacing of HTTP/1 requests through a netwerk/base/EventTokenBucket
  // Active requests <= *MinParallelism are not subject to the rate pacing
  bool mRequestTokenBucketEnabled{true};
  uint16_t mRequestTokenBucketMinParallelism{6};
  uint32_t mRequestTokenBucketHz{100};    // EventTokenBucket HZ
  uint32_t mRequestTokenBucketBurst{32};  // EventTokenBucket Burst

  // Whether or not to block requests for non head js/css items (e.g. media)
  // while those elements load.
  bool mCriticalRequestPrioritization{true};

  // TCP Keepalive configuration values.

  // True if TCP keepalive is enabled for short-lived conns.
  bool mTCPKeepaliveShortLivedEnabled{false};
  // Time (secs) indicating how long a conn is considered short-lived.
  int32_t mTCPKeepaliveShortLivedTimeS{60};
  // Time (secs) before first keepalive probe; between successful probes.
  int32_t mTCPKeepaliveShortLivedIdleTimeS{10};

  // True if TCP keepalive is enabled for long-lived conns.
  bool mTCPKeepaliveLongLivedEnabled{false};
  // Time (secs) before first keepalive probe; between successful probes.
  int32_t mTCPKeepaliveLongLivedIdleTimeS{600};

  // if true, generate NS_ERROR_PARTIAL_TRANSFER for h1 responses with
  // incorrect content lengths or malformed chunked encodings
  FrameCheckLevel mEnforceH1Framing{FRAMECHECK_BARELY};

  nsCOMPtr<nsIRequestContextService> mRequestContextService;

  // The default size (in bytes) of the HPACK decompressor table.
  uint32_t mDefaultHpackBuffer{4096};

  // Http3 parameters
  Atomic<uint32_t, Relaxed> mQpackTableSize{4096};
  // uint16_t is enough here, but Atomic only supports uint32_t or uint64_t.
  Atomic<uint32_t, Relaxed> mHttp3MaxBlockedStreams{10};

  nsCString mHttp3QlogDir;

  // The ratio for dispatching transactions from the focused window.
  float mFocusedWindowTransactionRatio{0.9f};

  // If true, the transactions from active tab will be dispatched first.
  bool mActiveTabPriority{true};

  HttpTrafficAnalyzer mHttpTrafficAnalyzer;

 private:
  // For Rate Pacing Certain Network Events. Only assign this pointer on
  // socket thread.
  void MakeNewRequestTokenBucket();
  RefPtr<EventTokenBucket> mRequestTokenBucket;

 public:
  // Socket thread only
  [[nodiscard]] nsresult SubmitPacedRequest(ATokenBucketEvent* event,
                                            nsICancelable** cancel) {
    MOZ_ASSERT(OnSocketThread(), "not on socket thread");
    if (!mRequestTokenBucket) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    return mRequestTokenBucket->SubmitEvent(event, cancel);
  }

  // Socket thread only
  void SetRequestTokenBucket(EventTokenBucket* aTokenBucket) {
    MOZ_ASSERT(OnSocketThread(), "not on socket thread");
    mRequestTokenBucket = aTokenBucket;
  }

  void StopRequestTokenBucket() {
    MOZ_ASSERT(OnSocketThread(), "not on socket thread");
    if (mRequestTokenBucket) {
      mRequestTokenBucket->Stop();
      mRequestTokenBucket = nullptr;
    }
  }

 private:
  RefPtr<Tickler> mWifiTickler;
  void TickleWifi(nsIInterfaceRequestor* cb);

 private:
  [[nodiscard]] nsresult SpeculativeConnectInternal(
      nsIURI* aURI, nsIPrincipal* aPrincipal,
      Maybe<OriginAttributes>&& aOriginAttributes,
      nsIInterfaceRequestor* aCallbacks, bool anonymous);
  void ExcludeHttp2OrHttp3Internal(const nsHttpConnectionInfo* ci);

  // State for generating channelIds
  uint64_t mUniqueProcessId{0};
  Atomic<uint32_t, Relaxed> mNextChannelId{1};

  // ProcessId used for logging.
  uint32_t mProcessId{0};

  // The last time any of the active tab page load optimization took place.
  // This is accessed on multiple threads, hence a lock is needed.
  // On the parent process this is updated to now every time a scheduling
  // or rate optimization related to the active/background tab is hit.
  // We carry this value through each http channel's onstoprequest notification
  // to the parent process.  On the content process then we just update this
  // value from ipc onstoprequest arguments.  This is a sufficent way of passing
  // it down to the content process, since the value will be used only after
  // onstoprequest notification coming from an http channel.
  Mutex mLastActiveTabLoadOptimizationLock{
      "nsHttpConnectionMgr::LastActiveTabLoadOptimization"};
  TimeStamp mLastActiveTabLoadOptimizationHit;

  Mutex mHttpExclusionLock MOZ_UNANNOTATED{"nsHttpHandler::HttpExclusion"};

 public:
  [[nodiscard]] nsresult NewChannelId(uint64_t& channelId);
  void AddHttpChannel(uint64_t aId, nsISupports* aChannel);
  void RemoveHttpChannel(uint64_t aId);
  nsWeakPtr GetWeakHttpChannel(uint64_t aId);

  void ExcludeHttp2(const nsHttpConnectionInfo* ci);
  [[nodiscard]] bool IsHttp2Excluded(const nsHttpConnectionInfo* ci);
  void ExcludeHttp3(const nsHttpConnectionInfo* ci);
  [[nodiscard]] bool IsHttp3Excluded(const nsACString& aRoutedHost);
  void Exclude0RttTcp(const nsHttpConnectionInfo* ci);
  [[nodiscard]] bool Is0RttTcpExcluded(const nsHttpConnectionInfo* ci);

  void ExcludeHTTPSRRHost(const nsACString& aHost);
  [[nodiscard]] bool IsHostExcludedForHTTPSRR(const nsACString& aHost);

#ifdef XP_MACOSX
  [[nodiscard]] bool IsHostMSAuthority(const nsACString& aHost);
#endif

 private:
  nsTHashSet<nsCString> mExcludedHttp2Origins;
  nsTHashSet<nsCString> mExcludedHttp3Origins;
  nsTHashSet<nsCString> mExcluded0RttTcpOrigins;
  // A set of hosts that we should not upgrade to HTTPS with HTTPS RR.
  nsTHashSet<nsCString> mExcludedHostsForHTTPSRRUpgrade;

#ifdef XP_MACOSX
  // A list of trusted Microsoft SSO authority URLs
  nsTHashSet<nsCString> mMSAuthorities;
#endif

  Atomic<bool, Relaxed> mThroughCaptivePortal{false};

  // The mapping of channel id and the weak pointer of nsHttpChannel.
  nsTHashMap<nsUint64HashKey, nsWeakPtr> mIDToHttpChannelMap;

  // This is parsed pref network.http.http3.alt-svc-mapping-for-testing.
  // The pref set artificial altSvc-s for origin for testing.
  // This maps an origin to an altSvc.
  nsClassHashtable<nsCStringHashKey, nsCString> mAltSvcMappingTemptativeMap;

  nsCOMPtr<nsIHttpActivityDistributor> mActivityDistributor;
};

extern StaticRefPtr<nsHttpHandler> gHttpHandler;

//-----------------------------------------------------------------------------
// nsHttpsHandler - thin wrapper to distinguish the HTTP handler from the
//                  HTTPS handler (even though they share the same impl).
//-----------------------------------------------------------------------------

class nsHttpsHandler : public nsIHttpProtocolHandler,
                       public nsSupportsWeakReference,
                       public nsISpeculativeConnect {
  virtual ~nsHttpsHandler() = default;

 public:
  // we basically just want to override GetScheme and GetDefaultPort...
  // all other methods should be forwarded to the nsHttpHandler instance.

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER
  NS_FORWARD_NSIPROXIEDPROTOCOLHANDLER(gHttpHandler->)
  NS_FORWARD_NSIHTTPPROTOCOLHANDLER(gHttpHandler->)

  NS_IMETHOD SpeculativeConnect(nsIURI* aURI, nsIPrincipal* aPrincipal,
                                nsIInterfaceRequestor* aCallbacks,
                                bool aAnonymous) override {
    return gHttpHandler->SpeculativeConnect(aURI, aPrincipal, aCallbacks,
                                            aAnonymous);
  }

  NS_IMETHOD SpeculativeConnectWithOriginAttributes(
      nsIURI* aURI, JS::Handle<JS::Value> originAttributes,
      nsIInterfaceRequestor* aCallbacks, bool aAnonymous,
      JSContext* cx) override {
    return gHttpHandler->SpeculativeConnectWithOriginAttributes(
        aURI, originAttributes, aCallbacks, aAnonymous, cx);
  }

  NS_IMETHOD_(void)
  SpeculativeConnectWithOriginAttributesNative(
      nsIURI* aURI, mozilla::OriginAttributes&& originAttributes,
      nsIInterfaceRequestor* aCallbacks, bool aAnonymous) override {
    gHttpHandler->SpeculativeConnectWithOriginAttributesNative(
        aURI, std::move(originAttributes), aCallbacks, aAnonymous);
  }

  nsHttpsHandler() = default;

  [[nodiscard]] nsresult Init();
};

//-----------------------------------------------------------------------------
// HSTSDataCallbackWrapper - A threadsafe helper class to wrap the callback.
//
// We need this because dom::promise and EnsureHSTSDataResolver are not
// threadsafe.
//-----------------------------------------------------------------------------
class HSTSDataCallbackWrapper final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(HSTSDataCallbackWrapper)

  explicit HSTSDataCallbackWrapper(std::function<void(bool)>&& aCallback)
      : mCallback(std::move(aCallback)) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  void DoCallback(bool aResult) {
    MOZ_ASSERT(NS_IsMainThread());
    mCallback(aResult);
  }

 private:
  ~HSTSDataCallbackWrapper() = default;

  std::function<void(bool)> mCallback;
};

}  // namespace mozilla::net

#endif  // nsHttpHandler_h__
