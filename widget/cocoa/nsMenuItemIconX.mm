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

static const uint32_t kIconSize = 16;

nsMenuItemIconX::nsMenuItemIconX(nsMenuObjectX* aMenuItem, nsIContent* aContent,
                                 NSMenuItem* aNativeMenuItem)
    : mContent(aContent),
      mTriggeringPrincipal(aContent->NodePrincipal()),
      mContentType(nsIContentPolicy::TYPE_INTERNAL_IMAGE),
      mMenuObject(aMenuItem),
      mSetIcon(false),
      mNativeMenuItem(aNativeMenuItem) {
  MOZ_COUNT_CTOR(nsMenuItemIconX);
}

nsMenuItemIconX::~nsMenuItemIconX() {
  Destroy();
  MOZ_COUNT_DTOR(nsMenuItemIconX);
}

// Called from mMenuObjectX's destructor, to prevent us from outliving it
// (as might otherwise happen if calls to our imgINotificationObserver methods
// are still outstanding).  mMenuObjectX owns our nNativeMenuItem.
void nsMenuItemIconX::Destroy() {
  if (mIconLoader) {
    mIconLoader->Destroy();
    mIconLoader = nullptr;
  }

  mMenuObject = nullptr;
  mNativeMenuItem = nil;
}

nsresult nsMenuItemIconX::SetupIcon() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Still don't have one, then something is wrong, get out of here.
  if (!mNativeMenuItem) {
    NS_ERROR("No native menu item");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> iconURI;
  nsresult rv = GetIconURI(getter_AddRefs(iconURI));
  if (NS_FAILED(rv)) {
    // There is no icon for this menu item. An icon might have been set
    // earlier.  Clear it.
    [mNativeMenuItem setImage:nil];

    return NS_OK;
  }

  if (!mSetIcon) {
    // Set a completely transparent 16x16 image as the icon on this menu item
    // as a placeholder.  This keeps the menu item text displayed in the same
    // position that it will be displayed when the real icon is loaded, and
    // prevents it from jumping around or looking misaligned.

    static bool sInitializedPlaceholder;
    static NSImage* sPlaceholderIconImage;
    if (!sInitializedPlaceholder) {
      sInitializedPlaceholder = true;

      // Note that we only create the one and reuse it forever, so this is not a leak.
      sPlaceholderIconImage = [[NSImage alloc] initWithSize:NSMakeSize(kIconSize, kIconSize)];
    }

    if (!sPlaceholderIconImage) {
      return NS_ERROR_FAILURE;
    }

    if (mNativeMenuItem) {
      [mNativeMenuItem setImage:sPlaceholderIconImage];
    }
  }
  if (!mIconLoader) {
    mIconLoader = new nsIconLoaderService(mContent, &mImageRegionRect, this, kIconSize, kIconSize);
    if (!mIconLoader) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  rv = mIconLoader->LoadIcon(iconURI);
  if (NS_FAILED(rv)) {
    // There is no icon for this menu item, as an error occurred while loading it.
    // An icon might have been set earlier or the place holder icon may have
    // been set.  Clear it.
    [mNativeMenuItem setImage:nil];
  }

  mSetIcon = true;

  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult nsMenuItemIconX::GetIconURI(nsIURI** aIconURI) {
  if (!mMenuObject) return NS_ERROR_FAILURE;

  // Mac native menu items support having both a checkmark and an icon
  // simultaneously, but this is unheard of in the cross-platform toolkit,
  // seemingly because the win32 theme is unable to cope with both at once.
  // The downside is that it's possible to get a menu item marked with a
  // native checkmark and a checkmark for an icon.  Head off that possibility
  // by pretending that no icon exists if this is a checkable menu item.
  if (mMenuObject->MenuObjectType() == eMenuItemObjectType) {
    nsMenuItemX* menuItem = static_cast<nsMenuItemX*>(mMenuObject);
    if (menuItem->GetMenuItemType() != eRegularMenuItemType) return NS_ERROR_FAILURE;
  }

  if (!mContent) return NS_ERROR_FAILURE;

  // First, look at the content node's "image" attribute.
  nsAutoString imageURIString;
  bool hasImageAttr =
      mContent->IsElement() &&
      mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::image, imageURIString);

  nsresult rv;
  RefPtr<ComputedStyle> sc;
  nsCOMPtr<nsIURI> iconURI;
  if (!hasImageAttr) {
    // If the content node has no "image" attribute, get the
    // "list-style-image" property from CSS.
    RefPtr<mozilla::dom::Document> document = mContent->GetComposedDoc();
    if (!document || !mContent->IsElement()) {
      return NS_ERROR_FAILURE;
    }

    sc = nsComputedDOMStyle::GetComputedStyle(mContent->AsElement(), nullptr);
    if (!sc) {
      return NS_ERROR_FAILURE;
    }

    iconURI = sc->StyleList()->GetListStyleImageURI();
    if (!iconURI) {
      return NS_ERROR_FAILURE;
    }
  } else {
    uint64_t dummy = 0;
    nsContentUtils::GetContentPolicyTypeForUIImageLoading(
        mContent, getter_AddRefs(mTriggeringPrincipal), mContentType, &dummy);

    // If this menu item shouldn't have an icon, the string will be empty,
    // and NS_NewURI will fail.
    rv = NS_NewURI(getter_AddRefs(iconURI), imageURIString);
    if (NS_FAILED(rv)) return rv;
  }

  // Empty the mImageRegionRect initially as the image region CSS could
  // have been changed and now have an error or have been removed since the
  // last GetIconURI call.
  mImageRegionRect.SetEmpty();

  iconURI.forget(aIconURI);

  if (!hasImageAttr) {
    // Check if the icon has a specified image region so that it can be
    // cropped appropriately before being displayed.
    const nsRect r = sc->StyleList()->GetImageRegion();

    // Return NS_ERROR_FAILURE if the image region is invalid so the image
    // is not drawn, and behavior is similar to XUL menus.
    if (r.X() < 0 || r.Y() < 0 || r.Width() < 0 || r.Height() < 0) {
      return NS_ERROR_FAILURE;
    }

    // 'auto' is represented by a [0, 0, 0, 0] rect. Only set mImageRegionRect
    // if we have some other value.
    if (!r.IsEmpty()) {
      mImageRegionRect = r.ToNearestPixels(mozilla::AppUnitsPerCSSPixel());
    }
  }

  return NS_OK;
}

//
// nsIconLoaderObserver
//

nsresult nsMenuItemIconX::OnComplete(NSImage* aImage) {
  if (!mNativeMenuItem) {
    if (aImage) {
      [aImage release];
    }
    return NS_ERROR_FAILURE;
  }

  if (!aImage) {
    [mNativeMenuItem setImage:nil];
    return NS_OK;
  }

  [mNativeMenuItem setImage:aImage];
  if (mMenuObject) {
    mMenuObject->IconUpdated();
  }
  [aImage release];
  return NS_OK;
}
