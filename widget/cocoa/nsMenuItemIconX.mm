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

nsMenuItemIconX::nsMenuItemIconX(nsMenuObjectX* aMenuItem, nsIContent* aContent,
                                 NSMenuItem* aNativeMenuItem)
    : mContent(aContent),
      mMenuObject(aMenuItem),
      mSetIcon(false),
      mNativeMenuItem(aNativeMenuItem) {
  MOZ_COUNT_CTOR(nsMenuItemIconX);
  MOZ_RELEASE_ASSERT(mContent);
}

nsMenuItemIconX::~nsMenuItemIconX() {
  if (mIconLoader) {
    mIconLoader->Destroy();
  }
  MOZ_COUNT_DTOR(nsMenuItemIconX);
}

nsresult nsMenuItemIconX::SetupIcon() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Still don't have one, then something is wrong, get out of here.
  if (!mNativeMenuItem) {
    NS_ERROR("No native menu item");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> iconURI = GetIconURI();
  if (!iconURI) {
    // There is no icon for this menu item. An icon might have been set
    // earlier.  Clear it.
    mNativeMenuItem.image = nil;

    return NS_OK;
  }

  if (!mIconLoader) {
    mIconLoader = new IconLoader(this);
  }
  if (!mSetIcon) {
    // Load placeholder icon.
    NSSize iconSize = NSMakeSize(kIconSize, kIconSize);
    mNativeMenuItem.image = [MOZIconHelper placeholderIconWithSize:iconSize];
  }

  nsresult rv = mIconLoader->LoadIcon(iconURI, mContent);
  if (NS_FAILED(rv)) {
    // There is no icon for this menu item, as an error occurred while loading it.
    // An icon might have been set earlier or the place holder icon may have
    // been set.  Clear it.
    mNativeMenuItem.image = nil;
  }

  mSetIcon = true;

  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

already_AddRefed<nsIURI> nsMenuItemIconX::GetIconURI() {
  // First, look at the content node's "image" attribute.
  nsAutoString imageURIString;
  bool hasImageAttr =
      mContent->IsElement() &&
      mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::image, imageURIString);

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
  RefPtr<mozilla::dom::Document> document = mContent->GetComposedDoc();
  if (!document || !mContent->IsElement()) {
    return nullptr;
  }

  RefPtr<ComputedStyle> sc = nsComputedDOMStyle::GetComputedStyle(mContent->AsElement(), nullptr);
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
  return iconURI.forget();
}

//
// mozilla::widget::IconLoader::Listener
//

nsresult nsMenuItemIconX::OnComplete(imgIContainer* aImage) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mNativeMenuItem) {
    mIconLoader->Destroy();
    return NS_ERROR_FAILURE;
  }

  NSImage* image = [MOZIconHelper iconImageFromImageContainer:aImage
                                                     withSize:NSMakeSize(kIconSize, kIconSize)
                                                      subrect:mImageRegionRect
                                                  scaleFactor:0.0f];
  mNativeMenuItem.image = image;
  if (mMenuObject) {
    mMenuObject->IconUpdated();
  }

  mIconLoader->Destroy();
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}
