/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Lowe <michael.lowe@bigfoot.com>
 *   Jens Bannmann <jens.b@web.de>
 *   Ryan Jones <sciguyryan@gmail.com>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
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

#include "nsLookAndFeel.h"
#include <windows.h>
#include <shellapi.h>
#include "nsWindow.h"
#include "nsStyleConsts.h"
#include "nsUXThemeData.h"
#include "nsUXThemeConstants.h"
#include "gfxFont.h"
#include "WinUtils.h"

using namespace mozilla::widget;
using mozilla::LookAndFeel;

static nsresult GetColorFromTheme(nsUXThemeClass cls,
                           PRInt32 aPart,
                           PRInt32 aState,
                           PRInt32 aPropId,
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

static PRInt32 GetSystemParam(long flag, PRInt32 def)
{
    DWORD value; 
    return ::SystemParametersInfo(flag, 0, &value, 0) ? value : def;
}

nsLookAndFeel::nsLookAndFeel() : nsXPLookAndFeel()
{
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
      if (WinUtils::GetWindowsVersion() < WinUtils::VISTA_VERSION ||
          !IsAppThemed())
      {
        idx = nsUXThemeData::sFlatMenus ?
                COLOR_HIGHLIGHTTEXT :
                COLOR_MENUTEXT;
        break;
      }
      // Fall through
    case eColorID__moz_menuhovertext:
      if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION &&
          IsAppThemed())
      {
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
    case eColorID__moz_win_mediatext:
      if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION &&
          IsAppThemed()) {
        res = ::GetColorFromTheme(eUXMediaToolbar,
                                  TP_BUTTON, TS_NORMAL, TMT_TEXTCOLOR, aColor);
        if (NS_SUCCEEDED(res))
          return res;
      }
      // if we've gotten here just return -moz-dialogtext instead
      idx = COLOR_WINDOWTEXT;
      break;
    case eColorID__moz_win_communicationstext:
      if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION &&
          IsAppThemed())
      {
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
      idx = COLOR_WINDOW;
      break;
    }

  DWORD color = ::GetSysColor(idx);
  aColor = COLOREF_2_NSRGB(color);

  return res;
}

nsresult
nsLookAndFeel::GetIntImpl(IntID aID, PRInt32 &aResult)
{
  nsresult res = nsXPLookAndFeel::GetIntImpl(aID, aResult);
  if (NS_SUCCEEDED(res))
    return res;
  res = NS_OK;

  switch (aID) {
    case eIntID_CaretBlinkTime:
        aResult = (PRInt32)::GetCaretBlinkTime();
        break;
    case eIntID_CaretWidth:
        aResult = 1;
        break;
    case eIntID_ShowCaretDuringSelection:
        aResult = 0;
        break;
    case eIntID_SelectTextfieldsOnKeyFocus:
        // Select textfield content when focused by kbd
        // used by nsEventStateManager::sTextfieldSelectModel
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
        HIGHCONTRAST contrastThemeInfo;
        contrastThemeInfo.cbSize = sizeof(contrastThemeInfo);
        ::SystemParametersInfo(SPI_GETHIGHCONTRAST, 0, &contrastThemeInfo, 0);

        aResult = ((contrastThemeInfo.dwFlags & HCF_HIGHCONTRASTON) != 0);
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
        aResult = 0;
        PRInt32 touchCapabilities;
        touchCapabilities = ::GetSystemMetrics(SM_DIGITIZER);
        if ((touchCapabilities & NID_READY) && 
           (touchCapabilities & (NID_EXTERNAL_TOUCH | NID_INTEGRATED_TOUCH))) {
            aResult = 1;
        }
        break;
    case eIntID_WindowsDefaultTheme:
        aResult = nsUXThemeData::IsDefaultWindowTheme();
        break;
    case eIntID_WindowsThemeIdentifier:
        aResult = nsUXThemeData::GetNativeThemeId();
        break;
    case eIntID_MacGraphiteTheme:
    case eIntID_MacLionTheme:
    case eIntID_MaemoClassic:
        aResult = 0;
        res = NS_ERROR_NOT_IMPLEMENTED;
        break;
    case eIntID_DWMCompositor:
        aResult = nsUXThemeData::CheckForCompositor();
        break;
    case eIntID_AlertNotificationOrigin:
        aResult = 0;
        {
          // Get task bar window handle
          HWND shellWindow = FindWindowW(L"Shell_TrayWnd", NULL);

          if (shellWindow != NULL)
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
  LOGFONTW* ptrLogFont = NULL;
  LOGFONTW logFont;
  NONCLIENTMETRICSW ncm;
  HGDIOBJ hGDI;
  PRUnichar name[LF_FACESIZE];

  // Depending on which stock font we want, there are three different
  // places we might have to look it up.
  switch (anID) {
  case LookAndFeel::eFont_Icon:
    if (!::SystemParametersInfoW(SPI_GETICONTITLELOGFONT,
                                 sizeof(logFont), (PVOID)&logFont, 0))
      return false;

    ptrLogFont = &logFont;
    break;

  case LookAndFeel::eFont_Menu:
  case LookAndFeel::eFont_MessageBox:
  case LookAndFeel::eFont_SmallCaption:
  case LookAndFeel::eFont_StatusBar:
  case LookAndFeel::eFont_Tooltips:
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    if (!::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,
                                 sizeof(ncm), (PVOID)&ncm, 0))
      return false;

    switch (anID) {
    case LookAndFeel::eFont_Menu:
      ptrLogFont = &ncm.lfMenuFont;
      break;
    case LookAndFeel::eFont_MessageBox:
      ptrLogFont = &ncm.lfMessageFont;
      break;
    case LookAndFeel::eFont_SmallCaption:
      ptrLogFont = &ncm.lfSmCaptionFont;
      break;
    case LookAndFeel::eFont_StatusBar:
    case LookAndFeel::eFont_Tooltips:
      ptrLogFont = &ncm.lfStatusFont;
      break;
    }
    break;

  case LookAndFeel::eFont_Widget:
  case LookAndFeel::eFont_Window:      // css3
  case LookAndFeel::eFont_Document:
  case LookAndFeel::eFont_Workspace:
  case LookAndFeel::eFont_Desktop:
  case LookAndFeel::eFont_Info:
  case LookAndFeel::eFont_Dialog:
  case LookAndFeel::eFont_Button:
  case LookAndFeel::eFont_PullDownMenu:
  case LookAndFeel::eFont_List:
  case LookAndFeel::eFont_Field:
  case LookAndFeel::eFont_Caption:
    hGDI = ::GetStockObject(DEFAULT_GUI_FONT);
    if (!hGDI)
      return false;

    if (::GetObjectW(hGDI, sizeof(logFont), &logFont) <= 0)
      return false;

    ptrLogFont = &logFont;
    break;
  }

  // FIXME?: mPixelScale is currently hardcoded to 1.
  float mPixelScale = 1.0f;

  // The lfHeight is in pixels, and it needs to be adjusted for the
  // device it will be displayed on.
  // Screens and Printers will differ in DPI
  //
  // So this accounts for the difference in the DeviceContexts
  // The mPixelScale will be a "1" for the screen and could be
  // any value when going to a printer, for example mPixleScale is
  // 6.25 when going to a 600dpi printer.
  // round, but take into account whether it is negative
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
  pixelHeight *= mPixelScale;

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

  name[0] = 0;
  memcpy(name, ptrLogFont->lfFaceName, LF_FACESIZE*sizeof(PRUnichar));
  aFontName = name;

  return true;
}

bool
nsLookAndFeel::GetFontImpl(FontID anID, nsString &aFontName,
                           gfxFontStyle &aFontStyle)
{
  HDC tdc = GetDC(NULL);
  bool status = GetSysFontInfo(tdc, anID, aFontName, aFontStyle);
  ReleaseDC(NULL, tdc);
  return status;
}

/* virtual */
PRUnichar
nsLookAndFeel::GetPasswordCharacterImpl()
{
#define UNICODE_BLACK_CIRCLE_CHAR 0x25cf
  return UNICODE_BLACK_CIRCLE_CHAR;
}
