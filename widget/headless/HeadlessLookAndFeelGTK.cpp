/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeadlessLookAndFeel.h"
#include "mozilla/FontPropertyTypes.h"
#include "nsIContent.h"

namespace mozilla::widget {

static const char16_t UNICODE_BULLET = 0x2022;

HeadlessLookAndFeel::HeadlessLookAndFeel() {}

HeadlessLookAndFeel::~HeadlessLookAndFeel() = default;

nsresult HeadlessLookAndFeel::NativeGetColor(ColorID aID, ColorScheme,
                                             nscolor& aColor) {
  // For headless mode, we use GetStandinForNativeColor for everything we can,
  // and hardcoded values for everything else.

  nsresult res = NS_OK;

  switch (aID) {
    // Override the solid black that GetStandinForNativeColor provides for
    // FieldText, to match our behavior under the real GTK.
    case ColorID::Fieldtext:
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
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
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
    case ColorID::Highlight:
    case ColorID::MozAccentColor:
      aColor = NS_RGB(53, 132, 228);
      break;
    case ColorID::Highlighttext:
    case ColorID::MozAccentColorForeground:
      aColor = NS_RGB(0xff, 0xff, 0xff);
      break;
    default:
      aColor = GetStandinForNativeColor(aID);
      break;
  }

  return res;
}

nsresult HeadlessLookAndFeel::NativeGetInt(IntID aID, int32_t& aResult) {
  nsresult res = NS_OK;
  // These values should be sane defaults for headless mode under GTK.
  switch (aID) {
    case IntID::CaretBlinkTime:
      aResult = 567;
      break;
    case IntID::CaretWidth:
      aResult = 1;
      break;
    case IntID::ShowCaretDuringSelection:
      aResult = 0;
      break;
    case IntID::SelectTextfieldsOnKeyFocus:
      aResult = 1;
      break;
    case IntID::SubmenuDelay:
      aResult = 200;
      break;
    case IntID::MenusCanOverlapOSBar:
      aResult = 0;
      break;
    case IntID::UseOverlayScrollbars:
      aResult = 0;
      break;
    case IntID::AllowOverlayScrollbarsOverlap:
      aResult = 0;
      break;
    case IntID::ShowHideScrollbars:
      aResult = 0;
      break;
    case IntID::SkipNavigatingDisabledMenuItem:
      aResult = 1;
      break;
    case IntID::DragThresholdX:
    case IntID::DragThresholdY:
      aResult = 4;
      break;
    case IntID::UseAccessibilityTheme:
      aResult = 0;
      break;
    case IntID::ScrollArrowStyle:
      aResult = eScrollArrow_None;
      break;
    case IntID::ScrollSliderStyle:
      aResult = eScrollThumbStyle_Proportional;
      break;
    case IntID::ScrollButtonLeftMouseButtonAction:
      aResult = 0;
      return NS_OK;
    case IntID::ScrollButtonMiddleMouseButtonAction:
      aResult = 3;
      return NS_OK;
    case IntID::ScrollButtonRightMouseButtonAction:
      aResult = 3;
      return NS_OK;
    case IntID::TreeOpenDelay:
      aResult = 1000;
      break;
    case IntID::TreeCloseDelay:
      aResult = 1000;
      break;
    case IntID::TreeLazyScrollDelay:
      aResult = 150;
      break;
    case IntID::TreeScrollDelay:
      aResult = 100;
      break;
    case IntID::TreeScrollLinesMax:
      aResult = 3;
      break;
    case IntID::TabFocusModel:
      aResult = nsIContent::eTabFocus_textControlsMask;
      break;
    case IntID::ChosenMenuItemsShouldBlink:
      aResult = 1;
      break;
    case IntID::WindowsAccentColorInTitlebar:
    case IntID::WindowsDefaultTheme:
    case IntID::DWMCompositor:
      aResult = 0;
      res = NS_ERROR_NOT_IMPLEMENTED;
      break;
    case IntID::WindowsClassic:
    case IntID::WindowsGlass:
      aResult = 0;
      res = NS_ERROR_FAILURE;
      break;
    case IntID::MacGraphiteTheme:
    case IntID::MacBigSurTheme:
      aResult = 0;
      res = NS_ERROR_NOT_IMPLEMENTED;
      break;
    case IntID::AlertNotificationOrigin:
      aResult = NS_ALERT_TOP;
      break;
    case IntID::ScrollToClick:
      aResult = 0;
      break;
    case IntID::IMERawInputUnderlineStyle:
    case IntID::IMESelectedRawTextUnderlineStyle:
    case IntID::IMEConvertedTextUnderlineStyle:
    case IntID::IMESelectedConvertedTextUnderline:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_SOLID;
      break;
    case IntID::SpellCheckerUnderlineStyle:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_DOTTED;
      break;
    case IntID::MenuBarDrag:
      aResult = 0;
      break;
    case IntID::WindowsThemeIdentifier:
    case IntID::OperatingSystemVersionIdentifier:
      aResult = 0;
      res = NS_ERROR_NOT_IMPLEMENTED;
      break;
    case IntID::ScrollbarButtonAutoRepeatBehavior:
      aResult = 0;
      break;
    case IntID::TooltipDelay:
      aResult = 500;
      break;
    case IntID::SwipeAnimationEnabled:
      aResult = 0;
      break;
    case IntID::ScrollbarDisplayOnMouseMove:
      aResult = 0;
      break;
    case IntID::ScrollbarFadeBeginDelay:
      aResult = 0;
      break;
    case IntID::ScrollbarFadeDuration:
      aResult = 0;
      break;
    case IntID::ContextMenuOffsetVertical:
      aResult = -6;
      break;
    case IntID::ContextMenuOffsetHorizontal:
      aResult = 1;
      break;
    case IntID::GTKCSDAvailable:
    case IntID::GTKCSDHideTitlebarByDefault:
    case IntID::GTKCSDTransparentBackground:
      aResult = 0;
      break;
    case IntID::GTKCSDMinimizeButton:
      aResult = 0;
      break;
    case IntID::GTKCSDMaximizeButton:
      aResult = 0;
      break;
    case IntID::GTKCSDCloseButton:
      aResult = 1;
      break;
    case IntID::GTKCSDReversedPlacement:
      aResult = 0;
      break;
    case IntID::SystemUsesDarkTheme:
      aResult = 0;
      break;
    case IntID::PrefersReducedMotion:
      aResult = 0;
      break;
    case IntID::PrimaryPointerCapabilities:
      aResult = 0;
      break;
    case IntID::AllPointerCapabilities:
      aResult = 0;
      break;
    default:
      NS_WARNING(
          "HeadlessLookAndFeel::NativeGetInt called with an unrecognized aID");
      aResult = 0;
      res = NS_ERROR_FAILURE;
      break;
  }
  return res;
}

nsresult HeadlessLookAndFeel::NativeGetFloat(FloatID aID, float& aResult) {
  nsresult res = NS_OK;

  // Hardcoded values for GTK.
  switch (aID) {
    case FloatID::IMEUnderlineRelativeSize:
      aResult = 1.0f;
      break;
    case FloatID::SpellCheckerUnderlineRelativeSize:
      aResult = 1.0f;
      break;
    case FloatID::CaretAspectRatio:
      // Intentionally failing to quietly indicate lack of support.
      aResult = -1.0;
      res = NS_ERROR_FAILURE;
      break;
    default:
      NS_WARNING(
          "HeadlessLookAndFeel::NativeGetFloat called with an unrecognized "
          "aID");
      aResult = -1.0;
      res = NS_ERROR_FAILURE;
      break;
  }

  return res;
}

bool HeadlessLookAndFeel::NativeGetFont(FontID aID, nsString& aFontName,
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

}  // namespace mozilla::widget
