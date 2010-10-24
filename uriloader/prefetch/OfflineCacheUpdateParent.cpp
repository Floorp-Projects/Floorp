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

#include "OfflineCacheUpdateParent.h"
#include "nsOfflineCacheUpdate.h"
#include "nsIApplicationCache.h"

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
extern PRLogModuleInfo *gOfflineCacheUpdateLog;
#endif
#define LOG(args) PR_LOG(gOfflineCacheUpdateLog, 4, args)
#define LOG_ENABLED() PR_LOG_TEST(gOfflineCacheUpdateLog, 4)

namespace mozilla {
namespace docshell {

//-----------------------------------------------------------------------------
// OfflineCacheUpdateParent::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS1(OfflineCacheUpdateParent,
                   nsIOfflineCacheUpdateObserver)

//-----------------------------------------------------------------------------
// OfflineCacheUpdateParent <public>
//-----------------------------------------------------------------------------

OfflineCacheUpdateParent::OfflineCacheUpdateParent()
{
    // Make sure the service has been initialized
    nsOfflineCacheUpdateService* service =
        nsOfflineCacheUpdateService::EnsureService();
    if (!service)
        return;

    LOG(("OfflineCacheUpdateParent::OfflineCacheUpdateParent [%p]", this));
}

OfflineCacheUpdateParent::~OfflineCacheUpdateParent()
{
    LOG(("OfflineCacheUpdateParent::~OfflineCacheUpdateParent [%p]", this));
}

nsresult
OfflineCacheUpdateParent::Schedule(const URI& aManifestURI,
                                   const URI& aDocumentURI,
                                   const nsCString& aClientID,
                                   const bool& stickDocument)
{
    LOG(("OfflineCacheUpdateParent::RecvSchedule [%p]", this));

    nsRefPtr<nsOfflineCacheUpdate> update;
    nsCOMPtr<nsIURI> manifestURI(aManifestURI);
    nsCOMPtr<nsIURI> documentURI(aDocumentURI);

    nsOfflineCacheUpdateService* service =
        nsOfflineCacheUpdateService::EnsureService();
    if (!service)
        return NS_ERROR_FAILURE;

    service->FindUpdate(manifestURI, documentURI, getter_AddRefs(update));
    if (!update) {
        update = new nsOfflineCacheUpdate();

        nsresult rv;
        // Leave aDocument argument null. Only glues and children keep 
        // document instances.
        rv = update->Init(manifestURI, documentURI, nsnull);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = update->Schedule();
        NS_ENSURE_SUCCESS(rv, rv);
    }

    update->AddObserver(this, PR_FALSE);

    if (stickDocument) {
      nsCOMPtr<nsIURI> stickURI;
      documentURI->Clone(getter_AddRefs(stickURI));
      update->StickDocument(stickURI);
    }

    return NS_OK;
}

NS_IMETHODIMP
OfflineCacheUpdateParent::UpdateStateChanged(nsIOfflineCacheUpdate *aUpdate, PRUint32 state)
{
    LOG(("OfflineCacheUpdateParent::StateEvent [%p]", this));

    SendNotifyStateEvent(state);

    if (state == nsIOfflineCacheUpdateObserver::STATE_FINISHED) {
        // Tell the child the particulars after the update has finished.
        // Sending the Finish event will release the child side of the protocol
        // and notify "offline-cache-update-completed" on the child process.
        PRBool isUpgrade;
        aUpdate->GetIsUpgrade(&isUpgrade);
        PRBool succeeded;
        aUpdate->GetSucceeded(&succeeded);

        SendFinish(succeeded, isUpgrade);
    }

    return NS_OK;
}

NS_IMETHODIMP
OfflineCacheUpdateParent::ApplicationCacheAvailable(nsIApplicationCache *aApplicationCache)
{
    NS_ENSURE_ARG(aApplicationCache);

    nsCString cacheClientId;
    aApplicationCache->GetClientID(cacheClientId);
    nsCString cacheGroupId;
    aApplicationCache->GetGroupID(cacheGroupId);

    SendAssociateDocuments(cacheGroupId, cacheClientId);
    return NS_OK;
}

} // docshell
} // mozilla