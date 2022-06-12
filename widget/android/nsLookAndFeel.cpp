/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStyleConsts.h"
#include "nsXULAppAPI.h"
#include "nsLookAndFeel.h"
#include "Theme.h"
#include "gfxFont.h"
#include "gfxFontConstants.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/java/GeckoAppShellWrappers.h"
#include "mozilla/java/GeckoRuntimeWrappers.h"
#include "mozilla/java/GeckoSystemStateListenerWrappers.h"
#include "ThemeColors.h"

using namespace mozilla;
using namespace mozilla::widget;

static const char16_t UNICODE_BULLET = 0x2022;

static void AccentColorPrefChanged(const char*, void*) {
  LookAndFeel::NotifyChangedAllWindows(widget::ThemeChangeKind::Style);
}

nsLookAndFeel::nsLookAndFeel() {
  Preferences::RegisterCallback(
      AccentColorPrefChanged,
      nsDependentCString(
          StaticPrefs::GetPrefName_widget_non_native_theme_use_theme_accent()));
}

nsLookAndFeel::~nsLookAndFeel() {
  Preferences::UnregisterCallback(
      AccentColorPrefChanged,
      nsDependentCString(
          StaticPrefs::GetPrefName_widget_non_native_theme_use_theme_accent()));
}

nsresult nsLookAndFeel::GetSystemColors() {
  if (!jni::IsAvailable()) {
    return NS_ERROR_FAILURE;
  }

  auto arr = java::GeckoAppShell::GetSystemColors();
  if (!arr) {
    return NS_ERROR_FAILURE;
  }

  JNIEnv* const env = arr.Env();
  uint32_t len = static_cast<uint32_t>(env->GetArrayLength(arr.Get()));
  jint* elements = env->GetIntArrayElements(arr.Get(), 0);

  uint32_t colorsCount = sizeof(AndroidSystemColors) / sizeof(nscolor);
  if (len < colorsCount) colorsCount = len;

  // Convert Android colors to nscolor by switching R and B in the ARGB 32 bit
  // value
  nscolor* colors = (nscolor*)&mSystemColors;

  for (uint32_t i = 0; i < colorsCount; i++) {
    uint32_t androidColor = static_cast<uint32_t>(elements[i]);
    uint8_t r = (androidColor & 0x00ff0000) >> 16;
    uint8_t b = (androidColor & 0x000000ff);
    colors[i] = (androidColor & 0xff00ff00) | (b << 16) | r;
  }

  env->ReleaseIntArrayElements(arr.Get(), elements, 0);

  return NS_OK;
}

void nsLookAndFeel::NativeInit() {
  EnsureInitSystemColors();
  EnsureInitShowPassword();
  RecordTelemetry();
}

void nsLookAndFeel::RefreshImpl() {
  mInitializedSystemColors = false;
  mInitializedShowPassword = false;
  nsXPLookAndFeel::RefreshImpl();
}

nsresult nsLookAndFeel::NativeGetColor(ColorID aID, ColorScheme aColorScheme,
                                       nscolor& aColor) {
  EnsureInitSystemColors();
  if (!mInitializedSystemColors) {
    // Failure to initialize colors is an error condition. Return black.
    aColor = 0;
    return NS_ERROR_FAILURE;
  }

  // Highlight/Highlighttext have native equivalents that we can map to (on
  // Android) which should work fine, regardless of the color-scheme.
  switch (aID) {
    case ColorID::Highlight: {
      // Matched to action_accent in java codebase. This works fine with both
      // light and dark color scheme.
      nscolor accent =
          Color(ColorID::MozAccentColor, aColorScheme, UseStandins::No);
      aColor =
          NS_RGBA(NS_GET_R(accent), NS_GET_G(accent), NS_GET_B(accent), 78);
      return NS_OK;
    }
    case ColorID::Highlighttext:
      // Selection background is transparent enough that any foreground color
      // will do.
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      return NS_OK;
    default:
      break;
  }

  if (aColorScheme == ColorScheme::Dark) {
    if (auto darkColor = GenericDarkColor(aID)) {
      aColor = *darkColor;
      return NS_OK;
    }
  }

  // XXX we'll want to use context.obtainStyledAttributes on the java side to
  // get all of these; see TextView.java for a good example.
  auto UseNativeAccent = [this] {
    return mSystemColors.colorAccent &&
           StaticPrefs::widget_non_native_theme_use_theme_accent();
  };

  switch (aID) {
      // These colors don't seem to be used for anything anymore in Mozilla
      // The CSS2 colors below are used.
    case ColorID::MozMenubartext:
      aColor = mSystemColors.colorForeground;
      break;

    case ColorID::ThemedScrollbarThumbInactive:
    case ColorID::ThemedScrollbarThumb:
      // We don't need to care about the Active and Hover colors because Android
      // scrollbars can't be hovered (they always have pointer-events: none).
      aColor = NS_RGBA(119, 119, 119, 102);
      break;

    case ColorID::IMESelectedRawTextBackground:
    case ColorID::IMESelectedConvertedTextBackground:
      aColor = mSystemColors.textColorHighlight;
      break;
    case ColorID::IMESelectedRawTextForeground:
    case ColorID::IMESelectedConvertedTextForeground:
      aColor = mSystemColors.textColorPrimaryInverse;
      break;
    case ColorID::IMERawInputBackground:
    case ColorID::IMEConvertedTextBackground:
      aColor = NS_TRANSPARENT;
      break;
    case ColorID::IMERawInputForeground:
    case ColorID::IMEConvertedTextForeground:
    case ColorID::IMERawInputUnderline:
    case ColorID::IMEConvertedTextUnderline:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::IMESelectedRawTextUnderline:
    case ColorID::IMESelectedConvertedTextUnderline:
      aColor = NS_TRANSPARENT;
      break;
    case ColorID::SpellCheckerUnderline:
      aColor = NS_RGB(0xff, 0x00, 0x00);
      break;

      // css2  http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
    case ColorID::Activeborder:     // active window border
    case ColorID::Appworkspace:     // MDI background color
    case ColorID::Activecaption:    // active window caption background
    case ColorID::Background:       // desktop background
    case ColorID::Inactiveborder:   // inactive window border
    case ColorID::Inactivecaption:  // inactive window caption
    case ColorID::Scrollbar:        // scrollbar gray area
      aColor = mSystemColors.colorBackground;
      break;
    case ColorID::Graytext:  // disabled text in windows, menus, etc.
      aColor = NS_RGB(0xb1, 0xa5, 0x98);
      break;
    // FIXME: -moz-cellhighlight should show some kind of unfocused state.
    case ColorID::MozCellhighlight:
    case ColorID::Selecteditem:
    case ColorID::MozAccentColor:
      aColor = UseNativeAccent() ? mSystemColors.colorAccent
                                 : widget::sDefaultAccent.ToABGR();
      break;
    case ColorID::MozCellhighlighttext:
    case ColorID::Selecteditemtext:
    case ColorID::MozAccentColorForeground:
      aColor = UseNativeAccent() ? ThemeColors::ComputeCustomAccentForeground(
                                       mSystemColors.colorAccent)
                                 : widget::sDefaultAccentForeground.ToABGR();
      break;
    case ColorID::Fieldtext:
      aColor = NS_RGB(0x1a, 0x1a, 0x1a);
      break;
    case ColorID::Inactivecaptiontext:
      // text in inactive window caption
      aColor = mSystemColors.textColorTertiary;
      break;
    case ColorID::Infobackground:
      aColor = NS_RGB(0xf5, 0xf5, 0xb5);
      break;
    case ColorID::Infotext:
    case ColorID::Threeddarkshadow:  // 3-D shadow outer edge color
    case ColorID::MozButtondefault:  // default button border color
      aColor = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::Menu:
      aColor = NS_RGB(0xf7, 0xf5, 0xf3);
      break;

    case ColorID::Buttonface:
    case ColorID::MozButtondisabledface:
    case ColorID::Threedface:
    case ColorID::Threedlightshadow:
    case ColorID::MozDisabledfield:
      aColor = NS_RGB(0xec, 0xe7, 0xe2);
      break;

    case ColorID::Buttonhighlight:
    case ColorID::Field:
    case ColorID::Threedhighlight:
    case ColorID::MozCombobox:
    case ColorID::MozEventreerow:
      aColor = NS_RGB(0xff, 0xff, 0xff);
      break;

    case ColorID::Buttonshadow:
    case ColorID::Threedshadow:
      aColor = NS_RGB(0xae, 0xa1, 0x94);
      break;

    case ColorID::MozDialog:
    case ColorID::Window:
    case ColorID::Windowframe:
      aColor = NS_RGB(0xef, 0xeb, 0xe7);
      break;
    case ColorID::Buttontext:
    case ColorID::Captiontext:
    case ColorID::Menutext:
    case ColorID::MozButtonhovertext:
    case ColorID::MozDialogtext:
    case ColorID::MozComboboxtext:
    case ColorID::Windowtext:
    case ColorID::MozColheadertext:
    case ColorID::MozColheaderhovertext:
      aColor = NS_RGB(0x10, 0x10, 0x10);
      break;
    case ColorID::MozDragtargetzone:
      aColor = mSystemColors.textColorHighlight;
      break;
    case ColorID::MozButtonhoverface:
    case ColorID::MozButtonactiveface:
      aColor = NS_RGB(0xf3, 0xf0, 0xed);
      break;
    case ColorID::MozMenuhover:
      aColor = NS_RGB(0xee, 0xee, 0xee);
      break;
    case ColorID::MozMenubarhovertext:
    case ColorID::MozMenuhovertext:
      aColor = NS_RGB(0x77, 0x77, 0x77);
      break;
    case ColorID::MozOddtreerow:
      aColor = NS_TRANSPARENT;
      break;
    case ColorID::MozNativehyperlinktext:
      aColor = NS_RGB(0, 0, 0xee);
      break;
    default:
      /* default color is BLACK */
      aColor = 0;
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult nsLookAndFeel::NativeGetInt(IntID aID, int32_t& aResult) {
  nsresult rv = NS_OK;

  switch (aID) {
    case IntID::ScrollButtonLeftMouseButtonAction:
      aResult = 0;
      break;

    case IntID::ScrollButtonMiddleMouseButtonAction:
    case IntID::ScrollButtonRightMouseButtonAction:
      aResult = 3;
      break;

    case IntID::CaretBlinkTime:
      aResult = 500;
      break;

    case IntID::CaretBlinkCount:
      aResult = 10;
      break;

    case IntID::CaretWidth:
      aResult = 1;
      break;

    case IntID::ShowCaretDuringSelection:
      aResult = 0;
      break;

    case IntID::SelectTextfieldsOnKeyFocus:
      // Select textfield content when focused by kbd
      // used by EventStateManager::sTextfieldSelectModel
      aResult = 1;
      break;

    case IntID::SubmenuDelay:
      aResult = 200;
      break;

    case IntID::TooltipDelay:
      aResult = 500;
      break;

    case IntID::MenusCanOverlapOSBar:
      // we want XUL popups to be able to overlap the task bar.
      aResult = 1;
      break;

    case IntID::ScrollArrowStyle:
      aResult = eScrollArrowStyle_Single;
      break;

    case IntID::UseOverlayScrollbars:
      aResult = 1;
      break;

    case IntID::SpellCheckerUnderlineStyle:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_WAVY;
      break;

    case IntID::ScrollbarButtonAutoRepeatBehavior:
      aResult = 0;
      break;

    case IntID::ContextMenuOffsetVertical:
    case IntID::ContextMenuOffsetHorizontal:
      aResult = 2;
      break;

    case IntID::PrefersReducedMotion:
      aResult = java::GeckoSystemStateListener::PrefersReducedMotion();
      break;

    case IntID::PrimaryPointerCapabilities:
      aResult = java::GeckoAppShell::GetAllPointerCapabilities();

      // We cannot assume what is primary device, so we use Blink's way for web
      // compatibility (https://crbug.com/136119#c6). If having coarse
      // capability in any devices, return it.
      if (aResult & static_cast<int32_t>(PointerCapabilities::Coarse)) {
        aResult = static_cast<int32_t>(PointerCapabilities::Coarse);
      }
      break;

    case IntID::AllPointerCapabilities:
      aResult = java::GeckoAppShell::GetAllPointerCapabilities();
      break;

    case IntID::SystemUsesDarkTheme: {
      java::GeckoRuntime::LocalRef runtime = java::GeckoRuntime::GetInstance();
      aResult = runtime && runtime->UsesDarkTheme();
      break;
    }

    case IntID::DragThresholdX:
    case IntID::DragThresholdY:
      // Threshold where a tap becomes a drag, in 1/240" reference pixels.
      aResult = 25;
      break;

    case IntID::TouchDeviceSupportPresent:
      // Touch support is always enabled on android.
      aResult = 1;
      break;

    default:
      aResult = 0;
      rv = NS_ERROR_FAILURE;
  }

  return rv;
}

nsresult nsLookAndFeel::NativeGetFloat(FloatID aID, float& aResult) {
  nsresult rv = NS_OK;

  switch (aID) {
    case FloatID::IMEUnderlineRelativeSize:
      aResult = 1.0f;
      break;

    case FloatID::SpellCheckerUnderlineRelativeSize:
      aResult = 1.0f;
      break;

    default:
      aResult = -1.0;
      rv = NS_ERROR_FAILURE;
      break;
  }
  return rv;
}

bool nsLookAndFeel::NativeGetFont(FontID aID, nsString& aFontName,
                                  gfxFontStyle& aFontStyle) {
  aFontName.AssignLiteral("Roboto");
  aFontStyle.style = FontSlantStyle::NORMAL;
  aFontStyle.weight = FontWeight::NORMAL;
  aFontStyle.stretch = FontStretch::NORMAL;
  aFontStyle.size = 9.0 * 96.0f / 72.0f;
  aFontStyle.systemFont = true;
  return true;
}

bool nsLookAndFeel::GetEchoPasswordImpl() {
  EnsureInitShowPassword();
  return mShowPassword;
}

uint32_t nsLookAndFeel::GetPasswordMaskDelayImpl() {
  // This value is hard-coded in Android OS's PasswordTransformationMethod.java
  return 1500;
}

char16_t nsLookAndFeel::GetPasswordCharacterImpl() {
  // This value is hard-coded in Android OS's PasswordTransformationMethod.java
  return UNICODE_BULLET;
}

void nsLookAndFeel::EnsureInitSystemColors() {
  if (!mInitializedSystemColors) {
    mInitializedSystemColors = NS_SUCCEEDED(GetSystemColors());
  }
}

void nsLookAndFeel::EnsureInitShowPassword() {
  if (!mInitializedShowPassword && jni::IsAvailable()) {
    mShowPassword = java::GeckoAppShell::GetShowPasswordSetting();
    mInitializedShowPassword = true;
  }
}
