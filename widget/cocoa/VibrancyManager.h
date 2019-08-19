/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VibrancyManager_h
#define VibrancyManager_h

#include "mozilla/Assertions.h"
#include "nsClassHashtable.h"
#include "nsRegion.h"
#include "nsTArray.h"
#include "ViewRegion.h"

#import <Foundation/NSGeometry.h>

@class NSColor;
@class NSView;
class nsChildView;

namespace mozilla {

enum class VibrancyType {
  LIGHT,
  DARK,
  TOOLTIP,
  MENU,
  HIGHLIGHTED_MENUITEM,
  SHEET,
  SOURCE_LIST,
  SOURCE_LIST_SELECTION,
  ACTIVE_SOURCE_LIST_SELECTION
};

/**
 * VibrancyManager takes care of updating the vibrant regions of a window.
 * Vibrancy is a visual look that was introduced on OS X starting with 10.10.
 * An app declares vibrant window regions to the window server, and the window
 * server will display a blurred rendering of the screen contents from behind
 * the window in these areas, behind the actual window contents. Consequently,
 * the effect is only visible in areas where the window contents are not
 * completely opaque. Usually this is achieved by clearing the background of
 * the window prior to drawing in the vibrant areas. This is possible even if
 * the window is declared as opaque.
 */
class VibrancyManager {
 public:
  /**
   * Create a new VibrancyManager instance and provide it with an NSView
   * to attach NSVisualEffectViews to.
   *
   * @param aCoordinateConverter  The nsChildView to use for converting
   *   nsIntRect device pixel coordinates into Cocoa NSRect coordinates. Must
   *   outlive this VibrancyManager instance.
   * @param aContainerView  The view that's going to be the superview of the
   *   NSVisualEffectViews which will be created for vibrant regions.
   */
  VibrancyManager(const nsChildView& aCoordinateConverter, NSView* aContainerView)
      : mCoordinateConverter(aCoordinateConverter), mContainerView(aContainerView) {
    MOZ_ASSERT(SystemSupportsVibrancy(), "Don't instantiate this if !SystemSupportsVibrancy()");
  }

  /**
   * Update the placement of the NSVisualEffectViews inside the container
   * NSView so that they cover aRegion, and create new NSVisualEffectViews
   * or remove existing ones as needed.
   * @param aType   The vibrancy type to use in the region.
   * @param aRegion The vibrant area, in device pixels.
   * @return Whether the region changed.
   */
  bool UpdateVibrantRegion(VibrancyType aType, const LayoutDeviceIntRegion& aRegion);

  bool HasVibrantRegions() { return !mVibrantRegions.IsEmpty(); }

  LayoutDeviceIntRegion GetUnionOfVibrantRegions() const;

  /**
   * Return the fill color that should be drawn on top of the cleared window
   * parts. Usually this would be drawn by -[NSVisualEffectView drawRect:].
   * The returned color is opaque if the system-wide "Reduce transparency"
   * preference is set.
   */
  NSColor* VibrancyFillColorForType(VibrancyType aType);

  /**
   * Check whether the operating system supports vibrancy at all.
   * You may only create a VibrancyManager instance if this returns true.
   * @return Whether VibrancyManager can be used on this OS.
   */
  static bool SystemSupportsVibrancy();

  /**
   * Create an NSVisualEffectView for the specified vibrancy type. The return
   * value is not autoreleased. We return an object of type NSView* because we
   * compile with an SDK that does not contain a definition for
   * NSVisualEffectView.
   * @param aIsContainer Whether this NSView will have child views. This value
   *                     affects hit testing: Container views will pass through
   *                     hit testing requests to their children, and leaf views
   *                     will be transparent to hit testing.
   */
  static NSView* CreateEffectView(VibrancyType aType, BOOL aIsContainer = NO);

 protected:
  const nsChildView& mCoordinateConverter;
  NSView* mContainerView;
  nsClassHashtable<nsUint32HashKey, ViewRegion> mVibrantRegions;
};

}  // namespace mozilla

#endif  // VibrancyManager_h
