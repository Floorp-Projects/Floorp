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

static Class
CreateEffectViewClass()
{
  // Create a class called EffectView that inherits from NSVisualEffectView
  // and overrides the methods -[NSVisualEffectView drawRect:] and
  // -[NSView hitTest:].
  Class NSVisualEffectViewClass = NSClassFromString(@"NSVisualEffectView");
  Class EffectViewClass = objc_allocateClassPair(NSVisualEffectViewClass, "EffectView", 0);
  class_addMethod(EffectViewClass, @selector(drawRect:), (IMP)DrawRectNothing,
                  "v@:{CGRect={CGPoint=dd}{CGSize=dd}}");
  class_addMethod(EffectViewClass, @selector(hitTest:), (IMP)HitTestNil,
                  "@@:{CGPoint=dd}");
  return EffectViewClass;
}

static id
AppearanceForVibrancyType(VibrancyType aType)
{
  Class NSAppearanceClass = NSClassFromString(@"NSAppearance");
  switch (aType) {
    case VibrancyType::LIGHT:
      return [NSAppearanceClass performSelector:@selector(appearanceNamed:)
                                     withObject:@"NSAppearanceNameVibrantLight"];
    case VibrancyType::DARK:
      return [NSAppearanceClass performSelector:@selector(appearanceNamed:)
                                     withObject:@"NSAppearanceNameVibrantDark"];
  }
}

NSView*
VibrancyManager::CreateEffectView(VibrancyType aType, NSRect aRect)
{
  static Class EffectViewClass = CreateEffectViewClass();
  NSView* effectView = [[EffectViewClass alloc] initWithFrame:aRect];
  [effectView performSelector:@selector(setAppearance:)
                   withObject:AppearanceForVibrancyType(aType)];
  return effectView;
}

static bool
ComputeSystemSupportsVibrancy()
{
  return NSClassFromString(@"NSAppearance") &&
      NSClassFromString(@"NSVisualEffectView");
}

/* static */ bool
VibrancyManager::SystemSupportsVibrancy()
{
  static bool supportsVibrancy = ComputeSystemSupportsVibrancy();
  return supportsVibrancy;
}
