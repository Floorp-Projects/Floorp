/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VibrancyManager.h"

#include "nsChildView.h"
#include "nsCocoaFeatures.h"
#import <objc/message.h>

#if !defined(MAC_OS_X_VERSION_10_14) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_14
enum {
  NSVisualEffectMaterialToolTip NS_ENUM_AVAILABLE_MAC(10_14) = 17,
};
#endif

using namespace mozilla;

@interface MOZVibrantView : NSVisualEffectView {
  VibrancyType mType;
}
- (instancetype)initWithFrame:(NSRect)aRect vibrancyType:(VibrancyType)aVibrancyType;
@end

@interface MOZVibrantLeafView : MOZVibrantView
@end

static NSAppearance* AppearanceForVibrancyType(VibrancyType aType) {
  if (@available(macOS 10.14, *)) {
    switch (aType) {
      case VibrancyType::TITLEBAR_LIGHT:
        // This must always be light (regular aqua), regardless of window appearance.
        return [NSAppearance appearanceNamed:NSAppearanceNameAqua];
      case VibrancyType::TITLEBAR_DARK:
        // This must always be dark (dark aqua), regardless of window appearance.
        return [NSAppearance appearanceNamed:@"NSAppearanceNameDarkAqua"];
      case VibrancyType::TOOLTIP:
      case VibrancyType::MENU:
      case VibrancyType::HIGHLIGHTED_MENUITEM:
      case VibrancyType::SOURCE_LIST:
      case VibrancyType::SOURCE_LIST_SELECTION:
      case VibrancyType::ACTIVE_SOURCE_LIST_SELECTION:
        // Inherit the appearance from the window. If the window is using Dark Mode, the vibrancy
        // will automatically be dark, too. This is available starting with macOS 10.14.
        return nil;
    }
  }

  // For 10.13 and below, a vibrant appearance name must be used. There is no system dark mode and
  // no automatic adaptation to the window; all windows are light.
  switch (aType) {
    case VibrancyType::TITLEBAR_LIGHT:
    case VibrancyType::TOOLTIP:
    case VibrancyType::MENU:
    case VibrancyType::HIGHLIGHTED_MENUITEM:
    case VibrancyType::SOURCE_LIST:
    case VibrancyType::SOURCE_LIST_SELECTION:
    case VibrancyType::ACTIVE_SOURCE_LIST_SELECTION:
      return [NSAppearance appearanceNamed:NSAppearanceNameVibrantLight];
    case VibrancyType::TITLEBAR_DARK:
      return [NSAppearance appearanceNamed:NSAppearanceNameVibrantDark];
  }
}

static NSVisualEffectState VisualEffectStateForVibrancyType(VibrancyType aType) {
  switch (aType) {
    case VibrancyType::TOOLTIP:
    case VibrancyType::MENU:
    case VibrancyType::HIGHLIGHTED_MENUITEM:
      // Tooltip and menu windows are never "key", so we need to tell the vibrancy effect to look
      // active regardless of window state.
      return NSVisualEffectStateActive;
    default:
      return NSVisualEffectStateFollowsWindowActiveState;
  }
}

static NSVisualEffectMaterial VisualEffectMaterialForVibrancyType(VibrancyType aType,
                                                                  BOOL* aOutIsEmphasized) {
  switch (aType) {
    case VibrancyType::TITLEBAR_LIGHT:
    case VibrancyType::TITLEBAR_DARK:
      return NSVisualEffectMaterialTitlebar;
    case VibrancyType::TOOLTIP:
      if (@available(macOS 10.14, *)) {
        return (NSVisualEffectMaterial)NSVisualEffectMaterialToolTip;
      } else {
        return NSVisualEffectMaterialMenu;
      }
    case VibrancyType::MENU:
      return NSVisualEffectMaterialMenu;
    case VibrancyType::SOURCE_LIST:
      return NSVisualEffectMaterialSidebar;
    case VibrancyType::SOURCE_LIST_SELECTION:
      return NSVisualEffectMaterialSelection;
    case VibrancyType::HIGHLIGHTED_MENUITEM:
    case VibrancyType::ACTIVE_SOURCE_LIST_SELECTION:
      *aOutIsEmphasized = YES;
      return NSVisualEffectMaterialSelection;
  }
}

static BOOL HasVibrantForeground(VibrancyType aType) {
  switch (aType) {
    case VibrancyType::MENU:
      return YES;
    default:
      return NO;
  }
}

@implementation MOZVibrantView

- (instancetype)initWithFrame:(NSRect)aRect vibrancyType:(VibrancyType)aType {
  self = [super initWithFrame:aRect];
  mType = aType;

  self.appearance = AppearanceForVibrancyType(mType);
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
  return HasVibrantForeground(mType);
}

@end

bool VibrancyManager::UpdateVibrantRegion(VibrancyType aType,
                                          const LayoutDeviceIntRegion& aRegion) {
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

/* static */ NSView* VibrancyManager::CreateEffectView(VibrancyType aType, BOOL aIsContainer) {
  return aIsContainer ? [[MOZVibrantView alloc] initWithFrame:NSZeroRect vibrancyType:aType]
                      : [[MOZVibrantLeafView alloc] initWithFrame:NSZeroRect vibrancyType:aType];
}
