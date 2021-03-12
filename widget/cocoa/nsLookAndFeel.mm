/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLookAndFeel.h"
#include "nsCocoaFeatures.h"
#include "nsNativeThemeColors.h"
#include "nsStyleConsts.h"
#include "nsCocoaFeatures.h"
#include "nsIContent.h"
#include "gfxFont.h"
#include "gfxFontConstants.h"
#include "gfxPlatformMac.h"
#include "nsCSSColorUtils.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/widget/WidgetMessageUtils.h"

#import <Cocoa/Cocoa.h>

// This must be included last:
#include "nsObjCExceptions.h"

// Available from 10.12 onwards; test availability at runtime before using
@interface NSWorkspace (AvailableSinceSierra)
@property(readonly) BOOL accessibilityDisplayShouldReduceMotion;
@end

#if !defined(MAC_OS_X_VERSION_10_14) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_14
@interface NSApplication (NSApplicationAppearance)
@property(strong) NSAppearance* appearance NS_AVAILABLE_MAC(10_14);
@end
#endif

static void RegisterRespectSystemAppearancePrefListenerOnce();

nsLookAndFeel::nsLookAndFeel(const LookAndFeelCache* aCache)
    : nsXPLookAndFeel(),
      mUseOverlayScrollbars(-1),
      mUseOverlayScrollbarsCached(false),
      mAllowOverlayScrollbarsOverlap(-1),
      mAllowOverlayScrollbarsOverlapCached(false),
      mSystemUsesDarkTheme(-1),
      mSystemUsesDarkThemeCached(false),
      mUseAccessibilityTheme(-1),
      mUseAccessibilityThemeCached(false),
      mColorTextSelectBackground(0),
      mColorTextSelectBackgroundDisabled(0),
      mColorHighlight(0),
      mColorTextSelectForeground(0),
      mColorMenuHoverText(0),
      mColorButtonText(0),
      mColorButtonHoverText(0),
      mColorText(0),
      mColorWindowText(0),
      mColorActiveCaption(0),
      mColorActiveBorder(0),
      mColorGrayText(0),
      mColorInactiveBorder(0),
      mColorInactiveCaption(0),
      mColorScrollbar(0),
      mColorThreeDHighlight(0),
      mColorMenu(0),
      mColorWindowFrame(0),
      mColorFieldText(0),
      mColorDialog(0),
      mColorDialogText(0),
      mColorDragTargetZone(0),
      mColorChromeActive(0),
      mColorChromeInactive(0),
      mColorFocusRing(0),
      mColorTextSelect(0),
      mColorDisabledToolbarText(0),
      mColorMenuSelect(0),
      mColorCellHighlight(0),
      mColorEvenTreeRow(0),
      mColorOddTreeRow(0),
      mColorActiveSourceListSelection(0),
      mInitialized(false) {
  if (aCache) {
    DoSetCache(*aCache);
  }
  RegisterRespectSystemAppearancePrefListenerOnce();
}

nsLookAndFeel::~nsLookAndFeel() {}

static nscolor GetColorFromNSColor(NSColor* aColor) {
  NSColor* deviceColor = [aColor colorUsingColorSpaceName:NSDeviceRGBColorSpace];
  return NS_RGB((unsigned int)([deviceColor redComponent] * 255.0),
                (unsigned int)([deviceColor greenComponent] * 255.0),
                (unsigned int)([deviceColor blueComponent] * 255.0));
}

static nscolor GetColorFromNSColorWithAlpha(NSColor* aColor, float alpha) {
  NSColor* deviceColor = [aColor colorUsingColorSpaceName:NSDeviceRGBColorSpace];
  return NS_RGBA((unsigned int)([deviceColor redComponent] * 255.0),
                 (unsigned int)([deviceColor greenComponent] * 255.0),
                 (unsigned int)([deviceColor blueComponent] * 255.0),
                 (unsigned int)(alpha * 255.0));
}

void nsLookAndFeel::NativeInit() { EnsureInit(); }

void nsLookAndFeel::RefreshImpl() {
  nsXPLookAndFeel::RefreshImpl();

  // We should only clear the cache if we're in the main browser process.
  // Otherwise, we should wait for the parent to inform us of new values to
  // cache via SetCacheImpl.
  if (XRE_IsParentProcess()) {
    mUseOverlayScrollbarsCached = false;
    mAllowOverlayScrollbarsOverlapCached = false;
    mPrefersReducedMotionCached = false;
    mSystemUsesDarkThemeCached = false;
    mUseAccessibilityThemeCached = false;
    // Fetch colors next time they are requested.
    mInitialized = false;
  }
}

// Turns an opaque selection color into a partially transparent selection color,
// which usually leads to better contrast with the text color and which should
// look more visually appealing in most contexts.
// The idea is that the text and its regular, non-selected background are
// usually chosen in such a way that they contrast well. Making the selection
// color partially transparent causes the selection color to mix with the text's
// regular background, so the end result will often have better contrast with
// the text than an arbitrary opaque selection color.
// The motivating example for this is the URL bar text field in the dark theme:
// White text on a light blue selection color has very bad contrast, whereas
// white text on dark blue (which what you get if you mix partially-transparent
// light blue with the black textbox background) has much better contrast.
nscolor nsLookAndFeel::ProcessSelectionBackground(nscolor aColor) {
  uint16_t hue, sat, value;
  uint8_t alpha;
  nscolor resultColor = aColor;
  NS_RGB2HSV(resultColor, hue, sat, value, alpha);
  int factor = 2;
  alpha = alpha / factor;
  if (sat > 0) {
    // The color is not a shade of grey, restore the saturation taken away by
    // the transparency.
    sat = mozilla::clamped(sat * factor, 0, 255);
  } else {
    // The color is a shade of grey, find the value that looks equivalent
    // on a white background with the given opacity.
    value = mozilla::clamped(255 - (255 - value) * factor, 0, 255);
  }
  NS_HSV2RGB(resultColor, hue, sat, value, alpha);
  return resultColor;
}

nsresult nsLookAndFeel::NativeGetColor(ColorID aID, nscolor& aColor) {
  EnsureInit();

  nsresult res = NS_OK;

  switch (aID) {
    case ColorID::WindowBackground:
      aColor = NS_RGB(0xff, 0xff, 0xff);
      break;
    case ColorID::WindowForeground:
      aColor = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::WidgetBackground:
    case ColorID::Infobackground:
      aColor = NS_RGB(0xdd, 0xdd, 0xdd);
      break;
    case ColorID::WidgetForeground:
      aColor = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::WidgetSelectBackground:
      aColor = NS_RGB(0x80, 0x80, 0x80);
      break;
    case ColorID::WidgetSelectForeground:
      aColor = NS_RGB(0x00, 0x00, 0x80);
      break;
    case ColorID::Widget3DHighlight:
      aColor = NS_RGB(0xa0, 0xa0, 0xa0);
      break;
    case ColorID::Widget3DShadow:
      aColor = NS_RGB(0x40, 0x40, 0x40);
      break;
    case ColorID::TextBackground:
      aColor = NS_RGB(0xff, 0xff, 0xff);
      break;
    case ColorID::TextForeground:
      aColor = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::TextSelectBackground:
      aColor = ProcessSelectionBackground(mColorTextSelectBackground);
      break;
    // This is used to gray out the selection when it's not focused. Used with
    // nsISelectionController::SELECTION_DISABLED.
    case ColorID::TextSelectBackgroundDisabled:
      aColor = ProcessSelectionBackground(mColorTextSelectBackgroundDisabled);
      break;
    case ColorID::Highlight:  // CSS2 color
    case ColorID::MozAccentColor:
    case ColorID::MozMenuhover:
      aColor = mColorHighlight;
      break;
    case ColorID::TextSelectForeground:
      aColor = mColorTextSelectForeground;
      break;
    case ColorID::Highlighttext:  // CSS2 color
    case ColorID::MozAccentColorForeground:
    case ColorID::MozMenuhovertext:
      aColor = mColorMenuHoverText;
      break;
    case ColorID::IMESelectedRawTextBackground:
    case ColorID::IMESelectedConvertedTextBackground:
    case ColorID::IMERawInputBackground:
    case ColorID::IMEConvertedTextBackground:
      aColor = NS_TRANSPARENT;
      break;
    case ColorID::IMESelectedRawTextForeground:
    case ColorID::IMESelectedConvertedTextForeground:
    case ColorID::IMERawInputForeground:
    case ColorID::IMEConvertedTextForeground:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::IMERawInputUnderline:
    case ColorID::IMEConvertedTextUnderline:
      aColor = NS_40PERCENT_FOREGROUND_COLOR;
      break;
    case ColorID::IMESelectedRawTextUnderline:
    case ColorID::IMESelectedConvertedTextUnderline:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::SpellCheckerUnderline:
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
    case ColorID::MozMacButtonactivetext:
    case ColorID::MozMacDefaultbuttontext:
      aColor = mColorButtonText;
      break;
    case ColorID::Buttontext:
    case ColorID::MozButtonhovertext:
      aColor = mColorButtonHoverText;
      break;
    case ColorID::Captiontext:
    case ColorID::Menutext:
    case ColorID::Infotext:
    case ColorID::MozMenubartext:
      aColor = mColorText;
      break;
    case ColorID::Windowtext:
      aColor = mColorWindowText;
      break;
    case ColorID::Activecaption:
      aColor = mColorActiveCaption;
      break;
    case ColorID::Activeborder:
      aColor = mColorActiveBorder;
      break;
    case ColorID::Appworkspace:
      aColor = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::Background:
      aColor = NS_RGB(0x63, 0x63, 0xCE);
      break;
    case ColorID::Buttonface:
    case ColorID::MozButtonhoverface:
      aColor = NS_RGB(0xF0, 0xF0, 0xF0);
      break;
    case ColorID::Buttonhighlight:
      aColor = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::Buttonshadow:
      aColor = NS_RGB(0xDC, 0xDC, 0xDC);
      break;
    case ColorID::Graytext:
      aColor = mColorGrayText;
      break;
    case ColorID::Inactiveborder:
      aColor = mColorInactiveBorder;
      break;
    case ColorID::Inactivecaption:
      aColor = mColorInactiveCaption;
      break;
    case ColorID::Inactivecaptiontext:
      aColor = NS_RGB(0x45, 0x45, 0x45);
      break;
    case ColorID::Scrollbar:
      aColor = mColorScrollbar;
      break;
    case ColorID::Threeddarkshadow:
      aColor = NS_RGB(0xDC, 0xDC, 0xDC);
      break;
    case ColorID::Threedshadow:
      aColor = NS_RGB(0xE0, 0xE0, 0xE0);
      break;
    case ColorID::Threedface:
      aColor = NS_RGB(0xF0, 0xF0, 0xF0);
      break;
    case ColorID::Threedhighlight:
      aColor = mColorThreeDHighlight;
      break;
    case ColorID::Threedlightshadow:
      aColor = NS_RGB(0xDA, 0xDA, 0xDA);
      break;
    case ColorID::Menu:
      aColor = mColorMenu;
      break;
    case ColorID::Windowframe:
      aColor = mColorWindowFrame;
      break;
    case ColorID::Window:
    case ColorID::Field:
    case ColorID::MozCombobox:
      aColor = NS_RGB(0xff, 0xff, 0xff);
      break;
    case ColorID::Fieldtext:
    case ColorID::MozComboboxtext:
      aColor = mColorFieldText;
      break;
    case ColorID::MozDialog:
      aColor = mColorDialog;
      break;
    case ColorID::MozDialogtext:
    case ColorID::MozCellhighlighttext:
    case ColorID::MozHtmlCellhighlighttext:
    case ColorID::MozColheadertext:
    case ColorID::MozColheaderhovertext:
      aColor = mColorDialogText;
      break;
    case ColorID::MozDragtargetzone:
      aColor = mColorDragTargetZone;
      break;
    case ColorID::MozMacChromeActive:
      aColor = mColorChromeActive;
      break;
    case ColorID::MozMacChromeInactive:
      aColor = mColorChromeInactive;
      break;
    case ColorID::MozMacFocusring:
      aColor = mColorFocusRing;
      break;
    case ColorID::MozMacMenushadow:
      aColor = NS_RGB(0xA3, 0xA3, 0xA3);
      break;
    case ColorID::MozMacMenutextdisable:
      aColor = NS_RGB(0x98, 0x98, 0x98);
      break;
    case ColorID::MozMacMenutextselect:
      aColor = mColorTextSelect;
      break;
    case ColorID::MozMacDisabledtoolbartext:
      aColor = mColorDisabledToolbarText;
      break;
    case ColorID::MozMacMenuselect:
      aColor = mColorMenuSelect;
      break;
    case ColorID::MozButtondefault:
      aColor = NS_RGB(0xDC, 0xDC, 0xDC);
      break;
    case ColorID::MozCellhighlight:
    case ColorID::MozHtmlCellhighlight:
    case ColorID::MozMacSecondaryhighlight:
      // For inactive list selection
      aColor = mColorCellHighlight;
      break;
    case ColorID::MozEventreerow:
      // Background color of even list rows.
      aColor = mColorEvenTreeRow;
      break;
    case ColorID::MozOddtreerow:
      // Background color of odd list rows.
      aColor = mColorOddTreeRow;
      break;
    case ColorID::MozNativehyperlinktext:
      // There appears to be no available system defined color. HARDCODING to the appropriate color.
      aColor = NS_RGB(0x14, 0x4F, 0xAE);
      break;
    // The following colors are supposed to be used as font-smoothing background
    // colors, in the chrome-only -moz-font-smoothing-background-color property.
    // This property is used for text on "vibrant" -moz-appearances.
    // The colors have been obtained from the system on 10.12.6 using the
    // program at https://bugzilla.mozilla.org/attachment.cgi?id=8907533 .
    // We could obtain them at runtime, but doing so may be expensive and
    // requires the use of the private API
    // -[NSVisualEffectView fontSmoothingBackgroundColor].
    case ColorID::MozMacVibrancyLight:
    case ColorID::MozMacVibrantTitlebarLight:
    case ColorID::MozMacSourceList:
    case ColorID::MozMacTooltip:
      aColor = NS_RGB(0xf7, 0xf7, 0xf7);
      break;
    case ColorID::MozMacVibrancyDark:
    case ColorID::MozMacVibrantTitlebarDark:
      aColor = NS_RGB(0x28, 0x28, 0x28);
      break;
    case ColorID::MozMacMenupopup:
    case ColorID::MozMacMenuitem:
      aColor = NS_RGB(0xe6, 0xe6, 0xe6);
      break;
    case ColorID::MozMacSourceListSelection:
      aColor = NS_RGB(0xc8, 0xc8, 0xc8);
      break;
    case ColorID::MozMacActiveMenuitem:
    case ColorID::MozMacActiveSourceListSelection:
      aColor = mColorActiveSourceListSelection;
      break;
    default:
      NS_WARNING("Someone asked nsILookAndFeel for a color I don't know about");
      aColor = NS_RGB(0xff, 0xff, 0xff);
      res = NS_ERROR_FAILURE;
      break;
  }

  return res;
}

nsresult nsLookAndFeel::NativeGetInt(IntID aID, int32_t& aResult) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  nsresult res = NS_OK;

  switch (aID) {
    case IntID::ScrollButtonLeftMouseButtonAction:
      aResult = 0;
      break;
    case IntID::ScrollButtonMiddleMouseButtonAction:
    case IntID::ScrollButtonRightMouseButtonAction:
      aResult = 3;
      break;
    case IntID::CaretBlinkTime:
      aResult = 567;
      break;
    case IntID::CaretWidth:
      aResult = 1;
      break;
    case IntID::ShowCaretDuringSelection:
      aResult = 0;
      break;
    case IntID::SelectTextfieldsOnKeyFocus:
      // Select textfield content when focused by kbd
      // used by EventStateManager::sTextfieldSelectModel
      aResult = 1;
      break;
    case IntID::SubmenuDelay:
      aResult = 200;
      break;
    case IntID::TooltipDelay:
      aResult = 500;
      break;
    case IntID::MenusCanOverlapOSBar:
      // xul popups are not allowed to overlap the menubar.
      aResult = 0;
      break;
    case IntID::SkipNavigatingDisabledMenuItem:
      aResult = 1;
      break;
    case IntID::DragThresholdX:
    case IntID::DragThresholdY:
      aResult = 4;
      break;
    case IntID::ScrollArrowStyle:
      aResult = eScrollArrow_None;
      break;
    case IntID::ScrollSliderStyle:
      aResult = eScrollThumbStyle_Proportional;
      break;
    case IntID::UseOverlayScrollbars:
      if (!mUseOverlayScrollbarsCached) {
        mUseOverlayScrollbars = NSScroller.preferredScrollerStyle == NSScrollerStyleOverlay ? 1 : 0;
        mUseOverlayScrollbarsCached = true;
      }
      aResult = mUseOverlayScrollbars;
      break;
    case IntID::AllowOverlayScrollbarsOverlap:
      if (!mAllowOverlayScrollbarsOverlapCached) {
        mAllowOverlayScrollbarsOverlap = AllowOverlayScrollbarsOverlap() ? 1 : 0;
        mAllowOverlayScrollbarsOverlapCached = true;
      }
      aResult = mAllowOverlayScrollbarsOverlap;
      break;
    case IntID::ScrollbarDisplayOnMouseMove:
      aResult = 0;
      break;
    case IntID::ScrollbarFadeBeginDelay:
      aResult = 450;
      break;
    case IntID::ScrollbarFadeDuration:
      aResult = 200;
      break;
    case IntID::TreeOpenDelay:
      aResult = 1000;
      break;
    case IntID::TreeCloseDelay:
      aResult = 1000;
      break;
    case IntID::TreeLazyScrollDelay:
      aResult = 150;
      break;
    case IntID::TreeScrollDelay:
      aResult = 100;
      break;
    case IntID::TreeScrollLinesMax:
      aResult = 3;
      break;
    case IntID::DWMCompositor:
    case IntID::WindowsClassic:
    case IntID::WindowsDefaultTheme:
    case IntID::TouchEnabled:
    case IntID::WindowsThemeIdentifier:
    case IntID::OperatingSystemVersionIdentifier:
      aResult = 0;
      res = NS_ERROR_NOT_IMPLEMENTED;
      break;
    case IntID::MacGraphiteTheme:
      aResult = [NSColor currentControlTint] == NSGraphiteControlTint;
      break;
    case IntID::MacBigSurTheme:
      aResult = nsCocoaFeatures::OnBigSurOrLater();
      break;
    case IntID::AlertNotificationOrigin:
      aResult = NS_ALERT_TOP;
      break;
    case IntID::TabFocusModel:
      aResult = [NSApp isFullKeyboardAccessEnabled] ? nsIContent::eTabFocus_any
                                                    : nsIContent::eTabFocus_textControlsMask;
      break;
    case IntID::ScrollToClick: {
      aResult = [[NSUserDefaults standardUserDefaults] boolForKey:@"AppleScrollerPagingBehavior"];
    } break;
    case IntID::ChosenMenuItemsShouldBlink:
      aResult = 1;
      break;
    case IntID::IMERawInputUnderlineStyle:
    case IntID::IMEConvertedTextUnderlineStyle:
    case IntID::IMESelectedRawTextUnderlineStyle:
    case IntID::IMESelectedConvertedTextUnderline:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_SOLID;
      break;
    case IntID::SpellCheckerUnderlineStyle:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_DOTTED;
      break;
    case IntID::ScrollbarButtonAutoRepeatBehavior:
      aResult = 0;
      break;
    case IntID::SwipeAnimationEnabled:
      aResult = 0;
      if ([NSEvent respondsToSelector:@selector(isSwipeTrackingFromScrollEventsEnabled)]) {
        aResult = [NSEvent isSwipeTrackingFromScrollEventsEnabled] ? 1 : 0;
      }
      break;
    case IntID::ContextMenuOffsetVertical:
      aResult = -6;
      break;
    case IntID::ContextMenuOffsetHorizontal:
      aResult = 1;
      break;
    case IntID::SystemUsesDarkTheme:
      if (!mSystemUsesDarkThemeCached) {
        mSystemUsesDarkTheme = SystemWantsDarkTheme();
        mSystemUsesDarkThemeCached = true;
      }
      aResult = mSystemUsesDarkTheme;
      break;
    case IntID::PrefersReducedMotion:
      // Without native event loops,
      // NSWorkspace.accessibilityDisplayShouldReduceMotion returns stale
      // information, so we get the information only on the parent processes
      // or when it's the initial query on child processes.  Otherwise we will
      // get the info via LookAndFeel::SetIntCache on child processes.
      if (!mPrefersReducedMotionCached) {
        mPrefersReducedMotion =
            [[NSWorkspace sharedWorkspace]
                respondsToSelector:@selector(accessibilityDisplayShouldReduceMotion)] &&
            [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
        mPrefersReducedMotionCached = true;
      }
      aResult = mPrefersReducedMotion;
      break;
    case IntID::UseAccessibilityTheme:
      // Without native event loops,
      // NSWorkspace.accessibilityDisplayShouldIncreaseContrast returns stale
      // information, so we get the information only on the parent processes
      // or when it's the initial query on child processes.  Otherwise we will
      // get the info via LookAndFeel::SetIntCache on child processes.
      if (!mUseAccessibilityThemeCached) {
        mUseAccessibilityTheme =
            [[NSWorkspace sharedWorkspace]
                respondsToSelector:@selector(accessibilityDisplayShouldIncreaseContrast)] &&
            [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldIncreaseContrast];
        mUseAccessibilityThemeCached = true;
      }
      aResult = mUseAccessibilityTheme;
      break;
    default:
      aResult = 0;
      res = NS_ERROR_FAILURE;
  }
  return res;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult nsLookAndFeel::NativeGetFloat(FloatID aID, float& aResult) {
  nsresult res = NS_OK;

  switch (aID) {
    case FloatID::IMEUnderlineRelativeSize:
      aResult = 2.0f;
      break;
    case FloatID::SpellCheckerUnderlineRelativeSize:
      aResult = 2.0f;
      break;
    default:
      aResult = -1.0;
      res = NS_ERROR_FAILURE;
  }

  return res;
}

bool nsLookAndFeel::UseOverlayScrollbars() { return GetInt(IntID::UseOverlayScrollbars) != 0; }

bool nsLookAndFeel::AllowOverlayScrollbarsOverlap() { return (UseOverlayScrollbars()); }

bool nsLookAndFeel::SystemWantsDarkTheme() {
  // This returns true if the macOS system appearance is set to dark mode on
  // 10.14+, false otherwise.
  if (!nsCocoaFeatures::OnMojaveOrLater()) {
    return false;
  }
  return !![[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"];
}

bool nsLookAndFeel::NativeGetFont(FontID aID, nsString& aFontName, gfxFontStyle& aFontStyle) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // hack for now
  if (aID == FontID::Window || aID == FontID::Document) {
    aFontStyle.style = mozilla::FontSlantStyle::Normal();
    aFontStyle.weight = mozilla::FontWeight::Normal();
    aFontStyle.stretch = mozilla::FontStretch::Normal();
    aFontStyle.size = 14;
    aFontStyle.systemFont = true;

    aFontName.AssignLiteral("sans-serif");
    return true;
  }

  // TODO: Add caching? Note that it needs to be thread-safe for stylo use.

  nsAutoCString name;
  gfxPlatformMac::LookupSystemFont(aID, name, aFontStyle);
  aFontName.Append(NS_ConvertUTF8toUTF16(name));

  return true;

  NS_OBJC_END_TRY_BLOCK_RETURN(false);
}

mozilla::widget::LookAndFeelCache nsLookAndFeel::GetCacheImpl() {
  LookAndFeelCache cache = nsXPLookAndFeel::GetCacheImpl();

  constexpr IntID kIntIdsToCache[] = {
      IntID::UseOverlayScrollbars, IntID::AllowOverlayScrollbarsOverlap,
      IntID::PrefersReducedMotion, IntID::SystemUsesDarkTheme, IntID::UseAccessibilityTheme};

  constexpr ColorID kColorIdsToCache[] = {
      ColorID::Highlight,
      ColorID::Highlighttext,
      ColorID::Menutext,
      ColorID::TextSelectBackground,
      ColorID::TextSelectBackgroundDisabled,
      ColorID::TextSelectForeground,
      ColorID::MozMacDefaultbuttontext,
      ColorID::MozButtonhovertext,
      ColorID::Windowtext,
      ColorID::Activecaption,
      ColorID::Activeborder,
      ColorID::Graytext,
      ColorID::Inactiveborder,
      ColorID::Inactivecaption,
      ColorID::Scrollbar,
      ColorID::Threedhighlight,
      ColorID::Menu,
      ColorID::Windowframe,
      ColorID::Fieldtext,
      ColorID::MozDialog,
      ColorID::MozDialogtext,
      ColorID::MozDragtargetzone,
      ColorID::MozMacChromeActive,
      ColorID::MozMacChromeInactive,
      ColorID::MozMacFocusring,
      ColorID::MozMacMenutextselect,
      ColorID::MozMacDisabledtoolbartext,
      ColorID::MozMacMenuselect,
      ColorID::MozCellhighlight,
      ColorID::MozEventreerow,
      ColorID::MozOddtreerow,
      ColorID::MozMacActiveMenuitem,
  };

  for (IntID id : kIntIdsToCache) {
    cache.mInts().AppendElement(LookAndFeelInt(id, GetInt(id)));
  }

  for (ColorID id : kColorIdsToCache) {
    nscolor color = 0;
    NativeGetColor(id, color);
    cache.mColors().AppendElement(LookAndFeelColor(id, color));
  }

  return cache;
}

void nsLookAndFeel::SetCacheImpl(const LookAndFeelCache& aCache) { DoSetCache(aCache); }

void nsLookAndFeel::DoSetCache(const LookAndFeelCache& aCache) {
  for (auto entry : aCache.mInts()) {
    switch (entry.id()) {
      case IntID::UseOverlayScrollbars:
        mUseOverlayScrollbars = entry.value();
        mUseOverlayScrollbarsCached = true;
        break;
      case IntID::AllowOverlayScrollbarsOverlap:
        mAllowOverlayScrollbarsOverlap = entry.value();
        mAllowOverlayScrollbarsOverlapCached = true;
        break;
      case IntID::SystemUsesDarkTheme:
        mSystemUsesDarkTheme = entry.value();
        mSystemUsesDarkThemeCached = true;
        break;
      case IntID::PrefersReducedMotion:
        mPrefersReducedMotion = entry.value();
        mPrefersReducedMotionCached = true;
        break;
      case IntID::UseAccessibilityTheme:
        mUseAccessibilityTheme = entry.value();
        mUseAccessibilityThemeCached = true;
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Bogus Int ID in cache");
        break;
    }
  }
  for (const auto& entry : aCache.mColors()) {
    nscolor& slot = [&]() -> nscolor& {
      switch (entry.id()) {
        case ColorID::Highlight:
          return mColorHighlight;
        case ColorID::Highlighttext:
          return mColorMenuHoverText;
        case ColorID::Menutext:
          return mColorText;
        case ColorID::TextSelectBackground:
          return mColorTextSelectBackground;
        case ColorID::TextSelectBackgroundDisabled:
          return mColorTextSelectBackgroundDisabled;
        case ColorID::TextSelectForeground:
          return mColorTextSelectForeground;
        case ColorID::MozMacDefaultbuttontext:
          return mColorButtonText;
        case ColorID::MozButtonhovertext:
          return mColorButtonHoverText;
        case ColorID::Windowtext:
          return mColorWindowText;
        case ColorID::Activecaption:
          return mColorActiveCaption;
        case ColorID::Activeborder:
          return mColorActiveBorder;
        case ColorID::Graytext:
          return mColorGrayText;
        case ColorID::Inactiveborder:
          return mColorInactiveBorder;
        case ColorID::Inactivecaption:
          return mColorInactiveCaption;
        case ColorID::Scrollbar:
          return mColorScrollbar;
        case ColorID::Threedhighlight:
          return mColorThreeDHighlight;
        case ColorID::Menu:
          return mColorMenu;
        case ColorID::Windowframe:
          return mColorWindowFrame;
        case ColorID::Fieldtext:
          return mColorFieldText;
        case ColorID::MozDialog:
          return mColorDialog;
        case ColorID::MozDialogtext:
          return mColorDialogText;
        case ColorID::MozDragtargetzone:
          return mColorDragTargetZone;
        case ColorID::MozMacChromeActive:
          return mColorChromeActive;
        case ColorID::MozMacChromeInactive:
          return mColorChromeInactive;
        case ColorID::MozMacFocusring:
          return mColorFocusRing;
        case ColorID::MozMacMenutextselect:
          return mColorTextSelect;
        case ColorID::MozMacDisabledtoolbartext:
          return mColorDisabledToolbarText;
        case ColorID::MozMacMenuselect:
          return mColorMenuSelect;
        case ColorID::MozCellhighlight:
          return mColorCellHighlight;
        case ColorID::MozEventreerow:
          return mColorEvenTreeRow;
        case ColorID::MozOddtreerow:
          return mColorOddTreeRow;
        case ColorID::MozMacActiveMenuitem:
          return mColorActiveSourceListSelection;
        default:
          MOZ_ASSERT_UNREACHABLE("Unknown color in the cache");
          return mColorOddTreeRow;
      }
    }();
    slot = entry.color();
  }
  mInitialized = true;
}

void nsLookAndFeel::EnsureInit() {
  if (mInitialized) {
    return;
  }
  mInitialized = true;

  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK

  nscolor color;

  mColorTextSelectBackground = GetColorFromNSColor([NSColor selectedTextBackgroundColor]);
  mColorTextSelectBackgroundDisabled = GetColorFromNSColor([NSColor secondarySelectedControlColor]);

  mColorHighlight = GetColorFromNSColor([NSColor alternateSelectedControlColor]);

  GetColor(ColorID::TextSelectBackground, color);
  if (color == 0x000000) {
    mColorTextSelectForeground = NS_RGB(0xff, 0xff, 0xff);
  } else {
    mColorTextSelectForeground = NS_DONT_CHANGE_COLOR;
  }

  mColorMenuHoverText = GetColorFromNSColor([NSColor alternateSelectedControlTextColor]);

  mColorButtonText = NS_RGB(0xFF, 0xFF, 0xFF);
  mColorButtonHoverText = GetColorFromNSColor([NSColor controlTextColor]);
  mColorText = GetColorFromNSColor([NSColor textColor]);
  mColorWindowText = GetColorFromNSColor([NSColor windowFrameTextColor]);
  mColorActiveCaption = GetColorFromNSColor([NSColor gridColor]);
  mColorActiveBorder = GetColorFromNSColor([NSColor keyboardFocusIndicatorColor]);
  NSColor* disabledColor = [NSColor disabledControlTextColor];
  mColorGrayText = GetColorFromNSColorWithAlpha(disabledColor, [disabledColor alphaComponent]);
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

  mColorFocusRing = GetColorFromNSColorWithAlpha([NSColor keyboardFocusIndicatorColor], 0.48);

  mColorTextSelect = GetColorFromNSColor([NSColor selectedMenuItemTextColor]);
  mColorDisabledToolbarText = GetColorFromNSColor([NSColor disabledControlTextColor]);
  mColorMenuSelect = GetColorFromNSColor([NSColor alternateSelectedControlColor]);
  mColorCellHighlight = GetColorFromNSColor([NSColor secondarySelectedControlColor]);
  mColorEvenTreeRow =
      GetColorFromNSColor([[NSColor controlAlternatingRowBackgroundColors] objectAtIndex:0]);
  mColorOddTreeRow =
      GetColorFromNSColor([[NSColor controlAlternatingRowBackgroundColors] objectAtIndex:1]);

  color = [NSColor currentControlTint];
  mColorActiveSourceListSelection =
      (color == NSGraphiteControlTint) ? NS_RGB(0xa0, 0xa0, 0xa0) : NS_RGB(0x0a, 0x64, 0xdc);

  RecordTelemetry();

  NS_OBJC_END_TRY_IGNORE_BLOCK
}

static void RespectSystemAppearancePrefChanged(const char* aPref, void* UserInfo) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (@available(macOS 10.14, *)) {
    if (StaticPrefs::widget_macos_respect_system_appearance()) {
      // nil means "no override".
      NSApp.appearance = nil;
    } else {
      // Override with aqua.
      NSApp.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    }
  }

  // Send a notification that ChildView reacts to. This will cause it to call ThemeChanged and
  // invalidate LookAndFeel colors.
  [[NSDistributedNotificationCenter defaultCenter]
      postNotificationName:@"AppleInterfaceThemeChangedNotification"
                    object:nil
                  userInfo:nil
        deliverImmediately:YES];
}

static void RegisterRespectSystemAppearancePrefListenerOnce() {
  static bool sRegistered = false;
  if (sRegistered || !XRE_IsParentProcess()) {
    return;
  }

  sRegistered = true;
  Preferences::RegisterCallbackAndCall(
      &RespectSystemAppearancePrefChanged,
      nsDependentCString(StaticPrefs::GetPrefName_widget_macos_respect_system_appearance()));
}
