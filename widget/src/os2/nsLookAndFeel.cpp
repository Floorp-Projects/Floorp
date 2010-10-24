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
 *   John Fairhurst <john_fairhurst@iname.com>
 *   Michael Lowe <michael.lowe@bigfoot.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#define INCL_WIN
#include <os2.h>
#include "nsLookAndFeel.h"
#include "nsFont.h"
#include "nsSize.h"
 
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
        idx = SYSCLR_WINDOW;
        break;
    case eColor_WindowForeground:
        idx = SYSCLR_WINDOWTEXT;
        break;
    case eColor_WidgetBackground:
        idx = SYSCLR_BUTTONMIDDLE;
        break;
    case eColor_WidgetForeground:
        idx = SYSCLR_WINDOWTEXT; 
        break;
    case eColor_WidgetSelectBackground:
        idx = SYSCLR_HILITEBACKGROUND;
        break;
    case eColor_WidgetSelectForeground:
        idx = SYSCLR_HILITEFOREGROUND;
        break;
    case eColor_Widget3DHighlight:
        idx = SYSCLR_BUTTONLIGHT;
        break;
    case eColor_Widget3DShadow:
        idx = SYSCLR_BUTTONDARK;
        break;
    case eColor_TextBackground:
        idx = SYSCLR_WINDOW;
        break;
    case eColor_TextForeground:
        idx = SYSCLR_WINDOWTEXT;
        break;
    case eColor_TextSelectBackground:
    case eColor_IMESelectedRawTextBackground:
    case eColor_IMESelectedConvertedTextBackground:
        idx = SYSCLR_HILITEBACKGROUND;
        break;
    case eColor_TextSelectForeground:
    case eColor_IMESelectedRawTextForeground:
    case eColor_IMESelectedConvertedTextForeground:
        idx = SYSCLR_HILITEFOREGROUND;
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
      idx = SYSCLR_ACTIVEBORDER;
      break;
    case eColor_activecaption:
      idx = SYSCLR_ACTIVETITLETEXT;
      break;
    case eColor_appworkspace:
      idx = SYSCLR_APPWORKSPACE;
      break;
    case eColor_background:
      idx = SYSCLR_BACKGROUND;
      break;
    case eColor_buttonface:
    case eColor__moz_buttonhoverface:
      idx = SYSCLR_BUTTONMIDDLE;
      break;
    case eColor_buttonhighlight:
      idx = SYSCLR_BUTTONLIGHT;
      break;
    case eColor_buttonshadow:
      idx = SYSCLR_BUTTONDARK;
      break;
    case eColor_buttontext:
    case eColor__moz_buttonhovertext:
      idx = SYSCLR_MENUTEXT;
      break;
    case eColor_captiontext:
      idx = SYSCLR_WINDOWTEXT;
      break;
    case eColor_graytext:
      idx = SYSCLR_MENUDISABLEDTEXT;
      break;
    case eColor_highlight:
    case eColor__moz_html_cellhighlight:
      idx = SYSCLR_HILITEBACKGROUND;
      break;
    case eColor_highlighttext:
    case eColor__moz_html_cellhighlighttext:
      idx = SYSCLR_HILITEFOREGROUND;
      break;
    case eColor_inactiveborder:
      idx = SYSCLR_INACTIVEBORDER;
      break;
    case eColor_inactivecaption:
      idx = SYSCLR_INACTIVETITLE;
      break;
    case eColor_inactivecaptiontext:
      idx = SYSCLR_INACTIVETITLETEXT;
      break;
    case eColor_infobackground:
      aColor = NS_RGB( 255, 255, 228);
      return res;
    case eColor_infotext:
      idx = SYSCLR_WINDOWTEXT;
      break;
    case eColor_menu:
      idx = SYSCLR_MENU;
      break;
    case eColor_menutext:
    case eColor__moz_menubartext:
      idx = SYSCLR_MENUTEXT;
      break;
    case eColor_scrollbar:
      idx = SYSCLR_SCROLLBAR;
      break;
    case eColor_threeddarkshadow:
      idx = SYSCLR_BUTTONDARK;
      break;
    case eColor_threedface:
      idx = SYSCLR_BUTTONMIDDLE;
      break;
    case eColor_threedhighlight:
      idx = SYSCLR_BUTTONLIGHT;
      break;
    case eColor_threedlightshadow:
      idx = SYSCLR_BUTTONMIDDLE;
      break;
    case eColor_threedshadow:
      idx = SYSCLR_BUTTONDARK;
      break;
    case eColor_window:
      idx = SYSCLR_WINDOW;
      break;
    case eColor_windowframe:
      idx = SYSCLR_WINDOWFRAME;
      break;
    case eColor_windowtext:
      idx = SYSCLR_WINDOWTEXT;
      break;
    case eColor__moz_eventreerow:
    case eColor__moz_oddtreerow:
    case eColor__moz_field:
    case eColor__moz_combobox:
      idx = SYSCLR_ENTRYFIELD;
      break;
    case eColor__moz_fieldtext:
    case eColor__moz_comboboxtext:
      idx = SYSCLR_WINDOWTEXT;
      break;
    case eColor__moz_dialog:
    case eColor__moz_cellhighlight:
      idx = SYSCLR_DIALOGBACKGROUND;
      break;
    case eColor__moz_dialogtext:
    case eColor__moz_cellhighlighttext:
      idx = SYSCLR_WINDOWTEXT;
      break;
    case eColor__moz_buttondefault:
      idx = SYSCLR_BUTTONDEFAULT;
      break;
    case eColor__moz_menuhover:
      if (WinQuerySysColor(HWND_DESKTOP, SYSCLR_MENUHILITEBGND, 0) ==
          WinQuerySysColor(HWND_DESKTOP, SYSCLR_MENU, 0)) {
        // if this happens, we would paint menu selections unreadable
        // (we are most likely on Warp3), so let's fake a dark grey
        // background for the selected menu item
        aColor = NS_RGB( 132, 130, 132);
        return res;
      } else {
        idx = SYSCLR_MENUHILITEBGND;
      }
      break;
    case eColor__moz_menuhovertext:
    case eColor__moz_menubarhovertext:
      if (WinQuerySysColor(HWND_DESKTOP, SYSCLR_MENUHILITEBGND, 0) ==
          WinQuerySysColor(HWND_DESKTOP, SYSCLR_MENU, 0)) {
        // white text to be readable on dark grey
        aColor = NS_RGB( 255, 255, 255);
        return res;
      } else {
        idx = SYSCLR_MENUHILITE;
      }
      break;
    case eColor__moz_nativehyperlinktext:
      aColor = NS_RGB( 0, 0, 255);
      return res;
    default:
      idx = SYSCLR_WINDOW;
      break;
  }

  long lColor = WinQuerySysColor( HWND_DESKTOP, idx, 0);

  int iRed = (lColor & RGB_RED) >> 16;
  int iGreen = (lColor & RGB_GREEN) >> 8;
  int iBlue = (lColor & RGB_BLUE);

  aColor = NS_RGB( iRed, iGreen, iBlue);

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
        aMetric = WinQuerySysValue( HWND_DESKTOP, SV_CURSORRATE);
        break;
    case eMetric_CaretWidth:
        aMetric = 1;
        break;
    case eMetric_ShowCaretDuringSelection:
        aMetric = 0;
        break;
    case eMetric_SelectTextfieldsOnKeyFocus:
        // Do not select textfield content when focused by kbd
        // used by nsEventStateManager::sTextfieldSelectModel
        aMetric = 0;
        break;
    case eMetric_SubmenuDelay:
        aMetric = 300;
        break;
    case eMetric_MenusCanOverlapOSBar:
        // we want XUL popups to be able to overlap the task bar.
        aMetric = 1;
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
    case eMetric_DWMCompositor:
    case eMetric_WindowsClassic:
    case eMetric_WindowsDefaultTheme:
    case eMetric_TouchEnabled:
    case eMetric_WindowsThemeIdentifier:
        aMetric = 0;
        res = NS_ERROR_NOT_IMPLEMENTED;
        break;
    case eMetric_MacGraphiteTheme:
    case eMetric_MaemoClassic:
        aMetric = 0;
        res = NS_ERROR_NOT_IMPLEMENTED;
        break;
    case eMetric_IMERawInputUnderlineStyle:
    case eMetric_IMEConvertedTextUnderlineStyle:
        aMetric = NS_UNDERLINE_STYLE_SOLID;
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
   /* Added to avoid warning errors - these are not used right now */
   case eMetricSize_ListBox:
   case eMetricSize_ComboBox:
   case eMetricSize_Radio:
   case eMetricSize_CheckBox:
   case eMetricSize_Button:
      break;
  } //switch

  return NS_OK;

}
#endif
