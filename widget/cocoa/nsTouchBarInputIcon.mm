/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Retrieves and displays icons on the macOS Touch Bar.
 */

#include "nsTouchBarInputIcon.h"

#include "nsCocoaUtils.h"
#include "nsComputedDOMStyle.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "nsINode.h"
#include "nsNameSpaceManager.h"
#include "nsObjCExceptions.h"

using namespace mozilla;

static const uint32_t kIconSize = 16;
static const CGFloat kHiDPIScalingFactor = 2.0f;

nsTouchBarInputIcon::nsTouchBarInputIcon(RefPtr<Document> aDocument, TouchBarInput* aInput,
                                         NSTouchBarItem* aItem)
    : mDocument(aDocument), mSetIcon(false), mButton(nil), mShareScrubber(nil), mPopoverItem(nil) {
  if ([[aInput nativeIdentifier] isEqualToString:ShareScrubberIdentifier]) {
    mShareScrubber = (NSSharingServicePickerTouchBarItem*)aItem;
  } else if ([aInput baseType] == TouchBarInputBaseType::kPopover) {
    mPopoverItem = (NSPopoverTouchBarItem*)aItem;
  } else if ([aInput baseType] == TouchBarInputBaseType::kButton ||
             [aInput baseType] == TouchBarInputBaseType::kMainButton) {
    mButton = (NSButton*)[aItem view];
  } else {
    NS_ERROR("Incompatible Touch Bar input passed to nsTouchBarInputIcon.");
  }
  aInput = nil;
  MOZ_COUNT_CTOR(nsTouchBarInputIcon);
}

nsTouchBarInputIcon::~nsTouchBarInputIcon() {
  Destroy();
  MOZ_COUNT_DTOR(nsTouchBarInputIcon);
}

// Called from nsTouchBar's destructor, to prevent us from outliving it
// (as might otherwise happen if calls to our imgINotificationObserver methods
// are still outstanding).  nsTouchBar owns our mTouchBarInput.
void nsTouchBarInputIcon::Destroy() {
  ReleaseJSObjects();
  if (mIconLoader) {
    mIconLoader->Destroy();
    mIconLoader = nullptr;
  }

  mButton = nil;
  mShareScrubber = nil;
  mPopoverItem = nil;
}

nsresult nsTouchBarInputIcon::SetupIcon(nsCOMPtr<nsIURI> aIconURI) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // We might not have a document if the Touch Bar tries to update when the main
  // window is closed.
  if (!mDocument) {
    return NS_OK;
  }

  if (!(mButton || mShareScrubber || mPopoverItem)) {
    NS_ERROR("No Touch Bar input provided.");
    return NS_ERROR_FAILURE;
  }

  if (!mIconLoader) {
    // We ask only for the HiDPI images since all Touch Bars are Retina
    // displays and we have no need for icons @1x.
    mIconLoader = new nsIconLoaderService(mDocument, &mImageRegionRect, this, kIconSize, kIconSize,
                                          kHiDPIScalingFactor);
    if (!mIconLoader) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (!mSetIcon) {
    // Load placeholder icon.
    [mButton setImage:mIconLoader->GetNativeIconImage()];
    [mShareScrubber setButtonImage:mIconLoader->GetNativeIconImage()];
    [mPopoverItem setCollapsedRepresentationImage:mIconLoader->GetNativeIconImage()];
  }

  nsresult rv = mIconLoader->LoadIcon(aIconURI, true /* aIsInternalIcon */);
  if (NS_FAILED(rv)) {
    // There is no icon for this menu item, as an error occurred while loading it.
    // An icon might have been set earlier or the place holder icon may have
    // been set.  Clear it.
    [mButton setImage:nil];
    [mShareScrubber setButtonImage:nil];
    [mPopoverItem setCollapsedRepresentationImage:nil];
  }

  mSetIcon = true;

  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void nsTouchBarInputIcon::ReleaseJSObjects() {
  if (mIconLoader) {
    mIconLoader->ReleaseJSObjects();
  }
  mDocument = nil;
}

//
// nsIconLoaderObserver
//

nsresult nsTouchBarInputIcon::OnComplete(NSImage* aImage) {
  [mButton setImage:aImage];
  [mShareScrubber setButtonImage:aImage];
  [mPopoverItem setCollapsedRepresentationImage:aImage];

  [aImage release];
  return NS_OK;
}
