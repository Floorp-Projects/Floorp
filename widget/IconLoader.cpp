/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/widget/IconLoader.h"
#include "gfxPlatform.h"
#include "imgIContainer.h"
#include "imgLoader.h"
#include "mozilla/dom/Document.h"
#include "nsContentUtils.h"
#include "nsIContent.h"

using namespace mozilla;

using mozilla::gfx::SourceSurface;

namespace mozilla::widget {

NS_IMPL_CYCLE_COLLECTION(mozilla::widget::IconLoader, mContent, mHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(mozilla::widget::IconLoader)
  NS_INTERFACE_MAP_ENTRY(imgINotificationObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTING_ADDREF(mozilla::widget::IconLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(mozilla::widget::IconLoader)

IconLoader::IconLoader(Helper* aHelper, nsINode* aContent,
                       const nsIntRect& aImageRegionRect)
    : mContent(aContent),
      mContentType(nsIContentPolicy::TYPE_INTERNAL_IMAGE),
      mImageRegionRect(aImageRegionRect),
      mLoadedIcon(false),
      mHelper(aHelper) {}

IconLoader::~IconLoader() { Destroy(); }

void IconLoader::Destroy() {
  if (mIconRequest) {
    mIconRequest->CancelAndForgetObserver(NS_BINDING_ABORTED);
    mIconRequest = nullptr;
  }
  if (mHelper) {
    mHelper = nullptr;
  }
}

nsresult IconLoader::LoadIcon(nsIURI* aIconURI, bool aIsInternalIcon) {
  if (mIconRequest) {
    // Another icon request is already in flight.  Kill it.
    mIconRequest->Cancel(NS_BINDING_ABORTED);
    mIconRequest = nullptr;
  }

  mLoadedIcon = false;

  if (!mContent) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<mozilla::dom::Document> document = mContent->OwnerDoc();

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
        nullptr, nsIRequest::LOAD_NORMAL, nullptr, mContentType, u""_ns,
        /* aUseUrgentStartForChannel */ false, /* aLinkPreload */ false,
        getter_AddRefs(mIconRequest));
  } else {
    rv = loader->LoadImage(
        aIconURI, nullptr, nullptr, mContent->NodePrincipal(), 0, loadGroup,
        this, mContent, document, nsIRequest::LOAD_NORMAL, nullptr,
        mContentType, u""_ns, /* aUseUrgentStartForChannel */ false,
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
      mIconRequest->Cancel(NS_BINDING_ABORTED);
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
    nsresult rv = OnFrameComplete(aRequest);

    if (NS_FAILED(rv)) {
      return;
    }

    nsCOMPtr<imgIContainer> image;
    aRequest->GetImage(getter_AddRefs(image));
    MOZ_ASSERT(image);

    mHelper->OnComplete(image, mImageRegionRect);
    return;
  }

  if (aType == imgINotificationObserver::DECODE_COMPLETE) {
    if (mIconRequest && mIconRequest == aRequest) {
      mIconRequest->Cancel(NS_BINDING_ABORTED);
      mIconRequest = nullptr;
    }
  }
}

nsresult IconLoader::OnFrameComplete(imgIRequest* aRequest) {
  if (aRequest != mIconRequest) {
    return NS_ERROR_FAILURE;
  }

  // Only support one frame.
  if (mLoadedIcon) {
    return NS_OK;
  }

  nsCOMPtr<imgIContainer> imageContainer;
  aRequest->GetImage(getter_AddRefs(imageContainer));
  if (!imageContainer) {
    return NS_ERROR_FAILURE;
  }

  int32_t origWidth = 0, origHeight = 0;
  imageContainer->GetWidth(&origWidth);
  imageContainer->GetHeight(&origHeight);

  // If the image region is invalid, don't draw the image to almost match
  // the behavior of other platforms.
  if (!mImageRegionRect.IsEmpty() && (mImageRegionRect.XMost() > origWidth ||
                                      mImageRegionRect.YMost() > origHeight)) {
    return NS_ERROR_FAILURE;
  }

  if (mImageRegionRect.IsEmpty()) {
    mImageRegionRect.SetRect(0, 0, origWidth, origHeight);
  }

  mLoadedIcon = true;

  return NS_OK;
}

}  // namespace mozilla::widget
