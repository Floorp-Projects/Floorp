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
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
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

static NS_DEFINE_IID(kDocLoaderServiceCID, NS_DOCUMENTLOADER_SERVICE_CID);
static NS_DEFINE_IID(kPrefServiceCID, NS_PREFSERVICE_CID);

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
// nsPrefetchListener <public>
//-----------------------------------------------------------------------------

nsPrefetchListener::nsPrefetchListener(nsPrefetchService *aService)
{
    NS_ADDREF(mService = aService);
}

nsPrefetchListener::~nsPrefetchListener()
{
    NS_RELEASE(mService);
}

//-----------------------------------------------------------------------------
// nsPrefetchListener <private>
//-----------------------------------------------------------------------------

NS_METHOD
nsPrefetchListener::ConsumeSegments(nsIInputStream *aInputStream,
                                    void *aClosure,
                                    const char *aFromSegment,
                                    PRUint32 aOffset,
                                    PRUint32 aCount,
                                    PRUint32 *aBytesConsumed)
{
    *aBytesConsumed = aCount;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchListener::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS4(nsPrefetchListener,
                   nsIRequestObserver,
                   nsIStreamListener,
                   nsIInterfaceRequestor,
                   nsIChannelEventSink)

//-----------------------------------------------------------------------------
// nsPrefetchListener::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchListener::OnStartRequest(nsIRequest *aRequest,
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
    return NS_OK;
}

NS_IMETHODIMP
nsPrefetchListener::OnDataAvailable(nsIRequest *aRequest,
                                    nsISupports *aContext,
                                    nsIInputStream *aStream,
                                    PRUint32 aOffset,
                                    PRUint32 aCount)
{
    PRUint32 bytesRead = 0;
    aStream->ReadSegments(ConsumeSegments, nsnull, aCount, &bytesRead);
    LOG(("prefetched %u bytes [offset=%u]\n", bytesRead, aOffset));
    return NS_OK;
}

NS_IMETHODIMP
nsPrefetchListener::OnStopRequest(nsIRequest *aRequest,
                                  nsISupports *aContext,
                                  nsresult aStatus)
{
    LOG(("done prefetching [status=%x]\n", aStatus));

    mService->ProcessNextURI();
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchListener::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchListener::GetInterface(const nsIID &aIID, void **aResult)
{
    if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
        NS_ADDREF_THIS();
        *aResult = NS_STATIC_CAST(nsIChannelEventSink *, this);
        return NS_OK;
    }

    return NS_ERROR_NO_INTERFACE;
}

//-----------------------------------------------------------------------------
// nsPrefetchListener::nsIChannelEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsPrefetchListener::OnChannelRedirect(nsIChannel *aOldChannel,
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

    mService->UpdateCurrentChannel(aNewChannel);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsPrefetchService <public>
//-----------------------------------------------------------------------------

nsPrefetchService::nsPrefetchService()
    : mQueueHead(nsnull)
    , mQueueTail(nsnull)
    , mStopCount(0)
    , mDisabled(PR_TRUE)
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
    nsCOMPtr<nsIPrefBranch2> prefs(do_GetService(kPrefServiceCID, &rv));
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

    mCurrentChannel = nsnull;

    nsRefPtr<nsPrefetchListener> listener(new nsPrefetchListener(this));
    if (!listener) return;

    do {
        PRBool offline;
        rv = DequeueURI(getter_AddRefs(uri), getter_AddRefs(referrer),
                        &offline);
        if (NS_FAILED(rv)) break;

#if defined(PR_LOGGING)
        if (LOG_ENABLED()) {
            nsCAutoString spec;
            uri->GetSpec(spec);
            LOG(("ProcessNextURI [%s]\n", spec.get()));
        }
#endif

        //
        // if opening the channel fails, then just skip to the next uri
        //
        rv = NS_NewChannel(getter_AddRefs(mCurrentChannel), uri,
                           nsnull, nsnull, listener,
                           nsIRequest::LOAD_BACKGROUND |
                           nsICachingChannel::LOAD_ONLY_IF_MODIFIED);
        if (NS_FAILED(rv)) continue;

        // configure HTTP specific stuff
        nsCOMPtr<nsIHttpChannel> httpChannel =
            do_QueryInterface(mCurrentChannel);
        if (httpChannel) {
            httpChannel->SetReferrer(referrer);
            httpChannel->SetRequestHeader(
                NS_LITERAL_CSTRING("X-Moz"),
                offline ?
                    NS_LITERAL_CSTRING("offline-resource") :
                    NS_LITERAL_CSTRING("prefetch"),
                PR_FALSE);
        }

        if (offline) {
            nsCOMPtr<nsICachingChannel> cachingChannel =
                do_QueryInterface(mCurrentChannel);
            if (cachingChannel) {
                if (NS_FAILED(cachingChannel->SetCacheForOfflineUse(PR_TRUE))) {
                    continue;
                }
            }
        }

        rv = mCurrentChannel->AsyncOpen(listener, nsnull);
    }
    while (NS_FAILED(rv));
}

//-----------------------------------------------------------------------------
// nsPrefetchService <private>
//-----------------------------------------------------------------------------

void
nsPrefetchService::AddProgressListener()
{
    // Register as an observer for the document loader  
    nsCOMPtr<nsIWebProgress> progress(do_GetService(kDocLoaderServiceCID));
    if (progress)
        progress->AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_DOCUMENT);
}

void
nsPrefetchService::RemoveProgressListener()
{
    // Register as an observer for the document loader  
    nsCOMPtr<nsIWebProgress> progress(do_GetService(kDocLoaderServiceCID));
    if (progress)
        progress->RemoveProgressListener(this);
}

nsresult
nsPrefetchService::EnqueueURI(nsIURI *aURI,
                              nsIURI *aReferrerURI,
                              PRBool aOffline)
{
    nsPrefetchNode *node = new nsPrefetchNode(aURI, aReferrerURI, aOffline);
    if (!node)
        return NS_ERROR_OUT_OF_MEMORY;

    if (!mQueueTail) {
        mQueueHead = node;
        mQueueTail = node;
    }
    else {
        mQueueTail->mNext = node;
        mQueueTail = node;
    }

    return NS_OK;
}

nsresult
nsPrefetchService::DequeueURI(nsIURI **aURI,
                              nsIURI **aReferrerURI,
                              PRBool *aOffline)
{
    if (!mQueueHead)
        return NS_ERROR_NOT_AVAILABLE;

    // remove from the head
    NS_ADDREF(*aURI = mQueueHead->mURI);
    NS_ADDREF(*aReferrerURI = mQueueHead->mReferrerURI);
    *aOffline = mQueueHead->mOffline;

    nsPrefetchNode *node = mQueueHead;
    mQueueHead = mQueueHead->mNext;
    delete node;

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
            delete node;
        }
        else
            prev = node;

        node = next;
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
    if (mStopCount == 0 && !mCurrentChannel)
        ProcessNextURI();
}

void
nsPrefetchService::StopPrefetching()
{
    mStopCount++;

    LOG(("StopPrefetching [stopcount=%d]\n", mStopCount));

    // only kill the prefetch queue if we've actually started prefetching.
    if (!mCurrentChannel)
        return;

    // if it's an offline prefetch, requeue it for when prefetching starts
    // again
    nsCOMPtr<nsICachingChannel> cachingChannel =
        do_QueryInterface(mCurrentChannel);
    PRBool offline;
    if (cachingChannel &&
        NS_SUCCEEDED(cachingChannel->GetCacheForOfflineUse(&offline)) &&
        offline) {
        nsCOMPtr<nsIHttpChannel> httpChannel =
            do_QueryInterface(mCurrentChannel);
        nsCOMPtr<nsIURI> uri;
        nsCOMPtr<nsIURI> referrerURI;
        if (NS_SUCCEEDED(mCurrentChannel->GetURI(getter_AddRefs(uri))) &&
            NS_SUCCEEDED(httpChannel->GetReferrer(getter_AddRefs(referrerURI)))) {
            EnqueueURI(uri, referrerURI, PR_TRUE);
        }
    }

    mCurrentChannel->Cancel(NS_BINDING_ABORTED);
    mCurrentChannel = nsnull;
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
    if (mCurrentChannel) {
        nsCOMPtr<nsIURI> currentURI;
        mCurrentChannel->GetURI(getter_AddRefs(currentURI));
        if (currentURI) {
            PRBool equals;
            if (NS_SUCCEEDED(currentURI->Equals(aURI, &equals)) && equals) {
                if (aOffline) {
                    // We may still need to put it on the queue if the channel
                    // isn't fetching to the offline cache
                    nsCOMPtr<nsICachingChannel> cachingChannel =
                        do_QueryInterface(mCurrentChannel, &rv);
                    if (NS_SUCCEEDED(rv)) {
                        PRBool offline;
                        rv = cachingChannel->GetCacheForOfflineUse(&offline);
                        if (NS_SUCCEEDED(rv) && offline) {
                            LOG(("rejected: URL is already being prefetched\n"));
                            return NS_ERROR_ABORT;
                        }
                    }
                }
                else {
                    LOG(("rejected: URL is already being prefetched\n"));
                    return NS_ERROR_ABORT;
                }
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

    return EnqueueURI(aURI, aReferrerURI, aOffline);
}

NS_IMETHODIMP
nsPrefetchService::PrefetchURI(nsIURI *aURI,
                               nsIURI *aReferrerURI,
                               PRBool aExplicit)
{
    return Prefetch(aURI, aReferrerURI, aExplicit, PR_FALSE);
}

NS_IMETHODIMP
nsPrefetchService::PrefetchURIForOfflineUse(nsIURI *aURI,
                                            nsIURI *aReferrerURI,
                                            PRBool aExplicit)
{
    return Prefetch(aURI, aReferrerURI, aExplicit, PR_TRUE);
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
