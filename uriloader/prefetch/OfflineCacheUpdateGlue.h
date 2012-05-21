/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsOfflineCacheUpdateGlue_h
#define nsOfflineCacheUpdateGlue_h

#include "nsIOfflineCacheUpdate.h"

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsWeakReference.h"

class nsOfflineCacheUpdate;

namespace mozilla {
namespace docshell {

// Like FORWARD_SAFE except methods:
//    Schedule
//    Init
#define NS_ADJUSTED_FORWARD_NSIOFFLINECACHEUPDATE(_to) \
  NS_SCRIPTABLE NS_IMETHOD GetStatus(PRUint16 *aStatus) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetStatus(aStatus); } \
  NS_SCRIPTABLE NS_IMETHOD GetPartial(bool *aPartial) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetPartial(aPartial); } \
  NS_SCRIPTABLE NS_IMETHOD GetIsUpgrade(bool *aIsUpgrade) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetIsUpgrade(aIsUpgrade); } \
  NS_SCRIPTABLE NS_IMETHOD GetUpdateDomain(nsACString & aUpdateDomain) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetUpdateDomain(aUpdateDomain); } \
  NS_SCRIPTABLE NS_IMETHOD GetManifestURI(nsIURI **aManifestURI) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetManifestURI(aManifestURI); } \
  NS_SCRIPTABLE NS_IMETHOD GetSucceeded(bool *aSucceeded) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetSucceeded(aSucceeded); } \
  NS_SCRIPTABLE NS_IMETHOD InitPartial(nsIURI *aManifestURI, const nsACString & aClientID, nsIURI *aDocumentURI) { return !_to ? NS_ERROR_NULL_POINTER : _to->InitPartial(aManifestURI, aClientID, aDocumentURI); } \
  NS_SCRIPTABLE NS_IMETHOD AddDynamicURI(nsIURI *aURI) { return !_to ? NS_ERROR_NULL_POINTER : _to->AddDynamicURI(aURI); } \
  NS_SCRIPTABLE NS_IMETHOD AddObserver(nsIOfflineCacheUpdateObserver *aObserver, bool aHoldWeak) { return !_to ? NS_ERROR_NULL_POINTER : _to->AddObserver(aObserver, aHoldWeak); } \
  NS_SCRIPTABLE NS_IMETHOD RemoveObserver(nsIOfflineCacheUpdateObserver *aObserver) { return !_to ? NS_ERROR_NULL_POINTER : _to->RemoveObserver(aObserver); } 

class OfflineCacheUpdateGlue : public nsSupportsWeakReference
                               , public nsIOfflineCacheUpdate
                               , public nsIOfflineCacheUpdateObserver
{
public:
    NS_DECL_ISUPPORTS

private:
    nsIOfflineCacheUpdate* EnsureUpdate();

public:
    NS_ADJUSTED_FORWARD_NSIOFFLINECACHEUPDATE(EnsureUpdate())
    NS_SCRIPTABLE NS_IMETHOD Schedule(void);
    NS_SCRIPTABLE NS_IMETHOD Init(nsIURI *aManifestURI, 
                                  nsIURI *aDocumentURI, 
                                  nsIDOMDocument *aDocument);

    NS_DECL_NSIOFFLINECACHEUPDATEOBSERVER

    OfflineCacheUpdateGlue();
    ~OfflineCacheUpdateGlue();

    void SetDocument(nsIDOMDocument *aDocument);

private:
    nsRefPtr<nsOfflineCacheUpdate> mUpdate;

    /* Document that requested this update */
    nsCOMPtr<nsIDOMDocument> mDocument;
    nsCOMPtr<nsIURI> mDocumentURI;
};

}
}

#endif
