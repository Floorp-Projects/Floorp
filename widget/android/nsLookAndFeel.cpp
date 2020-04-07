/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "nsStyleConsts.h"
#include "nsXULAppAPI.h"
#include "nsLookAndFeel.h"
#include "gfxFont.h"
#include "gfxFontConstants.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/gfx/2D.h"

using namespace mozilla;
using mozilla::dom::ContentChild;

bool nsLookAndFeel::mInitializedSystemColors = false;
AndroidSystemColors nsLookAndFeel::mSystemColors;

bool nsLookAndFeel::mInitializedShowPassword = false;
bool nsLookAndFeel::mShowPassword = true;

static const char16_t UNICODE_BULLET = 0x2022;

nsLookAndFeel::nsLookAndFeel() : nsXPLookAndFeel() {}

nsLookAndFeel::~nsLookAndFeel() {}

#define BG_PRELIGHT_COLOR NS_RGB(0xee, 0xee, 0xee)
#define FG_PRELIGHT_COLOR NS_RGB(0x77, 0x77, 0x77)
#define BLACK_COLOR NS_RGB(0x00, 0x00, 0x00)
#define DARK_GRAY_COLOR NS_RGB(0x40, 0x40, 0x40)
#define GRAY_COLOR NS_RGB(0x80, 0x80, 0x80)
#define LIGHT_GRAY_COLOR NS_RGB(0xa0, 0xa0, 0xa0)
#define RED_COLOR NS_RGB(0xff, 0x00, 0x00)

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

/* virtual */
void nsLookAndFeel::RefreshImpl() {
  if (mShouldRetainCacheForTest) {
    return;
  }

  nsXPLookAndFeel::RefreshImpl();

  mInitializedSystemColors = false;
  mInitializedShowPassword = false;

  if (XRE_IsParentProcess()) {
    mPrefersReducedMotionCached = false;
  }
}

nsresult nsLookAndFeel::NativeGetColor(ColorID aID, nscolor& aColor) {
  nsresult rv = NS_OK;

  EnsureInitSystemColors();
  if (!mInitializedSystemColors) {
    // Failure to initialize colors is an error condition. Return black.
    aColor = 0;
    return NS_ERROR_FAILURE;
  }

  // XXX we'll want to use context.obtainStyledAttributes on the java side to
  // get all of these; see TextView.java for a good exmaple.

  switch (aID) {
      // These colors don't seem to be used for anything anymore in Mozilla
      // (except here at least TextSelectBackground and TextSelectForeground)
      // The CSS2 colors below are used.
    case ColorID::WindowBackground:
      aColor = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::WindowForeground:
      aColor = mSystemColors.textColorPrimary;
      break;
    case ColorID::WidgetBackground:
      aColor = mSystemColors.colorBackground;
      break;
    case ColorID::WidgetForeground:
      aColor = mSystemColors.colorForeground;
      break;
    case ColorID::WidgetSelectBackground:
      aColor = mSystemColors.textColorHighlight;
      break;
    case ColorID::WidgetSelectForeground:
      aColor = mSystemColors.textColorPrimaryInverse;
      break;
    case ColorID::Widget3DHighlight:
      aColor = LIGHT_GRAY_COLOR;
      break;
    case ColorID::Widget3DShadow:
      aColor = DARK_GRAY_COLOR;
      break;
    case ColorID::TextBackground:
      // not used?
      aColor = mSystemColors.colorBackground;
      break;
    case ColorID::TextForeground:
      // not used?
      aColor = mSystemColors.textColorPrimary;
      break;
    case ColorID::TextSelectBackground:
      /* matched to action_accent in java codebase */
      aColor = NS_RGBA(10, 132, 255, 153);
      break;
    case ColorID::TextSelectForeground:
      aColor = NS_RGB(0, 0, 0);
      break;
    case ColorID::IMESelectedRawTextBackground:
    case ColorID::IMESelectedConvertedTextBackground:
      // still used
      aColor = mSystemColors.textColorHighlight;
      break;
    case ColorID::IMESelectedRawTextForeground:
    case ColorID::IMESelectedConvertedTextForeground:
      // still used
      aColor = mSystemColors.textColorPrimaryInverse;
      break;
    case ColorID::IMERawInputBackground:
    case ColorID::IMEConvertedTextBackground:
      aColor = NS_TRANSPARENT;
      break;
    case ColorID::IMERawInputForeground:
    case ColorID::IMEConvertedTextForeground:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::IMERawInputUnderline:
    case ColorID::IMEConvertedTextUnderline:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::IMESelectedRawTextUnderline:
    case ColorID::IMESelectedConvertedTextUnderline:
      aColor = NS_TRANSPARENT;
      break;
    case ColorID::SpellCheckerUnderline:
      aColor = RED_COLOR;
      break;

      // css2  http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
    case ColorID::Activeborder:
      // active window border
      aColor = mSystemColors.colorBackground;
      break;
    case ColorID::Activecaption:
      // active window caption background
      aColor = mSystemColors.colorBackground;
      break;
    case ColorID::Appworkspace:
      // MDI background color
      aColor = mSystemColors.colorBackground;
      break;
    case ColorID::Background:
      // desktop background
      aColor = mSystemColors.colorBackground;
      break;
    case ColorID::Graytext:
      // disabled text in windows, menus, etc.
      aColor = NS_RGB(0xb1, 0xa5, 0x98);
      break;
    case ColorID::MozCellhighlight:
    case ColorID::MozHtmlCellhighlight:
    case ColorID::Highlight:
      // background of selected item
      aColor = NS_RGB(0xfa, 0xd1, 0x84);
      break;
    case ColorID::MozCellhighlighttext:
    case ColorID::MozHtmlCellhighlighttext:
    case ColorID::Highlighttext:
    case ColorID::Fieldtext:
      aColor = NS_RGB(0x1a, 0x1a, 0x1a);
      break;
    case ColorID::Inactiveborder:
      // inactive window border
      aColor = mSystemColors.colorBackground;
      break;
    case ColorID::Inactivecaption:
      // inactive window caption
      aColor = mSystemColors.colorBackground;
      break;
    case ColorID::Inactivecaptiontext:
      // text in inactive window caption
      aColor = mSystemColors.textColorTertiary;
      break;
    case ColorID::Infobackground:
      aColor = NS_RGB(0xf5, 0xf5, 0xb5);
      break;
    case ColorID::Infotext:
      aColor = BLACK_COLOR;
      break;
    case ColorID::Menu:
      aColor = NS_RGB(0xf7, 0xf5, 0xf3);
      break;
    case ColorID::Scrollbar:
      // scrollbar gray area
      aColor = mSystemColors.colorBackground;
      break;

    case ColorID::Threedface:
    case ColorID::Buttonface:
    case ColorID::Threedlightshadow:
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

    case ColorID::Threeddarkshadow:
      // 3-D shadow outer edge color
      aColor = BLACK_COLOR;
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
      aColor = NS_RGB(0x10, 0x10, 0x10);
      break;
    case ColorID::MozDragtargetzone:
      aColor = mSystemColors.textColorHighlight;
      break;
    case ColorID::MozButtondefault:
      // default button border color
      aColor = BLACK_COLOR;
      break;
    case ColorID::MozButtonhoverface:
      aColor = NS_RGB(0xf3, 0xf0, 0xed);
      break;
    case ColorID::MozMenuhover:
      aColor = BG_PRELIGHT_COLOR;
      break;
    case ColorID::MozMenuhovertext:
      aColor = FG_PRELIGHT_COLOR;
      break;
    case ColorID::MozOddtreerow:
      aColor = NS_TRANSPARENT;
      break;
    case ColorID::MozNativehyperlinktext:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::MozMenubartext:
      aColor = mSystemColors.colorForeground;
      break;
    case ColorID::MozMenubarhovertext:
      aColor = FG_PRELIGHT_COLOR;
      break;
    default:
      /* default color is BLACK */
      aColor = 0;
      rv = NS_ERROR_FAILURE;
      break;
  }

  return rv;
}

nsresult nsLookAndFeel::GetIntImpl(IntID aID, int32_t& aResult) {
  nsresult rv = nsXPLookAndFeel::GetIntImpl(aID, aResult);
  if (NS_SUCCEEDED(rv)) return rv;

  rv = NS_OK;

  switch (aID) {
    case eIntID_CaretBlinkTime:
      aResult = 500;
      break;

    case eIntID_CaretWidth:
      aResult = 1;
      break;

    case eIntID_ShowCaretDuringSelection:
      aResult = 0;
      break;

    case eIntID_SelectTextfieldsOnKeyFocus:
      // Select textfield content when focused by kbd
      // used by EventStateManager::sTextfieldSelectModel
      aResult = 1;
      break;

    case eIntID_SubmenuDelay:
      aResult = 200;
      break;

    case eIntID_TooltipDelay:
      aResult = 500;
      break;

    case eIntID_MenusCanOverlapOSBar:
      // we want XUL popups to be able to overlap the task bar.
      aResult = 1;
      break;

    case eIntID_ScrollArrowStyle:
      aResult = eScrollArrowStyle_Single;
      break;

    case eIntID_ScrollSliderStyle:
      aResult = eScrollThumbStyle_Proportional;
      break;

    case eIntID_TouchEnabled:
      aResult = 1;
      break;

    case eIntID_WindowsDefaultTheme:
    case eIntID_WindowsThemeIdentifier:
    case eIntID_OperatingSystemVersionIdentifier:
      aResult = 0;
      rv = NS_ERROR_NOT_IMPLEMENTED;
      break;

    case eIntID_SpellCheckerUnderlineStyle:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_WAVY;
      break;

    case eIntID_ScrollbarButtonAutoRepeatBehavior:
      aResult = 0;
      break;

    case eIntID_ContextMenuOffsetVertical:
    case eIntID_ContextMenuOffsetHorizontal:
      aResult = 2;
      break;

    case eIntID_PrefersReducedMotion:
      if (!mPrefersReducedMotionCached && XRE_IsParentProcess()) {
        mPrefersReducedMotion =
            java::GeckoSystemStateListener::PrefersReducedMotion() ? 1 : 0;
      }
      aResult = mPrefersReducedMotion;
      break;

    case eIntID_PrimaryPointerCapabilities:
      aResult = java::GeckoAppShell::GetPrimaryPointerCapabilities();
      break;
    case eIntID_AllPointerCapabilities:
      aResult = java::GeckoAppShell::GetAllPointerCapabilities();
      break;

    case eIntID_SystemUsesDarkTheme:
      // Bail out if AndroidBridge hasn't initialized since we try to query
      // this vailue via nsMediaFeatures::InitSystemMetrics without initializing
      // AndroidBridge on xpcshell tests.
      if (!jni::IsAvailable()) {
        return NS_ERROR_FAILURE;
      }
      aResult = java::GeckoSystemStateListener::IsNightMode() ? 1 : 0;
      break;

    default:
      aResult = 0;
      rv = NS_ERROR_FAILURE;
  }

  return rv;
}

nsresult nsLookAndFeel::GetFloatImpl(FloatID aID, float& aResult) {
  nsresult rv = nsXPLookAndFeel::GetFloatImpl(aID, aResult);
  if (NS_SUCCEEDED(rv)) return rv;
  rv = NS_OK;

  switch (aID) {
    case eFloatID_IMEUnderlineRelativeSize:
      aResult = 1.0f;
      break;

    case eFloatID_SpellCheckerUnderlineRelativeSize:
      aResult = 1.0f;
      break;

    default:
      aResult = -1.0;
      rv = NS_ERROR_FAILURE;
      break;
  }
  return rv;
}

/*virtual*/
bool nsLookAndFeel::GetFontImpl(FontID aID, nsString& aFontName,
                                gfxFontStyle& aFontStyle) {
  aFontName.AssignLiteral("\"Roboto\"");
  aFontStyle.style = FontSlantStyle::Normal();
  aFontStyle.weight = FontWeight::Normal();
  aFontStyle.stretch = FontStretch::Normal();
  aFontStyle.size = 9.0 * 96.0f / 72.0f;
  aFontStyle.systemFont = true;
  return true;
}

/*virtual*/
bool nsLookAndFeel::GetEchoPasswordImpl() {
  EnsureInitShowPassword();
  return mShowPassword;
}

uint32_t nsLookAndFeel::GetPasswordMaskDelayImpl() {
  // This value is hard-coded in Android OS's PasswordTransformationMethod.java
  return 1500;
}

/* virtual */
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
  if (!mInitializedShowPassword) {
    if (XRE_IsParentProcess()) {
      mShowPassword =
          jni::IsAvailable() && java::GeckoAppShell::GetShowPasswordSetting();
    } else {
      ContentChild::GetSingleton()->SendGetShowPasswordSetting(&mShowPassword);
    }
    mInitializedShowPassword = true;
  }
}

nsTArray<LookAndFeelInt> nsLookAndFeel::GetIntCacheImpl() {
  nsTArray<LookAndFeelInt> lookAndFeelIntCache =
      nsXPLookAndFeel::GetIntCacheImpl();

  const IntID kIdsToCache[] = {eIntID_PrefersReducedMotion};

  for (IntID id : kIdsToCache) {
    lookAndFeelIntCache.AppendElement(
        LookAndFeelInt{.id = id, .value = GetInt(id)});
  }

  return lookAndFeelIntCache;
}

void nsLookAndFeel::SetIntCacheImpl(
    const nsTArray<LookAndFeelInt>& aLookAndFeelIntCache) {
  for (const auto& entry : aLookAndFeelIntCache) {
    switch (entry.id) {
      case eIntID_PrefersReducedMotion:
        mPrefersReducedMotion = entry.value;
        mPrefersReducedMotionCached = true;
        break;
    }
  }
}
