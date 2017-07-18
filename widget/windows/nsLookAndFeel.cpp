/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLookAndFeel.h"
#include <windows.h>
#include <shellapi.h>
#include "nsStyleConsts.h"
#include "nsUXThemeData.h"
#include "nsUXThemeConstants.h"
#include "WinUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/WindowsVersion.h"
#include "gfxFontConstants.h"

using namespace mozilla;
using namespace mozilla::widget;

//static
LookAndFeel::OperatingSystemVersion
nsLookAndFeel::GetOperatingSystemVersion()
{
  static OperatingSystemVersion version = eOperatingSystemVersion_Unknown;

  if (version != eOperatingSystemVersion_Unknown) {
    return version;
  }

  if (IsWin10OrLater()) {
    version = eOperatingSystemVersion_Windows10;
  } else if (IsWin8OrLater()) {
    version = eOperatingSystemVersion_Windows8;
  } else {
    version = eOperatingSystemVersion_Windows7;
  }

  return version;
}

static nsresult GetColorFromTheme(nsUXThemeClass cls,
                           int32_t aPart,
                           int32_t aState,
                           int32_t aPropId,
                           nscolor &aColor)
{
  COLORREF color;
  HRESULT hr = GetThemeColor(nsUXThemeData::GetTheme(cls), aPart, aState, aPropId, &color);
  if (hr == S_OK)
  {
    aColor = COLOREF_2_NSRGB(color);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

static int32_t GetSystemParam(long flag, int32_t def)
{
    DWORD value;
    return ::SystemParametersInfo(flag, 0, &value, 0) ? value : def;
}

nsLookAndFeel::nsLookAndFeel()
  : nsXPLookAndFeel()
  , mUseAccessibilityTheme(0)
  , mUseDefaultTheme(0)
  , mNativeThemeId(eWindowsTheme_Generic)
{
  mozilla::Telemetry::Accumulate(mozilla::Telemetry::TOUCH_ENABLED_DEVICE,
                                 WinUtils::IsTouchDeviceSupportPresent());
}

nsLookAndFeel::~nsLookAndFeel()
{
}

nsresult
nsLookAndFeel::NativeGetColor(ColorID aID, nscolor &aColor)
{
  nsresult res = NS_OK;

  int idx;
  switch (aID) {
    case eColorID_WindowBackground:
        idx = COLOR_WINDOW;
        break;
    case eColorID_WindowForeground:
        idx = COLOR_WINDOWTEXT;
        break;
    case eColorID_WidgetBackground:
        idx = COLOR_BTNFACE;
        break;
    case eColorID_WidgetForeground:
        idx = COLOR_BTNTEXT;
        break;
    case eColorID_WidgetSelectBackground:
        idx = COLOR_HIGHLIGHT;
        break;
    case eColorID_WidgetSelectForeground:
        idx = COLOR_HIGHLIGHTTEXT;
        break;
    case eColorID_Widget3DHighlight:
        idx = COLOR_BTNHIGHLIGHT;
        break;
    case eColorID_Widget3DShadow:
        idx = COLOR_BTNSHADOW;
        break;
    case eColorID_TextBackground:
        idx = COLOR_WINDOW;
        break;
    case eColorID_TextForeground:
        idx = COLOR_WINDOWTEXT;
        break;
    case eColorID_TextSelectBackground:
    case eColorID_IMESelectedRawTextBackground:
    case eColorID_IMESelectedConvertedTextBackground:
        idx = COLOR_HIGHLIGHT;
        break;
    case eColorID_TextSelectForeground:
    case eColorID_IMESelectedRawTextForeground:
    case eColorID_IMESelectedConvertedTextForeground:
        idx = COLOR_HIGHLIGHTTEXT;
        break;
    case eColorID_IMERawInputBackground:
    case eColorID_IMEConvertedTextBackground:
        aColor = NS_TRANSPARENT;
        return NS_OK;
    case eColorID_IMERawInputForeground:
    case eColorID_IMEConvertedTextForeground:
        aColor = NS_SAME_AS_FOREGROUND_COLOR;
        return NS_OK;
    case eColorID_IMERawInputUnderline:
    case eColorID_IMEConvertedTextUnderline:
        aColor = NS_SAME_AS_FOREGROUND_COLOR;
        return NS_OK;
    case eColorID_IMESelectedRawTextUnderline:
    case eColorID_IMESelectedConvertedTextUnderline:
        aColor = NS_TRANSPARENT;
        return NS_OK;
    case eColorID_SpellCheckerUnderline:
        aColor = NS_RGB(0xff, 0, 0);
        return NS_OK;

    // New CSS 2 Color definitions
    case eColorID_activeborder:
      idx = COLOR_ACTIVEBORDER;
      break;
    case eColorID_activecaption:
      idx = COLOR_ACTIVECAPTION;
      break;
    case eColorID_appworkspace:
      idx = COLOR_APPWORKSPACE;
      break;
    case eColorID_background:
      idx = COLOR_BACKGROUND;
      break;
    case eColorID_buttonface:
    case eColorID__moz_buttonhoverface:
      idx = COLOR_BTNFACE;
      break;
    case eColorID_buttonhighlight:
      idx = COLOR_BTNHIGHLIGHT;
      break;
    case eColorID_buttonshadow:
      idx = COLOR_BTNSHADOW;
      break;
    case eColorID_buttontext:
    case eColorID__moz_buttonhovertext:
      idx = COLOR_BTNTEXT;
      break;
    case eColorID_captiontext:
      idx = COLOR_CAPTIONTEXT;
      break;
    case eColorID_graytext:
      idx = COLOR_GRAYTEXT;
      break;
    case eColorID_highlight:
    case eColorID__moz_html_cellhighlight:
    case eColorID__moz_menuhover:
      idx = COLOR_HIGHLIGHT;
      break;
    case eColorID__moz_menubarhovertext:
      if (!IsAppThemed()) {
        idx = nsUXThemeData::sFlatMenus ?
                COLOR_HIGHLIGHTTEXT :
                COLOR_MENUTEXT;
        break;
      }
      // Fall through
    case eColorID__moz_menuhovertext:
      if (IsAppThemed()) {
        res = ::GetColorFromTheme(eUXMenu,
                                  MENU_POPUPITEM, MPI_HOT, TMT_TEXTCOLOR, aColor);
        if (NS_SUCCEEDED(res))
          return res;
        // fall through to highlight case
      }
    case eColorID_highlighttext:
    case eColorID__moz_html_cellhighlighttext:
      idx = COLOR_HIGHLIGHTTEXT;
      break;
    case eColorID_inactiveborder:
      idx = COLOR_INACTIVEBORDER;
      break;
    case eColorID_inactivecaption:
      idx = COLOR_INACTIVECAPTION;
      break;
    case eColorID_inactivecaptiontext:
      idx = COLOR_INACTIVECAPTIONTEXT;
      break;
    case eColorID_infobackground:
      idx = COLOR_INFOBK;
      break;
    case eColorID_infotext:
      idx = COLOR_INFOTEXT;
      break;
    case eColorID_menu:
      idx = COLOR_MENU;
      break;
    case eColorID_menutext:
    case eColorID__moz_menubartext:
      idx = COLOR_MENUTEXT;
      break;
    case eColorID_scrollbar:
      idx = COLOR_SCROLLBAR;
      break;
    case eColorID_threeddarkshadow:
      idx = COLOR_3DDKSHADOW;
      break;
    case eColorID_threedface:
      idx = COLOR_3DFACE;
      break;
    case eColorID_threedhighlight:
      idx = COLOR_3DHIGHLIGHT;
      break;
    case eColorID_threedlightshadow:
      idx = COLOR_3DLIGHT;
      break;
    case eColorID_threedshadow:
      idx = COLOR_3DSHADOW;
      break;
    case eColorID_window:
      idx = COLOR_WINDOW;
      break;
    case eColorID_windowframe:
      idx = COLOR_WINDOWFRAME;
      break;
    case eColorID_windowtext:
      idx = COLOR_WINDOWTEXT;
      break;
    case eColorID__moz_eventreerow:
    case eColorID__moz_oddtreerow:
    case eColorID__moz_field:
    case eColorID__moz_combobox:
      idx = COLOR_WINDOW;
      break;
    case eColorID__moz_fieldtext:
    case eColorID__moz_comboboxtext:
      idx = COLOR_WINDOWTEXT;
      break;
    case eColorID__moz_dialog:
    case eColorID__moz_cellhighlight:
      idx = COLOR_3DFACE;
      break;
    case eColorID__moz_win_accentcolor:
      res = GetAccentColor(aColor);
      if (NS_SUCCEEDED(res)) {
        return res;
      }
      // Seems to be the default color (hardcoded because of bug 1065998)
      aColor = NS_RGB(158, 158, 158);
      return NS_OK;
    case eColorID__moz_win_accentcolortext:
      res = GetAccentColorText(aColor);
      if (NS_SUCCEEDED(res)) {
        return res;
      }
      aColor = NS_RGB(0, 0, 0);
      return NS_OK;
    case eColorID__moz_win_mediatext:
      if (IsAppThemed()) {
        res = ::GetColorFromTheme(eUXMediaToolbar,
                                  TP_BUTTON, TS_NORMAL, TMT_TEXTCOLOR, aColor);
        if (NS_SUCCEEDED(res))
          return res;
      }
      // if we've gotten here just return -moz-dialogtext instead
      idx = COLOR_WINDOWTEXT;
      break;
    case eColorID__moz_win_communicationstext:
      if (IsAppThemed()) {
        res = ::GetColorFromTheme(eUXCommunicationsToolbar,
                                  TP_BUTTON, TS_NORMAL, TMT_TEXTCOLOR, aColor);
        if (NS_SUCCEEDED(res))
          return res;
      }
      // if we've gotten here just return -moz-dialogtext instead
      idx = COLOR_WINDOWTEXT;
      break;
    case eColorID__moz_dialogtext:
    case eColorID__moz_cellhighlighttext:
      idx = COLOR_WINDOWTEXT;
      break;
    case eColorID__moz_dragtargetzone:
      idx = COLOR_HIGHLIGHTTEXT;
      break;
    case eColorID__moz_buttondefault:
      idx = COLOR_3DDKSHADOW;
      break;
    case eColorID__moz_nativehyperlinktext:
      idx = COLOR_HOTLIGHT;
      break;
    default:
      NS_WARNING("Unknown color for nsLookAndFeel");
      idx = COLOR_WINDOW;
      res = NS_ERROR_FAILURE;
      break;
    }

  DWORD color = ::GetSysColor(idx);
  aColor = COLOREF_2_NSRGB(color);

  return res;
}

nsresult
nsLookAndFeel::GetIntImpl(IntID aID, int32_t &aResult)
{
  nsresult res = nsXPLookAndFeel::GetIntImpl(aID, aResult);
  if (NS_SUCCEEDED(res))
    return res;
  res = NS_OK;

  switch (aID) {
    case eIntID_CaretBlinkTime:
        aResult = (int32_t)::GetCaretBlinkTime();
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
        // This will default to the Windows' default
        // (400ms) on error.
        aResult = GetSystemParam(SPI_GETMENUSHOWDELAY, 400);
        break;
    case eIntID_TooltipDelay:
        aResult = 500;
        break;
    case eIntID_MenusCanOverlapOSBar:
        // we want XUL popups to be able to overlap the task bar.
        aResult = 1;
        break;
    case eIntID_DragThresholdX:
        // The system metric is the number of pixels at which a drag should
        // start.  Our look and feel metric is the number of pixels you can
        // move before starting a drag, so subtract 1.

        aResult = ::GetSystemMetrics(SM_CXDRAG) - 1;
        break;
    case eIntID_DragThresholdY:
        aResult = ::GetSystemMetrics(SM_CYDRAG) - 1;
        break;
    case eIntID_UseAccessibilityTheme:
        // High contrast is a misnomer under Win32 -- any theme can be used with it,
        // e.g. normal contrast with large fonts, low contrast, etc.
        // The high contrast flag really means -- use this theme and don't override it.
        if (XRE_IsContentProcess()) {
          // If we're running in the content process, then the parent should
          // have sent us the accessibility state when nsLookAndFeel
          // initialized, and stashed it in the mUseAccessibilityTheme cache.
          aResult = mUseAccessibilityTheme;
        } else {
          // Otherwise, we can ask the OS to see if we're using High Contrast
          // mode.
          aResult = nsUXThemeData::IsHighContrastOn();
        }
        break;
    case eIntID_ScrollArrowStyle:
        aResult = eScrollArrowStyle_Single;
        break;
    case eIntID_ScrollSliderStyle:
        aResult = eScrollThumbStyle_Proportional;
        break;
    case eIntID_TreeOpenDelay:
        aResult = 1000;
        break;
    case eIntID_TreeCloseDelay:
        aResult = 0;
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
    case eIntID_WindowsClassic:
        aResult = !IsAppThemed();
        break;
    case eIntID_TouchEnabled:
        aResult = WinUtils::IsTouchDeviceSupportPresent();
        break;
    case eIntID_WindowsDefaultTheme:
        if (XRE_IsContentProcess()) {
          aResult = mUseDefaultTheme;
        } else {
          aResult = nsUXThemeData::IsDefaultWindowTheme();
        }
        break;
    case eIntID_WindowsThemeIdentifier:
        if (XRE_IsContentProcess()) {
          aResult = mNativeThemeId;
        } else {
          aResult = nsUXThemeData::GetNativeThemeId();
        }
        break;

    case eIntID_OperatingSystemVersionIdentifier:
    {
        aResult = GetOperatingSystemVersion();
        break;
    }

    case eIntID_MacGraphiteTheme:
        aResult = 0;
        res = NS_ERROR_NOT_IMPLEMENTED;
        break;
    case eIntID_DWMCompositor:
        aResult = nsUXThemeData::CheckForCompositor();
        break;
    case eIntID_WindowsAccentColorInTitlebar:
        {
          nscolor unused;
          if (NS_WARN_IF(NS_FAILED(GetAccentColor(unused)))) {
            aResult = 0;
            break;
          }

          uint32_t colorPrevalence;
          nsresult rv = mDwmKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                                      NS_LITERAL_STRING("SOFTWARE\\Microsoft\\Windows\\DWM"),
                                      nsIWindowsRegKey::ACCESS_QUERY_VALUE);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }

          // The ColorPrevalence value is set to 1 when the "Show color on title bar"
          // setting in the Color section of Window's Personalization settings is
          // turned on.
          aResult =
            (NS_SUCCEEDED(mDwmKey->ReadIntValue(NS_LITERAL_STRING("ColorPrevalence"),
                                                &colorPrevalence)) &&
             colorPrevalence == 1) ? 1 : 0;

          mDwmKey->Close();
        }
        break;
    case eIntID_WindowsGlass:
        // Aero Glass is only available prior to Windows 8 when DWM is used.
        aResult = (nsUXThemeData::CheckForCompositor() && !IsWin8OrLater());
        break;
    case eIntID_AlertNotificationOrigin:
        aResult = 0;
        {
          // Get task bar window handle
          HWND shellWindow = FindWindowW(L"Shell_TrayWnd", nullptr);

          if (shellWindow != nullptr)
          {
            // Determine position
            APPBARDATA appBarData;
            appBarData.hWnd = shellWindow;
            appBarData.cbSize = sizeof(appBarData);
            if (SHAppBarMessage(ABM_GETTASKBARPOS, &appBarData))
            {
              // Set alert origin as a bit field - see LookAndFeel.h
              // 0 represents bottom right, sliding vertically.
              switch(appBarData.uEdge)
              {
                case ABE_LEFT:
                  aResult = NS_ALERT_HORIZONTAL | NS_ALERT_LEFT;
                  break;
                case ABE_RIGHT:
                  aResult = NS_ALERT_HORIZONTAL;
                  break;
                case ABE_TOP:
                  aResult = NS_ALERT_TOP;
                  // fall through for the right-to-left handling.
                case ABE_BOTTOM:
                  // If the task bar is right-to-left,
                  // move the origin to the left
                  if (::GetWindowLong(shellWindow, GWL_EXSTYLE) &
                        WS_EX_LAYOUTRTL)
                    aResult |= NS_ALERT_LEFT;
                  break;
              }
            }
          }
        }
        break;
    case eIntID_IMERawInputUnderlineStyle:
    case eIntID_IMEConvertedTextUnderlineStyle:
        aResult = NS_STYLE_TEXT_DECORATION_STYLE_DASHED;
        break;
    case eIntID_IMESelectedRawTextUnderlineStyle:
    case eIntID_IMESelectedConvertedTextUnderline:
        aResult = NS_STYLE_TEXT_DECORATION_STYLE_NONE;
        break;
    case eIntID_SpellCheckerUnderlineStyle:
        aResult = NS_STYLE_TEXT_DECORATION_STYLE_WAVY;
        break;
    case eIntID_ScrollbarButtonAutoRepeatBehavior:
        aResult = 0;
        break;
    case eIntID_SwipeAnimationEnabled:
        aResult = 0;
        break;
    case eIntID_ColorPickerAvailable:
        aResult = true;
        break;
    case eIntID_UseOverlayScrollbars:
        aResult = false;
        break;
    case eIntID_AllowOverlayScrollbarsOverlap:
        aResult = 0;
        break;
    case eIntID_ScrollbarDisplayOnMouseMove:
        aResult = 1;
        break;
    case eIntID_ScrollbarFadeBeginDelay:
        aResult = 2500;
        break;
    case eIntID_ScrollbarFadeDuration:
        aResult = 350;
        break;
    case eIntID_ContextMenuOffsetVertical:
    case eIntID_ContextMenuOffsetHorizontal:
        aResult = 2;
        break;
    default:
        aResult = 0;
        res = NS_ERROR_FAILURE;
    }
  return res;
}

nsresult
nsLookAndFeel::GetFloatImpl(FloatID aID, float &aResult)
{
  nsresult res = nsXPLookAndFeel::GetFloatImpl(aID, aResult);
  if (NS_SUCCEEDED(res))
    return res;
  res = NS_OK;

  switch (aID) {
    case eFloatID_IMEUnderlineRelativeSize:
        aResult = 1.0f;
        break;
    case eFloatID_SpellCheckerUnderlineRelativeSize:
        aResult = 1.0f;
        break;
    default:
        aResult = -1.0;
        res = NS_ERROR_FAILURE;
    }
  return res;
}

static bool
GetSysFontInfo(HDC aHDC, LookAndFeel::FontID anID,
               nsString &aFontName,
               gfxFontStyle &aFontStyle)
{
  const LOGFONTW* ptrLogFont = nullptr;
  LOGFONTW logFont;
  NONCLIENTMETRICSW ncm;
  char16_t name[LF_FACESIZE];
  bool useShellDlg = false;

  // Depending on which stock font we want, there are a couple of
  // places we might have to look it up.
  switch (anID) {
  case LookAndFeel::eFont_Icon:
    if (!::SystemParametersInfoW(SPI_GETICONTITLELOGFONT,
                                 sizeof(logFont), (PVOID)&logFont, 0))
      return false;

    ptrLogFont = &logFont;
    break;

  default:
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    if (!::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,
                                 sizeof(ncm), (PVOID)&ncm, 0))
      return false;

    switch (anID) {
    case LookAndFeel::eFont_Menu:
    case LookAndFeel::eFont_PullDownMenu:
      ptrLogFont = &ncm.lfMenuFont;
      break;
    case LookAndFeel::eFont_Caption:
      ptrLogFont = &ncm.lfCaptionFont;
      break;
    case LookAndFeel::eFont_SmallCaption:
      ptrLogFont = &ncm.lfSmCaptionFont;
      break;
    case LookAndFeel::eFont_StatusBar:
    case LookAndFeel::eFont_Tooltips:
      ptrLogFont = &ncm.lfStatusFont;
      break;
    case LookAndFeel::eFont_Widget:
    case LookAndFeel::eFont_Dialog:
    case LookAndFeel::eFont_Button:
    case LookAndFeel::eFont_Field:
    case LookAndFeel::eFont_List:
      // XXX It's not clear to me whether this is exactly the right
      // set of LookAndFeel values to map to the dialog font; we may
      // want to add or remove cases here after reviewing the visual
      // results under various Windows versions.
      useShellDlg = true;
      // Fall through so that we can get size from lfMessageFont;
      // but later we'll use the (virtual) "MS Shell Dlg 2" font name
      // instead of the LOGFONT's.
    default:
      ptrLogFont = &ncm.lfMessageFont;
      break;
    }
    break;
  }

  // Get scaling factor from physical to logical pixels
  double pixelScale = 1.0 / WinUtils::SystemScaleFactor();

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
  float pixelHeight = -ptrLogFont->lfHeight;
  if (pixelHeight < 0) {
    HFONT hFont = ::CreateFontIndirectW(ptrLogFont);
    if (!hFont)
      return false;
    HGDIOBJ hObject = ::SelectObject(aHDC, hFont);
    TEXTMETRIC tm;
    ::GetTextMetrics(aHDC, &tm);
    ::SelectObject(aHDC, hObject);
    ::DeleteObject(hFont);
    pixelHeight = tm.tmAscent;
  }
  pixelHeight *= pixelScale;

  // we have problem on Simplified Chinese system because the system
  // report the default font size is 8 points. but if we use 8, the text
  // display very ugly. force it to be at 9 points (12 pixels) on that
  // system (cp936), but leave other sizes alone.
  if (pixelHeight < 12 && ::GetACP() == 936)
    pixelHeight = 12;

  aFontStyle.size = pixelHeight;

  // FIXME: What about oblique?
  aFontStyle.style =
    (ptrLogFont->lfItalic) ? NS_FONT_STYLE_ITALIC : NS_FONT_STYLE_NORMAL;

  // FIXME: Other weights?
  aFontStyle.weight =
    (ptrLogFont->lfWeight == FW_BOLD ?
        NS_FONT_WEIGHT_BOLD : NS_FONT_WEIGHT_NORMAL);

  // FIXME: Set aFontStyle->stretch correctly!
  aFontStyle.stretch = NS_FONT_STRETCH_NORMAL;

  aFontStyle.systemFont = true;

  if (useShellDlg) {
    aFontName = NS_LITERAL_STRING("MS Shell Dlg 2");
  } else {
    memcpy(name, ptrLogFont->lfFaceName, LF_FACESIZE*sizeof(char16_t));
    aFontName = name;
  }

  return true;
}

bool
nsLookAndFeel::GetFontImpl(FontID anID, nsString &aFontName,
                           gfxFontStyle &aFontStyle,
                           float aDevPixPerCSSPixel)
{
  CachedSystemFont &cacheSlot = mSystemFontCache[anID];

  bool status;
  if (cacheSlot.mCacheValid) {
    status = cacheSlot.mHaveFont;
    if (status) {
      aFontName = cacheSlot.mFontName;
      aFontStyle = cacheSlot.mFontStyle;
    }
  } else {
    HDC tdc = GetDC(nullptr);
    status = GetSysFontInfo(tdc, anID, aFontName, aFontStyle);
    ReleaseDC(nullptr, tdc);

    cacheSlot.mCacheValid = true;
    cacheSlot.mHaveFont = status;
    if (status) {
      cacheSlot.mFontName = aFontName;
      cacheSlot.mFontStyle = aFontStyle;
    }
  }
  // now convert the logical font size from GetSysFontInfo into device pixels for layout
  aFontStyle.size *= aDevPixPerCSSPixel;
  return status;
}

/* virtual */ void
nsLookAndFeel::RefreshImpl()
{
  nsXPLookAndFeel::RefreshImpl();

  for (auto e = mSystemFontCache.begin(), end = mSystemFontCache.end();
       e != end; ++e) {
    e->mCacheValid = false;
  }
}

/* virtual */
char16_t
nsLookAndFeel::GetPasswordCharacterImpl()
{
#define UNICODE_BLACK_CIRCLE_CHAR 0x25cf
  return UNICODE_BLACK_CIRCLE_CHAR;
}

nsTArray<LookAndFeelInt>
nsLookAndFeel::GetIntCacheImpl()
{
  nsTArray<LookAndFeelInt> lookAndFeelIntCache =
    nsXPLookAndFeel::GetIntCacheImpl();

  LookAndFeelInt lafInt;
  lafInt.id = eIntID_UseAccessibilityTheme;
  lafInt.value = GetInt(eIntID_UseAccessibilityTheme);
  lookAndFeelIntCache.AppendElement(lafInt);

  lafInt.id = eIntID_WindowsDefaultTheme;
  lafInt.value = GetInt(eIntID_WindowsDefaultTheme);
  lookAndFeelIntCache.AppendElement(lafInt);

  lafInt.id = eIntID_WindowsThemeIdentifier;
  lafInt.value = GetInt(eIntID_WindowsThemeIdentifier);
  lookAndFeelIntCache.AppendElement(lafInt);

  return lookAndFeelIntCache;
}

void
nsLookAndFeel::SetIntCacheImpl(const nsTArray<LookAndFeelInt>& aLookAndFeelIntCache)
{
  for (auto entry : aLookAndFeelIntCache) {
    switch (entry.id) {
      case eIntID_UseAccessibilityTheme:
      mUseAccessibilityTheme = entry.value;
      break;
      case eIntID_WindowsDefaultTheme:
      mUseDefaultTheme = entry.value;
      break;
      case eIntID_WindowsThemeIdentifier:
      mNativeThemeId = entry.value;
      break;
    }
  }
}

/* static */ nsresult
nsLookAndFeel::GetAccentColor(nscolor& aColor)
{
  nsresult rv;

  if (!mDwmKey) {
    mDwmKey = do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = mDwmKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                     NS_LITERAL_STRING("SOFTWARE\\Microsoft\\Windows\\DWM"),
                     nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t accentColor;
  if (NS_SUCCEEDED(mDwmKey->ReadIntValue(NS_LITERAL_STRING("AccentColor"), &accentColor))) {
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

/* static */ nsresult
nsLookAndFeel::GetAccentColorText(nscolor& aColor)
{
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
  // value that seem consistent with what Windows does.

  float luminance = (NS_GET_R(accentColor) * 2 +
                     NS_GET_G(accentColor) * 5 +
                     NS_GET_B(accentColor)) / 8;

  aColor = (luminance <= 128) ? NS_RGB(255, 255, 255) : NS_RGB(0, 0, 0);

  return NS_OK;
}
