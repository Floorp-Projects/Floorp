/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrefetchService.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Attributes.h"
#include "mozilla/CORSMode.h"
#include "mozilla/Components.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/HTMLLinkElement.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/Preferences.h"
#include "ReferrerInfo.h"

#include "nsICacheEntry.h"
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsIObserverService.h"
#include "nsIWebProgress.h"
#include "nsICacheInfoChannel.h"
#include "nsIHttpChannel.h"
#include "nsIURL.h"
#include "nsISimpleEnumerator.h"
#include "nsISupportsPriority.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsStreamUtils.h"
#include "nsAutoPtr.h"
#include "prtime.h"
#include "mozilla/Logging.h"
#include "plstr.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsINode.h"
#include "mozilla/dom/Document.h"
#include "nsContentUtils.h"
#include "nsStyleLinkElement.h"
#include "mozilla/AsyncEventDispatcher.h"

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
#define PRELOAD_PREF "network.preload"
#define PARALLELISM_PREF "network.prefetch-next.parallelism"
#define AGGRESSIVE_PREF "network.prefetch-next.aggressive"

//-----------------------------------------------------------------------------
// nsPrefetchNode <public>
//-----------------------------------------------------------------------------

nsPrefetchNode::nsPrefetchNode(nsPrefetchService* aService, nsIURI* aURI,
                               nsIReferrerInfo* aReferrerInfo, nsINode* aSource,
                               nsContentPolicyType aPolicyType, bool aPreload)
    : mURI(aURI),
      mReferrerInfo(aReferrerInfo),
      mPolicyType(aPolicyType),
      mPreload(aPreload),
      mService(aService),
      mChannel(nullptr),
      mBytesRead(0),
      mShouldFireLoadEvent(false) {
  nsWeakPtr source = do_GetWeakReference(aSource);
  mSources.AppendElement(source);
}

nsresult nsPrefetchNode::OpenChannel() {
  if (mSources.IsEmpty()) {
    // Don't attempt to prefetch if we don't have a source node
    // (which should never happen).
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsINode> source;
  while (!mSources.IsEmpty() &&
         !(source = do_QueryReferent(mSources.ElementAt(0)))) {
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
  if (auto* link = dom::HTMLLinkElement::FromNode(source)) {
    corsMode = link->GetCORSMode();
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
  nsresult rv = NS_NewChannelInternal(
      getter_AddRefs(mChannel), mURI, source, source->NodePrincipal(),
      nullptr,  // aTriggeringPrincipal
      Maybe<ClientInfo>(), Maybe<ServiceWorkerDescriptor>(), securityFlags,
      mPolicyType, source->OwnerDoc()->CookieSettings(),
      nullptr,    // aPerformanceStorage
      loadGroup,  // aLoadGroup
      this,       // aCallbacks
      nsIRequest::LOAD_BACKGROUND | nsICachingChannel::LOAD_ONLY_IF_MODIFIED);

  NS_ENSURE_SUCCESS(rv, rv);

  // configure HTTP specific stuff
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
  if (httpChannel) {
    DebugOnly<nsresult> success = httpChannel->SetReferrerInfo(mReferrerInfo);
    MOZ_ASSERT(NS_SUCCEEDED(success));
    success = httpChannel->SetRequestHeader(
        NS_LITERAL_CSTRING("X-Moz"), NS_LITERAL_CSTRING("prefetch"), false);
    MOZ_ASSERT(NS_SUCCEEDED(success));
  }

  // Reduce the priority of prefetch network requests.
  nsCOMPtr<nsISupportsPriority> priorityChannel = do_QueryInterface(mChannel);
  if (priorityChannel) {
    priorityChannel->AdjustPriority(nsISupportsPriority::PRIORITY_LOWEST);
  }

  rv = mChannel->AsyncOpen(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Drop the ref to the channel, because we don't want to end up with
    // cycles through it.
    mChannel = nullptr;
  }
  return rv;
}

nsresult nsPrefetchNode::CancelChannel(nsresult error) {
  mChannel->Cancel(error);
  mChannel = nullptr;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchNode::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsPrefetchNode, nsIRequestObserver, nsIStreamListener,
                  nsIInterfaceRequestor, nsIChannelEventSink,
                  nsIRedirectResultListener)

//-----------------------------------------------------------------------------
// nsPrefetchNode::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchNode::OnStartRequest(nsIRequest* aRequest) {
  nsresult rv;

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest, &rv);
  if (NS_FAILED(rv)) return rv;

  // if the load is cross origin without CORS, or the CORS access is rejected,
  // always fire load event to avoid leaking site information.
  nsCOMPtr<nsILoadInfo> loadInfo = httpChannel->LoadInfo();
  mShouldFireLoadEvent =
      loadInfo->GetTainting() == LoadTainting::Opaque ||
      (loadInfo->GetTainting() == LoadTainting::CORS &&
       (NS_FAILED(httpChannel->GetStatus(&rv)) || NS_FAILED(rv)));

  // no need to prefetch http error page
  bool requestSucceeded;
  if (NS_FAILED(httpChannel->GetRequestSucceeded(&requestSucceeded)) ||
      !requestSucceeded) {
    return NS_BINDING_ABORTED;
  }

  nsCOMPtr<nsICacheInfoChannel> cacheInfoChannel =
      do_QueryInterface(aRequest, &rv);
  if (NS_FAILED(rv)) return rv;

  // no need to prefetch a document that is already in the cache
  bool fromCache;
  if (NS_SUCCEEDED(cacheInfoChannel->IsFromCache(&fromCache)) && fromCache) {
    LOG(("document is already in the cache; canceling prefetch\n"));
    // although it's canceled we still want to fire load event
    mShouldFireLoadEvent = true;
    return NS_BINDING_ABORTED;
  }

  //
  // no need to prefetch a document that must be requested fresh each
  // and every time.
  //
  uint32_t expTime;
  if (NS_SUCCEEDED(cacheInfoChannel->GetCacheTokenExpirationTime(&expTime))) {
    if (NowInSeconds() >= expTime) {
      LOG(
          ("document cannot be reused from cache; "
           "canceling prefetch\n"));
      return NS_BINDING_ABORTED;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPrefetchNode::OnDataAvailable(nsIRequest* aRequest, nsIInputStream* aStream,
                                uint64_t aOffset, uint32_t aCount) {
  uint32_t bytesRead = 0;
  aStream->ReadSegments(NS_DiscardSegment, nullptr, aCount, &bytesRead);
  mBytesRead += bytesRead;
  LOG(("prefetched %u bytes [offset=%" PRIu64 "]\n", bytesRead, aOffset));
  return NS_OK;
}

NS_IMETHODIMP
nsPrefetchNode::OnStopRequest(nsIRequest* aRequest, nsresult aStatus) {
  LOG(("done prefetching [status=%" PRIx32 "]\n",
       static_cast<uint32_t>(aStatus)));

  if (mBytesRead == 0 && aStatus == NS_OK && mChannel) {
    // we didn't need to read (because LOAD_ONLY_IF_MODIFIED was
    // specified), but the object should report loadedSize as if it
    // did.
    mChannel->GetContentLength(&mBytesRead);
  }

  mService->NotifyLoadCompleted(this);
  mService->DispatchEvent(this, mShouldFireLoadEvent || NS_SUCCEEDED(aStatus));
  mService->RemoveNodeAndMaybeStartNextPrefetchURI(this);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchNode::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchNode::GetInterface(const nsIID& aIID, void** aResult) {
  if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
    NS_ADDREF_THIS();
    *aResult = static_cast<nsIChannelEventSink*>(this);
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIRedirectResultListener))) {
    NS_ADDREF_THIS();
    *aResult = static_cast<nsIRedirectResultListener*>(this);
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

//-----------------------------------------------------------------------------
// nsPrefetchNode::nsIChannelEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchNode::AsyncOnChannelRedirect(
    nsIChannel* aOldChannel, nsIChannel* aNewChannel, uint32_t aFlags,
    nsIAsyncVerifyRedirectCallback* callback) {
  nsCOMPtr<nsIURI> newURI;
  nsresult rv = aNewChannel->GetURI(getter_AddRefs(newURI));
  if (NS_FAILED(rv)) return rv;

  if (!newURI->SchemeIs("http") && !newURI->SchemeIs("https")) {
    LOG(("rejected: URL is not of type http/https\n"));
    return NS_ERROR_ABORT;
  }

  // HTTP request headers are not automatically forwarded to the new channel.
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aNewChannel);
  NS_ENSURE_STATE(httpChannel);

  rv = httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("X-Moz"),
                                     NS_LITERAL_CSTRING("prefetch"), false);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

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
nsPrefetchNode::OnRedirectResult(bool proceeding) {
  if (proceeding && mRedirectChannel) mChannel = mRedirectChannel;

  mRedirectChannel = nullptr;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchService <public>
//-----------------------------------------------------------------------------

nsPrefetchService::nsPrefetchService()
    : mMaxParallelism(6),
      mStopCount(0),
      mHaveProcessed(false),
      mPrefetchDisabled(true),
      mPreloadDisabled(true),
      mAggressive(false) {}

nsPrefetchService::~nsPrefetchService() {
  Preferences::RemoveObserver(this, PREFETCH_PREF);
  Preferences::RemoveObserver(this, PRELOAD_PREF);
  Preferences::RemoveObserver(this, PARALLELISM_PREF);
  Preferences::RemoveObserver(this, AGGRESSIVE_PREF);
  // cannot reach destructor if prefetch in progress (listener owns reference
  // to this service)
  EmptyPrefetchQueue();
}

nsresult nsPrefetchService::Init() {
  nsresult rv;

  // read prefs and hook up pref observer
  mPrefetchDisabled = !Preferences::GetBool(PREFETCH_PREF, !mPrefetchDisabled);
  Preferences::AddWeakObserver(this, PREFETCH_PREF);

  mPreloadDisabled = !Preferences::GetBool(PRELOAD_PREF, !mPreloadDisabled);
  Preferences::AddWeakObserver(this, PRELOAD_PREF);

  mMaxParallelism = Preferences::GetInt(PARALLELISM_PREF, mMaxParallelism);
  if (mMaxParallelism < 1) {
    mMaxParallelism = 1;
  }
  Preferences::AddWeakObserver(this, PARALLELISM_PREF);

  mAggressive = Preferences::GetBool(AGGRESSIVE_PREF, false);
  Preferences::AddWeakObserver(this, AGGRESSIVE_PREF);

  // Observe xpcom-shutdown event
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (!observerService) return NS_ERROR_FAILURE;

  rv = observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, true);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mPrefetchDisabled || !mPreloadDisabled) {
    AddProgressListener();
  }

  return NS_OK;
}

void nsPrefetchService::RemoveNodeAndMaybeStartNextPrefetchURI(
    nsPrefetchNode* aFinished) {
  if (aFinished) {
    mCurrentNodes.RemoveElement(aFinished);
  }

  if ((!mStopCount && mHaveProcessed) || mAggressive) {
    ProcessNextPrefetchURI();
  }
}

void nsPrefetchService::ProcessNextPrefetchURI() {
  if (mCurrentNodes.Length() >= static_cast<uint32_t>(mMaxParallelism)) {
    // We already have enough prefetches going on, so hold off
    // for now.
    return;
  }

  nsresult rv;

  do {
    if (mPrefetchQueue.empty()) {
      break;
    }
    RefPtr<nsPrefetchNode> node = mPrefetchQueue.front().forget();
    mPrefetchQueue.pop_front();

    if (LOG_ENABLED()) {
      LOG(("ProcessNextPrefetchURI [%s]\n",
           node->mURI->GetSpecOrDefault().get()));
    }

    //
    // if opening the channel fails (e.g. security check returns an error),
    // send an error event and then just skip to the next uri
    //
    rv = node->OpenChannel();
    if (NS_SUCCEEDED(rv)) {
      mCurrentNodes.AppendElement(node);
    } else {
      DispatchEvent(node, false);
    }
  } while (NS_FAILED(rv));
}

void nsPrefetchService::NotifyLoadRequested(nsPrefetchNode* node) {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (!observerService) return;

  observerService->NotifyObservers(
      static_cast<nsIStreamListener*>(node),
      (node->mPreload) ? "preload-load-requested" : "prefetch-load-requested",
      nullptr);
}

void nsPrefetchService::NotifyLoadCompleted(nsPrefetchNode* node) {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (!observerService) return;

  observerService->NotifyObservers(
      static_cast<nsIStreamListener*>(node),
      (node->mPreload) ? "preload-load-completed" : "prefetch-load-completed",
      nullptr);
}

void nsPrefetchService::DispatchEvent(nsPrefetchNode* node, bool aSuccess) {
  for (uint32_t i = 0; i < node->mSources.Length(); i++) {
    nsCOMPtr<nsINode> domNode = do_QueryReferent(node->mSources.ElementAt(i));
    if (domNode && domNode->IsInComposedDoc()) {
      // We don't dispatch synchronously since |node| might be in a DocGroup
      // that we're not allowed to touch. (Our network request happens in the
      // DocGroup of one of the mSources nodes--not necessarily this one).
      RefPtr<AsyncEventDispatcher> dispatcher = new AsyncEventDispatcher(
          domNode,
          aSuccess ? NS_LITERAL_STRING("load") : NS_LITERAL_STRING("error"),
          CanBubble::eNo);
      dispatcher->RequireNodeInDocument();
      dispatcher->PostDOMEvent();
    }
  }
}

//-----------------------------------------------------------------------------
// nsPrefetchService <private>
//-----------------------------------------------------------------------------

void nsPrefetchService::AddProgressListener() {
  // Register as an observer for the document loader
  nsCOMPtr<nsIWebProgress> progress = components::DocLoader::Service();
  if (progress)
    progress->AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_DOCUMENT);
}

void nsPrefetchService::RemoveProgressListener() {
  // Register as an observer for the document loader
  nsCOMPtr<nsIWebProgress> progress = components::DocLoader::Service();
  if (progress) progress->RemoveProgressListener(this);
}

nsresult nsPrefetchService::EnqueueURI(nsIURI* aURI,
                                       nsIReferrerInfo* aReferrerInfo,
                                       nsINode* aSource,
                                       nsPrefetchNode** aNode) {
  RefPtr<nsPrefetchNode> node = new nsPrefetchNode(
      this, aURI, aReferrerInfo, aSource, nsIContentPolicy::TYPE_OTHER, false);
  mPrefetchQueue.push_back(node);
  node.forget(aNode);
  return NS_OK;
}

void nsPrefetchService::EmptyPrefetchQueue() {
  while (!mPrefetchQueue.empty()) {
    mPrefetchQueue.pop_back();
  }
}

void nsPrefetchService::StartPrefetching() {
  //
  // at initialization time we might miss the first DOCUMENT START
  // notification, so we have to be careful to avoid letting our
  // stop count go negative.
  //
  if (mStopCount > 0) mStopCount--;

  LOG(("StartPrefetching [stopcount=%d]\n", mStopCount));

  // only start prefetching after we've received enough DOCUMENT
  // STOP notifications.  we do this inorder to defer prefetching
  // until after all sub-frames have finished loading.
  if (!mStopCount) {
    mHaveProcessed = true;
    while (!mPrefetchQueue.empty() &&
           mCurrentNodes.Length() < static_cast<uint32_t>(mMaxParallelism)) {
      ProcessNextPrefetchURI();
    }
  }
}

void nsPrefetchService::StopPrefetching() {
  mStopCount++;

  LOG(("StopPrefetching [stopcount=%d]\n", mStopCount));

  // When we start a load, we need to stop all prefetches that has been
  // added by the old load, therefore call StopAll only at the moment we
  // switch to a new page load (i.e. mStopCount == 1).
  // TODO: do not stop prefetches that are relevant for the new load.
  if (mStopCount == 1) {
    StopAll();
  }
}

void nsPrefetchService::StopCurrentPrefetchsPreloads(bool aPreload) {
  for (int32_t i = mCurrentNodes.Length() - 1; i >= 0; --i) {
    if (mCurrentNodes[i]->mPreload == aPreload) {
      mCurrentNodes[i]->CancelChannel(NS_BINDING_ABORTED);
      mCurrentNodes.RemoveElementAt(i);
    }
  }

  if (!aPreload) {
    EmptyPrefetchQueue();
  }
}

void nsPrefetchService::StopAll() {
  for (uint32_t i = 0; i < mCurrentNodes.Length(); ++i) {
    mCurrentNodes[i]->CancelChannel(NS_BINDING_ABORTED);
  }
  mCurrentNodes.Clear();
  EmptyPrefetchQueue();
}

nsresult nsPrefetchService::CheckURIScheme(nsIURI* aURI,
                                           nsIReferrerInfo* aReferrerInfo) {
  //
  // XXX we should really be asking the protocol handler if it supports
  // caching, so we can determine if there is any value to prefetching.
  // for now, we'll only prefetch http and https links since we know that's
  // the most common case.
  //
  if (!aURI->SchemeIs("http") && !aURI->SchemeIs("https")) {
    LOG(("rejected: URL is not of type http/https\n"));
    return NS_ERROR_ABORT;
  }

  //
  // the referrer URI must be http:
  //
  nsCOMPtr<nsIURI> referrer = aReferrerInfo->GetOriginalReferrer();
  if (!referrer) {
    return NS_ERROR_ABORT;
  }

  if (!referrer->SchemeIs("http") && !referrer->SchemeIs("https")) {
    LOG(("rejected: referrer URL is neither http nor https\n"));
    return NS_ERROR_ABORT;
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchService::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsPrefetchService, nsIPrefetchService, nsIWebProgressListener,
                  nsIObserver, nsISupportsWeakReference)

//-----------------------------------------------------------------------------
// nsPrefetchService::nsIPrefetchService
//-----------------------------------------------------------------------------

nsresult nsPrefetchService::Preload(nsIURI* aURI,
                                    nsIReferrerInfo* aReferrerInfo,
                                    nsINode* aSource,
                                    nsContentPolicyType aPolicyType) {
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aReferrerInfo);
  if (LOG_ENABLED()) {
    LOG(("PreloadURI [%s]\n", aURI->GetSpecOrDefault().get()));
  }

  if (mPreloadDisabled) {
    LOG(("rejected: preload service is disabled\n"));
    return NS_ERROR_ABORT;
  }

  nsresult rv = CheckURIScheme(aURI, aReferrerInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // XXX we might want to either leverage nsIProtocolHandler::protocolFlags
  // or possibly nsIRequest::loadFlags to determine if this URI should be
  // prefetched.
  //

  if (aPolicyType == nsIContentPolicy::TYPE_INVALID) {
    if (aSource && aSource->IsInComposedDoc()) {
      RefPtr<AsyncEventDispatcher> asyncDispatcher =
          new AsyncEventDispatcher(aSource, NS_LITERAL_STRING("error"),
                                   CanBubble::eNo, ChromeOnlyDispatch::eNo);
      asyncDispatcher->RunDOMEventWhenSafe();
    }
    return NS_OK;
  }

  //
  // Check whether it is being preloaded.
  //
  for (uint32_t i = 0; i < mCurrentNodes.Length(); ++i) {
    bool equals;
    if ((mCurrentNodes[i]->mPolicyType == aPolicyType) &&
        NS_SUCCEEDED(mCurrentNodes[i]->mURI->Equals(aURI, &equals)) && equals) {
      nsWeakPtr source = do_GetWeakReference(aSource);
      if (mCurrentNodes[i]->mSources.IndexOf(source) ==
          mCurrentNodes[i]->mSources.NoIndex) {
        LOG(
            ("URL is already being preloaded, add a new reference "
             "document\n"));
        mCurrentNodes[i]->mSources.AppendElement(source);
        return NS_OK;
      } else {
        LOG(("URL is already being preloaded by this document"));
        return NS_ERROR_ABORT;
      }
    }
  }

  LOG(("This is a preload, so start loading immediately.\n"));
  RefPtr<nsPrefetchNode> enqueuedNode;
  enqueuedNode =
      new nsPrefetchNode(this, aURI, aReferrerInfo, aSource, aPolicyType, true);

  NotifyLoadRequested(enqueuedNode);
  rv = enqueuedNode->OpenChannel();
  if (NS_SUCCEEDED(rv)) {
    mCurrentNodes.AppendElement(enqueuedNode);
  } else {
    if (aSource && aSource->IsInComposedDoc()) {
      RefPtr<AsyncEventDispatcher> asyncDispatcher =
          new AsyncEventDispatcher(aSource, NS_LITERAL_STRING("error"),
                                   CanBubble::eNo, ChromeOnlyDispatch::eNo);
      asyncDispatcher->RunDOMEventWhenSafe();
    }
  }
  return NS_OK;
}

nsresult nsPrefetchService::Prefetch(nsIURI* aURI,
                                     nsIReferrerInfo* aReferrerInfo,
                                     nsINode* aSource, bool aExplicit) {
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aReferrerInfo);

  if (LOG_ENABLED()) {
    LOG(("PrefetchURI [%s]\n", aURI->GetSpecOrDefault().get()));
  }

  if (mPrefetchDisabled) {
    LOG(("rejected: prefetch service is disabled\n"));
    return NS_ERROR_ABORT;
  }

  nsresult rv = CheckURIScheme(aURI, aReferrerInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // XXX we might want to either leverage nsIProtocolHandler::protocolFlags
  // or possibly nsIRequest::loadFlags to determine if this URI should be
  // prefetched.
  //

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
    if (NS_SUCCEEDED(mCurrentNodes[i]->mURI->Equals(aURI, &equals)) && equals) {
      nsWeakPtr source = do_GetWeakReference(aSource);
      if (mCurrentNodes[i]->mSources.IndexOf(source) ==
          mCurrentNodes[i]->mSources.NoIndex) {
        LOG(
            ("URL is already being prefetched, add a new reference "
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
  for (std::deque<RefPtr<nsPrefetchNode>>::iterator nodeIt =
           mPrefetchQueue.begin();
       nodeIt != mPrefetchQueue.end(); nodeIt++) {
    bool equals;
    RefPtr<nsPrefetchNode> node = nodeIt->get();
    if (NS_SUCCEEDED(node->mURI->Equals(aURI, &equals)) && equals) {
      nsWeakPtr source = do_GetWeakReference(aSource);
      if (node->mSources.IndexOf(source) == node->mSources.NoIndex) {
        LOG(
            ("URL is already being prefetched, add a new reference "
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
  rv = EnqueueURI(aURI, aReferrerInfo, aSource, getter_AddRefs(enqueuedNode));
  NS_ENSURE_SUCCESS(rv, rv);

  NotifyLoadRequested(enqueuedNode);

  // if there are no pages loading, kick off the request immediately
  if ((!mStopCount && mHaveProcessed) || mAggressive) {
    ProcessNextPrefetchURI();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPrefetchService::CancelPrefetchPreloadURI(nsIURI* aURI, nsINode* aSource) {
  NS_ENSURE_ARG_POINTER(aURI);

  if (LOG_ENABLED()) {
    LOG(("CancelPrefetchURI [%s]\n", aURI->GetSpecOrDefault().get()));
  }

  //
  // look in current prefetches
  //
  for (uint32_t i = 0; i < mCurrentNodes.Length(); ++i) {
    bool equals;
    if (NS_SUCCEEDED(mCurrentNodes[i]->mURI->Equals(aURI, &equals)) && equals) {
      nsWeakPtr source = do_GetWeakReference(aSource);
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
  for (std::deque<RefPtr<nsPrefetchNode>>::iterator nodeIt =
           mPrefetchQueue.begin();
       nodeIt != mPrefetchQueue.end(); nodeIt++) {
    bool equals;
    RefPtr<nsPrefetchNode> node = nodeIt->get();
    if (NS_SUCCEEDED(node->mURI->Equals(aURI, &equals)) && equals) {
      nsWeakPtr source = do_GetWeakReference(aSource);
      if (node->mSources.IndexOf(source) != node->mSources.NoIndex) {
#ifdef DEBUG
        int32_t inx = node->mSources.IndexOf(source);
        nsCOMPtr<nsINode> domNode =
            do_QueryReferent(node->mSources.ElementAt(inx));
        MOZ_ASSERT(domNode);
#endif

        node->mSources.RemoveElement(source);
        if (node->mSources.IsEmpty()) {
          mPrefetchQueue.erase(nodeIt);
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
nsPrefetchService::PreloadURI(nsIURI* aURI, nsIReferrerInfo* aReferrerInfo,
                              nsINode* aSource,
                              nsContentPolicyType aPolicyType) {
  return Preload(aURI, aReferrerInfo, aSource, aPolicyType);
}

NS_IMETHODIMP
nsPrefetchService::PrefetchURI(nsIURI* aURI, nsIReferrerInfo* aReferrerInfo,
                               nsINode* aSource, bool aExplicit) {
  return Prefetch(aURI, aReferrerInfo, aSource, aExplicit);
}

NS_IMETHODIMP
nsPrefetchService::HasMoreElements(bool* aHasMore) {
  *aHasMore = (mCurrentNodes.Length() || !mPrefetchQueue.empty());
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchService::nsIWebProgressListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchService::OnProgressChange(nsIWebProgress* aProgress,
                                    nsIRequest* aRequest,
                                    int32_t curSelfProgress,
                                    int32_t maxSelfProgress,
                                    int32_t curTotalProgress,
                                    int32_t maxTotalProgress) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsPrefetchService::OnStateChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 uint32_t progressStateFlags,
                                 nsresult aStatus) {
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
                                    nsIRequest* aRequest, nsIURI* location,
                                    uint32_t aFlags) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsPrefetchService::OnStatusChange(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest, nsresult aStatus,
                                  const char16_t* aMessage) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsPrefetchService::OnSecurityChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest, uint32_t aState) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsPrefetchService::OnContentBlockingEvent(nsIWebProgress* aWebProgress,
                                          nsIRequest* aRequest,
                                          uint32_t aEvent) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchService::nsIObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchService::Observe(nsISupports* aSubject, const char* aTopic,
                           const char16_t* aData) {
  LOG(("nsPrefetchService::Observe [topic=%s]\n", aTopic));

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    StopAll();
    mPrefetchDisabled = true;
    mPreloadDisabled = true;
  } else if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    const nsCString converted = NS_ConvertUTF16toUTF8(aData);
    const char* pref = converted.get();
    if (!strcmp(pref, PREFETCH_PREF)) {
      if (Preferences::GetBool(PREFETCH_PREF, false)) {
        if (mPrefetchDisabled) {
          LOG(("enabling prefetching\n"));
          mPrefetchDisabled = false;
          if (mPreloadDisabled) {
            AddProgressListener();
          }
        }
      } else {
        if (!mPrefetchDisabled) {
          LOG(("disabling prefetching\n"));
          StopCurrentPrefetchsPreloads(false);
          mPrefetchDisabled = true;
          if (mPreloadDisabled) {
            RemoveProgressListener();
          }
        }
      }
    } else if (!strcmp(pref, PRELOAD_PREF)) {
      if (Preferences::GetBool(PRELOAD_PREF, false)) {
        if (mPreloadDisabled) {
          LOG(("enabling preloading\n"));
          mPreloadDisabled = false;
          if (mPrefetchDisabled) {
            AddProgressListener();
          }
        }
      } else {
        if (!mPreloadDisabled) {
          LOG(("disabling preloading\n"));
          StopCurrentPrefetchsPreloads(true);
          mPreloadDisabled = true;
          if (mPrefetchDisabled) {
            RemoveProgressListener();
          }
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
      while (((!mStopCount && mHaveProcessed) || mAggressive) &&
             !mPrefetchQueue.empty() &&
             mCurrentNodes.Length() < static_cast<uint32_t>(mMaxParallelism)) {
        ProcessNextPrefetchURI();
      }
    } else if (!strcmp(pref, AGGRESSIVE_PREF)) {
      mAggressive = Preferences::GetBool(AGGRESSIVE_PREF, false);
      // in aggressive mode, start prefetching immediately
      if (mAggressive) {
        while (mStopCount && !mPrefetchQueue.empty() &&
               mCurrentNodes.Length() <
                   static_cast<uint32_t>(mMaxParallelism)) {
          ProcessNextPrefetchURI();
        }
      }
    }
  }

  return NS_OK;
}

// vim: ts=4 sw=2 expandtab
