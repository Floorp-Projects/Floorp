/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include "nsCarbonHelpers.h"
#include "nsIInternetConfigService.h"
#include "nsIServiceManager.h"
#include "nsSize.h"

 
//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------

nsLookAndFeel::nsLookAndFeel() : nsXPLookAndFeel()
{
}

nsLookAndFeel::~nsLookAndFeel()
{
}

nsresult nsLookAndFeel::NativeGetColor(const nsColorID aID, nscolor &aColor)
{
    nsresult res = NS_OK;

    switch (aID) {
    case eColor_WindowBackground:
        {
          nsCOMPtr<nsIInternetConfigService> icService_wb (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
          if (icService_wb)
          {
            res = icService_wb->GetColor(nsIInternetConfigService::eICColor_WebBackgroundColour, &aColor);
            if (NS_SUCCEEDED(res))
              return res;
          }

          aColor = NS_RGB(0xff,0xff,0xff); // default to white if we didn't find it in internet config
          res = NS_OK;
        }
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
        {
          nsCOMPtr<nsIInternetConfigService> icService_tf (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
          if (icService_tf)
          {
            res = icService_tf->GetColor(nsIInternetConfigService::eICColor_WebTextColor, &aColor);
            if (NS_SUCCEEDED(res))
              return res;
          }
          aColor = NS_RGB(0x00,0x00,0x00);
          res = NS_OK;
        }
        break;
    case eColor_highlight: // CSS2 color
    case eColor_TextSelectBackground:
    case eColor__moz_menuhover:
        // XXX can probably just always use GetMacBrushColor here
#ifdef MOZ_WIDGET_COCOA
        res = GetMacBrushColor(kThemeBrushPrimaryHighlightColor, aColor, NS_RGB(0x00,0x00,0x00));
#else
        RGBColor macColor;
        CGrafPtr thePort;
        ::GetPort((GrafPtr*)&thePort);
       	if (thePort)
       	{
          ::GetPortHiliteColor(thePort,&macColor);
          aColor = NS_RGB(macColor.red>>8, macColor.green>>8, macColor.blue>>8);
       	}
       	else
        	aColor = NS_RGB(0x00,0x00,0x00);
#endif
        break;
    case eColor_highlighttext:  // CSS2 color
    case eColor_TextSelectForeground:
    case eColor__moz_menuhovertext:
    		GetColor(eColor_TextSelectBackground, aColor);
    		if (aColor == 0x000000)
					aColor = NS_RGB(0xff,0xff,0xff);
    		else
    			aColor = NS_DONT_CHANGE_COLOR;
        break;

    //
    // css2 system colors http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
    //
    // It's really hard to effectively map these to the Appearance Manager properly,
    // since they are modeled word for word after the win32 system colors and don't have any 
    // real counterparts in the Mac world. I'm sure we'll be tweaking these for 
    // years to come. 
    //
    // Thanks to mpt26@student.canterbury.ac.nz for the hardcoded values that form the defaults
    //	if querying the Appearance Manager fails ;)
    //
    
    case eColor_buttontext:
    case eColor__moz_buttonhovertext:
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
    case eColor__moz_buttonhoverface:
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
    case eColor__moz_fieldtext:
        // XXX There may be a better color for this, but I'm making it
        // the same as WindowText since that's what's currently used where
        // I will use -moz-FieldText.
        res = GetMacTextColor(kThemeTextColorDialogActive, aColor, NS_RGB(0x00,0x00,0x00));
        break;
    case eColor__moz_dialog:
    case eColor__moz_cellhighlight:
        // XXX There may be a better color for this, but I'm making it
        // the same as ThreeDFace since that's what's currently used where
        // I will use -moz-Dialog:
        res = GetMacBrushColor(kThemeBrushButtonFaceActive, aColor, NS_RGB(0xDD,0xDD,0xDD));
        break;
    case eColor__moz_dialogtext:
    case eColor__moz_cellhighlighttext:
        // XXX There may be a better color for this, but I'm making it
        // the same as WindowText since that's what's currently used where
        // I will use -moz-DialogText.
        res = GetMacTextColor(kThemeTextColorDialogActive, aColor, NS_RGB(0x00,0x00,0x00));
        break;
    case eColor__moz_dragtargetzone:
		//default to lavender if not available
		res = GetMacBrushColor(kThemeBrushDragHilite, aColor, NS_RGB(0x63,0x63,0xCE));
	    break;
	case eColor__moz_mac_focusring:
		//default to lavender if not available
		res = GetMacBrushColor(kThemeBrushFocusHighlight, aColor, NS_RGB(0x63,0x63,0xCE));
	    break;
	case eColor__moz_mac_menushadow:
		res = GetMacBrushColor(kThemeBrushBevelActiveDark, aColor, NS_RGB(0x88,0x88,0x88));
	    break;	        
	case eColor__moz_mac_menutextselect:
		//default to white, which is what Platinum uses, if not available		
		res = GetMacTextColor(kThemeTextColorMenuItemSelected, aColor, NS_RGB(0xFF,0xFF,0xFF));
	    break;	    
	case eColor__moz_mac_accentlightesthighlight:
		//get this colour by querying variation table, ows. default to Platinum/Lavendar
		res = GetMacAccentColor(eColorOffset_mac_accentlightesthighlight, aColor, NS_RGB(0xEE,0xEE,0xEE));
	    break;    
    case eColor__moz_mac_accentregularhighlight:
		//get this colour by querying variation table, ows. default to Platinum/Lavendar
		res = GetMacAccentColor(eColorOffset_mac_accentregularhighlight, aColor, NS_RGB(0xCC,0xCC,0xFF));
	    break;        
    case eColor__moz_mac_accentface:
		//get this colour by querying variation table, ows. default to Platinum/Lavendar
		res = GetMacAccentColor(eColorOffset_mac_accentface, aColor, NS_RGB(0x99,0x99,0xFF));
	    break;            
    case eColor__moz_mac_accentlightshadow:
		//get this colour by querying variation table, ows. default to Platinum/Lavendar
		res = GetMacAccentColor(eColorOffset_mac_accentlightshadow, aColor, NS_RGB(0x66,0x66,0xCC));
	    break; 
	case eColor__moz_mac_menuselect:
	case eColor__moz_mac_accentregularshadow:
		//get this colour by querying variation table, ows. default to Platinum/Lavendar
		res = GetMacAccentColor(eColorOffset_mac_accentregularshadow, aColor, NS_RGB(0x63,0x63,0xCE));
	    break;
    case eColor__moz_mac_accentdarkshadow:
    	//get this colour by querying variation table, ows. default to Platinum/Lavendar
		res = GetMacAccentColor(eColorOffset_mac_accentdarkshadow, aColor, NS_RGB(0x00,0x00,0x88));
	    break;
    case eColor__moz_mac_accentdarkestshadow:
    	//get this colour by querying variation table, ows. default to Platinum/Lavendar
		res = GetMacAccentColor(eColorOffset_mac_accentdarkestshadow, aColor, NS_RGB(0x00,0x00,0x55));
	    break;
    case eColor__moz_buttondefault:
        res = GetMacBrushColor(kThemeBrushButtonActiveDarkShadow, aColor, NS_RGB(0x77,0x77,0x77));
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

NS_IMETHODIMP nsLookAndFeel::GetMacAccentColor(	const nsMacAccentColorOffset aAccent, 
												nscolor & aColor,
												const nscolor & aDefaultColor)
{
	nsresult res = NS_OK;
	OSStatus err = noErr;
	ColorTable colourTable;
	CTabPtr ctPointer = &colourTable;
	CTabHandle ctHandle = &ctPointer;
	CTabHandle *colors = &ctHandle;
	RGBColor *macColor;		

	err = ::GetThemeAccentColors(colors);
	if (err == themeHasNoAccentsErr) {
		//fine, theme doesn't support accents, use default
		res = NS_OK;
		aColor = aDefaultColor;
	} else if (err != noErr || ! colors) {
		res = NS_ERROR_FAILURE;
		aColor = aDefaultColor;
	} else {
		if ((**colors)->ctSize == 
				(eColorOffset_mac_accentdarkestshadow - eColorOffset_mac_accentlightesthighlight +1)) {
			//if the size is 7 then its likely to be a standard Platinum variation, so lets use it
			macColor = &((**colors)->ctTable[aAccent].rgb);
			aColor = NS_RGB(macColor->red>>8, macColor->green>>8, macColor->blue>>8);
			res = NS_OK;		
		} else if ((**colors)->ctSize == -1) {
			//this is probably the high contrast Black & White Platinum variation
			//so lets be high contrast
			res = NS_OK;
			if (aAccent <= eColorOffset_mac_accentface) {
				aColor = NS_RGB(0xFF,0xFF,0xFF);
			} else {
				aColor = NS_RGB(0x00,0x00,0x00);
			}
		//else if ( ?? )  // add aqua support here?
		} else {
			//some other size we've never heard of, no idea what to do with it
			res = NS_ERROR_FAILURE;
			aColor = aDefaultColor;
		}
	}
	return res;
}

NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricID aID, PRInt32 & aMetric)
{
    nsresult res = nsXPLookAndFeel::GetMetric(aID, aMetric);
    if (NS_SUCCEEDED(res))
        return res;
    res = NS_OK;

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
    case eMetric_CaretWidth:
        aMetric = 1;
        break;
    case eMetric_ShowCaretDuringSelection:
        aMetric = 0;
        break;
    case eMetric_SelectTextfieldsOnKeyFocus:
        // Select textfield content when focused by kbd
        // used by nsEventStateManager::sTextfieldSelectModel
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
        aMetric = 1;
        break;        
    case eMetric_DragThresholdX:
    case eMetric_DragThresholdY:
        aMetric = 4;
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
        aMetric = 0;
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
