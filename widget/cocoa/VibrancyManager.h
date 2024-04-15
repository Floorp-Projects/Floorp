/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VibrancyManager_h
#define VibrancyManager_h

#include "mozilla/EnumeratedArray.h"
#include "Units.h"

@class NSView;
class nsChildView;

namespace mozilla {

class ViewRegion;

enum class VibrancyType {
  // Add new values here, or update MaxEnumValue below if you add them after.
  Titlebar,
};

template <>
struct MaxContiguousEnumValue<VibrancyType> {
  static constexpr auto value = VibrancyType::Titlebar;
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
  VibrancyManager(const nsChildView& aCoordinateConverter,
                  NSView* aContainerView);

  ~VibrancyManager();

  /**
   * Update the placement of the NSVisualEffectViews inside the container
   * NSView so that they cover aRegion, and create new NSVisualEffectViews
   * or remove existing ones as needed.
   * @param aType   The vibrancy type to use in the region.
   * @param aRegion The vibrant area, in device pixels.
   * @return Whether the region changed.
   */
  bool UpdateVibrantRegion(VibrancyType aType,
                           const LayoutDeviceIntRegion& aRegion);

 protected:
  const nsChildView& mCoordinateConverter;
  NSView* mContainerView;
  EnumeratedArray<VibrancyType, UniquePtr<ViewRegion>> mVibrantRegions;
};

}  // namespace mozilla

#endif  // VibrancyManager_h
