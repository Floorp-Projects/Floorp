/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsLookAndFeel.h"
#include "nsFont.h"

#include <InterfaceDefs.h>
 
nsLookAndFeel::nsLookAndFeel() : nsXPLookAndFeel()
{
}

nsLookAndFeel::~nsLookAndFeel()
{
}

nsresult nsLookAndFeel::NativeGetColor(const nsColorID aID, nscolor &aColor)
{
  nsresult res = NS_OK;

  /*
   * There used to be an entirely separate list of these colors in
   * nsDeviceContextBeOS::GetSystemAttribute.  The colors given there
   * were a bit different from these.  If these are inaccurate, it might
   * be worth looking at cvs history for the ones there to see if they
   * were better.
   */

  int idx;
  switch (aID) {
    case eColor_WindowBackground:
      aColor = NS_RGB(0xff,0xff,0xff);
      break;
    case eColor_WindowForeground:
      aColor = NS_RGB(0x00,0x00,0x00);        
      break;
    case eColor_WidgetBackground:
      aColor = NS_RGB(192, 192, 192);
      break;
    case eColor_WidgetForeground:
      aColor = NS_RGB(0x00,0x00,0x00);        
      break;
    case eColor_WidgetSelectBackground:
      aColor = NS_RGB(0x80,0x80,0x80);
      break;
    case eColor_WidgetSelectForeground:
      aColor = NS_RGB(0x00,0x00,0x80);
      break;
    case eColor_Widget3DHighlight:
      aColor = NS_RGB(0xa0,0xa0,0xa0);
      break;
    case eColor_Widget3DShadow:
      aColor = NS_RGB(0x40,0x40,0x40);
      break;
    case eColor_TextBackground:
      aColor = NS_RGB(0xff,0xff,0xff);
      break;
    case eColor_TextForeground: 
      aColor = NS_RGB(0x00,0x00,0x00);
      break;
    case eColor_TextSelectBackground:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;
    case eColor_TextSelectForeground:
      aColor = NS_RGB(0xff,0xff,0xff);
      break;

    // New CSS 2 Color definitions
    case eColor_activeborder:
      aColor = NS_RGB(0x00,0x00,0xff);
      break;
    case eColor_activecaption:
      aColor = NS_RGB(0x00,0x00,0xff);
      break;
    case eColor_appworkspace:
      aColor = NS_RGB(0xa0,0xa0,0xa0);
      break;
    case eColor_background:
      aColor = NS_RGB(0x80,0x80,0x80);
      break;
    case eColor_buttonface:
      aColor = NS_RGB(0xa0,0xa0,0xa0);
      break;
    case eColor_buttonhighlight:
      aColor = NS_RGB(0xff,0xff,0xff);
      break;
    case eColor_buttonshadow:
      aColor = NS_RGB(0x70,0x70,0x70);
      break;
    case eColor_buttontext:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;
    case eColor_captiontext:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;
    case eColor_graytext:
      aColor = NS_RGB(0x50,0x50,0x50);
      break;
    case eColor_highlight:
      aColor = NS_RGB(0xe0,0xc0,0xff);
      break;
    case eColor_highlighttext:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;
    case eColor_inactiveborder:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;
    case eColor_inactivecaption:
      aColor = NS_RGB(0xa0,0xa0,0xa0);
      break;
    case eColor_inactivecaptiontext:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;
    case eColor_infobackground:
      aColor = NS_RGB(0xff,0xff,0xff);
      break;
    case eColor_infotext:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;
    case eColor_menu:
      aColor = NS_RGB(0xa0,0xa0,0xa0);
      break;
    case eColor_menutext:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;
    case eColor_scrollbar:
      aColor = NS_RGB(0xa0,0xa0,0xa0);
      break;
    case eColor_threeddarkshadow:
      aColor = NS_RGB(0x50,0x50,0x50);
      break;
    case eColor_threedface:
      aColor = NS_RGB(0xd8,0xd8,0xd8);
      break;
    case eColor_threedhighlight:
      aColor = NS_RGB(0xf0,0xf0,0xf0);
      break;
    case eColor_threedlightshadow:
      aColor = NS_RGB(0xa0,0xa0,0xa0);
      break;
    case eColor_threedshadow:
      aColor = NS_RGB(0x80,0x80,0x80);
      break;
    case eColor_window:
      aColor = NS_RGB(0xd8,0xd8,0xd8);
      break;
    case eColor_windowframe:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;
    case eColor_windowtext:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;
    case eColor__moz_field: /* normal widget background */
      aColor = NS_RGB(0xff,0xff,0xff);
      break;  
    case eColor__moz_fieldtext:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;  
    case eColor__moz_dialog:
      aColor = NS_RGB(0xd8,0xd8,0xd8);
      break;  
    case eColor__moz_dialogtext:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;  
 
    default:
      aColor = NS_RGB(0x00,0x00,0x00);
      res    = NS_ERROR_FAILURE;
      break;
  }

  return res;
}
  
NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricID aID, PRInt32 & aMetric)
{
  nsresult res = nsXPLookAndFeel::GetMetric(aID, aMetric);
  if (NS_SUCCEEDED(res))
    return res;
  res = NS_OK;

  /*
   * There used to be an entirely separate list of these metrics in
   * nsDeviceContextBeOS::GetSystemAttribute.  The metrics given there
   * were a bit different from these.  If these are inaccurate, it might
   * be worth looking at cvs history for the ones there to see if they
   * were better.
   */

  switch (aID) 
  {
    case eMetric_WindowTitleHeight:
      aMetric = 0;
      break;
    case eMetric_WindowBorderWidth:
      aMetric = 0;
      break;
    case eMetric_WindowBorderHeight:
      aMetric = 0;
      break;
    case eMetric_Widget3DBorder:
      aMetric = 0;
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
      aMetric = 500;
      break;
    case eMetric_SingleLineCaretWidth:
      aMetric = 1;
      break;
    case eMetric_MultiLineCaretWidth:
      aMetric = 2;
      break;
    case eMetric_SubmenuDelay:
      aMetric = 200;
      break;
    case eMetric_ScrollArrowStyle:
      {
        aMetric = eMetric_ScrollArrowStyleBothAtEachEnd; // BeOS default

        scroll_bar_info info;
        if(B_OK == get_scroll_bar_info(&info) && !info.double_arrows)
        {
          aMetric = eMetric_ScrollArrowStyleSingle;
        }
      }
      break;
    case eMetric_ScrollSliderStyle:
      {
        aMetric = eMetric_ScrollThumbStyleProportional; // BeOS default

        scroll_bar_info info;
        if(B_OK == get_scroll_bar_info(&info) && !info.proportional)
        {
          aMetric = eMetric_ScrollThumbStyleNormal;
        }
      }
      break;
    default:
        aMetric = -1;
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
        aMetric = 0.95f;
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
