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
#include "nsXPLookAndFeel.h"
#include <windows.h>
#include <shellapi.h>
#include "nsWindow.h"

#ifndef WINCE
#include "nsUXThemeData.h"
#include "nsUXThemeConstants.h"

typedef UINT (CALLBACK *SHAppBarMessagePtr)(DWORD, PAPPBARDATA);
SHAppBarMessagePtr gSHAppBarMessage = NULL;
static HINSTANCE gShell32DLLInst = NULL;

static nsresult GetColorFromTheme(nsUXThemeClass cls,
                           PRInt32 aPart,
                           PRInt32 aState,
                           PRInt32 aPropId,
                           nscolor &aColor)
{
  COLORREF color;
  HRESULT hr = nsUXThemeData::GetThemeColor(cls, aPart, aState, aPropId, &color);
  if (hr == S_OK)
  {
    aColor = COLOREF_2_NSRGB(color);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}
#endif

static PRInt32 GetSystemParam(long flag, PRInt32 def)
{
    DWORD value; 
    return ::SystemParametersInfo(flag, 0, &value, 0) ? value : def;
}

nsLookAndFeel::nsLookAndFeel() : nsXPLookAndFeel()
{
#ifndef WINCE
  gShell32DLLInst = LoadLibraryW(L"Shell32.dll");
  if (gShell32DLLInst)
  {
      gSHAppBarMessage = (SHAppBarMessagePtr) GetProcAddress(gShell32DLLInst,
                                                             "SHAppBarMessage");
  }
#endif
}

nsLookAndFeel::~nsLookAndFeel()
{
#ifndef WINCE
   if (gShell32DLLInst)
   {
       FreeLibrary(gShell32DLLInst);
       gShell32DLLInst = NULL;
       gSHAppBarMessage = NULL;
   }
#endif
}

nsresult nsLookAndFeel::NativeGetColor(const nsColorID aID, nscolor &aColor)
{
  nsresult res = NS_OK;

  int idx;
  switch (aID) {
    case eColor_WindowBackground:
        idx = COLOR_WINDOW;
        break;
    case eColor_WindowForeground:
        idx = COLOR_WINDOWTEXT;
        break;
    case eColor_WidgetBackground:
        idx = COLOR_BTNFACE;
        break;
    case eColor_WidgetForeground:
        idx = COLOR_BTNTEXT;
        break;
    case eColor_WidgetSelectBackground:
        idx = COLOR_HIGHLIGHT;
        break;
    case eColor_WidgetSelectForeground:
        idx = COLOR_HIGHLIGHTTEXT;
        break;
    case eColor_Widget3DHighlight:
        idx = COLOR_BTNHIGHLIGHT;
        break;
    case eColor_Widget3DShadow:
        idx = COLOR_BTNSHADOW;
        break;
    case eColor_TextBackground:
        idx = COLOR_WINDOW;
        break;
    case eColor_TextForeground:
        idx = COLOR_WINDOWTEXT;
        break;
    case eColor_TextSelectBackground:
    case eColor_IMESelectedRawTextBackground:
    case eColor_IMESelectedConvertedTextBackground:
        idx = COLOR_HIGHLIGHT;
        break;
    case eColor_TextSelectForeground:
    case eColor_IMESelectedRawTextForeground:
    case eColor_IMESelectedConvertedTextForeground:
        idx = COLOR_HIGHLIGHTTEXT;
        break;
    case eColor_IMERawInputBackground:
    case eColor_IMEConvertedTextBackground:
        aColor = NS_TRANSPARENT;
        return NS_OK;
    case eColor_IMERawInputForeground:
    case eColor_IMEConvertedTextForeground:
        aColor = NS_SAME_AS_FOREGROUND_COLOR;
        return NS_OK;
    case eColor_IMERawInputUnderline:
    case eColor_IMEConvertedTextUnderline:
        aColor = NS_SAME_AS_FOREGROUND_COLOR;
        return NS_OK;
    case eColor_IMESelectedRawTextUnderline:
    case eColor_IMESelectedConvertedTextUnderline:
        aColor = NS_TRANSPARENT;
        return NS_OK;
    case eColor_SpellCheckerUnderline:
        aColor = NS_RGB(0xff, 0, 0);
        return NS_OK;

    // New CSS 2 Color definitions
    case eColor_activeborder:
      idx = COLOR_ACTIVEBORDER;
      break;
    case eColor_activecaption:
      idx = COLOR_ACTIVECAPTION;
      break;
    case eColor_appworkspace:
      idx = COLOR_APPWORKSPACE;
      break;
    case eColor_background:
      idx = COLOR_BACKGROUND;
      break;
    case eColor_buttonface:
    case eColor__moz_buttonhoverface:
      idx = COLOR_BTNFACE;
      break;
    case eColor_buttonhighlight:
      idx = COLOR_BTNHIGHLIGHT;
      break;
    case eColor_buttonshadow:
      idx = COLOR_BTNSHADOW;
      break;
    case eColor_buttontext:
    case eColor__moz_buttonhovertext:
      idx = COLOR_BTNTEXT;
      break;
    case eColor_captiontext:
      idx = COLOR_CAPTIONTEXT;
      break;
    case eColor_graytext:
      idx = COLOR_GRAYTEXT;
      break;
    case eColor_highlight:
    case eColor__moz_html_cellhighlight:
    case eColor__moz_menuhover:
      idx = COLOR_HIGHLIGHT;
      break;
    case eColor__moz_menubarhovertext:
#ifndef WINCE
      if (!nsUXThemeData::sIsVistaOrLater || !nsUXThemeData::isAppThemed())
      {
        idx = nsUXThemeData::sFlatMenus ?
                COLOR_HIGHLIGHTTEXT :
                COLOR_MENUTEXT;
        break;
      }
#endif
      // Fall through
    case eColor__moz_menuhovertext:
#ifndef WINCE
      if (nsUXThemeData::IsAppThemed() && nsUXThemeData::sIsVistaOrLater)
      {
        res = ::GetColorFromTheme(eUXMenu,
                                  MENU_POPUPITEM, MPI_HOT, TMT_TEXTCOLOR, aColor);
        if (NS_SUCCEEDED(res))
          return res;
        // fall through to highlight case
      }
#endif
    case eColor_highlighttext:
    case eColor__moz_html_cellhighlighttext:
      idx = COLOR_HIGHLIGHTTEXT;
      break;
    case eColor_inactiveborder:
      idx = COLOR_INACTIVEBORDER;
      break;
    case eColor_inactivecaption:
      idx = COLOR_INACTIVECAPTION;
      break;
    case eColor_inactivecaptiontext:
      idx = COLOR_INACTIVECAPTIONTEXT;
      break;
    case eColor_infobackground:
      idx = COLOR_INFOBK;
      break;
    case eColor_infotext:
      idx = COLOR_INFOTEXT;
      break;
    case eColor_menu:
      idx = COLOR_MENU;
      break;
    case eColor_menutext:
    case eColor__moz_menubartext:
      idx = COLOR_MENUTEXT;
      break;
    case eColor_scrollbar:
      idx = COLOR_SCROLLBAR;
      break;
    case eColor_threeddarkshadow:
      idx = COLOR_3DDKSHADOW;
      break;
    case eColor_threedface:
      idx = COLOR_3DFACE;
      break;
    case eColor_threedhighlight:
      idx = COLOR_3DHIGHLIGHT;
      break;
    case eColor_threedlightshadow:
      idx = COLOR_3DLIGHT;
      break;
    case eColor_threedshadow:
      idx = COLOR_3DSHADOW;
      break;
    case eColor_window:
      idx = COLOR_WINDOW;
      break;
    case eColor_windowframe:
      idx = COLOR_WINDOWFRAME;
      break;
    case eColor_windowtext:
      idx = COLOR_WINDOWTEXT;
      break;
    case eColor__moz_eventreerow:
    case eColor__moz_oddtreerow:
    case eColor__moz_field:
    case eColor__moz_combobox:
      idx = COLOR_WINDOW;
      break;
    case eColor__moz_fieldtext:
    case eColor__moz_comboboxtext:
      idx = COLOR_WINDOWTEXT;
      break;
    case eColor__moz_dialog:
    case eColor__moz_cellhighlight:
      idx = COLOR_3DFACE;
      break;
    case eColor__moz_win_mediatext:
#ifndef WINCE
      if (nsUXThemeData::IsAppThemed() && nsUXThemeData::sIsVistaOrLater) {
        res = ::GetColorFromTheme(eUXMediaToolbar,
                                  TP_BUTTON, TS_NORMAL, TMT_TEXTCOLOR, aColor);
        if (NS_SUCCEEDED(res))
          return res;
      }
#endif
      // if we've gotten here just return -moz-dialogtext instead
      idx = COLOR_WINDOWTEXT;
      break;
    case eColor__moz_win_communicationstext:
#ifndef WINCE
      if (nsUXThemeData::IsAppThemed() && nsUXThemeData::sIsVistaOrLater)
      {
        res = ::GetColorFromTheme(eUXCommunicationsToolbar,
                                  TP_BUTTON, TS_NORMAL, TMT_TEXTCOLOR, aColor);
        if (NS_SUCCEEDED(res))
          return res;
      }
#endif
      // if we've gotten here just return -moz-dialogtext instead
      idx = COLOR_WINDOWTEXT;
      break;
    case eColor__moz_dialogtext:
    case eColor__moz_cellhighlighttext:
      idx = COLOR_WINDOWTEXT;
      break;
    case eColor__moz_dragtargetzone:
      idx = COLOR_HIGHLIGHTTEXT;
      break;
    case eColor__moz_buttondefault:
      idx = COLOR_3DDKSHADOW;
      break;
    case eColor__moz_nativehyperlinktext:
#ifndef WINCE
      idx = COLOR_HOTLIGHT;
#else
      aColor = NS_RGB(0, 0, 0xee);
      return NS_OK;
#endif
      break;
    default:
      idx = COLOR_WINDOW;
      break;
    }

  DWORD color = ::GetSysColor(idx);
  aColor = COLOREF_2_NSRGB(color);

  return res;
}

NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricID aID, PRInt32 & aMetric)
{
  nsresult res = nsXPLookAndFeel::GetMetric(aID, aMetric);
  if (NS_SUCCEEDED(res))
    return res;
  res = NS_OK;

  switch (aID) {
    case eMetric_CaretBlinkTime:
        aMetric = (PRInt32)::GetCaretBlinkTime();
        break;
    case eMetric_CaretWidth:
        aMetric = 1;
        break;
    case eMetric_ShowCaretDuringSelection:
        aMetric = 0;
        break;
    case eMetric_SelectTextfieldsOnKeyFocus:
        // Select textfield content when focused by kbd
        // used by nsEventStateManager::sTextfieldSelectModel
        aMetric = 1;
        break;
    case eMetric_SubmenuDelay:
        // This will default to the Windows' default
        // (400ms) on error.
#ifndef WINCE
        aMetric = GetSystemParam(SPI_GETMENUSHOWDELAY, 400);
#else
        aMetric = 400;
#endif
        break;
    case eMetric_MenusCanOverlapOSBar:
        // we want XUL popups to be able to overlap the task bar.
        aMetric = 1;
        break;
#ifndef WINCE
    case eMetric_DragThresholdX:
        // The system metric is the number of pixels at which a drag should
        // start.  Our look and feel metric is the number of pixels you can
        // move before starting a drag, so subtract 1.

        aMetric = ::GetSystemMetrics(SM_CXDRAG) - 1;
        break;
    case eMetric_DragThresholdY:
        aMetric = ::GetSystemMetrics(SM_CYDRAG) - 1;
        break;
    case eMetric_UseAccessibilityTheme:
        // High contrast is a misnomer under Win32 -- any theme can be used with it, 
        // e.g. normal contrast with large fonts, low contrast, etc.
        // The high contrast flag really means -- use this theme and don't override it.
        HIGHCONTRAST contrastThemeInfo;
        contrastThemeInfo.cbSize = sizeof(contrastThemeInfo);
        ::SystemParametersInfo(SPI_GETHIGHCONTRAST, 0, &contrastThemeInfo, 0);

        aMetric = ((contrastThemeInfo.dwFlags & HCF_HIGHCONTRASTON) != 0);
        break;
#endif
    case eMetric_ScrollArrowStyle:
        aMetric = eMetric_ScrollArrowStyleSingle;
        break;
    case eMetric_ScrollSliderStyle:
        aMetric = eMetric_ScrollThumbStyleProportional;
        break;
    case eMetric_TreeOpenDelay:
        aMetric = 1000;
        break;
    case eMetric_TreeCloseDelay:
        aMetric = 0;
        break;
    case eMetric_TreeLazyScrollDelay:
        aMetric = 150;
        break;
    case eMetric_TreeScrollDelay:
        aMetric = 100;
        break;
    case eMetric_TreeScrollLinesMax:
        aMetric = 3;
        break;
    case eMetric_WindowsClassic:
#ifndef WINCE
        aMetric = !nsUXThemeData::IsAppThemed();
#else
        aMetric = 0;
#endif
        break;
    case eMetric_TouchEnabled:
        aMetric = 0;
#ifndef WINCE
        PRInt32 touchCapabilities;
        touchCapabilities = ::GetSystemMetrics(SM_DIGITIZER);
        if ((touchCapabilities & NID_READY) && 
           (touchCapabilities & (NID_EXTERNAL_TOUCH | NID_INTEGRATED_TOUCH))) {
            aMetric = 1;
        }
#elif defined(WINCE_WINDOWS_MOBILE)
        WCHAR platformType[MAX_PATH];
        SystemParametersInfo(SPI_GETPLATFORMTYPE, sizeof(platformType),
                             platformType, 0);
        if (!wcscmp(platformType, L"PocketPC"))
            aMetric = 1;
#endif
        break;
    case eMetric_WindowsDefaultTheme:
        aMetric = nsUXThemeData::IsDefaultWindowTheme();
        break;
    case eMetric_WindowsThemeIdentifier:
        aMetric = nsUXThemeData::GetNativeThemeId();
        break;
    case eMetric_MacGraphiteTheme:
    case eMetric_MaemoClassic:
        aMetric = 0;
        res = NS_ERROR_NOT_IMPLEMENTED;
        break;
    case eMetric_DWMCompositor:
#ifndef WINCE
        aMetric = nsUXThemeData::sHaveCompositor;
#else
        aMetric = 0;
#endif
        break;
    case eMetric_AlertNotificationOrigin:
        aMetric = 0;
#ifndef WINCE
        if (gSHAppBarMessage)
        {
          // Get task bar window handle
          HWND shellWindow = FindWindowW(L"Shell_TrayWnd", NULL);

          if (shellWindow != NULL)
          {
            // Determine position
            APPBARDATA appBarData;
            appBarData.hWnd = shellWindow;
            appBarData.cbSize = sizeof(appBarData);
            if (gSHAppBarMessage(ABM_GETTASKBARPOS, &appBarData))
            {
              // Set alert origin as a bit field - see nsILookAndFeel.h
              // 0 represents bottom right, sliding vertically.
              switch(appBarData.uEdge)
              {
                case ABE_LEFT:
                  aMetric = NS_ALERT_HORIZONTAL | NS_ALERT_LEFT;
                  break;
                case ABE_RIGHT:
                  aMetric = NS_ALERT_HORIZONTAL;
                  break;
                case ABE_TOP:
                  aMetric = NS_ALERT_TOP;
                  // fall through for the right-to-left handling.
                case ABE_BOTTOM:
                  // If the task bar is right-to-left,
                  // move the origin to the left
                  if (::GetWindowLong(shellWindow, GWL_EXSTYLE) &
                        WS_EX_LAYOUTRTL)
                    aMetric |= NS_ALERT_LEFT;
                  break;
              }
            }
          }
        }
#endif // WINCE
        break;
    case eMetric_IMERawInputUnderlineStyle:
    case eMetric_IMEConvertedTextUnderlineStyle:
        aMetric = NS_UNDERLINE_STYLE_DASHED;
        break;
    case eMetric_IMESelectedRawTextUnderlineStyle:
    case eMetric_IMESelectedConvertedTextUnderline:
        aMetric = NS_UNDERLINE_STYLE_NONE;
        break;
    case eMetric_SpellCheckerUnderlineStyle:
        aMetric = NS_UNDERLINE_STYLE_WAVY;
        break;
    default:
        aMetric = 0;
        res = NS_ERROR_FAILURE;
    }
  return res;
}

NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricFloatID aID, float & aMetric)
{
  nsresult res = nsXPLookAndFeel::GetMetric(aID, aMetric);
  if (NS_SUCCEEDED(res))
    return res;
  res = NS_OK;

  switch (aID) {
    case eMetricFloat_IMEUnderlineRelativeSize:
        aMetric = 1.0f;
        break;
    case eMetricFloat_SpellCheckerUnderlineRelativeSize:
        aMetric = 1.0f;
        break;
    default:
        aMetric = -1.0;
        res = NS_ERROR_FAILURE;
    }
  return res;
}

/* virtual */
PRUnichar nsLookAndFeel::GetPasswordCharacter()
{
#define UNICODE_BLACK_CIRCLE_CHAR 0x25cf
#ifdef WINCE
  return UNICODE_BLACK_CIRCLE_CHAR;
#else
  static PRUnichar passwordCharacter = 0;
  if (!passwordCharacter) {
    passwordCharacter = '*';
    if (nsUXThemeData::sIsXPOrLater)
      passwordCharacter = UNICODE_BLACK_CIRCLE_CHAR;
  }
  return passwordCharacter;
#endif
}

#ifdef NS_DEBUG

NS_IMETHODIMP nsLookAndFeel::GetNavSize(const nsMetricNavWidgetID aWidgetID,
                                        const nsMetricNavFontID   aFontID, 
                                        const PRInt32             aFontSize, 
                                        nsSize &aSize)
{
  nsresult rv = nsXPLookAndFeel::GetNavSize(aWidgetID, aFontID, aFontSize,
                                            aSize);
  if (NS_SUCCEEDED(rv))
    return rv;

  aSize.width  = 0;
  aSize.height = 0;

  if (aFontSize < 1 || aFontSize > 7) {
    return NS_ERROR_FAILURE;
  }

  PRInt32 kTextFieldWidths[2][7] = {
    {106,147,169,211,253,338,506}, // Courier
    {152,214,237,281,366,495,732}  // sans-serif
  };

  PRInt32 kTextFieldHeights[2][7] = {
    {18,21,24,27,33,45,63}, // Courier
    {18,21,24,27,34,48,67}  // sans-serif
  };

  PRInt32 kTextAreaWidths[2][7] = {
    {121,163,184,226,268,352,520}, // Courier
    {163,226,247,289,373,499,730}  // sans-serif
  };

  PRInt32 kTextAreaHeights[2][7] = {
    {40,44,48,52,60,76,100}, // Courier
    {40,44,48,52,62,80,106}  // sans-serif
  };

  switch (aWidgetID) {
    case eMetricSize_TextField:
      aSize.width  = kTextFieldWidths[aFontID][aFontSize-1];
      aSize.height = kTextFieldHeights[aFontID][aFontSize-1];
      break;
    case eMetricSize_TextArea:
      aSize.width  = kTextAreaWidths[aFontID][aFontSize-1];
      aSize.height = kTextAreaHeights[aFontID][aFontSize-1];
      break;
    default:
      break;
  } //switch

  return NS_OK;

}
#endif
