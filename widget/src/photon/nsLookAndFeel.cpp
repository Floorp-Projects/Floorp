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
#include <Pt.h>
#include "nsFont.h"
 
NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);
NS_IMPL_ISUPPORTS(nsLookAndFeel,kILookAndFeelIID);

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

#if 0
  switch( aID )
  {
    case eColor_WindowBackground:
        break;
    case eColor_WindowForeground:
        break;
    case eColor_WidgetBackground:
        break;
    case eColor_WidgetForeground:
        break;
    case eColor_WidgetSelectBackground:
        break;
    case eColor_WidgetSelectForeground:
        break;
    case eColor_Widget3DHighlight:
        break;
    case eColor_Widget3DShadow:
        break;
    case eColor_TextBackground:
        break;
    case eColor_TextForeground:
        break;
    case eColor_TextSelectBackground:
        break;
    case eColor_TextSelectForeground:
        break;
    default:
        break;
    }
#endif

  return res;
}
  
NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricID aID, PRInt32 & aMetric)
{
  nsresult res = NS_OK;

  switch( aID )
  {
    case eMetric_WindowTitleHeight:
        break;
    case eMetric_WindowBorderWidth:
        break;
    case eMetric_WindowBorderHeight:
        break;
    case eMetric_Widget3DBorder:
        break;
    case eMetric_TextFieldHeight:
        break;
    case eMetric_ButtonHorizontalInsidePaddingNavQuirks:
        break;
    case eMetric_ButtonHorizontalInsidePaddingOffsetNavQuirks:
        break;
    case eMetric_CheckboxSize:
        break;
    case eMetric_RadioboxSize:
        break;
    case eMetric_TextHorizontalInsideMinimumPadding:
        break;
    case eMetric_TextVerticalInsidePadding:
        break;
    case eMetric_TextShouldUseVerticalInsidePadding:
        break;
    case eMetric_TextShouldUseHorizontalInsideMinimumPadding:
        break;
    case eMetric_ListShouldUseHorizontalInsideMinimumPadding:
        break;
    case eMetric_ListHorizontalInsideMinimumPadding:
        break;
    case eMetric_ListShouldUseVerticalInsidePadding:
        break;
    case eMetric_ListVerticalInsidePadding:
        break;
    default:
        aMetric = -1;
        res = NS_ERROR_FAILURE;
        break;
    }

  return res;
}

NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricFloatID aID, float & aMetric)
{
  nsresult res = NS_OK;

  switch( aID )
  {
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


