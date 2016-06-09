/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeThemeColors_h_
#define nsNativeThemeColors_h_

#include "nsCocoaFeatures.h"
#import <Cocoa/Cocoa.h>

enum ColorName {
  toolbarTopBorderGrey,
  toolbarFillGrey,
  toolbarBottomBorderGrey,
};

static const int sSnowLeopardThemeColors[][2] = {
  /* { active window, inactive window } */
  // toolbar:
  { 0xD0, 0xF1 }, // top separator line
  { 0xA7, 0xD8 }, // fill color
  { 0x51, 0x99 }, // bottom separator line
};

static const int sLionThemeColors[][2] = {
  /* { active window, inactive window } */
  // toolbar:
  { 0xD0, 0xF0 }, // top separator line
  { 0xB2, 0xE1 }, // fill color
  { 0x59, 0x87 }, // bottom separator line
};

static const int sYosemiteThemeColors[][2] = {
  /* { active window, inactive window } */
  // toolbar:
  { 0xBD, 0xDF }, // top separator line
  { 0xD3, 0xF6 }, // fill color
  { 0xB3, 0xD1 }, // bottom separator line
};

__attribute__((unused))
static int NativeGreyColorAsInt(ColorName name, BOOL isMain)
{
  if (nsCocoaFeatures::OnYosemiteOrLater())
    return sYosemiteThemeColors[name][isMain ? 0 : 1];

  if (nsCocoaFeatures::OnLionOrLater())
    return sLionThemeColors[name][isMain ? 0 : 1];

  return sSnowLeopardThemeColors[name][isMain ? 0 : 1];
}

__attribute__((unused))
static float NativeGreyColorAsFloat(ColorName name, BOOL isMain)
{
  return NativeGreyColorAsInt(name, isMain) / 255.0f;
}

__attribute__((unused))
static void DrawNativeGreyColorInRect(CGContextRef context, ColorName name,
                                      CGRect rect, BOOL isMain)
{
  float grey = NativeGreyColorAsFloat(name, isMain);
  CGContextSetRGBFillColor(context, grey, grey, grey, 1.0f);
  CGContextFillRect(context, rect);
}

#endif // nsNativeThemeColors_h_
