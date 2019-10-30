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

HeadlessLookAndFeel::HeadlessLookAndFeel() {}

HeadlessLookAndFeel::~HeadlessLookAndFeel() {}

nsresult HeadlessLookAndFeel::NativeGetColor(ColorID aID, nscolor& aColor) {
  // For headless mode, we use GetStandinForNativeColor for everything we can,
  // and hardcoded values for everything else.

  nsresult res = NS_OK;

  switch (aID) {
    // Override the solid black that GetStandinForNativeColor provides for
    // -moz-FieldText, to match our behavior under the real GTK.
    case ColorID::MozFieldtext:
      aColor = NS_RGB(0x21, 0x21, 0x21);
      break;

    // The rest are not provided by GetStandinForNativeColor.
    case ColorID::IMESelectedRawTextBackground:
    case ColorID::IMESelectedConvertedTextBackground:
    case ColorID::IMERawInputBackground:
    case ColorID::IMEConvertedTextBackground:
      aColor = NS_TRANSPARENT;
      break;
    case ColorID::IMESelectedRawTextForeground:
    case ColorID::IMESelectedConvertedTextForeground:
    case ColorID::IMERawInputForeground:
    case ColorID::IMEConvertedTextForeground:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::IMERawInputUnderline:
    case ColorID::IMEConvertedTextUnderline:
      aColor = NS_40PERCENT_FOREGROUND_COLOR;
      break;
    case ColorID::IMESelectedRawTextUnderline:
    case ColorID::IMESelectedConvertedTextUnderline:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::MozEventreerow:
      aColor = NS_RGB(0xff, 0xff, 0xff);
      break;
    case ColorID::MozGtkInfoBarText:
      aColor = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::MozMacButtonactivetext:
    case ColorID::MozMacDefaultbuttontext:
      aColor = NS_RGB(0xff, 0xff, 0xff);
      break;
    case ColorID::SpellCheckerUnderline:
      aColor = NS_RGB(0xff, 0x00, 0x00);
      break;
    case ColorID::TextBackground:
      aColor = NS_RGB(0xff, 0xff, 0xff);
      break;
    case ColorID::TextForeground:
      aColor = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::TextHighlightBackground:
      aColor = NS_RGB(0xef, 0x0f, 0xff);
      break;
    case ColorID::TextHighlightForeground:
      aColor = NS_RGB(0xff, 0xff, 0xff);
      break;
    case ColorID::TextSelectBackground:
      aColor = NS_RGB(0xaa, 0xaa, 0xaa);
      break;
    case ColorID::TextSelectBackgroundAttention:
      aColor = NS_TRANSPARENT;
      break;
    case ColorID::TextSelectBackgroundDisabled:
      aColor = NS_RGB(0xaa, 0xaa, 0xaa);
      break;
    case ColorID::TextSelectForeground:
      GetColor(ColorID::TextSelectBackground, aColor);
      if (aColor == 0x000000)
        aColor = NS_RGB(0xff, 0xff, 0xff);
      else
        aColor = NS_DONT_CHANGE_COLOR;
      break;
    case ColorID::Widget3DHighlight:
      aColor = NS_RGB(0xa0, 0xa0, 0xa0);
      break;
    case ColorID::Widget3DShadow:
      aColor = NS_RGB(0x40, 0x40, 0x40);
      break;
    case ColorID::WidgetBackground:
      aColor = NS_RGB(0xdd, 0xdd, 0xdd);
      break;
    case ColorID::WidgetForeground:
      aColor = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::WidgetSelectBackground:
      aColor = NS_RGB(0x80, 0x80, 0x80);
      break;
    case ColorID::WidgetSelectForeground:
      aColor = NS_RGB(0x00, 0x00, 0x80);
      break;
    case ColorID::WindowBackground:
      aColor = NS_RGB(0xff, 0xff, 0xff);
      break;
    case ColorID::WindowForeground:
      aColor = NS_RGB(0x00, 0x00, 0x00);
      break;
    default:
      aColor = GetStandinForNativeColor(aID);
      break;
  }

  return res;
}

nsresult HeadlessLookAndFeel::GetIntImpl(IntID aID, int32_t& aResult) {
  nsresult res = nsXPLookAndFeel::GetIntImpl(aID, aResult);
  if (NS_SUCCEEDED(res)) return res;
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
    case eIntID_GTKCSDHideTitlebarByDefault:
    case eIntID_GTKCSDTransparentBackground:
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
    case eIntID_GTKCSDReversedPlacement:
      aResult = 0;
      break;
    default:
      NS_WARNING(
          "HeadlessLookAndFeel::GetIntImpl called with an unrecognized aID");
      aResult = 0;
      res = NS_ERROR_FAILURE;
      break;
  }
  return res;
}

nsresult HeadlessLookAndFeel::GetFloatImpl(FloatID aID, float& aResult) {
  nsresult res = nsXPLookAndFeel::GetFloatImpl(aID, aResult);
  if (NS_SUCCEEDED(res)) return res;
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
      NS_WARNING(
          "HeadlessLookAndFeel::GetFloatImpl called with an unrecognized aID");
      aResult = -1.0;
      res = NS_ERROR_FAILURE;
      break;
  }

  return res;
}

bool HeadlessLookAndFeel::GetFontImpl(FontID aID, nsString& aFontName,
                                      gfxFontStyle& aFontStyle) {
  // Default to san-serif for everything.
  aFontStyle.style = FontSlantStyle::Normal();
  aFontStyle.weight = FontWeight::Normal();
  aFontStyle.stretch = FontStretch::Normal();
  aFontStyle.size = 14;
  aFontStyle.systemFont = true;

  aFontName.AssignLiteral("sans-serif");
  return true;
}

char16_t HeadlessLookAndFeel::GetPasswordCharacterImpl() {
  return UNICODE_BULLET;
}

void HeadlessLookAndFeel::RefreshImpl() { nsXPLookAndFeel::RefreshImpl(); }

bool HeadlessLookAndFeel::GetEchoPasswordImpl() { return false; }

}  // namespace widget
}  // namespace mozilla
