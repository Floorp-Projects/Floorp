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
        RGBColor macColor;
        GrafPtr thePort;
        ::GetPort(&thePort);
       	if (thePort)
       	{
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
    	res = GetMacTextColor(kThemeTextColorPushButtonActive, aColor, NS_RGB(0x00,0x00,0x00));
	    break;
    case eColor_captiontext:
    	res = GetMacTextColor(kThemeTextColorWindowHeaderActive, aColor, NS_RGB(0x00,0x00,0x00));
	    break;
    case eColor_menutext:
    	res = GetMacTextColor(kThemeTextColorMenuItemActive, aColor, NS_RGB(0x00,0x00,0x00));
	    break;
    case eColor_infotext:
		//this will only work on MacOS 9. Mac OS < 9 will use hardcoded value
    	res = GetMacTextColor(kThemeTextColorNotification, aColor, NS_RGB(0x00,0x00,0x00));
	    break;    
    case eColor_windowtext:
		//DialogActive is closest match to "windowtext"
    	res = GetMacTextColor(kThemeTextColorDialogActive, aColor, NS_RGB(0x00,0x00,0x00));
	    break;
    case eColor_activecaption:
    	//no way to fetch this colour. HARDCODING to Platinum
 		//res = GetMacBrushColor(??, aColor, NS_RGB(0xCC,0xCC,0xCC));
		//active titlebar etc is #CCCCCC
    	aColor = NS_RGB(0xCC,0xCC,0xCC);
    	res = NS_OK;
	    break;
    case eColor_activeborder:
		//If it means anything at all on Mac OS, then its black in every theme I've tried,
		//but Aqua *has* no border!
    	res = GetMacBrushColor(kThemeBrushBlack, aColor, NS_RGB(0x00,0x00,0x00));
	    break;
   case eColor_appworkspace:
        // NOTE: this is an MDI color and does not exist on macOS.
		//used the closest match, which will likely be white.
    	res = GetMacBrushColor(kThemeBrushDocumentWindowBackground, aColor, NS_RGB(0x63,0x63,0xCE));
	    break;   
    case eColor_background:
        // NOTE: chances are good this is a pattern, not a pure color. What do we do?
        // Seconded. This is almost never going to be a flat colour...
        //incidentally, this is supposed to be the colour of the desktop, though how anyone
        //is supposed to guess that from the name?
        aColor = NS_RGB(0x63,0x63,0xCE);
        res = NS_OK;
        break;
    case eColor_buttonface:
    	res = GetMacBrushColor(kThemeBrushButtonFaceActive, aColor, NS_RGB(0xDD,0xDD,0xDD));
	    break;
    case eColor_buttonhighlight:
		//lighter of 2 possible highlight colours available
    	res = GetMacBrushColor(kThemeBrushButtonActiveLightHighlight, aColor, NS_RGB(0xFF,0xFF,0xFF));
	    break;
    case eColor_buttonshadow:
		//darker of 2 possible shadow colours available
    	res = GetMacBrushColor(kThemeBrushButtonActiveDarkShadow, aColor, NS_RGB(0x77,0x77,0x77));
	    break;
    case eColor_graytext:
    	res = GetMacTextColor(kThemeTextColorDialogInactive, aColor, NS_RGB(0x77,0x77,0x77));
	    break;
    case eColor_inactiveborder:
		//ScrollBar DelimiterInactive looks like an odd constant to use, but gives the right colour in most themes, 
		//also if you look at where this colour is *actually* used, it is pretty much what we want
    	res = GetMacBrushColor(kThemeBrushScrollBarDelimiterInactive, aColor, NS_RGB(0x55,0x55,0x55));
	    break;
    case eColor_inactivecaption:
		//best guess. Usually right in most themes I think
    	res = GetMacBrushColor(kThemeBrushDialogBackgroundInactive, aColor, NS_RGB(0xDD,0xDD,0xDD));
	    break;
    case eColor_inactivecaptiontext:
    	res = GetMacTextColor(kThemeTextColorWindowHeaderInactive, aColor, NS_RGB(0x77,0x77,0x77));
		break;
    case eColor_scrollbar:
        //this is the scrollbar trough. HARDCODING no way to get this colour
 		//res = GetMacBrushColor(??, aColor, NS_RGB(0xAA,0xAA,0xAA));
    	aColor = NS_RGB(0xAA,0xAA,0xAA);
    	res = NS_OK;
	    break;
    case eColor_threeddarkshadow:
		res = GetMacBrushColor(kThemeBrushButtonActiveDarkShadow, aColor, NS_RGB(0x77,0x77,0x77));
	    break;
    case eColor_threedshadow:
		res = GetMacBrushColor(kThemeBrushButtonActiveLightShadow, aColor, NS_RGB(0x99,0x99,0x99));
	    break;
    case eColor_threedface:
		res = GetMacBrushColor(kThemeBrushButtonFaceActive, aColor, NS_RGB(0xDD,0xDD,0xDD));
	    break;
    case eColor_threedhighlight:
		res = GetMacBrushColor(kThemeBrushButtonActiveLightHighlight, aColor, NS_RGB(0xFF,0xFF,0xFF));
	    break;
    case eColor_threedlightshadow:
		//the description in the CSS2 spec says this is on the side facing the lightsource,
		//so its not a shadow in Mac OS terms, its the darker of the highlights 
		res = GetMacBrushColor(kThemeBrushButtonActiveDarkHighlight, aColor, NS_RGB(0xDD,0xDD,0xDD));
	    break;
    case eColor_menu:
		//best guess. Menus have similar background to dialogs in most themes
		res = GetMacBrushColor(kThemeBrushDialogBackgroundActive, aColor, NS_RGB(0xDD,0xDD,0xDD));
	    break;
    case eColor_infobackground:
		//Brush exists on on MacOS 9. Earlier Mac OS will use default Platinum colour
		res = GetMacBrushColor(kThemeBrushNotificationWindowBackground, aColor, NS_RGB(0xFF,0xFF,0xC6));
	    break;
    case eColor_windowframe:
    	//no way to fetch this colour. HARDCODING to Platinum
 		//res = GetMacBrushColor(??, aColor, NS_RGB(0xCC,0xCC,0xCC));
    	aColor = NS_RGB(0xCC,0xCC,0xCC);
    	res = NS_OK;
	    break;
    case eColor_window:
		res = GetMacBrushColor(kThemeBrushDocumentWindowBackground, aColor, NS_RGB(0xFF,0xFF,0xFF));        
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

NS_IMETHODIMP nsLookAndFeel::GetMacBrushColor(	const PRInt32 aBrushType, nscolor & aColor,
												const nscolor & aDefaultColor)
{
	OSStatus err = noErr;
	RGBColor macColor;
	
	err = ::GetThemeBrushAsColor(aBrushType, 32, true, &macColor);
	if (err == noErr) 
		aColor =  NS_RGB(macColor.red>>8, macColor.green>>8, macColor.blue>>8);
	else
		aColor = aDefaultColor;
	return NS_OK;
	
}

NS_IMETHODIMP nsLookAndFeel::GetMacTextColor(const PRInt32 aTextType, nscolor & aColor,
											 const nscolor & aDefaultColor)
{
	OSStatus err = noErr;
	RGBColor macColor;
	
	err = ::GetThemeTextColor(aTextType, 32, true, &macColor);
	if (err == noErr) 
		aColor =  NS_RGB(macColor.red>>8, macColor.green>>8, macColor.blue>>8);
	else
		aColor = aDefaultColor;
	return NS_OK;
	
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
    case eMetric_DragFullWindow:
        aMetric = 0;
        break;        
    case eMetric_ScrollArrowStyle:
        ThemeScrollBarArrowStyle arrowStyle;
        ::GetThemeScrollBarArrowStyle ( &arrowStyle );
        aMetric = arrowStyle;
        break;
    case eMetric_ScrollSliderStyle:
        ThemeScrollBarThumbStyle thumbStyle;
        ::GetThemeScrollBarThumbStyle ( &thumbStyle );
        aMetric = thumbStyle;
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

