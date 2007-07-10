/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsPrefetchService.h"
#include "nsICacheSession.h"
#include "nsIOfflineCacheSession.h"
#include "nsICacheService.h"
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsIObserverService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"
#include "nsIDocCharset.h"
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
class nsPrefetchQueueEnumerator : public nsISimpleEnumerator
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISIMPLEENUMERATOR
    nsPrefetchQueueEnumerator(nsPrefetchService *aService,
                              PRBool aIncludeNormal,
                              PRBool aIncludeOffline);
    ~nsPrefetchQueueEnumerator();

private:
    void Increment();

    nsRefPtr<nsPrefetchService> mService;
    nsRefPtr<nsPrefetchNode> mCurrent;
    PRBool mIncludeNormal;
    PRBool mIncludeOffline;
    PRBool mStarted;
};

//-----------------------------------------------------------------------------
// nsPrefetchQueueEnumerator <public>
//-----------------------------------------------------------------------------
nsPrefetchQueueEnumerator::nsPrefetchQueueEnumerator(nsPrefetchService *aService,
                                                     PRBool aIncludeNormal,
                                                     PRBool aIncludeOffline)
    : mService(aService)
    , mIncludeNormal(aIncludeNormal)
    , mIncludeOffline(aIncludeOffline)
    , mStarted(PR_FALSE)
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
nsPrefetchQueueEnumerator::HasMoreElements(PRBool *aHasMore)
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
    do {
        if (!mStarted) {
            // If the service is currently serving a request, it won't be
            // in the pending queue, so we return it first.  If it isn't,
            // we'll just start with the pending queue.
            mStarted = PR_TRUE;
            mCurrent = mService->GetCurrentNode();
            if (!mCurrent)
                mCurrent = mService->GetQueueHead();
        }
        else if (mCurrent) {
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
    } while (mCurrent && mIncludeOffline != mCurrent->mOffline &&
             mIncludeNormal == mCurrent->mOffline);
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
                               nsIDOMNode *aSource,
                               PRBool aOffline)
    : mNext(nsnull)
    , mURI(aURI)
    , mReferrerURI(aReferrerURI)
    , mOffline(aOffline)
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
            mOffline ?
            NS_LITERAL_CSTRING("offline-resource") :
            NS_LITERAL_CSTRING("prefetch"),
            PR_FALSE);
    }

    if (mOffline) {
        nsCOMPtr<nsICachingChannel> cachingChannel =
            do_QueryInterface(mChannel);
        if (cachingChannel) {
            rv = cachingChannel->SetCacheForOfflineUse(PR_TRUE);
            NS_ENSURE_SUCCESS(rv, rv);
        }
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

    PRBool cacheForOfflineUse;
    rv = cachingChannel->GetCacheForOfflineUse(&cacheForOfflineUse);
    if (NS_FAILED(rv)) return rv;

    if (!cacheForOfflineUse) {
        // no need to prefetch a document that is already in the cache
        PRBool fromCache;
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
nsPrefetchNode::OnChannelRedirect(nsIChannel *aOldChannel,
                                  nsIChannel *aNewChannel,
                                  PRUint32 aFlags)
{
    nsCOMPtr<nsIURI> newURI;
    nsresult rv = aNewChannel->GetURI(getter_AddRefs(newURI));
    if (NS_FAILED(rv))
        return rv;

    PRBool offline;
    nsCOMPtr<nsICachingChannel> oldCachingChannel =
        do_QueryInterface(aOldChannel);
    if (NS_SUCCEEDED(oldCachingChannel->GetCacheForOfflineUse(&offline)) &&
        offline) {
        nsCOMPtr<nsICachingChannel> newCachingChannel =
            do_QueryInterface(aOldChannel);
        if (newCachingChannel)
            newCachingChannel->SetCacheForOfflineUse(PR_TRUE);
    }

    PRBool match;
    rv = newURI->SchemeIs("http", &match); 
    if (NS_FAILED(rv) || !match) {
        if (!offline ||
            NS_FAILED(newURI->SchemeIs("https", &match)) ||
            !match) {
            LOG(("rejected: URL is not of type http\n"));
            return NS_ERROR_ABORT;
        }
    }

    // HTTP request headers are not automatically forwarded to the new channel.
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aNewChannel);
    NS_ENSURE_STATE(httpChannel);

    httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("X-Moz"),
                                  offline ?
                                      NS_LITERAL_CSTRING("offline-resource") :
                                      NS_LITERAL_CSTRING("prefetch"),
                                  PR_FALSE);

    mChannel = aNewChannel;

    return NS_OK;
}


//-----------------------------------------------------------------------------
// nsPrefetchService <public>
//-----------------------------------------------------------------------------

nsPrefetchService::nsPrefetchService()
    : mQueueHead(nsnull)
    , mQueueTail(nsnull)
    , mStopCount(0)
    , mHaveProcessed(PR_FALSE)
    , mDisabled(PR_TRUE)
    , mFetchedOffline(PR_FALSE)
{
}

nsPrefetchService::~nsPrefetchService()
{
    // cannot reach destructor if prefetch in progress (listener owns reference
    // to this service)
    EmptyQueue(PR_TRUE);
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
    nsCOMPtr<nsIPrefBranch2> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv)) {
      PRBool enabled;
      rv = prefs->GetBoolPref(PREFETCH_PREF, &enabled);
      if (NS_SUCCEEDED(rv) && enabled)
        mDisabled = PR_FALSE;

      prefs->AddObserver(PREFETCH_PREF, this, PR_TRUE);
    }

    // Observe xpcom-shutdown event
    nsCOMPtr<nsIObserverService> observerServ(
            do_GetService("@mozilla.org/observer-service;1", &rv));
    if (NS_FAILED(rv)) return rv;

    rv = observerServ->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

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
        if (rv == NS_ERROR_NOT_AVAILABLE && mFetchedOffline) {
            // done loading stuff, go ahead and evict unowned entries from
            // the offline cache
            mFetchedOffline = PR_FALSE;

            nsCOMPtr<nsIOfflineCacheSession> session;
            rv = GetOfflineCacheSession(getter_AddRefs(session));
            if (NS_FAILED(rv)) break;

            session->EvictUnownedEntries();
            break;
        }

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
        if (NS_SUCCEEDED(rv) && mCurrentNode->mOffline)
            mFetchedOffline = PR_TRUE;
    }
    while (NS_FAILED(rv));
}

void
nsPrefetchService::NotifyLoadRequested(nsPrefetchNode *node)
{
    nsresult rv;

    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1", &rv);
    if (NS_FAILED(rv)) return;

    const char *topic = node->mOffline ? "offline-load-requested" :
                                         "prefetch-load-requested";

    observerService->NotifyObservers(static_cast<nsIDOMLoadStatus*>(node),
                                     topic, nsnull);
}

void
nsPrefetchService::NotifyLoadCompleted(nsPrefetchNode *node)
{
    nsresult rv;

    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1", &rv);
    if (NS_FAILED(rv)) return;

    const char *topic = node->mOffline ? "offline-load-completed" :
                                         "prefetch-load-completed";

    observerService->NotifyObservers(static_cast<nsIDOMLoadStatus*>(node),
                                     topic, nsnull);
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
                              PRBool aOffline,
                              nsPrefetchNode **aNode)
{
    nsPrefetchNode *node = new nsPrefetchNode(this, aURI, aReferrerURI,
                                              aSource, aOffline);
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
nsPrefetchService::EmptyQueue(PRBool includeOffline)
{
    nsPrefetchNode *prev = 0;
    nsPrefetchNode *node = mQueueHead;

    while (node) {
        nsPrefetchNode *next = node->mNext;
        if (includeOffline || !node->mOffline) {
            if (prev)
                prev->mNext = next;
            else
                mQueueHead = next;
            NS_RELEASE(node);
        }
        else
            prev = node;

        node = next;
    }
}

nsresult
nsPrefetchService::GetOfflineCacheSession(nsIOfflineCacheSession **aSession)
{
    if (!mOfflineCacheSession) {
        nsresult rv;
        nsCOMPtr<nsICacheService> serv =
            do_GetService(NS_CACHESERVICE_CONTRACTID,
                          &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsICacheSession> session;
        rv = serv->CreateSession("HTTP-offline",
                                 nsICache::STORE_OFFLINE,
                                 nsICache::STREAM_BASED,
                                 getter_AddRefs(session));
        NS_ENSURE_SUCCESS(rv, rv);

        mOfflineCacheSession = do_QueryInterface(session, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    NS_ADDREF(*aSession = mOfflineCacheSession);
    return NS_OK;
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
        mHaveProcessed = PR_TRUE;
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

    // if it's an offline prefetch, requeue it for when prefetching starts
    // again
    if (mCurrentNode->mOffline)
        EnqueueNode(mCurrentNode);

    mCurrentNode->CancelChannel(NS_BINDING_ABORTED);
    mCurrentNode = nsnull;
    EmptyQueue(PR_FALSE);
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
                            PRBool aExplicit,
                            PRBool aOffline)
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
    PRBool match;
    rv = aURI->SchemeIs("http", &match); 
    if (NS_FAILED(rv) || !match) {
        if (aOffline) {
            // Offline https urls can be prefetched
            rv = aURI->SchemeIs("https", &match);
        }

        if (NS_FAILED(rv) || !match) {
            LOG(("rejected: URL is not of type http\n"));
            return NS_ERROR_ABORT;
        }
    }

    // 
    // the referrer URI must be http:
    //
    rv = aReferrerURI->SchemeIs("http", &match);
    if (NS_FAILED(rv) || !match) {
        if (aOffline) {
            // Offline https urls can be prefetched
            rv = aURI->SchemeIs("https", &match);
        }

        if (NS_FAILED(rv) || !match) {
            LOG(("rejected: referrer URL is not of type http\n"));
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
        PRBool equals;
        if (NS_SUCCEEDED(mCurrentNode->mURI->Equals(aURI, &equals)) && equals) {
            // We may still need to put it on the queue if the channel
            // isn't fetching to the offline cache
            if (!aOffline || mCurrentNode->mOffline) {
                LOG(("rejected: URL is already being prefetched\n"));
                return NS_ERROR_ABORT;
            }
        }
    }

    //
    // cancel if already on the prefetch queue
    //
    nsPrefetchNode *node = mQueueHead;
    for (; node; node = node->mNext) {
        PRBool equals;
        if (NS_SUCCEEDED(node->mURI->Equals(aURI, &equals)) && equals) {
            if (aOffline) {
                // make sure the node is placed in the offline cache
                node->mOffline = PR_TRUE;
            }
            LOG(("rejected: URL is already on prefetch queue\n"));
            return NS_ERROR_ABORT;
        }
    }

    nsRefPtr<nsPrefetchNode> enqueuedNode;
    rv = EnqueueURI(aURI, aReferrerURI, aSource, aOffline,
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
                               PRBool aExplicit)
{
    return Prefetch(aURI, aReferrerURI, aSource, aExplicit, PR_FALSE);
}

NS_IMETHODIMP
nsPrefetchService::PrefetchURIForOfflineUse(nsIURI *aURI,
                                            nsIURI *aReferrerURI,
                                            nsIDOMNode *aSource,
                                            PRBool aExplicit)
{
    return Prefetch(aURI, aReferrerURI, aSource, aExplicit, PR_TRUE);
}

NS_IMETHODIMP
nsPrefetchService::EnumerateQueue(PRBool aIncludeNormalItems,
                                  PRBool aIncludeOfflineItems,
                                  nsISimpleEnumerator **aEnumerator)
{
    *aEnumerator = new nsPrefetchQueueEnumerator(this,
                                                 aIncludeNormalItems,
                                                 aIncludeOfflineItems);
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
                                    nsIURI *location)
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
        mDisabled = PR_TRUE;
    }
    else if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
        nsCOMPtr<nsIPrefBranch> prefs(do_QueryInterface(aSubject));
        PRBool enabled;
        nsresult rv = prefs->GetBoolPref(PREFETCH_PREF, &enabled);
        if (NS_SUCCEEDED(rv) && enabled) {
            if (mDisabled) {
                LOG(("enabling prefetching\n"));
                mDisabled = PR_FALSE;
                AddProgressListener();
            }
        } 
        else {
            if (!mDisabled) {
                LOG(("disabling prefetching\n"));
                StopPrefetching();
                mDisabled = PR_TRUE;
                RemoveProgressListener();
            }
        }
    }

    return NS_OK;
}

// vim: ts=4 sw=4 expandtab
