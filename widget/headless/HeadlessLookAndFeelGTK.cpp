/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeadlessLookAndFeel.h"
#include "mozilla/FontPropertyTypes.h"
#include "nsIContent.h"

using mozilla::LookAndFeel;

namespace mozilla {
namespace widget {

static const char16_t UNICODE_BULLET = 0x2022;

HeadlessLookAndFeel::HeadlessLookAndFeel()
{
}

HeadlessLookAndFeel::~HeadlessLookAndFeel()
{
}

nsresult
HeadlessLookAndFeel::NativeGetColor(ColorID aID, nscolor& aColor)
{
  // For headless mode, we use GetStandinForNativeColor for everything we can,
  // and hardcoded values for everything else.

  nsresult res = NS_OK;

  switch (aID) {
    // Override the solid black that GetStandinForNativeColor provides for
    // -moz-FieldText, to match our behavior under the real GTK.
    case eColorID__moz_fieldtext:
      aColor = NS_RGB(0x21, 0x21, 0x21);
      break;

    // The rest are not provided by GetStandinForNativeColor.
    case eColorID_IMESelectedRawTextBackground:
    case eColorID_IMESelectedConvertedTextBackground:
    case eColorID_IMERawInputBackground:
    case eColorID_IMEConvertedTextBackground:
      aColor = NS_TRANSPARENT;
      break;
    case eColorID_IMESelectedRawTextForeground:
    case eColorID_IMESelectedConvertedTextForeground:
    case eColorID_IMERawInputForeground:
    case eColorID_IMEConvertedTextForeground:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case eColorID_IMERawInputUnderline:
    case eColorID_IMEConvertedTextUnderline:
      aColor = NS_40PERCENT_FOREGROUND_COLOR;
      break;
    case eColorID_IMESelectedRawTextUnderline:
    case eColorID_IMESelectedConvertedTextUnderline:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case eColorID__moz_eventreerow:
      aColor = NS_RGB(0xff,0xff,0xff);
      break;
    case eColorID__moz_gtk_info_bar_text:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;
    case eColorID__moz_mac_buttonactivetext:
    case eColorID__moz_mac_defaultbuttontext:
      aColor = NS_RGB(0xff,0xff,0xff);
      break;
    case eColorID_SpellCheckerUnderline:
      aColor = NS_RGB(0xff, 0x00, 0x00);
      break;
    case eColorID_TextBackground:
      aColor = NS_RGB(0xff,0xff,0xff);
      break;
    case eColorID_TextForeground:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;
    case eColorID_TextHighlightBackground:
      aColor = NS_RGB(0xef, 0x0f, 0xff);
      break;
    case eColorID_TextHighlightForeground:
      aColor = NS_RGB(0xff, 0xff, 0xff);
      break;
    case eColorID_TextSelectBackground:
      aColor = NS_RGB(0xaa,0xaa,0xaa);
      break;
    case eColorID_TextSelectBackgroundAttention:
      aColor = NS_TRANSPARENT;
      break;
    case eColorID_TextSelectBackgroundDisabled:
      aColor = NS_RGB(0xaa,0xaa,0xaa);
      break;
    case eColorID_TextSelectForeground:
      GetColor(eColorID_TextSelectBackground, aColor);
      if (aColor == 0x000000)
        aColor = NS_RGB(0xff,0xff,0xff);
      else
        aColor = NS_DONT_CHANGE_COLOR;
      break;
    case eColorID_Widget3DHighlight:
      aColor = NS_RGB(0xa0,0xa0,0xa0);
      break;
    case eColorID_Widget3DShadow:
      aColor = NS_RGB(0x40,0x40,0x40);
      break;
    case eColorID_WidgetBackground:
      aColor = NS_RGB(0xdd,0xdd,0xdd);
      break;
    case eColorID_WidgetForeground:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;
    case eColorID_WidgetSelectBackground:
      aColor = NS_RGB(0x80,0x80,0x80);
      break;
    case eColorID_WidgetSelectForeground:
      aColor = NS_RGB(0x00,0x00,0x80);
      break;
    case eColorID_WindowBackground:
      aColor = NS_RGB(0xff,0xff,0xff);
      break;
    case eColorID_WindowForeground:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;
    default:
      aColor = GetStandinForNativeColor(aID);
      break;
  }

  return res;
}

nsresult
HeadlessLookAndFeel::GetIntImpl(IntID aID, int32_t &aResult)
{
  nsresult res = nsXPLookAndFeel::GetIntImpl(aID, aResult);
  if (NS_SUCCEEDED(res))
    return res;
  res = NS_OK;

  // These values should be sane defaults for headless mode under GTK.
  switch (aID) {
    case eIntID_CaretBlinkTime:
      aResult = 567;
      break;
    case eIntID_CaretWidth:
      aResult = 1;
      break;
    case eIntID_ShowCaretDuringSelection:
      aResult = 0;
      break;
    case eIntID_SelectTextfieldsOnKeyFocus:
      aResult = 1;
      break;
    case eIntID_SubmenuDelay:
      aResult = 200;
      break;
    case eIntID_MenusCanOverlapOSBar:
      aResult = 0;
      break;
    case eIntID_UseOverlayScrollbars:
      aResult = 0;
      break;
    case eIntID_AllowOverlayScrollbarsOverlap:
      aResult = 0;
      break;
    case eIntID_ShowHideScrollbars:
      aResult = 0;
      break;
    case eIntID_SkipNavigatingDisabledMenuItem:
      aResult = 1;
      break;
    case eIntID_DragThresholdX:
    case eIntID_DragThresholdY:
      aResult = 4;
      break;
    case eIntID_UseAccessibilityTheme:
      aResult = 0;
      break;
    case eIntID_ScrollArrowStyle:
      aResult = eScrollArrow_None;
      break;
    case eIntID_ScrollSliderStyle:
      aResult = eScrollThumbStyle_Proportional;
      break;
    case eIntID_ScrollButtonLeftMouseButtonAction:
      aResult = 0;
      return NS_OK;
    case eIntID_ScrollButtonMiddleMouseButtonAction:
      aResult = 3;
      return NS_OK;
    case eIntID_ScrollButtonRightMouseButtonAction:
      aResult = 3;
      return NS_OK;
    case eIntID_TreeOpenDelay:
      aResult = 1000;
      break;
    case eIntID_TreeCloseDelay:
      aResult = 1000;
      break;
    case eIntID_TreeLazyScrollDelay:
      aResult = 150;
      break;
    case eIntID_TreeScrollDelay:
      aResult = 100;
      break;
    case eIntID_TreeScrollLinesMax:
      aResult = 3;
      break;
    case eIntID_TabFocusModel:
      aResult = nsIContent::eTabFocus_textControlsMask;
      break;
    case eIntID_ChosenMenuItemsShouldBlink:
      aResult = 1;
      break;
    case eIntID_WindowsAccentColorInTitlebar:
    case eIntID_WindowsDefaultTheme:
    case eIntID_DWMCompositor:
      aResult = 0;
      res = NS_ERROR_NOT_IMPLEMENTED;
      break;
    case eIntID_WindowsClassic:
    case eIntID_WindowsGlass:
      aResult = 0;
      res = NS_ERROR_FAILURE;
      break;
    case eIntID_TouchEnabled:
    case eIntID_MacGraphiteTheme:
    case eIntID_MacYosemiteTheme:
      aResult = 0;
      res = NS_ERROR_NOT_IMPLEMENTED;
      break;
    case eIntID_AlertNotificationOrigin:
      aResult = NS_ALERT_TOP;
      break;
    case eIntID_ScrollToClick:
      aResult = 0;
      break;
    case eIntID_IMERawInputUnderlineStyle:
    case eIntID_IMESelectedRawTextUnderlineStyle:
    case eIntID_IMEConvertedTextUnderlineStyle:
    case eIntID_IMESelectedConvertedTextUnderline:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_SOLID;
      break;
    case eIntID_SpellCheckerUnderlineStyle:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_DOTTED;
      break;
    case eIntID_MenuBarDrag:
      aResult = 0;
      break;
    case eIntID_WindowsThemeIdentifier:
    case eIntID_OperatingSystemVersionIdentifier:
      aResult = 0;
      res = NS_ERROR_NOT_IMPLEMENTED;
      break;
    case eIntID_ScrollbarButtonAutoRepeatBehavior:
      aResult = 0;
      break;
    case eIntID_TooltipDelay:
      aResult = 500;
      break;
    case eIntID_SwipeAnimationEnabled:
      aResult = 0;
      break;
    case eIntID_ScrollbarDisplayOnMouseMove:
      aResult = 0;
      break;
    case eIntID_ScrollbarFadeBeginDelay:
      aResult = 0;
      break;
    case eIntID_ScrollbarFadeDuration:
      aResult = 0;
      break;
    case eIntID_ContextMenuOffsetVertical:
      aResult = -6;
      break;
    case eIntID_ContextMenuOffsetHorizontal:
      aResult = 1;
      break;
    case eIntID_GTKCSDAvailable:
      aResult = 0;
      break;
    case eIntID_GTKCSDMinimizeButton:
      aResult = 0;
      break;
    case eIntID_GTKCSDMaximizeButton:
      aResult = 0;
      break;
    case eIntID_GTKCSDCloseButton:
      aResult = 1;
      break;
    default:
      NS_WARNING("HeadlessLookAndFeel::GetIntImpl called with an unrecognized aID");
      aResult = 0;
      res = NS_ERROR_FAILURE;
      break;
  }
  return res;
}

nsresult
HeadlessLookAndFeel::GetFloatImpl(FloatID aID, float &aResult)
{
  nsresult res = nsXPLookAndFeel::GetFloatImpl(aID, aResult);
  if (NS_SUCCEEDED(res))
    return res;
  res = NS_OK;

  // Hardcoded values for GTK.
  switch (aID) {
    case eFloatID_IMEUnderlineRelativeSize:
      aResult = 1.0f;
      break;
    case eFloatID_SpellCheckerUnderlineRelativeSize:
      aResult = 1.0f;
      break;
    case eFloatID_CaretAspectRatio:
      // Intentionally failing to quietly indicate lack of support.
      aResult = -1.0;
      res = NS_ERROR_FAILURE;
      break;
    default:
      NS_WARNING("HeadlessLookAndFeel::GetFloatImpl called with an unrecognized aID");
      aResult = -1.0;
      res = NS_ERROR_FAILURE;
      break;
  }

  return res;
}

bool
HeadlessLookAndFeel::GetFontImpl(FontID aID, nsString& aFontName,
               gfxFontStyle& aFontStyle,
               float aDevPixPerCSSPixel)
{
  // Default to san-serif for everything.
  aFontStyle.style      = NS_FONT_STYLE_NORMAL;
  aFontStyle.weight     = FontWeight::Normal();
  aFontStyle.stretch    = NS_FONT_STRETCH_NORMAL;
  aFontStyle.size       = 14 * aDevPixPerCSSPixel;
  aFontStyle.systemFont = true;

  aFontName.AssignLiteral("sans-serif");
  return true;
}

char16_t
HeadlessLookAndFeel::GetPasswordCharacterImpl()
{
  return UNICODE_BULLET;
}

void
HeadlessLookAndFeel::RefreshImpl()
{
  nsXPLookAndFeel::RefreshImpl();
}

bool
HeadlessLookAndFeel::GetEchoPasswordImpl() {
  return false;
}

} // namespace widget
} // namespace mozilla
