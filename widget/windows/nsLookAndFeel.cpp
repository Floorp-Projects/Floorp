/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLookAndFeel.h"
#include <stdint.h>
#include <windows.h>
#include <shellapi.h>
#include "nsStyleConsts.h"
#include "nsUXThemeData.h"
#include "nsUXThemeConstants.h"
#include "nsWindowsHelpers.h"
#include "WinUtils.h"
#include "WindowsUIUtils.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/Telemetry.h"
#include "mozilla/WindowsVersion.h"
#include "gfxFontConstants.h"
#include "gfxWindowsPlatform.h"

using namespace mozilla;
using namespace mozilla::widget;

static nsresult GetColorFromTheme(nsUXThemeClass cls, int32_t aPart,
                                  int32_t aState, int32_t aPropId,
                                  nscolor& aColor) {
  COLORREF color;
  HRESULT hr = GetThemeColor(nsUXThemeData::GetTheme(cls), aPart, aState,
                             aPropId, &color);
  if (hr == S_OK) {
    aColor = COLOREF_2_NSRGB(color);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

static int32_t GetSystemParam(long flag, int32_t def) {
  DWORD value;
  return ::SystemParametersInfo(flag, 0, &value, 0) ? value : def;
}

static nsresult SystemWantsDarkTheme(int32_t& darkThemeEnabled) {
  if (!IsWin10OrLater()) {
    darkThemeEnabled = 0;
    return NS_OK;
  }

  nsresult rv = NS_OK;
  nsCOMPtr<nsIWindowsRegKey> personalizeKey =
      do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = personalizeKey->Open(
      nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
      nsLiteralString(
          u"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
      nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint32_t lightThemeEnabled;
  rv =
      personalizeKey->ReadIntValue(u"AppsUseLightTheme"_ns, &lightThemeEnabled);
  if (NS_SUCCEEDED(rv)) {
    darkThemeEnabled = !lightThemeEnabled;
  }

  return rv;
}

nsLookAndFeel::nsLookAndFeel()
    : nsXPLookAndFeel(),
      mHasColorMenuHoverText(false),
      mHasColorAccent(false),
      mHasColorAccentText(false),
      mHasColorMediaText(false),
      mHasColorCommunicationsText(false),
      mInitialized(false) {
  mozilla::Telemetry::Accumulate(mozilla::Telemetry::TOUCH_ENABLED_DEVICE,
                                 WinUtils::IsTouchDeviceSupportPresent());
}

nsLookAndFeel::~nsLookAndFeel() {}

void nsLookAndFeel::NativeInit() { EnsureInit(); }

/* virtual */
void nsLookAndFeel::RefreshImpl() {
  mInitialized = false;  // Fetch system colors next time they're used.
  nsXPLookAndFeel::RefreshImpl();
}

nsresult nsLookAndFeel::NativeGetColor(ColorID aID, ColorScheme aScheme,
                                       nscolor& aColor) {
  EnsureInit();

  if (aScheme == ColorScheme::Dark) {
    if (auto color = GenericDarkColor(aID)) {
      aColor = *color;
      return NS_OK;
    }
  }

  nsresult res = NS_OK;

  int idx;
  switch (aID) {
    case ColorID::IMERawInputBackground:
    case ColorID::IMEConvertedTextBackground:
      aColor = NS_TRANSPARENT;
      return NS_OK;
    case ColorID::IMERawInputForeground:
    case ColorID::IMEConvertedTextForeground:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      return NS_OK;
    case ColorID::IMERawInputUnderline:
    case ColorID::IMEConvertedTextUnderline:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      return NS_OK;
    case ColorID::IMESelectedRawTextUnderline:
    case ColorID::IMESelectedConvertedTextUnderline:
      aColor = NS_TRANSPARENT;
      return NS_OK;
    case ColorID::SpellCheckerUnderline:
      aColor = NS_RGB(0xff, 0, 0);
      return NS_OK;

    // New CSS 2 Color definitions
    case ColorID::Activeborder:
      idx = COLOR_ACTIVEBORDER;
      break;
    case ColorID::Activecaption:
      idx = COLOR_ACTIVECAPTION;
      break;
    case ColorID::Appworkspace:
      idx = COLOR_APPWORKSPACE;
      break;
    case ColorID::Background:
      idx = COLOR_BACKGROUND;
      break;
    case ColorID::Buttonface:
    case ColorID::MozButtonhoverface:
    case ColorID::MozButtonactiveface:
    case ColorID::MozButtondisabledface:
      idx = COLOR_BTNFACE;
      break;
    case ColorID::Buttonhighlight:
      idx = COLOR_BTNHIGHLIGHT;
      break;
    case ColorID::Buttonshadow:
      idx = COLOR_BTNSHADOW;
      break;
    case ColorID::Buttontext:
    case ColorID::MozButtonhovertext:
    case ColorID::MozButtonactivetext:
      idx = COLOR_BTNTEXT;
      break;
    case ColorID::Captiontext:
      idx = COLOR_CAPTIONTEXT;
      break;
    case ColorID::MozCellhighlighttext:
      aColor = NS_RGB(0, 0, 0);
      return NS_OK;
    case ColorID::MozCellhighlight:
      aColor = NS_RGB(206, 206, 206);
      return NS_OK;
    case ColorID::Graytext:
      idx = COLOR_GRAYTEXT;
      break;
    case ColorID::Highlight:
    case ColorID::Selecteditem:
    case ColorID::MozMenuhover:
    case ColorID::IMESelectedRawTextBackground:
    case ColorID::IMESelectedConvertedTextBackground:
      idx = COLOR_HIGHLIGHT;
      break;
    case ColorID::MozMenubarhovertext:
      if (!nsUXThemeData::IsAppThemed()) {
        idx = nsUXThemeData::AreFlatMenusEnabled() ? COLOR_HIGHLIGHTTEXT
                                                   : COLOR_MENUTEXT;
        break;
      }
      [[fallthrough]];
    case ColorID::MozMenuhovertext:
      if (mHasColorMenuHoverText) {
        aColor = mColorMenuHoverText;
        return NS_OK;
      }
      [[fallthrough]];
    case ColorID::Highlighttext:
    case ColorID::Selecteditemtext:
    case ColorID::IMESelectedRawTextForeground:
    case ColorID::IMESelectedConvertedTextForeground:
      idx = COLOR_HIGHLIGHTTEXT;
      break;
    case ColorID::Inactiveborder:
      idx = COLOR_INACTIVEBORDER;
      break;
    case ColorID::Inactivecaption:
      idx = COLOR_INACTIVECAPTION;
      break;
    case ColorID::Inactivecaptiontext:
      idx = COLOR_INACTIVECAPTIONTEXT;
      break;
    case ColorID::Infobackground:
      idx = COLOR_INFOBK;
      break;
    case ColorID::Infotext:
      idx = COLOR_INFOTEXT;
      break;
    case ColorID::Menu:
      idx = COLOR_MENU;
      break;
    case ColorID::Menutext:
    case ColorID::MozMenubartext:
      idx = COLOR_MENUTEXT;
      break;
    case ColorID::Scrollbar:
      idx = COLOR_SCROLLBAR;
      break;
    case ColorID::Threeddarkshadow:
      idx = COLOR_3DDKSHADOW;
      break;
    case ColorID::Threedface:
      idx = COLOR_3DFACE;
      break;
    case ColorID::Threedhighlight:
      idx = COLOR_3DHIGHLIGHT;
      break;
    case ColorID::Threedlightshadow:
    case ColorID::MozDisabledfield:
      idx = COLOR_3DLIGHT;
      break;
    case ColorID::Threedshadow:
      idx = COLOR_3DSHADOW;
      break;
    case ColorID::Window:
      idx = COLOR_WINDOW;
      break;
    case ColorID::Windowframe:
      idx = COLOR_WINDOWFRAME;
      break;
    case ColorID::Windowtext:
      idx = COLOR_WINDOWTEXT;
      break;
    case ColorID::MozEventreerow:
    case ColorID::MozOddtreerow:
    case ColorID::Field:
    case ColorID::MozCombobox:
      idx = COLOR_WINDOW;
      break;
    case ColorID::Fieldtext:
    case ColorID::MozComboboxtext:
      idx = COLOR_WINDOWTEXT;
      break;
    case ColorID::MozDialog:
      idx = COLOR_3DFACE;
      break;
    case ColorID::MozAccentColor:
      if (mHasColorAccent) {
        aColor = mColorAccent;
      } else {
        // Seems to be the default color (hardcoded because of bug 1065998)
        aColor = NS_RGB(0, 120, 215);
      }
      return NS_OK;
    case ColorID::MozAccentColorForeground:
      if (mHasColorAccentText) {
        aColor = mColorAccentText;
      } else {
        aColor = NS_RGB(255, 255, 255);
      }
      return NS_OK;
    case ColorID::MozWinMediatext:
      if (mHasColorMediaText) {
        aColor = mColorMediaText;
        return NS_OK;
      }
      // if we've gotten here just return -moz-dialogtext instead
      idx = COLOR_WINDOWTEXT;
      break;
    case ColorID::MozWinCommunicationstext:
      if (mHasColorCommunicationsText) {
        aColor = mColorCommunicationsText;
        return NS_OK;
      }
      // if we've gotten here just return -moz-dialogtext instead
      idx = COLOR_WINDOWTEXT;
      break;
    case ColorID::MozDialogtext:
    case ColorID::MozColheadertext:
    case ColorID::MozColheaderhovertext:
      idx = COLOR_WINDOWTEXT;
      break;
    case ColorID::MozDragtargetzone:
      idx = COLOR_HIGHLIGHTTEXT;
      break;
    case ColorID::MozButtondefault:
      idx = COLOR_3DDKSHADOW;
      break;
    case ColorID::MozNativehyperlinktext:
      idx = COLOR_HOTLIGHT;
      break;
    default:
      idx = COLOR_WINDOW;
      res = NS_ERROR_FAILURE;
      break;
  }

  aColor = GetColorForSysColorIndex(idx);

  return res;
}

nsresult nsLookAndFeel::NativeGetInt(IntID aID, int32_t& aResult) {
  nsresult res = NS_OK;

  switch (aID) {
    case IntID::ScrollButtonLeftMouseButtonAction:
      aResult = 0;
      break;
    case IntID::ScrollButtonMiddleMouseButtonAction:
    case IntID::ScrollButtonRightMouseButtonAction:
      aResult = 3;
      break;
    case IntID::CaretBlinkTime:
      aResult = static_cast<int32_t>(::GetCaretBlinkTime());
      break;
    case IntID::CaretBlinkCount: {
      int32_t timeout = GetSystemParam(SPI_GETCARETTIMEOUT, 5000);
      auto blinkTime = ::GetCaretBlinkTime();
      if (timeout <= 0 || blinkTime <= 0) {
        aResult = -1;
        break;
      }
      // 2 * blinkTime because this integer is a full blink cycle.
      aResult = std::ceil(float(timeout) / (2.0f * float(blinkTime)));
      break;
    }

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
      // This will default to the Windows' default
      // (400ms) on error.
      aResult = GetSystemParam(SPI_GETMENUSHOWDELAY, 400);
      break;
    case IntID::TooltipDelay:
      aResult = 500;
      break;
    case IntID::MenusCanOverlapOSBar:
      // we want XUL popups to be able to overlap the task bar.
      aResult = 1;
      break;
    case IntID::DragThresholdX:
      // The system metric is the number of pixels at which a drag should
      // start.  Our look and feel metric is the number of pixels you can
      // move before starting a drag, so subtract 1.

      aResult = ::GetSystemMetrics(SM_CXDRAG) - 1;
      break;
    case IntID::DragThresholdY:
      aResult = ::GetSystemMetrics(SM_CYDRAG) - 1;
      break;
    case IntID::UseAccessibilityTheme:
      // High contrast is a misnomer under Win32 -- any theme can be used with
      // it, e.g. normal contrast with large fonts, low contrast, etc. The high
      // contrast flag really means -- use this theme and don't override it.
      aResult = nsUXThemeData::IsHighContrastOn();
      break;
    case IntID::ScrollArrowStyle:
      aResult = eScrollArrowStyle_Single;
      break;
    case IntID::TreeOpenDelay:
      aResult = 1000;
      break;
    case IntID::TreeCloseDelay:
      aResult = 0;
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
    case IntID::WindowsClassic:
      aResult = !nsUXThemeData::IsAppThemed();
      break;
    case IntID::WindowsDefaultTheme:
      aResult = nsUXThemeData::IsDefaultWindowTheme();
      break;
    case IntID::DWMCompositor:
      aResult = gfxWindowsPlatform::GetPlatform()->DwmCompositionEnabled();
      break;
    case IntID::WindowsAccentColorInTitlebar: {
      nscolor unused;
      if (NS_WARN_IF(NS_FAILED(GetAccentColor(unused)))) {
        aResult = 0;
        break;
      }

      uint32_t colorPrevalence;
      nsresult rv = mDwmKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                                  u"SOFTWARE\\Microsoft\\Windows\\DWM"_ns,
                                  nsIWindowsRegKey::ACCESS_QUERY_VALUE);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // The ColorPrevalence value is set to 1 when the "Show color on title
      // bar" setting in the Color section of Window's Personalization settings
      // is turned on.
      aResult = (NS_SUCCEEDED(mDwmKey->ReadIntValue(u"ColorPrevalence"_ns,
                                                    &colorPrevalence)) &&
                 colorPrevalence == 1)
                    ? 1
                    : 0;

      mDwmKey->Close();
    } break;
    case IntID::WindowsGlass:
      // Aero Glass is only available prior to Windows 8 when DWM is used.
      aResult = (gfxWindowsPlatform::GetPlatform()->DwmCompositionEnabled() &&
                 !IsWin8OrLater());
      break;
    case IntID::AlertNotificationOrigin:
      aResult = 0;
      {
        // Get task bar window handle
        HWND shellWindow = FindWindowW(L"Shell_TrayWnd", nullptr);

        if (shellWindow != nullptr) {
          // Determine position
          APPBARDATA appBarData;
          appBarData.hWnd = shellWindow;
          appBarData.cbSize = sizeof(appBarData);
          if (SHAppBarMessage(ABM_GETTASKBARPOS, &appBarData)) {
            // Set alert origin as a bit field - see LookAndFeel.h
            // 0 represents bottom right, sliding vertically.
            switch (appBarData.uEdge) {
              case ABE_LEFT:
                aResult = NS_ALERT_HORIZONTAL | NS_ALERT_LEFT;
                break;
              case ABE_RIGHT:
                aResult = NS_ALERT_HORIZONTAL;
                break;
              case ABE_TOP:
                aResult = NS_ALERT_TOP;
                [[fallthrough]];
              case ABE_BOTTOM:
                // If the task bar is right-to-left,
                // move the origin to the left
                if (::GetWindowLong(shellWindow, GWL_EXSTYLE) & WS_EX_LAYOUTRTL)
                  aResult |= NS_ALERT_LEFT;
                break;
            }
          }
        }
      }
      break;
    case IntID::IMERawInputUnderlineStyle:
    case IntID::IMEConvertedTextUnderlineStyle:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_DASHED;
      break;
    case IntID::IMESelectedRawTextUnderlineStyle:
    case IntID::IMESelectedConvertedTextUnderline:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_NONE;
      break;
    case IntID::SpellCheckerUnderlineStyle:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_WAVY;
      break;
    case IntID::ScrollbarButtonAutoRepeatBehavior:
      aResult = 0;
      break;
    case IntID::SwipeAnimationEnabled:
      // Forcibly enable the swipe animation on Windows. It doesn't matter on
      // platforms where "Drag two fingers to scroll" isn't supported since on
      // the platforms we will never generate any swipe gesture events.
      aResult = 1;
      break;
    case IntID::UseOverlayScrollbars:
      aResult = WindowsUIUtils::ComputeOverlayScrollbars();
      break;
    case IntID::AllowOverlayScrollbarsOverlap:
      aResult = 0;
      break;
    case IntID::ScrollbarDisplayOnMouseMove:
      aResult = 1;
      break;
    case IntID::ScrollbarFadeBeginDelay:
      aResult = 2500;
      break;
    case IntID::ScrollbarFadeDuration:
      aResult = 350;
      break;
    case IntID::ContextMenuOffsetVertical:
    case IntID::ContextMenuOffsetHorizontal:
      aResult = 2;
      break;
    case IntID::SystemUsesDarkTheme:
      res = SystemWantsDarkTheme(aResult);
      break;
    case IntID::SystemVerticalScrollbarWidth:
      aResult = WinUtils::GetSystemMetricsForDpi(SM_CXVSCROLL, 96);
      break;
    case IntID::SystemHorizontalScrollbarHeight:
      aResult = WinUtils::GetSystemMetricsForDpi(SM_CXHSCROLL, 96);
      break;
    case IntID::PrefersReducedMotion: {
      BOOL enableAnimation = TRUE;
      ::SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enableAnimation,
                              0);
      aResult = enableAnimation ? 0 : 1;
      break;
    }
    case IntID::PrimaryPointerCapabilities: {
      aResult = static_cast<int32_t>(
          widget::WinUtils::GetPrimaryPointerCapabilities());
      break;
    }
    case IntID::AllPointerCapabilities: {
      aResult =
          static_cast<int32_t>(widget::WinUtils::GetAllPointerCapabilities());
      break;
    }
    case IntID::TouchDeviceSupportPresent:
      aResult = WinUtils::IsTouchDeviceSupportPresent() ? 1 : 0;
      break;
    default:
      aResult = 0;
      res = NS_ERROR_FAILURE;
  }
  return res;
}

nsresult nsLookAndFeel::NativeGetFloat(FloatID aID, float& aResult) {
  nsresult res = NS_OK;

  switch (aID) {
    case FloatID::IMEUnderlineRelativeSize:
      aResult = 1.0f;
      break;
    case FloatID::SpellCheckerUnderlineRelativeSize:
      aResult = 1.0f;
      break;
    case FloatID::TextScaleFactor:
      aResult = WindowsUIUtils::ComputeTextScaleFactor();
      break;
    default:
      aResult = -1.0;
      res = NS_ERROR_FAILURE;
  }
  return res;
}

LookAndFeelFont nsLookAndFeel::GetLookAndFeelFontInternal(
    const LOGFONTW& aLogFont, bool aUseShellDlg) {
  LookAndFeelFont result{};

  result.haveFont() = false;

  // Get scaling factor from physical to logical pixels
  double pixelScale =
      1.0 / WinUtils::SystemScaleFactor() / LookAndFeel::GetTextScaleFactor();

  // The lfHeight is in pixels, and it needs to be adjusted for the
  // device it will be displayed on.
  // Screens and Printers will differ in DPI
  //
  // So this accounts for the difference in the DeviceContexts
  // The pixelScale will typically be 1.0 for the screen
  // (though larger for hi-dpi screens where the Windows resolution
  // scale factor is 125% or 150% or even more), and could be
  // any value when going to a printer, for example pixelScale is
  // 6.25 when going to a 600dpi printer.
  float pixelHeight = -aLogFont.lfHeight;
  if (pixelHeight < 0) {
    nsAutoFont hFont(::CreateFontIndirectW(&aLogFont));
    if (!hFont) {
      return result;
    }

    nsAutoHDC dc(::GetDC(nullptr));
    HGDIOBJ hObject = ::SelectObject(dc, hFont);
    TEXTMETRIC tm;
    ::GetTextMetrics(dc, &tm);
    ::SelectObject(dc, hObject);

    pixelHeight = tm.tmAscent;
  }

  pixelHeight *= pixelScale;

  // we have problem on Simplified Chinese system because the system
  // report the default font size is 8 points. but if we use 8, the text
  // display very ugly. force it to be at 9 points (12 pixels) on that
  // system (cp936), but leave other sizes alone.
  if (pixelHeight < 12 && ::GetACP() == 936) {
    pixelHeight = 12;
  }

  result.haveFont() = true;

  if (aUseShellDlg) {
    result.name() = u"MS Shell Dlg 2"_ns;
  } else {
    result.name() = aLogFont.lfFaceName;
  }

  result.size() = pixelHeight;
  result.italic() = !!aLogFont.lfItalic;
  // FIXME: Other weights?
  result.weight() = ((aLogFont.lfWeight == FW_BOLD) ? FontWeight::BOLD
                                                    : FontWeight::NORMAL)
                        .ToFloat();

  return result;
}

LookAndFeelFont nsLookAndFeel::GetLookAndFeelFont(LookAndFeel::FontID anID) {
  LookAndFeelFont result{};

  result.haveFont() = false;

  // FontID::Icon is handled differently than the others
  if (anID == LookAndFeel::FontID::Icon) {
    LOGFONTW logFont;
    if (::SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(logFont),
                                (PVOID)&logFont, 0)) {
      result = GetLookAndFeelFontInternal(logFont, false);
    }
    return result;
  }

  NONCLIENTMETRICSW ncm;
  ncm.cbSize = sizeof(NONCLIENTMETRICSW);
  if (!::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm),
                               (PVOID)&ncm, 0)) {
    return result;
  }

  switch (anID) {
    case LookAndFeel::FontID::Menu:
    case LookAndFeel::FontID::MozPullDownMenu:
      result = GetLookAndFeelFontInternal(ncm.lfMenuFont, false);
      break;
    case LookAndFeel::FontID::Caption:
      result = GetLookAndFeelFontInternal(ncm.lfCaptionFont, false);
      break;
    case LookAndFeel::FontID::SmallCaption:
      result = GetLookAndFeelFontInternal(ncm.lfSmCaptionFont, false);
      break;
    case LookAndFeel::FontID::StatusBar:
      result = GetLookAndFeelFontInternal(ncm.lfStatusFont, false);
      break;
    case LookAndFeel::FontID::MozDialog:
    case LookAndFeel::FontID::MozButton:
    case LookAndFeel::FontID::MozField:
    case LookAndFeel::FontID::MozList:
      // XXX It's not clear to me whether this is exactly the right
      // set of LookAndFeel values to map to the dialog font; we may
      // want to add or remove cases here after reviewing the visual
      // results under various Windows versions.
      result = GetLookAndFeelFontInternal(ncm.lfMessageFont, true);
      break;
    default:
      result = GetLookAndFeelFontInternal(ncm.lfMessageFont, false);
      break;
  }

  return result;
}

bool nsLookAndFeel::NativeGetFont(LookAndFeel::FontID anID, nsString& aFontName,
                                  gfxFontStyle& aFontStyle) {
  LookAndFeelFont font = GetLookAndFeelFont(anID);
  return LookAndFeelFontToStyle(font, aFontName, aFontStyle);
}

/* virtual */
char16_t nsLookAndFeel::GetPasswordCharacterImpl() {
#define UNICODE_BLACK_CIRCLE_CHAR 0x25cf
  return UNICODE_BLACK_CIRCLE_CHAR;
}

/* static */
nsresult nsLookAndFeel::GetAccentColor(nscolor& aColor) {
  nsresult rv;

  if (!mDwmKey) {
    mDwmKey = do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = mDwmKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                     u"SOFTWARE\\Microsoft\\Windows\\DWM"_ns,
                     nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t accentColor;
  if (NS_SUCCEEDED(mDwmKey->ReadIntValue(u"AccentColor"_ns, &accentColor))) {
    // The order of the color components in the DWORD stored in the registry
    // happens to be the same order as we store the components in nscolor
    // so we can just assign directly here.
    aColor = accentColor;
    rv = NS_OK;
  } else {
    rv = NS_ERROR_NOT_AVAILABLE;
  }

  mDwmKey->Close();

  return rv;
}

/* static */
nsresult nsLookAndFeel::GetAccentColorText(nscolor& aColor) {
  nscolor accentColor;
  nsresult rv = GetAccentColor(accentColor);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // We want the color that we return for text that will be drawn over
  // a background that has the accent color to have good contrast with
  // the accent color.  Windows itself uses either white or black text
  // depending on how light or dark the accent color is.  We do the same
  // here based on the luminance of the accent color with a threshhold
  // value.  This algorithm should match what Windows does.  It comes from:
  //
  // https://docs.microsoft.com/en-us/windows/uwp/style/color

  float luminance = (NS_GET_R(accentColor) * 2 + NS_GET_G(accentColor) * 5 +
                     NS_GET_B(accentColor)) /
                    8;

  aColor = (luminance <= 128) ? NS_RGB(255, 255, 255) : NS_RGB(0, 0, 0);

  return NS_OK;
}

nscolor nsLookAndFeel::GetColorForSysColorIndex(int index) {
  MOZ_ASSERT(index >= SYS_COLOR_MIN && index <= SYS_COLOR_MAX);
  return mSysColorTable[index - SYS_COLOR_MIN];
}

void nsLookAndFeel::EnsureInit() {
  if (mInitialized) {
    return;
  }
  mInitialized = true;

  nsresult res;

  res = GetAccentColor(mColorAccent);
  mHasColorAccent = NS_SUCCEEDED(res);

  res = GetAccentColorText(mColorAccentText);
  mHasColorAccentText = NS_SUCCEEDED(res);

  if (nsUXThemeData::IsAppThemed()) {
    res = ::GetColorFromTheme(eUXMenu, MENU_POPUPITEM, MPI_HOT, TMT_TEXTCOLOR,
                              mColorMenuHoverText);
    mHasColorMenuHoverText = NS_SUCCEEDED(res);

    res = ::GetColorFromTheme(eUXMediaToolbar, TP_BUTTON, TS_NORMAL,
                              TMT_TEXTCOLOR, mColorMediaText);
    mHasColorMediaText = NS_SUCCEEDED(res);

    res = ::GetColorFromTheme(eUXCommunicationsToolbar, TP_BUTTON, TS_NORMAL,
                              TMT_TEXTCOLOR, mColorCommunicationsText);
    mHasColorCommunicationsText = NS_SUCCEEDED(res);
  }

  // Fill out the sys color table.
  for (int i = SYS_COLOR_MIN; i <= SYS_COLOR_MAX; ++i) {
    DWORD color = ::GetSysColor(i);
    mSysColorTable[i - SYS_COLOR_MIN] = COLOREF_2_NSRGB(color);
  }

  RecordTelemetry();
}
