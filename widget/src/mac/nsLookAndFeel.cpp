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
 
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsLookAndFeel, kILookAndFeelIID);

nsLookAndFeel::nsLookAndFeel()
{
    NS_INIT_REFCNT();
}

nsLookAndFeel::~nsLookAndFeel()
{
}

NS_IMETHODIMP nsLookAndFeel::GetColor(const nsColorID aID, nscolor &aColor)
{
    nsresult res = NS_OK;
    switch (aID) {
    case eColor_WindowBackground:
        aColor = NS_RGB(0xdd,0xdd,0xdd);
        break;
    case eColor_WindowForeground:
        aColor = NS_RGB(0x00,0x00,0x00);        
        break;
    case eColor_WidgetBackground:
        aColor = NS_RGB(0xdd,0xdd,0xdd);
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
        aColor = NS_RGB(0x00,0x00,0x80);
        break;
    case eColor_TextSelectForeground:
        aColor = NS_RGB(0xff,0xff,0xff);
        break;
    default:
        aColor = NS_RGB(0xff,0xff,0xff);
        res = NS_ERROR_FAILURE;
        break;
    }

    return res;
}

NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricID aID, PRInt32 & aMetric)
{
    nsresult res = NS_OK;
    switch (aID) {
    case eMetric_WindowTitleHeight:
        aMetric = 0;
        break;
    case eMetric_WindowBorderWidth:
        aMetric = 4;
        break;
    case eMetric_WindowBorderHeight:
        aMetric = 4;
        break;
    case eMetric_Widget3DBorder:
        aMetric = 4;
        break;
    case eMetric_TextFieldHeight:
        aMetric = 16;
        break;
    case eMetric_ButtonHorizontalInsidePaddingNavQuirks:
        aMetric = 20;
        break;
    case eMetric_ButtonHorizontalInsidePaddingOffsetNavQuirks:
        aMetric = 0;
        break;
    case eMetric_CheckboxSize:
        aMetric = 14;
        break;
    case eMetric_RadioboxSize:
        aMetric = 14;
        break;
    case eMetric_TextHorizontalInsideMinimumPadding:
        aMetric = 4;
        break;
    case eMetric_TextVerticalInsidePadding:
        aMetric = 4;
        break;
    case eMetric_TextShouldUseVerticalInsidePadding:
        aMetric = 1;
        break;
    case eMetric_TextShouldUseHorizontalInsideMinimumPadding:
        aMetric = 1;
        break;
    case eMetric_ListShouldUseHorizontalInsideMinimumPadding:
        aMetric = 0;
        break;
    case eMetric_ListHorizontalInsideMinimumPadding:
        aMetric = 4;
        break;
    case eMetric_ListShouldUseVerticalInsidePadding:
        aMetric = 1;
        break;
    case eMetric_ListVerticalInsidePadding:
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
        aMetric = 0.08f;
        break;
    case eMetricFloat_ListHorizontalInsidePadding:
        aMetric = 0.40f;
        break;
    case eMetricFloat_ButtonVerticalInsidePadding:
        aMetric = 0.5f;
        break;
    case eMetricFloat_ButtonHorizontalInsidePadding:
        aMetric = 0.5f;
        break;
    default:
        aMetric = -1.0;
        res = NS_ERROR_FAILURE;
    }
  return res;
}

