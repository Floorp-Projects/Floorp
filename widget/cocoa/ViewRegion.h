/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ViewRegion_h
#define ViewRegion_h

#include "Units.h"
#include "nsTArray.h"

class nsChildView;

@class NSView;

namespace mozilla {

/**
 * Manages a set of NSViews to cover a LayoutDeviceIntRegion.
 */
class ViewRegion {
 public:
  ~ViewRegion();

  mozilla::LayoutDeviceIntRegion Region() { return mRegion; }

  /**
   * Update the region.
   * @param aRegion  The new region.
   * @param aCoordinateConverter  The nsChildView to use for converting
   *   LayoutDeviceIntRect device pixel coordinates into Cocoa NSRect
   * coordinates.
   * @param aContainerView  The view that's going to be the superview of the
   *   NSViews which will be created for this region.
   * @param aViewCreationCallback  A block that instantiates new NSViews.
   * @return  Whether or not the region changed.
   */
  bool UpdateRegion(const mozilla::LayoutDeviceIntRegion& aRegion,
                    const nsChildView& aCoordinateConverter,
                    NSView* aContainerView, NSView* (^aViewCreationCallback)());

  /**
   * Return an NSView from the region, if there is any.
   */
  NSView* GetAnyView() { return mViews.Length() > 0 ? mViews[0] : NULL; }

 private:
  mozilla::LayoutDeviceIntRegion mRegion;
  nsTArray<NSView*> mViews;
};

}  // namespace mozilla

#endif  // ViewRegion_h
