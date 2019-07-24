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

nsTouchBarInputIcon::nsTouchBarInputIcon(RefPtr<mozilla::dom::Document> aDocument,
                                         NSButton* aButton,
                                         NSSharingServicePickerTouchBarItem* aShareScrubber)
    : mDocument(aDocument), mSetIcon(false), mButton(aButton), mShareScrubber(aShareScrubber) {
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
  if (mIconLoader) {
    mIconLoader->Destroy();
    mIconLoader = nullptr;
  }

  mButton = nil;
}

nsresult nsTouchBarInputIcon::SetupIcon(nsCOMPtr<nsIURI> aIconURI) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Still don't have one, then something is wrong, get out of here.
  if (!(mButton || mShareScrubber)) {
    NS_ERROR("No Touch Bar button");
    return NS_ERROR_FAILURE;
  }

  if (!mIconLoader) {
    mIconLoader = new nsIconLoaderService(mDocument, &mImageRegionRect, this, kIconSize, kIconSize);
    if (!mIconLoader) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (!mSetIcon) {
    // Load placeholder icon.
    [mButton setImage:mIconLoader->GetNativeIconImage()];
  }

  nsresult rv = mIconLoader->LoadIcon(aIconURI);
  if (NS_FAILED(rv)) {
    // There is no icon for this menu item, as an error occurred while loading it.
    // An icon might have been set earlier or the place holder icon may have
    // been set.  Clear it.
    [mButton setImage:nil];
  }

  mSetIcon = true;

  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

//
// nsIconLoaderObserver
//

nsresult nsTouchBarInputIcon::OnComplete(NSImage* aImage) {
  // Apple's share scrubber requires special handling since it does not expose
  // a NSButton* view like a regular TouchBarItem*.
  if (mShareScrubber) {
    mShareScrubber.buttonImage = aImage;
  } else {
    // If image is nil, this fails gracefully to a text label.
    [mButton setImage:aImage];
  }

  [aImage release];
  return NS_OK;
}
