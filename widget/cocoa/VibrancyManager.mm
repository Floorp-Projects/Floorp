/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VibrancyManager.h"

#include "nsChildView.h"
#include "nsCocoaFeatures.h"
#import <objc/message.h>

using namespace mozilla;

bool VibrancyManager::UpdateVibrantRegion(VibrancyType aType,
                                          const LayoutDeviceIntRegion& aRegion) {
  if (aRegion.IsEmpty()) {
    return mVibrantRegions.Remove(uint32_t(aType));
  }
  auto& vr = *mVibrantRegions.LookupOrAdd(uint32_t(aType));
  return vr.UpdateRegion(aRegion, mCoordinateConverter, mContainerView, ^() {
    return this->CreateEffectView(aType);
  });
}

@interface NSView (CurrentFillColor)
- (NSColor*)_currentFillColor;
@end

static NSColor* AdjustedColor(NSColor* aFillColor, VibrancyType aType) {
  if (aType == VibrancyType::MENU && [aFillColor alphaComponent] == 1.0) {
    // The opaque fill color that's used for the menu background when "Reduce
    // vibrancy" is checked in the system accessibility prefs is too dark.
    // This is probably because we're not using the right material for menus,
    // see VibrancyManager::CreateEffectView.
    return [NSColor colorWithDeviceWhite:0.96 alpha:1.0];
  }
  return aFillColor;
}

NSColor* VibrancyManager::VibrancyFillColorForType(VibrancyType aType) {
  NSView* view = mVibrantRegions.LookupOrAdd(uint32_t(aType))->GetAnyView();

  if (view && [view respondsToSelector:@selector(_currentFillColor)]) {
    // -[NSVisualEffectView _currentFillColor] is the color that the view
    // draws in its drawRect implementation.
    return AdjustedColor([view _currentFillColor], aType);
  }
  return [NSColor whiteColor];
}

static NSView* HitTestNil(id self, SEL _cmd, NSPoint aPoint) {
  // This view must be transparent to mouse events.
  return nil;
}

static BOOL AllowsVibrancyYes(id self, SEL _cmd) {
  // Means that the foreground is blended using a vibrant blend mode.
  return YES;
}

static Class CreateEffectViewClass(BOOL aForegroundVibrancy, BOOL aIsContainer) {
  // Create a class that inherits from NSVisualEffectView and overrides the
  // methods -[NSView hitTest:] and  -[NSVisualEffectView allowsVibrancy].
  Class NSVisualEffectViewClass = NSClassFromString(@"NSVisualEffectView");
  const char* className = aForegroundVibrancy ? "EffectViewWithForegroundVibrancy"
                                              : "EffectViewWithoutForegroundVibrancy";
  Class EffectViewClass = objc_allocateClassPair(NSVisualEffectViewClass, className, 0);
  if (!aIsContainer) {
    // Make this view transparent to mouse events.
    class_addMethod(EffectViewClass, @selector(hitTest:), (IMP)HitTestNil, "@@:{CGPoint=dd}");
  }
  if (aForegroundVibrancy) {
    // Override the -[NSView allowsVibrancy] method to return YES.
    class_addMethod(EffectViewClass, @selector(allowsVibrancy), (IMP)AllowsVibrancyYes, "I@:");
  }
  return EffectViewClass;
}

static id AppearanceForVibrancyType(VibrancyType aType) {
  Class NSAppearanceClass = NSClassFromString(@"NSAppearance");
  switch (aType) {
    case VibrancyType::LIGHT:
    case VibrancyType::TOOLTIP:
    case VibrancyType::MENU:
    case VibrancyType::HIGHLIGHTED_MENUITEM:
    case VibrancyType::SHEET:
    case VibrancyType::SOURCE_LIST:
    case VibrancyType::SOURCE_LIST_SELECTION:
    case VibrancyType::ACTIVE_SOURCE_LIST_SELECTION:
      return [NSAppearanceClass performSelector:@selector(appearanceNamed:)
                                     withObject:@"NSAppearanceNameVibrantLight"];
    case VibrancyType::DARK:
      return [NSAppearanceClass performSelector:@selector(appearanceNamed:)
                                     withObject:@"NSAppearanceNameVibrantDark"];
  }
}

#if !defined(MAC_OS_X_VERSION_10_10) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_10
enum {
  NSVisualEffectStateFollowsWindowActiveState,
  NSVisualEffectStateActive,
  NSVisualEffectStateInactive
};

enum { NSVisualEffectMaterialTitlebar = 3 };
#endif

#if !defined(MAC_OS_X_VERSION_10_11) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_11
enum { NSVisualEffectMaterialMenu = 5, NSVisualEffectMaterialSidebar = 7 };
#endif

static NSUInteger VisualEffectStateForVibrancyType(VibrancyType aType) {
  switch (aType) {
    case VibrancyType::TOOLTIP:
    case VibrancyType::MENU:
    case VibrancyType::HIGHLIGHTED_MENUITEM:
    case VibrancyType::SHEET:
      // Tooltip and menu windows are never "key" and sheets always looks
      // active, so we need to tell the vibrancy effect to look active
      // regardless of window state.
      return NSVisualEffectStateActive;
    default:
      return NSVisualEffectStateFollowsWindowActiveState;
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

#if !defined(MAC_OS_X_VERSION_10_12) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12
enum { NSVisualEffectMaterialSelection = 4 };
#endif

@interface NSView (NSVisualEffectViewMethods)
- (void)setState:(NSUInteger)state;
- (void)setMaterial:(NSUInteger)material;
- (void)setEmphasized:(BOOL)emphasized;
@end

/* static */ NSView* VibrancyManager::CreateEffectView(VibrancyType aType, BOOL aIsContainer) {
  static Class EffectViewWithoutForegroundVibrancy = CreateEffectViewClass(NO, NO);
  static Class EffectViewWithForegroundVibrancy = CreateEffectViewClass(YES, NO);
  static Class EffectViewContainer = CreateEffectViewClass(NO, YES);

  // Pick the right NSVisualEffectView subclass for the desired vibrancy mode.
  // For "container" views, never use foreground vibrancy, because returning
  // YES from allowsVibrancy forces on foreground vibrancy for all descendant
  // views which can have unintended effects.
  Class EffectViewClass = aIsContainer
                              ? EffectViewContainer
                              : (HasVibrantForeground(aType) ? EffectViewWithForegroundVibrancy
                                                             : EffectViewWithoutForegroundVibrancy);

  NSView* effectView = [[EffectViewClass alloc] initWithFrame:NSZeroRect];
  [effectView performSelector:@selector(setAppearance:)
                   withObject:AppearanceForVibrancyType(aType)];
  [effectView setState:VisualEffectStateForVibrancyType(aType)];

  BOOL canUseElCapitanMaterials = nsCocoaFeatures::OnElCapitanOrLater();
  if (aType == VibrancyType::MENU) {
    // Before 10.11 there is no material that perfectly matches the menu
    // look. Of all available material types, NSVisualEffectMaterialTitlebar
    // is the one that comes closest.
    [effectView setMaterial:canUseElCapitanMaterials ? NSVisualEffectMaterialMenu
                                                     : NSVisualEffectMaterialTitlebar];
  } else if (aType == VibrancyType::SOURCE_LIST && canUseElCapitanMaterials) {
    [effectView setMaterial:NSVisualEffectMaterialSidebar];
  } else if (aType == VibrancyType::HIGHLIGHTED_MENUITEM ||
             aType == VibrancyType::SOURCE_LIST_SELECTION ||
             aType == VibrancyType::ACTIVE_SOURCE_LIST_SELECTION) {
    [effectView setMaterial:NSVisualEffectMaterialSelection];
    if ([effectView respondsToSelector:@selector(setEmphasized:)] &&
        aType != VibrancyType::SOURCE_LIST_SELECTION) {
      [effectView setEmphasized:YES];
    }
  }

  return effectView;
}

static bool ComputeSystemSupportsVibrancy() {
#ifdef __x86_64__
  return NSClassFromString(@"NSAppearance") && NSClassFromString(@"NSVisualEffectView");
#else
  // objc_allocateClassPair doesn't work in 32 bit mode, so turn off vibrancy.
  return false;
#endif
}

/* static */ bool VibrancyManager::SystemSupportsVibrancy() {
  static bool supportsVibrancy = ComputeSystemSupportsVibrancy();
  return supportsVibrancy;
}
