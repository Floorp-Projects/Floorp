/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrefetchService.h"
#include "nsICacheEntry.h"
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsIObserverService.h"
#include "nsIWebProgress.h"
#include "nsCURILoader.h"
#include "nsICacheInfoChannel.h"
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
#include "mozilla/Logging.h"
#include "plstr.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "mozilla/Preferences.h"
#include "mozilla/Attributes.h"
#include "mozilla/CORSMode.h"
#include "mozilla/dom/HTMLLinkElement.h"
#include "nsIDOMNode.h"
#include "nsINode.h"
#include "nsIDocument.h"
#include "nsContentUtils.h"

using namespace mozilla;

//
// To enable logging (see mozilla/Logging.h for full details):
//
//    set MOZ_LOG=nsPrefetch:5
//    set MOZ_LOG_FILE=prefetch.log
//
// this enables LogLevel::Debug level information and places all output in
// the file prefetch.log
//
static LazyLogModule gPrefetchLog("nsPrefetch");

#undef LOG
#define LOG(args) MOZ_LOG(gPrefetchLog, mozilla::LogLevel::Debug, args)

#undef LOG_ENABLED
#define LOG_ENABLED() MOZ_LOG_TEST(gPrefetchLog, mozilla::LogLevel::Debug)

#define PREFETCH_PREF "network.prefetch-next"
#define PARALLELISM_PREF "network.prefetch-next.parallelism"

//-----------------------------------------------------------------------------
// helpers
//-----------------------------------------------------------------------------

static inline uint32_t
PRTimeToSeconds(PRTime t_usec)
{
    PRTime usec_per_sec = PR_USEC_PER_SEC;
    return uint32_t(t_usec /= usec_per_sec);
}

#define NowInSeconds() PRTimeToSeconds(PR_Now())

//-----------------------------------------------------------------------------
// nsPrefetchNode <public>
//-----------------------------------------------------------------------------

nsPrefetchNode::nsPrefetchNode(nsPrefetchService *aService,
                               nsIURI *aURI,
                               nsIURI *aReferrerURI,
                               nsIDOMNode *aSource)
    : mURI(aURI)
    , mReferrerURI(aReferrerURI)
    , mService(aService)
    , mChannel(nullptr)
    , mBytesRead(0)
{
    nsCOMPtr<nsIWeakReference> source = do_GetWeakReference(aSource);
    mSources.AppendElement(source);
}

nsresult
nsPrefetchNode::OpenChannel()
{
    if (mSources.IsEmpty()) {
        // Don't attempt to prefetch if we don't have a source node
        // (which should never happen).
        return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsINode> source;
    while (!mSources.IsEmpty() && !(source = do_QueryReferent(mSources.ElementAt(0)))) {
        // If source is null remove it.
        // (which should never happen).
        mSources.RemoveElementAt(0);
    }

    if (!source) {
        // Don't attempt to prefetch if we don't have a source node
        // (which should never happen).

        return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsILoadGroup> loadGroup = source->OwnerDoc()->GetDocumentLoadGroup();
    CORSMode corsMode = CORS_NONE;
    if (source->IsHTMLElement(nsGkAtoms::link)) {
      corsMode = static_cast<dom::HTMLLinkElement*>(source.get())->GetCORSMode();
    }
    uint32_t securityFlags;
    if (corsMode == CORS_NONE) {
      securityFlags = nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS;
    } else {
      securityFlags = nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS;
      if (corsMode == CORS_USE_CREDENTIALS) {
        securityFlags |= nsILoadInfo::SEC_COOKIES_INCLUDE;
      }
    }
    nsresult rv = NS_NewChannelInternal(getter_AddRefs(mChannel),
                                        mURI,
                                        source,
                                        source->NodePrincipal(),
                                        nullptr,   //aTriggeringPrincipal
                                        securityFlags,
                                        nsIContentPolicy::TYPE_OTHER,
                                        loadGroup, // aLoadGroup
                                        this,      // aCallbacks
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

    return mChannel->AsyncOpen2(this);
}

nsresult
nsPrefetchNode::CancelChannel(nsresult error)
{
    mChannel->Cancel(error);
    mChannel = nullptr;

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchNode::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsPrefetchNode,
                  nsIRequestObserver,
                  nsIStreamListener,
                  nsIInterfaceRequestor,
                  nsIChannelEventSink,
                  nsIRedirectResultListener)

//-----------------------------------------------------------------------------
// nsPrefetchNode::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchNode::OnStartRequest(nsIRequest *aRequest,
                               nsISupports *aContext)
{
    nsresult rv;

    nsCOMPtr<nsICacheInfoChannel> cacheInfoChannel =
        do_QueryInterface(aRequest, &rv);
    if (NS_FAILED(rv)) return rv;
 
    // no need to prefetch a document that is already in the cache
    bool fromCache;
    if (NS_SUCCEEDED(cacheInfoChannel->IsFromCache(&fromCache)) &&
        fromCache) {
        LOG(("document is already in the cache; canceling prefetch\n"));
        return NS_BINDING_ABORTED;
    }

    //
    // no need to prefetch a document that must be requested fresh each
    // and every time.
    //
    uint32_t expTime;
    if (NS_SUCCEEDED(cacheInfoChannel->GetCacheTokenExpirationTime(&expTime))) {
        if (NowInSeconds() >= expTime) {
            LOG(("document cannot be reused from cache; "
                 "canceling prefetch\n"));
            return NS_BINDING_ABORTED;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsPrefetchNode::OnDataAvailable(nsIRequest *aRequest,
                                nsISupports *aContext,
                                nsIInputStream *aStream,
                                uint64_t aOffset,
                                uint32_t aCount)
{
    uint32_t bytesRead = 0;
    aStream->ReadSegments(NS_DiscardSegment, nullptr, aCount, &bytesRead);
    mBytesRead += bytesRead;
    LOG(("prefetched %u bytes [offset=%llu]\n", bytesRead, aOffset));
    return NS_OK;
}


NS_IMETHODIMP
nsPrefetchNode::OnStopRequest(nsIRequest *aRequest,
                              nsISupports *aContext,
                              nsresult aStatus)
{
    LOG(("done prefetching [status=%x]\n", aStatus));

    if (mBytesRead == 0 && aStatus == NS_OK && mChannel) {
        // we didn't need to read (because LOAD_ONLY_IF_MODIFIED was
        // specified), but the object should report loadedSize as if it
        // did.
        mChannel->GetContentLength(&mBytesRead);
    }

    mService->NotifyLoadCompleted(this);
    mService->ProcessNextURI(this);
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

    if (aIID.Equals(NS_GET_IID(nsIRedirectResultListener))) {
        NS_ADDREF_THIS();
        *aResult = static_cast<nsIRedirectResultListener *>(this);
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
                                       uint32_t aFlags,
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

    // Assign to mChannel after we get notification about success of the
    // redirect in OnRedirectResult.
    mRedirectChannel = aNewChannel;

    callback->OnRedirectVerifyCallback(NS_OK);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchNode::nsIRedirectResultListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchNode::OnRedirectResult(bool proceeding)
{
    if (proceeding && mRedirectChannel)
        mChannel = mRedirectChannel;

    mRedirectChannel = nullptr;

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchService <public>
//-----------------------------------------------------------------------------

nsPrefetchService::nsPrefetchService()
    : mMaxParallelism(6)
    , mStopCount(0)
    , mHaveProcessed(false)
    , mDisabled(true)
{
}

nsPrefetchService::~nsPrefetchService()
{
    Preferences::RemoveObserver(this, PREFETCH_PREF);
    Preferences::RemoveObserver(this, PARALLELISM_PREF);
    // cannot reach destructor if prefetch in progress (listener owns reference
    // to this service)
    EmptyQueue();
}

nsresult
nsPrefetchService::Init()
{
    nsresult rv;

    // read prefs and hook up pref observer
    mDisabled = !Preferences::GetBool(PREFETCH_PREF, !mDisabled);
    Preferences::AddWeakObserver(this, PREFETCH_PREF);

    mMaxParallelism = Preferences::GetInt(PARALLELISM_PREF, mMaxParallelism);
    if (mMaxParallelism < 1) {
        mMaxParallelism = 1;
    }
    Preferences::AddWeakObserver(this, PARALLELISM_PREF);

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
nsPrefetchService::ProcessNextURI(nsPrefetchNode *aFinished)
{
    nsresult rv;
    nsCOMPtr<nsIURI> uri, referrer;

    if (aFinished) {
        mCurrentNodes.RemoveElement(aFinished);
    }

    if (mCurrentNodes.Length() >= static_cast<uint32_t>(mMaxParallelism)) {
        // We already have enough prefetches going on, so hold off
        // for now.
        return;
    }

    do {
        if (mQueue.empty()) {
          break;
        }
        RefPtr<nsPrefetchNode> node = mQueue.front().forget();
        mQueue.pop_front();

        if (LOG_ENABLED()) {
            nsAutoCString spec;
            node->mURI->GetSpec(spec);
            LOG(("ProcessNextURI [%s]\n", spec.get()));
        }

        //
        // if opening the channel fails, then just skip to the next uri
        //
        rv = node->OpenChannel();
        if (NS_SUCCEEDED(rv)) {
            mCurrentNodes.AppendElement(node);
        }
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

    observerService->NotifyObservers(static_cast<nsIStreamListener*>(node),
                                     "prefetch-load-requested", nullptr);
}

void
nsPrefetchService::NotifyLoadCompleted(nsPrefetchNode *node)
{
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (!observerService)
      return;

    observerService->NotifyObservers(static_cast<nsIStreamListener*>(node),
                                     "prefetch-load-completed", nullptr);
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
nsPrefetchService::EnqueueURI(nsIURI *aURI,
                              nsIURI *aReferrerURI,
                              nsIDOMNode *aSource,
                              nsPrefetchNode **aNode)
{
    RefPtr<nsPrefetchNode> node = new nsPrefetchNode(this, aURI, aReferrerURI,
                                                     aSource);
    mQueue.push_back(node);
    node.forget(aNode);
    return NS_OK;
}

void
nsPrefetchService::EmptyQueue()
{
    while (!mQueue.empty()) {
        mQueue.pop_back();
    }
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
    if (!mStopCount) {
        mHaveProcessed = true;
        while (!mQueue.empty() && mCurrentNodes.Length() < static_cast<uint32_t>(mMaxParallelism)) {
            ProcessNextURI(nullptr);
        }
    }
}

void
nsPrefetchService::StopPrefetching()
{
    mStopCount++;

    LOG(("StopPrefetching [stopcount=%d]\n", mStopCount));

    // only kill the prefetch queue if we are actively prefetching right now
    if (mCurrentNodes.IsEmpty()) {
        return;
    }

    for (uint32_t i = 0; i < mCurrentNodes.Length(); ++i) {
        mCurrentNodes[i]->CancelChannel(NS_BINDING_ABORTED);
    }
    mCurrentNodes.Clear();
    EmptyQueue();
}

//-----------------------------------------------------------------------------
// nsPrefetchService::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsPrefetchService,
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

    if (LOG_ENABLED()) {
        nsAutoCString spec;
        aURI->GetSpec(spec);
        LOG(("PrefetchURI [%s]\n", spec.get()));
    }

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
        nsAutoCString query;
        rv = url->GetQuery(query);
        if (NS_FAILED(rv) || !query.IsEmpty()) {
            LOG(("rejected: URL has a query string\n"));
            return NS_ERROR_ABORT;
        }
    }

    //
    // Check whether it is being prefetched.
    //
    for (uint32_t i = 0; i < mCurrentNodes.Length(); ++i) {
        bool equals;
        if (NS_SUCCEEDED(mCurrentNodes[i]->mURI->Equals(aURI, &equals)) &&
            equals) {
            nsCOMPtr<nsIWeakReference> source = do_GetWeakReference(aSource);
            if (mCurrentNodes[i]->mSources.IndexOf(source) ==
                mCurrentNodes[i]->mSources.NoIndex) {
                LOG(("URL is already being prefetched, add a new reference "
                     "document\n"));
                mCurrentNodes[i]->mSources.AppendElement(source);
                return NS_OK;
            } else {
                LOG(("URL is already being prefetched by this document"));
                return NS_ERROR_ABORT;
            }
        }
    }

    //
    // Check whether it is on the prefetch queue.
    //
    for (std::deque<RefPtr<nsPrefetchNode>>::iterator nodeIt = mQueue.begin();
         nodeIt != mQueue.end(); nodeIt++) {
        bool equals;
        RefPtr<nsPrefetchNode> node = nodeIt->get();
        if (NS_SUCCEEDED(node->mURI->Equals(aURI, &equals)) && equals) {
            nsCOMPtr<nsIWeakReference> source = do_GetWeakReference(aSource);
            if (node->mSources.IndexOf(source) ==
                node->mSources.NoIndex) {
                LOG(("URL is already being prefetched, add a new reference "
                     "document\n"));
                node->mSources.AppendElement(do_GetWeakReference(aSource));
                return NS_OK;
            } else {
                LOG(("URL is already being prefetched by this document"));
                return NS_ERROR_ABORT;
            }
     
        }
    }

    RefPtr<nsPrefetchNode> enqueuedNode;
    rv = EnqueueURI(aURI, aReferrerURI, aSource,
                    getter_AddRefs(enqueuedNode));
    NS_ENSURE_SUCCESS(rv, rv);

    NotifyLoadRequested(enqueuedNode);

    // if there are no pages loading, kick off the request immediately
    if (mStopCount == 0 && mHaveProcessed) {
        ProcessNextURI(nullptr);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsPrefetchService::CancelPrefetchURI(nsIURI* aURI,
                                     nsIDOMNode* aSource)
{
    NS_ENSURE_ARG_POINTER(aURI);

    if (LOG_ENABLED()) {
        nsAutoCString spec;
        aURI->GetSpec(spec);
        LOG(("CancelPrefetchURI [%s]\n", spec.get()));
    }

    //
    // look in current prefetches
    //
    for (uint32_t i = 0; i < mCurrentNodes.Length(); ++i) {
        bool equals;
        if (NS_SUCCEEDED(mCurrentNodes[i]->mURI->Equals(aURI, &equals)) &&
            equals) {
            nsCOMPtr<nsIWeakReference> source = do_GetWeakReference(aSource);
            if (mCurrentNodes[i]->mSources.IndexOf(source) !=
                mCurrentNodes[i]->mSources.NoIndex) {
                mCurrentNodes[i]->mSources.RemoveElement(source);
                if (mCurrentNodes[i]->mSources.IsEmpty()) {
                    mCurrentNodes[i]->CancelChannel(NS_BINDING_ABORTED);
                    mCurrentNodes.RemoveElementAt(i);
                }
                return NS_OK;
            }
            return NS_ERROR_FAILURE;
        }
    }

    //
    // look into the prefetch queue
    //
    for (std::deque<RefPtr<nsPrefetchNode>>::iterator nodeIt = mQueue.begin();
         nodeIt != mQueue.end(); nodeIt++) {
        bool equals;
        RefPtr<nsPrefetchNode> node = nodeIt->get();
        if (NS_SUCCEEDED(node->mURI->Equals(aURI, &equals)) && equals) {
            nsCOMPtr<nsIWeakReference> source = do_GetWeakReference(aSource);
            if (node->mSources.IndexOf(source) !=
                node->mSources.NoIndex) {

#ifdef DEBUG
                int32_t inx = node->mSources.IndexOf(source);
                nsCOMPtr<nsIDOMNode> domNode =
                    do_QueryReferent(node->mSources.ElementAt(inx));
                MOZ_ASSERT(domNode);
#endif

                node->mSources.RemoveElement(source);
                if (node->mSources.IsEmpty()) {
                    mQueue.erase(nodeIt);
                }
                return NS_OK;
            }
            return NS_ERROR_FAILURE;
        }
    }

    // not found!
    return NS_ERROR_FAILURE;
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
nsPrefetchService::HasMoreElements(bool *aHasMore)
{
    *aHasMore = (mCurrentNodes.Length() || !mQueue.empty());
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchService::nsIWebProgressListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchService::OnProgressChange(nsIWebProgress *aProgress,
                                  nsIRequest *aRequest, 
                                  int32_t curSelfProgress, 
                                  int32_t maxSelfProgress, 
                                  int32_t curTotalProgress, 
                                  int32_t maxTotalProgress)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP 
nsPrefetchService::OnStateChange(nsIWebProgress* aWebProgress, 
                                 nsIRequest *aRequest, 
                                 uint32_t progressStateFlags, 
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
                                    uint32_t aFlags)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP 
nsPrefetchService::OnStatusChange(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest,
                                  nsresult aStatus,
                                  const char16_t* aMessage)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP 
nsPrefetchService::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                    nsIRequest *aRequest, 
                                    uint32_t state)
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
                           const char16_t *aData)
{
    LOG(("nsPrefetchService::Observe [topic=%s]\n", aTopic));

    if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
        StopPrefetching();
        EmptyQueue();
        mDisabled = true;
    }
    else if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
        const nsCString converted = NS_ConvertUTF16toUTF8(aData);
        const char* pref = converted.get();
        if (!strcmp(pref, PREFETCH_PREF)) {
            if (Preferences::GetBool(PREFETCH_PREF, false)) {
                if (mDisabled) {
                    LOG(("enabling prefetching\n"));
                    mDisabled = false;
                    AddProgressListener();
                }
            } else {
                if (!mDisabled) {
                    LOG(("disabling prefetching\n"));
                    StopPrefetching();
                    EmptyQueue();
                    mDisabled = true;
                    RemoveProgressListener();
                }
            }
        } else if (!strcmp(pref, PARALLELISM_PREF)) {
            mMaxParallelism = Preferences::GetInt(PARALLELISM_PREF, mMaxParallelism);
            if (mMaxParallelism < 1) {
                mMaxParallelism = 1;
            }
            // If our parallelism has increased, go ahead and kick off enough
            // prefetches to fill up our allowance. If we're now over our
            // allowance, we'll just silently let some of them finish to get
            // back below our limit.
            while (!mQueue.empty() && mCurrentNodes.Length() < static_cast<uint32_t>(mMaxParallelism)) {
                ProcessNextURI(nullptr);
            }
        }
    }

    return NS_OK;
}

// vim: ts=4 sw=4 expandtab
