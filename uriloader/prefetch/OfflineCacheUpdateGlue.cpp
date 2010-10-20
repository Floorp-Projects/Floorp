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
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Honza Bambas <honzab@firemni.cz>
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

#include "OfflineCacheUpdateGlue.h"
#include "nsOfflineCacheUpdate.h"
#include "mozilla/Services.h"

#include "nsIApplicationCache.h"
#include "nsIApplicationCacheChannel.h"
#include "nsIApplicationCacheContainer.h"
#include "nsIChannel.h"
#include "nsIDocument.h"
#include "prlog.h"

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
#define LOG(args) PR_LOG(gOfflineCacheUpdateLog, 4, args)
#define LOG_ENABLED() PR_LOG_TEST(gOfflineCacheUpdateLog, 4)

namespace mozilla {
namespace docshell {

//-----------------------------------------------------------------------------
// OfflineCacheUpdateGlue::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS3(OfflineCacheUpdateGlue,
                   nsIOfflineCacheUpdate,
                   nsIOfflineCacheUpdateObserver,
                   nsISupportsWeakReference)

//-----------------------------------------------------------------------------
// OfflineCacheUpdateGlue <public>
//-----------------------------------------------------------------------------

OfflineCacheUpdateGlue::OfflineCacheUpdateGlue()
{
    LOG(("OfflineCacheUpdateGlue::OfflineCacheUpdateGlue [%p]", this));
}

OfflineCacheUpdateGlue::~OfflineCacheUpdateGlue()
{
    LOG(("OfflineCacheUpdateGlue::~OfflineCacheUpdateGlue [%p]", this));
}

nsIOfflineCacheUpdate*
OfflineCacheUpdateGlue::EnsureUpdate()
{
    if (!mUpdate) {
        mUpdate = new nsOfflineCacheUpdate();
        LOG(("OfflineCacheUpdateGlue [%p] is using update [%p]", this, mUpdate.get()));
    }

    return mUpdate;
}

NS_IMETHODIMP
OfflineCacheUpdateGlue::Schedule()
{
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService) {
        LOG(("Calling offline-cache-update-added"));
        observerService->NotifyObservers(static_cast<nsIOfflineCacheUpdate*>(this),
                                         "offline-cache-update-added",
                                         nsnull);
        LOG(("Done offline-cache-update-added"));
    }

    if (!EnsureUpdate())
        return NS_ERROR_NULL_POINTER;

    // Do not use weak reference, we must survive!
    mUpdate->AddObserver(this, PR_FALSE);

    return mUpdate->Schedule();
}

NS_IMETHODIMP
OfflineCacheUpdateGlue::Init(nsIURI *aManifestURI, 
                             nsIURI *aDocumentURI,
                             nsIDOMDocument *aDocument)
{
    if (!EnsureUpdate())
        return NS_ERROR_NULL_POINTER;

    mDocumentURI = aDocumentURI;

    if (aDocument)
        SetDocument(aDocument);

    return mUpdate->Init(aManifestURI, aDocumentURI, nsnull);
}

void
OfflineCacheUpdateGlue::SetDocument(nsIDOMDocument *aDocument)
{
    // The design is one document for one cache update on the content process.
    NS_ASSERTION(!mDocument, 
                 "Setting more then a single document on an instance of OfflineCacheUpdateGlue");

    LOG(("Document %p added to update glue %p", aDocument, this));

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

    PRBool loadedFromAppCache;
    appCacheChannel->GetLoadedFromApplicationCache(&loadedFromAppCache);
    if (loadedFromAppCache)
        return;

    if (EnsureUpdate()) {
        mUpdate->StickDocument(mDocumentURI);
    }

    mDocument = aDocument;
}

NS_IMETHODIMP
OfflineCacheUpdateGlue::UpdateStateChanged(nsIOfflineCacheUpdate *aUpdate, PRUint32 state)
{
    if (state == nsIOfflineCacheUpdateObserver::STATE_FINISHED) {
        LOG(("OfflineCacheUpdateGlue got STATE_FINISHED [%p]", this));

        nsCOMPtr<nsIObserverService> observerService =
          mozilla::services::GetObserverService();
        if (observerService) {
            LOG(("Calling offline-cache-update-completed"));
            observerService->NotifyObservers(static_cast<nsIOfflineCacheUpdate*>(this),
                                             "offline-cache-update-completed",
                                             nsnull);
            LOG(("Done offline-cache-update-completed"));
        }

        aUpdate->RemoveObserver(this);
    }

    return NS_OK;
}

NS_IMETHODIMP
OfflineCacheUpdateGlue::ApplicationCacheAvailable(nsIApplicationCache *aApplicationCache)
{
    NS_ENSURE_ARG(aApplicationCache);

    // Check that the document that requested this update was
    // previously associated with an application cache.  If not, it
    // should be associated with the new one.
    nsCOMPtr<nsIApplicationCacheContainer> container =
        do_QueryInterface(mDocument);
    if (!container)
        return NS_OK;

    nsCOMPtr<nsIApplicationCache> existingCache;
    nsresult rv = container->GetApplicationCache(getter_AddRefs(existingCache));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!existingCache) {
#if defined(PR_LOGGING)
        if (LOG_ENABLED()) {
            nsCAutoString clientID;
            if (aApplicationCache) {
                aApplicationCache->GetClientID(clientID);
            }
            LOG(("Update %p: associating app cache %s to document %p",
                 this, clientID.get(), mDocument));
        }
#endif

        rv = container->SetApplicationCache(aApplicationCache);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

}
}
