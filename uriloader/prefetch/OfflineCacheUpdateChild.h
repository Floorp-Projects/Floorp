/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsOfflineCacheUpdateChild_h
#define nsOfflineCacheUpdateChild_h

#include "mozilla/docshell/POfflineCacheUpdateChild.h"
#include "nsIOfflineCacheUpdate.h"

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsICacheService.h"
#include "nsIDOMDocument.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsWeakReference.h"

namespace mozilla {
namespace docshell {

class OfflineCacheUpdateChild : public nsIOfflineCacheUpdate
                              , public POfflineCacheUpdateChild
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOFFLINECACHEUPDATE

    virtual bool
    RecvNotifyStateEvent(const uint32_t& stateEvent,
                         const uint64_t& byteProgress) MOZ_OVERRIDE;

    virtual bool
    RecvAssociateDocuments(
            const nsCString& cacheGroupId,
            const nsCString& cacheClientId) MOZ_OVERRIDE;

    virtual bool
    RecvFinish(const bool& succeeded,
               const bool& isUpgrade) MOZ_OVERRIDE;

    OfflineCacheUpdateChild(nsIDOMWindow* aWindow);
    ~OfflineCacheUpdateChild();

    void SetDocument(nsIDOMDocument *aDocument);

private:
    nsresult AssociateDocument(nsIDOMDocument *aDocument,
                               nsIApplicationCache *aApplicationCache);
    void GatherObservers(nsCOMArray<nsIOfflineCacheUpdateObserver> &aObservers);
    nsresult Finish();

    void RefcountHitZero();

    enum {
        STATE_UNINITIALIZED,
        STATE_INITIALIZED,
        STATE_CHECKING,
        STATE_DOWNLOADING,
        STATE_CANCELLED,
        STATE_FINISHED
    } mState;

    bool mIsUpgrade;
    bool mSucceeded;
    bool mIPCActivated;

    nsCString mUpdateDomain;
    nsCOMPtr<nsIURI> mManifestURI;
    nsCOMPtr<nsIURI> mDocumentURI;

    nsCOMPtr<nsIObserverService> mObserverService;

    uint32_t mAppID;
    bool mInBrowser;

    /* Clients watching this update for changes */
    nsCOMArray<nsIWeakReference> mWeakObservers;
    nsCOMArray<nsIOfflineCacheUpdateObserver> mObservers;

    /* Document that requested this update */
    nsCOMPtr<nsIDOMDocument> mDocument;

    /* Keep reference to the window that owns this update to call the
       parent offline cache update construcor */
    nsCOMPtr<nsIDOMWindow> mWindow;

    uint64_t mByteProgress;
};

}
}

#endif
