/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "AppearanceOverride.h"

#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_widget.h"

#include "nsXULAppAPI.h"
#include "SDKDeclarations.h"

static void RespectSystemAppearancePrefChanged(const char* aPref, void* aUserInfo);

@interface MOZGlobalAppearance ()
@property BOOL shouldOverrideWithAqua;
@end

@implementation MOZGlobalAppearance

+ (MOZGlobalAppearance*)sharedInstance {
  static MOZGlobalAppearance* sInstance = nil;
  if (!sInstance) {
    sInstance = [[MOZGlobalAppearance alloc] init];
    if (XRE_IsParentProcess()) {
      mozilla::Preferences::RegisterCallbackAndCall(
          &RespectSystemAppearancePrefChanged,
          nsDependentCString(
              mozilla::StaticPrefs::GetPrefName_widget_macos_respect_system_appearance()));
    }
  }
  return sInstance;
}

+ (NSSet*)keyPathsForValuesAffectingAppearance {
  return [NSSet setWithObject:@"shouldOverrideWithAqua"];
}

- (NSAppearance*)appearance {
  if (self.shouldOverrideWithAqua) {
    // Override with aqua.
    return [NSAppearance appearanceNamed:NSAppearanceNameAqua];
  }
  // nil means "no override".
  return nil;
}

- (void)setAppearance:(NSAppearance*)aAppearance {
  // ignored
}

- (NSApplication*)_app {
  return NSApp;
}

+ (NSSet*)keyPathsForValuesAffectingEffectiveAppearance {
  if (@available(macOS 10.14, *)) {
    // Automatically notify any key-value observers of our effectiveAppearance property whenever the
    // pref or the NSApp's effectiveAppearance change.
    return [NSSet setWithObjects:@"shouldOverrideWithAqua", @"_app.effectiveAppearance", nil];
  }
  return [NSSet set];
}

- (NSAppearance*)effectiveAppearance {
  if (self.shouldOverrideWithAqua) {
    // Override with aqua.
    return [NSAppearance appearanceNamed:NSAppearanceNameAqua];
  }
  if (@available(macOS 10.14, *)) {
    // Use the NSApp effectiveAppearance. This is the system appearance.
    return NSApp.effectiveAppearance;
  }
  // Use aqua on pre-10.14.
  return [NSAppearance appearanceNamed:NSAppearanceNameAqua];
}

@end

static void RespectSystemAppearancePrefChanged(const char* aPref, void* aUserInfo) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  MOZGlobalAppearance.sharedInstance.shouldOverrideWithAqua =
      !mozilla::StaticPrefs::widget_macos_respect_system_appearance();
}
