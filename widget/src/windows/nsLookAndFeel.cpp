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
#include "nsWindow.h"
 
nsLookAndFeel::nsLookAndFeel() : nsXPLookAndFeel()
{
}

nsLookAndFeel::~nsLookAndFeel()
{
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
        idx = COLOR_HIGHLIGHT;
        break;
    case eColor_TextSelectForeground:
        idx = COLOR_HIGHLIGHTTEXT;
        break;

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
    case eColor__moz_menuhover:
      idx = COLOR_HIGHLIGHT;
      break;
    case eColor_highlighttext:
    case eColor__moz_menuhovertext:
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
    case eColor__moz_field:
      idx = COLOR_WINDOW;
      break;
    case eColor__moz_fieldtext:
      idx = COLOR_WINDOWTEXT;
      break;
    case eColor__moz_dialog:
    case eColor__moz_cellhighlight:
      idx = COLOR_3DFACE;
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
    case eMetric_WindowTitleHeight:
        aMetric = ::GetSystemMetrics(SM_CYCAPTION);
        break;
    case eMetric_WindowBorderWidth:
        aMetric = ::GetSystemMetrics(SM_CXFRAME);
        break;
    case eMetric_WindowBorderHeight:
        aMetric = ::GetSystemMetrics(SM_CYFRAME);
        break;
    case eMetric_Widget3DBorder:
        aMetric = ::GetSystemMetrics(SM_CXEDGE);
        break;
    case eMetric_TextFieldBorder:
        aMetric = 3;
        break;
    case eMetric_TextFieldHeight:
        aMetric = 24;
        break;
    case eMetric_ButtonHorizontalInsidePaddingNavQuirks:
        aMetric = 10;
        break;
    case eMetric_ButtonHorizontalInsidePaddingOffsetNavQuirks:
        aMetric = 8;
        break;
    case eMetric_CheckboxSize:
        aMetric = 12;
        break;
    case eMetric_RadioboxSize:
        aMetric = 12;
        break;
    case eMetric_TextHorizontalInsideMinimumPadding:
        aMetric = 3;
        break;
    case eMetric_TextVerticalInsidePadding:
        aMetric = 0;
        break;
    case eMetric_TextShouldUseVerticalInsidePadding:
        aMetric = 0;
        break;
    case eMetric_TextShouldUseHorizontalInsideMinimumPadding:
        aMetric = 1;
        break;
    case eMetric_ListShouldUseHorizontalInsideMinimumPadding:
        aMetric = 0;
        break;
    case eMetric_ListHorizontalInsideMinimumPadding:
        aMetric = 3;
        break;
    case eMetric_ListShouldUseVerticalInsidePadding:
        aMetric = 0;
        break;
    case eMetric_ListVerticalInsidePadding:
        aMetric = 0;
        break;
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
        {
        static PRInt32 sSubmenuDelay = -1;

        if (sSubmenuDelay == -1) {
          HKEY key;
          char value[100];
          DWORD length, type;
          LONG result;

          sSubmenuDelay = 300;

          result = ::RegOpenKeyEx(HKEY_CURRENT_USER, 
                   "Control Panel\\Desktop", 0, KEY_READ, &key);

          if (result == ERROR_SUCCESS) {
            length = sizeof(value);

            result = ::RegQueryValueEx(key, "MenuShowDelay",
                     NULL, &type, (LPBYTE)&value, &length);

            ::RegCloseKey(key);

            if (result == ERROR_SUCCESS) {
              PRInt32 errorCode;
              nsString str; str.AssignWithConversion(value);
              PRInt32 submenuDelay = str.ToInteger(&errorCode);
              if (errorCode == NS_OK) {
                sSubmenuDelay = submenuDelay;
              }
            }
          }
        }
        aMetric = sSubmenuDelay;
        }
        break;
    case eMetric_MenusCanOverlapOSBar:
        // we want XUL popups to be able to overlap the task bar.
        aMetric = 1;
        break;
    case eMetric_DragFullWindow:
        {
        static PRInt32 sDragFullWindow = -1;
        if (sDragFullWindow == -1) {
          HKEY key;
          char value[100];
          DWORD length, type;
          LONG result;


          result = ::RegOpenKeyEx(HKEY_CURRENT_USER, 
                   "Control Panel\\Desktop", 0, KEY_READ, &key);

          if (result == ERROR_SUCCESS) {
            length = sizeof(value);

            result = ::RegQueryValueEx(key, "DragFullWindows",
                     NULL, &type, (LPBYTE)&value, &length);

            ::RegCloseKey(key);

            if (result == ERROR_SUCCESS) {
              PRInt32 errorCode;
              nsString str; str.AssignWithConversion(value);
              sDragFullWindow = str.ToInteger(&errorCode);         
            }
          }
        }        
        aMetric = sDragFullWindow ? 1 : 0;
        }
        break;

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
        // Need to check return from SystemParametersInfo since 
        // SPI_GETHIGHCONTRAST is not supported on Windows NT 
        if (SystemParametersInfo(SPI_GETHIGHCONTRAST, 0, &contrastThemeInfo, 0)) {
          aMetric = (contrastThemeInfo.dwFlags & HCF_HIGHCONTRASTON) != 0;
        }
        else {
          aMetric = 0;
        }
        break;
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
    case eMetricFloat_TextFieldVerticalInsidePadding:
        aMetric = 0.25f;
        break;
    case eMetricFloat_TextFieldHorizontalInsidePadding:
        aMetric = 1.025f;
        break;
    case eMetricFloat_TextAreaVerticalInsidePadding:
        aMetric = 0.40f;
        break;
    case eMetricFloat_TextAreaHorizontalInsidePadding:
        aMetric = 0.40f;
        break;
    case eMetricFloat_ListVerticalInsidePadding:
        aMetric = 0.10f;
        break;
    case eMetricFloat_ListHorizontalInsidePadding:
        aMetric = 0.40f;
        break;
    case eMetricFloat_ButtonVerticalInsidePadding:
        aMetric = 0.25f;
        break;
    case eMetricFloat_ButtonHorizontalInsidePadding:
        aMetric = 0.25f;
        break;
    default:
        aMetric = -1.0;
        res = NS_ERROR_FAILURE;
    }
  return res;
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
  } //switch

  return NS_OK;

}
#endif
