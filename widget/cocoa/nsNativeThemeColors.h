/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeThemeColors_h_
#define nsNativeThemeColors_h_

#import <Cocoa/Cocoa.h>

#include "mozilla/ColorScheme.h"

inline NSAppearance* NSAppearanceForColorScheme(mozilla::ColorScheme aScheme) {
  NSAppearanceName appearanceName = aScheme == mozilla::ColorScheme::Light
                                        ? NSAppearanceNameAqua
                                        : NSAppearanceNameDarkAqua;
  return [NSAppearance appearanceNamed:appearanceName];
}

#endif  // nsNativeThemeColors_h_
