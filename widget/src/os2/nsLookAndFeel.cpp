/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *  John Fairhurst <john_fairhurst@iname.com>
 *  Michael Lowe <michael.lowe@bigfoot.com>
 *  Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define INCL_WIN
#include <os2.h>
#include "nsLookAndFeel.h"
#include "nsXPLookAndFeel.h"
#include "nsFont.h"
 
NS_IMPL_ISUPPORTS1(nsLookAndFeel, nsILookAndFeel)

nsLookAndFeel::nsLookAndFeel() : nsILookAndFeel()
{
  NS_INIT_REFCNT();

  (void)NS_NewXPLookAndFeel(getter_AddRefs(mXPLookAndFeel));
}

nsLookAndFeel::~nsLookAndFeel()
{
}

NS_IMETHODIMP nsLookAndFeel::GetColor(const nsColorID aID, nscolor &aColor)
{
  nsresult res = NS_OK;
  if (mXPLookAndFeel)
  {
    res = mXPLookAndFeel->GetColor(aID, aColor);
    if (NS_SUCCEEDED(res))
      return res;
    res = NS_OK;
  }

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
        idx = SYSCLR_HILITEBACKGROUND;
        break;
    case eColor_TextSelectForeground:
        idx = SYSCLR_HILITEFOREGROUND;
        break;

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
      idx = SYSCLR_BUTTONMIDDLE;
      break;
    case eColor_buttonhighlight:
      idx = SYSCLR_BUTTONLIGHT;
      break;
    case eColor_buttonshadow:
      idx = SYSCLR_BUTTONDARK;
      break;
    case eColor_buttontext:
      idx = SYSCLR_MENUTEXT;
      break;
    case eColor_captiontext:
      idx = SYSCLR_WINDOWTEXT;
      break;
    case eColor_graytext:
      idx = SYSCLR_MENUDISABLEDTEXT;
      break;
    case eColor_highlight:
      idx = SYSCLR_HILITEBACKGROUND;
      break;
    case eColor_highlighttext:
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
//      idx = SYSCLR_ENTRYFIELD;
//      break;
    case eColor_infotext:
      idx = SYSCLR_WINDOWTEXT;
      break;
    case eColor_menu:
      idx = SYSCLR_MENU;
      break;
    case eColor_menutext:
      idx = SYSCLR_MENUTEXT;
      break;
    case eColor_scrollbar:
      idx = SYSCLR_SCROLLBAR;
      break;
    case eColor_threeddarkshadow:
      idx = SYSCLR_BUTTONDEFAULT;
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
    case eColor__moz_field:
      idx = SYSCLR_WINDOW;
      break;
    case eColor__moz_fieldtext:
      idx = SYSCLR_WINDOWTEXT;
      break;
    case eColor__moz_dialog:
      idx = SYSCLR_BUTTONMIDDLE;
      break;
    case eColor__moz_dialogtext:
      idx = SYSCLR_WINDOWTEXT;
      break;
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
  nsresult res = NS_OK;

  if (mXPLookAndFeel)
  {
    res = mXPLookAndFeel->GetMetric(aID, aMetric);
    if (NS_SUCCEEDED(res))
      return res;
    res = NS_OK;
  }

  switch (aID) {
    case eMetric_WindowTitleHeight:
        aMetric = WinQuerySysValue( HWND_DESKTOP, SV_CYTITLEBAR);
        break;
    case eMetric_WindowBorderWidth:
        aMetric = WinQuerySysValue( HWND_DESKTOP, SV_CXSIZEBORDER);
        break;
    case eMetric_WindowBorderHeight:
        aMetric = WinQuerySysValue( HWND_DESKTOP, SV_CYSIZEBORDER);
        break;
    case eMetric_Widget3DBorder:
        aMetric = WinQuerySysValue( HWND_DESKTOP, SV_CXBORDER);
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
        aMetric = WinQuerySysValue( HWND_DESKTOP, SV_CURSORRATE);
        break;
    case eMetric_SingleLineCaretWidth:
        aMetric = 1;
        break;
    case eMetric_MultiLineCaretWidth:
        aMetric = 2;
        break;
    case eMetric_ShowCaretDuringSelection:
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

    default:
        aMetric = 0;
        res = NS_ERROR_FAILURE;
    }
  return res;
}

NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricFloatID aID, float & aMetric)
{
  nsresult res = NS_OK;

  if (mXPLookAndFeel)
  {
    res = mXPLookAndFeel->GetMetric(aID, aMetric);
    if (NS_SUCCEEDED(res))
      return res;
    res = NS_OK;
  }

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
  if (mXPLookAndFeel)
  {
    nsresult rv = mXPLookAndFeel->GetNavSize(aWidgetID, aFontID, aFontSize, aSize);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

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
