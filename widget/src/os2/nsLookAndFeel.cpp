/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.       Make more like Windows.
 * 06/21/2000       IBM Corp.       Add SubmenuDelay case in GetMetric
 */

#include "nsLookAndFeel.h"
#include "nsWidgetDefs.h"

#include "nsXPLookAndFeel.h"

static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);

//NS_IMPL_ISUPPORTS(nsLookAndFeel, NS_ILOOKANDFEEL_IID)
NS_IMPL_ISUPPORTS(nsLookAndFeel, NS_GET_IID(nsILookAndFeel))

nsLookAndFeel::nsLookAndFeel() : nsILookAndFeel()
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

  int idx;
  switch (aID) {
    case eColor_WindowBackground:
        idx = SYSCLR_WINDOW;          // white
        break;
    case eColor_WindowForeground:
        idx = SYSCLR_WINDOWTEXT;   // black
        break;
    case eColor_WidgetBackground:
        idx = SYSCLR_BUTTONMIDDLE;  // light gray 
        break;
    case eColor_WidgetForeground:
        idx = SYSCLR_WINDOWTEXT;    // black
        break;
    case eColor_WidgetSelectBackground:
        idx = SYSCLR_HILITEBACKGROUND;    // dark gray (items selected in a control)
        break;
    case eColor_WidgetSelectForeground:
        idx = SYSCLR_HILITEFOREGROUND;    // white (text of items selected in a control)
        break;
    case eColor_Widget3DHighlight:
        idx = SYSCLR_BUTTONLIGHT;   // white (highlight color for 3d for edges facing light source)
        break;
    case eColor_Widget3DShadow:
        idx = SYSCLR_BUTTONDARK;      // dark gray (for edges facing away from light source)
        break;
    case eColor_TextBackground:
        idx = SYSCLR_WINDOW;          // white
        break;
    case eColor_TextForeground:
        idx = SYSCLR_WINDOWTEXT;    // black
        break;
    case eColor_TextSelectBackground:
        idx = SYSCLR_HILITEBACKGROUND;         // dark gray
        break;
    case eColor_TextSelectForeground:
        idx = SYSCLR_HILITEFOREGROUND;        // white
        break;

    // New CSS 2 Color definitions
    case eColor_activeborder:
      idx = SYSCLR_ACTIVEBORDER;         // pale yellow
      break;
    case eColor_activecaption:
      idx = SYSCLR_ACTIVETITLETEXT;   // teal (active window title bar)
      break;
    case eColor_appworkspace:
      idx = SYSCLR_APPWORKSPACE;   // off-white
      break;
    case eColor_background:
      idx = SYSCLR_BACKGROUND;      // light gray
      break;
    case eColor_buttonface:
      idx = SYSCLR_BUTTONMIDDLE;    // light gray
      break;
    case eColor_buttonhighlight:
      idx = SYSCLR_BUTTONLIGHT;        // white
      break;
    case eColor_buttonshadow:
      idx = SYSCLR_BUTTONDARK;        // dark gray
      break;
    case eColor_buttontext:
      idx = SYSCLR_BUTTONDEFAULT;    // black
      break;
    case eColor_captiontext:
      idx = SYSCLR_WINDOWTEXT;          // black???
      break;
    case eColor_graytext:
      idx = SYSCLR_MENUDISABLEDTEXT;         // dark gray
      break;
    case eColor_highlight:
      idx = SYSCLR_HILITEFOREGROUND;    // white???
      break;
    case eColor_highlighttext:
      idx = SYSCLR_WINDOWTEXT;    // black ???
      break;
    case eColor_inactiveborder:
      idx = SYSCLR_INACTIVEBORDER;    // light gray
      break;
    case eColor_inactivecaption:
      idx = SYSCLR_INACTIVETITLE;   // light gray???
      break;
    case eColor_inactivecaptiontext:
      idx = SYSCLR_INACTIVETITLE;      // light gray???
      break;
    case eColor_infobackground:
      idx = SYSCLR_INACTIVETITLE;      // light gray???
      break;
    case eColor_infotext:
      idx = SYSCLR_WINDOWTEXT;    // black ???
      break;
    case eColor_menu:
      idx = SYSCLR_MENU;     // light gray
      break;
    case eColor_menutext:
      idx = SYSCLR_MENUTEXT;        // black
      break;
    case eColor_scrollbar:
      idx = SYSCLR_SCROLLBAR;        // pale gray
      break;
    case eColor_threeddarkshadow:
      idx = SYSCLR_SHADOW;    // dark gray???
      break;
    case eColor_threedface:
      idx = SYSCLR_BUTTONMIDDLE;    // light gray???
      break;
    case eColor_threedhighlight:
      idx = SYSCLR_BUTTONLIGHT;   // white???
      break;
    case eColor_threedlightshadow:
      idx = SYSCLR_BUTTONMIDDLE;    // light gray???
      break;
    case eColor_threedshadow:
      idx = SYSCLR_SHADOW;    // dark gray???
      break;
    case eColor_window:
      idx = SYSCLR_WINDOW;    // white
      break;
    case eColor_windowframe:
      idx = SYSCLR_WINDOWFRAME;   // dark gray
      break;
    case eColor_windowtext:
      idx = SYSCLR_WINDOWTEXT;    // black
      break;
    default:
        idx = SYSCLR_WINDOW;    // white
        break;
    }

  long lColor = WinQuerySysColor( HWND_DESKTOP, idx, 0);

  int iRed = (lColor & RGB_RED) >> 16;
  int iGreen = (lColor & RGB_GREEN) >> 8;
  int iBlue = (lColor & RGB_BLUE);

  aColor = NS_RGB( iRed, iGreen, iBlue);   

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
  }

  long svalue = 0;
  aMetric = 0;
  ULONG ulPels = 0;

  switch (aID) {
    case eMetric_WindowTitleHeight:
        svalue = SV_CYTITLEBAR; 
        break;
    case eMetric_WindowBorderWidth:
        svalue = SV_CXSIZEBORDER;
        break;
    case eMetric_WindowBorderHeight:
        svalue = SV_CYSIZEBORDER; 
        break;
    case eMetric_Widget3DBorder:
        svalue = SV_CXBORDER; 
        break;
    case eMetric_TextFieldBorder:
        aMetric = 3;
        break;
    case eMetric_TextFieldHeight:
        // ??? gModuleData.lHtEntryfield; 
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
    case eMetric_SubmenuDelay:
        aMetric = 300;
        break;
    case eMetric_CaretBlinkTime:
        svalue = SV_CURSORRATE;
        break;
    case eMetric_SingleLineCaretWidth:
    case eMetric_MultiLineCaretWidth:
        // Sigh - this is in 'twips', should be in 'app units', but there's no
        // DC anyway so we can't work it out!

        // The reason for this is that there is some rounding weirdness,
        // such that the 2-pixel caret requested by the windows guys
        // got rounded down to a 1-pixel caret for drawing, because
        // of errors in conversion to twips and back.
        
        ulPels = WinQuerySysValue( HWND_DESKTOP, SV_CYBORDER);

        // With luck, either:
        //   * these metrics will go into nsILookAndFeel, or
        //   * we'll get an nsIDeviceContext here
        //
        // For now, lets assume p2t = 20.
        aMetric = 1 * ulPels;
        break;
// Windows     aMetric = 30;
        break;
    default:
        aMetric = -1;
        res = NS_ERROR_FAILURE;
    }

  if( svalue != 0)
     aMetric = WinQuerySysValue( HWND_DESKTOP, svalue);

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
  }

  switch (aID) {
    case eMetricFloat_TextFieldVerticalInsidePadding:
        aMetric = 0.25f;
        break;
    case eMetricFloat_TextFieldHorizontalInsidePadding:
        aMetric = 1.025f;
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
  printf("nsLookAndFeel::GetNavSize not implemented\n");   // OS2TODO
  return NS_ERROR_NOT_IMPLEMENTED;  
}
#endif
