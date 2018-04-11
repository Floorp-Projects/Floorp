/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLookAndFeel.h"
#include "nsCocoaFeatures.h"
#include "nsIServiceManager.h"
#include "nsNativeThemeColors.h"
#include "nsStyleConsts.h"
#include "nsCocoaFeatures.h"
#include "nsIContent.h"
#include "gfxFont.h"
#include "gfxFontConstants.h"
#include "gfxPlatformMac.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/widget/WidgetMessageUtils.h"

#import <Cocoa/Cocoa.h>

// This must be included last:
#include "nsObjCExceptions.h"

enum {
  mozNSScrollerStyleLegacy       = 0,
  mozNSScrollerStyleOverlay      = 1
};
typedef NSInteger mozNSScrollerStyle;

@interface NSScroller(AvailableSinceLion)
+ (mozNSScrollerStyle)preferredScrollerStyle;
@end

nsLookAndFeel::nsLookAndFeel()
 : nsXPLookAndFeel()
 , mUseOverlayScrollbars(-1)
 , mUseOverlayScrollbarsCached(false)
 , mAllowOverlayScrollbarsOverlap(-1)
 , mAllowOverlayScrollbarsOverlapCached(false)
 , mColorTextSelectBackground{}, mColorTextSelectBackgroundDisabled{}, mColorHighlight{}, mColorMenuHover{}, mColorTextSelectForeground{}, mColorMenuHoverText{}, mColorButtonText{}, mHasColorButtonText(false)
 , mColorButtonHoverText{}, mColorText{}, mColorWindowText{}, mColorActiveCaption{}, mColorActiveBorder{}, mColorGrayText{}, mColorInactiveBorder{}, mColorInactiveCaption{}, mColorScrollbar{}, mColorThreeDHighlight{}, mColorMenu{}, mColorWindowFrame{}, mColorFieldText{}, mColorDialog{}, mColorDialogText{}, mColorDragTargetZone{}, mColorChromeActive{}, mColorChromeInactive{}, mColorFocusRing{}, mColorTextSelect{}, mColorDisabledToolbarText{}, mColorMenuSelect{}, mColorCellHighlight{}, mColorEvenTreeRow{}, mColorOddTreeRow{}, mColorActiveSourceListSelection{}, mInitialized(false)
{
}

nsLookAndFeel::~nsLookAndFeel()
{
}

static nscolor GetColorFromNSColor(NSColor* aColor)
{
  NSColor* deviceColor = [aColor colorUsingColorSpaceName:NSDeviceRGBColorSpace];
  return NS_RGB((unsigned int)([deviceColor redComponent] * 255.0),
                (unsigned int)([deviceColor greenComponent] * 255.0),
                (unsigned int)([deviceColor blueComponent] * 255.0));
}

static nscolor GetColorFromNSColorWithAlpha(NSColor* aColor, float alpha)
{
  NSColor* deviceColor = [aColor colorUsingColorSpaceName:NSDeviceRGBColorSpace];
  return NS_RGBA((unsigned int)([deviceColor redComponent] * 255.0),
                 (unsigned int)([deviceColor greenComponent] * 255.0),
                 (unsigned int)([deviceColor blueComponent] * 255.0),
                 (unsigned int)(alpha * 255.0));
}

void
nsLookAndFeel::NativeInit()
{
  EnsureInit();
}

void
nsLookAndFeel::RefreshImpl()
{
  nsXPLookAndFeel::RefreshImpl();

  // We should only clear the cache if we're in the main browser process.
  // Otherwise, we should wait for the parent to inform us of new values
  // to cache via LookAndFeel::SetIntCache.
  if (XRE_IsParentProcess()) {
    mUseOverlayScrollbarsCached = false;
    mAllowOverlayScrollbarsOverlapCached = false;
  }

  // Fetch colors next time they are requested.
  mInitialized = false;
}

nsresult
nsLookAndFeel::NativeGetColor(ColorID aID, nscolor &aColor)
{
  EnsureInit();

  nsresult res = NS_OK;

  switch (aID) {
    case eColorID_WindowBackground:
      aColor = NS_RGB(0xff,0xff,0xff);
      break;
    case eColorID_WindowForeground:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;
    case eColorID_WidgetBackground:
      aColor = NS_RGB(0xdd,0xdd,0xdd);
      break;
    case eColorID_WidgetForeground:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;
    case eColorID_WidgetSelectBackground:
      aColor = NS_RGB(0x80,0x80,0x80);
      break;
    case eColorID_WidgetSelectForeground:
      aColor = NS_RGB(0x00,0x00,0x80);
      break;
    case eColorID_Widget3DHighlight:
      aColor = NS_RGB(0xa0,0xa0,0xa0);
      break;
    case eColorID_Widget3DShadow:
      aColor = NS_RGB(0x40,0x40,0x40);
      break;
    case eColorID_TextBackground:
      aColor = NS_RGB(0xff,0xff,0xff);
      break;
    case eColorID_TextForeground:
      aColor = NS_RGB(0x00,0x00,0x00);
      break;
    case eColorID_TextSelectBackground:
      aColor = mColorTextSelectBackground;
      break;
    // This is used to gray out the selection when it's not focused. Used with
    // nsISelectionController::SELECTION_DISABLED.
    case eColorID_TextSelectBackgroundDisabled:
      aColor = mColorTextSelectBackgroundDisabled;
      break;
    case eColorID_highlight: // CSS2 color
      aColor = mColorHighlight;
      break;
    case eColorID__moz_menuhover:
      aColor = mColorMenuHover;
      break;
    case eColorID_TextSelectForeground:
      aColor = mColorTextSelectForeground;
      break;
    case eColorID_highlighttext:  // CSS2 color
    case eColorID__moz_menuhovertext:
      aColor = mColorMenuHoverText;
      break;
    case eColorID_IMESelectedRawTextBackground:
    case eColorID_IMESelectedConvertedTextBackground:
    case eColorID_IMERawInputBackground:
    case eColorID_IMEConvertedTextBackground:
      aColor = NS_TRANSPARENT;
      break;
    case eColorID_IMESelectedRawTextForeground:
    case eColorID_IMESelectedConvertedTextForeground:
    case eColorID_IMERawInputForeground:
    case eColorID_IMEConvertedTextForeground:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case eColorID_IMERawInputUnderline:
    case eColorID_IMEConvertedTextUnderline:
      aColor = NS_40PERCENT_FOREGROUND_COLOR;
      break;
    case eColorID_IMESelectedRawTextUnderline:
    case eColorID_IMESelectedConvertedTextUnderline:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case eColorID_SpellCheckerUnderline:
      aColor = NS_RGB(0xff, 0, 0);
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
      //  if querying the Appearance Manager fails ;)
      //
    case eColorID__moz_mac_buttonactivetext:
    case eColorID__moz_mac_defaultbuttontext:
      if (mHasColorButtonText) {
        aColor = mColorButtonText;
        break;
      }
      // Otherwise fall through and return the regular button text:
      MOZ_FALLTHROUGH;
    case eColorID_buttontext:
    case eColorID__moz_buttonhovertext:
      aColor = mColorButtonHoverText;
      break;
    case eColorID_captiontext:
    case eColorID_menutext:
    case eColorID_infotext:
    case eColorID__moz_menubartext:
      aColor = mColorText;
      break;
    case eColorID_windowtext:
      aColor = mColorWindowText;
      break;
    case eColorID_activecaption:
      aColor = mColorActiveCaption;
      break;
    case eColorID_activeborder:
      aColor = mColorActiveBorder;
      break;
     case eColorID_appworkspace:
      aColor = NS_RGB(0xFF,0xFF,0xFF);
      break;
    case eColorID_background:
      aColor = NS_RGB(0x63,0x63,0xCE);
      break;
    case eColorID_buttonface:
    case eColorID__moz_buttonhoverface:
      aColor = NS_RGB(0xF0,0xF0,0xF0);
      break;
    case eColorID_buttonhighlight:
      aColor = NS_RGB(0xFF,0xFF,0xFF);
      break;
    case eColorID_buttonshadow:
      aColor = NS_RGB(0xDC,0xDC,0xDC);
      break;
    case eColorID_graytext:
      aColor = mColorGrayText;
      break;
    case eColorID_inactiveborder:
      aColor = mColorInactiveBorder;
      break;
    case eColorID_inactivecaption:
      aColor = mColorInactiveCaption;
      break;
    case eColorID_inactivecaptiontext:
      aColor = NS_RGB(0x45,0x45,0x45);
      break;
    case eColorID_scrollbar:
      aColor = mColorScrollbar;
      break;
    case eColorID_threeddarkshadow:
      aColor = NS_RGB(0xDC,0xDC,0xDC);
      break;
    case eColorID_threedshadow:
      aColor = NS_RGB(0xE0,0xE0,0xE0);
      break;
    case eColorID_threedface:
      aColor = NS_RGB(0xF0,0xF0,0xF0);
      break;
    case eColorID_threedhighlight:
      aColor = mColorThreeDHighlight;
      break;
    case eColorID_threedlightshadow:
      aColor = NS_RGB(0xDA,0xDA,0xDA);
      break;
    case eColorID_menu:
      aColor = mColorMenu;
      break;
    case eColorID_infobackground:
      aColor = NS_RGB(0xFF,0xFF,0xC7);
      break;
    case eColorID_windowframe:
      aColor = mColorWindowFrame;
      break;
    case eColorID_window:
    case eColorID__moz_field:
    case eColorID__moz_combobox:
      aColor = NS_RGB(0xff,0xff,0xff);
      break;
    case eColorID__moz_fieldtext:
    case eColorID__moz_comboboxtext:
      aColor = mColorFieldText;
      break;
    case eColorID__moz_dialog:
      aColor = mColorDialog;
      break;
    case eColorID__moz_dialogtext:
    case eColorID__moz_cellhighlighttext:
    case eColorID__moz_html_cellhighlighttext:
      aColor = mColorDialogText;
      break;
    case eColorID__moz_dragtargetzone:
      aColor = mColorDragTargetZone;
      break;
    case eColorID__moz_mac_chrome_active:
      aColor = mColorChromeActive;
      break;
    case eColorID__moz_mac_chrome_inactive:
      aColor = mColorChromeInactive;
      break;
    case eColorID__moz_mac_focusring:
      aColor = mColorFocusRing;
      break;
    case eColorID__moz_mac_menushadow:
      aColor = NS_RGB(0xA3,0xA3,0xA3);
      break;
    case eColorID__moz_mac_menutextdisable:
      aColor = NS_RGB(0x98,0x98,0x98);
      break;
    case eColorID__moz_mac_menutextselect:
      aColor = mColorTextSelect;
      break;
    case eColorID__moz_mac_disabledtoolbartext:
      aColor = mColorDisabledToolbarText;
      break;
    case eColorID__moz_mac_menuselect:
      aColor = mColorMenuSelect;
      break;
    case eColorID__moz_buttondefault:
      aColor = NS_RGB(0xDC,0xDC,0xDC);
      break;
    case eColorID__moz_cellhighlight:
    case eColorID__moz_html_cellhighlight:
    case eColorID__moz_mac_secondaryhighlight:
      // For inactive list selection
      aColor = mColorCellHighlight;
      break;
    case eColorID__moz_eventreerow:
      // Background color of even list rows.
      aColor = mColorEvenTreeRow;
      break;
    case eColorID__moz_oddtreerow:
      // Background color of odd list rows.
      aColor = mColorOddTreeRow;
      break;
    case eColorID__moz_nativehyperlinktext:
      // There appears to be no available system defined color. HARDCODING to the appropriate color.
      aColor = NS_RGB(0x14,0x4F,0xAE);
      break;
    // The following colors are supposed to be used as font-smoothing background
    // colors, in the chrome-only -moz-font-smoothing-background-color property.
    // This property is used for text on "vibrant" -moz-appearances.
    // The colors have been obtained from the system on 10.12.6 using the
    // program at https://bugzilla.mozilla.org/attachment.cgi?id=8907533 .
    // We could obtain them at runtime, but doing so may be expensive and
    // requires the use of the private API
    // -[NSVisualEffectView fontSmoothingBackgroundColor].
    case eColorID__moz_mac_vibrancy_light:
    case eColorID__moz_mac_vibrant_titlebar_light:
    case eColorID__moz_mac_source_list:
    case eColorID__moz_mac_tooltip:
      aColor = NS_RGB(0xf7,0xf7,0xf7);
      break;
    case eColorID__moz_mac_vibrancy_dark:
    case eColorID__moz_mac_vibrant_titlebar_dark:
      aColor = NS_RGB(0x28,0x28,0x28);
      break;
    case eColorID__moz_mac_menupopup:
    case eColorID__moz_mac_menuitem:
      aColor = NS_RGB(0xe6,0xe6,0xe6);
      break;
    case eColorID__moz_mac_source_list_selection:
      aColor = NS_RGB(0xc8,0xc8,0xc8);
      break;
    case eColorID__moz_mac_active_menuitem:
    case eColorID__moz_mac_active_source_list_selection:
      aColor = mColorActiveSourceListSelection;
      break;
    default:
      NS_WARNING("Someone asked nsILookAndFeel for a color I don't know about");
      aColor = NS_RGB(0xff,0xff,0xff);
      res = NS_ERROR_FAILURE;
      break;
    }

  return res;
}

nsresult
nsLookAndFeel::GetIntImpl(IntID aID, int32_t &aResult)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsresult res = nsXPLookAndFeel::GetIntImpl(aID, aResult);
  if (NS_SUCCEEDED(res))
    return res;
  res = NS_OK;

  switch (aID) {
    case eIntID_CaretBlinkTime:
      aResult = 567;
      break;
    case eIntID_CaretWidth:
      aResult = 1;
      break;
    case eIntID_ShowCaretDuringSelection:
      aResult = 0;
      break;
    case eIntID_SelectTextfieldsOnKeyFocus:
      // Select textfield content when focused by kbd
      // used by EventStateManager::sTextfieldSelectModel
      aResult = 1;
      break;
    case eIntID_SubmenuDelay:
      aResult = 200;
      break;
    case eIntID_TooltipDelay:
      aResult = 500;
      break;
    case eIntID_MenusCanOverlapOSBar:
      // xul popups are not allowed to overlap the menubar.
      aResult = 0;
      break;
    case eIntID_SkipNavigatingDisabledMenuItem:
      aResult = 1;
      break;
    case eIntID_DragThresholdX:
    case eIntID_DragThresholdY:
      aResult = 4;
      break;
    case eIntID_ScrollArrowStyle:
      aResult = eScrollArrow_None;
      break;
    case eIntID_ScrollSliderStyle:
      aResult = eScrollThumbStyle_Proportional;
      break;
    case eIntID_UseOverlayScrollbars:
      if (!mUseOverlayScrollbarsCached) {
        mUseOverlayScrollbars = SystemWantsOverlayScrollbars() ? 1 : 0;
        mUseOverlayScrollbarsCached = true;
      }
      aResult = mUseOverlayScrollbars;
      break;
    case eIntID_AllowOverlayScrollbarsOverlap:
      if (!mAllowOverlayScrollbarsOverlapCached) {
        mAllowOverlayScrollbarsOverlap = AllowOverlayScrollbarsOverlap() ? 1 : 0;
        mAllowOverlayScrollbarsOverlapCached = true;
      }
      aResult = mAllowOverlayScrollbarsOverlap;
      break;
    case eIntID_ScrollbarDisplayOnMouseMove:
      aResult = 0;
      break;
    case eIntID_ScrollbarFadeBeginDelay:
      aResult = 450;
      break;
    case eIntID_ScrollbarFadeDuration:
      aResult = 200;
      break;
    case eIntID_TreeOpenDelay:
      aResult = 1000;
      break;
    case eIntID_TreeCloseDelay:
      aResult = 1000;
      break;
    case eIntID_TreeLazyScrollDelay:
      aResult = 150;
      break;
    case eIntID_TreeScrollDelay:
      aResult = 100;
      break;
    case eIntID_TreeScrollLinesMax:
      aResult = 3;
      break;
    case eIntID_DWMCompositor:
    case eIntID_WindowsClassic:
    case eIntID_WindowsDefaultTheme:
    case eIntID_TouchEnabled:
    case eIntID_WindowsThemeIdentifier:
    case eIntID_OperatingSystemVersionIdentifier:
      aResult = 0;
      res = NS_ERROR_NOT_IMPLEMENTED;
      break;
    case eIntID_MacGraphiteTheme:
      aResult = [NSColor currentControlTint] == NSGraphiteControlTint;
      break;
    case eIntID_MacYosemiteTheme:
      aResult = nsCocoaFeatures::OnYosemiteOrLater();
      break;
    case eIntID_AlertNotificationOrigin:
      aResult = NS_ALERT_TOP;
      break;
    case eIntID_TabFocusModel:
      aResult = [NSApp isFullKeyboardAccessEnabled] ?
                  nsIContent::eTabFocus_any : nsIContent::eTabFocus_textControlsMask;
      break;
    case eIntID_ScrollToClick:
    {
      aResult = [[NSUserDefaults standardUserDefaults] boolForKey:@"AppleScrollerPagingBehavior"];
    }
      break;
    case eIntID_ChosenMenuItemsShouldBlink:
      aResult = 1;
      break;
    case eIntID_IMERawInputUnderlineStyle:
    case eIntID_IMEConvertedTextUnderlineStyle:
    case eIntID_IMESelectedRawTextUnderlineStyle:
    case eIntID_IMESelectedConvertedTextUnderline:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_SOLID;
      break;
    case eIntID_SpellCheckerUnderlineStyle:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_DOTTED;
      break;
    case eIntID_ScrollbarButtonAutoRepeatBehavior:
      aResult = 0;
      break;
    case eIntID_SwipeAnimationEnabled:
      aResult = 0;
      if ([NSEvent respondsToSelector:@selector(
            isSwipeTrackingFromScrollEventsEnabled)]) {
        aResult = [NSEvent isSwipeTrackingFromScrollEventsEnabled] ? 1 : 0;
      }
      break;
    case eIntID_ContextMenuOffsetVertical:
      aResult = -6;
      break;
    case eIntID_ContextMenuOffsetHorizontal:
      aResult = 1;
      break;
    default:
      aResult = 0;
      res = NS_ERROR_FAILURE;
  }
  return res;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult
nsLookAndFeel::GetFloatImpl(FloatID aID, float &aResult)
{
  nsresult res = nsXPLookAndFeel::GetFloatImpl(aID, aResult);
  if (NS_SUCCEEDED(res))
    return res;
  res = NS_OK;

  switch (aID) {
    case eFloatID_IMEUnderlineRelativeSize:
      aResult = 2.0f;
      break;
    case eFloatID_SpellCheckerUnderlineRelativeSize:
      aResult = 2.0f;
      break;
    default:
      aResult = -1.0;
      res = NS_ERROR_FAILURE;
  }

  return res;
}

bool nsLookAndFeel::UseOverlayScrollbars()
{
  return GetInt(eIntID_UseOverlayScrollbars) != 0;
}

bool nsLookAndFeel::SystemWantsOverlayScrollbars()
{
  return ([NSScroller respondsToSelector:@selector(preferredScrollerStyle)] &&
          [NSScroller preferredScrollerStyle] == mozNSScrollerStyleOverlay);
}

bool nsLookAndFeel::AllowOverlayScrollbarsOverlap()
{
  return (UseOverlayScrollbars());
}

bool
nsLookAndFeel::GetFontImpl(FontID aID, nsString &aFontName,
                           gfxFontStyle &aFontStyle,
                           float aDevPixPerCSSPixel)
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

    // hack for now
    if (aID == eFont_Window || aID == eFont_Document) {
        aFontStyle.style      = NS_FONT_STYLE_NORMAL;
        aFontStyle.weight     = NS_FONT_WEIGHT_NORMAL;
        aFontStyle.stretch    = NS_FONT_STRETCH_NORMAL;
        aFontStyle.size       = 14 * aDevPixPerCSSPixel;
        aFontStyle.systemFont = true;

        aFontName.AssignLiteral("sans-serif");
        return true;
    }

    gfxPlatformMac::LookupSystemFont(aID, aFontName, aFontStyle,
                                     aDevPixPerCSSPixel);

    return true;

    NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
}

nsTArray<LookAndFeelInt>
nsLookAndFeel::GetIntCacheImpl()
{
  nsTArray<LookAndFeelInt> lookAndFeelIntCache =
    nsXPLookAndFeel::GetIntCacheImpl();

  LookAndFeelInt useOverlayScrollbars;
  useOverlayScrollbars.id = eIntID_UseOverlayScrollbars;
  useOverlayScrollbars.value = GetInt(eIntID_UseOverlayScrollbars);
  lookAndFeelIntCache.AppendElement(useOverlayScrollbars);

  LookAndFeelInt allowOverlayScrollbarsOverlap;
  allowOverlayScrollbarsOverlap.id = eIntID_AllowOverlayScrollbarsOverlap;
  allowOverlayScrollbarsOverlap.value = GetInt(eIntID_AllowOverlayScrollbarsOverlap);
  lookAndFeelIntCache.AppendElement(allowOverlayScrollbarsOverlap);

  return lookAndFeelIntCache;
}

void
nsLookAndFeel::SetIntCacheImpl(const nsTArray<LookAndFeelInt>& aLookAndFeelIntCache)
{
  for (auto entry : aLookAndFeelIntCache) {
    switch(entry.id) {
      case eIntID_UseOverlayScrollbars:
        mUseOverlayScrollbars = entry.value;
        mUseOverlayScrollbarsCached = true;
        break;
      case eIntID_AllowOverlayScrollbarsOverlap:
        mAllowOverlayScrollbarsOverlap = entry.value;
        mAllowOverlayScrollbarsOverlapCached = true;
        break;
    }
  }
}

void
nsLookAndFeel::EnsureInit()
{
  if (mInitialized) {
    return;
  }
  mInitialized = true;

  NS_OBJC_BEGIN_TRY_ABORT_BLOCK

  nscolor color;

  mColorTextSelectBackground = GetColorFromNSColor(
    [NSColor selectedTextBackgroundColor]);
  mColorTextSelectBackgroundDisabled = GetColorFromNSColor(
    [NSColor secondarySelectedControlColor]);

  mColorHighlight = GetColorFromNSColor([NSColor alternateSelectedControlColor]);
  mColorMenuHover = GetColorFromNSColor([NSColor alternateSelectedControlColor]);

  GetColor(eColorID_TextSelectBackground, color);
  if (color == 0x000000) {
    mColorTextSelectForeground = NS_RGB(0xff,0xff,0xff);
  } else {
    mColorTextSelectForeground = NS_DONT_CHANGE_COLOR;
  }

  mColorMenuHoverText = GetColorFromNSColor(
    [NSColor alternateSelectedControlTextColor]);

  if (nsCocoaFeatures::OnYosemiteOrLater()) {
    mColorButtonText = NS_RGB(0xFF,0xFF,0xFF);
    mHasColorButtonText = true;
  }

  mColorButtonHoverText = GetColorFromNSColor([NSColor controlTextColor]);
  mColorText = GetColorFromNSColor([NSColor textColor]);
  mColorWindowText = GetColorFromNSColor([NSColor windowFrameTextColor]);
  mColorActiveCaption = GetColorFromNSColor([NSColor gridColor]);
  mColorActiveBorder = GetColorFromNSColor([NSColor keyboardFocusIndicatorColor]);
  mColorGrayText = GetColorFromNSColor([NSColor disabledControlTextColor]);
  mColorInactiveBorder = GetColorFromNSColor([NSColor controlBackgroundColor]);
  mColorInactiveCaption = GetColorFromNSColor([NSColor controlBackgroundColor]);
  mColorScrollbar = GetColorFromNSColor([NSColor scrollBarColor]);
  mColorThreeDHighlight = GetColorFromNSColor([NSColor highlightColor]);
  mColorMenu = GetColorFromNSColor([NSColor alternateSelectedControlTextColor]);
  mColorWindowFrame = GetColorFromNSColor([NSColor gridColor]);
  mColorFieldText = GetColorFromNSColor([NSColor controlTextColor]);
  mColorDialog = GetColorFromNSColor([NSColor controlHighlightColor]);
  mColorDialogText = GetColorFromNSColor([NSColor controlTextColor]);
  mColorDragTargetZone = GetColorFromNSColor([NSColor selectedControlColor]);

  int grey = NativeGreyColorAsInt(toolbarFillGrey, true);
  mColorChromeActive = NS_RGB(grey, grey, grey);
  grey = NativeGreyColorAsInt(toolbarFillGrey, false);
  mColorChromeInactive = NS_RGB(grey, grey, grey);

  mColorFocusRing = GetColorFromNSColorWithAlpha([NSColor keyboardFocusIndicatorColor],
                                                 0.48);

  mColorTextSelect = GetColorFromNSColor([NSColor selectedMenuItemTextColor]);
  mColorDisabledToolbarText = GetColorFromNSColor([NSColor disabledControlTextColor]);
  mColorMenuSelect = GetColorFromNSColor([NSColor alternateSelectedControlColor]);
  mColorCellHighlight = GetColorFromNSColor([NSColor secondarySelectedControlColor]);
  mColorEvenTreeRow = GetColorFromNSColor([[NSColor controlAlternatingRowBackgroundColors]
                                           objectAtIndex:0]);
  mColorOddTreeRow = GetColorFromNSColor([[NSColor controlAlternatingRowBackgroundColors]
                                          objectAtIndex:1]);

  color = [NSColor currentControlTint];
  mColorActiveSourceListSelection = (color == NSGraphiteControlTint) ?
                                    NS_RGB(0xa0,0xa0,0xa0) :
                                    NS_RGB(0x0a,0x64,0xdc);

  NS_OBJC_END_TRY_ABORT_BLOCK
}
