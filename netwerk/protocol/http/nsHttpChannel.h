/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set et cin ts=4 sw=4 sts=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpChannel_h__
#define nsHttpChannel_h__

#include "HttpBaseChannel.h"
#include "nsTArray.h"
#include "nsICachingChannel.h"
#include "nsICacheEntry.h"
#include "nsICacheEntryOpenCallback.h"
#include "nsIDNSListener.h"
#include "nsIApplicationCacheChannel.h"
#include "nsIChannelWithDivertableParentListener.h"
#include "nsIProtocolProxyCallback.h"
#include "nsIHttpAuthenticableChannel.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "nsWeakReference.h"
#include "TimingStruct.h"
#include "ADivertableParentChannel.h"
#include "AutoClose.h"
#include "nsIStreamListener.h"
#include "nsISupportsPrimitives.h"
#include "nsICorsPreflightCallback.h"
#include "AlternateServices.h"
#include "nsIHstsPrimingCallback.h"
#include "nsIRaceCacheWithNetwork.h"

class nsDNSPrefetch;
class nsICancelable;
class nsIHttpChannelAuthProvider;
class nsInputStreamPump;
class nsISSLStatus;

namespace mozilla { namespace net {

class Http2PushedStream;

class HttpChannelSecurityWarningReporter
{
public:
  virtual MOZ_MUST_USE nsresult
  ReportSecurityMessage(const nsAString& aMessageTag,
                        const nsAString& aMessageCategory) = 0;
};

//-----------------------------------------------------------------------------
// nsHttpChannel
//-----------------------------------------------------------------------------

// Use to support QI nsIChannel to nsHttpChannel
#define NS_HTTPCHANNEL_IID                         \
{                                                  \
  0x301bf95b,                                      \
  0x7bb3,                                          \
  0x4ae1,                                          \
  {0xa9, 0x71, 0x40, 0xbc, 0xfa, 0x81, 0xde, 0x12} \
}

class nsHttpChannel final : public HttpBaseChannel
                          , public HttpAsyncAborter<nsHttpChannel>
                          , public nsIStreamListener
                          , public nsICachingChannel
                          , public nsICacheEntryOpenCallback
                          , public nsITransportEventSink
                          , public nsIProtocolProxyCallback
                          , public nsIHttpAuthenticableChannel
                          , public nsIApplicationCacheChannel
                          , public nsIAsyncVerifyRedirectCallback
                          , public nsIThreadRetargetableRequest
                          , public nsIThreadRetargetableStreamListener
                          , public nsIDNSListener
                          , public nsSupportsWeakReference
                          , public nsICorsPreflightCallback
                          , public nsIChannelWithDivertableParentListener
                          , public nsIHstsPrimingCallback
                          , public nsIRaceCacheWithNetwork
                          , public nsITimerCallback
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER
    NS_DECL_NSICACHEINFOCHANNEL
    NS_DECL_NSICACHINGCHANNEL
    NS_DECL_NSICACHEENTRYOPENCALLBACK
    NS_DECL_NSITRANSPORTEVENTSINK
    NS_DECL_NSIPROTOCOLPROXYCALLBACK
    NS_DECL_NSIPROXIEDCHANNEL
    NS_DECL_NSIAPPLICATIONCACHECONTAINER
    NS_DECL_NSIAPPLICATIONCACHECHANNEL
    NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK
    NS_DECL_NSIHSTSPRIMINGCALLBACK
    NS_DECL_NSITHREADRETARGETABLEREQUEST
    NS_DECL_NSIDNSLISTENER
    NS_DECL_NSICHANNELWITHDIVERTABLEPARENTLISTENER
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_HTTPCHANNEL_IID)
    NS_DECL_NSIRACECACHEWITHNETWORK
    NS_DECL_NSITIMERCALLBACK

    // nsIHttpAuthenticableChannel. We can't use
    // NS_DECL_NSIHTTPAUTHENTICABLECHANNEL because it duplicates cancel() and
    // others.
    NS_IMETHOD GetIsSSL(bool *aIsSSL) override;
    NS_IMETHOD GetProxyMethodIsConnect(bool *aProxyMethodIsConnect) override;
    NS_IMETHOD GetServerResponseHeader(nsACString & aServerResponseHeader) override;
    NS_IMETHOD GetProxyChallenges(nsACString & aChallenges) override;
    NS_IMETHOD GetWWWChallenges(nsACString & aChallenges) override;
    NS_IMETHOD SetProxyCredentials(const nsACString & aCredentials) override;
    NS_IMETHOD SetWWWCredentials(const nsACString & aCredentials) override;
    NS_IMETHOD OnAuthAvailable() override;
    NS_IMETHOD OnAuthCancelled(bool userCancel) override;
    NS_IMETHOD CloseStickyConnection() override;
    // Functions we implement from nsIHttpAuthenticableChannel but are
    // declared in HttpBaseChannel must be implemented in this class. We
    // just call the HttpBaseChannel:: impls.
    NS_IMETHOD GetLoadFlags(nsLoadFlags *aLoadFlags) override;
    NS_IMETHOD GetURI(nsIURI **aURI) override;
    NS_IMETHOD GetNotificationCallbacks(nsIInterfaceRequestor **aCallbacks) override;
    NS_IMETHOD GetLoadGroup(nsILoadGroup **aLoadGroup) override;
    NS_IMETHOD GetRequestMethod(nsACString& aMethod) override;

    nsHttpChannel();

    virtual MOZ_MUST_USE nsresult Init(nsIURI *aURI, uint32_t aCaps,
                                       nsProxyInfo *aProxyInfo,
                                       uint32_t aProxyResolveFlags,
                                       nsIURI *aProxyURI,
                                       uint64_t aChannelId) override;

    MOZ_MUST_USE nsresult OnPush(const nsACString &uri,
                                 Http2PushedStream *pushedStream);

    static bool IsRedirectStatus(uint32_t status);
    static bool WillRedirect(nsHttpResponseHead * response);


    // Methods HttpBaseChannel didn't implement for us or that we override.
    //
    // nsIRequest
    NS_IMETHOD Cancel(nsresult status) override;
    NS_IMETHOD Suspend() override;
    NS_IMETHOD Resume() override;
    // nsIChannel
    NS_IMETHOD GetSecurityInfo(nsISupports **aSecurityInfo) override;
    NS_IMETHOD AsyncOpen(nsIStreamListener *listener, nsISupports *aContext) override;
    NS_IMETHOD AsyncOpen2(nsIStreamListener *aListener) override;
    // nsIHttpChannel
    NS_IMETHOD GetEncodedBodySize(uint64_t *aEncodedBodySize) override;
    // nsIHttpChannelInternal
    NS_IMETHOD SetupFallbackChannel(const char *aFallbackKey) override;
    NS_IMETHOD ForceIntercepted(uint64_t aInterceptionID) override;
    // nsISupportsPriority
    NS_IMETHOD SetPriority(int32_t value) override;
    // nsIClassOfService
    NS_IMETHOD SetClassFlags(uint32_t inFlags) override;
    NS_IMETHOD AddClassFlags(uint32_t inFlags) override;
    NS_IMETHOD ClearClassFlags(uint32_t inFlags) override;

    // nsIResumableChannel
    NS_IMETHOD ResumeAt(uint64_t startPos, const nsACString& entityID) override;

    NS_IMETHOD SetNotificationCallbacks(nsIInterfaceRequestor *aCallbacks) override;
    NS_IMETHOD SetLoadGroup(nsILoadGroup *aLoadGroup) override;
    // nsITimedChannel
    NS_IMETHOD GetDomainLookupStart(mozilla::TimeStamp *aDomainLookupStart) override;
    NS_IMETHOD GetDomainLookupEnd(mozilla::TimeStamp *aDomainLookupEnd) override;
    NS_IMETHOD GetConnectStart(mozilla::TimeStamp *aConnectStart) override;
    NS_IMETHOD GetConnectEnd(mozilla::TimeStamp *aConnectEnd) override;
    NS_IMETHOD GetRequestStart(mozilla::TimeStamp *aRequestStart) override;
    NS_IMETHOD GetResponseStart(mozilla::TimeStamp *aResponseStart) override;
    NS_IMETHOD GetResponseEnd(mozilla::TimeStamp *aResponseEnd) override;
    // nsICorsPreflightCallback
    NS_IMETHOD OnPreflightSucceeded() override;
    NS_IMETHOD OnPreflightFailed(nsresult aError) override;

    MOZ_MUST_USE nsresult
    AddSecurityMessage(const nsAString& aMessageTag,
                       const nsAString& aMessageCategory) override;

    void SetWarningReporter(HttpChannelSecurityWarningReporter* aReporter)
      { mWarningReporter = aReporter; }

public: /* internal necko use only */

    using InitLocalBlockListCallback = std::function<void(bool)>;

    void InternalSetUploadStream(nsIInputStream *uploadStream)
      { mUploadStream = uploadStream; }
    void SetUploadStreamHasHeaders(bool hasHeaders)
      { mUploadStreamHasHeaders = hasHeaders; }

    MOZ_MUST_USE nsresult
    SetReferrerWithPolicyInternal(nsIURI *referrer, uint32_t referrerPolicy) {
        nsAutoCString spec;
        nsresult rv = referrer->GetAsciiSpec(spec);
        if (NS_FAILED(rv)) return rv;
        mReferrer = referrer;
        mReferrerPolicy = referrerPolicy;
        rv = mRequestHead.SetHeader(nsHttp::Referer, spec);
        return rv;
    }

    MOZ_MUST_USE nsresult SetTopWindowURI(nsIURI* aTopWindowURI) {
        mTopWindowURI = aTopWindowURI;
        return NS_OK;
    }

    uint32_t GetRequestTime() const
    {
        return mRequestTime;
    }

    MOZ_MUST_USE nsresult OpenCacheEntry(bool usingSSL);
    MOZ_MUST_USE nsresult ContinueConnect();

    // If the load is mixed-content, build and send an HSTS priming request.
    MOZ_MUST_USE nsresult TryHSTSPriming();

    MOZ_MUST_USE nsresult StartRedirectChannelToURI(nsIURI *, uint32_t);

    // This allows cache entry to be marked as foreign even after channel itself
    // is gone.  Needed for e10s (see HttpChannelParent::RecvDocumentChannelCleanup)
    class OfflineCacheEntryAsForeignMarker {
        nsCOMPtr<nsIApplicationCache> mApplicationCache;
        nsCOMPtr<nsIURI> mCacheURI;
    public:
        OfflineCacheEntryAsForeignMarker(nsIApplicationCache* appCache,
                                         nsIURI* aURI)
             : mApplicationCache(appCache)
             , mCacheURI(aURI)
        {}

        nsresult MarkAsForeign();
    };

    OfflineCacheEntryAsForeignMarker* GetOfflineCacheEntryAsForeignMarker();

    // Helper to keep cache callbacks wait flags consistent
    class AutoCacheWaitFlags
    {
    public:
      explicit AutoCacheWaitFlags(nsHttpChannel* channel)
        : mChannel(channel)
        , mKeep(0)
      {
        // Flags must be set before entering any AsyncOpenCacheEntry call.
        mChannel->mCacheEntriesToWaitFor =
          nsHttpChannel::WAIT_FOR_CACHE_ENTRY |
          nsHttpChannel::WAIT_FOR_OFFLINE_CACHE_ENTRY;
      }

      void Keep(uint32_t flags)
      {
        // Called after successful call to appropriate AsyncOpenCacheEntry call.
        mKeep |= flags;
      }

      ~AutoCacheWaitFlags()
      {
        // Keep only flags those are left to be wait for.
        mChannel->mCacheEntriesToWaitFor &= mKeep;
      }

    private:
      nsHttpChannel* mChannel;
      uint32_t mKeep : 2;
    };

    void MarkIntercepted();
    NS_IMETHOD GetResponseSynthesized(bool* aSynthesized) override;
    bool AwaitingCacheCallbacks();
    void SetCouldBeSynthesized();

    // Return true if the latest ODA is invoked by mCachePump.
    // Should only be called on the same thread as ODA.
    bool IsReadingFromCache() const { return mIsReadingFromCache; }

private: // used for alternate service validation
    RefPtr<TransactionObserver> mTransactionObserver;
public:
    void SetConnectionInfo(nsHttpConnectionInfo *); // clones the argument
    void SetTransactionObserver(TransactionObserver *arg) { mTransactionObserver = arg; }
    TransactionObserver *GetTransactionObserver() { return mTransactionObserver; }

protected:
    virtual ~nsHttpChannel();

private:
    typedef nsresult (nsHttpChannel::*nsContinueRedirectionFunc)(nsresult result);

    bool     RequestIsConditional();

    // Connections will only be established in this function.
    // (including DNS prefetch and speculative connection.)
    nsresult BeginConnectActual();

    // We might synchronously or asynchronously call BeginConnectActual,
    // which includes DNS prefetch and speculative connection, according to
    // whether an async tracker lookup is required. If the tracker lookup
    // is required, this funciton will just return NS_OK and BeginConnectActual()
    // will be called when callback. See Bug 1325054 for more information.
    nsresult BeginConnect();

    MOZ_MUST_USE nsresult ContinueBeginConnectWithResult();
    void     ContinueBeginConnect();
    MOZ_MUST_USE nsresult Connect();
    void     SpeculativeConnect();
    MOZ_MUST_USE nsresult SetupTransaction();
    void     SetupTransactionRequestContext();
    MOZ_MUST_USE nsresult CallOnStartRequest();
    MOZ_MUST_USE nsresult ProcessResponse();
    void                  AsyncContinueProcessResponse();
    MOZ_MUST_USE nsresult ContinueProcessResponse1();
    MOZ_MUST_USE nsresult ContinueProcessResponse2(nsresult);
    MOZ_MUST_USE nsresult ContinueProcessResponse3(nsresult);
    MOZ_MUST_USE nsresult ProcessNormal();
    MOZ_MUST_USE nsresult ContinueProcessNormal(nsresult);
    void     ProcessAltService();
    bool     ShouldBypassProcessNotModified();
    MOZ_MUST_USE nsresult ProcessNotModified();
    MOZ_MUST_USE nsresult AsyncProcessRedirection(uint32_t httpStatus);
    MOZ_MUST_USE nsresult ContinueProcessRedirection(nsresult);
    MOZ_MUST_USE nsresult ContinueProcessRedirectionAfterFallback(nsresult);
    MOZ_MUST_USE nsresult ProcessFailedProxyConnect(uint32_t httpStatus);
    MOZ_MUST_USE nsresult ProcessFallback(bool *waitingForRedirectCallback);
    MOZ_MUST_USE nsresult ContinueProcessFallback(nsresult);
    void     HandleAsyncAbort();
    MOZ_MUST_USE nsresult EnsureAssocReq();
    void     ProcessSSLInformation();
    bool     IsHTTPS();

    MOZ_MUST_USE nsresult ContinueOnStartRequest1(nsresult);
    MOZ_MUST_USE nsresult ContinueOnStartRequest2(nsresult);
    MOZ_MUST_USE nsresult ContinueOnStartRequest3(nsresult);

    bool InitLocalBlockList(const InitLocalBlockListCallback& aCallback);

    // redirection specific methods
    void     HandleAsyncRedirect();
    void     HandleAsyncAPIRedirect();
    MOZ_MUST_USE nsresult ContinueHandleAsyncRedirect(nsresult);
    void     HandleAsyncNotModified();
    void     HandleAsyncFallback();
    MOZ_MUST_USE nsresult ContinueHandleAsyncFallback(nsresult);
    MOZ_MUST_USE nsresult PromptTempRedirect();
    virtual MOZ_MUST_USE nsresult
    SetupReplacementChannel(nsIURI *, nsIChannel *, bool preserveMethod,
                            uint32_t redirectFlags) override;

    // proxy specific methods
    MOZ_MUST_USE nsresult ProxyFailover();
    MOZ_MUST_USE nsresult AsyncDoReplaceWithProxy(nsIProxyInfo *);
    MOZ_MUST_USE nsresult ContinueDoReplaceWithProxy(nsresult);
    MOZ_MUST_USE nsresult ResolveProxy();

    // cache specific methods
    MOZ_MUST_USE nsresult
    OnOfflineCacheEntryAvailable(nsICacheEntry *aEntry, bool aNew,
                                 nsIApplicationCache* aAppCache,
                                 nsresult aResult);
    MOZ_MUST_USE nsresult OnNormalCacheEntryAvailable(nsICacheEntry *aEntry,
                                                      bool aNew,
                                                      nsresult aResult);
    MOZ_MUST_USE nsresult OpenOfflineCacheEntryForWriting();
    MOZ_MUST_USE nsresult
    OnOfflineCacheEntryForWritingAvailable(nsICacheEntry *aEntry,
                                           nsIApplicationCache* aAppCache,
                                           nsresult aResult);
    MOZ_MUST_USE nsresult
    OnCacheEntryAvailableInternal(nsICacheEntry *entry, bool aNew,
                                  nsIApplicationCache* aAppCache,
                                  nsresult status);
    MOZ_MUST_USE nsresult GenerateCacheKey(uint32_t postID, nsACString &key);
    MOZ_MUST_USE nsresult UpdateExpirationTime();
    MOZ_MUST_USE nsresult CheckPartial(nsICacheEntry* aEntry, int64_t *aSize, int64_t *aContentLength);
    bool ShouldUpdateOfflineCacheEntry();
    MOZ_MUST_USE nsresult ReadFromCache(bool alreadyMarkedValid);
    void     CloseCacheEntry(bool doomOnFailure);
    void     CloseOfflineCacheEntry();
    MOZ_MUST_USE nsresult InitCacheEntry();
    void     UpdateInhibitPersistentCachingFlag();
    MOZ_MUST_USE nsresult InitOfflineCacheEntry();
    MOZ_MUST_USE nsresult AddCacheEntryHeaders(nsICacheEntry *entry);
    MOZ_MUST_USE nsresult FinalizeCacheEntry();
    MOZ_MUST_USE nsresult InstallCacheListener(int64_t offset = 0);
    MOZ_MUST_USE nsresult InstallOfflineCacheListener(int64_t offset = 0);
    void     MaybeInvalidateCacheEntryForSubsequentGet();
    void     AsyncOnExamineCachedResponse();

    // Handle the bogus Content-Encoding Apache sometimes sends
    void ClearBogusContentEncodingIfNeeded();

    // byte range request specific methods
    MOZ_MUST_USE nsresult ProcessPartialContent();
    MOZ_MUST_USE nsresult OnDoneReadingPartialCacheEntry(bool *streamDone);

    MOZ_MUST_USE nsresult DoAuthRetry(nsAHttpConnection *);

    void     HandleAsyncRedirectChannelToHttps();
    MOZ_MUST_USE nsresult StartRedirectChannelToHttps();
    MOZ_MUST_USE nsresult ContinueAsyncRedirectChannelToURI(nsresult rv);
    MOZ_MUST_USE nsresult OpenRedirectChannel(nsresult rv);

    /**
     * A function that takes care of reading STS and PKP headers and enforcing
     * STS and PKP load rules. After a secure channel is erected, STS and PKP
     * requires the channel to be trusted or any STS or PKP header data on
     * the channel is ignored. This is called from ProcessResponse.
     */
    MOZ_MUST_USE nsresult ProcessSecurityHeaders();

    /**
     * Taking care of the Content-Signature header and fail the channel if
     * the signature verification fails or is required but the header is not
     * present.
     * This sets mListener to ContentVerifier, which buffers the entire response
     * before verifying the Content-Signature header. If the verification is
     * successful, the load proceeds as usual. If the verification fails, a
     * NS_ERROR_INVALID_SIGNATURE is thrown and a fallback loaded in nsDocShell
     */
    MOZ_MUST_USE nsresult
    ProcessContentSignatureHeader(nsHttpResponseHead *aResponseHead);

    /**
     * A function that will, if the feature is enabled, send security reports.
     */
    void ProcessSecurityReport(nsresult status);

    /**
     * A function to process a single security header (STS or PKP), assumes
     * some basic sanity checks have been applied to the channel. Called
     * from ProcessSecurityHeaders.
     */
    MOZ_MUST_USE nsresult ProcessSingleSecurityHeader(uint32_t aType,
                                                      nsISSLStatus *aSSLStatus,
                                                      uint32_t aFlags);

    void InvalidateCacheEntryForLocation(const char *location);
    void AssembleCacheKey(const char *spec, uint32_t postID, nsACString &key);
    MOZ_MUST_USE nsresult CreateNewURI(const char *loc, nsIURI **newURI);
    void DoInvalidateCacheEntry(nsIURI* aURI);

    // Ref RFC2616 13.10: "invalidation... MUST only be performed if
    // the host part is the same as in the Request-URI"
    inline bool HostPartIsTheSame(nsIURI *uri) {
        nsAutoCString tmpHost1, tmpHost2;
        return (NS_SUCCEEDED(mURI->GetAsciiHost(tmpHost1)) &&
                NS_SUCCEEDED(uri->GetAsciiHost(tmpHost2)) &&
                (tmpHost1 == tmpHost2));
    }

    inline static bool DoNotRender3xxBody(nsresult rv) {
        return rv == NS_ERROR_REDIRECT_LOOP         ||
               rv == NS_ERROR_CORRUPTED_CONTENT     ||
               rv == NS_ERROR_UNKNOWN_PROTOCOL      ||
               rv == NS_ERROR_MALFORMED_URI;
    }

    // Report net vs cache time telemetry
    void ReportNetVSCacheTelemetry();
    int64_t ComputeTelemetryBucketNumber(int64_t difftime_ms);

    // Create a aggregate set of the current notification callbacks
    // and ensure the transaction is updated to use it.
    void UpdateAggregateCallbacks();

    static bool HasQueryString(nsHttpRequestHead::ParsedMethodType method, nsIURI * uri);
    bool ResponseWouldVary(nsICacheEntry* entry);
    bool IsResumable(int64_t partialLen, int64_t contentLength,
                     bool ignoreMissingPartialLen = false) const;
    MOZ_MUST_USE nsresult
    MaybeSetupByteRangeRequest(int64_t partialLen, int64_t contentLength,
                               bool ignoreMissingPartialLen = false);
    MOZ_MUST_USE nsresult SetupByteRangeRequest(int64_t partialLen);
    void UntieByteRangeRequest();
    void UntieValidationRequest();
    MOZ_MUST_USE nsresult OpenCacheInputStream(nsICacheEntry* cacheEntry,
                                               bool startBuffering,
                                               bool checkingAppCacheEntry);

    void SetPushedStream(Http2PushedStream *stream);

    void MaybeWarnAboutAppCache();

    void SetLoadGroupUserAgentOverride();

    void SetDoNotTrack();

private:
    // this section is for main-thread-only object
    // all the references need to be proxy released on main thread.
    nsCOMPtr<nsIApplicationCache> mApplicationCacheForWrite;
    // auth specific data
    nsCOMPtr<nsIHttpChannelAuthProvider> mAuthProvider;
    nsCOMPtr<nsIURI> mRedirectURI;
    nsCOMPtr<nsIChannel> mRedirectChannel;
    nsCOMPtr<nsIChannel> mPreflightChannel;

    // Proxy release all members above on main thread.
    void ReleaseMainThreadOnlyReferences();

private:
    nsCOMPtr<nsICancelable>           mProxyRequest;

    RefPtr<nsInputStreamPump>       mTransactionPump;
    RefPtr<nsHttpTransaction>       mTransaction;

    uint64_t                          mLogicalOffset;

    // cache specific data
    nsCOMPtr<nsICacheEntry>           mCacheEntry;
    // This will be set during OnStopRequest() before calling CloseCacheEntry(),
    // but only if the listener wants to use alt-data (signaled by
    // HttpBaseChannel::mPreferredCachedAltDataType being not empty)
    // Needed because calling openAlternativeOutputStream needs a reference
    // to the cache entry.
    nsCOMPtr<nsICacheEntry>           mAltDataCacheEntry;
    // We must close mCacheInputStream explicitly to avoid leaks.
    AutoClose<nsIInputStream>         mCacheInputStream;
    RefPtr<nsInputStreamPump>       mCachePump;
    nsAutoPtr<nsHttpResponseHead>     mCachedResponseHead;
    nsCOMPtr<nsISupports>             mCachedSecurityInfo;
    uint32_t                          mPostID;
    uint32_t                          mRequestTime;

    nsCOMPtr<nsICacheEntry> mOfflineCacheEntry;
    uint32_t                          mOfflineCacheLastModifiedTime;

    mozilla::TimeStamp                mOnStartRequestTimestamp;
    // Timestamp of the time the cnannel was suspended.
    mozilla::TimeStamp                mSuspendTimestamp;
    // Total time the channel spent suspended. This value is reported to
    // telemetry in nsHttpChannel::OnStartRequest().
    uint32_t                          mSuspendTotalTime;

    // States of channel interception
    enum {
        DO_NOT_INTERCEPT,  // no interception will occur
        MAYBE_INTERCEPT,   // interception in progress, but can be cancelled
        INTERCEPTED,       // a synthesized response has been provided
    } mInterceptCache;
    // ID of this channel for the interception purposes. Unique unless this
    // channel is replacing an intercepted one via an redirection.
    uint64_t mInterceptionID;

    bool PossiblyIntercepted() {
        return mInterceptCache != DO_NOT_INTERCEPT;
    }

    // If the channel is associated with a cache, and the URI matched
    // a fallback namespace, this will hold the key for the fallback
    // cache entry.
    nsCString                         mFallbackKey;

    friend class AutoRedirectVetoNotifier;
    friend class HttpAsyncAborter<nsHttpChannel>;

    uint32_t                          mRedirectType;

    static const uint32_t WAIT_FOR_CACHE_ENTRY = 1;
    static const uint32_t WAIT_FOR_OFFLINE_CACHE_ENTRY = 2;

    bool                              mCacheOpenWithPriority;
    uint32_t                          mCacheQueueSizeWhenOpen;

    // state flags
    uint32_t                          mCachedContentIsValid     : 1;
    uint32_t                          mCachedContentIsPartial   : 1;
    uint32_t                          mCacheOnlyMetadata        : 1;
    uint32_t                          mTransactionReplaced      : 1;
    uint32_t                          mAuthRetryPending         : 1;
    uint32_t                          mProxyAuthPending         : 1;
    // Set if before the first authentication attempt a custom authorization
    // header has been set on the channel.  This will make that custom header
    // go to the server instead of any cached credentials.
    uint32_t                          mCustomAuthHeader         : 1;
    uint32_t                          mResuming                 : 1;
    uint32_t                          mInitedCacheEntry         : 1;
    // True if we are loading a fallback cache entry from the
    // application cache.
    uint32_t                          mFallbackChannel          : 1;
    // True if consumer added its own If-None-Match or If-Modified-Since
    // headers. In such a case we must not override them in the cache code
    // and also we want to pass possible 304 code response through.
    uint32_t                          mCustomConditionalRequest : 1;
    uint32_t                          mFallingBack              : 1;
    uint32_t                          mWaitingForRedirectCallback : 1;
    // True if mRequestTime has been set. In such a case it is safe to update
    // the cache entry's expiration time. Otherwise, it is not(see bug 567360).
    uint32_t                          mRequestTimeInitialized : 1;
    uint32_t                          mCacheEntryIsReadOnly : 1;
    uint32_t                          mCacheEntryIsWriteOnly : 1;
    // see WAIT_FOR_* constants above
    uint32_t                          mCacheEntriesToWaitFor : 2;
    uint32_t                          mHasQueryString : 1;
    // whether cache entry data write was in progress during cache entry check
    // when true, after we finish read from cache we must check all data
    // had been loaded from cache. If not, then an error has to be propagated
    // to the consumer.
    uint32_t                          mConcurrentCacheAccess : 1;
    // whether the request is setup be byte-range
    uint32_t                          mIsPartialRequest : 1;
    // true iff there is AutoRedirectVetoNotifier on the stack
    uint32_t                          mHasAutoRedirectVetoNotifier : 1;
    // consumers set this to true to use cache pinning, this has effect
    // only when the channel is in an app context (load context has an appid)
    uint32_t                          mPinCacheContent : 1;
    // True if CORS preflight has been performed
    uint32_t                          mIsCorsPreflightDone : 1;

    // if the http transaction was performed (i.e. not cached) and
    // the result in OnStopRequest was known to be correctly delimited
    // by chunking, content-length, or h2 end-stream framing
    uint32_t                          mStronglyFramed : 1;

    // true if an HTTP transaction is created for the socket thread
    uint32_t                          mUsedNetwork : 1;

    nsTArray<nsContinueRedirectionFunc> mRedirectFuncStack;

    // Needed for accurate DNS timing
    RefPtr<nsDNSPrefetch>           mDNSPrefetch;

    Http2PushedStream                 *mPushedStream;
    // True if the channel's principal was found on a phishing, malware, or
    // tracking (if tracking protection is enabled) blocklist
    bool                              mLocalBlocklist;

    MOZ_MUST_USE nsresult WaitForRedirectCallback();
    void PushRedirectAsyncFunc(nsContinueRedirectionFunc func);
    void PopRedirectAsyncFunc(nsContinueRedirectionFunc func);

    nsCString mUsername;

    // If non-null, warnings should be reported to this object.
    HttpChannelSecurityWarningReporter* mWarningReporter;

    RefPtr<ADivertableParentChannel> mParentChannel;

    // True if the channel is reading from cache.
    Atomic<bool> mIsReadingFromCache;

    // These next members are only used in unit tests to delay the call to
    // cache->AsyncOpenURI in order to race the cache with the network.
    nsCOMPtr<nsITimer> mCacheOpenTimer;
    std::function<void(nsHttpChannel*)> mCacheOpenFunc;
    uint32_t mCacheOpenDelay = 0;

    // We need to remember which is the source of the response we are using.
    enum {
        RESPONSE_PENDING,           // response is pending
        RESPONSE_FROM_CACHE,        // response coming from cache. no network.
        RESPONSE_FROM_NETWORK,      // response coming from the network
    } mFirstResponseSource = RESPONSE_PENDING;

    nsresult TriggerNetwork(int32_t aTimeout);
    void CancelNetworkRequest(nsresult aStatus);
    // Timer used to delay the network request, or to trigger the network
    // request if retrieving the cache entry takes too long.
    nsCOMPtr<nsITimer> mNetworkTriggerTimer;
    // Is true if the network request has been triggered.
    bool mNetworkTriggered = false;
    bool mWaitingForProxy = false;
    // Is true if the onCacheEntryAvailable callback has been called.
    Atomic<bool> mOnCacheAvailableCalled;
    // Will be true if the onCacheEntryAvailable callback is not called by the
    // time we send the network request. This could also be true when we are
    // bypassing the cache.
    Atomic<bool> mRacingNetAndCache;

protected:
    virtual void DoNotifyListenerCleanup() override;

private: // cache telemetry
    bool mDidReval;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsHttpChannel, NS_HTTPCHANNEL_IID)
} // namespace net
} // namespace mozilla

#endif // nsHttpChannel_h__
