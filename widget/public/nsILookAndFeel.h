/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef MOZILLA_INTERNAL_API
#error "This header is only usable from within libxul (MOZILLA_INTERNAL_API)."
#endif

#include "nsISupports.h"
#include "nsColor.h"

  // for |#ifdef NS_DEBUG|
struct nsSize;

// 89401022-94b3-413e-a6b8-2203dab824f3
#define NS_ILOOKANDFEEL_IID \
{ 0x89401022, 0x94b3, 0x413e, \
  { 0xa6, 0xb8, 0x22, 0x03, 0xda, 0xb8, 0x24, 0xf3 } }

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
    eColor_TextHighlightBackground,
    eColor_TextHighlightForeground,

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

    eColor_SpellCheckerUnderline,

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
    eColor__moz_html_cellhighlight,                          //used to html select cell text background, selected but not focus
    eColor__moz_html_cellhighlighttext,                      //used to html select cell text, selected but not focus
    eColor__moz_buttonhoverface,                             //used to button text background, when mouse is over
    eColor__moz_buttonhovertext,                             //used to button text, when mouse is over
    eColor__moz_menuhover,                                   //used to menu item background, when mouse is over
    eColor__moz_menuhovertext,                               //used to menu item text, when mouse is over
    eColor__moz_menubartext,                                 //used to menu bar item text
    eColor__moz_menubarhovertext,                            //used to menu bar item text, when mouse is over
    // On platforms where these colors are the same as
    // -moz-field, use -moz-fieldtext as foreground color
    eColor__moz_eventreerow,
    eColor__moz_oddtreerow,

    // colors needed by the Mac OS X theme
    eColor__moz_mac_chrome_active,                          // background color of chrome toolbars in active windows
    eColor__moz_mac_chrome_inactive,                        // background color of chrome toolbars in inactive windows
    eColor__moz_mac_focusring,				//ring around text fields and lists
    eColor__moz_mac_menuselect,				//colour used when mouse is over a menu item
    eColor__moz_mac_menushadow,				//colour used to do shadows on menu items
    eColor__moz_mac_menutextdisable,                    // color used to display text for disabled menu items
    eColor__moz_mac_menutextselect,			//colour used to display text while mouse is over a menu item
    eColor__moz_mac_disabledtoolbartext,                    // text color of disabled text on toolbars

    //new in 10.2
    eColor__moz_mac_alternateprimaryhighlight, //active list highlight
    eColor__moz_mac_secondaryhighlight,        //inactive light hightlight

    // vista rebars
    eColor__moz_win_mediatext,                     // media rebar text
    eColor__moz_win_communicationstext,            // communications rebar text

    // Hyperlink color extracted from the system, not affected by the browser.anchor_color user pref.
    // There is no OS-specified safe background color for this text, 
    // but it is used regularly within Windows and the Gnome DE on Dialog and Window colors.
    eColor__moz_nativehyperlinktext,

    // Combo box widgets
    eColor__moz_comboboxtext,
    eColor__moz_combobox,

    // keep this one last, please
    eColor_LAST_COLOR
  } nsColorID;

  // When modifying this list, also modify nsXPLookAndFeel::sIntPrefs
  // in widget/src/xpwidgts/nsXPLookAndFeel.cpp.
  typedef enum {
    eMetric_CaretBlinkTime,                               // default, may be overriden by OS
    eMetric_CaretWidth,                                   // pixel width of caret
    eMetric_ShowCaretDuringSelection,                       // show the caret when text is selected?
    eMetric_SelectTextfieldsOnKeyFocus,                   // select textfields when focused via tab/accesskey?
    eMetric_SubmenuDelay,                                 // delay before submenus open
    eMetric_MenusCanOverlapOSBar,                         // can popups overlap menu/task bar?
    eMetric_ScrollbarsCanOverlapContent,                  // can scrollbars float above content?
    eMetric_SkipNavigatingDisabledMenuItem,               // skip navigating to disabled menu item?
    eMetric_DragThresholdX,                               // begin a drag if the mouse is moved further than the threshold while the button is down
    eMetric_DragThresholdY,
    eMetric_UseAccessibilityTheme,                        // Accessibility theme being used?

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
    eMetric_ChosenMenuItemsShouldBlink,                   // Should menu items blink when they're chosen?

    /*
     * A Boolean value to determine whether the Windows default theme is
     * being used.
     *
     * The value of this metric is not used on other platforms. These platforms
     * should return NS_ERROR_NOT_IMPLEMENTED when queried for this metric.
     */
    eMetric_WindowsDefaultTheme,

    /*
     * A Boolean value to determine whether the DWM compositor is being used
     *
     * This metric is not used on non-Windows platforms. These platforms
     * should return NS_ERROR_NOT_IMPLEMENTED when queried for this metric.
     */
    eMetric_DWMCompositor,

    /*
     * A Boolean value to determine whether Windows is themed (Classic vs.
     * uxtheme)
     *
     * This is Windows-specific and is not implemented on other platforms
     * (will return the default of NS_ERROR_FAILURE).
     */
    eMetric_WindowsClassic,

    /*
     * A Boolean value to determine whether the device is a touch enabled
     * device. Currently this is only supported by the Windows 7 Touch API.
     *
     * Platforms that do not support this metric should return
     * NS_ERROR_NOT_IMPLEMENTED when queried for this metric.
     */
    eMetric_TouchEnabled,

    /*
     * A Boolean value to determine whether the Mac graphite theme is
     * being used.
     *
     * The value of this metric is not used on other platforms. These platforms
     * should return NS_ERROR_NOT_IMPLEMENTED when queried for this metric.
     */
    eMetric_MacGraphiteTheme,

    /*
     * A Boolean value to determine whether the Mac OS X Lion-specific theming
     * should be used.
     *
     * The value of this metric is not used on non-Mac platforms. These
     * platforms should return NS_ERROR_NOT_IMPLEMENTED when queried for this
     * metric.
     */
    eMetric_MacLionTheme,

    /*
     * A Boolean value to determine whether Mameo is using the new Fremantle
     * theme.
     *
     * The value of this metric is not used on other platforms. These platforms
     * should return NS_ERROR_NOT_IMPLEMENTED when queried for this metric.
     */
    eMetric_MaemoClassic,

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
    eMetric_ScrollToClick,

    /**
     * IME and spell checker underline styles, the values should be
     * NS_DECORATION_LINE_STYLE_*.  They are defined below.
     */
    eMetric_IMERawInputUnderlineStyle,
    eMetric_IMESelectedRawTextUnderlineStyle,
    eMetric_IMEConvertedTextUnderlineStyle,
    eMetric_IMESelectedConvertedTextUnderline,
    eMetric_SpellCheckerUnderlineStyle,

    /**
     * If this metric != 0, show icons in menus.
     */
    eMetric_ImagesInMenus,
    /**
     * If this metric != 0, show icons in buttons.
     */
    eMetric_ImagesInButtons,
    /**
     * If this metric != 0, support window dragging on the menubar.
     */
    eMetric_MenuBarDrag,
    /**
     * Return the appropriate WindowsThemeIdentifier for the current theme.
     */
    eMetric_WindowsThemeIdentifier
  } nsMetricID;

  /**
   * Windows themes we currently detect.
   */
  enum WindowsThemeIdentifier {
    eWindowsTheme_Generic = 0, // unrecognized theme
    eWindowsTheme_Classic,
    eWindowsTheme_Aero,
    eWindowsTheme_LunaBlue,
    eWindowsTheme_LunaOlive,
    eWindowsTheme_LunaSilver,
    eWindowsTheme_Royale,
    eWindowsTheme_Zune
  };

  enum {
    eMetric_ScrollArrowNone = 0,
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
    eMetricFloat_IMEUnderlineRelativeSize,
    eMetricFloat_SpellCheckerUnderlineRelativeSize,

    // The width/height ratio of the cursor. If used, the CaretWidth int metric
    // should be added to the calculated caret width.
    eMetricFloat_CaretAspectRatio
  } nsMetricFloatID;

  NS_IMETHOD GetColor(const nsColorID aID, nscolor &aColor) = 0;
  NS_IMETHOD GetMetric(const nsMetricID aID, PRInt32 & aMetric) = 0;
  NS_IMETHOD GetMetric(const nsMetricFloatID aID, float & aMetric) = 0;
  virtual PRUnichar GetPasswordCharacter()
  {
    return PRUnichar('*');
  }

  virtual PRBool GetEchoPassword()
  {
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    return PR_TRUE;
#else
    return PR_FALSE;
#endif
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

namespace mozilla {

class LookAndFeel
{
public:
  // When modifying this list, also modify nsXPLookAndFeel::sColorPrefs
  // in widget/src/xpwidgts/nsXPLookAndFeel.cpp.
  enum ColorID {

    // WARNING : NO NEGATIVE VALUE IN THIS ENUMERATION
    // see patch in bug 57757 for more information

    eColorID_WindowBackground,
    eColorID_WindowForeground,
    eColorID_WidgetBackground,
    eColorID_WidgetForeground,
    eColorID_WidgetSelectBackground,
    eColorID_WidgetSelectForeground,
    eColorID_Widget3DHighlight,
    eColorID_Widget3DShadow,
    eColorID_TextBackground,
    eColorID_TextForeground,
    eColorID_TextSelectBackground,
    eColorID_TextSelectForeground,
    eColorID_TextSelectBackgroundDisabled,
    eColorID_TextSelectBackgroundAttention,
    eColorID_TextHighlightBackground,
    eColorID_TextHighlightForeground,

    eColorID_IMERawInputBackground,
    eColorID_IMERawInputForeground,
    eColorID_IMERawInputUnderline,
    eColorID_IMESelectedRawTextBackground,
    eColorID_IMESelectedRawTextForeground,
    eColorID_IMESelectedRawTextUnderline,
    eColorID_IMEConvertedTextBackground,
    eColorID_IMEConvertedTextForeground,
    eColorID_IMEConvertedTextUnderline,
    eColorID_IMESelectedConvertedTextBackground,
    eColorID_IMESelectedConvertedTextForeground,
    eColorID_IMESelectedConvertedTextUnderline,

    eColorID_SpellCheckerUnderline,

    // New CSS 2 color definitions
    eColorID_activeborder,
    eColorID_activecaption,
    eColorID_appworkspace,
    eColorID_background,
    eColorID_buttonface,
    eColorID_buttonhighlight,
    eColorID_buttonshadow,
    eColorID_buttontext,
    eColorID_captiontext,
    eColorID_graytext,
    eColorID_highlight,
    eColorID_highlighttext,
    eColorID_inactiveborder,
    eColorID_inactivecaption,
    eColorID_inactivecaptiontext,
    eColorID_infobackground,
    eColorID_infotext,
    eColorID_menu,
    eColorID_menutext,
    eColorID_scrollbar,
    eColorID_threeddarkshadow,
    eColorID_threedface,
    eColorID_threedhighlight,
    eColorID_threedlightshadow,
    eColorID_threedshadow,
    eColorID_window,
    eColorID_windowframe,
    eColorID_windowtext,

    eColorID__moz_buttondefault,
    // Colors which will hopefully become CSS3
    eColorID__moz_field,
    eColorID__moz_fieldtext,
    eColorID__moz_dialog,
    eColorID__moz_dialogtext,
    // used to highlight valid regions to drop something onto
    eColorID__moz_dragtargetzone,

    // used to cell text background, selected but not focus
    eColorID__moz_cellhighlight,
    // used to cell text, selected but not focus
    eColorID__moz_cellhighlighttext,
    // used to html select cell text background, selected but not focus
    eColorID__moz_html_cellhighlight,
    // used to html select cell text, selected but not focus
    eColorID__moz_html_cellhighlighttext,
    // used to button text background, when mouse is over
    eColorID__moz_buttonhoverface,
    // used to button text, when mouse is over
    eColorID__moz_buttonhovertext,
    // used to menu item background, when mouse is over
    eColorID__moz_menuhover,
    // used to menu item text, when mouse is over
    eColorID__moz_menuhovertext,
    // used to menu bar item text
    eColorID__moz_menubartext,
    // used to menu bar item text, when mouse is over
    eColorID__moz_menubarhovertext,
    // On platforms where these colors are the same as
    // -moz-field, use -moz-fieldtext as foreground color
    eColorID__moz_eventreerow,
    eColorID__moz_oddtreerow,

    // colors needed by the Mac OS X theme

    // background color of chrome toolbars in active windows
    eColorID__moz_mac_chrome_active,
    // background color of chrome toolbars in inactive windows
    eColorID__moz_mac_chrome_inactive,
    //ring around text fields and lists
    eColorID__moz_mac_focusring,
    //colour used when mouse is over a menu item
    eColorID__moz_mac_menuselect,
    //colour used to do shadows on menu items
    eColorID__moz_mac_menushadow,
    // color used to display text for disabled menu items
    eColorID__moz_mac_menutextdisable,
    //colour used to display text while mouse is over a menu item
    eColorID__moz_mac_menutextselect,
    // text color of disabled text on toolbars
    eColorID__moz_mac_disabledtoolbartext,

    //new in 10.2

    //active list highlight
    eColorID__moz_mac_alternateprimaryhighlight,
    //inactive light hightlight
    eColorID__moz_mac_secondaryhighlight,

    // vista rebars

    // media rebar text
    eColorID__moz_win_mediatext,
    // communications rebar text
    eColorID__moz_win_communicationstext,

    // Hyperlink color extracted from the system, not affected by the
    // browser.anchor_color user pref.
    // There is no OS-specified safe background color for this text, 
    // but it is used regularly within Windows and the Gnome DE on Dialog and
    // Window colors.
    eColorID__moz_nativehyperlinktext,

    // Combo box widgets
    eColorID__moz_comboboxtext,
    eColorID__moz_combobox,

    // keep this one last, please
    eColorID_LAST_COLOR
  };

  // When modifying this list, also modify nsXPLookAndFeel::sIntPrefs
  // in widget/src/xpwidgts/nsXPLookAndFeel.cpp.
  enum IntID {
    // default, may be overriden by OS
    eIntID_CaretBlinkTime,
    // pixel width of caret
    eIntID_CaretWidth,
    // show the caret when text is selected?
    eIntID_ShowCaretDuringSelection,
    // select textfields when focused via tab/accesskey?
    eIntID_SelectTextfieldsOnKeyFocus,
    // delay before submenus open
    eIntID_SubmenuDelay,
    // can popups overlap menu/task bar?
    eIntID_MenusCanOverlapOSBar,
    // can scrollbars float above content?
    eIntID_ScrollbarsCanOverlapContent,
    // skip navigating to disabled menu item?
    eIntID_SkipNavigatingDisabledMenuItem,
    // begin a drag if the mouse is moved further than the threshold while the
    // button is down
    eIntID_DragThresholdX,
    eIntID_DragThresholdY,
    // Accessibility theme being used?
    eIntID_UseAccessibilityTheme,

    // position of scroll arrows in a scrollbar
    eIntID_ScrollArrowStyle,
    // is scroll thumb proportional or fixed?
    eIntID_ScrollSliderStyle,

    // each button can take one of four values:
    eIntID_ScrollButtonLeftMouseButtonAction,
    // 0 - scrolls one  line, 1 - scrolls one page
    eIntID_ScrollButtonMiddleMouseButtonAction,
    // 2 - scrolls to end, 3 - button ignored
    eIntID_ScrollButtonRightMouseButtonAction,

    // delay for opening spring loaded folders
    eIntID_TreeOpenDelay,
    // delay for closing spring loaded folders
    eIntID_TreeCloseDelay,
    // delay for triggering the tree scrolling
    eIntID_TreeLazyScrollDelay,
    // delay for scrolling the tree
    eIntID_TreeScrollDelay,
    // the maximum number of lines to be scrolled at ones
    eIntID_TreeScrollLinesMax,
    // What type of tab-order to use
    eIntID_TabFocusModel,
    // Should menu items blink when they're chosen?
    eIntID_ChosenMenuItemsShouldBlink,

    /*
     * A Boolean value to determine whether the Windows default theme is
     * being used.
     *
     * The value of this metric is not used on other platforms. These platforms
     * should return NS_ERROR_NOT_IMPLEMENTED when queried for this metric.
     */
    eIntID_WindowsDefaultTheme,

    /*
     * A Boolean value to determine whether the DWM compositor is being used
     *
     * This metric is not used on non-Windows platforms. These platforms
     * should return NS_ERROR_NOT_IMPLEMENTED when queried for this metric.
     */
    eIntID_DWMCompositor,

    /*
     * A Boolean value to determine whether Windows is themed (Classic vs.
     * uxtheme)
     *
     * This is Windows-specific and is not implemented on other platforms
     * (will return the default of NS_ERROR_FAILURE).
     */
    eIntID_WindowsClassic,

    /*
     * A Boolean value to determine whether the device is a touch enabled
     * device. Currently this is only supported by the Windows 7 Touch API.
     *
     * Platforms that do not support this metric should return
     * NS_ERROR_NOT_IMPLEMENTED when queried for this metric.
     */
    eIntID_TouchEnabled,

    /*
     * A Boolean value to determine whether the Mac graphite theme is
     * being used.
     *
     * The value of this metric is not used on other platforms. These platforms
     * should return NS_ERROR_NOT_IMPLEMENTED when queried for this metric.
     */
    eIntID_MacGraphiteTheme,

    /*
     * A Boolean value to determine whether the Mac OS X Lion-specific theming
     * should be used.
     *
     * The value of this metric is not used on non-Mac platforms. These
     * platforms should return NS_ERROR_NOT_IMPLEMENTED when queried for this
     * metric.
     */
    eIntID_MacLionTheme,

    /*
     * A Boolean value to determine whether Mameo is using the new Fremantle
     * theme.
     *
     * The value of this metric is not used on other platforms. These platforms
     * should return NS_ERROR_NOT_IMPLEMENTED when queried for this metric.
     */
    eIntID_MaemoClassic,

    /*
     * eIntID_AlertNotificationOrigin indicates from which corner of the
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
    eIntID_AlertNotificationOrigin,

    /**
     * If true, clicking on a scrollbar (not as in dragging the thumb) defaults
     * to scrolling the view corresponding to the clicked point. Otherwise, we
     * only do so if the scrollbar is clicked using the middle mouse button or
     * if shift is pressed when the scrollbar is clicked.
     */
    eIntID_ScrollToClick,

    /**
     * IME and spell checker underline styles, the values should be
     * NS_DECORATION_LINE_STYLE_*.  They are defined below.
     */
    eIntID_IMERawInputUnderlineStyle,
    eIntID_IMESelectedRawTextUnderlineStyle,
    eIntID_IMEConvertedTextUnderlineStyle,
    eIntID_IMESelectedConvertedTextUnderline,
    eIntID_SpellCheckerUnderlineStyle,

    /**
     * If this metric != 0, show icons in menus.
     */
    eIntID_ImagesInMenus,
    /**
     * If this metric != 0, show icons in buttons.
     */
    eIntID_ImagesInButtons,
    /**
     * If this metric != 0, support window dragging on the menubar.
     */
    eIntID_MenuBarDrag,
    /**
     * Return the appropriate WindowsThemeIdentifier for the current theme.
     */
    eIntID_WindowsThemeIdentifier
  };

  /**
   * Windows themes we currently detect.
   */
  enum WindowsTheme {
    eWindowsTheme_Generic = 0, // unrecognized theme
    eWindowsTheme_Classic,
    eWindowsTheme_Aero,
    eWindowsTheme_LunaBlue,
    eWindowsTheme_LunaOlive,
    eWindowsTheme_LunaSilver,
    eWindowsTheme_Royale,
    eWindowsTheme_Zune
  };

  enum {
    eScrollArrow_None = 0,
    eScrollArrow_StartBackward = 0x1000,
    eScrollArrow_StartForward = 0x0100,
    eScrollArrow_EndBackward = 0x0010,
    eScrollArrow_EndForward = 0x0001
  };

  enum {
    // single arrow at each end
    eScrollArrowStyle_Single =
      eScrollArrow_StartBackward | eScrollArrow_EndForward, 
    // both arrows at bottom/right, none at top/left
    eScrollArrowStyle_BothAtBottom =
      eScrollArrow_EndBackward | eScrollArrow_EndForward,
    // both arrows at both ends
    eScrollArrowStyle_BothAtEachEnd =
      eScrollArrow_EndBackward | eScrollArrow_EndForward |
      eScrollArrow_StartBackward | eScrollArrow_StartForward,
    // both arrows at top/left, none at bottom/right
    eScrollArrowStyle_BothAtTop =
      eScrollArrow_StartBackward | eScrollArrow_StartForward
  };

  enum {
    eScrollThumbStyle_Normal,
    eScrollThumbStyle_Proportional
  };

  // When modifying this list, also modify nsXPLookAndFeel::sFloatPrefs
  // in widget/src/xpwidgts/nsXPLookAndFeel.cpp.
  enum FloatID {
    eFloatID_IMEUnderlineRelativeSize,
    eFloatID_SpellCheckerUnderlineRelativeSize,

    // The width/height ratio of the cursor. If used, the CaretWidth int metric
    // should be added to the calculated caret width.
    eFloatID_CaretAspectRatio
  };

  /**
   * GetColor() return a native color value (might be overwritten by prefs) for
   * aID.  Some platforms don't return an error even if the index doesn't
   * match any system colors.  And also some platforms may initialize the
   * return value even when it returns an error.  Therefore, if you want to
   * use a color for the default value, you should use the other GetColor()
   * which returns nscolor directly.
   *
   * NOTE:
   *   eColorID_TextSelectForeground might return NS_DONT_CHANGE_COLOR.
   *   eColorID_IME* might return NS_TRANSPARENT, NS_SAME_AS_FOREGROUND_COLOR or
   *   NS_40PERCENT_FOREGROUND_COLOR.
   *   These values have particular meaning.  Then, they are not an actual
   *   color value.
   */
  static nsresult GetColor(ColorID aID, nscolor* aResult);

  /**
   * GetInt() and GetFloat() return a int or float value for aID.  The result
   * might be distance, time, some flags or a int value which has particular
   * meaning.  See each document at definition of each ID for the detail.
   * The result is always 0 when they return error.  Therefore, if you want to
   * use a value for the default value, you should use the other method which
   * returns int or float directly.
   */
  static nsresult GetInt(IntID aID, PRInt32* aResult);
  static nsresult GetFloat(FloatID aID, float* aResult);

  static nscolor GetColor(ColorID aID, nscolor aDefault = NS_RGB(0, 0, 0))
  {
    nscolor result;
    if (NS_FAILED(GetColor(aID, &result))) {
      return aDefault;
    }
    return result;
  }

  static PRInt32 GetInt(IntID aID, PRInt32 aDefault = 0)
  {
    PRInt32 result;
    if (NS_FAILED(GetInt(aID, &result))) {
      return aDefault;
    }
    return result;
  }

  static float GetFloat(FloatID aID, float aDefault = 0.0f)
  {
    float result;
    if (NS_FAILED(GetFloat(aID, &result))) {
      return aDefault;
    }
    return result;
  }

  /**
   * GetPasswordCharacter() returns a unicode character which should be used
   * for a masked character in password editor.  E.g., '*'.
   */
  static PRUnichar GetPasswordCharacter();

  /**
   * If the latest character in password field shouldn't be hidden by the
   * result of GetPasswordCharacter(), GetEchoPassword() returns TRUE.
   * Otherwise, FALSE.
   */
  static PRBool GetEchoPassword();

  /**
   * When system look and feel is changed, Refresh() must be called.  Then,
   * cached data would be released.
   */
  static void Refresh();
};

} // namespace mozilla

// On the Mac, GetColor(eColorID_TextSelectForeground, color) returns this
// constant to specify that the foreground color should not be changed
// (ie. a colored text keeps its colors  when selected).
// Of course if other plaforms work like the Mac, they can use it too.
#define NS_DONT_CHANGE_COLOR 	NS_RGB(0x01, 0x01, 0x01)

// ---------------------------------------------------------------------
//  Special colors for eColorID_IME* and eColorID_SpellCheckerUnderline
// ---------------------------------------------------------------------

// For background color only.
#define NS_TRANSPARENT                NS_RGBA(0x01, 0x00, 0x00, 0x00)
// For foreground color only.
#define NS_SAME_AS_FOREGROUND_COLOR   NS_RGBA(0x02, 0x00, 0x00, 0x00)
#define NS_40PERCENT_FOREGROUND_COLOR NS_RGBA(0x03, 0x00, 0x00, 0x00)

#define NS_IS_SELECTION_SPECIAL_COLOR(c) ((c) == NS_TRANSPARENT || \
                                          (c) == NS_SAME_AS_FOREGROUND_COLOR || \
                                          (c) == NS_40PERCENT_FOREGROUND_COLOR)

// ------------------------------------------
//  Bits for eIntID_AlertNotificationOrigin
// ------------------------------------------

#define NS_ALERT_HORIZONTAL 1
#define NS_ALERT_LEFT       2
#define NS_ALERT_TOP        4

#endif /* __nsILookAndFeel */
