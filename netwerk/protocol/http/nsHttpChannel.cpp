/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 sw=4 sts=4 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHttp.h"
#include "nsHttpChannel.h"
#include "nsHttpHandler.h"
#include "nsStandardURL.h"
#include "nsIApplicationCacheService.h"
#include "nsIApplicationCacheContainer.h"
#include "nsIAuthInformation.h"
#include "nsICryptoHash.h"
#include "nsIStringBundle.h"
#include "nsIIDNService.h"
#include "nsIStreamListenerTee.h"
#include "nsISeekableStream.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "prprf.h"
#include "prnetdb.h"
#include "nsEscape.h"
#include "nsStreamUtils.h"
#include "nsIOService.h"
#include "nsICacheService.h"
#include "nsDNSPrefetch.h"
#include "nsChannelClassifier.h"
#include "nsIRedirectResultListener.h"
#include "mozilla/TimeStamp.h"
#include "nsError.h"
#include "nsAlgorithm.h"
#include "sampler.h"
#include "nsIConsoleService.h"
#include "base/compiler_specific.h"
#include "NullHttpTransaction.h"
#include "mozilla/Attributes.h"

namespace mozilla { namespace net {
 
namespace {

// Device IDs for various cache types
const char kDiskDeviceID[] = "disk";
const char kMemoryDeviceID[] = "memory";
const char kOfflineDeviceID[] = "offline";

// True if the local cache should be bypassed when processing a request.
#define BYPASS_LOCAL_CACHE(loadFlags) \
        (loadFlags & (nsIRequest::LOAD_BYPASS_CACHE | \
                      nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE))

static NS_DEFINE_CID(kStreamListenerTeeCID, NS_STREAMLISTENERTEE_CID);
static NS_DEFINE_CID(kStreamTransportServiceCID,
                     NS_STREAMTRANSPORTSERVICE_CID);

enum CacheDisposition {
    kCacheHit = 1,
    kCacheHitViaReval = 2,
    kCacheMissedViaReval = 3,
    kCacheMissed = 4
};

const Telemetry::ID UNKNOWN_DEVICE = static_cast<Telemetry::ID>(0);
void
AccumulateCacheHitTelemetry(Telemetry::ID deviceHistogram,
                            CacheDisposition hitOrMiss)
{
    // If we had a cache hit, or we revalidated an entry, then we should know
    // the device for the entry already. But, sometimes this assertion fails!
    // (Bug 769497).
    // MOZ_ASSERT(deviceHistogram != UNKNOWN_DEVICE || hitOrMiss == kCacheMissed);

    Telemetry::Accumulate(Telemetry::HTTP_CACHE_DISPOSITION_2, hitOrMiss);
    if (deviceHistogram != UNKNOWN_DEVICE) {
        Telemetry::Accumulate(deviceHistogram, hitOrMiss);
    }
}

// Computes and returns a SHA1 hash of the input buffer. The input buffer
// must be a null-terminated string.
nsresult
Hash(const char *buf, nsACString &hash)
{
    nsresult rv;
      
    nsCOMPtr<nsICryptoHash> hasher
      = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = hasher->Init(nsICryptoHash::SHA1);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = hasher->Update(reinterpret_cast<unsigned const char*>(buf),
                         strlen(buf));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = hasher->Finish(true, hash);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

bool IsRedirectStatus(uint32_t status)
{
    // 305 disabled as a security measure (see bug 187996).
    return status == 300 || status == 301 || status == 302 || status == 303 ||
           status == 307 || status == 308;
}

// We only treat 3xx responses as redirects if they have a Location header and
// the status code is in a whitelist.
bool
WillRedirect(const nsHttpResponseHead * response)
{
    return IsRedirectStatus(response->Status()) &&
           response->PeekHeader(nsHttp::Location);
}

void
MaybeMarkCacheEntryValid(const void * channel,
                         nsICacheEntryDescriptor * cacheEntry,
                         nsCacheAccessMode cacheAccess)
{
    // Mark the cache entry as valid in order to allow others access to it.
    // XXX: Is it really necessary to check for write acccess to the entry?
    if (cacheAccess & nsICache::ACCESS_WRITE) {
        nsresult rv = cacheEntry->MarkValid();
        LOG(("Marking cache entry valid "
             "[channel=%p, entry=%p, access=%d, result=%d]",
             channel, cacheEntry, int(cacheAccess), int(rv)));
    } else {
        LOG(("Not marking read-only cache entry valid "
             "[channel=%p, entry=%p, access=%d]", 
             channel, cacheEntry, int(cacheAccess)));
    }
}

} // unnamed namespace

class AutoRedirectVetoNotifier
{
public:
    AutoRedirectVetoNotifier(nsHttpChannel* channel) : mChannel(channel) {}
    ~AutoRedirectVetoNotifier() {ReportRedirectResult(false);}
    void RedirectSucceeded() {ReportRedirectResult(true);}

private:
    nsHttpChannel* mChannel;
    void ReportRedirectResult(bool succeeded);
};

void
AutoRedirectVetoNotifier::ReportRedirectResult(bool succeeded)
{
    if (!mChannel)
        return;

    mChannel->mRedirectChannel = nullptr;

    nsCOMPtr<nsIRedirectResultListener> vetoHook;
    NS_QueryNotificationCallbacks(mChannel, 
                                  NS_GET_IID(nsIRedirectResultListener), 
                                  getter_AddRefs(vetoHook));
    mChannel = nullptr;
    if (vetoHook)
        vetoHook->OnRedirectResult(succeeded);
}

class HttpCacheQuery : public nsRunnable, public nsICacheListener
{
public:
    HttpCacheQuery(nsHttpChannel * channel,
                   const nsACString & clientID,
                   nsCacheStoragePolicy storagePolicy,
                   bool usingPrivateBrowsing,
                   const nsACString & cacheKey,
                   nsCacheAccessMode accessToRequest,
                   bool noWait,
                   bool usingSSL,
                   bool loadedFromApplicationCache)
        // in
        : mChannel(channel)
        , mHasQueryString(HasQueryString(channel->mRequestHead.Method(),
                                         channel->mURI))
        , mLoadFlags(channel->mLoadFlags)
        , mCacheForOfflineUse(!!channel->mApplicationCacheForWrite)
        , mFallbackChannel(channel->mFallbackChannel)
        , mClientID(clientID)
        , mStoragePolicy(storagePolicy)
        , mUsingPrivateBrowsing(usingPrivateBrowsing)
        , mCacheKey(cacheKey)
        , mAccessToRequest(accessToRequest)
        , mNoWait(noWait)
        , mUsingSSL(usingSSL)
        , mLoadedFromApplicationCache(loadedFromApplicationCache)
        // internal
        , mCacheAccess(0)
        , mStatus(NS_ERROR_NOT_INITIALIZED)
        , mRunCount(0)
        // in/out
        , mRequestHead(channel->mRequestHead)
        , mRedirectedCachekeys(channel->mRedirectedCachekeys.forget())
        // out
        , mCachedContentIsValid(false)
        , mCachedContentIsPartial(false)
        , mCustomConditionalRequest(false)
        , mDidReval(false)
        , mCacheEntryDeviceTelemetryID(UNKNOWN_DEVICE)
    {
        MOZ_ASSERT(NS_IsMainThread());
    }

    nsresult Dispatch();

private:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIRUNNABLE
    NS_DECL_NSICACHELISTENER

    MOZ_ALWAYS_INLINE void AssertOnCacheThread() const
    {
        MOZ_ASSERT(mCacheThread);
#ifdef DEBUG
        bool onCacheThread;
        nsresult rv = mCacheThread->IsOnCurrentThread(&onCacheThread);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
        MOZ_ASSERT(onCacheThread);
#endif
    }

    static bool HasQueryString(nsHttpAtom method, nsIURI * uri);
    nsresult CheckCache();
    bool ResponseWouldVary() const;
    bool MustValidateBasedOnQueryUrl() const;
    nsresult SetupByteRangeRequest(uint32_t partialLen);
    nsresult OpenCacheInputStream(bool startBuffering);

    nsCOMPtr<nsICacheListener> mChannel;
    const bool mHasQueryString;
    const uint32_t mLoadFlags;
    const bool mCacheForOfflineUse;
    const bool mFallbackChannel;
    const InfallableCopyCString mClientID;
    const nsCacheStoragePolicy mStoragePolicy;
    const bool mUsingPrivateBrowsing;
    const InfallableCopyCString mCacheKey;
    const nsCacheAccessMode mAccessToRequest;
    const bool mNoWait;
    const bool mUsingSSL;
    const bool mLoadedFromApplicationCache;

    // Used only internally 
    nsCOMPtr<nsIEventTarget> mCacheThread;
    nsCOMPtr<nsICacheEntryDescriptor> mCacheEntry;
    nsCacheAccessMode mCacheAccess;
    nsresult mStatus;
    uint32_t mRunCount;

    // Copied from HttpcacheQuery into nsHttpChannel by nsHttpChannel
    friend class nsHttpChannel;
    /*in/out*/ nsHttpRequestHead mRequestHead;
    /*in/out*/ nsAutoPtr<nsTArray<nsCString> > mRedirectedCachekeys;
    /*out*/ AutoClose<nsIInputStream> mCacheInputStream;
    /*out*/ nsAutoPtr<nsHttpResponseHead> mCachedResponseHead;
    /*out*/ nsCOMPtr<nsISupports> mCachedSecurityInfo;
    /*out*/ bool mCachedContentIsValid;
    /*out*/ bool mCachedContentIsPartial;
    /*out*/ bool mCustomConditionalRequest;
    /*out*/ bool mDidReval;
    /*out*/ Telemetry::ID mCacheEntryDeviceTelemetryID;
};

NS_IMPL_ISUPPORTS_INHERITED1(HttpCacheQuery, nsRunnable, nsICacheListener)

//-----------------------------------------------------------------------------
// nsHttpChannel <public>
//-----------------------------------------------------------------------------

nsHttpChannel::nsHttpChannel()
    : ALLOW_THIS_IN_INITIALIZER_LIST(HttpAsyncAborter<nsHttpChannel>(this))
    , mLogicalOffset(0)
    , mCacheAccess(0)
    , mCacheEntryDeviceTelemetryID(UNKNOWN_DEVICE)
    , mPostID(0)
    , mRequestTime(0)
    , mOnCacheEntryAvailableCallback(nullptr)
    , mOfflineCacheAccess(0)
    , mOfflineCacheLastModifiedTime(0)
    , mCachedContentIsValid(false)
    , mCachedContentIsPartial(false)
    , mTransactionReplaced(false)
    , mAuthRetryPending(false)
    , mProxyAuthPending(false)
    , mResuming(false)
    , mInitedCacheEntry(false)
    , mFallbackChannel(false)
    , mCustomConditionalRequest(false)
    , mFallingBack(false)
    , mWaitingForRedirectCallback(false)
    , mRequestTimeInitialized(false)
    , mDidReval(false)
{
    LOG(("Creating nsHttpChannel [this=%p]\n", this));
    mChannelCreationTime = PR_Now();
    mChannelCreationTimestamp = TimeStamp::Now();
}

nsHttpChannel::~nsHttpChannel()
{
    LOG(("Destroying nsHttpChannel [this=%p]\n", this));

    if (mAuthProvider)
        mAuthProvider->Disconnect(NS_ERROR_ABORT);
}

nsresult
nsHttpChannel::Init(nsIURI *uri,
                    uint32_t caps,
                    nsProxyInfo *proxyInfo,
                    uint32_t proxyResolveFlags,
                    nsIURI *proxyURI)
{
    nsresult rv = HttpBaseChannel::Init(uri, caps, proxyInfo,
                                        proxyResolveFlags, proxyURI);
    if (NS_FAILED(rv))
        return rv;

    LOG(("nsHttpChannel::Init [this=%p]\n", this));

    return rv;
}
//-----------------------------------------------------------------------------
// nsHttpChannel <private>
//-----------------------------------------------------------------------------

nsresult
nsHttpChannel::Connect()
{
    nsresult rv;

    LOG(("nsHttpChannel::Connect [this=%p]\n", this));

    // Even if we're in private browsing mode, we still enforce existing STS
    // data (it is read-only).
    // if the connection is not using SSL and either the exact host matches or
    // a superdomain wants to force HTTPS, do it.
    bool usingSSL = false;
    rv = mURI->SchemeIs("https", &usingSSL);
    NS_ENSURE_SUCCESS(rv,rv);

    if (!usingSSL) {
        // enforce Strict-Transport-Security
        nsIStrictTransportSecurityService* stss = gHttpHandler->GetSTSService();
        NS_ENSURE_TRUE(stss, NS_ERROR_OUT_OF_MEMORY);

        bool isStsHost = false;
        uint32_t flags = mPrivateBrowsing ? nsISocketProvider::NO_PERMANENT_STORAGE : 0;
        rv = stss->IsStsURI(mURI, flags, &isStsHost);

        // if STS fails, there's no reason to cancel the load, but it's
        // worrisome.
        NS_ASSERTION(NS_SUCCEEDED(rv),
                     "Something is wrong with STS: IsStsURI failed.");

        if (NS_SUCCEEDED(rv) && isStsHost) {
            LOG(("nsHttpChannel::Connect() STS permissions found\n"));
            return AsyncCall(&nsHttpChannel::HandleAsyncRedirectChannelToHttps);
        }

        // Check for a previous SPDY Alternate-Protocol directive
        if (gHttpHandler->IsSpdyEnabled() && mAllowSpdy) {
            nsAutoCString hostPort;

            if (NS_SUCCEEDED(mURI->GetHostPort(hostPort)) &&
                gHttpHandler->ConnMgr()->GetSpdyAlternateProtocol(hostPort)) {
                LOG(("nsHttpChannel::Connect() Alternate-Protocol found\n"));
                return AsyncCall(
                    &nsHttpChannel::HandleAsyncRedirectChannelToHttps);
            }
        }
    }

    // ensure that we are using a valid hostname
    if (!net_IsValidHostName(nsDependentCString(mConnectionInfo->Host())))
        return NS_ERROR_UNKNOWN_HOST;

    // Consider opening a TCP connection right away
    SpeculativeConnect();

    // Don't allow resuming when cache must be used
    if (mResuming && (mLoadFlags & LOAD_ONLY_FROM_CACHE)) {
        LOG(("Resuming from cache is not supported yet"));
        return NS_ERROR_DOCUMENT_NOT_CACHED;
    }

    if (!gHttpHandler->UseCache())
        return ContinueConnect();

    // open a cache entry for this channel...
    rv = OpenCacheEntry(usingSSL);

    // do not continue if asyncOpenCacheEntry is in progress
    if (mOnCacheEntryAvailableCallback) {
        NS_ASSERTION(NS_SUCCEEDED(rv), "Unexpected state");
        return NS_OK;
    }

    if (NS_FAILED(rv)) {
        LOG(("OpenCacheEntry failed [rv=%x]\n", rv));
        // if this channel is only allowed to pull from the cache, then
        // we must fail if we were unable to open a cache entry.
        if (mLoadFlags & LOAD_ONLY_FROM_CACHE) {
            // If we have a fallback URI (and we're not already
            // falling back), process the fallback asynchronously.
            if (!mFallbackChannel && !mFallbackKey.IsEmpty()) {
                return AsyncCall(&nsHttpChannel::HandleAsyncFallback);
            }
            return NS_ERROR_DOCUMENT_NOT_CACHED;
        }
        // otherwise, let's just proceed without using the cache.
    }

    // if cacheForOfflineUse has been set, open up an offline cache
    // entry to update
    if (mApplicationCacheForWrite) {
        rv = OpenOfflineCacheEntryForWriting();
        if (NS_FAILED(rv)) return rv;

        if (mOnCacheEntryAvailableCallback)
            return NS_OK;
    }

    return ContinueConnect();
}

nsresult
nsHttpChannel::ContinueConnect()
{
    // we may or may not have a cache entry at this point
    if (mCacheEntry) {
        // read straight from the cache if possible...
        if (mCachedContentIsValid) {
            nsRunnableMethod<nsHttpChannel> *event = nullptr;
            if (!mCachedContentIsPartial) {
                AsyncCall(&nsHttpChannel::AsyncOnExamineCachedResponse, &event);
            }
            nsresult rv = ReadFromCache(true);
            if (NS_FAILED(rv) && event) {
                event->Revoke();
            }

            AccumulateCacheHitTelemetry(mCacheEntryDeviceTelemetryID,
                                        kCacheHit);

            return rv;
        }
        else if (mLoadFlags & LOAD_ONLY_FROM_CACHE) {
            // the cache contains the requested resource, but it must be 
            // validated before we can reuse it.  since we are not allowed
            // to hit the net, there's nothing more to do.  the document
            // is effectively not in the cache.
            return NS_ERROR_DOCUMENT_NOT_CACHED;
        }
    }
    else if (mLoadFlags & LOAD_ONLY_FROM_CACHE) {
        // If we have a fallback URI (and we're not already
        // falling back), process the fallback asynchronously.
        if (!mFallbackChannel && !mFallbackKey.IsEmpty()) {
            return AsyncCall(&nsHttpChannel::HandleAsyncFallback);
        }
        return NS_ERROR_DOCUMENT_NOT_CACHED;
    }

    if (mLoadFlags & LOAD_NO_NETWORK_IO) {
        return NS_ERROR_DOCUMENT_NOT_CACHED;
    }

    // hit the net...
    nsresult rv = SetupTransaction();
    if (NS_FAILED(rv)) return rv;

    rv = gHttpHandler->InitiateTransaction(mTransaction, mPriority);
    if (NS_FAILED(rv)) return rv;

    rv = mTransactionPump->AsyncRead(this, nullptr);
    if (NS_FAILED(rv)) return rv;

    uint32_t suspendCount = mSuspendCount;
    while (suspendCount--)
        mTransactionPump->Suspend();

    return NS_OK;
}

void
nsHttpChannel::SpeculativeConnect()
{
    // Before we take the latency hit of dealing with the cache, try and
    // get the TCP (and SSL) handshakes going so they can overlap.

    // don't speculate on uses of the offline application cache,
    // if we are offline, when doing http upgrade (i.e. websockets bootstrap),
    // or if we can't do keep-alive (because then we couldn't reuse
    // the speculative connection anyhow).
    if (mApplicationCache || gIOService->IsOffline() || 
        mUpgradeProtocolCallback || !(mCaps & NS_HTTP_ALLOW_KEEPALIVE))
        return;

    // LOAD_ONLY_FROM_CACHE and LOAD_NO_NETWORK_IO must not hit network.
    // LOAD_FROM_CACHE and LOAD_CHECK_OFFLINE_CACHE are unlikely to hit network,
    // so skip preconnects for them.
    if (mLoadFlags & (LOAD_ONLY_FROM_CACHE | LOAD_FROM_CACHE |
                      LOAD_NO_NETWORK_IO | LOAD_CHECK_OFFLINE_CACHE))
        return;
    
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    NS_NewNotificationCallbacksAggregation(mCallbacks, mLoadGroup,
                                           getter_AddRefs(callbacks));
    if (!callbacks)
        return;

    mConnectionInfo->SetAnonymous((mLoadFlags & LOAD_ANONYMOUS) != 0);
    mConnectionInfo->SetPrivate(mPrivateBrowsing);
    gHttpHandler->SpeculativeConnect(mConnectionInfo,
                                     callbacks);
}

void
nsHttpChannel::DoNotifyListenerCleanup()
{
    // We don't need this info anymore
    CleanRedirectCacheChainIfNecessary();
}

void
nsHttpChannel::HandleAsyncRedirect()
{
    NS_PRECONDITION(!mCallOnResume, "How did that happen?");
    
    if (mSuspendCount) {
        LOG(("Waiting until resume to do async redirect [this=%p]\n", this));
        mCallOnResume = &nsHttpChannel::HandleAsyncRedirect;
        return;
    }

    nsresult rv = NS_OK;

    LOG(("nsHttpChannel::HandleAsyncRedirect [this=%p]\n", this));

    // since this event is handled asynchronously, it is possible that this
    // channel could have been canceled, in which case there would be no point
    // in processing the redirect.
    if (NS_SUCCEEDED(mStatus)) {
        PushRedirectAsyncFunc(&nsHttpChannel::ContinueHandleAsyncRedirect);
        rv = AsyncProcessRedirection(mResponseHead->Status());
        if (NS_FAILED(rv)) {
            PopRedirectAsyncFunc(&nsHttpChannel::ContinueHandleAsyncRedirect);
            // TODO: if !DoNotRender3xxBody(), render redirect body instead.
            // But first we need to cache 3xx bodies (bug 748510)
            ContinueHandleAsyncRedirect(rv);
        }
    }
    else {
        ContinueHandleAsyncRedirect(NS_OK);
    }
}

nsresult
nsHttpChannel::ContinueHandleAsyncRedirect(nsresult rv)
{
    if (NS_FAILED(rv)) {
        // If AsyncProcessRedirection fails, then we have to send out the
        // OnStart/OnStop notifications.
        LOG(("ContinueHandleAsyncRedirect got failure result [rv=%x]\n", rv));
        mStatus = rv;
        DoNotifyListener();
    }

    // close the cache entry.  Blow it away if we couldn't process the redirect
    // for some reason (the cache entry might be corrupt).
    if (mCacheEntry) {
        if (NS_FAILED(rv))
            mCacheEntry->AsyncDoom(nullptr);
    }
    CloseCacheEntry(false);

    mIsPending = false;

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nullptr, mStatus);

    return NS_OK;
}

void
nsHttpChannel::HandleAsyncNotModified()
{
    NS_PRECONDITION(!mCallOnResume, "How did that happen?");
    
    if (mSuspendCount) {
        LOG(("Waiting until resume to do async not-modified [this=%p]\n",
             this));
        mCallOnResume = &nsHttpChannel::HandleAsyncNotModified;
        return;
    }
    
    LOG(("nsHttpChannel::HandleAsyncNotModified [this=%p]\n", this));

    DoNotifyListener();

    CloseCacheEntry(true);

    mIsPending = false;

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nullptr, mStatus);
}

void
nsHttpChannel::HandleAsyncFallback()
{
    NS_PRECONDITION(!mCallOnResume, "How did that happen?");

    if (mSuspendCount) {
        LOG(("Waiting until resume to do async fallback [this=%p]\n", this));
        mCallOnResume = &nsHttpChannel::HandleAsyncFallback;
        return;
    }

    nsresult rv = NS_OK;

    LOG(("nsHttpChannel::HandleAsyncFallback [this=%p]\n", this));

    // since this event is handled asynchronously, it is possible that this
    // channel could have been canceled, in which case there would be no point
    // in processing the fallback.
    if (!mCanceled) {
        PushRedirectAsyncFunc(&nsHttpChannel::ContinueHandleAsyncFallback);
        bool waitingForRedirectCallback;
        rv = ProcessFallback(&waitingForRedirectCallback);
        if (waitingForRedirectCallback)
            return;
        PopRedirectAsyncFunc(&nsHttpChannel::ContinueHandleAsyncFallback);
    }

    ContinueHandleAsyncFallback(rv);
}

nsresult
nsHttpChannel::ContinueHandleAsyncFallback(nsresult rv)
{
    if (!mCanceled && (NS_FAILED(rv) || !mFallingBack)) {
        // If ProcessFallback fails, then we have to send out the
        // OnStart/OnStop notifications.
        LOG(("ProcessFallback failed [rv=%x, %d]\n", rv, mFallingBack));
        mStatus = NS_FAILED(rv) ? rv : NS_ERROR_DOCUMENT_NOT_CACHED;
        DoNotifyListener();
    }

    mIsPending = false;

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nullptr, mStatus);

    return rv;
}

void
nsHttpChannel::SetupTransactionLoadGroupInfo()
{
    // Find the loadgroup at the end of the chain in order
    // to make sure all channels derived from the load group
    // use the same connection scope.
    nsCOMPtr<nsILoadGroup> rootLoadGroup = mLoadGroup;
    while (rootLoadGroup) {
        nsCOMPtr<nsILoadGroup> tmp;
        rootLoadGroup->GetLoadGroup(getter_AddRefs(tmp));
        if (tmp)
            rootLoadGroup.swap(tmp);
        else
            break;
    }

    // Set the load group connection scope on the transaction
    if (rootLoadGroup) {
        nsCOMPtr<nsILoadGroupConnectionInfo> ci;
        rootLoadGroup->GetConnectionInfo(getter_AddRefs(ci));
        if (ci)
            mTransaction->SetLoadGroupConnectionInfo(ci);
    }
}

nsresult
nsHttpChannel::SetupTransaction()
{
    LOG(("nsHttpChannel::SetupTransaction [this=%p]\n", this));

    NS_ENSURE_TRUE(!mTransaction, NS_ERROR_ALREADY_INITIALIZED);

    nsresult rv;

    if (mCaps & NS_HTTP_ALLOW_PIPELINING) {
        //
        // disable pipelining if:
        //   (1) pipelining has been disabled by config
        //   (2) pipelining has been disabled by connection mgr info
        //   (3) request corresponds to a top-level document load (link click)
        //   (4) request method is non-idempotent
        //   (5) request is marked slow (e.g XHR)
        //
        if (!mAllowPipelining ||
           (mLoadFlags & (LOAD_INITIAL_DOCUMENT_URI | INHIBIT_PIPELINE)) ||
            !(mRequestHead.Method() == nsHttp::Get ||
              mRequestHead.Method() == nsHttp::Head ||
              mRequestHead.Method() == nsHttp::Options ||
              mRequestHead.Method() == nsHttp::Propfind ||
              mRequestHead.Method() == nsHttp::Proppatch)) {
            LOG(("  pipelining disallowed\n"));
            mCaps &= ~NS_HTTP_ALLOW_PIPELINING;
        }
    }

    if (!mAllowSpdy)
        mCaps |= NS_HTTP_DISALLOW_SPDY;

    // Use the URI path if not proxying (transparent proxying such as proxy
    // CONNECT does not count here). Also figure out what HTTP version to use.
    nsAutoCString buf, path;
    nsCString* requestURI;
    if (mConnectionInfo->UsingConnect() ||
        !mConnectionInfo->UsingHttpProxy()) {
        rv = mURI->GetPath(path);
        if (NS_FAILED(rv)) return rv;
        // path may contain UTF-8 characters, so ensure that they're escaped.
        if (NS_EscapeURL(path.get(), path.Length(), esc_OnlyNonASCII, buf))
            requestURI = &buf;
        else
            requestURI = &path;
        mRequestHead.SetVersion(gHttpHandler->HttpVersion());
    }
    else {
        rv = mURI->GetUserPass(buf);
        if (NS_FAILED(rv)) return rv;
        if (!buf.IsEmpty() && ((strncmp(mSpec.get(), "http:", 5) == 0) ||
                                strncmp(mSpec.get(), "https:", 6) == 0)) {
            nsCOMPtr<nsIURI> tempURI;
            rv = mURI->Clone(getter_AddRefs(tempURI));
            if (NS_FAILED(rv)) return rv;
            rv = tempURI->SetUserPass(EmptyCString());
            if (NS_FAILED(rv)) return rv;
            rv = tempURI->GetAsciiSpec(path);
            if (NS_FAILED(rv)) return rv;
            requestURI = &path;
        }
        else
            requestURI = &mSpec;
        mRequestHead.SetVersion(gHttpHandler->ProxyHttpVersion());
    }

    // trim off the #ref portion if any...
    int32_t ref = requestURI->FindChar('#');
    if (ref != kNotFound)
        requestURI->SetLength(ref);

    mRequestHead.SetRequestURI(*requestURI);

    // set the request time for cache expiration calculations
    mRequestTime = NowInSeconds();
    mRequestTimeInitialized = true;

    // if doing a reload, force end-to-end
    if (mLoadFlags & LOAD_BYPASS_CACHE) {
        // We need to send 'Pragma:no-cache' to inhibit proxy caching even if
        // no proxy is configured since we might be talking with a transparent
        // proxy, i.e. one that operates at the network level.  See bug #14772.
        mRequestHead.SetHeaderOnce(nsHttp::Pragma, "no-cache", true);
        // If we're configured to speak HTTP/1.1 then also send 'Cache-control:
        // no-cache'
        if (mRequestHead.Version() >= NS_HTTP_VERSION_1_1)
            mRequestHead.SetHeaderOnce(nsHttp::Cache_Control, "no-cache", true);
    }
    else if ((mLoadFlags & VALIDATE_ALWAYS) && (mCacheAccess & nsICache::ACCESS_READ)) {
        // We need to send 'Cache-Control: max-age=0' to force each cache along
        // the path to the origin server to revalidate its own entry, if any,
        // with the next cache or server.  See bug #84847.
        //
        // If we're configured to speak HTTP/1.0 then just send 'Pragma: no-cache'
        if (mRequestHead.Version() >= NS_HTTP_VERSION_1_1)
            mRequestHead.SetHeaderOnce(nsHttp::Cache_Control, "max-age=0", true);
        else
            mRequestHead.SetHeaderOnce(nsHttp::Pragma, "no-cache", true);
    }

    if (mResuming) {
        char byteRange[32];
        PR_snprintf(byteRange, sizeof(byteRange), "bytes=%llu-", mStartPos);
        mRequestHead.SetHeader(nsHttp::Range, nsDependentCString(byteRange));

        if (!mEntityID.IsEmpty()) {
            // Also, we want an error if this resource changed in the meantime
            // Format of the entity id is: escaped_etag/size/lastmod
            nsCString::const_iterator start, end, slash;
            mEntityID.BeginReading(start);
            mEntityID.EndReading(end);
            mEntityID.BeginReading(slash);

            if (FindCharInReadable('/', slash, end)) {
                nsAutoCString ifMatch;
                mRequestHead.SetHeader(nsHttp::If_Match,
                        NS_UnescapeURL(Substring(start, slash), 0, ifMatch));

                ++slash; // Incrementing, so that searching for '/' won't find
                         // the same slash again
            }

            if (FindCharInReadable('/', slash, end)) {
                mRequestHead.SetHeader(nsHttp::If_Unmodified_Since,
                        Substring(++slash, end));
            }
        }
    }

    // create wrapper for this channel's notification callbacks
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    NS_NewNotificationCallbacksAggregation(mCallbacks, mLoadGroup,
                                           getter_AddRefs(callbacks));
    if (!callbacks)
        return NS_ERROR_OUT_OF_MEMORY;

    // create the transaction object
    mTransaction = new nsHttpTransaction();
    if (!mTransaction)
        return NS_ERROR_OUT_OF_MEMORY;

    // See bug #466080. Transfer LOAD_ANONYMOUS flag to socket-layer.
    if (mLoadFlags & LOAD_ANONYMOUS)
        mCaps |= NS_HTTP_LOAD_ANONYMOUS;

    if (mTimingEnabled)
        mCaps |= NS_HTTP_TIMING_ENABLED;

    mConnectionInfo->SetAnonymous((mLoadFlags & LOAD_ANONYMOUS) != 0);
    mConnectionInfo->SetPrivate(mPrivateBrowsing);

    if (mUpgradeProtocolCallback) {
        mRequestHead.SetHeader(nsHttp::Upgrade, mUpgradeProtocol, false);
        mRequestHead.SetHeaderOnce(nsHttp::Connection,
                                   nsHttp::Upgrade.get(),
                                   true);
        mCaps |=  NS_HTTP_STICKY_CONNECTION;
        mCaps &= ~NS_HTTP_ALLOW_PIPELINING;
        mCaps &= ~NS_HTTP_ALLOW_KEEPALIVE;
        mCaps |=  NS_HTTP_DISALLOW_SPDY;
    }

    nsCOMPtr<nsIAsyncInputStream> responseStream;
    rv = mTransaction->Init(mCaps, mConnectionInfo, &mRequestHead,
                            mUploadStream, mUploadStreamHasHeaders,
                            NS_GetCurrentThread(), callbacks, this,
                            getter_AddRefs(responseStream));
    if (NS_FAILED(rv)) {
        mTransaction = nullptr;
        return rv;
    }

    SetupTransactionLoadGroupInfo();
    
    rv = nsInputStreamPump::Create(getter_AddRefs(mTransactionPump),
                                   responseStream);
    return rv;
}

// NOTE: This function duplicates code from nsBaseChannel. This will go away
// once HTTP uses nsBaseChannel (part of bug 312760)
static void
CallTypeSniffers(void *aClosure, const uint8_t *aData, uint32_t aCount)
{
  nsIChannel *chan = static_cast<nsIChannel*>(aClosure);

  nsAutoCString newType;
  NS_SniffContent(NS_CONTENT_SNIFFER_CATEGORY, chan, aData, aCount, newType);
  if (!newType.IsEmpty()) {
    chan->SetContentType(newType);
  }
}

nsresult
nsHttpChannel::CallOnStartRequest()
{
    mTracingEnabled = false;

    // Allow consumers to override our content type
    if (mLoadFlags & LOAD_CALL_CONTENT_SNIFFERS) {
        // NOTE: We can have both a txn pump and a cache pump when the cache
        // content is partial. In that case, we need to read from the cache,
        // because that's the one that has the initial contents. If that fails
        // then give the transaction pump a shot.

        nsIChannel* thisChannel = static_cast<nsIChannel*>(this);

        bool typeSniffersCalled = false;
        if (mCachePump) {
          typeSniffersCalled =
            NS_SUCCEEDED(mCachePump->PeekStream(CallTypeSniffers, thisChannel));
        }
        
        if (!typeSniffersCalled && mTransactionPump) {
          mTransactionPump->PeekStream(CallTypeSniffers, thisChannel);
        }
    }

    bool shouldSniff = mResponseHead && (mResponseHead->ContentType().IsEmpty() ||
        ((mResponseHead->ContentType().EqualsLiteral(APPLICATION_OCTET_STREAM) &&
        (mLoadFlags & LOAD_TREAT_APPLICATION_OCTET_STREAM_AS_UNKNOWN))));

    if (shouldSniff) {
        NS_ASSERTION(mConnectionInfo, "Should have connection info here");
        if (!mContentTypeHint.IsEmpty())
            mResponseHead->SetContentType(mContentTypeHint);
        else if (mResponseHead->Version() == NS_HTTP_VERSION_0_9 &&
                 mConnectionInfo->Port() != mConnectionInfo->DefaultPort())
            mResponseHead->SetContentType(NS_LITERAL_CSTRING(TEXT_PLAIN));
        else {
            // Uh-oh.  We had better find out what type we are!

            // XXX This does not work with content-encodings...  but
            // neither does applying the conversion from the URILoader

            nsCOMPtr<nsIStreamConverterService> serv;
            nsresult rv = gHttpHandler->
                GetStreamConverterService(getter_AddRefs(serv));
            // If we failed, we just fall through to the "normal" case
            if (NS_SUCCEEDED(rv)) {
                nsCOMPtr<nsIStreamListener> converter;
                rv = serv->AsyncConvertData(UNKNOWN_CONTENT_TYPE,
                                            "*/*",
                                            mListener,
                                            mListenerContext,
                                            getter_AddRefs(converter));
                if (NS_SUCCEEDED(rv)) {
                    mListener = converter;
                }
            }
        }
    }

    if (mResponseHead && mResponseHead->ContentCharset().IsEmpty())
        mResponseHead->SetContentCharset(mContentCharsetHint);

    if (mResponseHead && mCacheEntry) {
        // If we have a cache entry, set its predicted size to ContentLength to
        // avoid caching an entry that will exceed the max size limit.
        nsresult rv = mCacheEntry->SetPredictedDataSize(
            mResponseHead->ContentLength());
        NS_ENSURE_SUCCESS(rv, rv);
    }

    LOG(("  calling mListener->OnStartRequest\n"));
    nsresult rv = mListener->OnStartRequest(this, mListenerContext);
    if (NS_FAILED(rv)) return rv;

    // install stream converter if required
    rv = ApplyContentConversions();
    if (NS_FAILED(rv)) return rv;

    rv = EnsureAssocReq();
    if (NS_FAILED(rv))
        return rv;

    // if this channel is for a download, close off access to the cache.
    if (mCacheEntry && mChannelIsForDownload) {
        mCacheEntry->AsyncDoom(nullptr);
        CloseCacheEntry(false);
    }

    if (!mCanceled) {
        // create offline cache entry if offline caching was requested
        if (ShouldUpdateOfflineCacheEntry()) {
            LOG(("writing to the offline cache"));
            rv = InitOfflineCacheEntry();
            if (NS_FAILED(rv)) return rv;
            
            // InitOfflineCacheEntry may have closed mOfflineCacheEntry
            if (mOfflineCacheEntry) {
                rv = InstallOfflineCacheListener();
                if (NS_FAILED(rv)) return rv;
            }
        } else if (mApplicationCacheForWrite) {
            LOG(("offline cache is up to date, not updating"));
            CloseOfflineCacheEntry();
        }
    }

    return NS_OK;
}

nsresult
nsHttpChannel::ProcessFailedProxyConnect(uint32_t httpStatus)
{
    // Failure to set up a proxy tunnel via CONNECT means one of the following:
    // 1) Proxy wants authorization, or forbids.
    // 2) DNS at proxy couldn't resolve target URL.
    // 3) Proxy connection to target failed or timed out.
    // 4) Eve intercepted our CONNECT, and is replying with malicious HTML.
    //
    // Our current architecture would parse the proxy's response content with
    // the permission of the target URL.  Given #4, we must avoid rendering the
    // body of the reply, and instead give the user a (hopefully helpful)
    // boilerplate error page, based on just the HTTP status of the reply.

    NS_ABORT_IF_FALSE(mConnectionInfo->UsingConnect(),
                      "proxy connect failed but not using CONNECT?");
    nsresult rv;
    switch (httpStatus) 
    {
    case 300: case 301: case 302: case 303: case 307: case 308:
        // Bad redirect: not top-level, or it's a POST, bad/missing Location,
        // or ProcessRedirect() failed for some other reason.  Legal
        // redirects that fail because site not available, etc., are handled
        // elsewhere, in the regular codepath.
        rv = NS_ERROR_CONNECTION_REFUSED;
        break;
    case 403: // HTTP/1.1: "Forbidden"
    case 407: // ProcessAuthentication() failed
    case 501: // HTTP/1.1: "Not Implemented"
        // user sees boilerplate Mozilla "Proxy Refused Connection" page.
        rv = NS_ERROR_PROXY_CONNECTION_REFUSED; 
        break;
    // Squid sends 404 if DNS fails (regular 404 from target is tunneled)
    case 404: // HTTP/1.1: "Not Found"
    // RFC 2616: "some deployed proxies are known to return 400 or 500 when
    // DNS lookups time out."  (Squid uses 500 if it runs out of sockets: so
    // we have a conflict here).
    case 400: // HTTP/1.1 "Bad Request"
    case 500: // HTTP/1.1: "Internal Server Error"
        /* User sees: "Address Not Found: Firefox can't find the server at
         * www.foo.com."
         */
        rv = NS_ERROR_UNKNOWN_HOST; 
        break;
    case 502: // HTTP/1.1: "Bad Gateway" (invalid resp from target server)
    // Squid returns 503 if target request fails for anything but DNS.
    case 503: // HTTP/1.1: "Service Unavailable"
        /* User sees: "Failed to Connect:
         *  Firefox can't establish a connection to the server at
         *  www.foo.com.  Though the site seems valid, the browser
         *  was unable to establish a connection."
         */
        rv = NS_ERROR_CONNECTION_REFUSED;
        break;
    // RFC 2616 uses 504 for both DNS and target timeout, so not clear what to
    // do here: picking target timeout, as DNS covered by 400/404/500
    case 504: // HTTP/1.1: "Gateway Timeout" 
        // user sees: "Network Timeout: The server at www.foo.com
        //              is taking too long to respond."
        rv = NS_ERROR_NET_TIMEOUT;
        break;
    // Confused proxy server or malicious response
    default:
        rv = NS_ERROR_PROXY_CONNECTION_REFUSED; 
        break;
    }
    LOG(("Cancelling failed proxy CONNECT [this=%p httpStatus=%u]\n",
         this, httpStatus)); 
    Cancel(rv);
    CallOnStartRequest();
    return rv;
}

/**
 * Decide whether or not to remember Strict-Transport-Security, and whether
 * or not to enforce channel integrity.
 *
 * @return NS_ERROR_FAILURE if there's security information missing even though
 *             it's an HTTPS connection.
 */
nsresult
nsHttpChannel::ProcessSTSHeader()
{
    nsresult rv;
    bool isHttps = false;
    rv = mURI->SchemeIs("https", &isHttps);
    NS_ENSURE_SUCCESS(rv, rv);

    // If this channel is not loading securely, STS doesn't do anything.
    // The upgrade to HTTPS takes place earlier in the channel load process.
    if (!isHttps)
        return NS_OK;

    nsAutoCString asciiHost;
    rv = mURI->GetAsciiHost(asciiHost);
    NS_ENSURE_SUCCESS(rv, NS_OK);

    // If the channel is not a hostname, but rather an IP, STS doesn't do
    // anything.
    PRNetAddr hostAddr;
    if (PR_SUCCESS == PR_StringToNetAddr(asciiHost.get(), &hostAddr))
        return NS_OK;

    nsIStrictTransportSecurityService* stss = gHttpHandler->GetSTSService();
    NS_ENSURE_TRUE(stss, NS_ERROR_OUT_OF_MEMORY);

    // mSecurityInfo may not always be present, and if it's not then it is okay
    // to just disregard any STS headers since we know nothing about the
    // security of the connection.
    NS_ENSURE_TRUE(mSecurityInfo, NS_OK);

    // Check the trustworthiness of the channel (are there any cert errors?)
    // If there are certificate errors, we still load the data, we just ignore
    // any STS headers that are present.
    bool tlsIsBroken = false;
    rv = stss->ShouldIgnoreStsHeader(mSecurityInfo, &tlsIsBroken);
    NS_ENSURE_SUCCESS(rv, NS_OK);

    // If this was already an STS host, the connection should have been aborted
    // by the bad cert handler in the case of cert errors.  If it didn't abort the connection,
    // there's probably something funny going on.
    // If this wasn't an STS host, errors are allowed, but no more STS processing
    // will happen during the session.
    bool wasAlreadySTSHost;
    uint32_t flags =
      NS_UsePrivateBrowsing(this) ? nsISocketProvider::NO_PERMANENT_STORAGE : 0;
    rv = stss->IsStsURI(mURI, flags, &wasAlreadySTSHost);
    // Failure here means STS is broken.  Don't prevent the load, but this
    // shouldn't fail.
    NS_ENSURE_SUCCESS(rv, NS_OK);
    NS_ASSERTION(!(wasAlreadySTSHost && tlsIsBroken),
                 "connection should have been aborted by nss-bad-cert-handler");

    // Any STS header is ignored if the channel is not trusted due to
    // certificate errors (STS Spec 7.1) -- there is nothing else to do, and
    // the load may progress.
    if (tlsIsBroken) {
        LOG(("STS: Transport layer is not trustworthy, ignoring "
             "STS headers and continuing load\n"));
        return NS_OK;
    }

    // If there's a STS header, process it (STS Spec 7.1).  At this point in
    // processing, the channel is trusted, so the header should not be ignored.
    const nsHttpAtom atom = nsHttp::ResolveAtom("Strict-Transport-Security");
    nsAutoCString stsHeader;
    rv = mResponseHead->GetHeader(atom, stsHeader);
    if (rv == NS_ERROR_NOT_AVAILABLE) {
        LOG(("STS: No STS header, continuing load.\n"));
        return NS_OK;
    }
    // All other failures are fatal.
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stss->ProcessStsHeader(mURI, stsHeader.get(), flags, NULL, NULL);
    if (NS_FAILED(rv)) {
        LOG(("STS: Failed to parse STS header, continuing load.\n"));
        return NS_OK;
    }

    return NS_OK;
}

nsresult
nsHttpChannel::ProcessResponse()
{
    nsresult rv;
    uint32_t httpStatus = mResponseHead->Status();

    LOG(("nsHttpChannel::ProcessResponse [this=%p httpStatus=%u]\n",
        this, httpStatus));

    if (mTransaction->ProxyConnectFailed()) {
        // Only allow 407 (authentication required) to continue
        if (httpStatus != 407)
            return ProcessFailedProxyConnect(httpStatus);
        // If proxy CONNECT response needs to complete, wait to process connection
        // for Strict-Transport-Security.
    } else {
        // Given a successful connection, process any STS data that's relevant.
        rv = ProcessSTSHeader();
        NS_ASSERTION(NS_SUCCEEDED(rv), "ProcessSTSHeader failed, continuing load.");
    }

    MOZ_ASSERT(!mCachedContentIsValid);

    // notify "http-on-examine-response" observers
    gHttpHandler->OnExamineResponse(this);

    SetCookie(mResponseHead->PeekHeader(nsHttp::Set_Cookie));

    // handle unused username and password in url (see bug 232567)
    if (httpStatus != 401 && httpStatus != 407) {
        if (!mAuthRetryPending)
            mAuthProvider->CheckForSuperfluousAuth();
        if (mCanceled)
            return CallOnStartRequest();

        // reset the authentication's current continuation state because our
        // last authentication attempt has been completed successfully
        mAuthProvider->Disconnect(NS_ERROR_ABORT);
        mAuthProvider = nullptr;
        LOG(("  continuation state has been reset"));
    }

    bool successfulReval = false;

    // handle different server response categories.  Note that we handle
    // caching or not caching of error pages in
    // nsHttpResponseHead::MustValidate; if you change this switch, update that
    // one
    switch (httpStatus) {
    case 200:
    case 203:
        // Per RFC 2616, 14.35.2, "A server MAY ignore the Range header".
        // So if a server does that and sends 200 instead of 206 that we
        // expect, notify our caller.
        // However, if we wanted to start from the beginning, let it go through
        if (mResuming && mStartPos != 0) {
            LOG(("Server ignored our Range header, cancelling [this=%p]\n", this));
            Cancel(NS_ERROR_NOT_RESUMABLE);
            rv = CallOnStartRequest();
            break;
        }
        // these can normally be cached
        rv = ProcessNormal();
        MaybeInvalidateCacheEntryForSubsequentGet();
        break;
    case 206:
        if (mCachedContentIsPartial) // an internal byte range request...
            rv = ProcessPartialContent();
        else {
            mCacheInputStream.CloseAndRelease();
            rv = ProcessNormal();
        }
        break;
    case 300:
    case 301:
    case 302:
    case 307:
    case 308:
    case 303:
#if 0
    case 305: // disabled as a security measure (see bug 187996).
#endif
        // don't store the response body for redirects
        MaybeInvalidateCacheEntryForSubsequentGet();
        PushRedirectAsyncFunc(&nsHttpChannel::ContinueProcessResponse);
        rv = AsyncProcessRedirection(httpStatus);
        if (NS_FAILED(rv)) {
            PopRedirectAsyncFunc(&nsHttpChannel::ContinueProcessResponse);
            LOG(("AsyncProcessRedirection failed [rv=%x]\n", rv));
            // don't cache failed redirect responses.
            if (mCacheEntry)
                mCacheEntry->AsyncDoom(nullptr);
            if (DoNotRender3xxBody(rv)) {
                mStatus = rv;
                DoNotifyListener();
            } else {
                rv = ContinueProcessResponse(rv);
            }
        }
        break;
    case 304:
        rv = ProcessNotModified();
        if (NS_FAILED(rv)) {
            LOG(("ProcessNotModified failed [rv=%x]\n", rv));
            mCacheInputStream.CloseAndRelease();
            rv = ProcessNormal();
        }
        else {
            successfulReval = true;
        }
        break;
    case 401:
    case 407:
        rv = mAuthProvider->ProcessAuthentication(
            httpStatus, mConnectionInfo->UsingSSL() &&
                        mTransaction->ProxyConnectFailed());
        if (rv == NS_ERROR_IN_PROGRESS)  {
            // authentication prompt has been invoked and result
            // is expected asynchronously
            mAuthRetryPending = true;
            if (httpStatus == 407 || mTransaction->ProxyConnectFailed())
                mProxyAuthPending = true;

            // suspend the transaction pump to stop receiving the
            // unauthenticated content data. We will throw that data
            // away when user provides credentials or resume the pump
            // when user refuses to authenticate.
            LOG(("Suspending the transaction, asynchronously prompting for credentials"));
            mTransactionPump->Suspend();
            rv = NS_OK;
        }
        else if (NS_FAILED(rv)) {
            LOG(("ProcessAuthentication failed [rv=%x]\n", rv));
            if (mTransaction->ProxyConnectFailed())
                return ProcessFailedProxyConnect(httpStatus);
            if (!mAuthRetryPending)
                mAuthProvider->CheckForSuperfluousAuth();
            rv = ProcessNormal();
        }
        else
            mAuthRetryPending = true; // see DoAuthRetry
        break;
    default:
        rv = ProcessNormal();
        MaybeInvalidateCacheEntryForSubsequentGet();
        break;
    }

    CacheDisposition cacheDisposition;
    if (!mDidReval)
        cacheDisposition = kCacheMissed;
    else if (successfulReval)
        cacheDisposition = kCacheHitViaReval;
    else
        cacheDisposition = kCacheMissedViaReval;

    AccumulateCacheHitTelemetry(mCacheEntryDeviceTelemetryID,
                                cacheDisposition);

    return rv;
}

nsresult
nsHttpChannel::ContinueProcessResponse(nsresult rv)
{
    bool doNotRender = DoNotRender3xxBody(rv);

    if (rv == NS_ERROR_DOM_BAD_URI && mRedirectURI) {
        bool isHTTP = false;
        if (NS_FAILED(mRedirectURI->SchemeIs("http", &isHTTP)))
            isHTTP = false;
        if (!isHTTP && NS_FAILED(mRedirectURI->SchemeIs("https", &isHTTP)))
            isHTTP = false;

        if (!isHTTP) {
            // This was a blocked attempt to redirect and subvert the system by
            // redirecting to another protocol (perhaps javascript:)
            // In that case we want to throw an error instead of displaying the
            // non-redirected response body.
            LOG(("ContinueProcessResponse detected rejected Non-HTTP Redirection"));
            doNotRender = true;
            rv = NS_ERROR_CORRUPTED_CONTENT;
        }
    }

    if (doNotRender) {
        Cancel(rv);
        DoNotifyListener();
        return rv;
    }

    if (NS_SUCCEEDED(rv)) {
        InitCacheEntry();
        CloseCacheEntry(false);

        if (mApplicationCacheForWrite) {
            // Store response in the offline cache
            InitOfflineCacheEntry();
            CloseOfflineCacheEntry();
        }
        return NS_OK;
    }

    LOG(("ContinueProcessResponse got failure result [rv=%x]\n", rv));
    if (mTransaction->ProxyConnectFailed()) {
        return ProcessFailedProxyConnect(mRedirectType);
    }
    return ProcessNormal();
}

nsresult
nsHttpChannel::ProcessNormal()
{
    nsresult rv;

    LOG(("nsHttpChannel::ProcessNormal [this=%p]\n", this));

    bool succeeded;
    rv = GetRequestSucceeded(&succeeded);
    if (NS_SUCCEEDED(rv) && !succeeded) {
        PushRedirectAsyncFunc(&nsHttpChannel::ContinueProcessNormal);
        bool waitingForRedirectCallback;
        (void)ProcessFallback(&waitingForRedirectCallback);
        if (waitingForRedirectCallback) {
            // The transaction has been suspended by ProcessFallback.
            return NS_OK;
        }
        PopRedirectAsyncFunc(&nsHttpChannel::ContinueProcessNormal);
    }

    return ContinueProcessNormal(NS_OK);
}

nsresult
nsHttpChannel::ContinueProcessNormal(nsresult rv)
{
    if (NS_FAILED(rv)) {
        // Fill the failure status here, we have failed to fall back, thus we
        // have to report our status as failed.
        mStatus = rv;
        DoNotifyListener();
        return rv;
    }

    if (mFallingBack) {
        // Do not continue with normal processing, fallback is in
        // progress now.
        return NS_OK;
    }

    // if we're here, then any byte-range requests failed to result in a partial
    // response.  we must clear this flag to prevent BufferPartialContent from
    // being called inside our OnDataAvailable (see bug 136678).
    mCachedContentIsPartial = false;

    ClearBogusContentEncodingIfNeeded();

    UpdateInhibitPersistentCachingFlag();

    // this must be called before firing OnStartRequest, since http clients,
    // such as imagelib, expect our cache entry to already have the correct
    // expiration time (bug 87710).
    if (mCacheEntry) {
        rv = InitCacheEntry();
        if (NS_FAILED(rv))
            CloseCacheEntry(true);
    }

    // Check that the server sent us what we were asking for
    if (mResuming) {
        // Create an entity id from the response
        nsAutoCString id;
        rv = GetEntityID(id);
        if (NS_FAILED(rv)) {
            // If creating an entity id is not possible -> error
            Cancel(NS_ERROR_NOT_RESUMABLE);
        }
        else if (mResponseHead->Status() != 206 &&
                 mResponseHead->Status() != 200) {
            // Probably 404 Not Found, 412 Precondition Failed or
            // 416 Invalid Range -> error
            LOG(("Unexpected response status while resuming, aborting [this=%p]\n",
                 this));
            Cancel(NS_ERROR_ENTITY_CHANGED);
        }
        // If we were passed an entity id, verify it's equal to the server's
        else if (!mEntityID.IsEmpty()) {
            if (!mEntityID.Equals(id)) {
                LOG(("Entity mismatch, expected '%s', got '%s', aborting [this=%p]",
                     mEntityID.get(), id.get(), this));
                Cancel(NS_ERROR_ENTITY_CHANGED);
            }
        }
    }

    rv = CallOnStartRequest();
    if (NS_FAILED(rv)) return rv;

    // install cache listener if we still have a cache entry open
    if (mCacheEntry && (mCacheAccess & nsICache::ACCESS_WRITE)) {
        rv = InstallCacheListener();
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

nsresult
nsHttpChannel::PromptTempRedirect()
{
    if (!gHttpHandler->PromptTempRedirect()) {
        return NS_OK;
    }
    nsresult rv;
    nsCOMPtr<nsIStringBundleService> bundleService =
            do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStringBundle> stringBundle;
    rv = bundleService->CreateBundle(NECKO_MSGS_URL, getter_AddRefs(stringBundle));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLString messageString;
    rv = stringBundle->GetStringFromName(NS_LITERAL_STRING("RepostFormData").get(), getter_Copies(messageString));
    // GetStringFromName can return NS_OK and NULL messageString.
    if (NS_SUCCEEDED(rv) && messageString) {
        bool repost = false;

        nsCOMPtr<nsIPrompt> prompt;
        GetCallback(prompt);
        if (!prompt)
            return NS_ERROR_NO_INTERFACE;

        prompt->Confirm(nullptr, messageString, &repost);
        if (!repost)
            return NS_ERROR_FAILURE;
    }

    return rv;
}

nsresult
nsHttpChannel::ProxyFailover()
{
    LOG(("nsHttpChannel::ProxyFailover [this=%p]\n", this));

    nsresult rv;

    nsCOMPtr<nsIProtocolProxyService> pps =
            do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIProxyInfo> pi;
    rv = pps->GetFailoverForProxy(mConnectionInfo->ProxyInfo(), mURI, mStatus,
                                  getter_AddRefs(pi));
    if (NS_FAILED(rv))
        return rv;

    // XXXbz so where does this codepath remove us from the loadgroup,
    // exactly?
    return AsyncDoReplaceWithProxy(pi);
}

void
nsHttpChannel::HandleAsyncRedirectChannelToHttps()
{
    NS_PRECONDITION(!mCallOnResume, "How did that happen?");

    if (mSuspendCount) {
        LOG(("Waiting until resume to do async redirect to https [this=%p]\n", this));
        mCallOnResume = &nsHttpChannel::HandleAsyncRedirectChannelToHttps;
        return;
    }

    nsresult rv = AsyncRedirectChannelToHttps();
    if (NS_FAILED(rv))
        ContinueAsyncRedirectChannelToHttps(rv);
}

nsresult
nsHttpChannel::AsyncRedirectChannelToHttps()
{
    nsresult rv = NS_OK;
    LOG(("nsHttpChannel::HandleAsyncRedirectChannelToHttps() [STS]\n"));

    nsCOMPtr<nsIChannel> newChannel;
    nsCOMPtr<nsIURI> upgradedURI;

    rv = mURI->Clone(getter_AddRefs(upgradedURI));
    NS_ENSURE_SUCCESS(rv,rv);

    upgradedURI->SetScheme(NS_LITERAL_CSTRING("https"));

    int32_t oldPort = -1;
    rv = mURI->GetPort(&oldPort);
    if (NS_FAILED(rv)) return rv;

    // Keep any nonstandard ports so only the scheme is changed.
    // For example:
    //  http://foo.com:80 -> https://foo.com:443
    //  http://foo.com:81 -> https://foo.com:81

    if (oldPort == 80 || oldPort == -1)
        upgradedURI->SetPort(-1);
    else
        upgradedURI->SetPort(oldPort);

    nsCOMPtr<nsIIOService> ioService;
    rv = gHttpHandler->GetIOService(getter_AddRefs(ioService));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ioService->NewChannelFromURI(upgradedURI, getter_AddRefs(newChannel));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = SetupReplacementChannel(upgradedURI, newChannel, true);
    NS_ENSURE_SUCCESS(rv, rv);

    // Inform consumers about this fake redirect
    mRedirectChannel = newChannel;
    uint32_t flags = nsIChannelEventSink::REDIRECT_PERMANENT;

    PushRedirectAsyncFunc(
        &nsHttpChannel::ContinueAsyncRedirectChannelToHttps);
    rv = gHttpHandler->AsyncOnChannelRedirect(this, newChannel, flags);

    if (NS_SUCCEEDED(rv))
        rv = WaitForRedirectCallback();

    if (NS_FAILED(rv)) {
        AutoRedirectVetoNotifier notifier(this);
        PopRedirectAsyncFunc(
            &nsHttpChannel::ContinueAsyncRedirectChannelToHttps);
    }

    return rv;
}

nsresult
nsHttpChannel::ContinueAsyncRedirectChannelToHttps(nsresult rv)
{
    if (NS_SUCCEEDED(rv))
        rv = OpenRedirectChannel(rv);

    if (NS_FAILED(rv)) {
        // Fill the failure status here, the update to https had been vetoed
        // but from the security reasons we have to discard the whole channel
        // load.
        mStatus = rv;
    }

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nullptr, mStatus);

    if (NS_FAILED(rv)) {
        // We have to manually notify the listener because there is not any pump
        // that would call our OnStart/StopRequest after resume from waiting for
        // the redirect callback.
        DoNotifyListener();
    }

    return rv;
}

nsresult
nsHttpChannel::OpenRedirectChannel(nsresult rv)
{
    AutoRedirectVetoNotifier notifier(this);

    // Make sure to do this _after_ calling OnChannelRedirect
    mRedirectChannel->SetOriginalURI(mOriginalURI);

    // And now, notify observers the deprecated way
    nsCOMPtr<nsIHttpEventSink> httpEventSink;
    GetCallback(httpEventSink);
    if (httpEventSink) {
        // NOTE: nsIHttpEventSink is only used for compatibility with pre-1.8
        // versions.
        rv = httpEventSink->OnRedirect(this, mRedirectChannel);
        if (NS_FAILED(rv)) {
            return rv;
        }
    }

    // open new channel
    rv = mRedirectChannel->AsyncOpen(mListener, mListenerContext);
    if (NS_FAILED(rv)) {
        return rv;
    }

    mStatus = NS_BINDING_REDIRECTED;

    notifier.RedirectSucceeded();

    ReleaseListeners();

    return NS_OK;
}

nsresult
nsHttpChannel::AsyncDoReplaceWithProxy(nsIProxyInfo* pi)
{
    LOG(("nsHttpChannel::AsyncDoReplaceWithProxy [this=%p pi=%p]", this, pi));
    nsresult rv;

    nsCOMPtr<nsIChannel> newChannel;
    rv = gHttpHandler->NewProxiedChannel(mURI, pi, mProxyResolveFlags,
                                         mProxyURI, getter_AddRefs(newChannel));
    if (NS_FAILED(rv))
        return rv;

    rv = SetupReplacementChannel(mURI, newChannel, true);
    if (NS_FAILED(rv))
        return rv;

    // Inform consumers about this fake redirect
    mRedirectChannel = newChannel;
    uint32_t flags = nsIChannelEventSink::REDIRECT_INTERNAL;

    PushRedirectAsyncFunc(&nsHttpChannel::ContinueDoReplaceWithProxy);
    rv = gHttpHandler->AsyncOnChannelRedirect(this, newChannel, flags);

    if (NS_SUCCEEDED(rv))
        rv = WaitForRedirectCallback();

    if (NS_FAILED(rv)) {
        AutoRedirectVetoNotifier notifier(this);
        PopRedirectAsyncFunc(&nsHttpChannel::ContinueDoReplaceWithProxy);
    }

    return rv;
}

nsresult
nsHttpChannel::ContinueDoReplaceWithProxy(nsresult rv)
{
    AutoRedirectVetoNotifier notifier(this);

    if (NS_FAILED(rv))
        return rv;

    NS_PRECONDITION(mRedirectChannel, "No redirect channel?");

    // Make sure to do this _after_ calling OnChannelRedirect
    mRedirectChannel->SetOriginalURI(mOriginalURI);

    // open new channel
    rv = mRedirectChannel->AsyncOpen(mListener, mListenerContext);
    if (NS_FAILED(rv))
        return rv;

    mStatus = NS_BINDING_REDIRECTED;

    notifier.RedirectSucceeded();

    ReleaseListeners();

    return rv;
}

nsresult
nsHttpChannel::ResolveProxy()
{
    LOG(("nsHttpChannel::ResolveProxy [this=%p]\n", this));

    nsresult rv;

    nsCOMPtr<nsIProtocolProxyService> pps =
            do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    return pps->AsyncResolve(mProxyURI ? mProxyURI : mURI, mProxyResolveFlags,
                             this, getter_AddRefs(mProxyRequest));
}

bool
HttpCacheQuery::ResponseWouldVary() const
{
    AssertOnCacheThread();

    nsresult rv;
    nsAutoCString buf, metaKey;
    mCachedResponseHead->GetHeader(nsHttp::Vary, buf);
    if (!buf.IsEmpty()) {
        NS_NAMED_LITERAL_CSTRING(prefix, "request-");

        // enumerate the elements of the Vary header...
        char *val = buf.BeginWriting(); // going to munge buf
        char *token = nsCRT::strtok(val, NS_HTTP_HEADER_SEPS, &val);
        while (token) {
            LOG(("HttpCacheQuery::ResponseWouldVary [channel=%p] " \
                 "processing %s\n",
                 mChannel.get(), token));
            //
            // if "*", then assume response would vary.  technically speaking,
            // "Vary: header, *" is not permitted, but we allow it anyways.
            //
            // We hash values of cookie-headers for the following reasons:
            //
            //   1- cookies can be very large in size
            //
            //   2- cookies may contain sensitive information.  (for parity with
            //      out policy of not storing Set-cookie headers in the cache
            //      meta data, we likewise do not want to store cookie headers
            //      here.)
            //
            if (*token == '*')
                return true; // if we encounter this, just get out of here

            // build cache meta data key...
            metaKey = prefix + nsDependentCString(token);

            // check the last value of the given request header to see if it has
            // since changed.  if so, then indeed the cached response is invalid.
            nsXPIDLCString lastVal;
            mCacheEntry->GetMetaDataElement(metaKey.get(), getter_Copies(lastVal));
            LOG(("HttpCacheQuery::ResponseWouldVary [channel=%p] "
                     "stored value = \"%s\"\n",
                 mChannel.get(), lastVal.get()));

            // Look for value of "Cookie" in the request headers
            nsHttpAtom atom = nsHttp::ResolveAtom(token);
            const char *newVal = mRequestHead.PeekHeader(atom);
            if (!lastVal.IsEmpty()) {
                // value for this header in cache, but no value in request
                if (!newVal)
                    return true; // yes - response would vary

                // If this is a cookie-header, stored metadata is not
                // the value itself but the hash. So we also hash the
                // outgoing value here in order to compare the hashes
                nsAutoCString hash;
                if (atom == nsHttp::Cookie) {
                    rv = Hash(newVal, hash);
                    // If hash failed, be conservative (the cached hash
                    // exists at this point) and claim response would vary
                    if (NS_FAILED(rv))
                        return true;
                    newVal = hash.get();

                    LOG(("HttpCacheQuery::ResponseWouldVary [this=%p] " \
                            "set-cookie value hashed to %s\n",
                         mChannel.get(), newVal));
                }

                if (strcmp(newVal, lastVal))
                    return true; // yes, response would vary

            } else if (newVal) { // old value is empty, but newVal is set
                return true;
            }

            // next token...
            token = nsCRT::strtok(val, NS_HTTP_HEADER_SEPS, &val);
        }
    }
    return false;
}

// We need to have an implementation of this function just so that we can keep
// all references to mCallOnResume of type nsHttpChannel:  it's not OK in C++
// to set a member function ptr to  a base class function.
void
nsHttpChannel::HandleAsyncAbort()
{
    HttpAsyncAborter<nsHttpChannel>::HandleAsyncAbort();
}


nsresult
nsHttpChannel::EnsureAssocReq()
{
    // Confirm Assoc-Req response header on pipelined transactions
    // per draft-nottingham-http-pipeline-01.txt
    // of the form: GET http://blah.com/foo/bar?qv
    // return NS_OK as long as we don't find a violation
    // (i.e. no header is ok, as are malformed headers, as are
    // transactions that have not been pipelined (unless those have been
    // opted in via pragma))

    if (!mResponseHead)
        return NS_OK;

    const char *assoc_val = mResponseHead->PeekHeader(nsHttp::Assoc_Req);
    if (!assoc_val)
        return NS_OK;

    if (!mTransaction || !mURI)
        return NS_OK;
    
    if (!mTransaction->PipelinePosition()) {
        // "Pragma: X-Verify-Assoc-Req" can be used to verify even non pipelined
        // transactions. It is used by test harness.

        const char *pragma_val = mResponseHead->PeekHeader(nsHttp::Pragma);
        if (!pragma_val ||
            !nsHttp::FindToken(pragma_val, "X-Verify-Assoc-Req",
                               HTTP_HEADER_VALUE_SEPS))
            return NS_OK;
    }

    char *method = net_FindCharNotInSet(assoc_val, HTTP_LWS);
    if (!method)
        return NS_OK;
    
    bool equals;
    char *endofmethod;
    
    assoc_val = nullptr;
    endofmethod = net_FindCharInSet(method, HTTP_LWS);
    if (endofmethod)
        assoc_val = net_FindCharNotInSet(endofmethod, HTTP_LWS);
    if (!assoc_val)
        return NS_OK;
    
    // check the method
    int32_t methodlen = PL_strlen(mRequestHead.Method().get());
    if ((methodlen != (endofmethod - method)) ||
        PL_strncmp(method,
                   mRequestHead.Method().get(),
                   endofmethod - method)) {
        LOG(("  Assoc-Req failure Method %s", method));
        if (mConnectionInfo)
            gHttpHandler->ConnMgr()->
                PipelineFeedbackInfo(mConnectionInfo,
                                     nsHttpConnectionMgr::RedCorruptedContent,
                                     nullptr, 0);

        nsCOMPtr<nsIConsoleService> consoleService =
            do_GetService(NS_CONSOLESERVICE_CONTRACTID);
        if (consoleService) {
            nsAutoString message
                (NS_LITERAL_STRING("Failed Assoc-Req. Received "));
            AppendASCIItoUTF16(
                mResponseHead->PeekHeader(nsHttp::Assoc_Req),
                message);
            message += NS_LITERAL_STRING(" expected method ");
            AppendASCIItoUTF16(mRequestHead.Method().get(), message);
            consoleService->LogStringMessage(message.get());
        }

        if (gHttpHandler->EnforceAssocReq())
            return NS_ERROR_CORRUPTED_CONTENT;
        return NS_OK;
    }
    
    // check the URL
    nsCOMPtr<nsIURI> assoc_url;
    if (NS_FAILED(NS_NewURI(getter_AddRefs(assoc_url), assoc_val)) ||
        !assoc_url)
        return NS_OK;

    mURI->Equals(assoc_url, &equals);
    if (!equals) {
        LOG(("  Assoc-Req failure URL %s", assoc_val));
        if (mConnectionInfo)
            gHttpHandler->ConnMgr()->
                PipelineFeedbackInfo(mConnectionInfo,
                                     nsHttpConnectionMgr::RedCorruptedContent,
                                     nullptr, 0);

        nsCOMPtr<nsIConsoleService> consoleService =
            do_GetService(NS_CONSOLESERVICE_CONTRACTID);
        if (consoleService) {
            nsAutoString message
                (NS_LITERAL_STRING("Failed Assoc-Req. Received "));
            AppendASCIItoUTF16(
                mResponseHead->PeekHeader(nsHttp::Assoc_Req),
                message);
            message += NS_LITERAL_STRING(" expected URL ");
            AppendASCIItoUTF16(mSpec.get(), message);
            consoleService->LogStringMessage(message.get());
        }

        if (gHttpHandler->EnforceAssocReq())
            return NS_ERROR_CORRUPTED_CONTENT;
    }
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel <byte-range>
//-----------------------------------------------------------------------------

nsresult
HttpCacheQuery::SetupByteRangeRequest(uint32_t partialLen)
{
    AssertOnCacheThread();

    // cached content has been found to be partial, add necessary request
    // headers to complete cache entry.

    // use strongest validator available...
    const char *val = mCachedResponseHead->PeekHeader(nsHttp::ETag);
    if (!val)
        val = mCachedResponseHead->PeekHeader(nsHttp::Last_Modified);
    if (!val) {
        // if we hit this code it means mCachedResponseHead->IsResumable() is
        // either broken or not being called.
        NS_NOTREACHED("no cache validator");
        return NS_ERROR_FAILURE;
    }

    char buf[32];
    PR_snprintf(buf, sizeof(buf), "bytes=%u-", partialLen);

    mRequestHead.SetHeader(nsHttp::Range, nsDependentCString(buf));
    mRequestHead.SetHeader(nsHttp::If_Range, nsDependentCString(val));

    return NS_OK;
}

nsresult
nsHttpChannel::ProcessPartialContent()
{
    // ok, we've just received a 206
    //
    // we need to stream whatever data is in the cache out first, and then
    // pick up whatever data is on the wire, writing it into the cache.

    LOG(("nsHttpChannel::ProcessPartialContent [this=%p]\n", this)); 

    NS_ENSURE_TRUE(mCachedResponseHead, NS_ERROR_NOT_INITIALIZED);
    NS_ENSURE_TRUE(mCacheEntry, NS_ERROR_NOT_INITIALIZED);

    // Make sure to clear bogus content-encodings before looking at the header
    ClearBogusContentEncodingIfNeeded();
    
    // Check if the content-encoding we now got is different from the one we
    // got before
    if (PL_strcasecmp(mResponseHead->PeekHeader(nsHttp::Content_Encoding),
                      mCachedResponseHead->PeekHeader(nsHttp::Content_Encoding))
                      != 0) {
        Cancel(NS_ERROR_INVALID_CONTENT_ENCODING);
        return CallOnStartRequest();
    }


    // suspend the current transaction
    nsresult rv = mTransactionPump->Suspend();
    if (NS_FAILED(rv)) return rv;

    // merge any new headers with the cached response headers
    rv = mCachedResponseHead->UpdateHeaders(mResponseHead->Headers());
    if (NS_FAILED(rv)) return rv;

    // update the cached response head
    nsAutoCString head;
    mCachedResponseHead->Flatten(head, true);
    rv = mCacheEntry->SetMetaDataElement("response-head", head.get());
    if (NS_FAILED(rv)) return rv;

    // make the cached response be the current response
    mResponseHead = mCachedResponseHead;

    UpdateInhibitPersistentCachingFlag();

    rv = UpdateExpirationTime();
    if (NS_FAILED(rv)) return rv;

    // notify observers interested in looking at a response that has been
    // merged with any cached headers (http-on-examine-merged-response).
    gHttpHandler->OnExamineMergedResponse(this);

    // the cached content is valid, although incomplete.
    mCachedContentIsValid = true;
    return ReadFromCache(false);
}

nsresult
nsHttpChannel::OnDoneReadingPartialCacheEntry(bool *streamDone)
{
    nsresult rv;

    LOG(("nsHttpChannel::OnDoneReadingPartialCacheEntry [this=%p]", this));

    // by default, assume we would have streamed all data or failed...
    *streamDone = true;

    // setup cache listener to append to cache entry
    uint32_t size;
    rv = mCacheEntry->GetDataSize(&size);
    if (NS_FAILED(rv)) return rv;

    rv = InstallCacheListener(size);
    if (NS_FAILED(rv)) return rv;

    // need to track the logical offset of the data being sent to our listener
    mLogicalOffset = size;

    // we're now completing the cached content, so we can clear this flag.
    // this puts us in the state of a regular download.
    mCachedContentIsPartial = false;

    // resume the transaction if it exists, otherwise the pipe contained the
    // remaining part of the document and we've now streamed all of the data.
    if (mTransactionPump) {
        rv = mTransactionPump->Resume();
        if (NS_SUCCEEDED(rv))
            *streamDone = false;
    }
    else
        NS_NOTREACHED("no transaction");
    return rv;
}

//-----------------------------------------------------------------------------
// nsHttpChannel <cache>
//-----------------------------------------------------------------------------

nsresult
nsHttpChannel::ProcessNotModified()
{
    nsresult rv;

    LOG(("nsHttpChannel::ProcessNotModified [this=%p]\n", this)); 

    if (mCustomConditionalRequest) {
        LOG(("Bypassing ProcessNotModified due to custom conditional headers")); 
        return NS_ERROR_FAILURE;
    }

    if (!mDidReval) {
        LOG(("Server returned a 304 response even though we did not send a "
             "conditional request"));
        return NS_ERROR_FAILURE;
    }

    MOZ_ASSERT(mCachedResponseHead);
    MOZ_ASSERT(mCacheEntry);
    NS_ENSURE_TRUE(mCachedResponseHead && mCacheEntry, NS_ERROR_UNEXPECTED);

    // If the 304 response contains a Last-Modified different than the
    // one in our cache that is pretty suspicious and is, in at least the
    // case of bug 716840, a sign of the server having previously corrupted
    // our cache with a bad response. Take the minor step here of just dooming
    // that cache entry so there is a fighting chance of getting things on the
    // right track as well as disabling pipelining for that host.

    nsAutoCString lastModifiedCached;
    nsAutoCString lastModified304;

    rv = mCachedResponseHead->GetHeader(nsHttp::Last_Modified,
                                        lastModifiedCached);
    if (NS_SUCCEEDED(rv)) {
        rv = mResponseHead->GetHeader(nsHttp::Last_Modified, 
                                      lastModified304);
    }

    if (NS_SUCCEEDED(rv) && !lastModified304.Equals(lastModifiedCached)) {
        LOG(("Cache Entry and 304 Last-Modified Headers Do Not Match "
             "[%s] and [%s]\n",
             lastModifiedCached.get(), lastModified304.get()));

        mCacheEntry->AsyncDoom(nullptr);
        if (mConnectionInfo)
            gHttpHandler->ConnMgr()->
                PipelineFeedbackInfo(mConnectionInfo,
                                     nsHttpConnectionMgr::RedCorruptedContent,
                                     nullptr, 0);
        Telemetry::Accumulate(Telemetry::CACHE_LM_INCONSISTENT, true);
    }

    // merge any new headers with the cached response headers
    rv = mCachedResponseHead->UpdateHeaders(mResponseHead->Headers());
    if (NS_FAILED(rv)) return rv;

    // update the cached response head
    nsAutoCString head;
    mCachedResponseHead->Flatten(head, true);
    rv = mCacheEntry->SetMetaDataElement("response-head", head.get());
    if (NS_FAILED(rv)) return rv;

    // make the cached response be the current response
    mResponseHead = mCachedResponseHead;

    UpdateInhibitPersistentCachingFlag();

    rv = UpdateExpirationTime();
    if (NS_FAILED(rv)) return rv;

    rv = AddCacheEntryHeaders(mCacheEntry);
    if (NS_FAILED(rv)) return rv;

    // notify observers interested in looking at a reponse that has been
    // merged with any cached headers
    gHttpHandler->OnExamineMergedResponse(this);

    mCachedContentIsValid = true;
    rv = ReadFromCache(false);
    if (NS_FAILED(rv)) return rv;

    mTransactionReplaced = true;
    return NS_OK;
}

nsresult
nsHttpChannel::ProcessFallback(bool *waitingForRedirectCallback)
{
    LOG(("nsHttpChannel::ProcessFallback [this=%p]\n", this));
    nsresult rv;

    *waitingForRedirectCallback = false;
    mFallingBack = false;

    // At this point a load has failed (either due to network problems
    // or an error returned on the server).  Perform an application
    // cache fallback if we have a URI to fall back to.
    if (!mApplicationCache || mFallbackKey.IsEmpty() || mFallbackChannel) {
        LOG(("  choosing not to fallback [%p,%s,%d]",
             mApplicationCache.get(), mFallbackKey.get(), mFallbackChannel));
        return NS_OK;
    }

    // Make sure the fallback entry hasn't been marked as a foreign
    // entry.
    uint32_t fallbackEntryType;
    rv = mApplicationCache->GetTypes(mFallbackKey, &fallbackEntryType);
    NS_ENSURE_SUCCESS(rv, rv);

    if (fallbackEntryType & nsIApplicationCache::ITEM_FOREIGN) {
        // This cache points to a fallback that refers to a different
        // manifest.  Refuse to fall back.
        return NS_OK;
    }

    NS_ASSERTION(fallbackEntryType & nsIApplicationCache::ITEM_FALLBACK,
                 "Fallback entry not marked correctly!");

    // Kill any offline cache entry, and disable offline caching for the
    // fallback.
    if (mOfflineCacheEntry) {
        mOfflineCacheEntry->AsyncDoom(nullptr);
        mOfflineCacheEntry = 0;
        mOfflineCacheAccess = 0;
    }

    mApplicationCacheForWrite = nullptr;
    mOfflineCacheEntry = 0;
    mOfflineCacheAccess = 0;

    // Close the current cache entry.
    CloseCacheEntry(true);

    // Create a new channel to load the fallback entry.
    nsRefPtr<nsIChannel> newChannel;
    rv = gHttpHandler->NewChannel(mURI, getter_AddRefs(newChannel));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = SetupReplacementChannel(mURI, newChannel, true);
    NS_ENSURE_SUCCESS(rv, rv);

    // Make sure the new channel loads from the fallback key.
    nsCOMPtr<nsIHttpChannelInternal> httpInternal =
        do_QueryInterface(newChannel, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = httpInternal->SetupFallbackChannel(mFallbackKey.get());
    NS_ENSURE_SUCCESS(rv, rv);

    // ... and fallbacks should only load from the cache.
    uint32_t newLoadFlags = mLoadFlags | LOAD_REPLACE | LOAD_ONLY_FROM_CACHE;
    rv = newChannel->SetLoadFlags(newLoadFlags);

    // Inform consumers about this fake redirect
    mRedirectChannel = newChannel;
    uint32_t redirectFlags = nsIChannelEventSink::REDIRECT_INTERNAL;

    PushRedirectAsyncFunc(&nsHttpChannel::ContinueProcessFallback);
    rv = gHttpHandler->AsyncOnChannelRedirect(this, newChannel, redirectFlags);

    if (NS_SUCCEEDED(rv))
        rv = WaitForRedirectCallback();

    if (NS_FAILED(rv)) {
        AutoRedirectVetoNotifier notifier(this);
        PopRedirectAsyncFunc(&nsHttpChannel::ContinueProcessFallback);
        return rv;
    }

    // Indicate we are now waiting for the asynchronous redirect callback
    // if all went OK.
    *waitingForRedirectCallback = true;
    return NS_OK;
}

nsresult
nsHttpChannel::ContinueProcessFallback(nsresult rv)
{
    AutoRedirectVetoNotifier notifier(this);

    if (NS_FAILED(rv))
        return rv;

    NS_PRECONDITION(mRedirectChannel, "No redirect channel?");

    // Make sure to do this _after_ calling OnChannelRedirect
    mRedirectChannel->SetOriginalURI(mOriginalURI);

    rv = mRedirectChannel->AsyncOpen(mListener, mListenerContext);
    if (NS_FAILED(rv))
        return rv;

    // close down this channel
    Cancel(NS_BINDING_REDIRECTED);

    notifier.RedirectSucceeded();

    ReleaseListeners();

    mFallingBack = true;

    return NS_OK;
}

// Determines if a request is a byte range request for a subrange,
// i.e. is a byte range request, but not a 0- byte range request.
static bool
IsSubRangeRequest(nsHttpRequestHead &aRequestHead)
{
    if (!aRequestHead.PeekHeader(nsHttp::Range))
        return false;
    nsAutoCString byteRange;
    aRequestHead.GetHeader(nsHttp::Range, byteRange);
    return !byteRange.EqualsLiteral("bytes=0-");
}

nsresult
nsHttpChannel::OpenCacheEntry(bool usingSSL)
{
    nsresult rv;

    NS_ASSERTION(!mOnCacheEntryAvailableCallback, "Unexpected state");
    mLoadedFromApplicationCache = false;

    LOG(("nsHttpChannel::OpenCacheEntry [this=%p]", this));

    // make sure we're not abusing this function
    NS_PRECONDITION(!mCacheEntry, "cache entry already open");

    nsAutoCString cacheKey;

    if (mRequestHead.Method() == nsHttp::Post) {
        // If the post id is already set then this is an attempt to replay
        // a post transaction via the cache.  Otherwise, we need a unique
        // post id for this transaction.
        if (mPostID == 0)
            mPostID = gHttpHandler->GenerateUniqueID();
    }
    else if ((mRequestHead.Method() != nsHttp::Get) &&
             (mRequestHead.Method() != nsHttp::Head)) {
        // don't use the cache for other types of requests
        return NS_OK;
    }

    if (mResuming) {
        // We don't support caching for requests initiated
        // via nsIResumableChannel.
        return NS_OK;
    }

    // Don't cache byte range requests which are subranges, only cache 0-
    // byte range requests.
    if (IsSubRangeRequest(mRequestHead))
        return NS_OK;

    GenerateCacheKey(mPostID, cacheKey);

    // Set the desired cache access mode accordingly...
    nsCacheAccessMode accessRequested;
    rv = DetermineCacheAccess(&accessRequested);
    if (NS_FAILED(rv)) return rv;

    if (!mApplicationCache && mInheritApplicationCache) {
        // Pick up an application cache from the notification
        // callbacks if available
        nsCOMPtr<nsIApplicationCacheContainer> appCacheContainer;
        GetCallback(appCacheContainer);

        if (appCacheContainer) {
            appCacheContainer->GetApplicationCache(getter_AddRefs(mApplicationCache));
        }
    }

    if (!mApplicationCache &&
        (mChooseApplicationCache || (mLoadFlags & LOAD_CHECK_OFFLINE_CACHE))) {
        // We're supposed to load from an application cache, but
        // one was not supplied by the load group.  Ask the
        // application cache service to choose one for us.
        nsCOMPtr<nsIApplicationCacheService> appCacheService =
            do_GetService(NS_APPLICATIONCACHESERVICE_CONTRACTID);
        if (appCacheService) {
            nsCOMPtr<nsILoadContext> loadContext;
            GetCallback(loadContext);

            if (!loadContext)
                LOG(("  no load context while choosing application cache"));

            nsresult rv = appCacheService->ChooseApplicationCache
                (cacheKey, loadContext, getter_AddRefs(mApplicationCache));
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    nsCOMPtr<nsICacheSession> session;

    // If we have an application cache, we check it first.
    if (mApplicationCache) {
        nsAutoCString appCacheClientID;
        rv = mApplicationCache->GetClientID(appCacheClientID);
        if (NS_SUCCEEDED(rv)) {
            // We open with ACCESS_READ only, because we don't want to overwrite
            // the offline cache entry non-atomically. ACCESS_READ will prevent
            // us from writing to the offline cache as a normal cache entry.
            mCacheQuery = new HttpCacheQuery(
                                this, appCacheClientID,
                                nsICache::STORE_OFFLINE, mPrivateBrowsing,
                                cacheKey, nsICache::ACCESS_READ,
                                mLoadFlags & LOAD_BYPASS_LOCAL_CACHE_IF_BUSY,
                                usingSSL, true);

            mOnCacheEntryAvailableCallback =
                &nsHttpChannel::OnOfflineCacheEntryAvailable;

            rv = mCacheQuery->Dispatch();

            if (NS_SUCCEEDED(rv))
                return NS_OK;

            mCacheQuery = nullptr;
            mOnCacheEntryAvailableCallback = nullptr;
        }

        // opening cache entry failed
        return OnOfflineCacheEntryAvailable(nullptr, nsICache::ACCESS_NONE, rv);
    }

    return OpenNormalCacheEntry(usingSSL);
}

nsresult
nsHttpChannel::OnOfflineCacheEntryAvailable(nsICacheEntryDescriptor *aEntry,
                                            nsCacheAccessMode aAccess,
                                            nsresult aEntryStatus)
{
    nsresult rv;

    if (NS_SUCCEEDED(aEntryStatus)) {
        // We successfully opened an offline cache session and the entry,
        // so indicate we will load from the offline cache.
        mLoadedFromApplicationCache = true;
        mCacheEntry = aEntry;
        mCacheAccess = aAccess;
    }

    // XXX: shouldn't we fail here? I thought we're supposed to fail the
    // connection if we can't open an offline cache entry for writing.
    if (aEntryStatus == NS_ERROR_CACHE_WAIT_FOR_VALIDATION) {
        LOG(("bypassing local cache since it is busy\n"));
        // Don't try to load normal cache entry
        return NS_ERROR_NOT_AVAILABLE;
    }

    if (mCanceled && NS_FAILED(mStatus)) {
        LOG(("channel was canceled [this=%p status=%x]\n", this, mStatus));
        return mStatus;
    }

    if (NS_SUCCEEDED(aEntryStatus))
        return NS_OK;

    if (!mApplicationCacheForWrite && !mFallbackChannel) {
        nsAutoCString cacheKey;
        GenerateCacheKey(mPostID, cacheKey);

        // Check for namespace match.
        nsCOMPtr<nsIApplicationCacheNamespace> namespaceEntry;
        rv = mApplicationCache->GetMatchingNamespace
            (cacheKey, getter_AddRefs(namespaceEntry));
        NS_ENSURE_SUCCESS(rv, rv);

        uint32_t namespaceType = 0;
        if (!namespaceEntry ||
            NS_FAILED(namespaceEntry->GetItemType(&namespaceType)) ||
            (namespaceType &
             (nsIApplicationCacheNamespace::NAMESPACE_FALLBACK |
              nsIApplicationCacheNamespace::NAMESPACE_BYPASS)) == 0) {
            // When loading from an application cache, only items
            // on the whitelist or matching a
            // fallback namespace should hit the network...
            mLoadFlags |= LOAD_ONLY_FROM_CACHE;

            // ... and if there were an application cache entry,
            // we would have found it earlier.
            return NS_ERROR_CACHE_KEY_NOT_FOUND;
        }

        if (namespaceType &
            nsIApplicationCacheNamespace::NAMESPACE_FALLBACK) {
            rv = namespaceEntry->GetData(mFallbackKey);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    bool usingSSL = false;
    (void) mURI->SchemeIs("https", &usingSSL);
    return OpenNormalCacheEntry(usingSSL);
}

nsresult
nsHttpChannel::OpenNormalCacheEntry(bool usingSSL)
{
    NS_ASSERTION(!mCacheEntry, "We have already mCacheEntry");

    nsresult rv;

    uint32_t appId = NECKO_NO_APP_ID;
    bool isInBrowser = false;
    NS_GetAppInfo(this, &appId, &isInBrowser);

    nsCacheStoragePolicy storagePolicy = DetermineStoragePolicy();
    nsAutoCString clientID;
    nsHttpHandler::GetCacheSessionNameForStoragePolicy(storagePolicy, mPrivateBrowsing,
                                                       appId, isInBrowser, clientID);

    nsAutoCString cacheKey;
    GenerateCacheKey(mPostID, cacheKey);

    nsCacheAccessMode accessRequested;
    rv = DetermineCacheAccess(&accessRequested);
    if (NS_FAILED(rv))
        return rv;
 
    mCacheQuery = new HttpCacheQuery(
                                this, clientID, storagePolicy,
                                mPrivateBrowsing, cacheKey, accessRequested,
                                mLoadFlags & LOAD_BYPASS_LOCAL_CACHE_IF_BUSY,
                                usingSSL, false);

    mOnCacheEntryAvailableCallback =
        &nsHttpChannel::OnNormalCacheEntryAvailable;

    rv = mCacheQuery->Dispatch();
    if (NS_SUCCEEDED(rv))
        return NS_OK;

    mCacheQuery = nullptr;
    mOnCacheEntryAvailableCallback = nullptr;

    return rv;
}

nsresult
nsHttpChannel::OnNormalCacheEntryAvailable(nsICacheEntryDescriptor *aEntry,
                                           nsCacheAccessMode aAccess,
                                           nsresult aEntryStatus)
{
    if (NS_SUCCEEDED(aEntryStatus)) {
        mCacheEntry = aEntry;
        mCacheAccess = aAccess;
    }

    if (aEntryStatus == NS_ERROR_CACHE_WAIT_FOR_VALIDATION) {
        LOG(("bypassing local cache since it is busy\n"));
    }

    if (mCanceled && NS_FAILED(mStatus)) {
        LOG(("channel was canceled [this=%p status=%x]\n", this, mStatus));
        return mStatus;
    }

    if ((mLoadFlags & LOAD_ONLY_FROM_CACHE) && NS_FAILED(aEntryStatus))
        // if this channel is only allowed to pull from the cache, then
        // we must fail if we were unable to open a cache entry.
        return NS_ERROR_DOCUMENT_NOT_CACHED;

    // advance to the next state...
    return NS_OK;
}


nsresult
nsHttpChannel::OpenOfflineCacheEntryForWriting()
{
    nsresult rv;

    LOG(("nsHttpChannel::OpenOfflineCacheEntryForWriting [this=%p]", this));

    // make sure we're not abusing this function
    NS_PRECONDITION(!mOfflineCacheEntry, "cache entry already open");

    bool offline = gIOService->IsOffline();
    if (offline) {
        // only put things in the offline cache while online
        return NS_OK;
    }

    if (mLoadFlags & INHIBIT_CACHING) {
        // respect demand not to cache
        return NS_OK;
    }

    if (mRequestHead.Method() != nsHttp::Get) {
        // only cache complete documents offline
        return NS_OK;
    }

    // Don't cache byte range requests which are subranges, only cache 0-
    // byte range requests.
    if (IsSubRangeRequest(mRequestHead))
        return NS_OK;

    nsAutoCString cacheKey;
    GenerateCacheKey(mPostID, cacheKey);

    NS_ENSURE_TRUE(mApplicationCacheForWrite,
                   NS_ERROR_NOT_AVAILABLE);

    nsAutoCString offlineCacheClientID;
    rv = mApplicationCacheForWrite->GetClientID(offlineCacheClientID);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(!offlineCacheClientID.IsEmpty(),
                   NS_ERROR_NOT_AVAILABLE);

    nsCOMPtr<nsICacheSession> session;
    nsCOMPtr<nsICacheService> serv =
        do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = serv->CreateSession(offlineCacheClientID.get(),
                             nsICache::STORE_OFFLINE,
                             nsICache::STREAM_BASED,
                             getter_AddRefs(session));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIFile> profileDirectory;
    rv = mApplicationCacheForWrite->GetProfileDirectory(
            getter_AddRefs(profileDirectory));
    NS_ENSURE_SUCCESS(rv, rv);

    if (profileDirectory) {
        rv = session->SetProfileDirectory(profileDirectory);
        if (NS_FAILED(rv)) return rv;
    }

    mOnCacheEntryAvailableCallback =
        &nsHttpChannel::OnOfflineCacheEntryForWritingAvailable;
    rv = session->AsyncOpenCacheEntry(cacheKey, nsICache::ACCESS_READ_WRITE,
                                      this, true);
    if (NS_SUCCEEDED(rv))
        return NS_OK;

    mOnCacheEntryAvailableCallback = nullptr;

    return rv;
}

nsresult
nsHttpChannel::OnOfflineCacheEntryForWritingAvailable(
    nsICacheEntryDescriptor *aEntry,
    nsCacheAccessMode aAccess,
    nsresult aEntryStatus)
{
    if (NS_SUCCEEDED(aEntryStatus)) {
        mOfflineCacheEntry = aEntry;
        mOfflineCacheAccess = aAccess;
        if (NS_FAILED(aEntry->GetLastModified(&mOfflineCacheLastModifiedTime))) {
            mOfflineCacheLastModifiedTime = 0;
        }
    }

    if (aEntryStatus == NS_ERROR_CACHE_WAIT_FOR_VALIDATION) {
        // access to the cache entry has been denied (because the cache entry
        // is probably in use by another channel).  Either the cache is being
        // read from (we're offline) or it's being updated elsewhere.
        aEntryStatus = NS_OK;
    }

    if (mCanceled && NS_FAILED(mStatus)) {
        LOG(("channel was canceled [this=%p status=%x]\n", this, mStatus));
        return mStatus;
    }

    // advance to the next state...
    return aEntryStatus;
}

// Generates the proper cache-key for this instance of nsHttpChannel
nsresult
nsHttpChannel::GenerateCacheKey(uint32_t postID, nsACString &cacheKey)
{
    AssembleCacheKey(mFallbackChannel ? mFallbackKey.get() : mSpec.get(),
                     postID, cacheKey);
    return NS_OK;
}

// Assembles a cache-key from the given pieces of information and |mLoadFlags|
void
nsHttpChannel::AssembleCacheKey(const char *spec, uint32_t postID,
                                nsACString &cacheKey)
{
    cacheKey.Truncate();

    if (mLoadFlags & LOAD_ANONYMOUS) {
        cacheKey.AssignLiteral("anon&");
    }

    if (postID) {
        char buf[32];
        PR_snprintf(buf, sizeof(buf), "id=%x&", postID);
        cacheKey.Append(buf);
    }

    if (!cacheKey.IsEmpty()) {
        cacheKey.AppendLiteral("uri=");
    }

    // Strip any trailing #ref from the URL before using it as the key
    const char *p = strchr(spec, '#');
    if (p)
        cacheKey.Append(spec, p - spec);
    else
        cacheKey.Append(spec);
}

// UpdateExpirationTime is called when a new response comes in from the server.
// It updates the stored response-time and sets the expiration time on the
// cache entry.  
//
// From section 13.2.4 of RFC2616, we compute expiration time as follows:
//
//    timeRemaining = freshnessLifetime - currentAge
//    expirationTime = now + timeRemaining
// 
nsresult
nsHttpChannel::UpdateExpirationTime()
{
    NS_ENSURE_TRUE(mResponseHead, NS_ERROR_FAILURE);

    nsresult rv;

    uint32_t expirationTime = 0;
    if (!mResponseHead->MustValidate()) {
        uint32_t freshnessLifetime = 0;

        rv = mResponseHead->ComputeFreshnessLifetime(&freshnessLifetime);
        if (NS_FAILED(rv)) return rv;

        if (freshnessLifetime > 0) {
            uint32_t now = NowInSeconds(), currentAge = 0;

            rv = mResponseHead->ComputeCurrentAge(now, mRequestTime, &currentAge); 
            if (NS_FAILED(rv)) return rv;

            LOG(("freshnessLifetime = %u, currentAge = %u\n",
                freshnessLifetime, currentAge));

            if (freshnessLifetime > currentAge) {
                uint32_t timeRemaining = freshnessLifetime - currentAge;
                // be careful... now + timeRemaining may overflow
                if (now + timeRemaining < now)
                    expirationTime = uint32_t(-1);
                else
                    expirationTime = now + timeRemaining;
            }
            else
                expirationTime = now;
        }
    }

    rv = mCacheEntry->SetExpirationTime(expirationTime);
    NS_ENSURE_SUCCESS(rv, rv);

    if (mOfflineCacheEntry) {
        rv = mOfflineCacheEntry->SetExpirationTime(expirationTime);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

NS_IMETHODIMP
HttpCacheQuery::OnCacheEntryDoomed(nsresult)
{
    return NS_ERROR_UNEXPECTED;
}

nsresult
HttpCacheQuery::Dispatch()
{
    MOZ_ASSERT(NS_IsMainThread());

    nsresult rv;

    // XXX: Start the cache service; otherwise DispatchToCacheIOThread will
    // fail.
    nsCOMPtr<nsICacheService> service = 
        do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);

    // Ensure the stream transport service gets initialized on the main thread
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIStreamTransportService> sts =
            do_GetService(kStreamTransportServiceCID, &rv);
    }

    if (NS_SUCCEEDED(rv)) {
        rv = service->GetCacheIOTarget(getter_AddRefs(mCacheThread));
    }

    if (NS_SUCCEEDED(rv)) {
        rv = mCacheThread->Dispatch(this, NS_DISPATCH_NORMAL);
    }

    return rv;
}

NS_IMETHODIMP
HttpCacheQuery::Run()
{
    nsresult rv;
    if (!NS_IsMainThread()) {
        AssertOnCacheThread();

        nsCOMPtr<nsICacheService> serv =
            do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
        nsCOMPtr<nsICacheSession> session;
        if (NS_SUCCEEDED(rv)) {
            rv = serv->CreateSession(mClientID.get(), mStoragePolicy,
                                     nsICache::STREAM_BASED,
                                     getter_AddRefs(session));
        }
        if (NS_SUCCEEDED(rv)) {
            rv = session->SetIsPrivate(mUsingPrivateBrowsing);
        }
        if (NS_SUCCEEDED(rv)) {
            rv = session->SetDoomEntriesIfExpired(false);
        }
        if (NS_SUCCEEDED(rv)) {
            // AsyncOpenCacheEntry isn't really async when its called on the
            // cache service thread.
            rv = session->AsyncOpenCacheEntry(mCacheKey, mAccessToRequest, this,
                                              mNoWait);
        }
        if (NS_FAILED(rv)) {
            LOG(("Failed to open cache entry -- calling OnCacheEntryAvailable "
                 "directly."));
            rv = OnCacheEntryAvailable(nullptr, 0, rv);
        }
    } else {
        // break cycles
        nsCOMPtr<nsICacheListener> channel = mChannel.forget();
        mCacheThread = nullptr;
        nsCOMPtr<nsICacheEntryDescriptor> entry = mCacheEntry.forget();

        rv = channel->OnCacheEntryAvailable(entry, mCacheAccess, mStatus);
    }
    
    return rv;
}

NS_IMETHODIMP
HttpCacheQuery::OnCacheEntryAvailable(nsICacheEntryDescriptor *entry,
                                      nsCacheAccessMode access,
                                      nsresult status)

{
    LOG(("HttpCacheQuery::OnCacheEntryAvailable [channel=%p entry=%p "
         "access=%x status=%x, mRunConut=%d]\n", mChannel.get(), entry, access,
         status, int(mRunCount)));

    // XXX Bug 759805: Sometimes we will call this method directly from
    // HttpCacheQuery::Run when AsyncOpenCacheEntry fails, but
    // AsyncOpenCacheEntry will also call this method. As a workaround, we just
    // ensure we only execute this code once.
    NS_ENSURE_TRUE(mRunCount == 0, NS_ERROR_UNEXPECTED);
    ++mRunCount;

    AssertOnCacheThread();

    mCacheEntry = entry;
    mCacheAccess = access;
    mStatus = status;

    if (mCacheEntry) {
        char* cacheDeviceID = nullptr;
        mCacheEntry->GetDeviceID(&cacheDeviceID);
        if (cacheDeviceID) {
            if (!strcmp(cacheDeviceID, kDiskDeviceID)) {
                mCacheEntryDeviceTelemetryID
                    = Telemetry::HTTP_DISK_CACHE_DISPOSITION_2;
            } else if (!strcmp(cacheDeviceID, kMemoryDeviceID)) {
                mCacheEntryDeviceTelemetryID
                    = Telemetry::HTTP_MEMORY_CACHE_DISPOSITION_2;
            } else if (!strcmp(cacheDeviceID, kOfflineDeviceID)) {
                mCacheEntryDeviceTelemetryID
                    = Telemetry::HTTP_OFFLINE_CACHE_DISPOSITION_2;
            } else {
                MOZ_NOT_REACHED("unknown cache device ID");
            }

            delete cacheDeviceID;
        } else {
            // If we can read from the entry, it must have a device, but
            // sometimes we don't (Bug 769497).
            // MOZ_ASSERT(!(mCacheAccess & nsICache::ACCESS_READ));
        }
    }

    nsresult rv = CheckCache();
    if (NS_FAILED(rv))
        NS_WARNING("cache check failed");

    rv = NS_DispatchToMainThread(this);
    return rv;
}

nsresult
HttpCacheQuery::CheckCache()
{
    AssertOnCacheThread();

    nsresult rv = NS_OK;

    LOG(("HttpCacheQuery::CheckCache enter [channel=%p entry=%p access=%d]",
        mChannel.get(), mCacheEntry.get(), mCacheAccess));

    // Remember the request is a custom conditional request so that we can
    // process any 304 response correctly.
    mCustomConditionalRequest =
        mRequestHead.PeekHeader(nsHttp::If_Modified_Since) ||
        mRequestHead.PeekHeader(nsHttp::If_None_Match) ||
        mRequestHead.PeekHeader(nsHttp::If_Unmodified_Since) ||
        mRequestHead.PeekHeader(nsHttp::If_Match) ||
        mRequestHead.PeekHeader(nsHttp::If_Range);

    // Be pessimistic: assume the cache entry has no useful data.
    mCachedContentIsValid = false;

    // Don't proceed unless we have opened a cache entry for reading.
    if (!mCacheEntry || !(mCacheAccess & nsICache::ACCESS_READ))
        return NS_OK;

    nsXPIDLCString buf;

    // Get the method that was used to generate the cached response
    rv = mCacheEntry->GetMetaDataElement("request-method", getter_Copies(buf));
    NS_ENSURE_SUCCESS(rv, rv);

    nsHttpAtom method = nsHttp::ResolveAtom(buf);
    if (method == nsHttp::Head) {
        // The cached response does not contain an entity.  We can only reuse
        // the response if the current request is also HEAD.
        if (mRequestHead.Method() != nsHttp::Head)
            return NS_OK;
    }
    buf.Adopt(0);

    // We'll need this value in later computations...
    uint32_t lastModifiedTime;
    rv = mCacheEntry->GetLastModified(&lastModifiedTime);
    NS_ENSURE_SUCCESS(rv, rv);

    // Determine if this is the first time that this cache entry
    // has been accessed during this session.
    bool fromPreviousSession =
            (gHttpHandler->SessionStartTime() > lastModifiedTime);

    // Get the cached HTTP response headers
    rv = mCacheEntry->GetMetaDataElement("response-head", getter_Copies(buf));
    NS_ENSURE_SUCCESS(rv, rv);

    // Parse the cached HTTP response headers
    mCachedResponseHead = new nsHttpResponseHead();
    rv = mCachedResponseHead->Parse((char *) buf.get());
    NS_ENSURE_SUCCESS(rv, rv);
    buf.Adopt(0);

    bool isCachedRedirect = WillRedirect(mCachedResponseHead);

    // Do not return 304 responses from the cache, and also do not return
    // any other non-redirect 3xx responses from the cache (see bug 759043).
    NS_ENSURE_TRUE((mCachedResponseHead->Status() / 100 != 3) ||
                   isCachedRedirect, NS_ERROR_ABORT);

    // Don't bother to validate items that are read-only,
    // unless they are read-only because of INHIBIT_CACHING or because
    // we're updating the offline cache.
    // Don't bother to validate if this is a fallback entry.
    if (!mCacheForOfflineUse &&
        (mLoadedFromApplicationCache ||
         (mCacheAccess == nsICache::ACCESS_READ &&
          !(mLoadFlags & nsIRequest::INHIBIT_CACHING)) ||
         mFallbackChannel)) {
        rv = OpenCacheInputStream(true);
        if (NS_SUCCEEDED(rv)) {
            mCachedContentIsValid = true;
            // XXX: Isn't the cache entry already valid?
            MaybeMarkCacheEntryValid(this, mCacheEntry, mCacheAccess);
        }
        return rv;
    }

    if (method != nsHttp::Head && !isCachedRedirect) {
        // If the cached content-length is set and it does not match the data
        // size of the cached content, then the cached response is partial...
        // either we need to issue a byte range request or we need to refetch
        // the entire document.
        //
        // We exclude redirects from this check because we (usually) strip the
        // entity when we store the cache entry, and even if we didn't, we
        // always ignore a cached redirect's entity anyway. See bug 759043.
        int64_t contentLength = mCachedResponseHead->ContentLength();
        if (contentLength != int64_t(-1)) {
            uint32_t size;
            rv = mCacheEntry->GetDataSize(&size);
            NS_ENSURE_SUCCESS(rv, rv);

            if (int64_t(size) != contentLength) {
                LOG(("Cached data size does not match the Content-Length header "
                     "[content-length=%lld size=%u]\n", int64_t(contentLength), size));

                bool hasContentEncoding =
                    mCachedResponseHead->PeekHeader(nsHttp::Content_Encoding)
                    != nullptr;
                if ((int64_t(size) < contentLength) &&
                     size > 0 &&
                     !hasContentEncoding &&
                     mCachedResponseHead->IsResumable() &&
                     !mCustomConditionalRequest &&
                     !mCachedResponseHead->NoStore()) {
                    // looks like a partial entry we can reuse; add If-Range
                    // and Range headers.
                    rv = SetupByteRangeRequest(size);
                    mCachedContentIsPartial = NS_SUCCEEDED(rv);
                    if (mCachedContentIsPartial) {
                        rv = OpenCacheInputStream(false);
                    } else {
                        // Make the request unconditional again.
                        mRequestHead.ClearHeader(nsHttp::Range);
                        mRequestHead.ClearHeader(nsHttp::If_Range);
                    }
                }
                return rv;
            }
        }
    }

    bool doValidation = false;
    bool canAddImsHeader = true;

    // Cached entry is not the entity we request (see bug #633743)
    if (ResponseWouldVary()) {
        LOG(("Validating based on Vary headers returning TRUE\n"));
        canAddImsHeader = false;
        doValidation = true;
    }
    // If the LOAD_FROM_CACHE flag is set, any cached data can simply be used
    else if (mLoadFlags & nsIRequest::LOAD_FROM_CACHE) {
        LOG(("NOT validating based on LOAD_FROM_CACHE load flag\n"));
        doValidation = false;
    }
    // If the VALIDATE_ALWAYS flag is set, any cached data won't be used until
    // it's revalidated with the server.
    else if (mLoadFlags & nsIRequest::VALIDATE_ALWAYS) {
        LOG(("Validating based on VALIDATE_ALWAYS load flag\n"));
        doValidation = true;
    }
    // Even if the VALIDATE_NEVER flag is set, there are still some cases in
    // which we must validate the cached response with the server.
    else if (mLoadFlags & nsIRequest::VALIDATE_NEVER) {
        LOG(("VALIDATE_NEVER set\n"));
        // if no-store or if no-cache and ssl, validate cached response (see
        // bug 112564 for an explanation of this logic)
        if (mCachedResponseHead->NoStore() ||
           (mCachedResponseHead->NoCache() && mUsingSSL)) {
            LOG(("Validating based on (no-store || (no-cache && ssl)) logic\n"));
            doValidation = true;
        }
        else {
            LOG(("NOT validating based on VALIDATE_NEVER load flag\n"));
            doValidation = false;
        }
    }
    // check if validation is strictly required...
    else if (mCachedResponseHead->MustValidate()) {
        LOG(("Validating based on MustValidate() returning TRUE\n"));
        doValidation = true;
    }

    else if (MustValidateBasedOnQueryUrl()) {
        LOG(("Validating based on RFC 2616 section 13.9 "
             "(query-url w/o explicit expiration-time)\n"));
        doValidation = true;
    }
    // Check if the cache entry has expired...
    else {
        uint32_t time = 0; // a temporary variable for storing time values...

        rv = mCacheEntry->GetExpirationTime(&time);
        NS_ENSURE_SUCCESS(rv, rv);

        if (NowInSeconds() <= time)
            doValidation = false;
        else if (mCachedResponseHead->MustValidateIfExpired())
            doValidation = true;
        else if (mLoadFlags & nsIRequest::VALIDATE_ONCE_PER_SESSION) {
            // If the cached response does not include expiration infor-
            // mation, then we must validate the response, despite whether
            // or not this is the first access this session.  This behavior
            // is consistent with existing browsers and is generally expected
            // by web authors.
            rv = mCachedResponseHead->ComputeFreshnessLifetime(&time);
            NS_ENSURE_SUCCESS(rv, rv);

            if (time == 0)
                doValidation = true;
            else
                doValidation = fromPreviousSession;
        }
        else
            doValidation = true;

        LOG(("%salidating based on expiration time\n", doValidation ? "V" : "Not v"));
    }

    if (!doValidation && mRequestHead.PeekHeader(nsHttp::If_Match) &&
        (method == nsHttp::Get || method == nsHttp::Head)) {
        const char *requestedETag, *cachedETag;
        cachedETag = mCachedResponseHead->PeekHeader(nsHttp::ETag);
        requestedETag = mRequestHead.PeekHeader(nsHttp::If_Match);
        if (cachedETag && (!strncmp(cachedETag, "W/", 2) ||
            strcmp(requestedETag, cachedETag))) {
            // User has defined If-Match header, if the cached entry is not 
            // matching the provided header value or the cached ETag is weak,
            // force validation.
            doValidation = true;
        }
    }

    if (!doValidation) {
        //
        // Check the authorization headers used to generate the cache entry.
        // We must validate the cache entry if:
        //
        // 1) the cache entry was generated prior to this session w/
        //    credentials (see bug 103402).
        // 2) the cache entry was generated w/o credentials, but would now
        //    require credentials (see bug 96705).
        //
        // NOTE: this does not apply to proxy authentication.
        //
        mCacheEntry->GetMetaDataElement("auth", getter_Copies(buf));
        doValidation =
            (fromPreviousSession && !buf.IsEmpty()) ||
            (buf.IsEmpty() && mRequestHead.PeekHeader(nsHttp::Authorization));
    }

    // Bug #561276: We maintain a chain of cache-keys which returns cached
    // 3xx-responses (redirects) in order to detect cycles. If a cycle is
    // found, ignore the cached response and hit the net. Otherwise, use
    // the cached response and add the cache-key to the chain. Note that
    // a limited number of redirects (cached or not) is allowed and is
    // enforced independently of this mechanism
    if (!doValidation && isCachedRedirect) {
        if (!mRedirectedCachekeys)
            mRedirectedCachekeys = new nsTArray<nsCString>();
        else if (mRedirectedCachekeys->Contains(mCacheKey))
            doValidation = true;

        LOG(("Redirection-chain %s key %s\n",
             doValidation ? "contains" : "does not contain", mCacheKey.get()));

        // Append cacheKey if not in the chain already
        if (!doValidation)
            mRedirectedCachekeys->AppendElement(mCacheKey);
    }

    mCachedContentIsValid = !doValidation;

    if (doValidation) {
        //
        // now, we are definitely going to issue a HTTP request to the server.
        // make it conditional if possible.
        //
        // do not attempt to validate no-store content, since servers will not
        // expect it to be cached.  (we only keep it in our cache for the
        // purposes of back/forward, etc.)
        //
        // the request method MUST be either GET or HEAD (see bug 175641).
        //
        // do not override conditional headers when consumer has defined its own
        if (!mCachedResponseHead->NoStore() &&
            (mRequestHead.Method() == nsHttp::Get ||
             mRequestHead.Method() == nsHttp::Head) &&
             !mCustomConditionalRequest) {
            const char *val;
            // Add If-Modified-Since header if a Last-Modified was given
            // and we are allowed to do this (see bugs 510359 and 269303)
            if (canAddImsHeader) {
                val = mCachedResponseHead->PeekHeader(nsHttp::Last_Modified);
                if (val)
                    mRequestHead.SetHeader(nsHttp::If_Modified_Since,
                                           nsDependentCString(val));
            }
            // Add If-None-Match header if an ETag was given in the response
            val = mCachedResponseHead->PeekHeader(nsHttp::ETag);
            if (val)
                mRequestHead.SetHeader(nsHttp::If_None_Match,
                                       nsDependentCString(val));
            mDidReval = true;
        }
    }

    if (mCachedContentIsValid || mDidReval) {
        rv = OpenCacheInputStream(mCachedContentIsValid);
        if (NS_FAILED(rv)) {
            // If we can't get the entity then we have to act as though we
            // don't have the cache entry.
            if (mDidReval) {
                // Make the request unconditional again.
                mRequestHead.ClearHeader(nsHttp::If_Modified_Since);
                mRequestHead.ClearHeader(nsHttp::ETag);
                mDidReval = false;
            }
            mCachedContentIsValid = false;
        }
    }

    if (mCachedContentIsValid) {
        // XXX: Isn't the cache entry already valid?
        MaybeMarkCacheEntryValid(this, mCacheEntry, mCacheAccess);
    }

    LOG(("nsHTTPChannel::CheckCache exit [this=%p doValidation=%d]\n",
         this, doValidation));
    return rv;
}

/*static*/ inline bool
HttpCacheQuery::HasQueryString(nsHttpAtom method, nsIURI * uri)
{
    // Must be called on the main thread because nsIURI does not implement
    // thread-safe QueryInterface.
    MOZ_ASSERT(NS_IsMainThread());

    if (method != nsHttp::Get && method != nsHttp::Head)
        return false;

    nsAutoCString query;
    nsCOMPtr<nsIURL> url = do_QueryInterface(uri);
    nsresult rv = url->GetQuery(query);
    return NS_SUCCEEDED(rv) && !query.IsEmpty();
}

bool
HttpCacheQuery::MustValidateBasedOnQueryUrl() const
{
    AssertOnCacheThread();

    // RFC 2616, section 13.9 states that GET-requests with a query-url
    // MUST NOT be treated as fresh unless the server explicitly provides
    // an expiration-time in the response. See bug #468594
    // Section 13.2.1 (6th paragraph) defines "explicit expiration time"
    if (mHasQueryString)
    {
        uint32_t tmp; // we don't need the value, just whether it's set
        nsresult rv = mCachedResponseHead->GetExpiresValue(&tmp);
        if (NS_FAILED(rv)) {
            rv = mCachedResponseHead->GetMaxAgeValue(&tmp);
            if (NS_FAILED(rv)) {
                return true;
            }
        }
    }
    return false;
}


bool
nsHttpChannel::ShouldUpdateOfflineCacheEntry()
{
    if (!mApplicationCacheForWrite || !mOfflineCacheEntry) {
        return false;
    }

    // if we're updating the cache entry, update the offline cache entry too
    if (mCacheEntry && (mCacheAccess & nsICache::ACCESS_WRITE)) {
        return true;
    }

    // if there's nothing in the offline cache, add it
    if (mOfflineCacheEntry && (mOfflineCacheAccess == nsICache::ACCESS_WRITE)) {
        return true;
    }

    // if the document is newer than the offline entry, update it
    uint32_t docLastModifiedTime;
    nsresult rv = mResponseHead->GetLastModifiedValue(&docLastModifiedTime);
    if (NS_FAILED(rv)) {
        return true;
    }

    if (mOfflineCacheLastModifiedTime == 0) {
        return false;
    }

    if (docLastModifiedTime > mOfflineCacheLastModifiedTime) {
        return true;
    }

    return false;
}

nsresult
HttpCacheQuery::OpenCacheInputStream(bool startBuffering)
{
    AssertOnCacheThread();

    if (mUsingSSL) {
        nsresult rv = mCacheEntry->GetSecurityInfo(
                                      getter_AddRefs(mCachedSecurityInfo));
        if (NS_FAILED(rv)) {
            LOG(("failed to parse security-info [channel=%p, entry=%p]",
                 this, mCacheEntry.get()));
            NS_WARNING("failed to parse security-info");
            return rv;
        }

        // XXX: We should not be skilling this check in the offline cache
        // case, but we have to do so now to work around bug 794507.
        MOZ_ASSERT(mCachedSecurityInfo || mLoadedFromApplicationCache);
        if (!mCachedSecurityInfo && !mLoadedFromApplicationCache) {
            LOG(("mCacheEntry->GetSecurityInfo returned success but did not "
                 "return the security info [channel=%p, entry=%p]",
                 this, mCacheEntry.get()));
            return NS_ERROR_UNEXPECTED; // XXX error code
        }
    }

    nsresult rv = NS_OK;

    // Keep the conditions below in sync with the conditions in ReadFromCache.

    if (WillRedirect(mCachedResponseHead)) {
        // Do not even try to read the entity for a redirect because we do not
        // return an entity to the application when we process redirects.
        LOG(("Will skip read of cached redirect entity\n"));
        return NS_OK;
    }

    if ((mLoadFlags & nsICachingChannel::LOAD_ONLY_IF_MODIFIED) &&
        !mCachedContentIsPartial) {
        // For LOAD_ONLY_IF_MODIFIED, we usually don't have to deal with the
        // cached entity. 
        if (!mCacheForOfflineUse) {
            LOG(("Will skip read from cache based on LOAD_ONLY_IF_MODIFIED "
                 "load flag\n"));
            return NS_OK;
        }

        // If offline caching has been requested and the offline cache needs
        // updating, we must complete the call even if the main cache entry
        // is up to date. We don't know yet for sure whether the offline
        // cache needs updating because at this point we haven't opened it
        // for writing yet, so we have to start reading the cached entity now
        // just in case.
        LOG(("May skip read from cache based on LOAD_ONLY_IF_MODIFIED "
              "load flag\n"));
    }

    // Open an input stream for the entity, so that the call to OpenInputStream
    // happens off the main thread.
    nsCOMPtr<nsIInputStream> stream;
    rv = mCacheEntry->OpenInputStream(0, getter_AddRefs(stream));

    if (NS_FAILED(rv)) {
        LOG(("Failed to open cache input stream [channel=%p, "
             "mCacheEntry=%p]", mChannel.get(), mCacheEntry.get()));
        return rv;
    }

    if (!startBuffering) {
        // We do not connect the stream to the stream transport service if we
        // have to validate the entry with the server. If we did, we would get
        // into a race condition between the stream transport service reading
        // the existing contents and the opening of the cache entry's output
        // stream to write the new contents in the case where we get a non-304
        // response.
        LOG(("Opened cache input stream without buffering [channel=%p, "
              "mCacheEntry=%p, stream=%p]", mChannel.get(),
              mCacheEntry.get(), stream.get()));
        mCacheInputStream.takeOver(stream);
        return rv;
    }

    // Have the stream transport service start reading the entity on one of its
    // background threads.
    
    nsCOMPtr<nsITransport> transport;
    nsCOMPtr<nsIInputStream> wrapper;

    nsCOMPtr<nsIStreamTransportService> sts =
        do_GetService(kStreamTransportServiceCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = sts->CreateInputTransport(stream, int64_t(-1), int64_t(-1),
                                        true, getter_AddRefs(transport));
    }
    if (NS_SUCCEEDED(rv)) {
        rv = transport->OpenInputStream(0, 0, 0, getter_AddRefs(wrapper));
    }
    if (NS_SUCCEEDED(rv)) {
        LOG(("Opened cache input stream [channel=%p, wrapper=%p, "
              "transport=%p, stream=%p]", this, wrapper.get(),
              transport.get(), stream.get()));
    } else {
        LOG(("Failed to open cache input stream [channel=%p, "
              "wrapper=%p, transport=%p, stream=%p]", this,
              wrapper.get(), transport.get(), stream.get()));
    
        stream->Close();
        return rv;
    }

    mCacheInputStream.takeOver(wrapper);

    return NS_OK;
}

// Actually process the cached response that we started to handle in CheckCache
// and/or StartBufferingCachedEntity.
nsresult
nsHttpChannel::ReadFromCache(bool alreadyMarkedValid)
{
    NS_ENSURE_TRUE(mCacheEntry, NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(mCachedContentIsValid, NS_ERROR_FAILURE);

    LOG(("nsHttpChannel::ReadFromCache [this=%p] "
         "Using cached copy of: %s\n", this, mSpec.get()));

    if (mCachedResponseHead)
        mResponseHead = mCachedResponseHead;

    UpdateInhibitPersistentCachingFlag();

    // if we don't already have security info, try to get it from the cache 
    // entry. there are two cases to consider here: 1) we are just reading
    // from the cache, or 2) this may be due to a 304 not modified response,
    // in which case we could have security info from a socket transport.
    if (!mSecurityInfo)
        mSecurityInfo = mCachedSecurityInfo;

    if (!alreadyMarkedValid && !mCachedContentIsPartial) {
        // We validated the entry, and we have write access to the cache, so
        // mark the cache entry as valid in order to allow others access to
        // this cache entry.
        //
        // TODO: This should be done asynchronously so we don't take the cache
        // service lock on the main thread.
        MaybeMarkCacheEntryValid(this, mCacheEntry, mCacheAccess);
    }

    nsresult rv;

    // Keep the conditions below in sync with the conditions in
    // StartBufferingCachedEntity.

    if (WillRedirect(mResponseHead)) {
        // TODO: Bug 759040 - We should call HandleAsyncRedirect directly here,
        // to avoid event dispatching latency.
        MOZ_ASSERT(!mCacheInputStream);
        LOG(("Skipping skip read of cached redirect entity\n"));
        return AsyncCall(&nsHttpChannel::HandleAsyncRedirect);
    }
    
    if ((mLoadFlags & LOAD_ONLY_IF_MODIFIED) && !mCachedContentIsPartial) {
        if (!mApplicationCacheForWrite) {
            LOG(("Skipping read from cache based on LOAD_ONLY_IF_MODIFIED "
                 "load flag\n"));
            MOZ_ASSERT(!mCacheInputStream);
            // TODO: Bug 759040 - We should call HandleAsyncNotModified directly
            // here, to avoid event dispatching latency.
            return AsyncCall(&nsHttpChannel::HandleAsyncNotModified);
        }
        
        if (!ShouldUpdateOfflineCacheEntry()) {
            LOG(("Skipping read from cache based on LOAD_ONLY_IF_MODIFIED "
                 "load flag (mApplicationCacheForWrite not null case)\n"));
            mCacheInputStream.CloseAndRelease();
            // TODO: Bug 759040 - We should call HandleAsyncNotModified directly
            // here, to avoid event dispatching latency.
            return AsyncCall(&nsHttpChannel::HandleAsyncNotModified);
        }
    }

    MOZ_ASSERT(mCacheInputStream);
    if (!mCacheInputStream) {
        NS_ERROR("mCacheInputStream is null but we're expecting to "
                        "be able to read from it.");
        return NS_ERROR_UNEXPECTED;
    }


    nsCOMPtr<nsIInputStream> inputStream = mCacheInputStream.forget();
 
    rv = nsInputStreamPump::Create(getter_AddRefs(mCachePump), inputStream,
                                   int64_t(-1), int64_t(-1), 0, 0, true);
    if (NS_FAILED(rv)) {
        inputStream->Close();
        return rv;
    }

    rv = mCachePump->AsyncRead(this, mListenerContext);
    if (NS_FAILED(rv)) return rv;

    if (mTimingEnabled)
        mCacheReadStart = TimeStamp::Now();

    uint32_t suspendCount = mSuspendCount;
    while (suspendCount--)
        mCachePump->Suspend();

    return NS_OK;
}

void
nsHttpChannel::CloseCacheEntry(bool doomOnFailure)
{
    mCacheQuery = nullptr;
    mCacheInputStream.CloseAndRelease();

    if (!mCacheEntry)
        return;

    LOG(("nsHttpChannel::CloseCacheEntry [this=%p] mStatus=%x mCacheAccess=%x",
         this, mStatus, mCacheAccess));

    // If we have begun to create or replace a cache entry, and that cache
    // entry is not complete and not resumable, then it needs to be doomed.
    // Otherwise, CheckCache will make the mistake of thinking that the
    // partial cache entry is complete.

    bool doom = false;
    if (mInitedCacheEntry) {
        NS_ASSERTION(mResponseHead, "oops");
        if (NS_FAILED(mStatus) && doomOnFailure &&
            (mCacheAccess & nsICache::ACCESS_WRITE) &&
            !mResponseHead->IsResumable())
            doom = true;
    }
    else if (mCacheAccess == nsICache::ACCESS_WRITE)
        doom = true;

    if (doom) {
        LOG(("  dooming cache entry!!"));
        mCacheEntry->AsyncDoom(nullptr);
    }

    mCachedResponseHead = nullptr;

    mCachePump = 0;
    mCacheEntry = 0;
    mCacheAccess = 0;
    mInitedCacheEntry = false;
}


void
nsHttpChannel::CloseOfflineCacheEntry()
{
    if (!mOfflineCacheEntry)
        return;

    LOG(("nsHttpChannel::CloseOfflineCacheEntry [this=%p]", this));

    if (NS_FAILED(mStatus)) {
        mOfflineCacheEntry->AsyncDoom(nullptr);
    }
    else {
        bool succeeded;
        if (NS_SUCCEEDED(GetRequestSucceeded(&succeeded)) && !succeeded)
            mOfflineCacheEntry->AsyncDoom(nullptr);
    }

    mOfflineCacheEntry = 0;
    mOfflineCacheAccess = 0;
}


// Initialize the cache entry for writing.
//  - finalize storage policy
//  - store security info
//  - update expiration time
//  - store headers and other meta data
nsresult
nsHttpChannel::InitCacheEntry()
{
    nsresult rv;

    NS_ENSURE_TRUE(mCacheEntry, NS_ERROR_UNEXPECTED);
    // if only reading, nothing to be done here.
    if (mCacheAccess == nsICache::ACCESS_READ)
        return NS_OK;

    // Don't cache the response again if already cached...
    if (mCachedContentIsValid)
        return NS_OK;

    LOG(("nsHttpChannel::InitCacheEntry [this=%p entry=%p]\n",
        this, mCacheEntry.get()));

    if (mLoadFlags & INHIBIT_PERSISTENT_CACHING) {
        rv = mCacheEntry->SetStoragePolicy(nsICache::STORE_IN_MEMORY);
        if (NS_FAILED(rv)) return rv;
    }

    // Set the expiration time for this cache entry
    rv = UpdateExpirationTime();
    if (NS_FAILED(rv)) return rv;

    rv = AddCacheEntryHeaders(mCacheEntry);
    if (NS_FAILED(rv)) return rv;

    mInitedCacheEntry = true;
    return NS_OK;
}

void
nsHttpChannel::UpdateInhibitPersistentCachingFlag()
{
    // The no-store directive within the 'Cache-Control:' header indicates
    // that we must not store the response in a persistent cache.
    if (mResponseHead->NoStore())
        mLoadFlags |= INHIBIT_PERSISTENT_CACHING;

    // Only cache SSL content on disk if the pref is set
    if (!gHttpHandler->IsPersistentHttpsCachingEnabled() &&
        mConnectionInfo->UsingSSL())
        mLoadFlags |= INHIBIT_PERSISTENT_CACHING;
}

nsresult
nsHttpChannel::InitOfflineCacheEntry()
{
    // This function can be called even when we fail to connect (bug 551990)

    if (!mOfflineCacheEntry) {
        return NS_OK;
    }

    if (!mResponseHead || mResponseHead->NoStore()) {
        if (mResponseHead && mResponseHead->NoStore()) {
            mOfflineCacheEntry->AsyncDoom(nullptr);
        }

        CloseOfflineCacheEntry();

        if (mResponseHead && mResponseHead->NoStore()) {
            return NS_ERROR_NOT_AVAILABLE;
        }

        return NS_OK;
    }

    // This entry's expiration time should match the main entry's expiration
    // time.  UpdateExpirationTime() will keep it in sync once the offline
    // cache entry has been created.
    if (mCacheEntry) {
        uint32_t expirationTime;
        nsresult rv = mCacheEntry->GetExpirationTime(&expirationTime);
        NS_ENSURE_SUCCESS(rv, rv);

        mOfflineCacheEntry->SetExpirationTime(expirationTime);
    }

    return AddCacheEntryHeaders(mOfflineCacheEntry);
}


nsresult
nsHttpChannel::AddCacheEntryHeaders(nsICacheEntryDescriptor *entry)
{
    nsresult rv;

    LOG(("nsHttpChannel::AddCacheEntryHeaders [this=%x] begin", this));
    // Store secure data in memory only
    if (mSecurityInfo)
        entry->SetSecurityInfo(mSecurityInfo);

    // Store the HTTP request method with the cache entry so we can distinguish
    // for example GET and HEAD responses.
    rv = entry->SetMetaDataElement("request-method",
                                   mRequestHead.Method().get());
    if (NS_FAILED(rv)) return rv;

    // Store the HTTP authorization scheme used if any...
    rv = StoreAuthorizationMetaData(entry);
    if (NS_FAILED(rv)) return rv;

    // Iterate over the headers listed in the Vary response header, and
    // store the value of the corresponding request header so we can verify
    // that it has not varied when we try to re-use the cached response at
    // a later time.  Take care to store "Cookie" headers only as hashes
    // due to security considerations and the fact that they can be pretty
    // large (bug 468426). We take care of "Vary: cookie" in ResponseWouldVary.
    //
    // NOTE: if "Vary: accept, cookie", then we will store the "accept" header
    // in the cache.  we could try to avoid needlessly storing the "accept"
    // header in this case, but it doesn't seem worth the extra code to perform
    // the check.
    {
        nsAutoCString buf, metaKey;
        mResponseHead->GetHeader(nsHttp::Vary, buf);
        if (!buf.IsEmpty()) {
            NS_NAMED_LITERAL_CSTRING(prefix, "request-");
           
            char *val = buf.BeginWriting(); // going to munge buf
            char *token = nsCRT::strtok(val, NS_HTTP_HEADER_SEPS, &val);
            while (token) {
                LOG(("nsHttpChannel::AddCacheEntryHeaders [this=%x] " \
                        "processing %s", this, token));
                if (*token != '*') {
                    nsHttpAtom atom = nsHttp::ResolveAtom(token);
                    const char *val = mRequestHead.PeekHeader(atom);
                    nsAutoCString hash;
                    if (val) {
                        // If cookie-header, store a hash of the value
                        if (atom == nsHttp::Cookie) {
                            LOG(("nsHttpChannel::AddCacheEntryHeaders [this=%x] " \
                                    "cookie-value %s", this, val));
                            rv = Hash(val, hash);
                            // If hash failed, store a string not very likely
                            // to be the result of subsequent hashes
                            if (NS_FAILED(rv))
                                val = "<hash failed>";
                            else
                                val = hash.get();

                            LOG(("   hashed to %s\n", val));
                        }

                        // build cache meta data key and set meta data element...
                        metaKey = prefix + nsDependentCString(token);
                        entry->SetMetaDataElement(metaKey.get(), val);
                    } else {
                        LOG(("nsHttpChannel::AddCacheEntryHeaders [this=%x] " \
                                "clearing metadata for %s", this, token));
                        metaKey = prefix + nsDependentCString(token);
                        entry->SetMetaDataElement(metaKey.get(), nullptr);
                    }
                }
                token = nsCRT::strtok(val, NS_HTTP_HEADER_SEPS, &val);
            }
        }
    }


    // Store the received HTTP head with the cache entry as an element of
    // the meta data.
    nsAutoCString head;
    mResponseHead->Flatten(head, true);
    rv = entry->SetMetaDataElement("response-head", head.get());

    return rv;
}

inline void
GetAuthType(const char *challenge, nsCString &authType)
{
    const char *p;

    // get the challenge type
    if ((p = strchr(challenge, ' ')) != nullptr)
        authType.Assign(challenge, p - challenge);
    else
        authType.Assign(challenge);
}

nsresult
nsHttpChannel::StoreAuthorizationMetaData(nsICacheEntryDescriptor *entry)
{
    // Not applicable to proxy authorization...
    const char *val = mRequestHead.PeekHeader(nsHttp::Authorization);
    if (!val)
        return NS_OK;

    // eg. [Basic realm="wally world"]
    nsAutoCString buf;
    GetAuthType(val, buf);
    return entry->SetMetaDataElement("auth", buf.get());
}

// Finalize the cache entry
//  - may need to rewrite response headers if any headers changed
//  - may need to recalculate the expiration time if any headers changed
//  - called only for freshly written cache entries
nsresult
nsHttpChannel::FinalizeCacheEntry()
{
    LOG(("nsHttpChannel::FinalizeCacheEntry [this=%p]\n", this));

    if (mResponseHead && mResponseHeadersModified) {
        // Set the expiration time for this cache entry
        nsresult rv = UpdateExpirationTime();
        if (NS_FAILED(rv)) return rv;
    }
    return NS_OK;
}

// Open an output stream to the cache entry and insert a listener tee into
// the chain of response listeners.
nsresult
nsHttpChannel::InstallCacheListener(uint32_t offset)
{
    nsresult rv;

    LOG(("Preparing to write data into the cache [uri=%s]\n", mSpec.get()));

    NS_ASSERTION(mCacheEntry, "no cache entry");
    NS_ASSERTION(mListener, "no listener");

    // If the content is compressible and the server has not compressed it,
    // mark the cache entry for compression.
    if ((mResponseHead->PeekHeader(nsHttp::Content_Encoding) == nullptr) && (
         mResponseHead->ContentType().EqualsLiteral(TEXT_HTML) ||
         mResponseHead->ContentType().EqualsLiteral(TEXT_PLAIN) ||
         mResponseHead->ContentType().EqualsLiteral(TEXT_CSS) ||
         mResponseHead->ContentType().EqualsLiteral(TEXT_JAVASCRIPT) ||
         mResponseHead->ContentType().EqualsLiteral(TEXT_ECMASCRIPT) ||
         mResponseHead->ContentType().EqualsLiteral(TEXT_XML) ||
         mResponseHead->ContentType().EqualsLiteral(APPLICATION_JAVASCRIPT) ||
         mResponseHead->ContentType().EqualsLiteral(APPLICATION_ECMASCRIPT) ||
         mResponseHead->ContentType().EqualsLiteral(APPLICATION_XJAVASCRIPT) ||
         mResponseHead->ContentType().EqualsLiteral(APPLICATION_XHTML_XML))) {
        rv = mCacheEntry->SetMetaDataElement("uncompressed-len", "0"); 
        if (NS_FAILED(rv)) {
            LOG(("unable to mark cache entry for compression"));
        }
    } 

    LOG(("Trading cache input stream for output stream [channel=%p]", this));

    // We must close the input stream first because cache entries do not
    // correctly handle having an output stream and input streams open at
    // the same time.
    mCacheInputStream.CloseAndRelease();

    nsCOMPtr<nsIOutputStream> out;
    rv = mCacheEntry->OpenOutputStream(offset, getter_AddRefs(out));
    if (NS_FAILED(rv)) return rv;

    // XXX disk cache does not support overlapped i/o yet
#if 0
    // Mark entry valid inorder to allow simultaneous reading...
    rv = mCacheEntry->MarkValid();
    if (NS_FAILED(rv)) return rv;
#endif

    nsCOMPtr<nsIStreamListenerTee> tee =
        do_CreateInstance(kStreamListenerTeeCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsICacheService> serv =
        do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIEventTarget> cacheIOTarget;
    serv->GetCacheIOTarget(getter_AddRefs(cacheIOTarget));

    if (!cacheIOTarget) {
        LOG(("nsHttpChannel::InstallCacheListener sync tee %p rv=%x "
             "cacheIOTarget=%p", tee.get(), rv, cacheIOTarget.get()));
        rv = tee->Init(mListener, out, nullptr);
    } else {
        LOG(("nsHttpChannel::InstallCacheListener async tee %p", tee.get()));
        rv = tee->InitAsync(mListener, cacheIOTarget, out, nullptr);
    }

    if (NS_FAILED(rv)) return rv;
    mListener = tee;
    return NS_OK;
}

nsresult
nsHttpChannel::InstallOfflineCacheListener()
{
    nsresult rv;

    LOG(("Preparing to write data into the offline cache [uri=%s]\n",
         mSpec.get()));

    NS_ASSERTION(mOfflineCacheEntry, "no offline cache entry");
    NS_ASSERTION(mListener, "no listener");

    nsCOMPtr<nsIOutputStream> out;
    rv = mOfflineCacheEntry->OpenOutputStream(0, getter_AddRefs(out));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamListenerTee> tee =
        do_CreateInstance(kStreamListenerTeeCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = tee->Init(mListener, out, nullptr);
    if (NS_FAILED(rv)) return rv;

    mListener = tee;

    return NS_OK;
}

void
nsHttpChannel::ClearBogusContentEncodingIfNeeded()
{
    // For .gz files, apache sends both a Content-Type: application/x-gzip
    // as well as Content-Encoding: gzip, which is completely wrong.  In
    // this case, we choose to ignore the rogue Content-Encoding header. We
    // must do this early on so as to prevent it from being seen up stream.
    // The same problem exists for Content-Encoding: compress in default
    // Apache installs.
    if (mResponseHead->HasHeaderValue(nsHttp::Content_Encoding, "gzip") && (
        mResponseHead->ContentType().EqualsLiteral(APPLICATION_GZIP) ||
        mResponseHead->ContentType().EqualsLiteral(APPLICATION_GZIP2) ||
        mResponseHead->ContentType().EqualsLiteral(APPLICATION_GZIP3))) {
        // clear the Content-Encoding header
        mResponseHead->ClearHeader(nsHttp::Content_Encoding);
    }
    else if (mResponseHead->HasHeaderValue(nsHttp::Content_Encoding, "compress") && (
             mResponseHead->ContentType().EqualsLiteral(APPLICATION_COMPRESS) ||
             mResponseHead->ContentType().EqualsLiteral(APPLICATION_COMPRESS2))) {
        // clear the Content-Encoding header
        mResponseHead->ClearHeader(nsHttp::Content_Encoding);
    }
}

//-----------------------------------------------------------------------------
// nsHttpChannel <redirect>
//-----------------------------------------------------------------------------

nsresult
nsHttpChannel::SetupReplacementChannel(nsIURI       *newURI, 
                                       nsIChannel   *newChannel,
                                       bool          preserveMethod)
{
    LOG(("nsHttpChannel::SetupReplacementChannel "
         "[this=%p newChannel=%p preserveMethod=%d]",
         this, newChannel, preserveMethod));

    nsresult rv = HttpBaseChannel::SetupReplacementChannel(newURI, newChannel, preserveMethod);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(newChannel);
    if (!httpChannel)
        return NS_OK; // no other options to set

    // convey the mApplyConversion flag (bug 91862)
    nsCOMPtr<nsIEncodedChannel> encodedChannel = do_QueryInterface(httpChannel);
    if (encodedChannel)
        encodedChannel->SetApplyConversion(mApplyConversion);

    // transfer the resume information
    if (mResuming) {
        nsCOMPtr<nsIResumableChannel> resumableChannel(do_QueryInterface(newChannel));
        if (!resumableChannel) {
            NS_WARNING("Got asked to resume, but redirected to non-resumable channel!");
            return NS_ERROR_NOT_RESUMABLE;
        }
        resumableChannel->ResumeAt(mStartPos, mEntityID);
    }

    return NS_OK;
}

nsresult
nsHttpChannel::AsyncProcessRedirection(uint32_t redirectType)
{
    LOG(("nsHttpChannel::AsyncProcessRedirection [this=%p type=%u]\n",
        this, redirectType));

    const char *location = mResponseHead->PeekHeader(nsHttp::Location);

    // if a location header was not given, then we can't perform the redirect,
    // so just carry on as though this were a normal response.
    if (!location)
        return NS_ERROR_FAILURE;

    // make sure non-ASCII characters in the location header are escaped.
    nsAutoCString locationBuf;
    if (NS_EscapeURL(location, -1, esc_OnlyNonASCII, locationBuf))
        location = locationBuf.get();

    if (mRedirectionLimit == 0) {
        LOG(("redirection limit reached!\n"));
        return NS_ERROR_REDIRECT_LOOP;
    }

    mRedirectType = redirectType;

    LOG(("redirecting to: %s [redirection-limit=%u]\n",
        location, uint32_t(mRedirectionLimit)));

    nsresult rv = CreateNewURI(location, getter_AddRefs(mRedirectURI));

    if (NS_FAILED(rv)) {
        LOG(("Invalid URI for redirect: Location: %s\n", location));
        return NS_ERROR_CORRUPTED_CONTENT;
    }

    if (mApplicationCache) {
        // if we are redirected to a different origin check if there is a fallback
        // cache entry to fall back to. we don't care about file strict 
        // checking, at least mURI is not a file URI.
        if (!NS_SecurityCompareURIs(mURI, mRedirectURI, false)) {
            PushRedirectAsyncFunc(&nsHttpChannel::ContinueProcessRedirectionAfterFallback);
            bool waitingForRedirectCallback;
            (void)ProcessFallback(&waitingForRedirectCallback);
            if (waitingForRedirectCallback)
                return NS_OK;
            PopRedirectAsyncFunc(&nsHttpChannel::ContinueProcessRedirectionAfterFallback);
        }
    }

    return ContinueProcessRedirectionAfterFallback(NS_OK);
}

// Creates an URI to the given location using current URI for base and charset
nsresult
nsHttpChannel::CreateNewURI(const char *loc, nsIURI **newURI)
{
    nsCOMPtr<nsIIOService> ioService;
    nsresult rv = gHttpHandler->GetIOService(getter_AddRefs(ioService));
    if (NS_FAILED(rv)) return rv;

    // the new uri should inherit the origin charset of the current uri
    nsAutoCString originCharset;
    rv = mURI->GetOriginCharset(originCharset);
    if (NS_FAILED(rv))
        originCharset.Truncate();

    return ioService->NewURI(nsDependentCString(loc),
                             originCharset.get(),
                             mURI,
                             newURI);
}

nsresult
nsHttpChannel::ContinueProcessRedirectionAfterFallback(nsresult rv)
{
    if (NS_SUCCEEDED(rv) && mFallingBack) {
        // do not continue with redirect processing, fallback is in
        // progress now.
        return NS_OK;
    }

    // Kill the current cache entry if we are redirecting
    // back to ourself.
    bool redirectingBackToSameURI = false;
    if (mCacheEntry && (mCacheAccess & nsICache::ACCESS_WRITE) &&
        NS_SUCCEEDED(mURI->Equals(mRedirectURI, &redirectingBackToSameURI)) &&
        redirectingBackToSameURI)
            mCacheEntry->AsyncDoom(nullptr);

    // move the reference of the old location to the new one if the new
    // one has none.
    nsAutoCString ref;
    rv = mRedirectURI->GetRef(ref);
    if (NS_SUCCEEDED(rv) && ref.IsEmpty()) {
        mURI->GetRef(ref);
        if (!ref.IsEmpty()) {
            // NOTE: SetRef will fail if mRedirectURI is immutable
            // (e.g. an about: URI)... Oh well.
            mRedirectURI->SetRef(ref);
        }
    }

    bool rewriteToGET = nsHttp::ShouldRewriteRedirectToGET(
                                    mRedirectType, mRequestHead.Method());
      
    // prompt if the method is not safe (such as POST, PUT, DELETE, ...)
    if (!rewriteToGET &&
        !nsHttp::IsSafeMethod(mRequestHead.Method())) {
        rv = PromptTempRedirect();
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsIIOService> ioService;
    rv = gHttpHandler->GetIOService(getter_AddRefs(ioService));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChannel> newChannel;
    rv = ioService->NewChannelFromURI(mRedirectURI, getter_AddRefs(newChannel));
    if (NS_FAILED(rv)) return rv;

    rv = SetupReplacementChannel(mRedirectURI, newChannel, !rewriteToGET);
    if (NS_FAILED(rv)) return rv;

    uint32_t redirectFlags;
    if (nsHttp::IsPermanentRedirect(mRedirectType))
        redirectFlags = nsIChannelEventSink::REDIRECT_PERMANENT;
    else
        redirectFlags = nsIChannelEventSink::REDIRECT_TEMPORARY;

    // verify that this is a legal redirect
    mRedirectChannel = newChannel;

    PushRedirectAsyncFunc(&nsHttpChannel::ContinueProcessRedirection);
    rv = gHttpHandler->AsyncOnChannelRedirect(this, newChannel, redirectFlags);

    if (NS_SUCCEEDED(rv))
        rv = WaitForRedirectCallback();

    if (NS_FAILED(rv)) {
        AutoRedirectVetoNotifier notifier(this);
        PopRedirectAsyncFunc(&nsHttpChannel::ContinueProcessRedirection);
    }

    return rv;
}

nsresult
nsHttpChannel::ContinueProcessRedirection(nsresult rv)
{
    AutoRedirectVetoNotifier notifier(this);

    LOG(("ContinueProcessRedirection [rv=%x]\n", rv));
    if (NS_FAILED(rv))
        return rv;

    NS_PRECONDITION(mRedirectChannel, "No redirect channel?");

    // Make sure to do this _after_ calling OnChannelRedirect
    mRedirectChannel->SetOriginalURI(mOriginalURI);

    // And now, the deprecated way
    nsCOMPtr<nsIHttpEventSink> httpEventSink;
    GetCallback(httpEventSink);
    if (httpEventSink) {
        // NOTE: nsIHttpEventSink is only used for compatibility with pre-1.8
        // versions.
        rv = httpEventSink->OnRedirect(this, mRedirectChannel);
        if (NS_FAILED(rv))
            return rv;
    }
    // XXX we used to talk directly with the script security manager, but that
    // should really be handled by the event sink implementation.

    // begin loading the new channel
    rv = mRedirectChannel->AsyncOpen(mListener, mListenerContext);

    if (NS_FAILED(rv))
        return rv;

    // close down this channel
    Cancel(NS_BINDING_REDIRECTED);
    
    notifier.RedirectSucceeded();

    ReleaseListeners();

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel <auth>
//-----------------------------------------------------------------------------

NS_IMETHODIMP nsHttpChannel::OnAuthAvailable()
{
    LOG(("nsHttpChannel::OnAuthAvailable [this=%p]", this));

    // setting mAuthRetryPending flag and resuming the transaction
    // triggers process of throwing away the unauthenticated data already
    // coming from the network
    mAuthRetryPending = true;
    mProxyAuthPending = false;
    LOG(("Resuming the transaction, we got credentials from user"));
    mTransactionPump->Resume();
  
    return NS_OK;
}

NS_IMETHODIMP nsHttpChannel::OnAuthCancelled(bool userCancel)
{
    LOG(("nsHttpChannel::OnAuthCancelled [this=%p]", this));

    if (mTransactionPump) {
        // If the channel is trying to authenticate to a proxy and
        // that was canceled we cannot show the http response body
        // from the 40x as that might mislead the user into thinking
        // it was a end host response instead of a proxy reponse.
        // This must check explicitly whether a proxy auth was being done
        // because we do want to show the content if this is an error from
        // the origin server.
        if (mProxyAuthPending)
            Cancel(NS_ERROR_PROXY_CONNECTION_REFUSED);

        // ensure call of OnStartRequest of the current listener here,
        // it would not be called otherwise at all
        nsresult rv = CallOnStartRequest();

        // drop mAuthRetryPending flag and resume the transaction
        // this resumes load of the unauthenticated content data (which
        // may have been canceled if we don't want to show it)
        mAuthRetryPending = false;
        LOG(("Resuming the transaction, user cancelled the auth dialog"));
        mTransactionPump->Resume();

        if (NS_FAILED(rv))
            mTransactionPump->Cancel(rv);
    }

    mProxyAuthPending = false;
    return NS_OK;
}

NS_IMETHODIMP nsHttpChannel::GetAsciiHostForAuth(nsACString &host)
{
    if (mAuthProvider)
        return mAuthProvider->GetAsciiHostForAuth(host);

    nsresult rv;
    nsCOMPtr<nsIURI> uri;
    rv = GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv))
        return rv;
    return uri->GetAsciiHost(host);
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ADDREF_INHERITED(nsHttpChannel, HttpBaseChannel)
NS_IMPL_RELEASE_INHERITED(nsHttpChannel, HttpBaseChannel)

NS_INTERFACE_MAP_BEGIN(nsHttpChannel)
    NS_INTERFACE_MAP_ENTRY(nsIRequest)
    NS_INTERFACE_MAP_ENTRY(nsIChannel)
    NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
    NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
    NS_INTERFACE_MAP_ENTRY(nsIHttpChannel)
    NS_INTERFACE_MAP_ENTRY(nsICacheInfoChannel)
    NS_INTERFACE_MAP_ENTRY(nsICachingChannel)
    NS_INTERFACE_MAP_ENTRY(nsIUploadChannel)
    NS_INTERFACE_MAP_ENTRY(nsIUploadChannel2)
    NS_INTERFACE_MAP_ENTRY(nsICacheListener)
    NS_INTERFACE_MAP_ENTRY(nsIHttpChannelInternal)
    NS_INTERFACE_MAP_ENTRY(nsIResumableChannel)
    NS_INTERFACE_MAP_ENTRY(nsITransportEventSink)
    NS_INTERFACE_MAP_ENTRY(nsISupportsPriority)
    NS_INTERFACE_MAP_ENTRY(nsIProtocolProxyCallback)
    NS_INTERFACE_MAP_ENTRY(nsIProxiedChannel)
    NS_INTERFACE_MAP_ENTRY(nsIHttpAuthenticableChannel)
    NS_INTERFACE_MAP_ENTRY(nsIApplicationCacheContainer)
    NS_INTERFACE_MAP_ENTRY(nsIApplicationCacheChannel)
    NS_INTERFACE_MAP_ENTRY(nsIAsyncVerifyRedirectCallback)
    NS_INTERFACE_MAP_ENTRY(nsITimedChannel)
NS_INTERFACE_MAP_END_INHERITING(HttpBaseChannel)

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::Cancel(nsresult status)
{
    LOG(("nsHttpChannel::Cancel [this=%p status=%x]\n", this, status));
    if (mCanceled) {
        LOG(("  ignoring; already canceled\n"));
        return NS_OK;
    }
    if (mWaitingForRedirectCallback) {
        LOG(("channel canceled during wait for redirect callback"));
    }
    mCanceled = true;
    mStatus = status;
    if (mProxyRequest)
        mProxyRequest->Cancel(status);
    if (mTransaction)
        gHttpHandler->CancelTransaction(mTransaction, status);
    if (mTransactionPump)
        mTransactionPump->Cancel(status);
    mCacheQuery = nullptr;
    mCacheInputStream.CloseAndRelease();
    if (mCachePump)
        mCachePump->Cancel(status);
    if (mAuthProvider)
        mAuthProvider->Cancel(status);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::Suspend()
{
    NS_ENSURE_TRUE(mIsPending, NS_ERROR_NOT_AVAILABLE);
    
    LOG(("nsHttpChannel::Suspend [this=%p]\n", this));

    ++mSuspendCount;

    if (mTransactionPump)
        return mTransactionPump->Suspend();
    if (mCachePump)
        return mCachePump->Suspend();

    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::Resume()
{
    NS_ENSURE_TRUE(mSuspendCount > 0, NS_ERROR_UNEXPECTED);
    
    LOG(("nsHttpChannel::Resume [this=%p]\n", this));
        
    if (--mSuspendCount == 0 && mCallOnResume) {
        nsresult rv = AsyncCall(mCallOnResume);
        mCallOnResume = nullptr;
        NS_ENSURE_SUCCESS(rv, rv);
    }

    if (mTransactionPump)
        return mTransactionPump->Resume();
    if (mCachePump)
        return mCachePump->Resume();

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::GetSecurityInfo(nsISupports **securityInfo)
{
    NS_ENSURE_ARG_POINTER(securityInfo);
    *securityInfo = mSecurityInfo;
    NS_IF_ADDREF(*securityInfo);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *context)
{
    LOG(("nsHttpChannel::AsyncOpen [this=%p]\n", this));

    NS_ENSURE_ARG_POINTER(listener);
    NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
    NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

    nsresult rv;

    rv = NS_CheckPortSafety(mURI);
    if (NS_FAILED(rv)) {
        ReleaseListeners();
        return rv;
    }

    // Remember the cookie header that was set, if any
    const char *cookieHeader = mRequestHead.PeekHeader(nsHttp::Cookie);
    if (cookieHeader) {
        mUserSetCookieHeader = cookieHeader;
    }

    AddCookiesToRequest();

    // notify "http-on-opening-request" observers, but not if this is a redirect
    if (!(mLoadFlags & LOAD_REPLACE)) {
        gHttpHandler->OnOpeningRequest(this);
    }

    mIsPending = true;
    mWasOpened = true;

    mListener = listener;
    mListenerContext = context;

    // add ourselves to the load group.  from this point forward, we'll report
    // all failures asynchronously.
    if (mLoadGroup)
        mLoadGroup->AddRequest(this, nullptr);

    // record asyncopen time unconditionally and clear it if we
    // don't want it after OnModifyRequest() weighs in. But waiting for
    // that to complete would mean we don't include proxy resolution in the
    // timing.
    mAsyncOpenTime = TimeStamp::Now();

    // the only time we would already know the proxy information at this
    // point would be if we were proxying a non-http protocol like ftp
    if (!mProxyInfo && NS_SUCCEEDED(ResolveProxy()))
        return NS_OK;

    rv = BeginConnect();
    if (NS_FAILED(rv))
        ReleaseListeners();

    return rv;
}

nsresult
nsHttpChannel::BeginConnect()
{
    LOG(("nsHttpChannel::BeginConnect [this=%p]\n", this));
    nsresult rv;

    // notify "http-on-modify-request" observers
    gHttpHandler->OnModifyRequest(this);

    mRequestObserversCalled = true;

    // If mTimingEnabled flag is not set after OnModifyRequest() then
    // clear the already recorded AsyncOpen value for consistency.
    if (!mTimingEnabled)
        mAsyncOpenTime = TimeStamp();

    // Construct connection info object
    nsAutoCString host;
    int32_t port = -1;
    bool usingSSL = false;

    rv = mURI->SchemeIs("https", &usingSSL);
    if (NS_SUCCEEDED(rv))
        rv = mURI->GetAsciiHost(host);
    if (NS_SUCCEEDED(rv))
        rv = mURI->GetPort(&port);
    if (NS_SUCCEEDED(rv))
        rv = mURI->GetAsciiSpec(mSpec);
    if (NS_FAILED(rv))
        return rv;

    // Reject the URL if it doesn't specify a host
    if (host.IsEmpty())
        return NS_ERROR_MALFORMED_URI;
    LOG(("host=%s port=%d\n", host.get(), port));
    LOG(("uri=%s\n", mSpec.get()));

    nsCOMPtr<nsProxyInfo> proxyInfo;
    if (mProxyInfo)
        proxyInfo = do_QueryInterface(mProxyInfo);

    mConnectionInfo = new nsHttpConnectionInfo(host, port, proxyInfo, usingSSL);

    mAuthProvider =
        do_CreateInstance("@mozilla.org/network/http-channel-auth-provider;1",
                          &rv);
    if (NS_SUCCEEDED(rv))
        rv = mAuthProvider->Init(this);
    if (NS_FAILED(rv))
        return rv;

    // check to see if authorization headers should be included
    mAuthProvider->AddAuthorizationHeaders();

    // when proxying only use the pipeline bit if ProxyPipelining() allows it.
    if (!mConnectionInfo->UsingConnect() && mConnectionInfo->UsingHttpProxy()) {
        mCaps &= ~NS_HTTP_ALLOW_PIPELINING;
        if (gHttpHandler->ProxyPipelining())
            mCaps |= NS_HTTP_ALLOW_PIPELINING;
    }

    // if this somehow fails we can go on without it
    gHttpHandler->AddConnectionHeader(&mRequestHead.Headers(), mCaps);

    if (!mConnectionInfo->UsingHttpProxy()) {
        // Start a DNS lookup very early in case the real open is queued the DNS can 
        // happen in parallel. Do not do so in the presence of an HTTP proxy as 
        // all lookups other than for the proxy itself are done by the proxy.
        //
        // We keep the DNS prefetch object around so that we can retrieve
        // timing information from it. There is no guarantee that we actually
        // use the DNS prefetch data for the real connection, but as we keep
        // this data around for 3 minutes by default, this should almost always
        // be correct, and even when it isn't, the timing still represents _a_
        // valid DNS lookup timing for the site, even if it is not _the_
        // timing we used.
        mDNSPrefetch = new nsDNSPrefetch(mURI, mTimingEnabled);
        mDNSPrefetch->PrefetchHigh();
    }
    
    // Adjust mCaps according to our request headers:
    //  - If "Connection: close" is set as a request header, then do not bother
    //    trying to establish a keep-alive connection.
    if (mRequestHead.HasHeaderValue(nsHttp::Connection, "close"))
        mCaps &= ~(NS_HTTP_ALLOW_KEEPALIVE | NS_HTTP_ALLOW_PIPELINING);
    
    if ((mLoadFlags & VALIDATE_ALWAYS) || 
        (BYPASS_LOCAL_CACHE(mLoadFlags)))
        mCaps |= NS_HTTP_REFRESH_DNS;

    if (gHttpHandler->CritialRequestPrioritization()) {
        if (mLoadAsBlocking)
            mCaps |= NS_HTTP_LOAD_AS_BLOCKING;
        if (mLoadUnblocked)
            mCaps |= NS_HTTP_LOAD_UNBLOCKED;
    }

    // Force-Reload should reset the persistent connection pool for this host
    if (mLoadFlags & LOAD_FRESH_CONNECTION) {
        // just the initial document resets the whole pool
        if (mLoadFlags & LOAD_INITIAL_DOCUMENT_URI)
            gHttpHandler->ConnMgr()->ClosePersistentConnections();
        // each sub resource gets a fresh connection
        mCaps &= ~(NS_HTTP_ALLOW_KEEPALIVE | NS_HTTP_ALLOW_PIPELINING);
    }

    // We may have been cancelled already, either by on-modify-request
    // listeners or by load group observers; in that case, we should
    // not send the request to the server
    if (mCanceled)
        rv = mStatus;
    else
        rv = Connect();
    if (NS_FAILED(rv)) {
        LOG(("Calling AsyncAbort [rv=%x mCanceled=%i]\n", rv, mCanceled));
        CloseCacheEntry(true);
        AsyncAbort(rv);
    } else if (mLoadFlags & LOAD_CLASSIFY_URI) {
        nsRefPtr<nsChannelClassifier> classifier = new nsChannelClassifier();
        rv = classifier->Start(this);
        if (NS_FAILED(rv)) {
            Cancel(rv);
            return rv;
        }
    }

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIHttpChannelInternal
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::SetupFallbackChannel(const char *aFallbackKey)
{
    ENSURE_CALLED_BEFORE_CONNECT();

    LOG(("nsHttpChannel::SetupFallbackChannel [this=%x, key=%s]",
         this, aFallbackKey));
    mFallbackChannel = true;
    mFallbackKey = aFallbackKey;

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsISupportsPriority
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::SetPriority(int32_t value)
{
    int16_t newValue = clamped<int32_t>(value, INT16_MIN, INT16_MAX);
    if (mPriority == newValue)
        return NS_OK;
    mPriority = newValue;
    if (mTransaction)
        gHttpHandler->RescheduleTransaction(mTransaction, mPriority);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIProtocolProxyCallback
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::OnProxyAvailable(nsICancelable *request, nsIURI *uri,
                                nsIProxyInfo *pi, nsresult status)
{
    LOG(("nsHttpChannel::OnProxyAvailable [this=%p pi=%p status=%x mStatus=%x]\n",
         this, pi, status, mStatus));
    mProxyRequest = nullptr;

    nsresult rv;

    // If status is a failure code, then it means that we failed to resolve
    // proxy info.  That is a non-fatal error assuming it wasn't because the
    // request was canceled.  We just failover to DIRECT when proxy resolution
    // fails (failure can mean that the PAC URL could not be loaded).
    
    if (NS_SUCCEEDED(status))
        mProxyInfo = pi;

    if (!gHttpHandler->Active()) {
        LOG(("nsHttpChannel::OnProxyAvailable [this=%p] "
             "Handler no longer active.\n", this));
        rv = NS_ERROR_NOT_AVAILABLE;
    }
    else {
        rv = BeginConnect();
    }

    if (NS_FAILED(rv)) {
        Cancel(rv);
        DoNotifyListener();
    }
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIProxiedChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::GetProxyInfo(nsIProxyInfo **result)
{
    if (!mConnectionInfo)
        *result = mProxyInfo;
    else
        *result = mConnectionInfo->ProxyInfo();
    NS_IF_ADDREF(*result);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsITimedChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::SetTimingEnabled(bool enabled) {
    mTimingEnabled = enabled;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetTimingEnabled(bool* _retval) {
    *_retval = mTimingEnabled;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetChannelCreation(TimeStamp* _retval) {
    *_retval = mChannelCreationTimestamp;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetAsyncOpen(TimeStamp* _retval) {
    *_retval = mAsyncOpenTime;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetDomainLookupStart(TimeStamp* _retval) {
    if (mDNSPrefetch && mDNSPrefetch->TimingsValid())
        *_retval = mDNSPrefetch->StartTimestamp();
    else if (mTransaction)
        *_retval = mTransaction->Timings().domainLookupStart;
    else
        *_retval = mTransactionTimings.domainLookupStart;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetDomainLookupEnd(TimeStamp* _retval) {
    if (mDNSPrefetch && mDNSPrefetch->TimingsValid())
        *_retval = mDNSPrefetch->EndTimestamp();
    else if (mTransaction)
        *_retval = mTransaction->Timings().domainLookupEnd;
    else
        *_retval = mTransactionTimings.domainLookupEnd;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetConnectStart(TimeStamp* _retval) {
    if (mTransaction)
        *_retval = mTransaction->Timings().connectStart;
    else
        *_retval = mTransactionTimings.connectStart;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetConnectEnd(TimeStamp* _retval) {
    if (mTransaction)
        *_retval = mTransaction->Timings().connectEnd;
    else
        *_retval = mTransactionTimings.connectEnd;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetRequestStart(TimeStamp* _retval) {
    if (mTransaction)
        *_retval = mTransaction->Timings().requestStart;
    else
        *_retval = mTransactionTimings.requestStart;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetResponseStart(TimeStamp* _retval) {
    if (mTransaction)
        *_retval = mTransaction->Timings().responseStart;
    else
        *_retval = mTransactionTimings.responseStart;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetResponseEnd(TimeStamp* _retval) {
    if (mTransaction)
        *_retval = mTransaction->Timings().responseEnd;
    else
        *_retval = mTransactionTimings.responseEnd;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetCacheReadStart(TimeStamp* _retval) {
    *_retval = mCacheReadStart;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetCacheReadEnd(TimeStamp* _retval) {
    *_retval = mCacheReadEnd;
    return NS_OK;
}

#define IMPL_TIMING_ATTR(name)                                 \
NS_IMETHODIMP                                                  \
nsHttpChannel::Get##name##Time(PRTime* _retval) {              \
    TimeStamp stamp;                                  \
    Get##name(&stamp);                                         \
    if (stamp.IsNull()) {                                      \
        *_retval = 0;                                          \
        return NS_OK;                                          \
    }                                                          \
    *_retval = mChannelCreationTime +                          \
        (PRTime) ((stamp - mChannelCreationTimestamp).ToSeconds() * 1e6); \
    return NS_OK;                                              \
}

IMPL_TIMING_ATTR(ChannelCreation)
IMPL_TIMING_ATTR(AsyncOpen)
IMPL_TIMING_ATTR(DomainLookupStart)
IMPL_TIMING_ATTR(DomainLookupEnd)
IMPL_TIMING_ATTR(ConnectStart)
IMPL_TIMING_ATTR(ConnectEnd)
IMPL_TIMING_ATTR(RequestStart)
IMPL_TIMING_ATTR(ResponseStart)
IMPL_TIMING_ATTR(ResponseEnd)
IMPL_TIMING_ATTR(CacheReadStart)
IMPL_TIMING_ATTR(CacheReadEnd)

#undef IMPL_TIMING_ATTR

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIHttpAuthenticableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::GetIsSSL(bool *aIsSSL)
{
    *aIsSSL = mConnectionInfo->UsingSSL();
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetProxyMethodIsConnect(bool *aProxyMethodIsConnect)
{
    *aProxyMethodIsConnect = mConnectionInfo->UsingConnect();
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetServerResponseHeader(nsACString &value)
{
    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;
    return mResponseHead->GetHeader(nsHttp::Server, value);
}

NS_IMETHODIMP
nsHttpChannel::GetProxyChallenges(nsACString &value)
{
    if (!mResponseHead)
        return NS_ERROR_UNEXPECTED;
    return mResponseHead->GetHeader(nsHttp::Proxy_Authenticate, value);
}

NS_IMETHODIMP
nsHttpChannel::GetWWWChallenges(nsACString &value)
{
    if (!mResponseHead)
        return NS_ERROR_UNEXPECTED;
    return mResponseHead->GetHeader(nsHttp::WWW_Authenticate, value);
}

NS_IMETHODIMP
nsHttpChannel::SetProxyCredentials(const nsACString &value)
{
    return mRequestHead.SetHeader(nsHttp::Proxy_Authorization, value);
}

NS_IMETHODIMP
nsHttpChannel::SetWWWCredentials(const nsACString &value)
{
    return mRequestHead.SetHeader(nsHttp::Authorization, value);
}

//-----------------------------------------------------------------------------
// Methods that nsIHttpAuthenticableChannel dupes from other IDLs, which we
// get from HttpBaseChannel, must be explicitly forwarded, because C++ sucks.
//

NS_IMETHODIMP
nsHttpChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    return HttpBaseChannel::GetLoadFlags(aLoadFlags);
}

NS_IMETHODIMP
nsHttpChannel::GetURI(nsIURI **aURI)
{
    return HttpBaseChannel::GetURI(aURI);
}

NS_IMETHODIMP
nsHttpChannel::GetNotificationCallbacks(nsIInterfaceRequestor **aCallbacks)
{
    return HttpBaseChannel::GetNotificationCallbacks(aCallbacks);
}

NS_IMETHODIMP
nsHttpChannel::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
    return HttpBaseChannel::GetLoadGroup(aLoadGroup);
}

NS_IMETHODIMP
nsHttpChannel::GetRequestMethod(nsACString& aMethod)
{
    return HttpBaseChannel::GetRequestMethod(aMethod);
}


//-----------------------------------------------------------------------------
// nsHttpChannel::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
    SAMPLE_LABEL("nsHttpChannel", "OnStartRequest");
    if (!(mCanceled || NS_FAILED(mStatus))) {
        // capture the request's status, so our consumers will know ASAP of any
        // connection failures, etc - bug 93581
        request->GetStatus(&mStatus);
    }

    LOG(("nsHttpChannel::OnStartRequest [this=%p request=%p status=%x]\n",
        this, request, mStatus));

    // Make sure things are what we expect them to be...
    NS_ASSERTION(request == mCachePump || request == mTransactionPump,
                 "Unexpected request");
    NS_ASSERTION(!(mTransactionPump && mCachePump) || mCachedContentIsPartial,
                 "If we have both pumps, the cache content must be partial");

    if (!mSecurityInfo && !mCachePump && mTransaction) {
        // grab the security info from the connection object; the transaction
        // is guaranteed to own a reference to the connection.
        mSecurityInfo = mTransaction->SecurityInfo();
    }

    if (!mCachePump && NS_FAILED(mStatus) &&
        (mLoadFlags & LOAD_REPLACE) && mOriginalURI && mAllowSpdy) {
        // For sanity's sake we may want to cancel an alternate protocol
        // redirection involving the original host name

        nsAutoCString hostPort;
        if (NS_SUCCEEDED(mOriginalURI->GetHostPort(hostPort)))
            gHttpHandler->ConnMgr()->RemoveSpdyAlternateProtocol(hostPort);
    }

    // don't enter this block if we're reading from the cache...
    if (NS_SUCCEEDED(mStatus) && !mCachePump && mTransaction) {
        // mTransactionPump doesn't hit OnInputStreamReady and call this until
        // all of the response headers have been acquired, so we can take ownership
        // of them from the transaction.
        mResponseHead = mTransaction->TakeResponseHead();
        // the response head may be null if the transaction was cancelled.  in
        // which case we just need to call OnStartRequest/OnStopRequest.
        if (mResponseHead)
            return ProcessResponse();

        NS_WARNING("No response head in OnStartRequest");
    }

    // avoid crashing if mListener happens to be null...
    if (!mListener) {
        NS_NOTREACHED("mListener is null");
        return NS_OK;
    }

    // on proxy errors, try to failover
    if (mConnectionInfo->ProxyInfo() &&
       (mStatus == NS_ERROR_PROXY_CONNECTION_REFUSED ||
        mStatus == NS_ERROR_UNKNOWN_PROXY_HOST ||
        mStatus == NS_ERROR_NET_TIMEOUT)) {

        PushRedirectAsyncFunc(&nsHttpChannel::ContinueOnStartRequest1);
        if (NS_SUCCEEDED(ProxyFailover()))
            return NS_OK;
        PopRedirectAsyncFunc(&nsHttpChannel::ContinueOnStartRequest1);
    }

    return ContinueOnStartRequest2(NS_OK);
}

nsresult
nsHttpChannel::ContinueOnStartRequest1(nsresult result)
{
    // Success indicates we passed ProxyFailover, in that case we must not continue
    // with this code chain.
    if (NS_SUCCEEDED(result))
        return NS_OK;

    return ContinueOnStartRequest2(result);
}

nsresult
nsHttpChannel::ContinueOnStartRequest2(nsresult result)
{
    // on other request errors, try to fall back
    if (NS_FAILED(mStatus)) {
        PushRedirectAsyncFunc(&nsHttpChannel::ContinueOnStartRequest3);
        bool waitingForRedirectCallback;
        ProcessFallback(&waitingForRedirectCallback);
        if (waitingForRedirectCallback)
            return NS_OK;
        PopRedirectAsyncFunc(&nsHttpChannel::ContinueOnStartRequest3);
    }

    return ContinueOnStartRequest3(NS_OK);
}

nsresult
nsHttpChannel::ContinueOnStartRequest3(nsresult result)
{
    if (mFallingBack)
        return NS_OK;

    return CallOnStartRequest();
}

NS_IMETHODIMP
nsHttpChannel::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult status)
{
    SAMPLE_LABEL("network", "nsHttpChannel::OnStopRequest");
    LOG(("nsHttpChannel::OnStopRequest [this=%p request=%p status=%x]\n",
        this, request, status));

    if (mTimingEnabled && request == mCachePump) {
        mCacheReadEnd = TimeStamp::Now();
    }

     // allow content to be cached if it was loaded successfully (bug #482935)
     bool contentComplete = NS_SUCCEEDED(status);

    // honor the cancelation status even if the underlying transaction completed.
    if (mCanceled || NS_FAILED(mStatus))
        status = mStatus;

    if (mCachedContentIsPartial) {
        if (NS_SUCCEEDED(status)) {
            // mTransactionPump should be suspended
            NS_ASSERTION(request != mTransactionPump,
                "byte-range transaction finished prematurely");

            if (request == mCachePump) {
                bool streamDone;
                status = OnDoneReadingPartialCacheEntry(&streamDone);
                if (NS_SUCCEEDED(status) && !streamDone)
                    return status;
                // otherwise, fall through and fire OnStopRequest...
            }
            else
                NS_NOTREACHED("unexpected request");
        }
        // Do not to leave the transaction in a suspended state in error cases.
        if (NS_FAILED(status) && mTransaction)
            gHttpHandler->CancelTransaction(mTransaction, status); 
    }

    if (mTransaction) {
        // determine if we should call DoAuthRetry
        bool authRetry = mAuthRetryPending && NS_SUCCEEDED(status);

        //
        // grab references to connection in case we need to retry an
        // authentication request over it or use it for an upgrade
        // to another protocol.
        //
        // this code relies on the code in nsHttpTransaction::Close, which
        // tests for NS_HTTP_STICKY_CONNECTION to determine whether or not to
        // keep the connection around after the transaction is finished.
        //
        nsRefPtr<nsAHttpConnection> conn;
        if (authRetry && (mCaps & NS_HTTP_STICKY_CONNECTION)) {
            conn = mTransaction->Connection();
            // This is so far a workaround to fix leak when reusing unpersistent
            // connection for authentication retry. See bug 459620 comment 4
            // for details.
            if (conn && !conn->IsPersistent())
                conn = nullptr;
        }

        nsRefPtr<nsAHttpConnection> stickyConn;
        if (mCaps & NS_HTTP_STICKY_CONNECTION)
            stickyConn = mTransaction->Connection();
        
        // at this point, we're done with the transaction
        mTransactionTimings = mTransaction->Timings();
        mTransaction = nullptr;
        mTransactionPump = 0;

        // We no longer need the dns prefetch object
        if (mDNSPrefetch && mDNSPrefetch->TimingsValid()) {
            mTransactionTimings.domainLookupStart =
                mDNSPrefetch->StartTimestamp();
            mTransactionTimings.domainLookupEnd =
                mDNSPrefetch->EndTimestamp();
        }
        mDNSPrefetch = nullptr;

        // handle auth retry...
        if (authRetry) {
            mAuthRetryPending = false;
            status = DoAuthRetry(conn);
            if (NS_SUCCEEDED(status))
                return NS_OK;
        }

        // If DoAuthRetry failed, or if we have been cancelled since showing
        // the auth. dialog, then we need to send OnStartRequest now
        if (authRetry || (mAuthRetryPending && NS_FAILED(status))) {
            NS_ASSERTION(NS_FAILED(status), "should have a failure code here");
            // NOTE: since we have a failure status, we can ignore the return
            // value from onStartRequest.
            mListener->OnStartRequest(this, mListenerContext);
        }

        // if this transaction has been replaced, then bail.
        if (mTransactionReplaced)
            return NS_OK;
        
        if (mUpgradeProtocolCallback && stickyConn &&
            mResponseHead && mResponseHead->Status() == 101) {
            gHttpHandler->ConnMgr()->CompleteUpgrade(stickyConn,
                                                     mUpgradeProtocolCallback);
        }
    }

    mIsPending = false;
    mStatus = status;

    // perform any final cache operations before we close the cache entry.
    if (mCacheEntry && (mCacheAccess & nsICache::ACCESS_WRITE) &&
        mRequestTimeInitialized){
        FinalizeCacheEntry();
    }
    
    if (mListener) {
        LOG(("  calling OnStopRequest\n"));
        mListener->OnStopRequest(this, mListenerContext, status);
    }

    CloseCacheEntry(!contentComplete);

    if (mOfflineCacheEntry)
        CloseOfflineCacheEntry();

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nullptr, status);

    // We don't need this info anymore
    CleanRedirectCacheChainIfNecessary();

    ReleaseListeners();
    
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::OnDataAvailable(nsIRequest *request, nsISupports *ctxt,
                               nsIInputStream *input,
                               uint64_t offset, uint32_t count)
{
    SAMPLE_LABEL("network", "nsHttpChannel::OnDataAvailable");
    LOG(("nsHttpChannel::OnDataAvailable [this=%p request=%p offset=%llu count=%u]\n",
        this, request, offset, count));

    // don't send out OnDataAvailable notifications if we've been canceled.
    if (mCanceled)
        return mStatus;

    NS_ASSERTION(mResponseHead, "No response head in ODA!!");

    NS_ASSERTION(!(mCachedContentIsPartial && (request == mTransactionPump)),
            "transaction pump not suspended");

    if (mAuthRetryPending || (request == mTransactionPump && mTransactionReplaced)) {
        uint32_t n;
        return input->ReadSegments(NS_DiscardSegment, nullptr, count, &n);
    }

    if (mListener) {
        //
        // synthesize transport progress event.  we do this here since we want
        // to delay OnProgress events until we start streaming data.  this is
        // crucially important since it impacts the lock icon (see bug 240053).
        //
        nsresult transportStatus;
        if (request == mCachePump)
            transportStatus = NS_NET_STATUS_READING;
        else
            transportStatus = NS_NET_STATUS_RECEIVING_FROM;

        // mResponseHead may reference new or cached headers, but either way it
        // holds our best estimate of the total content length.  Even in the case
        // of a byte range request, the content length stored in the cached
        // response headers is what we want to use here.

        uint64_t progressMax(uint64_t(mResponseHead->ContentLength()));
        uint64_t progress = mLogicalOffset + uint64_t(count);
        NS_ASSERTION(progress <= progressMax, "unexpected progress values");

        OnTransportStatus(nullptr, transportStatus, progress, progressMax);

        //
        // we have to manually keep the logical offset of the stream up-to-date.
        // we cannot depend solely on the offset provided, since we may have 
        // already streamed some data from another source (see, for example,
        // OnDoneReadingPartialCacheEntry).
        //

        nsresult rv =  mListener->OnDataAvailable(this,
                                                  mListenerContext,
                                                  input,
                                                  mLogicalOffset,
                                                  count);
        if (NS_SUCCEEDED(rv))
            mLogicalOffset = progress;
        return rv;
    }

    return NS_ERROR_ABORT;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsITransportEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::OnTransportStatus(nsITransport *trans, nsresult status,
                                 uint64_t progress, uint64_t progressMax)
{
    // cache the progress sink so we don't have to query for it each time.
    if (!mProgressSink)
        GetCallback(mProgressSink);

    if (status == NS_NET_STATUS_CONNECTED_TO ||
        status == NS_NET_STATUS_WAITING_FOR) {
        nsCOMPtr<nsISocketTransport> socketTransport =
            do_QueryInterface(trans);
        if (socketTransport) {
            socketTransport->GetSelfAddr(&mSelfAddr);
            socketTransport->GetPeerAddr(&mPeerAddr);
        }
    }

    // block socket status event after Cancel or OnStopRequest has been called.
    if (mProgressSink && NS_SUCCEEDED(mStatus) && mIsPending && !(mLoadFlags & LOAD_BACKGROUND)) {
        LOG(("sending status notification [this=%p status=%x progress=%llu/%llu]\n",
            this, status, progress, progressMax));

        nsAutoCString host;
        mURI->GetHost(host);
        mProgressSink->OnStatus(this, nullptr, status,
                                NS_ConvertUTF8toUTF16(host).get());

        if (progress > 0) {
            NS_ASSERTION(progress <= progressMax, "unexpected progress values");
            mProgressSink->OnProgress(this, nullptr, progress, progressMax);
        }
    }
#ifdef DEBUG
    else
        LOG(("skipping status notification [this=%p sink=%p pending=%u background=%x]\n",
            this, mProgressSink.get(), mIsPending, (mLoadFlags & LOAD_BACKGROUND)));
#endif

    return NS_OK;
} 

//-----------------------------------------------------------------------------
// nsHttpChannel::nsICacheInfoChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::IsFromCache(bool *value)
{
    if (!mIsPending)
        return NS_ERROR_NOT_AVAILABLE;

    // return false if reading a partial cache entry; the data isn't entirely
    // from the cache!

    *value = (mCachePump || (mLoadFlags & LOAD_ONLY_IF_MODIFIED)) &&
              mCachedContentIsValid && !mCachedContentIsPartial;

    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetCacheTokenExpirationTime(uint32_t *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    if (!mCacheEntry)
        return NS_ERROR_NOT_AVAILABLE;

    return mCacheEntry->GetExpirationTime(_retval);
}

NS_IMETHODIMP
nsHttpChannel::GetCacheTokenCachedCharset(nsACString &_retval)
{
    nsresult rv;

    if (!mCacheEntry)
        return NS_ERROR_NOT_AVAILABLE;

    nsXPIDLCString cachedCharset;
    rv = mCacheEntry->GetMetaDataElement("charset",
                                         getter_Copies(cachedCharset));
    if (NS_SUCCEEDED(rv))
        _retval = cachedCharset;

    return rv;
}

NS_IMETHODIMP
nsHttpChannel::SetCacheTokenCachedCharset(const nsACString &aCharset)
{
    if (!mCacheEntry)
        return NS_ERROR_NOT_AVAILABLE;

    return mCacheEntry->SetMetaDataElement("charset",
                                           PromiseFlatCString(aCharset).get());
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsICachingChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::GetCacheToken(nsISupports **token)
{
    NS_ENSURE_ARG_POINTER(token);
    if (!mCacheEntry)
        return NS_ERROR_NOT_AVAILABLE;
    return CallQueryInterface(mCacheEntry, token);
}

NS_IMETHODIMP
nsHttpChannel::SetCacheToken(nsISupports *token)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpChannel::GetOfflineCacheToken(nsISupports **token)
{
    NS_ENSURE_ARG_POINTER(token);
    if (!mOfflineCacheEntry)
        return NS_ERROR_NOT_AVAILABLE;
    return CallQueryInterface(mOfflineCacheEntry, token);
}

NS_IMETHODIMP
nsHttpChannel::SetOfflineCacheToken(nsISupports *token)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

class nsHttpChannelCacheKey MOZ_FINAL : public nsISupportsPRUint32,
                                        public nsISupportsCString
{
    NS_DECL_ISUPPORTS

    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_FORWARD_NSISUPPORTSPRUINT32(mSupportsPRUint32->)
    
    // Both interfaces declares toString method with the same signature.
    // Thus we have to delegate only to nsISupportsPRUint32 implementation.
    NS_IMETHOD GetData(nsACString & aData) 
    { 
        return mSupportsCString->GetData(aData);
    }
    NS_IMETHOD SetData(const nsACString & aData)
    { 
        return mSupportsCString->SetData(aData);
    }
    
public:
    nsresult SetData(uint32_t aPostID, const nsACString& aKey);

protected:
    nsCOMPtr<nsISupportsPRUint32> mSupportsPRUint32;
    nsCOMPtr<nsISupportsCString> mSupportsCString;
};

NS_IMPL_ADDREF(nsHttpChannelCacheKey)
NS_IMPL_RELEASE(nsHttpChannelCacheKey)
NS_INTERFACE_TABLE_HEAD(nsHttpChannelCacheKey)
NS_INTERFACE_TABLE_BEGIN
NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(nsHttpChannelCacheKey,
                                   nsISupports, nsISupportsPRUint32)
NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(nsHttpChannelCacheKey,
                                   nsISupportsPrimitive, nsISupportsPRUint32)
NS_INTERFACE_TABLE_ENTRY(nsHttpChannelCacheKey,
                         nsISupportsPRUint32)
NS_INTERFACE_TABLE_ENTRY(nsHttpChannelCacheKey,
                         nsISupportsCString)
NS_INTERFACE_TABLE_END
NS_INTERFACE_TABLE_TAIL

NS_IMETHODIMP nsHttpChannelCacheKey::GetType(uint16_t *aType)
{
    NS_ENSURE_ARG_POINTER(aType);

    *aType = TYPE_PRUINT32;
    return NS_OK;
}

nsresult nsHttpChannelCacheKey::SetData(uint32_t aPostID,
                                        const nsACString& aKey)
{
    nsresult rv;

    mSupportsCString = 
        do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    mSupportsCString->SetData(aKey);
    if (NS_FAILED(rv)) return rv;

    mSupportsPRUint32 = 
        do_CreateInstance(NS_SUPPORTS_PRUINT32_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    mSupportsPRUint32->SetData(aPostID);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetCacheKey(nsISupports **key)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(key);

    LOG(("nsHttpChannel::GetCacheKey [this=%p]\n", this));

    *key = nullptr;

    nsRefPtr<nsHttpChannelCacheKey> container =
        new nsHttpChannelCacheKey();

    if (!container)
        return NS_ERROR_OUT_OF_MEMORY;

    nsAutoCString cacheKey;
    rv = GenerateCacheKey(mPostID, cacheKey);
    if (NS_FAILED(rv)) return rv;

    rv = container->SetData(mPostID, cacheKey);
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface(container.get(), key);
}

NS_IMETHODIMP
nsHttpChannel::SetCacheKey(nsISupports *key)
{
    nsresult rv;

    LOG(("nsHttpChannel::SetCacheKey [this=%p key=%p]\n", this, key));

    ENSURE_CALLED_BEFORE_CONNECT();

    if (!key)
        mPostID = 0;
    else {
        // extract the post id
        nsCOMPtr<nsISupportsPRUint32> container = do_QueryInterface(key, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = container->GetData(&mPostID);
        if (NS_FAILED(rv)) return rv;
    }
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIResumableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::ResumeAt(uint64_t aStartPos,
                        const nsACString& aEntityID)
{
    LOG(("nsHttpChannel::ResumeAt [this=%p startPos=%llu id='%s']\n",
         this, aStartPos, PromiseFlatCString(aEntityID).get()));
    mEntityID = aEntityID;
    mStartPos = aStartPos;
    mResuming = true;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsICacheListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::OnCacheEntryAvailable(nsICacheEntryDescriptor *entry,
                                     nsCacheAccessMode access,
                                     nsresult status)
{
    MOZ_ASSERT(NS_IsMainThread());

    nsresult rv;

    LOG(("nsHttpChannel::OnCacheEntryAvailable [this=%p entry=%p "
         "access=%x status=%x]\n", this, entry, access, status));

    if (mCacheQuery) {
        mRequestHead = mCacheQuery->mRequestHead;
        mRedirectedCachekeys = mCacheQuery->mRedirectedCachekeys.forget();
        mCacheInputStream.takeOver(mCacheQuery->mCacheInputStream);
        mCachedResponseHead = mCacheQuery->mCachedResponseHead.forget();
        mCachedSecurityInfo = mCacheQuery->mCachedSecurityInfo.forget();
        mCachedContentIsValid = mCacheQuery->mCachedContentIsValid;
        mCachedContentIsPartial = mCacheQuery->mCachedContentIsPartial;
        mCustomConditionalRequest = mCacheQuery->mCustomConditionalRequest;
        mDidReval = mCacheQuery->mDidReval;
        mCacheEntryDeviceTelemetryID = mCacheQuery->mCacheEntryDeviceTelemetryID;
        mCacheQuery = nullptr;
    }

    // if the channel's already fired onStopRequest, then we should ignore
    // this event.
    if (!mIsPending) {
        mCacheInputStream.CloseAndRelease();
        return NS_OK;
    }

    rv = OnCacheEntryAvailableInternal(entry, access, status);

    if (NS_FAILED(rv)) {
        CloseCacheEntry(true);
        AsyncAbort(rv);
    }

    return NS_OK;
}

nsresult
nsHttpChannel::OnCacheEntryAvailableInternal(nsICacheEntryDescriptor *entry,
                                             nsCacheAccessMode access,
                                             nsresult status)
{
    nsresult rv;

    nsOnCacheEntryAvailableCallback callback = mOnCacheEntryAvailableCallback;
    mOnCacheEntryAvailableCallback = nullptr;

    NS_ASSERTION(callback,
        "nsHttpChannel::OnCacheEntryAvailable called without callback");
    rv = ((*this).*callback)(entry, access, status);

    if (mOnCacheEntryAvailableCallback) {
        // callback fired another async open
        NS_ASSERTION(NS_SUCCEEDED(rv), "Unexpected state");
        return NS_OK;
    }

    if (callback != &nsHttpChannel::OnOfflineCacheEntryForWritingAvailable) {
        if (NS_FAILED(rv)) {
            LOG(("AsyncOpenCacheEntry failed [rv=%x]\n", rv));
            if (mLoadFlags & LOAD_ONLY_FROM_CACHE) {
                // If we have a fallback URI (and we're not already
                // falling back), process the fallback asynchronously.
                if (!mFallbackChannel && !mFallbackKey.IsEmpty()) {
                    return AsyncCall(&nsHttpChannel::HandleAsyncFallback);
                }
                return NS_ERROR_DOCUMENT_NOT_CACHED;
            }
            if (mCanceled)
                // If the request was canceled then don't continue without using
                // the cache entry. See bug #764337
                return rv;

            // proceed without using the cache
        }

        // if app cache for write has been set, open up an offline cache entry
        // to update
        if (mApplicationCacheForWrite) {
            rv = OpenOfflineCacheEntryForWriting();
            if (mOnCacheEntryAvailableCallback) {
                NS_ASSERTION(NS_SUCCEEDED(rv), "Unexpected state");
                return NS_OK;
            }

            if (NS_FAILED(rv))
                return rv;
        }
    } else {
        // check result of OnOfflineCacheEntryForWritingAvailable()
        if (NS_FAILED(rv))
            return rv;
    }

    return ContinueConnect();
}

NS_IMETHODIMP
nsHttpChannel::OnCacheEntryDoomed(nsresult status)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsHttpChannel::DoAuthRetry(nsAHttpConnection *conn)
{
    LOG(("nsHttpChannel::DoAuthRetry [this=%p]\n", this));

    NS_ASSERTION(!mTransaction, "should not have a transaction");
    nsresult rv;

    // toggle mIsPending to allow nsIObserver implementations to modify
    // the request headers (bug 95044).
    mIsPending = false;

    // fetch cookies, and add them to the request header.
    // the server response could have included cookies that must be sent with
    // this authentication attempt (bug 84794).
    // TODO: save cookies from auth response and send them here (bug 572151).
    AddCookiesToRequest();
    
    // notify "http-on-modify-request" observers
    gHttpHandler->OnModifyRequest(this);

    mIsPending = true;

    // get rid of the old response headers
    mResponseHead = nullptr;

    // set sticky connection flag and disable pipelining.
    mCaps |=  NS_HTTP_STICKY_CONNECTION;
    mCaps &= ~NS_HTTP_ALLOW_PIPELINING;
   
    // and create a new one...
    rv = SetupTransaction();
    if (NS_FAILED(rv)) return rv;

    // transfer ownership of connection to transaction
    if (conn)
        mTransaction->SetConnection(conn);

    // rewind the upload stream
    if (mUploadStream) {
        nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mUploadStream);
        if (seekable)
            seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
    }

    rv = gHttpHandler->InitiateTransaction(mTransaction, mPriority);
    if (NS_FAILED(rv)) return rv;

    rv = mTransactionPump->AsyncRead(this, nullptr);
    if (NS_FAILED(rv)) return rv;

    uint32_t suspendCount = mSuspendCount;
    while (suspendCount--)
        mTransactionPump->Suspend();

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIApplicationCacheChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::GetApplicationCache(nsIApplicationCache **out)
{
    NS_IF_ADDREF(*out = mApplicationCache);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetApplicationCache(nsIApplicationCache *appCache)
{
    ENSURE_CALLED_BEFORE_CONNECT();

    mApplicationCache = appCache;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetApplicationCacheForWrite(nsIApplicationCache **out)
{
    NS_IF_ADDREF(*out = mApplicationCacheForWrite);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetApplicationCacheForWrite(nsIApplicationCache *appCache)
{
    ENSURE_CALLED_BEFORE_CONNECT();

    mApplicationCacheForWrite = appCache;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetLoadedFromApplicationCache(bool *aLoadedFromApplicationCache)
{
    *aLoadedFromApplicationCache = mLoadedFromApplicationCache;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetInheritApplicationCache(bool *aInherit)
{
    *aInherit = mInheritApplicationCache;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetInheritApplicationCache(bool aInherit)
{
    ENSURE_CALLED_BEFORE_CONNECT();

    mInheritApplicationCache = aInherit;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetChooseApplicationCache(bool *aChoose)
{
    *aChoose = mChooseApplicationCache;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetChooseApplicationCache(bool aChoose)
{
    ENSURE_CALLED_BEFORE_CONNECT();

    mChooseApplicationCache = aChoose;
    return NS_OK;
}

nsHttpChannel::OfflineCacheEntryAsForeignMarker*
nsHttpChannel::GetOfflineCacheEntryAsForeignMarker()
{
    if (!mApplicationCache)
        return nullptr;

    nsresult rv;

    nsAutoCString cacheKey;
    rv = GenerateCacheKey(mPostID, cacheKey);
    NS_ENSURE_SUCCESS(rv, nullptr);

    return new OfflineCacheEntryAsForeignMarker(mApplicationCache, cacheKey);
}

nsresult
nsHttpChannel::OfflineCacheEntryAsForeignMarker::MarkAsForeign()
{
    return mApplicationCache->MarkEntry(mCacheKey,
                                        nsIApplicationCache::ITEM_FOREIGN);
}

NS_IMETHODIMP
nsHttpChannel::MarkOfflineCacheEntryAsForeign()
{
    nsresult rv;

    nsAutoPtr<OfflineCacheEntryAsForeignMarker> marker(
        GetOfflineCacheEntryAsForeignMarker());

    if (!marker)
        return NS_ERROR_NOT_AVAILABLE;

    rv = marker->MarkAsForeign();
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIAsyncVerifyRedirectCallback
//-----------------------------------------------------------------------------

nsresult
nsHttpChannel::WaitForRedirectCallback()
{
    nsresult rv;
    LOG(("nsHttpChannel::WaitForRedirectCallback [this=%p]\n", this));

    if (mTransactionPump) {
        rv = mTransactionPump->Suspend();
        NS_ENSURE_SUCCESS(rv, rv);
    }
    if (mCachePump) {
        rv = mCachePump->Suspend();
        if (NS_FAILED(rv) && mTransactionPump) {
#ifdef DEBUG
            nsresult resume = 
#endif
            mTransactionPump->Resume();
            NS_ASSERTION(NS_SUCCEEDED(resume),
                "Failed to resume transaction pump");
        }
        NS_ENSURE_SUCCESS(rv, rv);
    }

    mWaitingForRedirectCallback = true;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::OnRedirectVerifyCallback(nsresult result)
{
    LOG(("nsHttpChannel::OnRedirectVerifyCallback [this=%p] "
         "result=%x stack=%d mWaitingForRedirectCallback=%u\n",
         this, result, mRedirectFuncStack.Length(), mWaitingForRedirectCallback));
    NS_ASSERTION(mWaitingForRedirectCallback,
                 "Someone forgot to call WaitForRedirectCallback() ?!");
    mWaitingForRedirectCallback = false;

    if (mCanceled && NS_SUCCEEDED(result))
        result = NS_BINDING_ABORTED;

    for (uint32_t i = mRedirectFuncStack.Length(); i > 0;) {
        --i;
        // Pop the last function pushed to the stack
        nsContinueRedirectionFunc func = mRedirectFuncStack[i];
        mRedirectFuncStack.RemoveElementAt(mRedirectFuncStack.Length() - 1);

        // Call it with the result we got from the callback or the deeper
        // function call.
        result = (this->*func)(result);

        // If a new function has been pushed to the stack and placed us in the
        // waiting state, we need to break the chain and wait for the callback
        // again.
        if (mWaitingForRedirectCallback)
            break;
    }

    if (NS_FAILED(result) && !mCanceled) {
        // First, cancel this channel if we are in failure state to set mStatus
        // and let it be propagated to pumps.
        Cancel(result);
    }

    if (!mWaitingForRedirectCallback) {
        // We are not waiting for the callback. At this moment we must release
        // reference to the redirect target channel, otherwise we may leak.
        mRedirectChannel = nullptr;
    }

    // We always resume the pumps here. If all functions on stack have been
    // called we need OnStopRequest to be triggered, and if we broke out of the
    // loop above (and are thus waiting for a new callback) the suspension
    // count must be balanced in the pumps.
    if (mTransactionPump)
        mTransactionPump->Resume();
    if (mCachePump)
        mCachePump->Resume();

    return result;
}

void
nsHttpChannel::PushRedirectAsyncFunc(nsContinueRedirectionFunc func)
{
    mRedirectFuncStack.AppendElement(func);
}

void
nsHttpChannel::PopRedirectAsyncFunc(nsContinueRedirectionFunc func)
{
    NS_ASSERTION(func == mRedirectFuncStack[mRedirectFuncStack.Length() - 1],
        "Trying to pop wrong method from redirect async stack!");

    mRedirectFuncStack.TruncateLength(mRedirectFuncStack.Length() - 1);
}


//-----------------------------------------------------------------------------
// nsHttpChannel internal functions
//-----------------------------------------------------------------------------

void
nsHttpChannel::MaybeInvalidateCacheEntryForSubsequentGet()
{
    // See RFC 2616 section 5.1.1. These are considered valid
    // methods which DO NOT invalidate cache-entries for the
    // referred resource. POST, PUT and DELETE as well as any
    // other method not listed here will potentially invalidate
    // any cached copy of the resource
    if (mRequestHead.Method() == nsHttp::Options ||
       mRequestHead.Method() == nsHttp::Get ||
       mRequestHead.Method() == nsHttp::Head ||
       mRequestHead.Method() == nsHttp::Trace ||
       mRequestHead.Method() == nsHttp::Connect)
        return;


    // Invalidate the request-uri.
    // Pass 0 in first param to get the cache-key for a GET-request.
    nsAutoCString tmpCacheKey;
    GenerateCacheKey(0, tmpCacheKey);
    LOG(("MaybeInvalidateCacheEntryForSubsequentGet [this=%p uri=%s]\n", 
        this, tmpCacheKey.get()));
    DoInvalidateCacheEntry(tmpCacheKey);

    // Invalidate Location-header if set
    const char *location = mResponseHead->PeekHeader(nsHttp::Location);
    if (location) {
        LOG(("  Location-header=%s\n", location));
        InvalidateCacheEntryForLocation(location);
    }

    // Invalidate Content-Location-header if set
    location = mResponseHead->PeekHeader(nsHttp::Content_Location);
    if (location) {
        LOG(("  Content-Location-header=%s\n", location));
        InvalidateCacheEntryForLocation(location);
    }
}

void
nsHttpChannel::InvalidateCacheEntryForLocation(const char *location)
{
    nsAutoCString tmpCacheKey, tmpSpec;
    nsCOMPtr<nsIURI> resultingURI;
    nsresult rv = CreateNewURI(location, getter_AddRefs(resultingURI));
    if (NS_SUCCEEDED(rv) && HostPartIsTheSame(resultingURI)) {
        if (NS_SUCCEEDED(resultingURI->GetAsciiSpec(tmpSpec))) {
            location = tmpSpec.get();  //reusing |location|

            // key for a GET-request to |location| with current load-flags
            AssembleCacheKey(location, 0, tmpCacheKey);
            DoInvalidateCacheEntry(tmpCacheKey);
        } else
            NS_WARNING(("  failed getting ascii-spec\n"));
    } else {
        LOG(("  hosts not matching\n"));
    }
}

void
nsHttpChannel::DoInvalidateCacheEntry(const nsCString &key)
{
    // NOTE:
    // Following comments 24,32 and 33 in bug #327765, we only care about
    // the cache in the protocol-handler, not the application cache.
    // The logic below deviates from the original logic in OpenCacheEntry on
    // one point by using only READ_ONLY access-policy. I think this is safe.

    uint32_t appId = NECKO_NO_APP_ID;
    bool isInBrowser = false;
    NS_GetAppInfo(this, &appId, &isInBrowser);

    // First, find session holding the cache-entry - use current storage-policy
    nsCacheStoragePolicy storagePolicy = DetermineStoragePolicy();
    nsAutoCString clientID;
    nsHttpHandler::GetCacheSessionNameForStoragePolicy(storagePolicy, mPrivateBrowsing,
                                                       appId, isInBrowser, clientID);

    LOG(("DoInvalidateCacheEntry [channel=%p session=%s policy=%d key=%s]",
         this, clientID.get(), int(storagePolicy), key.get()));

    nsresult rv;
    nsCOMPtr<nsICacheService> serv =
        do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
    nsCOMPtr<nsICacheSession> session;
    if (NS_SUCCEEDED(rv)) {
        rv = serv->CreateSession(clientID.get(), storagePolicy,
                                 nsICache::STREAM_BASED,
                                 getter_AddRefs(session));
    }
    if (NS_SUCCEEDED(rv)) {
        rv = session->SetIsPrivate(mPrivateBrowsing);
    }
    if (NS_SUCCEEDED(rv)) {
        rv = session->DoomEntry(key, nullptr);
    }

    LOG(("DoInvalidateCacheEntry [channel=%p session=%s policy=%d key=%s rv=%d]",
         this, clientID.get(), int(storagePolicy), key.get(), int(rv)));
}

nsCacheStoragePolicy
nsHttpChannel::DetermineStoragePolicy()
{
    nsCacheStoragePolicy policy = nsICache::STORE_ANYWHERE;
    if (mPrivateBrowsing)
        policy = nsICache::STORE_IN_MEMORY;
    else if (mLoadFlags & INHIBIT_PERSISTENT_CACHING)
        policy = nsICache::STORE_IN_MEMORY;

    return policy;
}

nsresult
nsHttpChannel::DetermineCacheAccess(nsCacheAccessMode *_retval)
{
    bool offline = gIOService->IsOffline();

    if (offline || (mLoadFlags & INHIBIT_CACHING)) {
        // If we have been asked to bypass the cache and not write to the
        // cache, then don't use the cache at all.  Unless we're actually
        // offline, which takes precedence over BYPASS_LOCAL_CACHE.
        if (BYPASS_LOCAL_CACHE(mLoadFlags) && !offline)
            return NS_ERROR_NOT_AVAILABLE;
        *_retval = nsICache::ACCESS_READ;
    }
    else if (BYPASS_LOCAL_CACHE(mLoadFlags))
        *_retval = nsICache::ACCESS_WRITE; // replace cache entry
    else
        *_retval = nsICache::ACCESS_READ_WRITE; // normal browsing

    return NS_OK;
}

void
nsHttpChannel::AsyncOnExamineCachedResponse()
{
    gHttpHandler->OnExamineCachedResponse(this);

}

void
nsHttpChannel::UpdateAggregateCallbacks()
{
    if (!mTransaction) {
        return;
    }
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    NS_NewNotificationCallbacksAggregation(mCallbacks, mLoadGroup,
                                           NS_GetCurrentThread(),
                                           getter_AddRefs(callbacks));
    mTransaction->SetSecurityCallbacks(callbacks);
}

NS_IMETHODIMP
nsHttpChannel::SetLoadGroup(nsILoadGroup *aLoadGroup)
{
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread.");

    nsresult rv = HttpBaseChannel::SetLoadGroup(aLoadGroup);
    if (NS_SUCCEEDED(rv)) {
        UpdateAggregateCallbacks();
    }
    return rv;
}

NS_IMETHODIMP
nsHttpChannel::SetNotificationCallbacks(nsIInterfaceRequestor *aCallbacks)
{
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread.");

    nsresult rv = HttpBaseChannel::SetNotificationCallbacks(aCallbacks);
    if (NS_SUCCEEDED(rv)) {
        UpdateAggregateCallbacks();
    }
    return rv;
}

} } // namespace mozilla::net
