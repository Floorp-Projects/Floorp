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
  NS_IMETHOD GetStatus(uint16_t *aStatus) override { return !_to ? NS_ERROR_NULL_POINTER : _to->GetStatus(aStatus); } \
  NS_IMETHOD GetPartial(bool *aPartial) override { return !_to ? NS_ERROR_NULL_POINTER : _to->GetPartial(aPartial); } \
  NS_IMETHOD GetIsUpgrade(bool *aIsUpgrade) override { return !_to ? NS_ERROR_NULL_POINTER : _to->GetIsUpgrade(aIsUpgrade); } \
  NS_IMETHOD GetUpdateDomain(nsACString & aUpdateDomain) override { return !_to ? NS_ERROR_NULL_POINTER : _to->GetUpdateDomain(aUpdateDomain); } \
  NS_IMETHOD GetManifestURI(nsIURI **aManifestURI) override { return !_to ? NS_ERROR_NULL_POINTER : _to->GetManifestURI(aManifestURI); } \
  NS_IMETHOD GetSucceeded(bool *aSucceeded) override { return !_to ? NS_ERROR_NULL_POINTER : _to->GetSucceeded(aSucceeded); } \
  NS_IMETHOD InitPartial(nsIURI *aManifestURI, const nsACString & aClientID, nsIURI *aDocumentURI, nsIPrincipal *aLoadingPrincipal) override { return !_to ? NS_ERROR_NULL_POINTER : _to->InitPartial(aManifestURI, aClientID, aDocumentURI, aLoadingPrincipal); } \
  NS_IMETHOD InitForUpdateCheck(nsIURI *aManifestURI, nsIPrincipal* aLoadingPrincipal, nsIObserver *aObserver) override { return !_to ? NS_ERROR_NULL_POINTER : _to->InitForUpdateCheck(aManifestURI, aLoadingPrincipal, aObserver); } \
  NS_IMETHOD AddDynamicURI(nsIURI *aURI) override { return !_to ? NS_ERROR_NULL_POINTER : _to->AddDynamicURI(aURI); } \
  NS_IMETHOD AddObserver(nsIOfflineCacheUpdateObserver *aObserver, bool aHoldWeak) override { return !_to ? NS_ERROR_NULL_POINTER : _to->AddObserver(aObserver, aHoldWeak); } \
  NS_IMETHOD RemoveObserver(nsIOfflineCacheUpdateObserver *aObserver) override { return !_to ? NS_ERROR_NULL_POINTER : _to->RemoveObserver(aObserver); } \
  NS_IMETHOD GetByteProgress(uint64_t * _result) override { return !_to ? NS_ERROR_NULL_POINTER : _to->GetByteProgress(_result); } \
  NS_IMETHOD Cancel() override { return !_to ? NS_ERROR_NULL_POINTER : _to->Cancel(); }

class OfflineCacheUpdateGlue final : public nsSupportsWeakReference
                                   , public nsIOfflineCacheUpdate
                                   , public nsIOfflineCacheUpdateObserver
{
public:
    NS_DECL_ISUPPORTS

private:
    nsIOfflineCacheUpdate* EnsureUpdate();

public:
    NS_ADJUSTED_FORWARD_NSIOFFLINECACHEUPDATE(EnsureUpdate())
    NS_IMETHOD Schedule(void) override;
    NS_IMETHOD Init(nsIURI *aManifestURI,
                    nsIURI *aDocumentURI,
                    nsIPrincipal* aLoadingPrincipal,
                    nsIDOMDocument *aDocument,
                    nsIFile *aCustomProfileDir) override;

    NS_DECL_NSIOFFLINECACHEUPDATEOBSERVER

    OfflineCacheUpdateGlue();

    void SetDocument(nsIDOMDocument *aDocument);

private:
    ~OfflineCacheUpdateGlue();

    RefPtr<nsOfflineCacheUpdate> mUpdate;
    bool mCoalesced;

    /* Document that requested this update */
    nsCOMPtr<nsIDOMDocument> mDocument;
    nsCOMPtr<nsIURI> mDocumentURI;
    nsCOMPtr<nsIPrincipal> mLoadingPrincipal;
};

} // namespace docshell
} // namespace mozilla

#endif
