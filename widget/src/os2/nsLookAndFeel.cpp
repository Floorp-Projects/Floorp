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
#include "nsStyleConsts.h"

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
        idx = SYSCLR_WINDOW;
        break;
    case eColorID_WindowForeground:
        idx = SYSCLR_WINDOWTEXT;
        break;
    case eColorID_WidgetBackground:
        idx = SYSCLR_BUTTONMIDDLE;
        break;
    case eColorID_WidgetForeground:
        idx = SYSCLR_WINDOWTEXT; 
        break;
    case eColorID_WidgetSelectBackground:
        idx = SYSCLR_HILITEBACKGROUND;
        break;
    case eColorID_WidgetSelectForeground:
        idx = SYSCLR_HILITEFOREGROUND;
        break;
    case eColorID_Widget3DHighlight:
        idx = SYSCLR_BUTTONLIGHT;
        break;
    case eColorID_Widget3DShadow:
        idx = SYSCLR_BUTTONDARK;
        break;
    case eColorID_TextBackground:
        idx = SYSCLR_WINDOW;
        break;
    case eColorID_TextForeground:
        idx = SYSCLR_WINDOWTEXT;
        break;
    case eColorID_TextSelectBackground:
    case eColorID_IMESelectedRawTextBackground:
    case eColorID_IMESelectedConvertedTextBackground:
        idx = SYSCLR_HILITEBACKGROUND;
        break;
    case eColorID_TextSelectForeground:
    case eColorID_IMESelectedRawTextForeground:
    case eColorID_IMESelectedConvertedTextForeground:
        idx = SYSCLR_HILITEFOREGROUND;
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
      idx = SYSCLR_ACTIVEBORDER;
      break;
    case eColorID_activecaption:
      idx = SYSCLR_ACTIVETITLETEXT;
      break;
    case eColorID_appworkspace:
      idx = SYSCLR_APPWORKSPACE;
      break;
    case eColorID_background:
      idx = SYSCLR_BACKGROUND;
      break;
    case eColorID_buttonface:
    case eColorID__moz_buttonhoverface:
      idx = SYSCLR_BUTTONMIDDLE;
      break;
    case eColorID_buttonhighlight:
      idx = SYSCLR_BUTTONLIGHT;
      break;
    case eColorID_buttonshadow:
      idx = SYSCLR_BUTTONDARK;
      break;
    case eColorID_buttontext:
    case eColorID__moz_buttonhovertext:
      idx = SYSCLR_MENUTEXT;
      break;
    case eColorID_captiontext:
      idx = SYSCLR_WINDOWTEXT;
      break;
    case eColorID_graytext:
      idx = SYSCLR_MENUDISABLEDTEXT;
      break;
    case eColorID_highlight:
    case eColorID__moz_html_cellhighlight:
      idx = SYSCLR_HILITEBACKGROUND;
      break;
    case eColorID_highlighttext:
    case eColorID__moz_html_cellhighlighttext:
      idx = SYSCLR_HILITEFOREGROUND;
      break;
    case eColorID_inactiveborder:
      idx = SYSCLR_INACTIVEBORDER;
      break;
    case eColorID_inactivecaption:
      idx = SYSCLR_INACTIVETITLE;
      break;
    case eColorID_inactivecaptiontext:
      idx = SYSCLR_INACTIVETITLETEXT;
      break;
    case eColorID_infobackground:
      aColor = NS_RGB( 255, 255, 228);
      return res;
    case eColorID_infotext:
      idx = SYSCLR_WINDOWTEXT;
      break;
    case eColorID_menu:
      idx = SYSCLR_MENU;
      break;
    case eColorID_menutext:
    case eColorID__moz_menubartext:
      idx = SYSCLR_MENUTEXT;
      break;
    case eColorID_scrollbar:
      idx = SYSCLR_SCROLLBAR;
      break;
    case eColorID_threeddarkshadow:
      idx = SYSCLR_BUTTONDARK;
      break;
    case eColorID_threedface:
      idx = SYSCLR_BUTTONMIDDLE;
      break;
    case eColorID_threedhighlight:
      idx = SYSCLR_BUTTONLIGHT;
      break;
    case eColorID_threedlightshadow:
      idx = SYSCLR_BUTTONMIDDLE;
      break;
    case eColorID_threedshadow:
      idx = SYSCLR_BUTTONDARK;
      break;
    case eColorID_window:
      idx = SYSCLR_WINDOW;
      break;
    case eColorID_windowframe:
      idx = SYSCLR_WINDOWFRAME;
      break;
    case eColorID_windowtext:
      idx = SYSCLR_WINDOWTEXT;
      break;
    case eColorID__moz_eventreerow:
    case eColorID__moz_oddtreerow:
    case eColorID__moz_field:
    case eColorID__moz_combobox:
      idx = SYSCLR_ENTRYFIELD;
      break;
    case eColorID__moz_fieldtext:
    case eColorID__moz_comboboxtext:
      idx = SYSCLR_WINDOWTEXT;
      break;
    case eColorID__moz_dialog:
    case eColorID__moz_cellhighlight:
      idx = SYSCLR_DIALOGBACKGROUND;
      break;
    case eColorID__moz_dialogtext:
    case eColorID__moz_cellhighlighttext:
      idx = SYSCLR_WINDOWTEXT;
      break;
    case eColorID__moz_buttondefault:
      idx = SYSCLR_BUTTONDEFAULT;
      break;
    case eColorID__moz_menuhover:
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
    case eColorID__moz_menuhovertext:
    case eColorID__moz_menubarhovertext:
      if (WinQuerySysColor(HWND_DESKTOP, SYSCLR_MENUHILITEBGND, 0) ==
          WinQuerySysColor(HWND_DESKTOP, SYSCLR_MENU, 0)) {
        // white text to be readable on dark grey
        aColor = NS_RGB( 255, 255, 255);
        return res;
      } else {
        idx = SYSCLR_MENUHILITE;
      }
      break;
    case eColorID__moz_nativehyperlinktext:
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
  
nsresult
nsLookAndFeel::GetIntImpl(IntID aID, PRInt32 &aResult)
{
  nsresult res = nsXPLookAndFeel::GetIntImpl(aID, aResult);
  if (NS_SUCCEEDED(res))
      return res;
  res = NS_OK;

  switch (aID) {
    case eIntID_CaretBlinkTime:
        aResult = WinQuerySysValue( HWND_DESKTOP, SV_CURSORRATE);
        break;
    case eIntID_CaretWidth:
        aResult = 1;
        break;
    case eIntID_ShowCaretDuringSelection:
        aResult = 0;
        break;
    case eIntID_SelectTextfieldsOnKeyFocus:
        // Do not select textfield content when focused by kbd
        // used by nsEventStateManager::sTextfieldSelectModel
        aResult = 0;
        break;
    case eIntID_SubmenuDelay:
        aResult = 300;
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
    case eIntID_DWMCompositor:
    case eIntID_WindowsClassic:
    case eIntID_WindowsDefaultTheme:
    case eIntID_TouchEnabled:
    case eIntID_WindowsThemeIdentifier:
        aResult = 0;
        res = NS_ERROR_NOT_IMPLEMENTED;
        break;
    case eIntID_MacGraphiteTheme:
    case eIntID_MacLionTheme:
    case eIntID_MaemoClassic:
        aResult = 0;
        res = NS_ERROR_NOT_IMPLEMENTED;
        break;
    case eIntID_IMERawInputUnderlineStyle:
    case eIntID_IMEConvertedTextUnderlineStyle:
        aResult = NS_STYLE_TEXT_DECORATION_STYLE_SOLID;
        break;
    case eIntID_IMESelectedRawTextUnderlineStyle:
    case eIntID_IMESelectedConvertedTextUnderline:
        aResult = NS_STYLE_TEXT_DECORATION_STYLE_NONE;
        break;
    case eIntID_SpellCheckerUnderlineStyle:
        aResult = NS_STYLE_TEXT_DECORATION_STYLE_WAVY;
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
