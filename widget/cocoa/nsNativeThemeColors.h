/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeThemeColors_h_
#define nsNativeThemeColors_h_

#import <Cocoa/Cocoa.h>

#include "nsCocoaFeatures.h"
#include "SDKDeclarations.h"
#include "mozilla/ColorScheme.h"

enum ColorName {
  toolbarTopBorderGrey,
  toolbarFillGrey,
  toolbarBottomBorderGrey,
};

static const int sLionThemeColors[][2] = {
    /* { active window, inactive window } */
    // toolbar:
    {0xD0, 0xF0},  // top separator line
    {0xB2, 0xE1},  // fill color
    {0x59, 0x87},  // bottom separator line
};

static const int sYosemiteThemeColors[][2] = {
    /* { active window, inactive window } */
    // toolbar:
    {0xBD, 0xDF},  // top separator line
    {0xD3, 0xF6},  // fill color
    {0xB3, 0xD1},  // bottom separator line
};

inline int NativeGreyColorAsInt(ColorName name, BOOL isMain) {
  return sYosemiteThemeColors[name][isMain ? 0 : 1];
}

inline float NativeGreyColorAsFloat(ColorName name, BOOL isMain) {
  return NativeGreyColorAsInt(name, isMain) / 255.0f;
}

inline void DrawNativeGreyColorInRect(CGContextRef context, ColorName name, CGRect rect,
                                      BOOL isMain) {
  float grey = NativeGreyColorAsFloat(name, isMain);
  CGContextSetRGBFillColor(context, grey, grey, grey, 1.0f);
  CGContextFillRect(context, rect);
}

#if !defined(MAC_OS_X_VERSION_10_14) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_14
@interface NSColor (NSColorControlAccentColor)
@property(class, strong, readonly) NSColor* controlAccentColor NS_AVAILABLE_MAC(10_14);
@end
#endif

inline NSColor* ControlAccentColor() {
  if (@available(macOS 10.14, *)) {
    return [NSColor controlAccentColor];
  }

  // Pre-10.14, use hardcoded colors.
  return [NSColor currentControlTint] == NSGraphiteControlTint
             ? [NSColor colorWithSRGBRed:0.635 green:0.635 blue:0.655 alpha:1.0]
             : [NSColor colorWithSRGBRed:0.247 green:0.584 blue:0.965 alpha:1.0];
}

inline NSAppearance* NSAppearanceForColorScheme(mozilla::ColorScheme aScheme) {
  if (@available(macOS 10.14, *)) {
    NSAppearanceName appearanceName =
        aScheme == mozilla::ColorScheme::Light ? NSAppearanceNameAqua : NSAppearanceNameDarkAqua;
    return [NSAppearance appearanceNamed:appearanceName];
  }
  return [NSAppearance appearanceNamed:NSAppearanceNameAqua];
}

#endif  // nsNativeThemeColors_h_
