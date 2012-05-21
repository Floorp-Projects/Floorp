/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeThemeColors_h_
#define nsNativeThemeColors_h_

#include "nsCocoaFeatures.h"
#import <Cocoa/Cocoa.h>

extern "C" {
  typedef CFTypeRef CUIRendererRef;
  void CUIDraw(CUIRendererRef r, CGRect rect, CGContextRef ctx, CFDictionaryRef options, CFDictionaryRef* result);
}

@interface NSWindow(CoreUIRendererPrivate)
+ (CUIRendererRef)coreUIRenderer;
@end

enum ColorName {
  toolbarTopBorderGrey,
  toolbarFillGrey,
  toolbarBottomBorderGrey,
};

static const int sLeopardThemeColors[][2] = {
  /* { active window, inactive window } */
  // toolbar:
  { 0xC0, 0xE2 }, // top separator line
  { 0x96, 0xCA }, // fill color
  { 0x42, 0x89 }, // bottom separator line
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

__attribute__((unused))
static int NativeGreyColorAsInt(ColorName name, BOOL isMain)
{
  if (nsCocoaFeatures::OnLionOrLater())
    return sLionThemeColors[name][isMain ? 0 : 1];

  if (nsCocoaFeatures::OnSnowLeopardOrLater())
    return sSnowLeopardThemeColors[name][isMain ? 0 : 1];

  return sLeopardThemeColors[name][isMain ? 0 : 1];
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
