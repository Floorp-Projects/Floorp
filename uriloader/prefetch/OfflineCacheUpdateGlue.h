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
#include "mozilla/Attributes.h"

class nsOfflineCacheUpdate;

namespace mozilla {
namespace docshell {

// Like FORWARD_SAFE except methods:
//    Schedule
//    Init
#define NS_ADJUSTED_FORWARD_NSIOFFLINECACHEUPDATE(_to) \
  NS_IMETHOD GetStatus(uint16_t *aStatus) MOZ_OVERRIDE { return !_to ? NS_ERROR_NULL_POINTER : _to->GetStatus(aStatus); } \
  NS_IMETHOD GetPartial(bool *aPartial) MOZ_OVERRIDE { return !_to ? NS_ERROR_NULL_POINTER : _to->GetPartial(aPartial); } \
  NS_IMETHOD GetIsUpgrade(bool *aIsUpgrade) MOZ_OVERRIDE { return !_to ? NS_ERROR_NULL_POINTER : _to->GetIsUpgrade(aIsUpgrade); } \
  NS_IMETHOD GetUpdateDomain(nsACString & aUpdateDomain) MOZ_OVERRIDE { return !_to ? NS_ERROR_NULL_POINTER : _to->GetUpdateDomain(aUpdateDomain); } \
  NS_IMETHOD GetManifestURI(nsIURI **aManifestURI) MOZ_OVERRIDE { return !_to ? NS_ERROR_NULL_POINTER : _to->GetManifestURI(aManifestURI); } \
  NS_IMETHOD GetSucceeded(bool *aSucceeded) MOZ_OVERRIDE { return !_to ? NS_ERROR_NULL_POINTER : _to->GetSucceeded(aSucceeded); } \
  NS_IMETHOD InitPartial(nsIURI *aManifestURI, const nsACString & aClientID, nsIURI *aDocumentURI) MOZ_OVERRIDE { return !_to ? NS_ERROR_NULL_POINTER : _to->InitPartial(aManifestURI, aClientID, aDocumentURI); } \
  NS_IMETHOD InitForUpdateCheck(nsIURI *aManifestURI, uint32_t aAppID, bool aInBrowser, nsIObserver *aObserver) MOZ_OVERRIDE { return !_to ? NS_ERROR_NULL_POINTER : _to->InitForUpdateCheck(aManifestURI, aAppID, aInBrowser, aObserver); } \
  NS_IMETHOD AddDynamicURI(nsIURI *aURI) MOZ_OVERRIDE { return !_to ? NS_ERROR_NULL_POINTER : _to->AddDynamicURI(aURI); } \
  NS_IMETHOD AddObserver(nsIOfflineCacheUpdateObserver *aObserver, bool aHoldWeak) MOZ_OVERRIDE { return !_to ? NS_ERROR_NULL_POINTER : _to->AddObserver(aObserver, aHoldWeak); } \
  NS_IMETHOD RemoveObserver(nsIOfflineCacheUpdateObserver *aObserver) MOZ_OVERRIDE { return !_to ? NS_ERROR_NULL_POINTER : _to->RemoveObserver(aObserver); } \
  NS_IMETHOD GetByteProgress(uint64_t * _result) MOZ_OVERRIDE { return !_to ? NS_ERROR_NULL_POINTER : _to->GetByteProgress(_result); } \
  NS_IMETHOD Cancel() MOZ_OVERRIDE { return !_to ? NS_ERROR_NULL_POINTER : _to->Cancel(); }

class OfflineCacheUpdateGlue MOZ_FINAL : public nsSupportsWeakReference
                                       , public nsIOfflineCacheUpdate
                                       , public nsIOfflineCacheUpdateObserver
{
public:
    NS_DECL_ISUPPORTS

private:
    nsIOfflineCacheUpdate* EnsureUpdate();

public:
    NS_ADJUSTED_FORWARD_NSIOFFLINECACHEUPDATE(EnsureUpdate())
    NS_IMETHOD Schedule(void) MOZ_OVERRIDE;
    NS_IMETHOD Init(nsIURI *aManifestURI, 
                    nsIURI *aDocumentURI,
                    nsIDOMDocument *aDocument,
                    nsIFile *aCustomProfileDir,
                    uint32_t aAppID,
                    bool aInBrowser) MOZ_OVERRIDE;

    NS_DECL_NSIOFFLINECACHEUPDATEOBSERVER

    OfflineCacheUpdateGlue();

    void SetDocument(nsIDOMDocument *aDocument);

private:
    ~OfflineCacheUpdateGlue();

    nsRefPtr<nsOfflineCacheUpdate> mUpdate;
    bool mCoalesced;

    /* Document that requested this update */
    nsCOMPtr<nsIDOMDocument> mDocument;
    nsCOMPtr<nsIURI> mDocumentURI;
};

}
}

#endif
