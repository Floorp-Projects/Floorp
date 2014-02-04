/* -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZ_LOGGING)
#define FORCE_PR_LOG
#endif

#include "OfflineCacheUpdateChild.h"
#include "OfflineCacheUpdateParent.h"
#include "nsXULAppAPI.h"
#include "OfflineCacheUpdateGlue.h"
#include "nsOfflineCacheUpdate.h"

#include "nsCPrefetchService.h"
#include "nsCURILoader.h"
#include "nsIApplicationCacheContainer.h"
#include "nsIApplicationCacheChannel.h"
#include "nsIApplicationCacheService.h"
#include "nsICache.h"
#include "nsICacheService.h"
#include "nsICacheSession.h"
#include "nsICachingChannel.h"
#include "nsIContent.h"
#include "nsIDocShell.h"
#include "nsIDocumentLoader.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsIDOMOfflineResourceList.h"
#include "nsIDocument.h"
#include "nsIObserverService.h"
#include "nsIURL.h"
#include "nsIWebProgress.h"
#include "nsIWebNavigation.h"
#include "nsICryptoHash.h"
#include "nsICacheEntryDescriptor.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"
#include "prlog.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "mozilla/Preferences.h"
#include "mozilla/Attributes.h"
#include "nsIDiskSpaceWatcher.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

static nsOfflineCacheUpdateService *gOfflineCacheUpdateService = nullptr;

nsTHashtable<nsCStringHashKey>* nsOfflineCacheUpdateService::mAllowedDomains = nullptr;

nsTHashtable<nsCStringHashKey>* nsOfflineCacheUpdateService::AllowedDomains()
{
    if (!mAllowedDomains)
        mAllowedDomains = new nsTHashtable<nsCStringHashKey>();

    return mAllowedDomains;
}


typedef mozilla::docshell::OfflineCacheUpdateParent OfflineCacheUpdateParent;
typedef mozilla::docshell::OfflineCacheUpdateChild OfflineCacheUpdateChild;
typedef mozilla::docshell::OfflineCacheUpdateGlue OfflineCacheUpdateGlue;

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
PRLogModuleInfo *gOfflineCacheUpdateLog;
#endif

#undef LOG
#define LOG(args) PR_LOG(gOfflineCacheUpdateLog, 4, args)

#undef LOG_ENABLED
#define LOG_ENABLED() PR_LOG_TEST(gOfflineCacheUpdateLog, 4)

namespace { // anon

nsresult
GetAppIDAndInBrowserFromWindow(nsIDOMWindow *aWindow,
                               uint32_t *aAppId,
                               bool *aInBrowser)
{
    *aAppId = NECKO_NO_APP_ID;
    *aInBrowser = false;

    if (!aWindow) {
        return NS_OK;
    }

    nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(aWindow);
    if (!loadContext) {
        return NS_OK;
    }

    nsresult rv;

    rv = loadContext->GetAppId(aAppId);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = loadContext->GetIsInBrowserElement(aInBrowser);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

} // anon

//-----------------------------------------------------------------------------
// nsOfflineCachePendingUpdate
//-----------------------------------------------------------------------------

class nsOfflineCachePendingUpdate MOZ_FINAL : public nsIWebProgressListener
                                            , public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESSLISTENER

    nsOfflineCachePendingUpdate(nsOfflineCacheUpdateService *aService,
                                nsIURI *aManifestURI,
                                nsIURI *aDocumentURI,
                                nsIDOMDocument *aDocument)
        : mService(aService)
        , mManifestURI(aManifestURI)
        , mDocumentURI(aDocumentURI)
        , mDidReleaseThis(false)
        {
            mDocument = do_GetWeakReference(aDocument);
        }

private:
    nsRefPtr<nsOfflineCacheUpdateService> mService;
    nsCOMPtr<nsIURI> mManifestURI;
    nsCOMPtr<nsIURI> mDocumentURI;
    nsCOMPtr<nsIWeakReference> mDocument;
    bool mDidReleaseThis;
};

NS_IMPL_ISUPPORTS2(nsOfflineCachePendingUpdate,
                   nsIWebProgressListener,
                   nsISupportsWeakReference)

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateService::nsIWebProgressListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsOfflineCachePendingUpdate::OnProgressChange(nsIWebProgress *aProgress,
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
nsOfflineCachePendingUpdate::OnStateChange(nsIWebProgress* aWebProgress,
                                           nsIRequest *aRequest,
                                           uint32_t progressStateFlags,
                                           nsresult aStatus)
{
    if (mDidReleaseThis) {
        return NS_OK;
    }
    nsCOMPtr<nsIDOMDocument> updateDoc = do_QueryReferent(mDocument);
    if (!updateDoc) {
        // The document that scheduled this update has gone away,
        // we don't need to listen anymore.
        aWebProgress->RemoveProgressListener(this);
        MOZ_ASSERT(!mDidReleaseThis);
        mDidReleaseThis = true;
        NS_RELEASE_THIS();
        return NS_OK;
    }

    if (!(progressStateFlags & STATE_STOP)) {
        return NS_OK;
    }

    nsCOMPtr<nsIDOMWindow> window;
    aWebProgress->GetDOMWindow(getter_AddRefs(window));
    if (!window) return NS_OK;

    nsCOMPtr<nsIDOMDocument> progressDoc;
    window->GetDocument(getter_AddRefs(progressDoc));
    if (!progressDoc) return NS_OK;

    if (!SameCOMIdentity(progressDoc, updateDoc)) {
        return NS_OK;
    }

    LOG(("nsOfflineCachePendingUpdate::OnStateChange [%p, doc=%p]",
         this, progressDoc.get()));

    // Only schedule the update if the document loaded successfully
    if (NS_SUCCEEDED(aStatus)) {
        // Get extended origin attributes
        uint32_t appId;
        bool isInBrowserElement;
        nsresult rv = GetAppIDAndInBrowserFromWindow(window, &appId, &isInBrowserElement);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIOfflineCacheUpdate> update;
        mService->Schedule(mManifestURI, mDocumentURI,
                           updateDoc, window, nullptr,
                           appId, isInBrowserElement, getter_AddRefs(update));
        if (mDidReleaseThis) {
            return NS_OK;
        }
    }

    aWebProgress->RemoveProgressListener(this);
    MOZ_ASSERT(!mDidReleaseThis);
    mDidReleaseThis = true;
    NS_RELEASE_THIS();

    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCachePendingUpdate::OnLocationChange(nsIWebProgress* aWebProgress,
                                              nsIRequest* aRequest,
                                              nsIURI *location,
                                              uint32_t aFlags)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCachePendingUpdate::OnStatusChange(nsIWebProgress* aWebProgress,
                                            nsIRequest* aRequest,
                                            nsresult aStatus,
                                            const char16_t* aMessage)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCachePendingUpdate::OnSecurityChange(nsIWebProgress *aWebProgress,
                                              nsIRequest *aRequest,
                                              uint32_t state)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateService::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS3(nsOfflineCacheUpdateService,
                   nsIOfflineCacheUpdateService,
                   nsIObserver,
                   nsISupportsWeakReference)

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateService <public>
//-----------------------------------------------------------------------------

nsOfflineCacheUpdateService::nsOfflineCacheUpdateService()
    : mDisabled(false)
    , mUpdateRunning(false)
    , mLowFreeSpace(false)
{
}

nsOfflineCacheUpdateService::~nsOfflineCacheUpdateService()
{
    gOfflineCacheUpdateService = nullptr;
}

nsresult
nsOfflineCacheUpdateService::Init()
{
#if defined(PR_LOGGING)
    if (!gOfflineCacheUpdateLog)
        gOfflineCacheUpdateLog = PR_NewLogModule("nsOfflineCacheUpdate");
#endif

    // Observe xpcom-shutdown event
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (!observerService)
      return NS_ERROR_FAILURE;

    nsresult rv = observerService->AddObserver(this,
                                               NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                               true);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the current status of the disk in terms of free space and observe
    // low device storage notifications.
    nsCOMPtr<nsIDiskSpaceWatcher> diskSpaceWatcherService =
      do_GetService("@mozilla.org/toolkit/disk-space-watcher;1");
    if (diskSpaceWatcherService) {
      diskSpaceWatcherService->GetIsDiskFull(&mLowFreeSpace);
    } else {
      NS_WARNING("Could not get disk status from nsIDiskSpaceWatcher");
    }

    rv = observerService->AddObserver(this, "disk-space-watcher", false);
    NS_ENSURE_SUCCESS(rv, rv);

    gOfflineCacheUpdateService = this;

    return NS_OK;
}

/* static */
nsOfflineCacheUpdateService *
nsOfflineCacheUpdateService::GetInstance()
{
    if (!gOfflineCacheUpdateService) {
        gOfflineCacheUpdateService = new nsOfflineCacheUpdateService();
        if (!gOfflineCacheUpdateService)
            return nullptr;
        NS_ADDREF(gOfflineCacheUpdateService);
        nsresult rv = gOfflineCacheUpdateService->Init();
        if (NS_FAILED(rv)) {
            NS_RELEASE(gOfflineCacheUpdateService);
            return nullptr;
        }
        return gOfflineCacheUpdateService;
    }

    NS_ADDREF(gOfflineCacheUpdateService);

    return gOfflineCacheUpdateService;
}

/* static */
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
nsOfflineCacheUpdateService::ScheduleUpdate(nsOfflineCacheUpdate *aUpdate)
{
    LOG(("nsOfflineCacheUpdateService::Schedule [%p, update=%p]",
         this, aUpdate));

    aUpdate->SetOwner(this);

    mUpdates.AppendElement(aUpdate);
    ProcessNextUpdate();

    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdateService::ScheduleOnDocumentStop(nsIURI *aManifestURI,
                                                    nsIURI *aDocumentURI,
                                                    nsIDOMDocument *aDocument)
{
    LOG(("nsOfflineCacheUpdateService::ScheduleOnDocumentStop [%p, manifestURI=%p, documentURI=%p doc=%p]",
         this, aManifestURI, aDocumentURI, aDocument));

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDocument);
    nsCOMPtr<nsIWebProgress> progress = do_QueryInterface(doc->GetContainer());
    NS_ENSURE_TRUE(progress, NS_ERROR_INVALID_ARG);

    // Proceed with cache update
    nsRefPtr<nsOfflineCachePendingUpdate> update =
        new nsOfflineCachePendingUpdate(this, aManifestURI,
                                        aDocumentURI, aDocument);
    NS_ENSURE_TRUE(update, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = progress->AddProgressListener
        (update, nsIWebProgress::NOTIFY_STATE_DOCUMENT);
    NS_ENSURE_SUCCESS(rv, rv);

    // The update will release when it has scheduled itself.
    update.forget();

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
    mUpdateRunning = false;

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
        mUpdateRunning = true;
        // Canceling the update before Begin() call will make the update
        // asynchronously finish with an error.
        if (mLowFreeSpace) {
            mUpdates[0]->Cancel();
        }
        return mUpdates[0]->Begin();
    }

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateService::nsIOfflineCacheUpdateService
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsOfflineCacheUpdateService::GetNumUpdates(uint32_t *aNumUpdates)
{
    LOG(("nsOfflineCacheUpdateService::GetNumUpdates [%p]", this));

    *aNumUpdates = mUpdates.Length();
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdateService::GetUpdate(uint32_t aIndex,
                                       nsIOfflineCacheUpdate **aUpdate)
{
    LOG(("nsOfflineCacheUpdateService::GetUpdate [%p, %d]", this, aIndex));

    if (aIndex < mUpdates.Length()) {
        NS_ADDREF(*aUpdate = mUpdates[aIndex]);
    } else {
        *aUpdate = nullptr;
    }

    return NS_OK;
}

nsresult
nsOfflineCacheUpdateService::FindUpdate(nsIURI *aManifestURI,
                                        uint32_t aAppID,
                                        bool aInBrowser,
                                        nsOfflineCacheUpdate **aUpdate)
{
    nsresult rv;

    nsCOMPtr<nsIApplicationCacheService> cacheService =
        do_GetService(NS_APPLICATIONCACHESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString groupID;
    rv = cacheService->BuildGroupIDForApp(aManifestURI,
                                          aAppID, aInBrowser,
                                          groupID);
    NS_ENSURE_SUCCESS(rv, rv);

    nsRefPtr<nsOfflineCacheUpdate> update;
    for (uint32_t i = 0; i < mUpdates.Length(); i++) {
        update = mUpdates[i];

        bool partial;
        rv = update->GetPartial(&partial);
        NS_ENSURE_SUCCESS(rv, rv);

        if (partial) {
            // Partial updates aren't considered
            continue;
        }

        if (update->IsForGroupID(groupID)) {
            update.swap(*aUpdate);
            return NS_OK;
        }
    }

    return NS_ERROR_NOT_AVAILABLE;
}

nsresult
nsOfflineCacheUpdateService::Schedule(nsIURI *aManifestURI,
                                      nsIURI *aDocumentURI,
                                      nsIDOMDocument *aDocument,
                                      nsIDOMWindow* aWindow,
                                      nsIFile* aCustomProfileDir,
                                      uint32_t aAppID,
                                      bool aInBrowser,
                                      nsIOfflineCacheUpdate **aUpdate)
{
    nsCOMPtr<nsIOfflineCacheUpdate> update;
    if (GeckoProcessType_Default != XRE_GetProcessType()) {
        update = new OfflineCacheUpdateChild(aWindow);
    }
    else {
        update = new OfflineCacheUpdateGlue();
    }

    nsresult rv;

    if (aWindow) {
      // Ensure there is window.applicationCache object that is
      // responsible for association of the new applicationCache
      // with the corresponding document.  Just ignore the result.
      nsCOMPtr<nsIDOMOfflineResourceList> appCacheWindowObject;
      aWindow->GetApplicationCache(getter_AddRefs(appCacheWindowObject));
    }

    rv = update->Init(aManifestURI, aDocumentURI, aDocument,
                      aCustomProfileDir, aAppID, aInBrowser);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = update->Schedule();
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ADDREF(*aUpdate = update);

    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdateService::ScheduleUpdate(nsIURI *aManifestURI,
                                            nsIURI *aDocumentURI,
                                            nsIDOMWindow *aWindow,
                                            nsIOfflineCacheUpdate **aUpdate)
{
    // Get extended origin attributes
    uint32_t appId;
    bool isInBrowser;
    nsresult rv = GetAppIDAndInBrowserFromWindow(aWindow, &appId, &isInBrowser);
    NS_ENSURE_SUCCESS(rv, rv);

    return Schedule(aManifestURI, aDocumentURI, nullptr, aWindow, nullptr,
                    appId, isInBrowser, aUpdate);
}

NS_IMETHODIMP
nsOfflineCacheUpdateService::ScheduleAppUpdate(nsIURI *aManifestURI,
                                               nsIURI *aDocumentURI,
                                               uint32_t aAppID, bool aInBrowser,
                                               nsIFile *aProfileDir,
                                               nsIOfflineCacheUpdate **aUpdate)
{
    return Schedule(aManifestURI, aDocumentURI, nullptr, nullptr, aProfileDir,
                    aAppID, aInBrowser, aUpdate);
}

NS_IMETHODIMP nsOfflineCacheUpdateService::CheckForUpdate(nsIURI *aManifestURI,
                                                          uint32_t aAppID,
                                                          bool aInBrowser,
                                                          nsIObserver *aObserver)
{
    if (GeckoProcessType_Default != XRE_GetProcessType()) {
        // Not intended to support this on child processes
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    nsCOMPtr<nsIOfflineCacheUpdate> update = new OfflineCacheUpdateGlue();

    nsresult rv;

    rv = update->InitForUpdateCheck(aManifestURI, aAppID, aInBrowser, aObserver);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = update->Schedule();
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateService::nsIObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsOfflineCacheUpdateService::Observe(nsISupports     *aSubject,
                                     const char      *aTopic,
                                     const char16_t *aData)
{
    if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
        if (mUpdates.Length() > 0)
            mUpdates[0]->Cancel();
        mDisabled = true;
    }

    if (!strcmp(aTopic, "disk-space-watcher")) {
        if (NS_LITERAL_STRING("full").Equals(aData)) {
            mLowFreeSpace = true;
            for (uint32_t i = 0; i < mUpdates.Length(); i++) {
                mUpdates[i]->Cancel();
            }
        } else if (NS_LITERAL_STRING("free").Equals(aData)) {
            mLowFreeSpace = false;
        }
    }

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateService::nsIOfflineCacheUpdateService
//-----------------------------------------------------------------------------

static nsresult
OfflineAppPermForPrincipal(nsIPrincipal *aPrincipal,
                           nsIPrefBranch *aPrefBranch,
                           bool pinned,
                           bool *aAllowed)
{
    *aAllowed = false;

    if (!aPrincipal)
        return NS_ERROR_INVALID_ARG;

    nsCOMPtr<nsIURI> uri;
    aPrincipal->GetURI(getter_AddRefs(uri));

    if (!uri)
        return NS_OK;

    nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(uri);
    if (!innerURI)
        return NS_OK;

    // only http and https applications can use offline APIs.
    bool match;
    nsresult rv = innerURI->SchemeIs("http", &match);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!match) {
        rv = innerURI->SchemeIs("https", &match);
        NS_ENSURE_SUCCESS(rv, rv);
        if (!match) {
            return NS_OK;
        }
    }

    nsAutoCString domain;
    rv = innerURI->GetAsciiHost(domain);
    NS_ENSURE_SUCCESS(rv, rv);

    if (nsOfflineCacheUpdateService::AllowedDomains()->Contains(domain)) {
        *aAllowed = true;
        return NS_OK;
    }

    nsCOMPtr<nsIPermissionManager> permissionManager =
        do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
    if (!permissionManager) {
        return NS_OK;
    }

    uint32_t perm;
    const char *permName = pinned ? "pin-app" : "offline-app";
    permissionManager->TestExactPermissionFromPrincipal(aPrincipal, permName, &perm);

    if (perm == nsIPermissionManager::ALLOW_ACTION ||
        perm == nsIOfflineCacheUpdateService::ALLOW_NO_WARN) {
        *aAllowed = true;
    }

    // offline-apps.allow_by_default is now effective at the cache selection
    // algorithm code (nsContentSink).

    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdateService::OfflineAppAllowed(nsIPrincipal *aPrincipal,
                                               nsIPrefBranch *aPrefBranch,
                                               bool *aAllowed)
{
    return OfflineAppPermForPrincipal(aPrincipal, aPrefBranch, false, aAllowed);
}

NS_IMETHODIMP
nsOfflineCacheUpdateService::OfflineAppAllowedForURI(nsIURI *aURI,
                                                     nsIPrefBranch *aPrefBranch,
                                                     bool *aAllowed)
{
    nsCOMPtr<nsIPrincipal> principal;
    nsContentUtils::GetSecurityManager()->
        GetNoAppCodebasePrincipal(aURI, getter_AddRefs(principal));
    return OfflineAppPermForPrincipal(principal, aPrefBranch, false, aAllowed);
}

nsresult
nsOfflineCacheUpdateService::OfflineAppPinnedForURI(nsIURI *aDocumentURI,
                                                    nsIPrefBranch *aPrefBranch,
                                                    bool *aPinned)
{
    nsCOMPtr<nsIPrincipal> principal;
    nsContentUtils::GetSecurityManager()->
        GetNoAppCodebasePrincipal(aDocumentURI, getter_AddRefs(principal));
    return OfflineAppPermForPrincipal(principal, aPrefBranch, true, aPinned);
}

NS_IMETHODIMP
nsOfflineCacheUpdateService::AllowOfflineApp(nsIDOMWindow *aWindow,
                                             nsIPrincipal *aPrincipal)
{
    nsresult rv;

    if (GeckoProcessType_Default != XRE_GetProcessType()) {
        TabChild* child = TabChild::GetFrom(aWindow);
        NS_ENSURE_TRUE(child, NS_ERROR_FAILURE);

        if (!child->SendSetOfflinePermission(IPC::Principal(aPrincipal))) {
            return NS_ERROR_FAILURE;
        }

        nsAutoCString domain;
        rv = aPrincipal->GetBaseDomain(domain);
        NS_ENSURE_SUCCESS(rv, rv);

        nsOfflineCacheUpdateService::AllowedDomains()->PutEntry(domain);
    }
    else {
        nsCOMPtr<nsIPermissionManager> permissionManager =
            do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
        if (!permissionManager)
            return NS_ERROR_NOT_AVAILABLE;

        rv = permissionManager->AddFromPrincipal(
            aPrincipal, "offline-app", nsIPermissionManager::ALLOW_ACTION,
            nsIPermissionManager::EXPIRE_NEVER, 0);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}
