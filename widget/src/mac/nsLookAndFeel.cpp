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
#include "nsXPLookAndFeel.h"
#include "nsCarbonHelpers.h"

 
//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------

NS_IMPL_ISUPPORTS1(nsLookAndFeel, nsILookAndFeel);

nsLookAndFeel::nsLookAndFeel()
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
    case eColor_highlight: // CSS2 color
    case eColor_TextSelectBackground:
        GrafPtr thePort;
        ::GetPort(&thePort);
       	if (thePort)
       	{
          RGBColor macColor;
          ::GetPortHiliteColor(thePort,&macColor);
          aColor = NS_RGB(macColor.red>>8, macColor.green>>8, macColor.blue>>8);
       	}
       	else
        	aColor = NS_RGB(0x00,0x00,0x00);
        break;
    case eColor_highlighttext:  // CSS2 color
    case eColor_TextSelectForeground:
    		GetColor(eColor_TextSelectBackground, aColor);
    		if (aColor == 0x000000)
					aColor = NS_RGB(0xff,0xff,0xff);
    		else
    			aColor = NS_DONT_CHANGE_COLOR;
        break;

    //
    // css2 system colors http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
    //
    // Right now, the majority of these colors are just guesses since they are
    // modeled word for word after the win32 system colors and don't have any 
    // real counterparts in the Mac world. I'm sure we'll be tweaking these for 
    // years to come. 
    //
    // Thanks to mpt26@student.canterbury.ac.nz for the hardcoded values ;)
    //
    
    case eColor_buttontext:
    case eColor_captiontext:
    case eColor_menutext:
    case eColor_infotext:
    case eColor_windowtext:
        aColor = NS_RGB(0x0,0x0,0x0);
        break;
    case eColor_activecaption:
        aColor = NS_RGB(0xdd,0xdd,0xdd);
        break;
    case eColor_activeborder:
        aColor = NS_RGB(0xdd,0xdd,0xdd);
        break;
   case eColor_appworkspace:
        // NOTE: this is an MDI color and does not exist on macOS.
        aColor = NS_RGB(0x66,0x99,0xff);
        break;      
    case eColor_background:
        // NOTE: chances are good this is a pattern, not a pure color. What do we do?
        aColor = NS_RGB(0x66,0x99,0xff);
        break;
    case eColor_buttonface:
        aColor = NS_RGB(0xde,0xde,0xde);
        break;
    case eColor_buttonhighlight:
        aColor = NS_RGB(0xff,0xff,0xff);
        break;
    case eColor_buttonshadow:
        aColor = NS_RGB(0x73,0x73,0x73);
        break;
    case eColor_graytext:
        aColor = NS_RGB(0x76,0x76,0x76);
        break;
    case eColor_inactiveborder:
        aColor = NS_RGB(0xdd,0xdd,0xdd);
        break;
    case eColor_inactivecaption:
        aColor = NS_RGB(0xdd,0xdd,0xdd);
        break;
    case eColor_inactivecaptiontext:
        aColor = NS_RGB(0x76,0x76,0x76);
        break;
    case eColor_scrollbar:
        aColor = NS_RGB(0xad,0xad,0xad);
        break;
    case eColor_threeddarkshadow:
    case eColor_threedshadow:
        aColor = NS_RGB(0x9c,0x9c,0x9c);
        break;
    case eColor_threedface:
        aColor = NS_RGB(0xaa,0xaa,0xaa);
        break;
    case eColor_threedhighlight:
        aColor = NS_RGB(0xa0,0xa0,0xa0);
        break;
    case eColor_threedlightshadow:
        aColor = NS_RGB(0xff,0xff,0xff);
        break;
    case eColor_menu:
    case eColor_infobackground:
    case eColor_windowframe:
        aColor = NS_RGB(0xde,0xde,0xde);
        break;
    case eColor_window:
        aColor = NS_RGB(0xff,0xff,0xff);
        break;
    case eColor__moz_field:
        aColor = NS_RGB(0xff,0xff,0xff);
        break;

    default:
        NS_WARNING("Someone asked nsILookAndFeel for a color I don't know about");
        aColor = NS_RGB(0xff,0xff,0xff);
        res = NS_ERROR_FAILURE;
        break;
    }

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
    case eMetric_TextFieldBorder:
        aMetric = 2;
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
    case eMetric_CaretBlinkTime:
        aMetric = ::GetCaretTime() * 1000 / 60;
        break;
    case eMetric_SingleLineCaretWidth:
    case eMetric_MultiLineCaretWidth:
        aMetric = 1;
        break;
    case eMetric_SubmenuDelay:
        aMetric = 200;
        break;
    case eMetric_MenusCanOverlapOSBar:
        // xul popups are not allowed to overlap the menubar.
        aMetric = 0;
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

