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
 *   Sergei Dolgov <sergei_d@fi.tartu.ee>
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
#include "nsFont.h"
#include "nsSize.h"

#include <InterfaceDefs.h>
#include <Menu.h>

nsLookAndFeel::nsLookAndFeel() : nsXPLookAndFeel()
{
}

nsLookAndFeel::~nsLookAndFeel()
{
}

nsresult nsLookAndFeel::NativeGetColor(const nsColorID aID, nscolor &aColor)
{
  nsresult res = NS_OK;
  rgb_color color;
  /*
   * There used to be an entirely separate list of these colors in
   * nsDeviceContextBeOS::GetSystemAttribute.  The colors given there
   * were a bit different from these.  If these are inaccurate, it might
   * be worth looking at cvs history for the ones there to see if they
   * were better.
   */

  switch (aID) {
    case eColor_WindowBackground:
      aColor = NS_RGB(0xff, 0xff, 0xff); 
      break;
    case eColor_WindowForeground:
      aColor = NS_RGB(0x00, 0x00, 0x00);        
      break;
    case eColor_WidgetBackground:
      aColor = NS_RGB(0xdd, 0xdd, 0xdd);
      break;
    case eColor_WidgetForeground:
      aColor = NS_RGB(0x00, 0x00, 0x00);        
      break;
    case eColor_WidgetSelectBackground:
      {
        color = ui_color(B_MENU_SELECTION_BACKGROUND_COLOR);
        aColor = NS_RGB(color.red, color.green, color.blue);
      }
      break;
    case eColor_WidgetSelectForeground:
      aColor = NS_RGB(0x00, 0x00, 0x80);
      break;
    case eColor_Widget3DHighlight:
      aColor = NS_RGB(0xa0, 0xa0, 0xa0);
      break;
    case eColor_Widget3DShadow:
      aColor = NS_RGB(0x40, 0x40, 0x40);
      break;
    case eColor_TextBackground:
      aColor = NS_RGB(0xff, 0xff, 0xff);
      break;
    case eColor_TextForeground: 
      aColor = NS_RGB(0x00, 0x00, 0x00);
      break;
    case eColor_TextSelectBackground:
      {
        // looks good in Mozilla, though, never noticed this color in BeOS menu
        color = ui_color(B_MENU_SELECTION_BACKGROUND_COLOR);
        aColor = NS_RGB(color.red, color.green, color.blue);
      }
      break;
    case eColor_TextSelectForeground:
      {
        color = ui_color(B_MENU_SELECTED_ITEM_TEXT_COLOR);
        aColor = NS_RGB(color.red, color.green, color.blue);
      }
      break;
	// two following colors get initialisation in XPLookAndFeel.
	//eColor_TextSelectBackgroundDisabled,
    //eColor_TextSelectBackgroundAttention,
    
    //  CSS 2 Colors
    case eColor_activeborder:
      aColor = NS_RGB(0x88, 0x88, 0x88);
      break;
    // active titletab
    case eColor_activecaption:
      {
        color = ui_color(B_WINDOW_TAB_COLOR);
        aColor = NS_RGB(color.red, color.green, color.blue);
      }    
      break;
    //MDI color
    case eColor_appworkspace:
      aColor = NS_RGB(0xd8, 0xd8, 0xd8);
      break;
    //incidentally, this is supposed to be the colour of the desktop, though how anyone
    //is supposed to guess that from the name?
    case eColor_background:
      {
        color = ui_color(B_DESKTOP_COLOR);
        aColor = NS_RGB(color.red, color.green, color.blue);
      }
      break;
    case eColor_buttonface:
      aColor = NS_RGB(0xdd, 0xdd, 0xdd);
      break;
    //should be lighter of 2 possible highlight colours available
    case eColor_buttonhighlight:
      aColor = NS_RGB(0xff, 0xff, 0xff);
      break;
    //darker of 2 possible shadow colours available
    case eColor_buttonshadow:
      aColor = NS_RGB(0x77, 0x77, 0x77);
      break;
    case eColor_buttontext:
      aColor = NS_RGB(0x00, 0x00, 0x00);
      break;
    case eColor_captiontext:
      aColor = NS_RGB(0x00, 0x00, 0x00);
      break;
    case eColor_graytext:
      aColor = NS_RGB(0x77, 0x77, 0x77);
      break;
    case eColor_highlight:
      {
        // B_MENU_SELECTION_BACKGROUND_COLOR  is used for text selection
        // this blue colors seems more suitable
        color = ui_color(B_KEYBOARD_NAVIGATION_COLOR);
        aColor = NS_RGB(color.red, color.green, color.blue);
      }
      break;
    case eColor_highlighttext:
      {
        color = ui_color(B_MENU_SELECTED_ITEM_TEXT_COLOR);
        aColor = NS_RGB(color.red, color.green, color.blue);
      }
      break;
    case eColor_inactiveborder:
      aColor = NS_RGB(0x55, 0x55, 0x55);
      break;
    case eColor_inactivecaption:
      aColor = NS_RGB(0xdd, 0xdd, 0xdd);
      break;
    case eColor_inactivecaptiontext:
      aColor = NS_RGB(0x77, 0x77, 0x77);
      break;
    case eColor_infobackground:
      // tooltips
      aColor = NS_RGB(0xff, 0xff, 0xd0);
      break;
    case eColor_infotext:
      aColor = NS_RGB(0x00, 0x00, 0x00);
      break;
    case eColor_menu:
      {
        color = ui_color(B_MENU_BACKGROUND_COLOR);
        aColor = NS_RGB(color.red, color.green, color.blue);
      }
      break;
    case eColor_menutext:
      {
        color = ui_color(B_MENU_ITEM_TEXT_COLOR);
        aColor = NS_RGB(color.red, color.green, color.blue);
      }
      break;
    case eColor_scrollbar:
      aColor = NS_RGB(0xaa, 0xaa, 0xaa);
      break;
    case eColor_threeddarkshadow:
      aColor = NS_RGB(0x77, 0x77, 0x77);
      break;
    case eColor_threedface:
      aColor = NS_RGB(0xdd, 0xdd, 0xdd);
      break;
    case eColor_threedhighlight:
      aColor = NS_RGB(0xff, 0xff, 0xff);
      break;
    case eColor_threedlightshadow:
      aColor = NS_RGB(0xdd, 0xdd, 0xdd);
      break;
    case eColor_threedshadow:
      aColor = NS_RGB(0x99, 0x99, 0x99);
      break;
    case eColor_window:
      aColor = NS_RGB(0xff, 0xff, 0xff);
      break;
    case eColor_windowframe:
      aColor = NS_RGB(0xcc, 0xcc, 0xcc);
      break;
    case eColor_windowtext:
      aColor = NS_RGB(0x00, 0x00, 0x00);
      break;
    // CSS3 candidates
    case eColor__moz_field: 
      // normal widget background
      aColor = NS_RGB(0xff, 0xff, 0xff);
      break;  
    case eColor__moz_fieldtext:
      aColor = NS_RGB(0x00, 0x00, 0x00);
      break;  
    case eColor__moz_dialog:
      //all bars  including MenuBar
      aColor = NS_RGB(0xdd, 0xdd, 0xdd);
      break;  
    case eColor__moz_dialogtext:
      aColor = NS_RGB(0x00, 0x00, 0x00);
      break;  
    case eColor__moz_dragtargetzone:
      aColor = NS_RGB(0x63, 0x63, 0xCE);
      break;
    case eColor__moz_buttondefault:
      aColor = NS_RGB(0x77, 0x77, 0x77);
      break;
    case eColor_LAST_COLOR:
    default:
      aColor = NS_RGB(0xff, 0xff, 0xff);
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
      // 2*horizontal scrollbar height
      aMetric = 28;
      break;
    case eMetric_WindowBorderWidth:
      aMetric = 2;
      break;
    case eMetric_WindowBorderHeight:
      aMetric = 2;
      break;
    case eMetric_Widget3DBorder:
      aMetric = 5;
      break;
    case eMetric_TextFieldBorder:
      aMetric = 3;
      break;
    case eMetric_TextFieldHeight:
      aMetric = 24;
      break;
    case eMetric_TextVerticalInsidePadding:
      aMetric = 0;
      break;    
    case eMetric_TextShouldUseVerticalInsidePadding:
      aMetric = 0;
      break;
    case eMetric_TextHorizontalInsideMinimumPadding:
      aMetric = 3;
      break;
    case eMetric_TextShouldUseHorizontalInsideMinimumPadding:
      aMetric = 1;
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
    case eMetric_ShowCaretDuringSelection:
      aMetric = 1;
      break;
    case eMetric_SelectTextfieldsOnKeyFocus:
      // Do not select textfield content when focused by kbd
      // used by nsEventStateManager::sTextfieldSelectModel
      aMetric = 0;
      break;
    case eMetric_SubmenuDelay:
      aMetric = 500;
      break;
    case eMetric_MenusCanOverlapOSBar: // can popups overlap menu/task bar?
      aMetric = 0;
      break;
    case eMetric_DragFullWindow:
      aMetric = 0;
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
    case eMetric_TreeOpenDelay:
      aMetric = 1000;
      break;
    case eMetric_TreeCloseDelay:
      aMetric = 1000;
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
