/* -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsICachingChannel.h"
#include "nsIContent.h"
#include "nsIDocShell.h"
#include "nsIDocumentLoader.h"
#include "nsIDOMWindow.h"
#include "nsIDOMOfflineResourceList.h"
#include "nsIDocument.h"
#include "nsIObserverService.h"
#include "nsIURL.h"
#include "nsIWebProgress.h"
#include "nsIWebNavigation.h"
#include "nsICryptoHash.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"
#include "mozilla/Logging.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "mozilla/Preferences.h"
#include "mozilla/Attributes.h"
#include "mozilla/Unused.h"
#include "nsIDiskSpaceWatcher.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "nsContentUtils.h"
#include "mozilla/Unused.h"

using namespace mozilla;
using namespace mozilla::dom;

static nsOfflineCacheUpdateService *gOfflineCacheUpdateService = nullptr;
static bool sAllowOfflineCache = true;
static bool sAllowInsecureOfflineCache = true;

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

//
// To enable logging (see mozilla/Logging.h for full details):
//
//    set MOZ_LOG=nsOfflineCacheUpdate:5
//    set MOZ_LOG_FILE=offlineupdate.log
//
// this enables LogLevel::Debug level information and places all output in
// the file offlineupdate.log
//
LazyLogModule gOfflineCacheUpdateLog("nsOfflineCacheUpdate");

#undef LOG
#define LOG(args) MOZ_LOG(gOfflineCacheUpdateLog, mozilla::LogLevel::Debug, args)

#undef LOG_ENABLED
#define LOG_ENABLED() MOZ_LOG_TEST(gOfflineCacheUpdateLog, mozilla::LogLevel::Debug)

//-----------------------------------------------------------------------------
// nsOfflineCachePendingUpdate
//-----------------------------------------------------------------------------

class nsOfflineCachePendingUpdate final : public nsIWebProgressListener
                                        , public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESSLISTENER

    nsOfflineCachePendingUpdate(nsOfflineCacheUpdateService *aService,
                                nsIURI *aManifestURI,
                                nsIURI *aDocumentURI,
                                nsIPrincipal* aLoadingPrincipal,
                                nsIDocument *aDocument)
        : mService(aService)
        , mManifestURI(aManifestURI)
        , mDocumentURI(aDocumentURI)
        , mLoadingPrincipal(aLoadingPrincipal)
        , mDidReleaseThis(false)
        {
            mDocument = do_GetWeakReference(aDocument);
        }

private:
    ~nsOfflineCachePendingUpdate() {}

    RefPtr<nsOfflineCacheUpdateService> mService;
    nsCOMPtr<nsIURI> mManifestURI;
    nsCOMPtr<nsIURI> mDocumentURI;
    nsCOMPtr<nsIPrincipal> mLoadingPrincipal;
    nsCOMPtr<nsIWeakReference> mDocument;
    bool mDidReleaseThis;
};

NS_IMPL_ISUPPORTS(nsOfflineCachePendingUpdate,
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
    MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
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
    nsCOMPtr<nsIDocument> updateDoc = do_QueryReferent(mDocument);
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

    nsCOMPtr<mozIDOMWindowProxy> windowProxy;
    aWebProgress->GetDOMWindow(getter_AddRefs(windowProxy));
    if (!windowProxy) return NS_OK;

    auto* outerWindow = nsPIDOMWindowOuter::From(windowProxy);
    nsPIDOMWindowInner* innerWindow = outerWindow->GetCurrentInnerWindow();

    nsCOMPtr<nsIDocument> progressDoc = outerWindow->GetDoc();
    if (!progressDoc) return NS_OK;

    if (!SameCOMIdentity(progressDoc, updateDoc)) {
        return NS_OK;
    }

    LOG(("nsOfflineCachePendingUpdate::OnStateChange [%p, doc=%p]",
         this, progressDoc.get()));

    // Only schedule the update if the document loaded successfully
    if (NS_SUCCEEDED(aStatus)) {
        nsCOMPtr<nsIOfflineCacheUpdate> update;
        mService->Schedule(mManifestURI, mDocumentURI, mLoadingPrincipal, updateDoc, innerWindow,
                           nullptr, getter_AddRefs(update));
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
    MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCachePendingUpdate::OnStatusChange(nsIWebProgress* aWebProgress,
                                            nsIRequest* aRequest,
                                            nsresult aStatus,
                                            const char16_t* aMessage)
{
    MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCachePendingUpdate::OnSecurityChange(nsIWebProgress *aWebProgress,
                                              nsIRequest *aRequest,
                                              uint32_t state)
{
    MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsOfflineCacheUpdateService::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsOfflineCacheUpdateService,
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
    MOZ_ASSERT(NS_IsMainThread());
    Preferences::AddBoolVarCache(&sAllowOfflineCache,
                                 "browser.cache.offline.enable",
                                 true);
    Preferences::AddBoolVarCache(&sAllowInsecureOfflineCache,
                                 "browser.cache.offline.insecure.enable",
                                 true);
}

nsOfflineCacheUpdateService::~nsOfflineCacheUpdateService()
{
    MOZ_ASSERT(gOfflineCacheUpdateService == this);
    gOfflineCacheUpdateService = nullptr;

    delete mAllowedDomains;
    mAllowedDomains = nullptr;
}

nsresult
nsOfflineCacheUpdateService::Init()
{
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
already_AddRefed<nsOfflineCacheUpdateService>
nsOfflineCacheUpdateService::GetInstance()
{
    if (!gOfflineCacheUpdateService) {
        auto serv = MakeRefPtr<nsOfflineCacheUpdateService>();
        if (NS_FAILED(serv->Init()))
            serv = nullptr;
        MOZ_ASSERT(gOfflineCacheUpdateService == serv.get());
        return serv.forget();
    }

    return do_AddRef(gOfflineCacheUpdateService);
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
                                                    nsIPrincipal* aLoadingPrincipal,
                                                    nsIDocument *aDocument)
{
    LOG(("nsOfflineCacheUpdateService::ScheduleOnDocumentStop [%p, manifestURI=%p, documentURI=%p doc=%p]",
         this, aManifestURI, aDocumentURI, aDocument));

    nsCOMPtr<nsIWebProgress> progress = do_QueryInterface(aDocument->GetContainer());
    NS_ENSURE_TRUE(progress, NS_ERROR_INVALID_ARG);

    // Proceed with cache update
    RefPtr<nsOfflineCachePendingUpdate> update =
        new nsOfflineCachePendingUpdate(this, aManifestURI, aDocumentURI,
                                        aLoadingPrincipal, aDocument);
    NS_ENSURE_TRUE(update, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = progress->AddProgressListener
        (update, nsIWebProgress::NOTIFY_STATE_DOCUMENT);
    NS_ENSURE_SUCCESS(rv, rv);

    // The update will release when it has scheduled itself.
    Unused << update.forget();

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
    RefPtr<nsOfflineCacheUpdate> update = mUpdates[0];
    Unused << update;
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
    LOG(("nsOfflineCacheUpdateService::ProcessNextUpdate [%p, num=%zu]",
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
                                        nsACString const &aOriginSuffix,
                                        nsIFile *aCustomProfileDir,
                                        nsOfflineCacheUpdate **aUpdate)
{
    nsresult rv;

    nsCOMPtr<nsIApplicationCacheService> cacheService =
        do_GetService(NS_APPLICATIONCACHESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString groupID;
    rv = cacheService->BuildGroupIDForSuffix(aManifestURI, aOriginSuffix, groupID);
    NS_ENSURE_SUCCESS(rv, rv);

    RefPtr<nsOfflineCacheUpdate> update;
    for (uint32_t i = 0; i < mUpdates.Length(); i++) {
        update = mUpdates[i];

        bool partial;
        rv = update->GetPartial(&partial);
        NS_ENSURE_SUCCESS(rv, rv);

        if (partial) {
            // Partial updates aren't considered
            continue;
        }

        if (update->IsForGroupID(groupID) && update->IsForProfile(aCustomProfileDir)) {
            update.swap(*aUpdate);
            return NS_OK;
        }
    }

    return NS_ERROR_NOT_AVAILABLE;
}

nsresult
nsOfflineCacheUpdateService::Schedule(nsIURI *aManifestURI,
                                      nsIURI *aDocumentURI,
                                      nsIPrincipal* aLoadingPrincipal,
                                      nsIDocument *aDocument,
                                      nsPIDOMWindowInner* aWindow,
                                      nsIFile* aCustomProfileDir,
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
      nsCOMPtr<nsIDOMOfflineResourceList> appCacheWindowObject =
          aWindow->GetApplicationCache();
    }

    rv = update->Init(aManifestURI, aDocumentURI, aLoadingPrincipal, aDocument,
                      aCustomProfileDir);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = update->Schedule();
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ADDREF(*aUpdate = update);

    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCacheUpdateService::ScheduleUpdate(nsIURI *aManifestURI,
                                            nsIURI *aDocumentURI,
                                            nsIPrincipal* aLoadingPrincipal,
                                            mozIDOMWindow* aWindow,
                                            nsIOfflineCacheUpdate **aUpdate)
{
    return Schedule(aManifestURI, aDocumentURI, aLoadingPrincipal, nullptr,
                    nsPIDOMWindowInner::From(aWindow), nullptr, aUpdate);
}

NS_IMETHODIMP
nsOfflineCacheUpdateService::ScheduleAppUpdate(nsIURI *aManifestURI,
                                               nsIURI *aDocumentURI,
                                               nsIPrincipal* aLoadingPrincipal,
                                               nsIFile *aProfileDir,
                                               nsIOfflineCacheUpdate **aUpdate)
{
    return Schedule(aManifestURI, aDocumentURI, aLoadingPrincipal, nullptr, nullptr,
                    aProfileDir, aUpdate);
}

NS_IMETHODIMP nsOfflineCacheUpdateService::CheckForUpdate(nsIURI *aManifestURI,
                                                          nsIPrincipal* aLoadingPrincipal,
                                                          nsIObserver *aObserver)
{
    if (GeckoProcessType_Default != XRE_GetProcessType()) {
        // Not intended to support this on child processes
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    nsCOMPtr<nsIOfflineCacheUpdate> update = new OfflineCacheUpdateGlue();

    nsresult rv;

    rv = update->InitForUpdateCheck(aManifestURI, aLoadingPrincipal, aObserver);
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

    if (!sAllowOfflineCache) {
        return NS_OK;
    }

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
    } else {
        if (!sAllowInsecureOfflineCache) {
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
        services::GetPermissionManager();
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
    OriginAttributes attrs;
    nsCOMPtr<nsIPrincipal> principal =
        BasePrincipal::CreateCodebasePrincipal(aURI, attrs);
    return OfflineAppPermForPrincipal(principal, aPrefBranch, false, aAllowed);
}

nsresult
nsOfflineCacheUpdateService::OfflineAppPinnedForURI(nsIURI *aDocumentURI,
                                                    nsIPrefBranch *aPrefBranch,
                                                    bool *aPinned)
{
    OriginAttributes attrs;
    nsCOMPtr<nsIPrincipal> principal =
        BasePrincipal::CreateCodebasePrincipal(aDocumentURI, attrs);
    return OfflineAppPermForPrincipal(principal, aPrefBranch, true, aPinned);
}

NS_IMETHODIMP
nsOfflineCacheUpdateService::AllowOfflineApp(nsIPrincipal *aPrincipal)
{
    nsresult rv;

    if (!sAllowOfflineCache) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    if (!sAllowInsecureOfflineCache) {
        nsCOMPtr<nsIURI> uri;
        aPrincipal->GetURI(getter_AddRefs(uri));

        if (!uri) {
            return NS_ERROR_NOT_AVAILABLE;
        }

        nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(uri);
        if (!innerURI) {
            return NS_ERROR_NOT_AVAILABLE;
        }

        // if http then we should prevent this cache
        bool match;
        rv = innerURI->SchemeIs("http", &match);
        NS_ENSURE_SUCCESS(rv, rv);

        if (match) {
            return NS_ERROR_NOT_AVAILABLE;
        }
    }

    if (GeckoProcessType_Default != XRE_GetProcessType()) {
        ContentChild* child = ContentChild::GetSingleton();

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
            services::GetPermissionManager();
        if (!permissionManager)
            return NS_ERROR_NOT_AVAILABLE;

        rv = permissionManager->AddFromPrincipal(
            aPrincipal, "offline-app", nsIPermissionManager::ALLOW_ACTION,
            nsIPermissionManager::EXPIRE_NEVER, 0);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}
