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

#ifndef __nsILookAndFeel
#define __nsILookAndFeel
#include "nsISupports.h"
#include "nsColor.h"

  // for |#ifdef NS_DEBUG|
struct nsSize;


// {BEC234D0-AAA5-430D-8435-B10100F78003}
#define NS_ILOOKANDFEEL_IID \
{ 0xbec234d0, 0xaaa5, 0x430d, \
    { 0x84, 0x35, 0xb1, 0x01, 0x00, 0xf7, 0x80, 0x03} }


class nsILookAndFeel: public nsISupports {
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_ILOOKANDFEEL_IID)

  // When modifying this list, also modify nsXPLookAndFeel::sColorPrefs
  // in widget/src/xpwidgts/nsXPLookAndFeel.cpp.
  typedef enum {

    // WARNING : NO NEGATIVE VALUE IN THIS ENUMERATION
    // see patch in bug 57757 for more information

    eColor_WindowBackground,
    eColor_WindowForeground,
    eColor_WidgetBackground,
    eColor_WidgetForeground,
    eColor_WidgetSelectBackground,
    eColor_WidgetSelectForeground,
    eColor_Widget3DHighlight,
    eColor_Widget3DShadow,
    eColor_TextBackground,
    eColor_TextForeground,
    eColor_TextSelectBackground,
    eColor_TextSelectForeground,
    eColor_TextSelectBackgroundDisabled,
    eColor_TextSelectBackgroundAttention,

    eColor_IMERawInputBackground,
    eColor_IMERawInputForeground,
    eColor_IMERawInputUnderline,
    eColor_IMESelectedRawTextBackground,
    eColor_IMESelectedRawTextForeground,
    eColor_IMESelectedRawTextUnderline,
    eColor_IMEConvertedTextBackground,
    eColor_IMEConvertedTextForeground,
    eColor_IMEConvertedTextUnderline,
    eColor_IMESelectedConvertedTextBackground,
    eColor_IMESelectedConvertedTextForeground,
    eColor_IMESelectedConvertedTextUnderline,

    // New CSS 2 color definitions
    eColor_activeborder,
    eColor_activecaption,
    eColor_appworkspace,
    eColor_background,
    eColor_buttonface,
    eColor_buttonhighlight,
    eColor_buttonshadow,
    eColor_buttontext,
    eColor_captiontext,
    eColor_graytext,
    eColor_highlight,
    eColor_highlighttext,
    eColor_inactiveborder,
    eColor_inactivecaption,
    eColor_inactivecaptiontext,
    eColor_infobackground,
    eColor_infotext,
    eColor_menu,
    eColor_menutext,
    eColor_scrollbar,
    eColor_threeddarkshadow,
    eColor_threedface,
    eColor_threedhighlight,
    eColor_threedlightshadow,
    eColor_threedshadow,
    eColor_window,
    eColor_windowframe,
    eColor_windowtext,

    eColor__moz_buttondefault,
    // Colors which will hopefully become CSS3
    eColor__moz_field,
    eColor__moz_fieldtext,
    eColor__moz_dialog,
    eColor__moz_dialogtext,
    eColor__moz_dragtargetzone,				//used to highlight valid regions to drop something onto

    eColor__moz_cellhighlight,                               //used to cell text background, selected but not focus
    eColor__moz_cellhighlighttext,                           //used to cell text, selected but not focus
    eColor__moz_buttonhoverface,                             //used to button text background, when mouse is over
    eColor__moz_buttonhovertext,                             //used to button text, when mouse is over
    eColor__moz_menuhover,                                   //used to menu item background, when mouse is over
    eColor__moz_menuhovertext,                               //used to menu item text, when mouse is over
    eColor__moz_menubarhovertext,                            //used to menu bar item text, when mouse is over

    //colours needed by Mac Classic skin
    eColor__moz_mac_focusring,				//ring around text fields and lists
    eColor__moz_mac_menuselect,				//colour used when mouse is over a menu item
    eColor__moz_mac_menushadow,				//colour used to do shadows on menu items
    eColor__moz_mac_menutextdisable,                    // color used to display text for disabled menu items
    eColor__moz_mac_menutextselect,			//colour used to display text while mouse is over a menu item

  	//all of the accent colours
  	eColor__moz_mac_accentlightesthighlight,
    eColor__moz_mac_accentregularhighlight,
    eColor__moz_mac_accentface,
    eColor__moz_mac_accentlightshadow,
    eColor__moz_mac_accentregularshadow,
    eColor__moz_mac_accentdarkshadow,
    eColor__moz_mac_accentdarkestshadow,
    
    //new in 10.2
    eColor__moz_mac_alternateprimaryhighlight, //active list highlight
    eColor__moz_mac_secondaryhighlight,        //inactive light hightlight
  
    // keep this one last, please
    eColor_LAST_COLOR
  } nsColorID;

  // When modifying this list, also modify nsXPLookAndFeel::sIntPrefs
  // in widget/src/xpwidgts/nsXPLookAndFeel.cpp.
  typedef enum {
    eMetric_WindowTitleHeight,
    eMetric_WindowBorderWidth,
    eMetric_WindowBorderHeight,
    eMetric_Widget3DBorder,
    eMetric_TextFieldBorder,                              // Native border size
    eMetric_TextFieldHeight,
    eMetric_TextVerticalInsidePadding,                    // needed only because of GTK
    eMetric_TextShouldUseVerticalInsidePadding,           // needed only because of GTK
    eMetric_TextHorizontalInsideMinimumPadding,  
    eMetric_TextShouldUseHorizontalInsideMinimumPadding,  // needed only because of GTK
    eMetric_ButtonHorizontalInsidePaddingNavQuirks,  
    eMetric_ButtonHorizontalInsidePaddingOffsetNavQuirks, 
    eMetric_CheckboxSize,
    eMetric_RadioboxSize,
    
    eMetric_ListShouldUseHorizontalInsideMinimumPadding,  // needed only because of GTK
    eMetric_ListHorizontalInsideMinimumPadding,         

    eMetric_ListShouldUseVerticalInsidePadding,           // needed only because of GTK
    eMetric_ListVerticalInsidePadding,                    // needed only because of GTK

    eMetric_CaretBlinkTime,                               // default, may be overriden by OS
    eMetric_CaretWidth,                                   // pixel width of caret
    eMetric_ShowCaretDuringSelection,                       // show the caret when text is selected?
    eMetric_SelectTextfieldsOnKeyFocus,                   // select textfields when focused via tab/accesskey?
    eMetric_SubmenuDelay,                                 // delay before submenus open
    eMetric_MenusCanOverlapOSBar,                         // can popups overlap menu/task bar?
    eMetric_SkipNavigatingDisabledMenuItem,               // skip navigating to disabled menu item?
    eMetric_DragFullWindow,                               // show window contents while dragging?
    eMetric_DragThresholdX,                               // begin a drag if the mouse is moved further than the threshold while the button is down
    eMetric_DragThresholdY,
    eMetric_UseAccessibilityTheme,                        // Accessibility theme being used?
    eMetric_IsScreenReaderActive,                         // Screen reader being used?

    eMetric_ScrollArrowStyle,                             // position of scroll arrows in a scrollbar
    eMetric_ScrollSliderStyle,                            // is scroll thumb proportional or fixed?

    eMetric_ScrollButtonLeftMouseButtonAction,            // each button can take one of four values:
    eMetric_ScrollButtonMiddleMouseButtonAction,          // 0 - scrolls one  line, 1 - scrolls one page
    eMetric_ScrollButtonRightMouseButtonAction,           // 2 - scrolls to end, 3 - button ignored
 
    eMetric_TreeOpenDelay,                                // delay for opening spring loaded folders
    eMetric_TreeCloseDelay,                               // delay for closing spring loaded folders
    eMetric_TreeLazyScrollDelay,                          // delay for triggering the tree scrolling
    eMetric_TreeScrollDelay,                              // delay for scrolling the tree
    eMetric_TreeScrollLinesMax,                           // the maximum number of lines to be scrolled at ones
    eMetric_TabFocusModel,                                // What type of tab-order to use

    /*
     * eMetric_AlertNotificationOrigin indicates from which corner of the
     * screen alerts slide in, and from which direction (horizontal/vertical).
     * 0, the default, represents bottom right, sliding vertically.
     * Use any bitwise combination of the following constants:
     * NS_ALERT_HORIZONTAL (1), NS_ALERT_LEFT (2), NS_ALERT_TOP (4).
     *
     *       6       4
     *     +-----------+
     *    7|           |5
     *     |           |
     *    3|           |1
     *     +-----------+
     *       2       0
     */
    eMetric_AlertNotificationOrigin,

    /**
     * If true, clicking on a scrollbar (not as in dragging the thumb) defaults
     * to scrolling the view corresponding to the clicked point. Otherwise, we
     * only do so if the scrollbar is clicked using the middle mouse button or
     * if shift is pressed when the scrollbar is clicked.
     */
    eMetric_ScrollToClick
  } nsMetricID;

  enum {
    eMetric_ScrollArrowStartBackward = 0x1000,
    eMetric_ScrollArrowStartForward = 0x0100,
    eMetric_ScrollArrowEndBackward = 0x0010,
    eMetric_ScrollArrowEndForward = 0x0001,
    eMetric_ScrollArrowStyleSingle =                      // single arrow at each end
      eMetric_ScrollArrowStartBackward|eMetric_ScrollArrowEndForward, 
    eMetric_ScrollArrowStyleBothAtBottom =                // both arrows at bottom/right, none at top/left
      eMetric_ScrollArrowEndBackward|eMetric_ScrollArrowEndForward,
    eMetric_ScrollArrowStyleBothAtEachEnd =               // both arrows at both ends
      eMetric_ScrollArrowEndBackward|eMetric_ScrollArrowEndForward|
      eMetric_ScrollArrowStartBackward|eMetric_ScrollArrowStartForward,
    eMetric_ScrollArrowStyleBothAtTop =                   // both arrows at top/left, none at bottom/right
      eMetric_ScrollArrowStartBackward|eMetric_ScrollArrowStartForward
  };
  enum {
    eMetric_ScrollThumbStyleNormal,
    eMetric_ScrollThumbStyleProportional
  };
  
  // When modifying this list, also modify nsXPLookAndFeel::sFloatPrefs
  // in widget/src/xpwidgts/nsXPLookAndFeel.cpp.
  typedef enum {
    eMetricFloat_TextFieldVerticalInsidePadding,
    eMetricFloat_TextFieldHorizontalInsidePadding,
    eMetricFloat_TextAreaVerticalInsidePadding,
    eMetricFloat_TextAreaHorizontalInsidePadding,
    eMetricFloat_ListVerticalInsidePadding,
    eMetricFloat_ListHorizontalInsidePadding,
    eMetricFloat_ButtonVerticalInsidePadding,
    eMetricFloat_ButtonHorizontalInsidePadding,
    eMetricFloat_IMEUnderlineRelativeSize
  } nsMetricFloatID;

  NS_IMETHOD GetColor(const nsColorID aID, nscolor &aColor) = 0;
  NS_IMETHOD GetMetric(const nsMetricID aID, PRInt32 & aMetric) = 0;
  NS_IMETHOD GetMetric(const nsMetricFloatID aID, float & aMetric) = 0;
  virtual PRUnichar GetPasswordCharacter()
  {
    return PRUnichar('*');
  }

  NS_IMETHOD LookAndFeelChanged() = 0;


#ifdef NS_DEBUG
  typedef enum {
    eMetricSize_TextField = 0,
    eMetricSize_TextArea  = 1,
    eMetricSize_ListBox   = 2,
    eMetricSize_ComboBox  = 3,
    eMetricSize_Radio     = 4,
    eMetricSize_CheckBox  = 5,
    eMetricSize_Button    = 6
  } nsMetricNavWidgetID;

  typedef enum {
    eMetricSize_Courier   = 0,
    eMetricSize_SansSerif = 1
  } nsMetricNavFontID;

  // This method returns the actual (or nearest estimate) 
  // of the Navigator size for a given form control for a given font
  // and font size. This is used in NavQuirks mode to see how closely
  // we match its size
  NS_IMETHOD GetNavSize(const nsMetricNavWidgetID aWidgetID,
                        const nsMetricNavFontID   aFontID, 
                        const PRInt32             aFontSize, 
                        nsSize &aSize) = 0;
#endif
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsILookAndFeel, NS_ILOOKANDFEEL_IID)


	// On the Mac, GetColor(eColor_TextSelectForeground, color) returns this
	// constant to specify that the foreground color should not be changed
	// (ie. a colored text keeps its colors  when selected).
	// Of course if other plaforms work like the Mac, they can use it too.
#define NS_DONT_CHANGE_COLOR 	NS_RGB(0x01, 0x01, 0x01)

// --------------------------------
//  Special colors for eColor_IME*
// --------------------------------

// For background color only.
#define NS_TRANSPARENT                NS_RGBA(0x01, 0x00, 0x00, 0x00)
// For foreground color only.
#define NS_SAME_AS_FOREGROUND_COLOR   NS_RGBA(0x02, 0x00, 0x00, 0x00)
#define NS_40PERCENT_FOREGROUND_COLOR NS_RGBA(0x03, 0x00, 0x00, 0x00)

#define NS_IS_IME_SPECIAL_COLOR(c) ((c) == NS_TRANSPARENT || \
                                    (c) == NS_SAME_AS_FOREGROUND_COLOR || \
                                    (c) == NS_40PERCENT_FOREGROUND_COLOR)

// ------------------------------------------
//  Bits for eMetric_AlertNotificationOrigin
// ------------------------------------------

#define NS_ALERT_HORIZONTAL 1
#define NS_ALERT_LEFT       2
#define NS_ALERT_TOP        4

#endif /* __nsILookAndFeel */
