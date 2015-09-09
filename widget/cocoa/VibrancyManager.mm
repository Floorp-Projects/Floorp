/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VibrancyManager.h"
#include "nsChildView.h"
#import <objc/message.h>

using namespace mozilla;

void
VibrancyManager::UpdateVibrantRegion(VibrancyType aType, const nsIntRegion& aRegion)
{
  auto& vr = *mVibrantRegions.LookupOrAdd(uint32_t(aType));
  if (vr.region == aRegion) {
    return;
  }

  // We need to construct the required region using as many EffectViews
  // as necessary. We try to update the geometry of existing views if
  // possible, or create new ones or remove old ones if the number of
  // rects in the region has changed.

  nsTArray<NSView*> viewsToRecycle;
  vr.effectViews.SwapElements(viewsToRecycle);
  // vr.effectViews is now empty.

  nsIntRegionRectIterator iter(aRegion);
  const nsIntRect* iterRect = nullptr;
  for (size_t i = 0; (iterRect = iter.Next()) || i < viewsToRecycle.Length(); ++i) {
    if (iterRect) {
      NSView* view = nil;
      NSRect rect = mCoordinateConverter.DevPixelsToCocoaPoints(*iterRect);
      if (i < viewsToRecycle.Length()) {
        view = viewsToRecycle[i];
        [view setFrame:rect];
      } else {
        view = CreateEffectView(aType, rect);
        [mContainerView addSubview:view];

        // Now that the view is in the view hierarchy, it'll be kept alive by
        // its superview, so we can drop our reference.
        [view release];
      }
      vr.effectViews.AppendElement(view);
    } else {
      // Our new region is made of less rects than the old region, so we can
      // remove this view. We only have a weak reference to it, so removing it
      // from the view hierarchy will release it.
      [viewsToRecycle[i] removeFromSuperview];
    }
  }

  vr.region = aRegion;
}

static PLDHashOperator
ClearVibrantRegionFunc(const uint32_t& aVibrancyType,
                       VibrancyManager::VibrantRegion* aVibrantRegion,
                       void* aVM)
{
  static_cast<VibrancyManager*>(aVM)->ClearVibrantRegion(*aVibrantRegion);
  return PL_DHASH_NEXT;
}

void
VibrancyManager::ClearVibrantAreas() const
{
  mVibrantRegions.EnumerateRead(ClearVibrantRegionFunc,
                                const_cast<VibrancyManager*>(this));
}

void
VibrancyManager::ClearVibrantRegion(const VibrantRegion& aVibrantRegion) const
{
  [[NSColor clearColor] set];

  nsIntRegionRectIterator iter(aVibrantRegion.region);
  while (const nsIntRect* rect = iter.Next()) {
    NSRectFill(mCoordinateConverter.DevPixelsToCocoaPoints(*rect));
  }
}

@interface NSView(CurrentFillColor)
- (NSColor*)_currentFillColor;
@end

static NSColor*
AdjustedColor(NSColor* aFillColor, VibrancyType aType)
{
  if (aType == VibrancyType::MENU && [aFillColor alphaComponent] == 1.0) {
    // The opaque fill color that's used for the menu background when "Reduce
    // vibrancy" is checked in the system accessibility prefs is too dark.
    // This is probably because we're not using the right material for menus,
    // see VibrancyManager::CreateEffectView.
    return [NSColor colorWithDeviceWhite:0.96 alpha:1.0];
  }
  return aFillColor;
}

NSColor*
VibrancyManager::VibrancyFillColorForType(VibrancyType aType)
{
  const nsTArray<NSView*>& views =
    mVibrantRegions.LookupOrAdd(uint32_t(aType))->effectViews;

  if (!views.IsEmpty() &&
      [views[0] respondsToSelector:@selector(_currentFillColor)]) {
    // -[NSVisualEffectView _currentFillColor] is the color that our view
    // would draw during its drawRect implementation, if we hadn't
    // disabled that.
    return AdjustedColor([views[0] _currentFillColor], aType);
  }
  return [NSColor whiteColor];
}

@interface NSView(FontSmoothingBackgroundColor)
- (NSColor*)fontSmoothingBackgroundColor;
@end

NSColor*
VibrancyManager::VibrancyFontSmoothingBackgroundColorForType(VibrancyType aType)
{
  const nsTArray<NSView*>& views =
    mVibrantRegions.LookupOrAdd(uint32_t(aType))->effectViews;

  if (!views.IsEmpty() &&
      [views[0] respondsToSelector:@selector(fontSmoothingBackgroundColor)]) {
    return [views[0] fontSmoothingBackgroundColor];
  }
  return [NSColor clearColor];
}

static void
DrawRectNothing(id self, SEL _cmd, NSRect aRect)
{
  // The super implementation would clear the background.
  // That's fine for views that are placed below their content, but our
  // setup is different: Our drawn content is drawn to mContainerView, which
  // sits below this EffectView. So we must not clear the background here,
  // because we'd erase that drawn content.
  // Of course the regular content drawing still needs to clear the background
  // behind vibrant areas. This is taken care of by having nsNativeThemeCocoa
  // return true from NeedToClearBackgroundBehindWidget for vibrant widgets.
}

static NSView*
HitTestNil(id self, SEL _cmd, NSPoint aPoint)
{
  // This view must be transparent to mouse events.
  return nil;
}

static BOOL
AllowsVibrancyYes(id self, SEL _cmd)
{
  // Means that the foreground is blended using a vibrant blend mode.
  return YES;
}

static Class
CreateEffectViewClass(BOOL aForegroundVibrancy)
{
  // Create a class called EffectView that inherits from NSVisualEffectView
  // and overrides the methods -[NSVisualEffectView drawRect:] and
  // -[NSView hitTest:].
  Class NSVisualEffectViewClass = NSClassFromString(@"NSVisualEffectView");
  const char* className = aForegroundVibrancy
    ? "EffectViewWithForegroundVibrancy" : "EffectViewWithoutForegroundVibrancy";
  Class EffectViewClass = objc_allocateClassPair(NSVisualEffectViewClass, className, 0);
  class_addMethod(EffectViewClass, @selector(drawRect:), (IMP)DrawRectNothing,
                  "v@:{CGRect={CGPoint=dd}{CGSize=dd}}");
  class_addMethod(EffectViewClass, @selector(hitTest:), (IMP)HitTestNil,
                  "@@:{CGPoint=dd}");
  if (aForegroundVibrancy) {
    // Also override the -[NSView allowsVibrancy] method to return YES.
    class_addMethod(EffectViewClass, @selector(allowsVibrancy), (IMP)AllowsVibrancyYes, "I@:");
  }
  return EffectViewClass;
}

static id
AppearanceForVibrancyType(VibrancyType aType)
{
  Class NSAppearanceClass = NSClassFromString(@"NSAppearance");
  switch (aType) {
    case VibrancyType::LIGHT:
    case VibrancyType::TOOLTIP:
    case VibrancyType::MENU:
    case VibrancyType::HIGHLIGHTED_MENUITEM:
    case VibrancyType::SHEET:
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

enum {
  NSVisualEffectMaterialTitlebar = 3
};
#endif

#if !defined(MAC_OS_X_VERSION_10_11) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_11
enum {
  NSVisualEffectMaterialMenu = 5
};
#endif

static NSUInteger
VisualEffectStateForVibrancyType(VibrancyType aType)
{
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

static BOOL
HasVibrantForeground(VibrancyType aType)
{
  switch (aType) {
    case VibrancyType::MENU:
      return YES;
    default:
      return NO;
  }
}

enum {
  NSVisualEffectMaterialMenuItem = 4
};

@interface NSView(NSVisualEffectViewMethods)
- (void)setState:(NSUInteger)state;
- (void)setMaterial:(NSUInteger)material;
- (void)setEmphasized:(BOOL)emphasized;
@end

NSView*
VibrancyManager::CreateEffectView(VibrancyType aType, NSRect aRect)
{
  static Class EffectViewClassWithoutForegroundVibrancy = CreateEffectViewClass(NO);
  static Class EffectViewClassWithForegroundVibrancy = CreateEffectViewClass(YES);

  Class EffectViewClass = HasVibrantForeground(aType)
    ? EffectViewClassWithForegroundVibrancy : EffectViewClassWithoutForegroundVibrancy;
  NSView* effectView = [[EffectViewClass alloc] initWithFrame:aRect];
  [effectView performSelector:@selector(setAppearance:)
                   withObject:AppearanceForVibrancyType(aType)];
  [effectView setState:VisualEffectStateForVibrancyType(aType)];

  if (aType == VibrancyType::MENU) {
    if (nsCocoaFeatures::OnElCapitanOrLater()) {
      [effectView setMaterial:NSVisualEffectMaterialMenu];
    } else {
      // Before 10.11 there is no material that perfectly matches the menu
      // look. Of all available material types, NSVisualEffectMaterialTitlebar
      // is the one that comes closest.
      [effectView setMaterial:NSVisualEffectMaterialTitlebar];
    }
  } else if (aType == VibrancyType::HIGHLIGHTED_MENUITEM) {
    [effectView setMaterial:NSVisualEffectMaterialMenuItem];
    if ([effectView respondsToSelector:@selector(setEmphasized:)]) {
      [effectView setEmphasized:YES];
    }
  }

  return effectView;
}

static bool
ComputeSystemSupportsVibrancy()
{
#ifdef __x86_64__
  return NSClassFromString(@"NSAppearance") &&
      NSClassFromString(@"NSVisualEffectView");
#else
  // objc_allocateClassPair doesn't work in 32 bit mode, so turn off vibrancy.
  return false;
#endif
}

/* static */ bool
VibrancyManager::SystemSupportsVibrancy()
{
  static bool supportsVibrancy = ComputeSystemSupportsVibrancy();
  return supportsVibrancy;
}
