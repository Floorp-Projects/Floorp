/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __LookAndFeel
#define __LookAndFeel

#ifndef MOZILLA_INTERNAL_API
#  error "This header is only usable from within libxul (MOZILLA_INTERNAL_API)."
#endif

#include "nsDebug.h"
#include "nsColor.h"
#include "nsTArray.h"

struct gfxFontStyle;

struct LookAndFeelInt {
  int32_t id;
  union {
    int32_t value;
    nscolor colorValue;
  };
};

namespace mozilla {

enum class StyleSystemColor : uint8_t;

class LookAndFeel {
 public:
  using ColorID = StyleSystemColor;

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
     * A Boolean value to determine whether the Windows accent color
     * should be applied to the title bar.
     *
     * The value of this metric is not used on other platforms. These platforms
     * should return NS_ERROR_NOT_IMPLEMENTED when queried for this metric.
     */
    eIntID_WindowsAccentColorInTitlebar,

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
     * A Boolean value to determine whether the Mac OS X Yosemite-specific
     * theming should be used.
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
    eIntID_ContextMenuOffsetHorizontal,

    /*
     * A boolean value indicating whether client-side decorations are
     * supported by the user's GTK version.
     */
    eIntID_GTKCSDAvailable,

    /*
     * A boolean value indicating whether GTK+ system titlebar should be
     * disabled by default.
     */
    eIntID_GTKCSDHideTitlebarByDefault,

    /*
     * A boolean value indicating whether client-side decorations should
     * have transparent background.
     */
    eIntID_GTKCSDTransparentBackground,

    /*
     * A boolean value indicating whether client-side decorations should
     * contain a minimize button.
     */
    eIntID_GTKCSDMinimizeButton,

    /*
     * A boolean value indicating whether client-side decorations should
     * contain a maximize button.
     */
    eIntID_GTKCSDMaximizeButton,

    /*
     * A boolean value indicating whether client-side decorations should
     * contain a close button.
     */
    eIntID_GTKCSDCloseButton,

    /*
     * A boolean value indicating whether titlebar buttons are located
     * in left titlebar corner.
     */
    eIntID_GTKCSDReversedPlacement,

    /*
     * A boolean value indicating whether or not the OS is using a dark theme,
     * which we may want to switch to as well if not overridden by the user.
     */
    eIntID_SystemUsesDarkTheme,

    /**
     * Corresponding to prefers-reduced-motion.
     * https://drafts.csswg.org/mediaqueries-5/#prefers-reduced-motion
     * 0: no-preference
     * 1: reduce
     */

    eIntID_PrefersReducedMotion,
    /**
     * Corresponding to PointerCapabilities in ServoTypes.h
     * 0: None
     * 1: Coarse
     * 2: Fine
     * 4: Hover
     */
    eIntID_PrimaryPointerCapabilities,
    /**
     * Corresponding to union of PointerCapabilities values in ServoTypes.h
     * E.g. if there is a mouse and a digitizer, the value will be
     * 'Coarse | Fine | Hover'.
     */
    eIntID_AllPointerCapabilities,
  };

  /**
   * Windows themes we currently detect.
   */
  enum WindowsTheme {
    eWindowsTheme_Generic = 0,  // unrecognized theme
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

  enum { eScrollThumbStyle_Normal, eScrollThumbStyle_Proportional };

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
    eFont_Caption = 1,  // css2
    FontID_MINIMUM = eFont_Caption,
    eFont_Icon,
    eFont_Menu,
    eFont_MessageBox,
    eFont_SmallCaption,
    eFont_StatusBar,

    eFont_Window,  // css3
    eFont_Document,
    eFont_Workspace,
    eFont_Desktop,
    eFont_Info,
    eFont_Dialog,
    eFont_Button,
    eFont_PullDownMenu,
    eFont_List,
    eFont_Field,

    eFont_Tooltips,  // moz
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
   *   ColorID::TextSelectForeground might return NS_DONT_CHANGE_COLOR.
   *   ColorID::IME* might return NS_TRANSPARENT, NS_SAME_AS_FOREGROUND_COLOR or
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

  static nscolor GetColor(ColorID aID, nscolor aDefault = NS_RGB(0, 0, 0)) {
    nscolor result = NS_RGB(0, 0, 0);
    if (NS_FAILED(GetColor(aID, &result))) {
      return aDefault;
    }
    return result;
  }

  static nscolor GetColorUsingStandins(ColorID aID,
                                       nscolor aDefault = NS_RGB(0, 0, 0)) {
    nscolor result = NS_RGB(0, 0, 0);
    if (NS_FAILED(GetColor(aID,
                           true,  // aUseStandinsForNativeColors
                           &result))) {
      return aDefault;
    }
    return result;
  }

  static int32_t GetInt(IntID aID, int32_t aDefault = 0) {
    int32_t result;
    if (NS_FAILED(GetInt(aID, &result))) {
      return aDefault;
    }
    return result;
  }

  static float GetFloat(FloatID aID, float aDefault = 0.0f) {
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
   * Size of the font should be in CSS pixels, not device pixels.
   *
   * @param aID    Which system-theme font is wanted.
   * @param aName  The name of the font to use.
   * @param aStyle Styling to apply to the font.
   */
  static bool GetFont(FontID aID, nsString& aName, gfxFontStyle& aStyle);

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
   * GTK's initialization code can't be run off main thread, call this
   * if you plan on using LookAndFeel off main thread later.
   *
   * This initialized state may get reset due to theme changes, so it
   * must be called prior to each potential off-main-thread LookAndFeel
   * call, not just once.
   */
  static void NativeInit();

  /**
   * If the implementation is caching values, these accessors allow the
   * cache to be exported and imported.
   */
  static nsTArray<LookAndFeelInt> GetIntCache();
  static void SetIntCache(const nsTArray<LookAndFeelInt>& aLookAndFeelIntCache);
  /**
   * Set a flag indicating whether the cache should be cleared in RefreshImpl()
   * or not.
   */
  static void SetShouldRetainCacheForTest(bool aValue);
};

}  // namespace mozilla

// On the Mac, GetColor(ColorID::TextSelectForeground, color) returns this
// constant to specify that the foreground color should not be changed
// (ie. a colored text keeps its colors  when selected).
// Of course if other plaforms work like the Mac, they can use it too.
#define NS_DONT_CHANGE_COLOR NS_RGB(0x01, 0x01, 0x01)

// Similar with NS_DONT_CHANGE_COLOR, except NS_DONT_CHANGE_COLOR would returns
// complementary color if fg color is same as bg color.
// NS_CHANGE_COLOR_IF_SAME_AS_BG would returns
// ColorID::TextSelectForegroundCustom if fg and bg color are the same.
#define NS_CHANGE_COLOR_IF_SAME_AS_BG NS_RGB(0x02, 0x02, 0x02)

// ---------------------------------------------------------------------
//  Special colors for ColorID::IME* and ColorID::SpellCheckerUnderline
// ---------------------------------------------------------------------

// For background color only.
#define NS_TRANSPARENT NS_RGBA(0x01, 0x00, 0x00, 0x00)
// For foreground color only.
#define NS_SAME_AS_FOREGROUND_COLOR NS_RGBA(0x02, 0x00, 0x00, 0x00)
#define NS_40PERCENT_FOREGROUND_COLOR NS_RGBA(0x03, 0x00, 0x00, 0x00)

#define NS_IS_SELECTION_SPECIAL_COLOR(c)                          \
  ((c) == NS_TRANSPARENT || (c) == NS_SAME_AS_FOREGROUND_COLOR || \
   (c) == NS_40PERCENT_FOREGROUND_COLOR)

// ------------------------------------------
//  Bits for eIntID_AlertNotificationOrigin
// ------------------------------------------

#define NS_ALERT_HORIZONTAL 1
#define NS_ALERT_LEFT 2
#define NS_ALERT_TOP 4

#endif /* __LookAndFeel */
