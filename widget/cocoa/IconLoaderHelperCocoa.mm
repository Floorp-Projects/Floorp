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
#include "nsIContent.h"
#include "nsNameSpaceManager.h"
#include "nsNetUtil.h"
#include "nsObjCExceptions.h"
#include "nsThreadUtils.h"
#include "nsToolkit.h"
#include "IconLoaderHelperCocoa.h"

using namespace mozilla;

using mozilla::gfx::SourceSurface;
using mozilla::widget::IconLoaderListenerCocoa;

namespace mozilla::widget {

NS_IMPL_ISUPPORTS0(IconLoaderHelperCocoa)

IconLoaderHelperCocoa::IconLoaderHelperCocoa(IconLoaderListenerCocoa* aListener,
                                             uint32_t aIconHeight, uint32_t aIconWidth,
                                             CGFloat aScaleFactor)
    : mLoadListener(aListener),
      mIconHeight(aIconHeight),
      mIconWidth(aIconWidth),
      mScaleFactor(aScaleFactor) {
  // Placeholder icon, which will later be replaced.
  mNativeIconImage = [[NSImage alloc] initWithSize:NSMakeSize(mIconHeight, mIconWidth)];
  MOZ_ASSERT(aListener);
}

IconLoaderHelperCocoa::~IconLoaderHelperCocoa() { Destroy(); }

nsresult IconLoaderHelperCocoa::OnComplete(imgIContainer* aImage, const nsIntRect& aRect) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT

  NS_ENSURE_ARG_POINTER(aImage);

  bool isEntirelyBlack = false;
  NSImage* newImage = nil;
  nsresult rv;
  if (mScaleFactor != 0.0f) {
    rv = nsCocoaUtils::CreateNSImageFromImageContainer(aImage, imgIContainer::FRAME_CURRENT,
                                                       &newImage, mScaleFactor, &isEntirelyBlack);
  } else {
    rv = nsCocoaUtils::CreateDualRepresentationNSImageFromImageContainer(
        aImage, imgIContainer::FRAME_CURRENT, &newImage, &isEntirelyBlack);
  }

  if (NS_FAILED(rv) || !newImage) {
    mNativeIconImage = nil;
    [newImage release];
    return NS_ERROR_FAILURE;
  }

  NSSize requestedSize = NSMakeSize(mIconWidth, mIconHeight);

  int32_t origWidth = 0, origHeight = 0;
  aImage->GetWidth(&origWidth);
  aImage->GetHeight(&origHeight);

  bool createSubImage =
      !(aRect.x == 0 && aRect.y == 0 && aRect.width == origWidth && aRect.height == origHeight);

  if (createSubImage) {
    // If aRect is set using CSS, we need to slice a piece out of the
    // overall image to use as the icon.
    NSImage* subImage =
        [NSImage imageWithSize:requestedSize
                       flipped:NO
                drawingHandler:^BOOL(NSRect subImageRect) {
                  [newImage drawInRect:NSMakeRect(0, 0, mIconWidth, mIconHeight)
                              fromRect:NSMakeRect(aRect.x, aRect.y, aRect.width, aRect.height)
                             operation:NSCompositingOperationCopy
                              fraction:1.0f];
                  return YES;
                }];
    [newImage release];
    newImage = [subImage retain];
    subImage = nil;
  }

  // If all the color channels in the image are black, treat the image as a
  // template. This will cause macOS to use the image's alpha channel as a mask
  // and it will fill it with a color that looks good in the context that it's
  // used in. For example, for regular menu items, the image will be black, but
  // when the menu item is hovered (and its background is blue), it will be
  // filled with white.
  [newImage setTemplate:isEntirelyBlack];

  [newImage setSize:requestedSize];

  NSImage* placeholderImage = mNativeIconImage;
  mNativeIconImage = newImage;
  [placeholderImage release];
  placeholderImage = nil;

  mLoadListener->OnComplete();

  return NS_OK;
  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT
}

NSImage* IconLoaderHelperCocoa::GetNativeIconImage() { return mNativeIconImage; }

void IconLoaderHelperCocoa::Destroy() {
  if (mNativeIconImage) {
    [mNativeIconImage release];
    mNativeIconImage = nil;
  }
}

}  // namespace mozilla::widget
