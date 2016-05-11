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
  virtual nsresult ReportSecurityMessage(const nsAString& aMessageTag,
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
    NS_DECL_NSITHREADRETARGETABLEREQUEST
    NS_DECL_NSIDNSLISTENER
    NS_DECL_NSICHANNELWITHDIVERTABLEPARENTLISTENER
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_HTTPCHANNEL_IID)

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
    // Functions we implement from nsIHttpAuthenticableChannel but are
    // declared in HttpBaseChannel must be implemented in this class. We
    // just call the HttpBaseChannel:: impls.
    NS_IMETHOD GetLoadFlags(nsLoadFlags *aLoadFlags) override;
    NS_IMETHOD GetURI(nsIURI **aURI) override;
    NS_IMETHOD GetNotificationCallbacks(nsIInterfaceRequestor **aCallbacks) override;
    NS_IMETHOD GetLoadGroup(nsILoadGroup **aLoadGroup) override;
    NS_IMETHOD GetRequestMethod(nsACString& aMethod) override;

    nsHttpChannel();

    virtual nsresult Init(nsIURI *aURI, uint32_t aCaps, nsProxyInfo *aProxyInfo,
                          uint32_t aProxyResolveFlags,
                          nsIURI *aProxyURI) override;

    nsresult OnPush(const nsACString &uri, Http2PushedStream *pushedStream);

    static bool IsRedirectStatus(uint32_t status);


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

    nsresult AddSecurityMessage(const nsAString& aMessageTag,
                                const nsAString& aMessageCategory) override;

    void SetWarningReporter(HttpChannelSecurityWarningReporter* aReporter)
      { mWarningReporter = aReporter; }

public: /* internal necko use only */

    void InternalSetUploadStream(nsIInputStream *uploadStream)
      { mUploadStream = uploadStream; }
    void SetUploadStreamHasHeaders(bool hasHeaders)
      { mUploadStreamHasHeaders = hasHeaders; }

    nsresult SetReferrerWithPolicyInternal(nsIURI *referrer,
                                           uint32_t referrerPolicy) {
        nsAutoCString spec;
        nsresult rv = referrer->GetAsciiSpec(spec);
        if (NS_FAILED(rv)) return rv;
        mReferrer = referrer;
        mReferrerPolicy = referrerPolicy;
        mRequestHead.SetHeader(nsHttp::Referer, spec);
        return NS_OK;
    }

    nsresult SetTopWindowURI(nsIURI* aTopWindowURI) {
        mTopWindowURI = aTopWindowURI;
        return NS_OK;
    }

    uint32_t GetRequestTime() const
    {
        return mRequestTime;
    }

    nsresult OpenCacheEntry(bool usingSSL);
    nsresult ContinueConnect();

    nsresult StartRedirectChannelToURI(nsIURI *, uint32_t);

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

protected:
    virtual ~nsHttpChannel();

private:
    typedef nsresult (nsHttpChannel::*nsContinueRedirectionFunc)(nsresult result);

    bool     RequestIsConditional();
    nsresult BeginConnect();
    nsresult ContinueBeginConnectWithResult();
    void     ContinueBeginConnect();
    nsresult Connect();
    void     SpeculativeConnect();
    nsresult SetupTransaction();
    void     SetupTransactionRequestContext();
    nsresult CallOnStartRequest();
    nsresult ProcessResponse();
    nsresult ContinueProcessResponse1(nsresult);
    nsresult ContinueProcessResponse2(nsresult);
    nsresult ProcessNormal();
    nsresult ContinueProcessNormal(nsresult);
    void     ProcessAltService();
    nsresult ProcessNotModified();
    nsresult AsyncProcessRedirection(uint32_t httpStatus);
    nsresult ContinueProcessRedirection(nsresult);
    nsresult ContinueProcessRedirectionAfterFallback(nsresult);
    nsresult ProcessFailedProxyConnect(uint32_t httpStatus);
    nsresult ProcessFallback(bool *waitingForRedirectCallback);
    nsresult ContinueProcessFallback(nsresult);
    void     HandleAsyncAbort();
    nsresult EnsureAssocReq();
    void     ProcessSSLInformation();
    bool     IsHTTPS();

    nsresult ContinueOnStartRequest1(nsresult);
    nsresult ContinueOnStartRequest2(nsresult);
    nsresult ContinueOnStartRequest3(nsresult);

    // redirection specific methods
    void     HandleAsyncRedirect();
    void     HandleAsyncAPIRedirect();
    nsresult ContinueHandleAsyncRedirect(nsresult);
    void     HandleAsyncNotModified();
    void     HandleAsyncFallback();
    nsresult ContinueHandleAsyncFallback(nsresult);
    nsresult PromptTempRedirect();
    virtual  nsresult SetupReplacementChannel(nsIURI *, nsIChannel *,
                                              bool preserveMethod,
                                              uint32_t redirectFlags) override;

    // proxy specific methods
    nsresult ProxyFailover();
    nsresult AsyncDoReplaceWithProxy(nsIProxyInfo *);
    nsresult ContinueDoReplaceWithProxy(nsresult);
    nsresult ResolveProxy();

    // cache specific methods
    nsresult OnOfflineCacheEntryAvailable(nsICacheEntry *aEntry,
                                          bool aNew,
                                          nsIApplicationCache* aAppCache,
                                          nsresult aResult);
    nsresult OnNormalCacheEntryAvailable(nsICacheEntry *aEntry,
                                         bool aNew,
                                         nsresult aResult);
    nsresult OpenOfflineCacheEntryForWriting();
    nsresult OnOfflineCacheEntryForWritingAvailable(nsICacheEntry *aEntry,
                                                    nsIApplicationCache* aAppCache,
                                                    nsresult aResult);
    nsresult OnCacheEntryAvailableInternal(nsICacheEntry *entry,
                                      bool aNew,
                                      nsIApplicationCache* aAppCache,
                                      nsresult status);
    nsresult GenerateCacheKey(uint32_t postID, nsACString &key);
    nsresult UpdateExpirationTime();
    nsresult CheckPartial(nsICacheEntry* aEntry, int64_t *aSize, int64_t *aContentLength);
    bool ShouldUpdateOfflineCacheEntry();
    nsresult ReadFromCache(bool alreadyMarkedValid);
    void     CloseCacheEntry(bool doomOnFailure);
    void     CloseOfflineCacheEntry();
    nsresult InitCacheEntry();
    void     UpdateInhibitPersistentCachingFlag();
    nsresult InitOfflineCacheEntry();
    nsresult AddCacheEntryHeaders(nsICacheEntry *entry);
    nsresult FinalizeCacheEntry();
    nsresult InstallCacheListener(int64_t offset = 0);
    nsresult InstallOfflineCacheListener(int64_t offset = 0);
    void     MaybeInvalidateCacheEntryForSubsequentGet();
    void     AsyncOnExamineCachedResponse();

    // Handle the bogus Content-Encoding Apache sometimes sends
    void ClearBogusContentEncodingIfNeeded();

    // byte range request specific methods
    nsresult ProcessPartialContent();
    nsresult OnDoneReadingPartialCacheEntry(bool *streamDone);

    nsresult DoAuthRetry(nsAHttpConnection *);

    void     HandleAsyncRedirectChannelToHttps();
    nsresult StartRedirectChannelToHttps();
    nsresult ContinueAsyncRedirectChannelToURI(nsresult rv);
    nsresult OpenRedirectChannel(nsresult rv);

    /**
     * A function that takes care of reading STS and PKP headers and enforcing
     * STS and PKP load rules. After a secure channel is erected, STS and PKP
     * requires the channel to be trusted or any STS or PKP header data on
     * the channel is ignored. This is called from ProcessResponse.
     */
    nsresult ProcessSecurityHeaders();

    /**
     * Taking care of the Content-Signature header and fail the channel if
     * the signature verification fails or is required but the header is not
     * present.
     * This sets mListener to ContentVerifier, which buffers the entire response
     * before verifying the Content-Signature header. If the verification is
     * successful, the load proceeds as usual. If the verification fails, a
     * NS_ERROR_INVALID_SIGNATURE is thrown and a fallback loaded in nsDocShell
     */
    nsresult ProcessContentSignatureHeader(nsHttpResponseHead *aResponseHead);

    /**
     * A function that will, if the feature is enabled, send security reports.
     */
    void ProcessSecurityReport(nsresult status);

    /**
     * A function to process a single security header (STS or PKP), assumes
     * some basic sanity checks have been applied to the channel. Called
     * from ProcessSecurityHeaders.
     */
    nsresult ProcessSingleSecurityHeader(uint32_t aType,
                                         nsISSLStatus *aSSLStatus,
                                         uint32_t aFlags);

    void InvalidateCacheEntryForLocation(const char *location);
    void AssembleCacheKey(const char *spec, uint32_t postID, nsACString &key);
    nsresult CreateNewURI(const char *loc, nsIURI **newURI);
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

    // Create a aggregate set of the current notification callbacks
    // and ensure the transaction is updated to use it.
    void UpdateAggregateCallbacks();

    static bool HasQueryString(nsHttpRequestHead::ParsedMethodType method, nsIURI * uri);
    bool ResponseWouldVary(nsICacheEntry* entry);
    bool IsResumable(int64_t partialLen, int64_t contentLength,
                     bool ignoreMissingPartialLen = false) const;
    nsresult MaybeSetupByteRangeRequest(int64_t partialLen, int64_t contentLength,
                                        bool ignoreMissingPartialLen = false);
    nsresult SetupByteRangeRequest(int64_t partialLen);
    void UntieByteRangeRequest();
    void UntieValidationRequest();
    nsresult OpenCacheInputStream(nsICacheEntry* cacheEntry, bool startBuffering,
                                  bool checkingAppCacheEntry);

    void SetPushedStream(Http2PushedStream *stream);

    void MaybeWarnAboutAppCache();

    void SetLoadGroupUserAgentOverride();

private:
    nsCOMPtr<nsICancelable>           mProxyRequest;

    RefPtr<nsInputStreamPump>       mTransactionPump;
    RefPtr<nsHttpTransaction>       mTransaction;

    uint64_t                          mLogicalOffset;

    // cache specific data
    nsCOMPtr<nsICacheEntry>           mCacheEntry;
    // We must close mCacheInputStream explicitly to avoid leaks.
    AutoClose<nsIInputStream>         mCacheInputStream;
    RefPtr<nsInputStreamPump>       mCachePump;
    nsAutoPtr<nsHttpResponseHead>     mCachedResponseHead;
    nsCOMPtr<nsISupports>             mCachedSecurityInfo;
    uint32_t                          mPostID;
    uint32_t                          mRequestTime;

    nsCOMPtr<nsICacheEntry> mOfflineCacheEntry;
    uint32_t                          mOfflineCacheLastModifiedTime;
    nsCOMPtr<nsIApplicationCache>     mApplicationCacheForWrite;

    // auth specific data
    nsCOMPtr<nsIHttpChannelAuthProvider> mAuthProvider;

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

    nsCOMPtr<nsIURI>                  mRedirectURI;
    nsCOMPtr<nsIChannel>              mRedirectChannel;
    uint32_t                          mRedirectType;

    static const uint32_t WAIT_FOR_CACHE_ENTRY = 1;
    static const uint32_t WAIT_FOR_OFFLINE_CACHE_ENTRY = 2;

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
    uint32_t                          mConcurentCacheAccess : 1;
    // whether the request is setup be byte-range
    uint32_t                          mIsPartialRequest : 1;
    // true iff there is AutoRedirectVetoNotifier on the stack
    uint32_t                          mHasAutoRedirectVetoNotifier : 1;
    // consumers set this to true to use cache pinning, this has effect
    // only when the channel is in an app context (load context has an appid)
    uint32_t                          mPinCacheContent : 1;
    // Whether fetching the content is meant to be handled by the
    // packaged app service, which behaves like a caching layer.
    // Upon successfully fetching the package, the resource will be placed in
    // the cache, and served by calling OnCacheEntryAvailable.
    uint32_t                          mIsPackagedAppResource : 1;
    // True if CORS preflight has been performed
    uint32_t                          mIsCorsPreflightDone : 1;

    nsCOMPtr<nsIChannel>              mPreflightChannel;

    nsTArray<nsContinueRedirectionFunc> mRedirectFuncStack;

    // Needed for accurate DNS timing
    RefPtr<nsDNSPrefetch>           mDNSPrefetch;

    Http2PushedStream                 *mPushedStream;
    // True if the channel's principal was found on a phishing, malware, or
    // tracking (if tracking protection is enabled) blocklist
    bool                              mLocalBlocklist;

    nsresult WaitForRedirectCallback();
    void PushRedirectAsyncFunc(nsContinueRedirectionFunc func);
    void PopRedirectAsyncFunc(nsContinueRedirectionFunc func);

    nsCString mUsername;

    // If non-null, warnings should be reported to this object.
    HttpChannelSecurityWarningReporter* mWarningReporter;

    RefPtr<ADivertableParentChannel> mParentChannel;
protected:
    virtual void DoNotifyListenerCleanup() override;

private: // cache telemetry
    bool mDidReval;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsHttpChannel, NS_HTTPCHANNEL_IID)
} // namespace net
} // namespace mozilla

#endif // nsHttpChannel_h__
