/* -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OfflineCacheUpdateParent.h"

#include "mozilla/dom/TabParent.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/unused.h"
#include "nsOfflineCacheUpdate.h"
#include "nsIApplicationCache.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"

using namespace mozilla::ipc;
using mozilla::dom::TabParent;

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

#undef LOG_ENABLED
#define LOG_ENABLED() PR_LOG_TEST(gOfflineCacheUpdateLog, 4)

namespace mozilla {
namespace docshell {

//-----------------------------------------------------------------------------
// OfflineCacheUpdateParent::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(OfflineCacheUpdateParent,
                  nsIOfflineCacheUpdateObserver,
                  nsILoadContext)

//-----------------------------------------------------------------------------
// OfflineCacheUpdateParent <public>
//-----------------------------------------------------------------------------

OfflineCacheUpdateParent::OfflineCacheUpdateParent(uint32_t aAppId,
                                                   bool aIsInBrowser)
    : mIPCClosed(false)
    , mIsInBrowserElement(aIsInBrowser)
    , mAppId(aAppId)
{
    // Make sure the service has been initialized
    nsOfflineCacheUpdateService::EnsureService();

    LOG(("OfflineCacheUpdateParent::OfflineCacheUpdateParent [%p]", this));
}

OfflineCacheUpdateParent::~OfflineCacheUpdateParent()
{
    LOG(("OfflineCacheUpdateParent::~OfflineCacheUpdateParent [%p]", this));
}

void
OfflineCacheUpdateParent::ActorDestroy(ActorDestroyReason why)
{
    mIPCClosed = true;
}

nsresult
OfflineCacheUpdateParent::Schedule(const URIParams& aManifestURI,
                                   const URIParams& aDocumentURI,
                                   const bool& stickDocument)
{
    LOG(("OfflineCacheUpdateParent::RecvSchedule [%p]", this));

    nsRefPtr<nsOfflineCacheUpdate> update;
    nsCOMPtr<nsIURI> manifestURI = DeserializeURI(aManifestURI);
    if (!manifestURI)
        return NS_ERROR_FAILURE;

    nsOfflineCacheUpdateService* service =
        nsOfflineCacheUpdateService::EnsureService();
    if (!service)
        return NS_ERROR_FAILURE;

    bool offlinePermissionAllowed = false;

    nsCOMPtr<nsIPrincipal> principal;
    nsContentUtils::GetSecurityManager()->
        GetAppCodebasePrincipal(manifestURI, mAppId, mIsInBrowserElement,
                                getter_AddRefs(principal));

    nsresult rv = service->OfflineAppAllowed(
        principal, nullptr, &offlinePermissionAllowed);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!offlinePermissionAllowed)
        return NS_ERROR_DOM_SECURITY_ERR;

    nsCOMPtr<nsIURI> documentURI = DeserializeURI(aDocumentURI);
    if (!documentURI)
        return NS_ERROR_FAILURE;

    if (!NS_SecurityCompareURIs(manifestURI, documentURI, false))
        return NS_ERROR_DOM_SECURITY_ERR;

    service->FindUpdate(manifestURI, mAppId, mIsInBrowserElement,
                        getter_AddRefs(update));
    if (!update) {
        update = new nsOfflineCacheUpdate();

        // Leave aDocument argument null. Only glues and children keep 
        // document instances.
        rv = update->Init(manifestURI, documentURI, nullptr, nullptr,
                          mAppId, mIsInBrowserElement);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = update->Schedule();
        NS_ENSURE_SUCCESS(rv, rv);
    }

    update->AddObserver(this, false);

    if (stickDocument) {
        nsCOMPtr<nsIURI> stickURI;
        documentURI->Clone(getter_AddRefs(stickURI));
        update->StickDocument(stickURI);
    }

    return NS_OK;
}

NS_IMETHODIMP
OfflineCacheUpdateParent::UpdateStateChanged(nsIOfflineCacheUpdate *aUpdate, uint32_t state)
{
    if (mIPCClosed)
        return NS_ERROR_UNEXPECTED;

    LOG(("OfflineCacheUpdateParent::StateEvent [%p]", this));

    uint64_t byteProgress;
    aUpdate->GetByteProgress(&byteProgress);
    unused << SendNotifyStateEvent(state, byteProgress);

    if (state == nsIOfflineCacheUpdateObserver::STATE_FINISHED) {
        // Tell the child the particulars after the update has finished.
        // Sending the Finish event will release the child side of the protocol
        // and notify "offline-cache-update-completed" on the child process.
        bool isUpgrade;
        aUpdate->GetIsUpgrade(&isUpgrade);
        bool succeeded;
        aUpdate->GetSucceeded(&succeeded);

        unused << SendFinish(succeeded, isUpgrade);
    }

    return NS_OK;
}

NS_IMETHODIMP
OfflineCacheUpdateParent::ApplicationCacheAvailable(nsIApplicationCache *aApplicationCache)
{
    if (mIPCClosed)
        return NS_ERROR_UNEXPECTED;

    NS_ENSURE_ARG(aApplicationCache);

    nsCString cacheClientId;
    aApplicationCache->GetClientID(cacheClientId);
    nsCString cacheGroupId;
    aApplicationCache->GetGroupID(cacheGroupId);

    unused << SendAssociateDocuments(cacheGroupId, cacheClientId);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// OfflineCacheUpdateParent::nsILoadContext
//-----------------------------------------------------------------------------

NS_IMETHODIMP
OfflineCacheUpdateParent::GetAssociatedWindow(nsIDOMWindow * *aAssociatedWindow)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OfflineCacheUpdateParent::GetTopWindow(nsIDOMWindow * *aTopWindow)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OfflineCacheUpdateParent::GetTopFrameElement(nsIDOMElement** aElement)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OfflineCacheUpdateParent::IsAppOfType(uint32_t appType, bool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OfflineCacheUpdateParent::GetIsContent(bool *aIsContent)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OfflineCacheUpdateParent::GetUsePrivateBrowsing(bool *aUsePrivateBrowsing)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
OfflineCacheUpdateParent::SetUsePrivateBrowsing(bool aUsePrivateBrowsing)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OfflineCacheUpdateParent::SetPrivateBrowsing(bool aUsePrivateBrowsing)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OfflineCacheUpdateParent::GetUseRemoteTabs(bool *aUseRemoteTabs)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OfflineCacheUpdateParent::SetRemoteTabs(bool aUseRemoteTabs)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
OfflineCacheUpdateParent::GetIsInBrowserElement(bool *aIsInBrowserElement)
{
    *aIsInBrowserElement = mIsInBrowserElement;
    return NS_OK;
}

NS_IMETHODIMP
OfflineCacheUpdateParent::GetAppId(uint32_t *aAppId)
{
    *aAppId = mAppId;
    return NS_OK;
}

} // docshell
} // mozilla
