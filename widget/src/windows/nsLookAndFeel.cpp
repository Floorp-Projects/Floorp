/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsLookAndFeel.h"
#include <windows.h>
#include "nsFont.h"
 
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);

NS_IMPL_ISUPPORTS(nsLookAndFeel, NS_ILOOKANDFEEL_IID)

nsLookAndFeel::nsLookAndFeel() : nsILookAndFeel()
{
  NS_INIT_REFCNT();
}

nsLookAndFeel::~nsLookAndFeel()
{
}

NS_IMETHODIMP nsLookAndFeel::GetColor(const nsColorID aID, nscolor &aColor)
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
      idx = COLOR_DESKTOP;
      break;
    case eColor_background:
      idx = COLOR_BACKGROUND; 
      break;
    case eColor_buttonface:
      idx = COLOR_BTNFACE;
      break;
    case eColor_buttonhighlight:
      idx = COLOR_BTNHIGHLIGHT;
      break;
    case eColor_buttonshadow:
      idx = COLOR_BTNSHADOW;
      break;
    case eColor_buttontext:
      idx = COLOR_BTNTEXT;
      break;
    case eColor_captiontext:
      idx = COLOR_CAPTIONTEXT;
      break;
    case eColor_graytext:
      idx = COLOR_GRAYTEXT;
      break;
    case eColor_highlight:
      idx = COLOR_HIGHLIGHT;
      break;
    case eColor_highlighttext:
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
    default:
        idx = COLOR_WINDOW;    
        break;
    }

  aColor = ::GetSysColor(idx);

  return res;
}
  
NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricID aID, PRInt32 & aMetric)
{
  nsresult res = NS_OK;
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
        aMetric = 500;
        break;
    case eMetric_CaretWidthTwips:
        aMetric = 30;
        break;
    default:
        aMetric = -1;
        res = NS_ERROR_FAILURE;
    }
  return res;
}

NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricFloatID aID, float & aMetric)
{
  nsresult res = NS_OK;
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


