/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsLookAndFeel.h"

#include "nsQApplication.h"
#include <qpalette.h>
#include "nsWidget.h"

#include "nsXPLookAndFeel.h"

static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);

#define GDK_COLOR_TO_NS_RGB(c) \
  ((nscolor) NS_RGB(c.red, c.green, c.blue))

NS_IMPL_ISUPPORTS(nsLookAndFeel, kILookAndFeelIID);

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsLookAndFeel::nsLookAndFeel()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsLookAndFeel::nsLookAndFeel()\n"));
    NS_INIT_REFCNT();
}

nsLookAndFeel::~nsLookAndFeel()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsLookAndFeel::~nsLookAndFeel()\n"));
}

NS_IMETHODIMP nsLookAndFeel::GetColor(const nsColorID aID, nscolor &aColor)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsLookAndFeel::GetColor()\n"));
    nsresult    res         = NS_OK;

    if (mXPLookAndFeel)
    {
        res = mXPLookAndFeel->GetColor(aID, aColor);
        if (NS_SUCCEEDED(res))
            return res;
    }

    QPalette    palette     = qApp->palette();
    QColorGroup normalGroup = palette.normal();
    QColorGroup activeGroup = palette.active();

    switch (aID) 
    {
    case eColor_WindowBackground:
        aColor = normalGroup.background().rgb();
        break;
    case eColor_WindowForeground:
        aColor = normalGroup.foreground().rgb();
        break;
    case eColor_WidgetBackground:
        aColor = normalGroup.background().rgb();
        break;
    case eColor_WidgetForeground:
        aColor = normalGroup.foreground().rgb();
        break;
    case eColor_WidgetSelectBackground:
        aColor = activeGroup.background().rgb();
        break;
    case eColor_WidgetSelectForeground:
        aColor = activeGroup.foreground().rgb();
        break;
    case eColor_Widget3DHighlight:
        aColor = normalGroup.highlight().rgb();
        break;
    case eColor_Widget3DShadow:
        aColor = normalGroup.shadow().rgb();
        break;
    case eColor_TextBackground:
        aColor = normalGroup.background().rgb();
        break;
    case eColor_TextForeground: 
        aColor = normalGroup.text().rgb();
        break;
    case eColor_TextSelectBackground:
        aColor = activeGroup.background().rgb();
        break;
    case eColor_TextSelectForeground:
        aColor = activeGroup.foreground().rgb();
        break;
    default:
        aColor = normalGroup.text().rgb();
        res    = NS_ERROR_FAILURE;
        break;
    }

    return res;
}

NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricID aID, PRInt32 & aMetric)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsLookAndFeel::GetMetric()\n"));
    nsresult res = NS_OK;

    if (mXPLookAndFeel)
    {
        res = mXPLookAndFeel->GetMetric(aID, aMetric);
        if (NS_SUCCEEDED(res))
            return res;
    }

    switch (aID) 
    {
    case eMetric_WindowTitleHeight:
        aMetric = 0;
        break;
    case eMetric_WindowBorderWidth:
        aMetric = 5;
        break;
    case eMetric_WindowBorderHeight:
        aMetric = 5;
        break;
    case eMetric_Widget3DBorder:
// XXX look into this
        aMetric = 4;
        break;
    case eMetric_TextFieldHeight:
//        aMetric = 10;
        aMetric = 15;
        break;
    case eMetric_TextFieldBorder:
        aMetric = 2;
        break;
    case eMetric_TextVerticalInsidePadding:
        aMetric = 0;
        break;
    case eMetric_TextShouldUseVerticalInsidePadding:
        aMetric = 0;
        break;
    case eMetric_TextHorizontalInsideMinimumPadding:
        aMetric = 15;
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
        aMetric = 15;
        break;
    case eMetric_RadioboxSize:
        aMetric = 15;
        break;
    case eMetric_ListShouldUseHorizontalInsideMinimumPadding:
        aMetric = 15;
        break;
    case eMetric_ListHorizontalInsideMinimumPadding:
        aMetric = 15;
        break;
    case eMetric_ListShouldUseVerticalInsidePadding:
        aMetric = 1;
        break;
    case eMetric_ListVerticalInsidePadding:
        aMetric = 1;
        break;
    case eMetric_CaretBlinkTime:
        aMetric = 500;
        break;
    case eMetric_CaretWidthTwips:
        aMetric = 20;
        break;
    case eMetric_SubmenuDelay:
        aMetric = 200;
        break;
    default:
        aMetric = -1;
        res     = NS_ERROR_FAILURE;
    }

    return res;
}

NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricFloatID aID, 
                                       float & aMetric)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsLookAndFeel::GetMetric()\n"));
    nsresult res = NS_OK;

    if (mXPLookAndFeel)
    {
        res = mXPLookAndFeel->GetMetric(aID, aMetric);
        if (NS_SUCCEEDED(res))
            return res;
    }

    switch (aID) 
    {
    case eMetricFloat_TextFieldVerticalInsidePadding:
        aMetric = 0.25f;
        break;
    case eMetricFloat_TextFieldHorizontalInsidePadding:
        aMetric = 0.95f; // large number on purpose so minimum padding is used
        break;
    case eMetricFloat_TextAreaVerticalInsidePadding:
        aMetric = 0.40f;    
        break;
    case eMetricFloat_TextAreaHorizontalInsidePadding:
        aMetric = 0.40f; // large number on purpose so minimum padding is used
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
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif
