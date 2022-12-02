/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/widget/IconLoader.h"
#include "gfxPlatform.h"
#include "imgIContainer.h"
#include "imgLoader.h"
#include "imgRequestProxy.h"
#include "mozilla/dom/Document.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsIContentPolicy.h"

using namespace mozilla;

namespace mozilla::widget {

NS_IMPL_ISUPPORTS(IconLoader, imgINotificationObserver)

IconLoader::IconLoader(Listener* aListener) : mListener(aListener) {}

IconLoader::~IconLoader() { Destroy(); }

void IconLoader::Destroy() {
  if (mIconRequest) {
    mIconRequest->CancelAndForgetObserver(NS_BINDING_ABORTED);
    mIconRequest = nullptr;
  }
  mListener = nullptr;
}

nsresult IconLoader::LoadIcon(nsIURI* aIconURI, nsINode* aNode,
                              bool aIsInternalIcon) {
  if (mIconRequest) {
    // Another icon request is already in flight.  Kill it.
    mIconRequest->CancelWithReason(
        NS_BINDING_ABORTED, "Another icon request is already in flight"_ns);
    mIconRequest = nullptr;
  }

  if (!aNode) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<mozilla::dom::Document> document = aNode->OwnerDoc();

  nsCOMPtr<nsILoadGroup> loadGroup = document->GetDocumentLoadGroup();
  if (!loadGroup) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<imgLoader> loader = nsContentUtils::GetImgLoaderForDocument(document);
  if (!loader) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  if (aIsInternalIcon) {
    rv = loader->LoadImage(
        aIconURI, nullptr, nullptr, nullptr, 0, loadGroup, this, nullptr,
        nullptr, nsIRequest::LOAD_NORMAL, nullptr,
        nsIContentPolicy::TYPE_INTERNAL_IMAGE, u""_ns,
        /* aUseUrgentStartForChannel */ false, /* aLinkPreload */ false,
        getter_AddRefs(mIconRequest));
  } else {
    // TODO: nsIContentPolicy::TYPE_INTERNAL_IMAGE may not be the correct
    // policy. See bug 1691868 for more details.
    rv = loader->LoadImage(
        aIconURI, nullptr, nullptr, aNode->NodePrincipal(), 0, loadGroup, this,
        aNode, document, nsIRequest::LOAD_NORMAL, nullptr,
        nsIContentPolicy::TYPE_INTERNAL_IMAGE, u""_ns,
        /* aUseUrgentStartForChannel */ false,
        /* aLinkPreload */ false, getter_AddRefs(mIconRequest));
  }
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

//
// imgINotificationObserver
//

void IconLoader::Notify(imgIRequest* aRequest, int32_t aType,
                        const nsIntRect* aData) {
  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    // Make sure the image loaded successfully.
    uint32_t status = imgIRequest::STATUS_ERROR;
    if (NS_FAILED(aRequest->GetImageStatus(&status)) ||
        (status & imgIRequest::STATUS_ERROR)) {
      mIconRequest->CancelWithReason(NS_BINDING_ABORTED,
                                     "GetImageStatus failed"_ns);
      mIconRequest = nullptr;
      return;
    }

    nsCOMPtr<imgIContainer> image;
    aRequest->GetImage(getter_AddRefs(image));
    MOZ_ASSERT(image);

    // Ask the image to decode at its intrinsic size.
    int32_t width = 0, height = 0;
    image->GetWidth(&width);
    image->GetHeight(&height);
    image->RequestDecodeForSize(nsIntSize(width, height),
                                imgIContainer::FLAG_HIGH_QUALITY_SCALING);
  }

  if (aType == imgINotificationObserver::FRAME_COMPLETE) {
    nsCOMPtr<imgIContainer> image;
    aRequest->GetImage(getter_AddRefs(image));
    MOZ_ASSERT(image);

    if (mListener) {
      mListener->OnComplete(image);
    }
    return;
  }

  if (aType == imgINotificationObserver::DECODE_COMPLETE) {
    if (mIconRequest && mIconRequest == aRequest) {
      mIconRequest->CancelWithReason(NS_BINDING_ABORTED, "DECODE_COMPLETE"_ns);
      mIconRequest = nullptr;
    }
  }
}

}  // namespace mozilla::widget
