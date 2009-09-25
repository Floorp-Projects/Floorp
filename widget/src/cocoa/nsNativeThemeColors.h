/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Markus Stange.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsNativeThemeColors_h_
#define nsNativeThemeColors_h_

#include "nsToolkit.h"
#import <Cocoa/Cocoa.h>

enum ColorName {
  headerStartGrey,
  headerEndGrey,
  headerBorderGrey,
  toolbarTopBorderGrey,
  statusbarFirstTopBorderGrey,
  statusbarSecondTopBorderGrey,
  statusbarGradientStartGrey,
  statusbarGradientEndGrey
};

static const int sLeopardThemeColors[][2] = {
  /* { active window, inactive window } */
  // titlebar and toolbar:
  { 0xC5, 0xE9 }, // start grey
  { 0x96, 0xCA }, // end grey
  { 0x42, 0x89 }, // bottom separator line
  { 0xC0, 0xE2 }, // top separator line
  // statusbar:
  { 0x42, 0x86 }, // first top border
  { 0xD8, 0xEE }, // second top border
  { 0xBD, 0xE4 }, // gradient start
  { 0x96, 0xCF }  // gradient end
};

static const int sSnowLeopardThemeColors[][2] = {
  /* { active window, inactive window } */
  // titlebar and toolbar:
  { 0xD1, 0xEE }, // start grey
  { 0xA7, 0xD8 }, // end grey
  { 0x51, 0x99 }, // bottom separator line
  { 0xD0, 0xF1 }, // top separator line
  // statusbar:
  { 0x51, 0x99 }, // first top border
  { 0xE8, 0xF6 }, // second top border
  { 0xCB, 0xEA }, // gradient start
  { 0xA7, 0xDE }  // gradient end
};

static int NativeGreyColorAsInt(ColorName name, BOOL isMain)
{
  if (nsToolkit::OnSnowLeopardOrLater())
    return sSnowLeopardThemeColors[name][isMain ? 0 : 1];

  return sLeopardThemeColors[name][isMain ? 0 : 1];
}

static float NativeGreyColorAsFloat(ColorName name, BOOL isMain)
{
  return NativeGreyColorAsInt(name, isMain) / 255.0f;
}

static void DrawNativeGreyColorInRect(CGContextRef context, ColorName name,
                                      CGRect rect, BOOL isMain)
{
  float grey = NativeGreyColorAsFloat(name, isMain);
  CGContextSetRGBFillColor(context, grey, grey, grey, 1.0f);
  CGContextFillRect(context, rect);
}

#endif // nsNativeThemeColors_h_
