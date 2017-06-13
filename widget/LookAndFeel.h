/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __LookAndFeel
#define __LookAndFeel

#ifndef MOZILLA_INTERNAL_API
#error "This header is only usable from within libxul (MOZILLA_INTERNAL_API)."
#endif

#include "nsDebug.h"
#include "nsColor.h"
#include "nsTArray.h"

struct gfxFontStyle;

struct LookAndFeelInt
{
  int32_t id;
  int32_t value;
};

namespace mozilla {

class LookAndFeel
{
public:
  // When modifying this list, also modify nsXPLookAndFeel::sColorPrefs
  // in widget/xpwidgts/nsXPLookAndFeel.cpp.
  enum ColorID : uint8_t {

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
    eColorID_TextSelectForegroundCustom,
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

    // foreground color of :hover:active buttons
    eColorID__moz_mac_buttonactivetext,
    // background color of chrome toolbars in active windows
    eColorID__moz_mac_chrome_active,
    // background color of chrome toolbars in inactive windows
    eColorID__moz_mac_chrome_inactive,
    // foreground color of default buttons
    eColorID__moz_mac_defaultbuttontext,
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

    // GtkInfoBar
    eColorID__moz_gtk_info_bar_text,

    // keep this one last, please
    eColorID_LAST_COLOR
  };

  // When modifying this list, also modify nsXPLookAndFeel::sIntPrefs
  // in widget/xpwidgts/nsXPLookAndFeel.cpp.
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
    // should overlay scrollbars be used?
    eIntID_UseOverlayScrollbars,
    // allow H and V overlay scrollbars to overlap?
    eIntID_AllowOverlayScrollbarsOverlap,
    // show/hide scrollbars based on activity
    eIntID_ShowHideScrollbars,
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
     * A Boolean value to determine whether the current Windows desktop theme
     * supports Aero Glass.
     *
     * This is Windows-specific and is not implemented on other platforms
     * (will return the default of NS_ERROR_FAILURE).
     */
    eIntID_WindowsGlass,

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
    * A Boolean value to determine whether the Mac OS X Yosemite-specific theming
    * should be used.
    *
    * The value of this metric is not used on non-Mac platforms. These
    * platforms should return NS_ERROR_NOT_IMPLEMENTED when queried for this
    * metric.
    */
   eIntID_MacYosemiteTheme,

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
     * If this metric != 0, support window dragging on the menubar.
     */
    eIntID_MenuBarDrag,
    /**
     * Return the appropriate WindowsThemeIdentifier for the current theme.
     */
    eIntID_WindowsThemeIdentifier,
    /**
     * Return an appropriate os version identifier.
     */
    eIntID_OperatingSystemVersionIdentifier,
    /**
     * 0: scrollbar button repeats to scroll only when cursor is on the button.
     * 1: scrollbar button repeats to scroll even if cursor is outside of it.
     */
    eIntID_ScrollbarButtonAutoRepeatBehavior,
    /**
     * Delay before showing a tooltip.
     */
    eIntID_TooltipDelay,
    /*
     * A Boolean value to determine whether Mac OS X Lion style swipe animations
     * should be used.
     */
    eIntID_SwipeAnimationEnabled,

    /*
     * A Boolean value to determine whether we have a color picker available
     * for <input type="color"> to hook into.
     *
     * This lets us selectively enable the style for <input type="color">
     * based on whether it's functional or not.
     */
    eIntID_ColorPickerAvailable,

    /*
     * A boolean value indicating whether or not the device has a hardware
     * home button. Used on gaia to determine whether a home button
     * is shown.
     */
     eIntID_PhysicalHomeButton,

     /*
      * Controls whether overlay scrollbars display when the user moves
      * the mouse in a scrollable frame.
      */
     eIntID_ScrollbarDisplayOnMouseMove,

     /*
      * Overlay scrollbar animation constants.
      */
     eIntID_ScrollbarFadeBeginDelay,
     eIntID_ScrollbarFadeDuration,
      
     /**
      * Distance in pixels to offset the context menu from the cursor
      * on open.
      */
     eIntID_ContextMenuOffsetVertical,
     eIntID_ContextMenuOffsetHorizontal
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
    eWindowsTheme_Zune,
    eWindowsTheme_AeroLite
  };

  /**
   * Operating system versions.
   */
  enum OperatingSystemVersion {
    eOperatingSystemVersion_Windows7 = 2,
    eOperatingSystemVersion_Windows8,
    eOperatingSystemVersion_Windows10,
    eOperatingSystemVersion_Unknown
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
  // in widget/xpwidgts/nsXPLookAndFeel.cpp.
  enum FloatID {
    eFloatID_IMEUnderlineRelativeSize,
    eFloatID_SpellCheckerUnderlineRelativeSize,

    // The width/height ratio of the cursor. If used, the CaretWidth int metric
    // should be added to the calculated caret width.
    eFloatID_CaretAspectRatio
  };

  // These constants must be kept in 1:1 correspondence with the
  // NS_STYLE_FONT_* system font constants.
  enum FontID {
    eFont_Caption = 1,     // css2
    FontID_MINIMUM = eFont_Caption,
    eFont_Icon,
    eFont_Menu,
    eFont_MessageBox,
    eFont_SmallCaption,
    eFont_StatusBar,

    eFont_Window,          // css3
    eFont_Document,
    eFont_Workspace,
    eFont_Desktop,
    eFont_Info,
    eFont_Dialog,
    eFont_Button,
    eFont_PullDownMenu,
    eFont_List,
    eFont_Field,

    eFont_Tooltips,        // moz
    eFont_Widget,
    FontID_MAXIMUM = eFont_Widget
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
   * This variant of GetColor() takes an extra Boolean parameter that allows
   * the caller to ask that hard-coded color values be substituted for
   * native colors (used when it is desireable to hide system colors to
   * avoid system fingerprinting).
   */
  static nsresult GetColor(ColorID aID, bool aUseStandinsForNativeColors,
                           nscolor* aResult);

  /**
   * GetInt() and GetFloat() return a int or float value for aID.  The result
   * might be distance, time, some flags or a int value which has particular
   * meaning.  See each document at definition of each ID for the detail.
   * The result is always 0 when they return error.  Therefore, if you want to
   * use a value for the default value, you should use the other method which
   * returns int or float directly.
   */
  static nsresult GetInt(IntID aID, int32_t* aResult);
  static nsresult GetFloat(FloatID aID, float* aResult);

  static nscolor GetColor(ColorID aID, nscolor aDefault = NS_RGB(0, 0, 0))
  {
    nscolor result = NS_RGB(0, 0, 0);
    if (NS_FAILED(GetColor(aID, &result))) {
      return aDefault;
    }
    return result;
  }

  static nscolor GetColorUsingStandins(ColorID aID,
                                       nscolor aDefault = NS_RGB(0, 0, 0))
  {
    nscolor result = NS_RGB(0, 0, 0);
    if (NS_FAILED(GetColor(aID,
                           true, // aUseStandinsForNativeColors
                           &result))) {
      return aDefault;
    }
    return result;
  }

  static int32_t GetInt(IntID aID, int32_t aDefault = 0)
  {
    int32_t result;
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
   * Retrieve the name and style of a system-theme font.  Returns true
   * if the system theme specifies this font, false if a default should
   * be used.  In the latter case neither aName nor aStyle is modified.
   *
   * @param aID    Which system-theme font is wanted.
   * @param aName  The name of the font to use.
   * @param aStyle Styling to apply to the font.
   * @param aDevPixPerCSSPixel  Ratio of device pixels to CSS pixels
   */
  static bool GetFont(FontID aID, nsString& aName, gfxFontStyle& aStyle,
                      float aDevPixPerCSSPixel);

  /**
   * GetPasswordCharacter() returns a unicode character which should be used
   * for a masked character in password editor.  E.g., '*'.
   */
  static char16_t GetPasswordCharacter();

  /**
   * If the latest character in password field shouldn't be hidden by the
   * result of GetPasswordCharacter(), GetEchoPassword() returns TRUE.
   * Otherwise, FALSE.
   */
  static bool GetEchoPassword();

  /**
   * The millisecond to mask password value.
   * This value is only valid when GetEchoPassword() returns true.
   */
  static uint32_t GetPasswordMaskDelay();

  /**
   * When system look and feel is changed, Refresh() must be called.  Then,
   * cached data would be released.
   */
  static void Refresh();

  /**
   * If the implementation is caching values, these accessors allow the
   * cache to be exported and imported.
   */
  static nsTArray<LookAndFeelInt> GetIntCache();
  static void SetIntCache(const nsTArray<LookAndFeelInt>& aLookAndFeelIntCache);
};

} // namespace mozilla

// On the Mac, GetColor(eColorID_TextSelectForeground, color) returns this
// constant to specify that the foreground color should not be changed
// (ie. a colored text keeps its colors  when selected).
// Of course if other plaforms work like the Mac, they can use it too.
#define NS_DONT_CHANGE_COLOR 	NS_RGB(0x01, 0x01, 0x01)

// Similar with NS_DONT_CHANGE_COLOR, except NS_DONT_CHANGE_COLOR would returns
// complementary color if fg color is same as bg color.
// NS_CHANGE_COLOR_IF_SAME_AS_BG would returns eColorID_TextSelectForegroundCustom if
// fg and bg color are the same.
#define NS_CHANGE_COLOR_IF_SAME_AS_BG NS_RGB(0x02, 0x02, 0x02)

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

#endif /* __LookAndFeel */
