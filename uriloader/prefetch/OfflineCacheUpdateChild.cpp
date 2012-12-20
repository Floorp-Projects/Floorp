/* -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OfflineCacheUpdateChild.h"
#include "nsOfflineCacheUpdate.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/ipc/URIUtils.h"

#include "nsIApplicationCacheContainer.h"
#include "nsIApplicationCacheChannel.h"
#include "nsIApplicationCacheService.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDOMWindow.h"
#include "nsIDOMOfflineResourceList.h"
#include "nsIDocument.h"
#include "nsIObserverService.h"
#include "nsIURL.h"
#include "nsITabChild.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"
#include "prlog.h"
#include "nsIAsyncVerifyRedirectCallback.h"

using namespace mozilla::ipc;

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
extern PRLogModuleInfo *gOfflineCacheUpdateLog;
#endif
#undef LOG
#define LOG(args) PR_LOG(gOfflineCacheUpdateLog, 4, args)
#define LOG_ENABLED() PR_LOG_TEST(gOfflineCacheUpdateLog, 4)

namespace mozilla {
namespace docshell {

//-----------------------------------------------------------------------------
// OfflineCacheUpdateChild::nsISupports
//-----------------------------------------------------------------------------

NS_INTERFACE_MAP_BEGIN(OfflineCacheUpdateChild)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIOfflineCacheUpdate)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(OfflineCacheUpdateChild)
NS_IMPL_RELEASE_WITH_DESTROY(OfflineCacheUpdateChild, RefcountHitZero())

void
OfflineCacheUpdateChild::RefcountHitZero()
{
    if (mIPCActivated) {
        // ContentChild::DeallocPOfflineCacheUpdate will delete this
        OfflineCacheUpdateChild::Send__delete__(this);
    } else {
        delete this;    // we never opened IPDL channel
    }
}

//-----------------------------------------------------------------------------
// OfflineCacheUpdateChild <public>
//-----------------------------------------------------------------------------

OfflineCacheUpdateChild::OfflineCacheUpdateChild(nsIDOMWindow* aWindow)
    : mState(STATE_UNINITIALIZED)
    , mIsUpgrade(false)
    , mIPCActivated(false)
    , mAppID(NECKO_NO_APP_ID)
    , mInBrowser(false)
    , mWindow(aWindow)
    , mByteProgress(0)
{
}

OfflineCacheUpdateChild::~OfflineCacheUpdateChild()
{
    LOG(("OfflineCacheUpdateChild::~OfflineCacheUpdateChild [%p]", this));
}

void
OfflineCacheUpdateChild::GatherObservers(nsCOMArray<nsIOfflineCacheUpdateObserver> &aObservers)
{
    for (int32_t i = 0; i < mWeakObservers.Count(); i++) {
        nsCOMPtr<nsIOfflineCacheUpdateObserver> observer =
            do_QueryReferent(mWeakObservers[i]);
        if (observer)
            aObservers.AppendObject(observer);
        else
            mWeakObservers.RemoveObjectAt(i--);
    }

    for (int32_t i = 0; i < mObservers.Count(); i++) {
        aObservers.AppendObject(mObservers[i]);
    }
}

void
OfflineCacheUpdateChild::SetDocument(nsIDOMDocument *aDocument)
{
    // The design is one document for one cache update on the content process.
    NS_ASSERTION(!mDocument, "Setting more then a single document on a child offline cache update");

    LOG(("Document %p added to update child %p", aDocument, this));

    // Add document only if it was not loaded from an offline cache.
    // If it were loaded from an offline cache then it has already
    // been associated with it and must not be again cached as
    // implicit (which are the reasons we collect documents here).
    nsCOMPtr<nsIDocument> document = do_QueryInterface(aDocument);
    if (!document)
        return;

    nsIChannel* channel = document->GetChannel();
    nsCOMPtr<nsIApplicationCacheChannel> appCacheChannel =
        do_QueryInterface(channel);
    if (!appCacheChannel)
        return;

    bool loadedFromAppCache;
    appCacheChannel->GetLoadedFromApplicationCache(&loadedFromAppCache);
    if (loadedFromAppCache)
        return;

    mDocument = aDocument;
}

nsresult
OfflineCacheUpdateChild::AssociateDocument(nsIDOMDocument *aDocument,
                                        nsIApplicationCache *aApplicationCache)
{
    // Check that the document that requested this update was
    // previously associated with an application cache.  If not, it
    // should be associated with the new one.
    nsCOMPtr<nsIApplicationCacheContainer> container =
        do_QueryInterface(aDocument);
    if (!container)
        return NS_OK;

    nsCOMPtr<nsIApplicationCache> existingCache;
    nsresult rv = container->GetApplicationCache(getter_AddRefs(existingCache));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!existingCache) {
#if defined(PR_LOGGING)
        if (LOG_ENABLED()) {
            nsAutoCString clientID;
            if (aApplicationCache) {
                aApplicationCache->GetClientID(clientID);
            }
            LOG(("Update %p: associating app cache %s to document %p",
                 this, clientID.get(), aDocument));
        }
#endif

        rv = container->SetApplicationCache(aApplicationCache);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

//-----------------------------------------------------------------------------
// OfflineCacheUpdateChild::nsIOfflineCacheUpdate
//-----------------------------------------------------------------------------

NS_IMETHODIMP
OfflineCacheUpdateChild::Init(nsIURI *aManifestURI,
                              nsIURI *aDocumentURI,
                              nsIDOMDocument *aDocument,
                              nsIFile *aCustomProfileDir,
                              uint32_t aAppID,
                              bool aInBrowser)
{
    nsresult rv;

    // Make sure the service has been initialized
    nsOfflineCacheUpdateService* service =
        nsOfflineCacheUpdateService::EnsureService();
    if (!service)
        return NS_ERROR_FAILURE;

    if (aCustomProfileDir) {
        NS_ERROR("Custom Offline Cache Update not supported on child process");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    LOG(("OfflineCacheUpdateChild::Init [%p]", this));

    // Only http and https applications are supported.
    bool match;
    rv = aManifestURI->SchemeIs("http", &match);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!match) {
        rv = aManifestURI->SchemeIs("https", &match);
        NS_ENSURE_SUCCESS(rv, rv);
        if (!match)
            return NS_ERROR_ABORT;
    }

    mManifestURI = aManifestURI;

    rv = mManifestURI->GetAsciiHost(mUpdateDomain);
    NS_ENSURE_SUCCESS(rv, rv);

    mDocumentURI = aDocumentURI;

    mState = STATE_INITIALIZED;

    if (aDocument)
        SetDocument(aDocument);

    mAppID = aAppID;
    mInBrowser = aInBrowser;

    return NS_OK;
}

NS_IMETHODIMP
OfflineCacheUpdateChild::InitPartial(nsIURI *aManifestURI,
                                  const nsACString& clientID,
                                  nsIURI *aDocumentURI)
{
    NS_NOTREACHED("Not expected to do partial offline cache updates"
                  " on the child process");
    // For now leaving this method, we may discover we need it.
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OfflineCacheUpdateChild::InitForUpdateCheck(nsIURI *aManifestURI,
                                            uint32_t aAppID,
                                            bool aInBrowser,
                                            nsIObserver *aObserver)
{
    NS_NOTREACHED("Not expected to do only update checks"
                  " from the child process");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OfflineCacheUpdateChild::GetUpdateDomain(nsACString &aUpdateDomain)
{
    NS_ENSURE_TRUE(mState >= STATE_INITIALIZED, NS_ERROR_NOT_INITIALIZED);

    aUpdateDomain = mUpdateDomain;
    return NS_OK;
}

NS_IMETHODIMP
OfflineCacheUpdateChild::GetStatus(uint16_t *aStatus)
{
    switch (mState) {
    case STATE_CHECKING :
        *aStatus = nsIDOMOfflineResourceList::CHECKING;
        return NS_OK;
    case STATE_DOWNLOADING :
        *aStatus = nsIDOMOfflineResourceList::DOWNLOADING;
        return NS_OK;
    default :
        *aStatus = nsIDOMOfflineResourceList::IDLE;
        return NS_OK;
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
OfflineCacheUpdateChild::GetPartial(bool *aPartial)
{
    *aPartial = false;
    return NS_OK;
}

NS_IMETHODIMP
OfflineCacheUpdateChild::GetManifestURI(nsIURI **aManifestURI)
{
    NS_ENSURE_TRUE(mState >= STATE_INITIALIZED, NS_ERROR_NOT_INITIALIZED);

    NS_IF_ADDREF(*aManifestURI = mManifestURI);
    return NS_OK;
}

NS_IMETHODIMP
OfflineCacheUpdateChild::GetSucceeded(bool *aSucceeded)
{
    NS_ENSURE_TRUE(mState == STATE_FINISHED, NS_ERROR_NOT_AVAILABLE);

    *aSucceeded = mSucceeded;

    return NS_OK;
}

NS_IMETHODIMP
OfflineCacheUpdateChild::GetIsUpgrade(bool *aIsUpgrade)
{
    NS_ENSURE_TRUE(mState >= STATE_INITIALIZED, NS_ERROR_NOT_INITIALIZED);

    *aIsUpgrade = mIsUpgrade;

    return NS_OK;
}

NS_IMETHODIMP
OfflineCacheUpdateChild::AddDynamicURI(nsIURI *aURI)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OfflineCacheUpdateChild::Cancel()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OfflineCacheUpdateChild::AddObserver(nsIOfflineCacheUpdateObserver *aObserver,
                                  bool aHoldWeak)
{
    LOG(("OfflineCacheUpdateChild::AddObserver [%p]", this));

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
OfflineCacheUpdateChild::RemoveObserver(nsIOfflineCacheUpdateObserver *aObserver)
{
    LOG(("OfflineCacheUpdateChild::RemoveObserver [%p]", this));

    NS_ENSURE_TRUE(mState >= STATE_INITIALIZED, NS_ERROR_NOT_INITIALIZED);

    for (int32_t i = 0; i < mWeakObservers.Count(); i++) {
        nsCOMPtr<nsIOfflineCacheUpdateObserver> observer =
            do_QueryReferent(mWeakObservers[i]);
        if (observer == aObserver) {
            mWeakObservers.RemoveObjectAt(i);
            return NS_OK;
        }
    }

    for (int32_t i = 0; i < mObservers.Count(); i++) {
        if (mObservers[i] == aObserver) {
            mObservers.RemoveObjectAt(i);
            return NS_OK;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
OfflineCacheUpdateChild::GetByteProgress(uint64_t * _result)
{
    NS_ENSURE_ARG(_result);

    *_result = mByteProgress;
    return NS_OK;
}

NS_IMETHODIMP
OfflineCacheUpdateChild::Schedule()
{
    LOG(("OfflineCacheUpdateChild::Schedule [%p]", this));

    NS_ASSERTION(mWindow, "Window must be provided to the offline cache update child");

    nsCOMPtr<nsPIDOMWindow> piWindow = 
        do_QueryInterface(mWindow);
    mWindow = nullptr;

    nsIDocShell *docshell = piWindow->GetDocShell();

    nsCOMPtr<nsIDocShellTreeItem> item = do_QueryInterface(docshell);
    if (!item) {
      NS_WARNING("doc shell tree item is null");
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIDocShellTreeOwner> owner;
    item->GetTreeOwner(getter_AddRefs(owner));

    nsCOMPtr<nsITabChild> tabchild = do_GetInterface(owner);
    if (!tabchild) {
      NS_WARNING("tab is null");
      return NS_ERROR_FAILURE;
    }

    URIParams manifestURI, documentURI;
    SerializeURI(mManifestURI, manifestURI);
    SerializeURI(mDocumentURI, documentURI);

    // because owner implements nsITabChild, we can assume that it is
    // the one and only TabChild.
    mozilla::dom::TabChild* child = static_cast<mozilla::dom::TabChild*>(tabchild.get());
    
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (observerService) {
      LOG(("Calling offline-cache-update-added"));
      observerService->NotifyObservers(static_cast<nsIOfflineCacheUpdate*>(this),
                                       "offline-cache-update-added",
                                       nullptr);
      LOG(("Done offline-cache-update-added"));
    }

    // mDocument is non-null if both:
    // 1. this update was initiated by a document that referred a manifest
    // 2. the document has not already been loaded from the application cache
    // This tells the update to cache this document even in case the manifest
    // has not been changed since the last fetch.
    // See also nsOfflineCacheUpdate::ScheduleImplicit.
    bool stickDocument = mDocument != nullptr; 

    // Need to addref ourself here, because the IPC stack doesn't hold
    // a reference to us. Will be released in RecvFinish() that identifies 
    // the work has been done.
    child->SendPOfflineCacheUpdateConstructor(this, manifestURI, documentURI,
                                              mInBrowser, mAppID,
                                              stickDocument);

    mIPCActivated = true;
    this->AddRef();

    return NS_OK;
}

bool
OfflineCacheUpdateChild::RecvAssociateDocuments(const nsCString &cacheGroupId,
                                                  const nsCString &cacheClientId)
{
    LOG(("OfflineCacheUpdateChild::RecvAssociateDocuments [%p, cache=%s]", this, cacheClientId.get()));

    nsresult rv;

    nsCOMPtr<nsIApplicationCache> cache =
        do_CreateInstance(NS_APPLICATIONCACHE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
      return true;

    cache->InitAsHandle(cacheGroupId, cacheClientId);

    if (mDocument) {
        AssociateDocument(mDocument, cache);
    }

    nsCOMArray<nsIOfflineCacheUpdateObserver> observers;
    GatherObservers(observers);

    for (int32_t i = 0; i < observers.Count(); i++)
        observers[i]->ApplicationCacheAvailable(cache);

    return true;
}

bool
OfflineCacheUpdateChild::RecvNotifyStateEvent(const uint32_t &event,
                                              const uint64_t &byteProgress)
{
    LOG(("OfflineCacheUpdateChild::RecvNotifyStateEvent [%p]", this));

    mByteProgress = byteProgress;

    // Convert the public observer state to our internal state
    switch (event) {
        case nsIOfflineCacheUpdateObserver::STATE_CHECKING:
            mState = STATE_CHECKING;
            break;

        case nsIOfflineCacheUpdateObserver::STATE_DOWNLOADING:
            mState = STATE_DOWNLOADING;
            break;

        default:
            break;
    }

    nsCOMArray<nsIOfflineCacheUpdateObserver> observers;
    GatherObservers(observers);

    for (int32_t i = 0; i < observers.Count(); i++)
        observers[i]->UpdateStateChanged(this, event);

    return true;
}

bool
OfflineCacheUpdateChild::RecvFinish(const bool &succeeded,
                                    const bool &isUpgrade)
{
    LOG(("OfflineCacheUpdateChild::RecvFinish [%p]", this));

    nsRefPtr<OfflineCacheUpdateChild> kungFuDeathGrip(this);

    mState = STATE_FINISHED;
    mSucceeded = succeeded;
    mIsUpgrade = isUpgrade;

    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (observerService) {
        LOG(("Calling offline-cache-update-completed"));
        observerService->NotifyObservers(static_cast<nsIOfflineCacheUpdate*>(this),
                                         "offline-cache-update-completed",
                                         nullptr);
        LOG(("Done offline-cache-update-completed"));
    }

    // This is by contract the last notification from the parent, release
    // us now. This is corresponding to AddRef in Schedule().
    this->Release();

    return true;
}

}
}
