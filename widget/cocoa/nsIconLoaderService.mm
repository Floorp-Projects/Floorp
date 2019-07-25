/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Retrieves and displays icons in native menu items on Mac OS X.
 */

/* exception_defines.h defines 'try' to 'if (true)' which breaks objective-c
   exceptions and produces errors like: error: unexpected '@' in program'.
   If we define __EXCEPTIONS exception_defines.h will avoid doing this.

   See bug 666609 for more information.

   We use <limits> to get the libstdc++ version. */
#include <limits>
#if __GLIBCXX__ <= 20070719
#  ifndef __EXCEPTIONS
#    define __EXCEPTIONS
#  endif
#endif

#include "gfxPlatform.h"
#include "imgIContainer.h"
#include "imgLoader.h"
#include "imgRequestProxy.h"
#include "mozilla/dom/Document.h"
#include "nsCocoaUtils.h"
#include "nsContentUtils.h"
#include "nsIconLoaderService.h"
#include "nsIContent.h"
#include "nsNameSpaceManager.h"
#include "nsNetUtil.h"
#include "nsObjCExceptions.h"
#include "nsThreadUtils.h"
#include "nsToolkit.h"

using namespace mozilla;

using mozilla::gfx::SourceSurface;

NS_IMPL_ISUPPORTS(nsIconLoaderService, imgINotificationObserver)

nsIconLoaderService::nsIconLoaderService(nsINode* aContent, nsIntRect* aImageRegionRect,
                                         RefPtr<nsIconLoaderObserver> aObserver,
                                         uint32_t aIconHeight, uint32_t aIconWidth)
    : mContent(aContent),
      mContentType(nsIContentPolicy::TYPE_INTERNAL_IMAGE),
      mImageRegionRect(aImageRegionRect),
      mLoadedIcon(false),
      mIconHeight(aIconHeight),
      mIconWidth(aIconWidth),
      mCompletionHandler(aObserver) {
  // Placeholder icon, which will later be replaced.
  mNativeIconImage = [[NSImage alloc] initWithSize:NSMakeSize(mIconHeight, mIconWidth)];
}

nsIconLoaderService::~nsIconLoaderService() { Destroy(); }

// Called from mMenuObjectX's destructor, to prevent us from outliving it
// (as might otherwise happen if calls to our imgINotificationObserver methods
// are still outstanding).  mMenuObjectX owns our nNativeMenuItem.
void nsIconLoaderService::Destroy() {
  if (mIconRequest) {
    mIconRequest->CancelAndForgetObserver(NS_BINDING_ABORTED);
    mIconRequest = nullptr;
  }
  mNativeIconImage = nil;
  mImageRegionRect = nullptr;
  mCompletionHandler = nil;
}

nsresult nsIconLoaderService::LoadIcon(nsIURI* aIconURI) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

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

  nsresult rv = loader->LoadImage(
      aIconURI, nullptr, nullptr, mContent->NodePrincipal(), 0, loadGroup, this, mContent, document,
      nsIRequest::LOAD_NORMAL, nullptr, mContentType, EmptyString(),
      /* aUseUrgentStartForChannel */ false, getter_AddRefs(mIconRequest));
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NSImage* nsIconLoaderService::GetNativeIconImage() { return mNativeIconImage; }

//
// imgINotificationObserver
//

NS_IMETHODIMP
nsIconLoaderService::Notify(imgIRequest* aRequest, int32_t aType, const nsIntRect* aData) {
  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    // Make sure the image loaded successfully.
    uint32_t status = imgIRequest::STATUS_ERROR;
    if (NS_FAILED(aRequest->GetImageStatus(&status)) || (status & imgIRequest::STATUS_ERROR)) {
      mIconRequest->Cancel(NS_BINDING_ABORTED);
      mIconRequest = nullptr;
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<imgIContainer> image;
    aRequest->GetImage(getter_AddRefs(image));
    MOZ_ASSERT(image);

    // Ask the image to decode at its intrinsic size.
    int32_t width = 0, height = 0;
    image->GetWidth(&width);
    image->GetHeight(&height);
    image->RequestDecodeForSize(nsIntSize(width, height), imgIContainer::FLAG_HIGH_QUALITY_SCALING);
  }

  if (aType == imgINotificationObserver::FRAME_COMPLETE) {
    nsresult rv = OnFrameComplete(aRequest);

    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = mCompletionHandler->OnComplete(mNativeIconImage);
    return rv;
  }

  if (aType == imgINotificationObserver::DECODE_COMPLETE) {
    if (mIconRequest && mIconRequest == aRequest) {
      mIconRequest->Cancel(NS_BINDING_ABORTED);
      mIconRequest = nullptr;
    }
  }

  return NS_OK;
}

nsresult nsIconLoaderService::OnFrameComplete(imgIRequest* aRequest) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

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
    mNativeIconImage = nil;
    return NS_ERROR_FAILURE;
  }

  int32_t origWidth = 0, origHeight = 0;
  imageContainer->GetWidth(&origWidth);
  imageContainer->GetHeight(&origHeight);

  // If the image region is invalid, don't draw the image to almost match
  // the behavior of other platforms.
  if (!mImageRegionRect->IsEmpty() &&
      (mImageRegionRect->XMost() > origWidth || mImageRegionRect->YMost() > origHeight)) {
    mNativeIconImage = nil;
    return NS_ERROR_FAILURE;
  }

  if (mImageRegionRect->IsEmpty()) {
    mImageRegionRect->SetRect(0, 0, origWidth, origHeight);
  }

  RefPtr<SourceSurface> surface =
      imageContainer->GetFrame(imgIContainer::FRAME_CURRENT, imgIContainer::FLAG_SYNC_DECODE);
  if (!surface) {
    mNativeIconImage = nil;
    return NS_ERROR_FAILURE;
  }

  CGImageRef origImage = NULL;
  bool isEntirelyBlack = false;
  nsresult rv = nsCocoaUtils::CreateCGImageFromSurface(surface, &origImage, &isEntirelyBlack);
  if (NS_FAILED(rv) || !origImage) {
    mNativeIconImage = nil;
    return NS_ERROR_FAILURE;
  }

  bool createSubImage =
      !(mImageRegionRect->x == 0 && mImageRegionRect->y == 0 &&
        mImageRegionRect->width == origWidth && mImageRegionRect->height == origHeight);

  CGImageRef finalImage = origImage;
  if (createSubImage) {
    // if mImageRegionRect is set using CSS, we need to slice a piece out of the overall
    // image to use as the icon
    finalImage = ::CGImageCreateWithImageInRect(
        origImage, ::CGRectMake(mImageRegionRect->x, mImageRegionRect->y, mImageRegionRect->width,
                                mImageRegionRect->height));
    ::CGImageRelease(origImage);
    if (!finalImage) {
      mNativeIconImage = nil;
      return NS_ERROR_FAILURE;
    }
  }

  NSImage* newImage = nil;
  rv = nsCocoaUtils::CreateNSImageFromCGImage(finalImage, &newImage);
  if (NS_FAILED(rv) || !newImage) {
    mNativeIconImage = nil;
    ::CGImageRelease(finalImage);
    return NS_ERROR_FAILURE;
  }

  // If all the color channels in the image are black, treat the image as a
  // template. This will cause macOS to use the image's alpha channel as a mask
  // and it will fill it with a color that looks good in the context that it's
  // used in. For example, for regular menu items, the image will be black, but
  // when the menu item is hovered (and its background is blue), it will be
  // filled with white.
  [newImage setTemplate:isEntirelyBlack];

  [newImage setSize:NSMakeSize(mIconWidth, mIconHeight)];

  NSImage* placeholderImage = mNativeIconImage;
  mNativeIconImage = newImage;
  [placeholderImage release];
  placeholderImage = nil;

  ::CGImageRelease(finalImage);

  mLoadedIcon = true;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
