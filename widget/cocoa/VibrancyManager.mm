/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VibrancyManager.h"

#import <objc/message.h>

#include "nsChildView.h"

using namespace mozilla;

@interface MOZVibrantView : NSVisualEffectView {
  VibrancyType mType;
}
- (instancetype)initWithFrame:(NSRect)aRect
                 vibrancyType:(VibrancyType)aVibrancyType;
@end

@interface MOZVibrantLeafView : MOZVibrantView
@end

static NSVisualEffectState VisualEffectStateForVibrancyType(
    VibrancyType aType) {
  switch (aType) {
    case VibrancyType::TOOLTIP:
    case VibrancyType::MENU:
      // Tooltip and menu windows are never "key", so we need to tell the
      // vibrancy effect to look active regardless of window state.
      return NSVisualEffectStateActive;
    default:
      return NSVisualEffectStateFollowsWindowActiveState;
  }
}

static NSVisualEffectMaterial VisualEffectMaterialForVibrancyType(
    VibrancyType aType, BOOL* aOutIsEmphasized) {
  switch (aType) {
    case VibrancyType::TOOLTIP:
      return (NSVisualEffectMaterial)NSVisualEffectMaterialToolTip;
    case VibrancyType::MENU:
      return NSVisualEffectMaterialMenu;
  }
}

@implementation MOZVibrantView

- (instancetype)initWithFrame:(NSRect)aRect vibrancyType:(VibrancyType)aType {
  self = [super initWithFrame:aRect];
  mType = aType;

  self.appearance = nil;
  self.state = VisualEffectStateForVibrancyType(mType);

  BOOL isEmphasized = NO;
  self.material = VisualEffectMaterialForVibrancyType(mType, &isEmphasized);
  self.emphasized = isEmphasized;

  return self;
}

// Don't override allowsVibrancy here, because this view may have subviews, and
// returning YES from allowsVibrancy forces on foreground vibrancy for all
// descendant views, which can have unintended effects.

@end

@implementation MOZVibrantLeafView

- (NSView*)hitTest:(NSPoint)aPoint {
  // This view must be transparent to mouse events.
  return nil;
}

// MOZVibrantLeafView does not have subviews, so we can return YES here without
// having unintended effects on other contents of the window.
- (BOOL)allowsVibrancy {
  return NO;
}

@end

bool VibrancyManager::UpdateVibrantRegion(
    VibrancyType aType, const LayoutDeviceIntRegion& aRegion) {
  if (aRegion.IsEmpty()) {
    return mVibrantRegions.Remove(uint32_t(aType));
  }
  auto& vr = *mVibrantRegions.GetOrInsertNew(uint32_t(aType));
  return vr.UpdateRegion(aRegion, mCoordinateConverter, mContainerView, ^() {
    return this->CreateEffectView(aType);
  });
}

LayoutDeviceIntRegion VibrancyManager::GetUnionOfVibrantRegions() const {
  LayoutDeviceIntRegion result;
  for (const auto& region : mVibrantRegions.Values()) {
    result.OrWith(region->Region());
  }
  return result;
}

/* static */ NSView* VibrancyManager::CreateEffectView(VibrancyType aType,
                                                       BOOL aIsContainer) {
  return aIsContainer ? [[MOZVibrantView alloc] initWithFrame:NSZeroRect
                                                 vibrancyType:aType]
                      : [[MOZVibrantLeafView alloc] initWithFrame:NSZeroRect
                                                     vibrancyType:aType];
}
