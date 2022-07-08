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

#include "MOZIconHelper.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "nsCocoaUtils.h"
#include "nsComputedDOMStyle.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsIContentPolicy.h"
#include "nsMenuItemX.h"
#include "nsMenuItemIconX.h"
#include "nsNameSpaceManager.h"
#include "nsObjCExceptions.h"

using namespace mozilla;

using mozilla::dom::Element;
using mozilla::widget::IconLoader;

static const uint32_t kIconSize = 16;

nsMenuItemIconX::nsMenuItemIconX(Listener* aListener) : mListener(aListener) {
  MOZ_COUNT_CTOR(nsMenuItemIconX);
}

nsMenuItemIconX::~nsMenuItemIconX() {
  if (mIconLoader) {
    mIconLoader->Destroy();
  }
  if (mIconImage) {
    [mIconImage release];
    mIconImage = nil;
  }
  MOZ_COUNT_DTOR(nsMenuItemIconX);
}

void nsMenuItemIconX::SetupIcon(nsIContent* aContent) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  bool shouldHaveIcon = StartIconLoad(aContent);
  if (!shouldHaveIcon) {
    // There is no icon for this menu item, as an error occurred while loading it.
    // An icon might have been set earlier or the place holder icon may have
    // been set.  Clear it.
    if (mIconImage) {
      [mIconImage release];
      mIconImage = nil;
    }
    return;
  }

  if (!mIconImage) {
    // Set a placeholder icon, so that the menuitem reserves space for the icon during the load and
    // there is no sudden shift once the icon finishes loading.
    NSSize iconSize = NSMakeSize(kIconSize, kIconSize);
    mIconImage = [[MOZIconHelper placeholderIconWithSize:iconSize] retain];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

bool nsMenuItemIconX::StartIconLoad(nsIContent* aContent) {
  RefPtr<nsIURI> iconURI = GetIconURI(aContent);
  if (!iconURI) {
    return false;
  }

  if (!mIconLoader) {
    mIconLoader = new IconLoader(this);
  }

  nsresult rv = mIconLoader->LoadIcon(iconURI, aContent);
  return NS_SUCCEEDED(rv);
}

already_AddRefed<nsIURI> nsMenuItemIconX::GetIconURI(nsIContent* aContent) {
  // First, look at the content node's "image" attribute.
  nsAutoString imageURIString;
  bool hasImageAttr =
      aContent->IsElement() &&
      aContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::image, imageURIString);

  if (hasImageAttr) {
    // Use the URL from the image attribute.
    // If this menu item shouldn't have an icon, the string will be empty,
    // and NS_NewURI will fail.
    RefPtr<nsIURI> iconURI;
    nsresult rv = NS_NewURI(getter_AddRefs(iconURI), imageURIString);
    if (NS_FAILED(rv)) {
      return nullptr;
    }
    mImageRegionRect.SetEmpty();
    return iconURI.forget();
  }

  // If the content node has no "image" attribute, get the
  // "list-style-image" property from CSS.
  RefPtr<mozilla::dom::Document> document = aContent->GetComposedDoc();
  if (!document || !aContent->IsElement()) {
    return nullptr;
  }

  RefPtr<const ComputedStyle> sc = nsComputedDOMStyle::GetComputedStyle(aContent->AsElement());
  if (!sc) {
    return nullptr;
  }

  RefPtr<nsIURI> iconURI = sc->StyleList()->GetListStyleImageURI();
  if (!iconURI) {
    return nullptr;
  }

  // Check if the icon has a specified image region so that it can be
  // cropped appropriately before being displayed.
  const nsRect r = sc->StyleList()->GetImageRegion();

  // Return nullptr if the image region is invalid so the image
  // is not drawn, and behavior is similar to XUL menus.
  if (r.X() < 0 || r.Y() < 0 || r.Width() < 0 || r.Height() < 0) {
    return nullptr;
  }

  // 'auto' is represented by a [0, 0, 0, 0] rect. Only set mImageRegionRect
  // if we have some other value.
  if (r.IsEmpty()) {
    mImageRegionRect.SetEmpty();
  } else {
    mImageRegionRect = r.ToNearestPixels(mozilla::AppUnitsPerCSSPixel());
  }
  mComputedStyle = std::move(sc);
  mPresContext = document->GetPresContext();

  return iconURI.forget();
}

//
// mozilla::widget::IconLoader::Listener
//

nsresult nsMenuItemIconX::OnComplete(imgIContainer* aImage) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mIconImage) {
    [mIconImage release];
    mIconImage = nil;
  }
  RefPtr<nsPresContext> pc = mPresContext.get();
  mIconImage = [[MOZIconHelper iconImageFromImageContainer:aImage
                                                  withSize:NSMakeSize(kIconSize, kIconSize)
                                               presContext:pc
                                             computedStyle:mComputedStyle
                                                   subrect:mImageRegionRect
                                               scaleFactor:0.0f] retain];
  mComputedStyle = nullptr;
  mPresContext = nullptr;

  if (mListener) {
    mListener->IconUpdated();
  }

  mIconLoader->Destroy();

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}
