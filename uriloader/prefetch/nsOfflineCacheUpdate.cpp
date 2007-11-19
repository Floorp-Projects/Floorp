/* -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
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

#include "nsOfflineCacheUpdate.h"

#include "nsCPrefetchService.h"
#include "nsCURILoader.h"
#include "nsICache.h"
#include "nsICacheService.h"
#include "nsICacheSession.h"
#include "nsICachingChannel.h"
#include "nsIDOMWindow.h"
#include "nsIObserverService.h"
#include "nsIOfflineCacheSession.h"
#include "nsIWebProgress.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "prlog.h"

static nsOfflineCacheUpdateService *gOfflineCacheUpdateService = nsnull;

#if defined(PR_LOGGING)
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsOfflineCacheUpdate:5
//    set NSPR_LOG_FILE=offlineupdate.log
//
// this enables PR_LOG_ALWAYS level information and places all output in
// the file offlineupdate.log
//
static PRLogModuleInfo *gOfflineCacheUpdateLog;
#endif
#define LOG(args) PR_LOG(gOfflineCacheUpdateLog, 4, args)
#define LOG_ENABLED() PR_LOG_TEST(gOfflineCacheUpdateLog, 4)

class AutoFreeArray {
public:
    AutoFreeArray(PRUint32 count, char **values)
        : mCount(count), mValues(values) {};
    ~AutoFreeArray() { NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(mCount, mValues); }
private:
    PRUint32 mCount;
    char **mValues;
};

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateItem::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS6(nsOfflineCacheUpdateItem,
                   nsIDOMLoadStatus,
                   nsIRequestObserver,
                   nsIStreamListener,
                   nsIRunnable,
                   nsIInterfaceRequestor,
                   nsIChannelEventSink)

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateItem <public>
//-----------------------------------------------------------------------------

nsOfflineCacheUpdateItem::nsOfflineCacheUpdateItem(nsOfflineCacheUpdate *aUpdate,
                                                   nsIURI *aURI,
                                                   nsIURI *aReferrerURI,
                                                   nsIDOMNode *aSource,
                                                   const nsACString &aClientID)
    : mURI(aURI)
    , mReferrerURI(aReferrerURI)
    , mClientID(aClientID)
    , mUpdate(aUpdate)
    , mChannel(nsnull)
    , mState(nsIDOMLoadStatus::UNINITIALIZED)
    , mBytesRead(0)
{
    mSource = do_GetWeakReference(aSource);
}

nsOfflineCacheUpdateItem::~nsOfflineCacheUpdateItem()
{
}

nsresult
nsOfflineCacheUpdateItem::OpenChannel()
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
        httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("X-Moz"),
                                      NS_LITERAL_CSTRING("offline-resource"),
                                      PR_FALSE);
    }

    nsCOMPtr<nsICachingChannel> cachingChannel =
        do_QueryInterface(mChannel);
    if (cachingChannel) {
        rv = cachingChannel->SetCacheForOfflineUse(PR_TRUE);
        NS_ENSURE_SUCCESS(rv, rv);

        if (!mClientID.IsEmpty()) {
            rv = cachingChannel->SetOfflineCacheClientID(mClientID);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    rv = mChannel->AsyncOpen(this, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    mState = nsIDOMLoadStatus::REQUESTED;

    return NS_OK;
}

nsresult
nsOfflineCacheUpdateItem::Cancel()
{
    if (mChannel) {
        mChannel->Cancel(NS_ERROR_ABORT);
        mChannel = nsnull;
    }

    mState = nsIDOMLoadStatus::UNINITIALIZED;

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateItem::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsOfflineCacheUpdateItem::OnStartRequest(nsIRequest *aRequest,
                                         nsISupports *aContext)
{
    mState = nsIDOMLoadStatus::RECEIVING;

    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdateItem::OnDataAvailable(nsIRequest *aRequest,
                                          nsISupports *aContext,
                                          nsIInputStream *aStream,
                                          PRUint32 aOffset,
                                          PRUint32 aCount)
{
    PRUint32 bytesRead = 0;
    aStream->ReadSegments(NS_DiscardSegment, nsnull, aCount, &bytesRead);
    mBytesRead += bytesRead;
    LOG(("loaded %u bytes into offline cache [offset=%u]\n",
         bytesRead, aOffset));
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdateItem::OnStopRequest(nsIRequest *aRequest,
                                        nsISupports *aContext,
                                        nsresult aStatus)
{
    LOG(("done fetching offline item [status=%x]\n", aStatus));

    mState = nsIDOMLoadStatus::LOADED;

    if (mBytesRead == 0 && aStatus == NS_OK) {
        // we didn't need to read (because LOAD_ONLY_IF_MODIFIED was
        // specified), but the object should report loadedSize as if it
        // did.
        mChannel->GetContentLength(&mBytesRead);
    }

    // We need to notify the update that the load is complete, but we
    // want to give the channel a chance to close the cache entries.
    NS_DispatchToCurrentThread(this);

    return NS_OK;
}


//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateItem::nsIRunnable
//-----------------------------------------------------------------------------
NS_IMETHODIMP
nsOfflineCacheUpdateItem::Run()
{
    mUpdate->LoadCompleted();

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateItem::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsOfflineCacheUpdateItem::GetInterface(const nsIID &aIID, void **aResult)
{
    if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
        NS_ADDREF_THIS();
        *aResult = static_cast<nsIChannelEventSink *>(this);
        return NS_OK;
    }

    return NS_ERROR_NO_INTERFACE;
}

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateItem::nsIChannelEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsOfflineCacheUpdateItem::OnChannelRedirect(nsIChannel *aOldChannel,
                                            nsIChannel *aNewChannel,
                                            PRUint32 aFlags)
{
    nsCOMPtr<nsIURI> newURI;
    nsresult rv = aNewChannel->GetURI(getter_AddRefs(newURI));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsICachingChannel> oldCachingChannel =
        do_QueryInterface(aOldChannel);
    nsCOMPtr<nsICachingChannel> newCachingChannel =
      do_QueryInterface(aOldChannel);
    if (newCachingChannel) {
        rv = newCachingChannel->SetCacheForOfflineUse(PR_TRUE);
        NS_ENSURE_SUCCESS(rv, rv);
        if (!mClientID.IsEmpty()) {
            rv = newCachingChannel->SetOfflineCacheClientID(mClientID);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }


    PRBool match;
    rv = newURI->SchemeIs("http", &match);
    if (NS_FAILED(rv) || !match) {
        if (NS_FAILED(newURI->SchemeIs("https", &match)) ||
            !match) {
            LOG(("rejected: URL is not of type http\n"));
            return NS_ERROR_ABORT;
        }
    }

    // HTTP request headers are not automatically forwarded to the new channel.
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aNewChannel);
    NS_ENSURE_STATE(httpChannel);

    httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("X-Moz"),
                                  NS_LITERAL_CSTRING("offline-resource"),
                                  PR_FALSE);

    mChannel = aNewChannel;

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateItem::nsIDOMLoadStatus
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsOfflineCacheUpdateItem::GetSource(nsIDOMNode **aSource)
{
    if (mSource) {
        return CallQueryReferent(mSource.get(), aSource);
    } else {
        *aSource = nsnull;
        return NS_OK;
    }
}

NS_IMETHODIMP
nsOfflineCacheUpdateItem::GetUri(nsAString &aURI)
{
    nsCAutoString spec;
    nsresult rv = mURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    CopyUTF8toUTF16(spec, aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdateItem::GetTotalSize(PRInt32 *aTotalSize)
{
    if (mChannel) {
        return mChannel->GetContentLength(aTotalSize);
    }

    *aTotalSize = -1;
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdateItem::GetLoadedSize(PRInt32 *aLoadedSize)
{
    *aLoadedSize = mBytesRead;
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdateItem::GetReadyState(PRUint16 *aReadyState)
{
    *aReadyState = mState;
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdateItem::GetStatus(PRUint16 *aStatus)
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
// nsOfflineCacheUpdate::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS1(nsOfflineCacheUpdate,
                   nsIOfflineCacheUpdate)

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdate <public>
//-----------------------------------------------------------------------------

nsOfflineCacheUpdate::nsOfflineCacheUpdate()
    : mState(STATE_UNINITIALIZED)
    , mAddedItems(PR_FALSE)
    , mPartialUpdate(PR_FALSE)
    , mSucceeded(PR_TRUE)
    , mCurrentItem(-1)
{
}

nsOfflineCacheUpdate::~nsOfflineCacheUpdate()
{
    LOG(("nsOfflineCacheUpdate::~nsOfflineCacheUpdate [%p]", this));
}

nsresult
nsOfflineCacheUpdate::Init(PRBool aPartialUpdate,
                           const nsACString &aUpdateDomain,
                           const nsACString &aOwnerURI,
                           nsIURI *aReferrerURI)
{
    nsresult rv;

    // Make sure the service has been initialized
    nsOfflineCacheUpdateService* service =
        nsOfflineCacheUpdateService::EnsureService();
    if (!service)
        return NS_ERROR_FAILURE;

    LOG(("nsOfflineCacheUpdate::Init [%p]", this));

    mPartialUpdate = aPartialUpdate;
    mUpdateDomain = aUpdateDomain;
    mOwnerURI = aOwnerURI;
    mReferrerURI = aReferrerURI;

    nsCOMPtr<nsICacheService> cacheService =
        do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsICacheSession> session;
    rv = cacheService->CreateSession("HTTP-offline",
                                     nsICache::STORE_OFFLINE,
                                     nsICache::STREAM_BASED,
                                     getter_AddRefs(session));
    NS_ENSURE_SUCCESS(rv, rv);

    mMainCacheSession = do_QueryInterface(session, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Partial updates don't use temporary cache sessions
    if (aPartialUpdate) {
        mCacheSession = mMainCacheSession;
    } else {
        rv = cacheService->CreateTemporaryClientID(nsICache::STORE_OFFLINE,
                                                   mClientID);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = cacheService->CreateSession(mClientID.get(),
                                         nsICache::STORE_OFFLINE,
                                         nsICache::STREAM_BASED,
                                         getter_AddRefs(session));
        NS_ENSURE_SUCCESS(rv, rv);

        mCacheSession = do_QueryInterface(session, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    mState = STATE_INITIALIZED;

    return NS_OK;
}

void
nsOfflineCacheUpdate::LoadCompleted()
{
    nsresult rv;

    LOG(("nsOfflineCacheUpdate::LoadCompleted [%p]", this));

    nsRefPtr<nsOfflineCacheUpdateItem> item = mItems[mCurrentItem];
    mCurrentItem++;

    PRUint16 status;
    rv = item->GetStatus(&status);

    // Check for failures.  Only connection or server errors (5XX) will cause
    // the update to fail.
    if (NS_FAILED(rv) || status == 0 || status >= 500) {
        // Only fail updates from this domain.  Outside-of-domain updates
        // are not guaranteeed to be updated.
        nsCAutoString domain;
        item->mURI->GetHostPort(domain);
        if (domain == mUpdateDomain) {
            mSucceeded = PR_FALSE;
        }
    }

    rv = NotifyCompleted(item);
    if (NS_FAILED(rv)) return;

    ProcessNextURI();
}

nsresult
nsOfflineCacheUpdate::Begin()
{
    LOG(("nsOfflineCacheUpdate::Begin [%p]", this));

    if (!mPartialUpdate) {
        // All offline items for a domain should be updated as a group;  add
        // the other offline items requested for this domain
        nsresult rv = AddDomainItems();
        NS_ENSURE_SUCCESS(rv, rv);
    }

    mState = STATE_RUNNING;

    mCurrentItem = 0;
    ProcessNextURI();

    return NS_OK;
}

nsresult
nsOfflineCacheUpdate::Cancel()
{
    LOG(("nsOfflineCacheUpdate::Cancel [%p]", this));

    mState = STATE_CANCELLED;
    mSucceeded = PR_FALSE;

    if (mCurrentItem >= 0 &&
        mCurrentItem < static_cast<PRInt32>(mItems.Length())) {
        // Load might be running
        mItems[mCurrentItem]->Cancel();
    }

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdate <private>
//-----------------------------------------------------------------------------

nsresult
nsOfflineCacheUpdate::AddOwnedItems(const nsACString &aOwnerURI)
{
    PRUint32 count;
    char **keys;
    nsresult rv = mMainCacheSession->GetOwnedKeys(mUpdateDomain, aOwnerURI,
                                                  &count, &keys);
    NS_ENSURE_SUCCESS(rv, rv);

    AutoFreeArray autoFree(count, keys);

    for (PRUint32 i = 0; i < count; i++) {
        nsCOMPtr<nsIURI> uri;
        if (NS_SUCCEEDED(NS_NewURI(getter_AddRefs(uri), keys[i]))) {
            nsRefPtr<nsOfflineCacheUpdateItem> item =
                new nsOfflineCacheUpdateItem(this, uri, mReferrerURI,
                                             nsnull, mClientID);
            if (!item) return NS_ERROR_OUT_OF_MEMORY;

            mItems.AppendElement(item);
        }
    }

    return NS_OK;
}

// Add all URIs needed by this domain to the update
nsresult
nsOfflineCacheUpdate::AddDomainItems()
{
    PRUint32 count;
    char **uris;
    nsresult rv = mMainCacheSession->GetOwnerURIs(mUpdateDomain, &count, &uris);
    NS_ENSURE_SUCCESS(rv, rv);

    AutoFreeArray autoFree(count, uris);

    for (PRUint32 i = 0; i < count; i++) {
        const char *ownerURI = uris[i];
        // if this update includes changes to this owner URI, ignore the
        // set in the database.
        if (!mAddedItems || !mOwnerURI.Equals(ownerURI)) {
            rv = AddOwnedItems(nsDependentCString(ownerURI));
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    return NS_OK;
}

nsresult
nsOfflineCacheUpdate::ProcessNextURI()
{
    LOG(("nsOfflineCacheUpdate::ProcessNextURI [%p, current=%d, numItems=%d]",
         this, mCurrentItem, mItems.Length()));

    if (mState == STATE_CANCELLED ||
        mCurrentItem >= static_cast<PRInt32>(mItems.Length())) {
        return Finish();
    }

#if defined(PR_LOGGING)
    if (LOG_ENABLED()) {
        nsCAutoString spec;
        mItems[mCurrentItem]->mURI->GetSpec(spec);
        LOG(("%p: Opening channel for %s", this, spec.get()));
    }
#endif

    nsresult rv = mItems[mCurrentItem]->OpenChannel();
    if (NS_FAILED(rv)) {
        LoadCompleted();
        return rv;
    }

    return NS_OK;
}

nsresult
nsOfflineCacheUpdate::NotifyCompleted(nsOfflineCacheUpdateItem *aItem)
{
    nsCOMArray<nsIOfflineCacheUpdateObserver> observers;

    for (PRInt32 i = 0; i < mWeakObservers.Count(); i++) {
        nsCOMPtr<nsIOfflineCacheUpdateObserver> observer =
            do_QueryReferent(mWeakObservers[i]);
        if (observer)
            observers.AppendObject(observer);
        else
            mWeakObservers.RemoveObjectAt(i--);
    }

    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        observers.AppendObject(mObservers[i]);
    }

    for (PRInt32 i = 0; i < observers.Count(); i++) {
        observers[i]->ItemCompleted(aItem);
    }

    return NS_OK;
}

nsresult
nsOfflineCacheUpdate::Finish()
{
    LOG(("nsOfflineCacheUpdate::Finish [%p]", this));

    mState = STATE_FINISHED;

    nsOfflineCacheUpdateService* service =
        nsOfflineCacheUpdateService::EnsureService();

    if (!mPartialUpdate) {
        if (mSucceeded) {
            nsresult rv = mMainCacheSession->MergeTemporaryClientID(mClientID);
            if (NS_FAILED(rv))
                mSucceeded = PR_FALSE;
        }

        if (!mSucceeded) {
            // Update was not merged, mark all the loads as failures
            for (PRUint32 i = 0; i < mItems.Length(); i++) {
                mItems[i]->Cancel();
            }
        }
    }

    if (!service)
        return NS_ERROR_FAILURE;

    return service->UpdateFinished(this);
}

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdate::nsIOfflineCacheUpdate
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsOfflineCacheUpdate::GetUpdateDomain(nsACString &aUpdateDomain)
{
    NS_ENSURE_TRUE(mState >= STATE_INITIALIZED, NS_ERROR_NOT_INITIALIZED);

    aUpdateDomain = mUpdateDomain;
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdate::GetOwnerURI(nsACString &aOwnerURI)
{
    NS_ENSURE_TRUE(mState >= STATE_INITIALIZED, NS_ERROR_NOT_INITIALIZED);

    aOwnerURI = mOwnerURI;
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdate::AddURI(nsIURI *aURI, nsIDOMNode *aSource)
{
    NS_ENSURE_TRUE(mState >= STATE_INITIALIZED, NS_ERROR_NOT_INITIALIZED);

    if (mState >= STATE_RUNNING)
        return NS_ERROR_NOT_AVAILABLE;

    // only http and https urls can be put in the offline cache
    PRBool match;
    nsresult rv = aURI->SchemeIs("http", &match);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!match) {
        rv = aURI->SchemeIs("https", &match);
        NS_ENSURE_SUCCESS(rv, rv);
        if (!match)
            return NS_ERROR_ABORT;
    }

    // Save the cache key as an owned URI
    nsCAutoString spec;
    rv = aURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    // url fragments aren't used in cache keys
    nsCAutoString::const_iterator specStart, specEnd;
    spec.BeginReading(specStart);
    spec.EndReading(specEnd);
    if (FindCharInReadable('#', specStart, specEnd)) {
        spec.BeginReading(specEnd);
        rv = mCacheSession->AddOwnedKey(mUpdateDomain, mOwnerURI,
                                        Substring(specEnd, specStart));
        NS_ENSURE_SUCCESS(rv, rv);
    } else {
        rv = mCacheSession->AddOwnedKey(mUpdateDomain, mOwnerURI, spec);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    nsRefPtr<nsOfflineCacheUpdateItem> item =
        new nsOfflineCacheUpdateItem(this, aURI, mReferrerURI,
                                     aSource, mClientID);
    if (!item) return NS_ERROR_OUT_OF_MEMORY;

    mItems.AppendElement(item);
    mAddedItems = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdate::GetCount(PRUint32 *aNumItems)
{
    LOG(("nsOfflineCacheUpdate::GetNumItems [%p, num=%d]",
         this, mItems.Length()));

    NS_ENSURE_TRUE(mState >= STATE_INITIALIZED, NS_ERROR_NOT_INITIALIZED);

    *aNumItems = mItems.Length();
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdate::Item(PRUint32 aIndex, nsIDOMLoadStatus **aItem)
{
    LOG(("nsOfflineCacheUpdate::GetItems [%p, index=%d]", this, aIndex));

    NS_ENSURE_TRUE(mState >= STATE_INITIALIZED, NS_ERROR_NOT_INITIALIZED);

    if (aIndex < mItems.Length())
        NS_IF_ADDREF(*aItem = mItems.ElementAt(aIndex));
    else
        *aItem = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdate::AddObserver(nsIOfflineCacheUpdateObserver *aObserver,
                                  PRBool aHoldWeak)
{
    LOG(("nsOfflineCacheUpdate::AddObserver [%p]", this));

    NS_ENSURE_TRUE(mState >= STATE_INITIALIZED, NS_ERROR_NOT_INITIALIZED);

    if (aHoldWeak) {
        nsCOMPtr<nsIWeakReference> weakRef = do_GetWeakReference(aObserver);
        mWeakObservers.AppendObject(weakRef);
    } else {
        mObservers.AppendObject(aObserver);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdate::RemoveObserver(nsIOfflineCacheUpdateObserver *aObserver)
{
    LOG(("nsOfflineCacheUpdate::RemoveObserver [%p]", this));

    NS_ENSURE_TRUE(mState >= STATE_INITIALIZED, NS_ERROR_NOT_INITIALIZED);

    for (PRInt32 i = 0; i < mWeakObservers.Count(); i++) {
        nsCOMPtr<nsIOfflineCacheUpdateObserver> observer =
            do_QueryReferent(mWeakObservers[i]);
        if (observer == aObserver) {
            mWeakObservers.RemoveObjectAt(i);
            return NS_OK;
        }
    }

    for (PRInt32 i = 0; i < mObservers.Count(); i++) {
        if (mObservers[i] == aObserver) {
            mObservers.RemoveObjectAt(i);
            return NS_OK;
        }
    }

    return NS_OK;
}


NS_IMETHODIMP
nsOfflineCacheUpdate::Schedule()
{
    LOG(("nsOfflineCacheUpdate::Schedule [%p]", this));

    nsOfflineCacheUpdateService* service =
        nsOfflineCacheUpdateService::EnsureService();

    if (!service) {
        return NS_ERROR_FAILURE;
    }

    return service->Schedule(this);
}

NS_IMETHODIMP
nsOfflineCacheUpdate::ScheduleOnDocumentStop(nsIDOMDocument *aDocument)
{
    LOG(("nsOfflineCacheUpdate::ScheduleOnDocumentStop [%p]", this));

    nsOfflineCacheUpdateService* service =
        nsOfflineCacheUpdateService::EnsureService();

    if (!service) {
        return NS_ERROR_FAILURE;
    }

    return service->ScheduleOnDocumentStop(this, aDocument);
}

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateService::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS4(nsOfflineCacheUpdateService,
                   nsIOfflineCacheUpdateService,
                   nsIWebProgressListener,
                   nsIObserver,
                   nsISupportsWeakReference)

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateService <public>
//-----------------------------------------------------------------------------

nsOfflineCacheUpdateService::nsOfflineCacheUpdateService()
    : mDisabled(PR_FALSE)
    , mUpdateRunning(PR_FALSE)
{
}

nsOfflineCacheUpdateService::~nsOfflineCacheUpdateService()
{
    gOfflineCacheUpdateService = nsnull;
}

nsresult
nsOfflineCacheUpdateService::Init()
{
    nsresult rv;

#if defined(PR_LOGGING)
    if (!gOfflineCacheUpdateLog)
        gOfflineCacheUpdateLog = PR_NewLogModule("nsOfflineCacheUpdate");
#endif

    if (!mDocUpdates.Init())
        return NS_ERROR_FAILURE;

    // Observe xpcom-shutdown event
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = observerService->AddObserver(this,
                                      NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                      PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    // Register as an observer for the document loader
    nsCOMPtr<nsIWebProgress> progress =
        do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID);
    if (progress) {
        nsresult rv = progress->AddProgressListener
                          (this, nsIWebProgress::NOTIFY_STATE_DOCUMENT);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    gOfflineCacheUpdateService = this;

    return NS_OK;
}

nsOfflineCacheUpdateService *
nsOfflineCacheUpdateService::GetInstance()
{
    if (!gOfflineCacheUpdateService) {
        gOfflineCacheUpdateService = new nsOfflineCacheUpdateService();
        if (!gOfflineCacheUpdateService)
            return nsnull;
        NS_ADDREF(gOfflineCacheUpdateService);
        nsresult rv = gOfflineCacheUpdateService->Init();
        if (NS_FAILED(rv)) {
            NS_RELEASE(gOfflineCacheUpdateService);
            return nsnull;
        }
        return gOfflineCacheUpdateService;
    }

    NS_ADDREF(gOfflineCacheUpdateService);

    return gOfflineCacheUpdateService;
}

nsOfflineCacheUpdateService *
nsOfflineCacheUpdateService::EnsureService()
{
    if (!gOfflineCacheUpdateService) {
        // Make the service manager hold a long-lived reference to the service
        nsCOMPtr<nsIOfflineCacheUpdateService> service =
            do_GetService(NS_OFFLINECACHEUPDATESERVICE_CONTRACTID);
    }

    return gOfflineCacheUpdateService;
}

nsresult
nsOfflineCacheUpdateService::Schedule(nsOfflineCacheUpdate *aUpdate)
{
    LOG(("nsOfflineCacheUpdateService::Schedule [%p, update=%p]",
         this, aUpdate));

    nsresult rv;
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    observerService->NotifyObservers(static_cast<nsIOfflineCacheUpdate*>(aUpdate),
                                     "offline-cache-update-added",
                                     nsnull);

    mUpdates.AppendElement(aUpdate);

    ProcessNextUpdate();

    return NS_OK;
}

nsresult
nsOfflineCacheUpdateService::ScheduleOnDocumentStop(nsOfflineCacheUpdate *aUpdate,
                                                    nsIDOMDocument *aDocument)
{
    LOG(("nsOfflineCacheUpdateService::ScheduleOnDocumentStop [%p, update=%p, doc=%p]",
         this, aUpdate, aDocument));

    if (!mDocUpdates.Put(aDocument, aUpdate))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

nsresult
nsOfflineCacheUpdateService::UpdateFinished(nsOfflineCacheUpdate *aUpdate)
{
    LOG(("nsOfflineCacheUpdateService::UpdateFinished [%p, update=%p]",
         this, aUpdate));

    NS_ASSERTION(mUpdates.Length() > 0 &&
                 mUpdates[0] == aUpdate, "Unknown update completed");

    // keep this item alive until we're done notifying observers
    nsRefPtr<nsOfflineCacheUpdate> update = mUpdates[0];
    mUpdates.RemoveElementAt(0);
    mUpdateRunning = PR_FALSE;

    nsresult rv;
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    observerService->NotifyObservers(static_cast<nsIOfflineCacheUpdate*>(aUpdate),
                                     "offline-cache-update-completed",
                                     nsnull);

    ProcessNextUpdate();

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateService <private>
//-----------------------------------------------------------------------------

nsresult
nsOfflineCacheUpdateService::ProcessNextUpdate()
{
    LOG(("nsOfflineCacheUpdateService::ProcessNextUpdate [%p, num=%d]",
         this, mUpdates.Length()));

    if (mDisabled)
        return NS_ERROR_ABORT;

    if (mUpdateRunning)
        return NS_OK;

    if (mUpdates.Length() > 0) {
        mUpdateRunning = PR_TRUE;
        return mUpdates[0]->Begin();
    }

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateService::nsIOfflineCacheUpdateService
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsOfflineCacheUpdateService::GetNumUpdates(PRUint32 *aNumUpdates)
{
    LOG(("nsOfflineCacheUpdateService::GetNumUpdates [%p]", this));

    *aNumUpdates = mUpdates.Length();
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdateService::GetUpdate(PRUint32 aIndex,
                                       nsIOfflineCacheUpdate **aUpdate)
{
    LOG(("nsOfflineCacheUpdateService::GetUpdate [%p, %d]", this, aIndex));

    if (aIndex < mUpdates.Length()) {
        NS_ADDREF(*aUpdate = mUpdates[aIndex]);
    } else {
        *aUpdate = nsnull;
    }

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateService::nsIObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsOfflineCacheUpdateService::Observe(nsISupports     *aSubject,
                                     const char      *aTopic,
                                     const PRUnichar *aData)
{
    if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
        if (mUpdates.Length() > 0)
            mUpdates[0]->Cancel();
        mDisabled = PR_TRUE;
    }

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateService::nsIWebProgressListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsOfflineCacheUpdateService::OnProgressChange(nsIWebProgress *aProgress,
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
nsOfflineCacheUpdateService::OnStateChange(nsIWebProgress* aWebProgress,
                                           nsIRequest *aRequest,
                                           PRUint32 progressStateFlags,
                                           nsresult aStatus)
{
    if ((progressStateFlags & STATE_IS_DOCUMENT) &&
        (progressStateFlags & STATE_STOP)) {
        if (mDocUpdates.Count() == 0)
            return NS_OK;

        nsCOMPtr<nsIDOMWindow> window;
        aWebProgress->GetDOMWindow(getter_AddRefs(window));
        if (!window) return NS_OK;

        nsCOMPtr<nsIDOMDocument> doc;
        window->GetDocument(getter_AddRefs(doc));
        if (!doc) return NS_OK;

        LOG(("nsOfflineCacheUpdateService::OnStateChange [%p, doc=%p]",
             this, doc.get()));

        nsRefPtr<nsOfflineCacheUpdate> update;
        if (mDocUpdates.Get(doc, getter_AddRefs(update))) {
            Schedule(update);
            mDocUpdates.Remove(doc);
        }

        return NS_OK;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdateService::OnLocationChange(nsIWebProgress* aWebProgress,
                                              nsIRequest* aRequest,
                                              nsIURI *location)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdateService::OnStatusChange(nsIWebProgress* aWebProgress,
                                            nsIRequest* aRequest,
                                            nsresult aStatus,
                                            const PRUnichar* aMessage)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdateService::OnSecurityChange(nsIWebProgress *aWebProgress,
                                              nsIRequest *aRequest,
                                              PRUint32 state)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}
