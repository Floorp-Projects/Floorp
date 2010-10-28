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

#ifdef MOZ_IPC
#include "OfflineCacheUpdateChild.h"
#include "OfflineCacheUpdateParent.h"
#include "nsXULAppAPI.h"
#endif
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
#include "nsIDocumentLoader.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsIDOMOfflineResourceList.h"
#include "nsIDocument.h"
#include "nsIObserverService.h"
#include "nsIURL.h"
#include "nsIWebProgress.h"
#include "nsICryptoHash.h"
#include "nsICacheEntryDescriptor.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"
#include "prlog.h"
#include "nsIAsyncVerifyRedirectCallback.h"

static nsOfflineCacheUpdateService *gOfflineCacheUpdateService = nsnull;

#ifdef MOZ_IPC
typedef mozilla::docshell::OfflineCacheUpdateParent OfflineCacheUpdateParent;
typedef mozilla::docshell::OfflineCacheUpdateChild OfflineCacheUpdateChild;
#endif
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

static nsresult
DropReferenceFromURL(nsIURI * aURI)
{
    nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);
    if (url) {
        nsresult rv = url->SetRef(EmptyCString());
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsOfflineCachePendingUpdate
//-----------------------------------------------------------------------------

class nsOfflineCachePendingUpdate : public nsIWebProgressListener
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
        {
            mDocument = do_GetWeakReference(aDocument);
        }

private:
    nsRefPtr<nsOfflineCacheUpdateService> mService;
    nsCOMPtr<nsIURI> mManifestURI;
    nsCOMPtr<nsIURI> mDocumentURI;
    nsCOMPtr<nsIWeakReference> mDocument;
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
                                              PRInt32 curSelfProgress,
                                              PRInt32 maxSelfProgress,
                                              PRInt32 curTotalProgress,
                                              PRInt32 maxTotalProgress)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCachePendingUpdate::OnStateChange(nsIWebProgress* aWebProgress,
                                           nsIRequest *aRequest,
                                           PRUint32 progressStateFlags,
                                           nsresult aStatus)
{
    nsCOMPtr<nsIDOMDocument> updateDoc = do_QueryReferent(mDocument);
    if (!updateDoc) {
        // The document that scheduled this update has gone away,
        // we don't need to listen anymore.
        aWebProgress->RemoveProgressListener(this);
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
        nsCOMPtr<nsIOfflineCacheUpdate> update;
        mService->Schedule(mManifestURI, mDocumentURI,
                           updateDoc, window, getter_AddRefs(update));
    }

    aWebProgress->RemoveProgressListener(this);
    NS_RELEASE_THIS();

    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCachePendingUpdate::OnLocationChange(nsIWebProgress* aWebProgress,
                                              nsIRequest* aRequest,
                                              nsIURI *location)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCachePendingUpdate::OnStatusChange(nsIWebProgress* aWebProgress,
                                            nsIRequest* aRequest,
                                            nsresult aStatus,
                                            const PRUnichar* aMessage)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

NS_IMETHODIMP
nsOfflineCachePendingUpdate::OnSecurityChange(nsIWebProgress *aWebProgress,
                                              nsIRequest *aRequest,
                                              PRUint32 state)
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
                                               PR_TRUE);
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
    nsCOMPtr<nsISupports> container = doc->GetContainer();
    nsCOMPtr<nsIWebProgress> progress = do_QueryInterface(container);
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
    mUpdateRunning = PR_FALSE;

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

nsresult
nsOfflineCacheUpdateService::FindUpdate(nsIURI *aManifestURI,
                                        nsIURI *aDocumentURI,
                                        nsOfflineCacheUpdate **aUpdate)
{
    nsresult rv;

    nsRefPtr<nsOfflineCacheUpdate> update;
    for (PRUint32 i = 0; i < mUpdates.Length(); i++) {
        update = mUpdates[i];

        PRBool partial;
        rv = update->GetPartial(&partial);
        NS_ENSURE_SUCCESS(rv, rv);

        if (partial) {
            // Partial updates aren't considered
            continue;
        }

        nsCOMPtr<nsIURI> manifestURI;
        update->GetManifestURI(getter_AddRefs(manifestURI));
        if (manifestURI) {
            PRBool equals;
            rv = manifestURI->Equals(aManifestURI, &equals);
            if (equals) {
                update.swap(*aUpdate);
                return NS_OK;
            }
        }
    }

    return NS_ERROR_NOT_AVAILABLE;
}

nsresult
nsOfflineCacheUpdateService::Schedule(nsIURI *aManifestURI,
                                      nsIURI *aDocumentURI,
                                      nsIDOMDocument *aDocument,
                                      nsIDOMWindow* aWindow,
                                      nsIOfflineCacheUpdate **aUpdate)
{
    nsCOMPtr<nsIOfflineCacheUpdate> update;
#ifdef MOZ_IPC
    if (GeckoProcessType_Default != XRE_GetProcessType()) {
        update = new OfflineCacheUpdateChild(aWindow);
    }
    else
#endif
    {
        update = new OfflineCacheUpdateGlue();
    }

    nsresult rv;

    rv = update->Init(aManifestURI, aDocumentURI, aDocument);
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
    return Schedule(aManifestURI, aDocumentURI, nsnull, aWindow, aUpdate);
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
// nsOfflineCacheUpdateService::nsIOfflineCacheUpdateService
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsOfflineCacheUpdateService::OfflineAppAllowed(nsIPrincipal *aPrincipal,
                                               nsIPrefBranch *aPrefBranch,
                                               PRBool *aAllowed)
{
    nsCOMPtr<nsIURI> codebaseURI;
    nsresult rv = aPrincipal->GetURI(getter_AddRefs(codebaseURI));
    NS_ENSURE_SUCCESS(rv, rv);

    return OfflineAppAllowedForURI(codebaseURI, aPrefBranch, aAllowed);
}

NS_IMETHODIMP
nsOfflineCacheUpdateService::OfflineAppAllowedForURI(nsIURI *aURI,
                                                     nsIPrefBranch *aPrefBranch,
                                                     PRBool *aAllowed)
{
    *aAllowed = PR_FALSE;
    if (!aURI)
        return NS_OK;

    nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(aURI);
    if (!innerURI)
        return NS_OK;

    // only http and https applications can use offline APIs.
    PRBool match;
    nsresult rv = innerURI->SchemeIs("http", &match);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!match) {
        rv = innerURI->SchemeIs("https", &match);
        NS_ENSURE_SUCCESS(rv, rv);
        if (!match) {
            return NS_OK;
        }
    }

    nsCOMPtr<nsIPermissionManager> permissionManager =
        do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
    if (!permissionManager) {
        return NS_OK;
    }

    PRUint32 perm;
    permissionManager->TestExactPermission(innerURI, "offline-app", &perm);

    if (perm == nsIPermissionManager::UNKNOWN_ACTION) {
        nsCOMPtr<nsIPrefBranch> branch = aPrefBranch;
        if (!branch) {
            branch = do_GetService(NS_PREFSERVICE_CONTRACTID);
        }
        if (branch) {
            rv = branch->GetBoolPref("offline-apps.allow_by_default", aAllowed);
            if (NS_FAILED(rv)) {
                *aAllowed = PR_FALSE;
            }
        }

        return NS_OK;
    }

    if (perm == nsIPermissionManager::DENY_ACTION) {
        return NS_OK;
    }

    *aAllowed = PR_TRUE;

    return NS_OK;
}
