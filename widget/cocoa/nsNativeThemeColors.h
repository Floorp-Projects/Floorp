/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeThemeColors_h_
#define nsNativeThemeColors_h_

#import <Cocoa/Cocoa.h>

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

inline void DrawNativeGreyColorInRect(CGContextRef context, ColorName name,
                                      CGRect rect, BOOL isMain) {
  float grey = NativeGreyColorAsFloat(name, isMain);
  CGContextSetRGBFillColor(context, grey, grey, grey, 1.0f);
  CGContextFillRect(context, rect);
}

inline NSAppearance* NSAppearanceForColorScheme(mozilla::ColorScheme aScheme) {
  NSAppearanceName appearanceName = aScheme == mozilla::ColorScheme::Light
                                        ? NSAppearanceNameAqua
                                        : NSAppearanceNameDarkAqua;
  return [NSAppearance appearanceNamed:appearanceName];
}

#endif  // nsNativeThemeColors_h_
