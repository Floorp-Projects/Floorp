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
#include "nsWidget.h"

#include "nsXPLookAndFeel.h"

static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);

NS_IMPL_ISUPPORTS(nsLookAndFeel, kILookAndFeelIID)

nsLookAndFeel::nsLookAndFeel() : nsILookAndFeel()
{
    NS_INIT_REFCNT();

    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsLookAndFeel::nsLookAndFeel()\n"));
}

nsLookAndFeel::~nsLookAndFeel()
{
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsLookAndFeel::~nsLookAndFeel()\n"));
}

NS_IMETHODIMP nsLookAndFeel::GetColor(const nsColorID aID, nscolor &aColor)
{
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsLookAndFeel::GetColor()\n"));
    nsresult res = NS_OK;

    if (mXPLookAndFeel)
    {
        nsresult res;
        res = mXPLookAndFeel->GetColor(aID, aColor);
        if (NS_SUCCEEDED(res))
            return res;
        res = NS_OK;
    }

    aColor = 0;
  switch (aID) {
  case eColor_WindowBackground:
    aColor = NS_RGB(0xc0, 0xc0, 0xc0);
    break;
  case eColor_WindowForeground:
    aColor = NS_RGB(0x00, 0x00, 0x00);
    break;
  case eColor_WidgetBackground:
    aColor = NS_RGB(0xc0, 0xc0, 0xc0);
    break;
  case eColor_WidgetForeground:
    aColor = NS_RGB(0x00, 0x00, 0x00);
    break;
  case eColor_WidgetSelectBackground:
    aColor = NS_RGB(0x00, 0x00, 0x9c);
    break;
  case eColor_WidgetSelectForeground:
    aColor = NS_RGB(0xff, 0xff, 0xff);
    break;
  case eColor_Widget3DHighlight:
    aColor = NS_RGB(0xa0,0xa0,0xa0);
    break;
  case eColor_Widget3DShadow:
    aColor = NS_RGB(0x40,0x40,0x40);
    break;
  case eColor_TextBackground:
    aColor = NS_RGB(0xff, 0xff, 0xff);
    break;
  case eColor_TextForeground:
    aColor = NS_RGB(0x00, 0x00, 0x00);
    break;
  case eColor_TextSelectBackground:
    aColor = NS_RGB(0x00, 0x00, 0x9c);
    break;
  case eColor_TextSelectForeground:
    aColor = NS_RGB(0xff, 0xff, 0xff);
    break;

    // css2  http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
  case eColor_activeborder:
    aColor = NS_RGB(0x00, 0x00, 0x00);
    break;
  case eColor_activecaption:
    aColor = NS_RGB(0xc0, 0xc0, 0xc0);
    break;
  case eColor_appworkspace:
    aColor = NS_RGB(0xc0, 0xc0, 0xc0);
    break;
  case eColor_background:
    aColor = NS_RGB(0xc0, 0xc0, 0xc0);
    break;

  case eColor_captiontext:
    aColor = NS_RGB(0x00, 0x00, 0x00);
    break;
  case eColor_graytext:
    aColor = NS_RGB(0x80, 0x80, 0x80);
    break;
  case eColor_highlight:
    aColor = NS_RGB(0x00, 0x00, 0x9c);
    break;
  case eColor_highlighttext:
    aColor = NS_RGB(0xff, 0xff, 0xff);
    break;
  case eColor_inactiveborder:
    aColor = NS_RGB(0xc0, 0xc0, 0xc0);
    break;
  case eColor_inactivecaption:
    aColor = NS_RGB(0x80, 0x80, 0x80);
    break;
  case eColor_inactivecaptiontext:
    aColor = NS_RGB(0xff, 0xff, 0xff);
    break;
  case eColor_infobackground:
    aColor = NS_RGB(0xc0, 0xc0, 0xc0);
    break;
  case eColor_infotext:
    aColor = NS_RGB(0x00, 0x00, 0x00);
    break;
  case eColor_menu:
    aColor = NS_RGB(0xc0, 0xc0, 0xc0);
    break;
  case eColor_menutext:
    aColor = NS_RGB(0x00, 0x00, 0x00);
    break;
  case eColor_scrollbar:
    aColor = NS_RGB(0xc0, 0xc0, 0xc0);
    break;

  case eColor_threedface:
  case eColor_buttonface:
    aColor = NS_RGB(0xc0, 0xc0, 0xc0);
    break;

  case eColor_buttonhighlight:  // ?
  case eColor_threedhighlight:
    aColor = NS_RGB(0xa0, 0xa0, 0xa0);
    break;

  case eColor_buttontext:
    aColor = NS_RGB(0x00, 0x00, 0x00);
    break;

  case eColor_buttonshadow:
  case eColor_threeddarkshadow:
  case eColor_threedshadow: // i think these should be the same
    aColor = NS_RGB(0x60, 0x60, 0x60);
    break;

  case eColor_threedlightshadow:
    aColor = NS_RGB(0xa0, 0xa0, 0xa0);
    break;

  case eColor_window:
  case eColor_windowframe:
    aColor = NS_RGB(0xc0, 0xc0, 0xc0);
    break;

  case eColor_windowtext:
    aColor = NS_RGB(0x00, 0x00, 0x00);
    break;

  default:
    /* default color is BLACK */
    aColor = 0;
    res    = NS_ERROR_FAILURE;
    break;
  }
    return NS_OK;
}

NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricID aID, PRInt32 & aMetric)
{
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsLookAndFeel::GetMetric()\n"));
    // XXX These are not complete!
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
        aMetric = 20;
        break;
    case eMetric_WindowBorderWidth:
        aMetric = 1;
        break;
    case eMetric_WindowBorderHeight:
        aMetric = 1;
        break;
    case eMetric_Widget3DBorder:
        aMetric = 1;
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
        aMetric = 15;
        break;
    case eMetric_RadioboxSize:
        aMetric = 15;
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
    case eMetric_MultiLineCaretWidth:
        aMetric = 1;
        break;
    case eMetric_SubmenuDelay:
        aMetric = 200;
        break;
    default:
        aMetric = -1;
        res = NS_ERROR_FAILURE;
    }
    return res;
}

NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricFloatID aID, float & aMetric)
{
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsLookAndFeel::GetMetric()\n"));

    // XXX this is not complete
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
