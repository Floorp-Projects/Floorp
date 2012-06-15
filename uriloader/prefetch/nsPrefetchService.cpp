/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrefetchService.h"
#include "nsICacheSession.h"
#include "nsICacheService.h"
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsIObserverService.h"
#include "nsIWebProgress.h"
#include "nsCURILoader.h"
#include "nsICachingChannel.h"
#include "nsICacheVisitor.h"
#include "nsIHttpChannel.h"
#include "nsIURL.h"
#include "nsISimpleEnumerator.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsStreamUtils.h"
#include "nsAutoPtr.h"
#include "prtime.h"
#include "prlog.h"
#include "plstr.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "mozilla/Preferences.h"
#include "mozilla/Attributes.h"

using namespace mozilla;

#if defined(PR_LOGGING)
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsPrefetch:5
//    set NSPR_LOG_FILE=prefetch.log
//
// this enables PR_LOG_ALWAYS level information and places all output in
// the file http.log
//
static PRLogModuleInfo *gPrefetchLog;
#endif
#define LOG(args) PR_LOG(gPrefetchLog, 4, args)
#define LOG_ENABLED() PR_LOG_TEST(gPrefetchLog, 4)

#define PREFETCH_PREF "network.prefetch-next"

//-----------------------------------------------------------------------------
// helpers
//-----------------------------------------------------------------------------

static inline PRUint32
PRTimeToSeconds(PRTime t_usec)
{
    PRTime usec_per_sec;
    PRUint32 t_sec;
    LL_I2L(usec_per_sec, PR_USEC_PER_SEC);
    LL_DIV(t_usec, t_usec, usec_per_sec);
    LL_L2I(t_sec, t_usec);
    return t_sec;
}

#define NowInSeconds() PRTimeToSeconds(PR_Now())

//-----------------------------------------------------------------------------
// nsPrefetchQueueEnumerator
//-----------------------------------------------------------------------------
class nsPrefetchQueueEnumerator MOZ_FINAL : public nsISimpleEnumerator
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISIMPLEENUMERATOR
    nsPrefetchQueueEnumerator(nsPrefetchService *aService);
    ~nsPrefetchQueueEnumerator();

private:
    void Increment();

    nsRefPtr<nsPrefetchService> mService;
    nsRefPtr<nsPrefetchNode> mCurrent;
    bool mStarted;
};

//-----------------------------------------------------------------------------
// nsPrefetchQueueEnumerator <public>
//-----------------------------------------------------------------------------
nsPrefetchQueueEnumerator::nsPrefetchQueueEnumerator(nsPrefetchService *aService)
    : mService(aService)
    , mStarted(false)
{
    Increment();
}

nsPrefetchQueueEnumerator::~nsPrefetchQueueEnumerator()
{
}

//-----------------------------------------------------------------------------
// nsPrefetchQueueEnumerator::nsISimpleEnumerator
//-----------------------------------------------------------------------------
NS_IMETHODIMP
nsPrefetchQueueEnumerator::HasMoreElements(bool *aHasMore)
{
    *aHasMore = (mCurrent != nsnull);
    return NS_OK;
}

NS_IMETHODIMP
nsPrefetchQueueEnumerator::GetNext(nsISupports **aItem)
{
    if (!mCurrent) return NS_ERROR_FAILURE;

    NS_ADDREF(*aItem = static_cast<nsIDOMLoadStatus*>(mCurrent.get()));

    Increment();

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchQueueEnumerator <private>
//-----------------------------------------------------------------------------

void
nsPrefetchQueueEnumerator::Increment()
{
  if (!mStarted) {
    // If the service is currently serving a request, it won't be in
    // the pending queue, so we return it first.  If it isn't, we'll
    // just start with the pending queue.
    mStarted = true;
    mCurrent = mService->GetCurrentNode();
    if (!mCurrent)
      mCurrent = mService->GetQueueHead();
    return;
  }

  if (mCurrent) {
    if (mCurrent == mService->GetCurrentNode()) {
      // If we just returned the node being processed by the service,
      // start with the pending queue
      mCurrent = mService->GetQueueHead();
    }
    else {
      // Otherwise just advance to the next item in the queue
      mCurrent = mCurrent->mNext;
    }
  }
}

//-----------------------------------------------------------------------------
// nsPrefetchQueueEnumerator::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS1(nsPrefetchQueueEnumerator, nsISimpleEnumerator)

//-----------------------------------------------------------------------------
// nsPrefetchNode <public>
//-----------------------------------------------------------------------------

nsPrefetchNode::nsPrefetchNode(nsPrefetchService *aService,
                               nsIURI *aURI,
                               nsIURI *aReferrerURI,
                               nsIDOMNode *aSource)
    : mNext(nsnull)
    , mURI(aURI)
    , mReferrerURI(aReferrerURI)
    , mService(aService)
    , mChannel(nsnull)
    , mState(nsIDOMLoadStatus::UNINITIALIZED)
    , mBytesRead(0)
{
    mSource = do_GetWeakReference(aSource);
}

nsresult
nsPrefetchNode::OpenChannel()
{
    nsresult rv = NS_NewChannel(getter_AddRefs(mChannel),
                                mURI,
                                nsnull, nsnull, this,
                                nsIRequest::LOAD_BACKGROUND |
                                nsICachingChannel::LOAD_ONLY_IF_MODIFIED);
    NS_ENSURE_SUCCESS(rv, rv);

    // configure HTTP specific stuff
    nsCOMPtr<nsIHttpChannel> httpChannel =
        do_QueryInterface(mChannel);
    if (httpChannel) {
        httpChannel->SetReferrer(mReferrerURI);
        httpChannel->SetRequestHeader(
            NS_LITERAL_CSTRING("X-Moz"),
            NS_LITERAL_CSTRING("prefetch"),
            false);
    }

    rv = mChannel->AsyncOpen(this, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    mState = nsIDOMLoadStatus::REQUESTED;

    return NS_OK;
}

nsresult
nsPrefetchNode::CancelChannel(nsresult error)
{
    mChannel->Cancel(error);
    mChannel = nsnull;

    mState = nsIDOMLoadStatus::UNINITIALIZED;

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchNode::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS5(nsPrefetchNode,
                   nsIDOMLoadStatus,
                   nsIRequestObserver,
                   nsIStreamListener,
                   nsIInterfaceRequestor,
                   nsIChannelEventSink)

//-----------------------------------------------------------------------------
// nsPrefetchNode::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchNode::OnStartRequest(nsIRequest *aRequest,
                               nsISupports *aContext)
{
    nsresult rv;

    nsCOMPtr<nsICachingChannel> cachingChannel =
        do_QueryInterface(aRequest, &rv);
    if (NS_FAILED(rv)) return rv;

    // no need to prefetch a document that is already in the cache
    bool fromCache;
    if (NS_SUCCEEDED(cachingChannel->IsFromCache(&fromCache)) &&
        fromCache) {
        LOG(("document is already in the cache; canceling prefetch\n"));
        return NS_BINDING_ABORTED;
    }

    //
    // no need to prefetch a document that must be requested fresh each
    // and every time.
    //
    nsCOMPtr<nsISupports> cacheToken;
    cachingChannel->GetCacheToken(getter_AddRefs(cacheToken));
    if (!cacheToken)
        return NS_ERROR_ABORT; // bail, no cache entry

    nsCOMPtr<nsICacheEntryInfo> entryInfo =
        do_QueryInterface(cacheToken, &rv);
    if (NS_FAILED(rv)) return rv;

    PRUint32 expTime;
    if (NS_SUCCEEDED(entryInfo->GetExpirationTime(&expTime))) {
        if (NowInSeconds() >= expTime) {
            LOG(("document cannot be reused from cache; "
                 "canceling prefetch\n"));
            return NS_BINDING_ABORTED;
        }
    }

    mState = nsIDOMLoadStatus::RECEIVING;

    return NS_OK;
}

NS_IMETHODIMP
nsPrefetchNode::OnDataAvailable(nsIRequest *aRequest,
                                nsISupports *aContext,
                                nsIInputStream *aStream,
                                PRUint32 aOffset,
                                PRUint32 aCount)
{
    PRUint32 bytesRead = 0;
    aStream->ReadSegments(NS_DiscardSegment, nsnull, aCount, &bytesRead);
    mBytesRead += bytesRead;
    LOG(("prefetched %u bytes [offset=%u]\n", bytesRead, aOffset));
    return NS_OK;
}


NS_IMETHODIMP
nsPrefetchNode::OnStopRequest(nsIRequest *aRequest,
                              nsISupports *aContext,
                              nsresult aStatus)
{
    LOG(("done prefetching [status=%x]\n", aStatus));

    mState = nsIDOMLoadStatus::LOADED;

    if (mBytesRead == 0 && aStatus == NS_OK) {
        // we didn't need to read (because LOAD_ONLY_IF_MODIFIED was
        // specified), but the object should report loadedSize as if it
        // did.
        mChannel->GetContentLength(&mBytesRead);
    }

    mService->NotifyLoadCompleted(this);
    mService->ProcessNextURI();
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchNode::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchNode::GetInterface(const nsIID &aIID, void **aResult)
{
    if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
        NS_ADDREF_THIS();
        *aResult = static_cast<nsIChannelEventSink *>(this);
        return NS_OK;
    }

    return NS_ERROR_NO_INTERFACE;
}

//-----------------------------------------------------------------------------
// nsPrefetchNode::nsIChannelEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchNode::AsyncOnChannelRedirect(nsIChannel *aOldChannel,
                                       nsIChannel *aNewChannel,
                                       PRUint32 aFlags,
                                       nsIAsyncVerifyRedirectCallback *callback)
{
    nsCOMPtr<nsIURI> newURI;
    nsresult rv = aNewChannel->GetURI(getter_AddRefs(newURI));
    if (NS_FAILED(rv))
        return rv;

    bool match;
    rv = newURI->SchemeIs("http", &match); 
    if (NS_FAILED(rv) || !match) {
        rv = newURI->SchemeIs("https", &match); 
        if (NS_FAILED(rv) || !match) {
            LOG(("rejected: URL is not of type http/https\n"));
            return NS_ERROR_ABORT;
        }
    }

    // HTTP request headers are not automatically forwarded to the new channel.
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aNewChannel);
    NS_ENSURE_STATE(httpChannel);

    httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("X-Moz"),
                                  NS_LITERAL_CSTRING("prefetch"),
                                  false);

    mChannel = aNewChannel;

    callback->OnRedirectVerifyCallback(NS_OK);
    return NS_OK;
}


//-----------------------------------------------------------------------------
// nsPrefetchService <public>
//-----------------------------------------------------------------------------

nsPrefetchService::nsPrefetchService()
    : mQueueHead(nsnull)
    , mQueueTail(nsnull)
    , mStopCount(0)
    , mHaveProcessed(false)
    , mDisabled(true)
{
}

nsPrefetchService::~nsPrefetchService()
{
    Preferences::RemoveObserver(this, PREFETCH_PREF);
    // cannot reach destructor if prefetch in progress (listener owns reference
    // to this service)
    EmptyQueue();
}

nsresult
nsPrefetchService::Init()
{
#if defined(PR_LOGGING)
    if (!gPrefetchLog)
        gPrefetchLog = PR_NewLogModule("nsPrefetch");
#endif

    nsresult rv;

    // read prefs and hook up pref observer
    mDisabled = !Preferences::GetBool(PREFETCH_PREF, !mDisabled);
    Preferences::AddWeakObserver(this, PREFETCH_PREF);

    // Observe xpcom-shutdown event
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (!observerService)
      return NS_ERROR_FAILURE;

    rv = observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, true);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mDisabled)
        AddProgressListener();

    return NS_OK;
}

void
nsPrefetchService::ProcessNextURI()
{
    nsresult rv;
    nsCOMPtr<nsIURI> uri, referrer;

    mCurrentNode = nsnull;

    do {
        rv = DequeueNode(getter_AddRefs(mCurrentNode));

        if (NS_FAILED(rv)) break;

#if defined(PR_LOGGING)
        if (LOG_ENABLED()) {
            nsCAutoString spec;
            mCurrentNode->mURI->GetSpec(spec);
            LOG(("ProcessNextURI [%s]\n", spec.get()));
        }
#endif

        //
        // if opening the channel fails, then just skip to the next uri
        //
        rv = mCurrentNode->OpenChannel();
    }
    while (NS_FAILED(rv));
}

void
nsPrefetchService::NotifyLoadRequested(nsPrefetchNode *node)
{
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (!observerService)
      return;

    observerService->NotifyObservers(static_cast<nsIDOMLoadStatus*>(node),
                                     "prefetch-load-requested", nsnull);
}

void
nsPrefetchService::NotifyLoadCompleted(nsPrefetchNode *node)
{
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (!observerService)
      return;

    observerService->NotifyObservers(static_cast<nsIDOMLoadStatus*>(node),
                                     "prefetch-load-completed", nsnull);
}

//-----------------------------------------------------------------------------
// nsPrefetchService <private>
//-----------------------------------------------------------------------------

void
nsPrefetchService::AddProgressListener()
{
    // Register as an observer for the document loader  
    nsCOMPtr<nsIWebProgress> progress = 
        do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID);
    if (progress)
        progress->AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_DOCUMENT);
}

void
nsPrefetchService::RemoveProgressListener()
{
    // Register as an observer for the document loader  
    nsCOMPtr<nsIWebProgress> progress =
        do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID);
    if (progress)
        progress->RemoveProgressListener(this);
}

nsresult
nsPrefetchService::EnqueueNode(nsPrefetchNode *aNode)
{
    NS_ADDREF(aNode);

    if (!mQueueTail) {
        mQueueHead = aNode;
        mQueueTail = aNode;
    }
    else {
        mQueueTail->mNext = aNode;
        mQueueTail = aNode;
    }

    return NS_OK;
}

nsresult
nsPrefetchService::EnqueueURI(nsIURI *aURI,
                              nsIURI *aReferrerURI,
                              nsIDOMNode *aSource,
                              nsPrefetchNode **aNode)
{
    nsPrefetchNode *node = new nsPrefetchNode(this, aURI, aReferrerURI,
                                              aSource);
    if (!node)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aNode = node);

    return EnqueueNode(node);
}

nsresult
nsPrefetchService::DequeueNode(nsPrefetchNode **node)
{
    if (!mQueueHead)
        return NS_ERROR_NOT_AVAILABLE;

    // give the ref to the caller
    *node = mQueueHead;
    mQueueHead = mQueueHead->mNext;
    (*node)->mNext = nsnull;

    if (!mQueueHead)
        mQueueTail = nsnull;

    return NS_OK;
}

void
nsPrefetchService::EmptyQueue()
{
    do {
        nsRefPtr<nsPrefetchNode> node;
        DequeueNode(getter_AddRefs(node));
    } while (mQueueHead);
}

void
nsPrefetchService::StartPrefetching()
{
    //
    // at initialization time we might miss the first DOCUMENT START
    // notification, so we have to be careful to avoid letting our
    // stop count go negative.
    //
    if (mStopCount > 0)
        mStopCount--;

    LOG(("StartPrefetching [stopcount=%d]\n", mStopCount));

    // only start prefetching after we've received enough DOCUMENT
    // STOP notifications.  we do this inorder to defer prefetching
    // until after all sub-frames have finished loading.
    if (mStopCount == 0 && !mCurrentNode) {
        mHaveProcessed = true;
        ProcessNextURI();
    }
}

void
nsPrefetchService::StopPrefetching()
{
    mStopCount++;

    LOG(("StopPrefetching [stopcount=%d]\n", mStopCount));

    // only kill the prefetch queue if we've actually started prefetching.
    if (!mCurrentNode)
        return;

    mCurrentNode->CancelChannel(NS_BINDING_ABORTED);
    mCurrentNode = nsnull;
    EmptyQueue();
}

//-----------------------------------------------------------------------------
// nsPrefetchService::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS4(nsPrefetchService,
                   nsIPrefetchService,
                   nsIWebProgressListener,
                   nsIObserver,
                   nsISupportsWeakReference)

//-----------------------------------------------------------------------------
// nsPrefetchService::nsIPrefetchService
//-----------------------------------------------------------------------------

nsresult
nsPrefetchService::Prefetch(nsIURI *aURI,
                            nsIURI *aReferrerURI,
                            nsIDOMNode *aSource,
                            bool aExplicit)
{
    nsresult rv;

    NS_ENSURE_ARG_POINTER(aURI);
    NS_ENSURE_ARG_POINTER(aReferrerURI);

#if defined(PR_LOGGING)
    if (LOG_ENABLED()) {
        nsCAutoString spec;
        aURI->GetSpec(spec);
        LOG(("PrefetchURI [%s]\n", spec.get()));
    }
#endif

    if (mDisabled) {
        LOG(("rejected: prefetch service is disabled\n"));
        return NS_ERROR_ABORT;
    }

    //
    // XXX we should really be asking the protocol handler if it supports
    // caching, so we can determine if there is any value to prefetching.
    // for now, we'll only prefetch http links since we know that's the 
    // most common case.  ignore https links since https content only goes
    // into the memory cache.
    //
    // XXX we might want to either leverage nsIProtocolHandler::protocolFlags
    // or possibly nsIRequest::loadFlags to determine if this URI should be
    // prefetched.
    //
    bool match;
    rv = aURI->SchemeIs("http", &match); 
    if (NS_FAILED(rv) || !match) {
        rv = aURI->SchemeIs("https", &match); 
        if (NS_FAILED(rv) || !match) {
            LOG(("rejected: URL is not of type http/https\n"));
            return NS_ERROR_ABORT;
        }
    }

    // 
    // the referrer URI must be http:
    //
    rv = aReferrerURI->SchemeIs("http", &match);
    if (NS_FAILED(rv) || !match) {
        rv = aReferrerURI->SchemeIs("https", &match);
        if (NS_FAILED(rv) || !match) {
            LOG(("rejected: referrer URL is neither http nor https\n"));
            return NS_ERROR_ABORT;
        }
    }

    // skip URLs that contain query strings, except URLs for which prefetching
    // has been explicitly requested.
    if (!aExplicit) {
        nsCOMPtr<nsIURL> url(do_QueryInterface(aURI, &rv));
        if (NS_FAILED(rv)) return rv;
        nsCAutoString query;
        rv = url->GetQuery(query);
        if (NS_FAILED(rv) || !query.IsEmpty()) {
            LOG(("rejected: URL has a query string\n"));
            return NS_ERROR_ABORT;
        }
    }

    //
    // cancel if being prefetched
    //
    if (mCurrentNode) {
        bool equals;
        if (NS_SUCCEEDED(mCurrentNode->mURI->Equals(aURI, &equals)) && equals) {
            LOG(("rejected: URL is already being prefetched\n"));
            return NS_ERROR_ABORT;
        }
    }

    //
    // cancel if already on the prefetch queue
    //
    nsPrefetchNode *node = mQueueHead;
    for (; node; node = node->mNext) {
        bool equals;
        if (NS_SUCCEEDED(node->mURI->Equals(aURI, &equals)) && equals) {
            LOG(("rejected: URL is already on prefetch queue\n"));
            return NS_ERROR_ABORT;
        }
    }

    nsRefPtr<nsPrefetchNode> enqueuedNode;
    rv = EnqueueURI(aURI, aReferrerURI, aSource,
                    getter_AddRefs(enqueuedNode));
    NS_ENSURE_SUCCESS(rv, rv);

    NotifyLoadRequested(enqueuedNode);

    // if there are no pages loading, kick off the request immediately
    if (mStopCount == 0 && mHaveProcessed)
        ProcessNextURI();

    return NS_OK;
}

NS_IMETHODIMP
nsPrefetchService::PrefetchURI(nsIURI *aURI,
                               nsIURI *aReferrerURI,
                               nsIDOMNode *aSource,
                               bool aExplicit)
{
    return Prefetch(aURI, aReferrerURI, aSource, aExplicit);
}

NS_IMETHODIMP
nsPrefetchService::PrefetchURIForOfflineUse(nsIURI *aURI,
                                            nsIURI *aReferrerURI,
                                            nsIDOMNode *aSource,
                                            bool aExplicit)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPrefetchService::EnumerateQueue(bool aIncludeNormalItems,
                                  bool aIncludeOfflineItems,
                                  nsISimpleEnumerator **aEnumerator)
{
    NS_ENSURE_TRUE(aIncludeNormalItems && !aIncludeOfflineItems,
                   NS_ERROR_NOT_IMPLEMENTED);

    *aEnumerator = new nsPrefetchQueueEnumerator(this);
    if (!*aEnumerator) return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aEnumerator);

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchNode::nsIDOMLoadStatus
//-----------------------------------------------------------------------------
NS_IMETHODIMP
nsPrefetchNode::GetSource(nsIDOMNode **aSource)
{
    *aSource = nsnull;
    nsCOMPtr<nsIDOMNode> source = do_QueryReferent(mSource);
    if (source)
        source.swap(*aSource);

    return NS_OK;
}

NS_IMETHODIMP
nsPrefetchNode::GetUri(nsAString &aURI)
{
    nsCAutoString spec;
    nsresult rv = mURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    CopyUTF8toUTF16(spec, aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsPrefetchNode::GetTotalSize(PRInt32 *aTotalSize)
{
    if (mChannel) {
        return mChannel->GetContentLength(aTotalSize);
    }

    *aTotalSize = -1;
    return NS_OK;
}

NS_IMETHODIMP
nsPrefetchNode::GetLoadedSize(PRInt32 *aLoadedSize)
{
    *aLoadedSize = mBytesRead;
    return NS_OK;
}

NS_IMETHODIMP
nsPrefetchNode::GetReadyState(PRUint16 *aReadyState)
{
    *aReadyState = mState;
    return NS_OK;
}

NS_IMETHODIMP
nsPrefetchNode::GetStatus(PRUint16 *aStatus)
{
    if (!mChannel) {
        *aStatus = 0;
        return NS_OK;
    }

    nsresult rv;
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 httpStatus;
    rv = httpChannel->GetResponseStatus(&httpStatus);
    if (rv == NS_ERROR_NOT_AVAILABLE) {
        // Someone's calling this before we got a response... Check our
        // ReadyState.  If we're at RECEIVING or LOADED, then this means the
        // connection errored before we got any data; return a somewhat
        // sensible error code in that case.
        if (mState >= nsIDOMLoadStatus::RECEIVING) {
            *aStatus = NS_ERROR_NOT_AVAILABLE;
            return NS_OK;
        }

        *aStatus = 0;
        return NS_OK;
    }

    NS_ENSURE_SUCCESS(rv, rv);
    *aStatus = PRUint16(httpStatus);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchService::nsIWebProgressListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchService::OnProgressChange(nsIWebProgress *aProgress,
                                  nsIRequest *aRequest, 
                                  PRInt32 curSelfProgress, 
                                  PRInt32 maxSelfProgress, 
                                  PRInt32 curTotalProgress, 
                                  PRInt32 maxTotalProgress)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP 
nsPrefetchService::OnStateChange(nsIWebProgress* aWebProgress, 
                                 nsIRequest *aRequest, 
                                 PRUint32 progressStateFlags, 
                                 nsresult aStatus)
{
    if (progressStateFlags & STATE_IS_DOCUMENT) {
        if (progressStateFlags & STATE_STOP)
            StartPrefetching();
        else if (progressStateFlags & STATE_START)
            StopPrefetching();
    }
            
    return NS_OK;
}


NS_IMETHODIMP
nsPrefetchService::OnLocationChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    nsIURI *location,
                                    PRUint32 aFlags)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP 
nsPrefetchService::OnStatusChange(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest,
                                  nsresult aStatus,
                                  const PRUnichar* aMessage)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP 
nsPrefetchService::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                    nsIRequest *aRequest, 
                                    PRUint32 state)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchService::nsIObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchService::Observe(nsISupports     *aSubject,
                           const char      *aTopic,
                           const PRUnichar *aData)
{
    LOG(("nsPrefetchService::Observe [topic=%s]\n", aTopic));

    if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
        StopPrefetching();
        EmptyQueue();
        mDisabled = true;
    }
    else if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
        if (Preferences::GetBool(PREFETCH_PREF, false)) {
            if (mDisabled) {
                LOG(("enabling prefetching\n"));
                mDisabled = false;
                AddProgressListener();
            }
        } 
        else {
            if (!mDisabled) {
                LOG(("disabling prefetching\n"));
                StopPrefetching();
                EmptyQueue();
                mDisabled = true;
                RemoveProgressListener();
            }
        }
    }

    return NS_OK;
}

// vim: ts=4 sw=4 expandtab
