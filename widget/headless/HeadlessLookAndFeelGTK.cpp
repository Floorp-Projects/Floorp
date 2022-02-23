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

HeadlessLookAndFeel::HeadlessLookAndFeel() = default;

HeadlessLookAndFeel::~HeadlessLookAndFeel() = default;

nsresult HeadlessLookAndFeel::NativeGetColor(ColorID aID, ColorScheme aScheme,
                                             nscolor& aResult) {
  aResult = GetStandinForNativeColor(aID, aScheme);
  return NS_OK;
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

}  // namespace mozilla::widget
