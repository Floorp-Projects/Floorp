/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "AppearanceOverride.h"

#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_widget.h"

#include "nsXULAppAPI.h"

static void ToolbarThemePrefChanged(const char* aPref, void* aUserInfo);

@interface MOZGlobalAppearance ()
@property NSInteger toolbarTheme;
@end

@implementation MOZGlobalAppearance

+ (MOZGlobalAppearance*)sharedInstance {
  static MOZGlobalAppearance* sInstance = nil;
  if (!sInstance) {
    sInstance = [[MOZGlobalAppearance alloc] init];
    if (XRE_IsParentProcess()) {
      mozilla::Preferences::RegisterCallbackAndCall(
          &ToolbarThemePrefChanged,
          nsDependentCString(
              mozilla::StaticPrefs::GetPrefName_browser_theme_toolbar_theme()));
    }
  }
  return sInstance;
}

+ (NSSet*)keyPathsForValuesAffectingAppearance {
  return [NSSet setWithObjects:@"toolbarTheme", nil];
}

- (NSAppearance*)appearance {
  switch (self.toolbarTheme) {  // Value for browser.theme.toolbar-theme pref
    case 0:                     // Dark
      return [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    case 1:  // Light
      return [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    case 2:  // System
    default:
      return nil;  // nil means "no override".
  }
}

- (void)setAppearance:(NSAppearance*)aAppearance {
  // ignored
}

- (NSApplication*)_app {
  return NSApp;
}

+ (NSSet*)keyPathsForValuesAffectingEffectiveAppearance {
  // Automatically notify any key-value observers of our effectiveAppearance
  // property whenever the pref or the NSApp's effectiveAppearance change.
  return
      [NSSet setWithObjects:@"toolbarTheme", @"_app.effectiveAppearance", nil];
}

- (NSAppearance*)effectiveAppearance {
  switch (self.toolbarTheme) {  // Value for browser.theme.toolbar-theme pref
    case 0:                     // Dark
      return [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    case 1:  // Light
      return [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    case 2:  // System
    default:
      // Use the NSApp effectiveAppearance. This is the system appearance.
      return NSApp.effectiveAppearance;
  }
}

@end

static void ToolbarThemePrefChanged(const char* aPref, void* aUserInfo) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  MOZGlobalAppearance.sharedInstance.toolbarTheme =
      mozilla::StaticPrefs::browser_theme_toolbar_theme();
}
