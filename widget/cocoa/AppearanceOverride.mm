/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "AppearanceOverride.h"
#include "nsNativeThemeColors.h"
#include "nsXULAppAPI.h"
#include "nsThreadUtils.h"

@implementation MOZGlobalAppearance
NSAppearance* mAppearance;

+ (MOZGlobalAppearance*)sharedInstance {
  static MOZGlobalAppearance* sInstance = nil;
  if (!sInstance) {
    sInstance = [[MOZGlobalAppearance alloc] init];
  }
  return sInstance;
}

- (NSAppearance*)appearance {
  return mAppearance;
}

- (void)setAppearance:(NSAppearance*)aAppearance {
  mAppearance = aAppearance;
}

- (NSApplication*)_app {
  return NSApp;
}

+ (NSSet*)keyPathsForValuesAffectingEffectiveAppearance {
  // Automatically notify any key-value observers of our effectiveAppearance
  // property whenever appearance or the NSApp's effectiveAppearance change.
  return [NSSet setWithObjects:@"appearance", @"_app.effectiveAppearance", nil];
}

- (NSAppearance*)effectiveAppearance {
  return mAppearance ? mAppearance : NSApp.effectiveAppearance;
}

@end

void OverrideGlobalAppearance(mozilla::ColorScheme aScheme) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!XRE_IsParentProcess()) {
    return;
  }

  auto* inst = MOZGlobalAppearance.sharedInstance;
  auto* appearance = NSAppearanceForColorScheme(aScheme);
  if (inst.appearance != appearance) {
    inst.appearance = appearance;
  }
}
