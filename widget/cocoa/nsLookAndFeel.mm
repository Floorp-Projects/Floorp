/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsCSSColorUtils.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/widget/WidgetMessageUtils.h"

#import <Cocoa/Cocoa.h>

// This must be included last:
#include "nsObjCExceptions.h"

enum { mozNSScrollerStyleLegacy = 0, mozNSScrollerStyleOverlay = 1 };
typedef NSInteger mozNSScrollerStyle;

@interface NSScroller (AvailableSinceLion)
+ (mozNSScrollerStyle)preferredScrollerStyle;
@end

// Available from 10.12 onwards; test availability at runtime before using
@interface NSWorkspace (AvailableSinceSierra)
@property(readonly) BOOL accessibilityDisplayShouldReduceMotion;
@end

nsLookAndFeel::nsLookAndFeel()
    : nsXPLookAndFeel(),
      mUseOverlayScrollbars(-1),
      mUseOverlayScrollbarsCached(false),
      mAllowOverlayScrollbarsOverlap(-1),
      mAllowOverlayScrollbarsOverlapCached(false),
      mPrefersReducedMotion(-1),
      mPrefersReducedMotionCached(false),
      mSystemUsesDarkTheme(-1),
      mSystemUsesDarkThemeCached(false),
      mColorTextSelectBackground(0),
      mColorTextSelectBackgroundDisabled(0),
      mColorHighlight(0),
      mColorMenuHover(0),
      mColorTextSelectForeground(0),
      mColorMenuHoverText(0),
      mColorButtonText(0),
      mHasColorButtonText(false),
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
      mInitialized(false) {}

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
  if (mShouldRetainCacheForTest) {
    return;
  }

  nsXPLookAndFeel::RefreshImpl();

  // We should only clear the cache if we're in the main browser process.
  // Otherwise, we should wait for the parent to inform us of new values
  // to cache via LookAndFeel::SetIntCache.
  if (XRE_IsParentProcess()) {
    mUseOverlayScrollbarsCached = false;
    mAllowOverlayScrollbarsOverlapCached = false;
    mPrefersReducedMotionCached = false;
    mSystemUsesDarkThemeCached = false;
  }

  // Fetch colors next time they are requested.
  mInitialized = false;
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
      aColor = mColorHighlight;
      break;
    case ColorID::MozMenuhover:
      aColor = mColorMenuHover;
      break;
    case ColorID::TextSelectForeground:
      aColor = mColorTextSelectForeground;
      break;
    case ColorID::Highlighttext:  // CSS2 color
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
      if (mHasColorButtonText) {
        aColor = mColorButtonText;
        break;
      }
      // Otherwise fall through and return the regular button text:
      MOZ_FALLTHROUGH;
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
    case ColorID::Infobackground:
      aColor = NS_RGB(0xFF, 0xFF, 0xC7);
      break;
    case ColorID::Windowframe:
      aColor = mColorWindowFrame;
      break;
    case ColorID::Window:
    case ColorID::MozField:
    case ColorID::MozCombobox:
      aColor = NS_RGB(0xff, 0xff, 0xff);
      break;
    case ColorID::MozFieldtext:
    case ColorID::MozComboboxtext:
      aColor = mColorFieldText;
      break;
    case ColorID::MozDialog:
      aColor = mColorDialog;
      break;
    case ColorID::MozDialogtext:
    case ColorID::MozCellhighlighttext:
    case ColorID::MozHtmlCellhighlighttext:
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

nsresult nsLookAndFeel::GetIntImpl(IntID aID, int32_t& aResult) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsresult res = nsXPLookAndFeel::GetIntImpl(aID, aResult);
  if (NS_SUCCEEDED(res)) return res;
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
      aResult = [NSApp isFullKeyboardAccessEnabled] ? nsIContent::eTabFocus_any
                                                    : nsIContent::eTabFocus_textControlsMask;
      break;
    case eIntID_ScrollToClick: {
      aResult = [[NSUserDefaults standardUserDefaults] boolForKey:@"AppleScrollerPagingBehavior"];
    } break;
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
      if ([NSEvent respondsToSelector:@selector(isSwipeTrackingFromScrollEventsEnabled)]) {
        aResult = [NSEvent isSwipeTrackingFromScrollEventsEnabled] ? 1 : 0;
      }
      break;
    case eIntID_ContextMenuOffsetVertical:
      aResult = -6;
      break;
    case eIntID_ContextMenuOffsetHorizontal:
      aResult = 1;
      break;
    case eIntID_SystemUsesDarkTheme:
      if (!mSystemUsesDarkThemeCached) {
        mSystemUsesDarkTheme = SystemWantsDarkTheme();
        mSystemUsesDarkThemeCached = true;
      }
      aResult = mSystemUsesDarkTheme;
      break;
    case eIntID_PrefersReducedMotion:
      // Without native event loops,
      // NSWorkspace.accessibilityDisplayShouldReduceMotion returns stale
      // information, so we get the information only on the parent processes
      // or when it's the initial query on child processes.  Otherwise we will
      // get the info via LookAndFeel::SetIntCache on child processes.
      if (!mPrefersReducedMotionCached &&
          [[NSWorkspace sharedWorkspace]
              respondsToSelector:@selector(accessibilityDisplayShouldReduceMotion)]) {
        mPrefersReducedMotion =
            [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion] ? 1 : 0;
        mPrefersReducedMotionCached = true;
      }
      aResult = mPrefersReducedMotion;
      break;
    default:
      aResult = 0;
      res = NS_ERROR_FAILURE;
  }
  return res;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult nsLookAndFeel::GetFloatImpl(FloatID aID, float& aResult) {
  nsresult res = nsXPLookAndFeel::GetFloatImpl(aID, aResult);
  if (NS_SUCCEEDED(res)) return res;
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

bool nsLookAndFeel::UseOverlayScrollbars() { return GetInt(eIntID_UseOverlayScrollbars) != 0; }

bool nsLookAndFeel::SystemWantsOverlayScrollbars() {
  return ([NSScroller respondsToSelector:@selector(preferredScrollerStyle)] &&
          [NSScroller preferredScrollerStyle] == mozNSScrollerStyleOverlay);
}

bool nsLookAndFeel::AllowOverlayScrollbarsOverlap() { return (UseOverlayScrollbars()); }

bool nsLookAndFeel::SystemWantsDarkTheme() {
  // This returns true if the macOS system appearance is set to dark mode on
  // 10.14+, false otherwise.
  if (!nsCocoaFeatures::OnMojaveOrLater()) {
    return false;
  }
  return !![[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"];
}

bool nsLookAndFeel::GetFontImpl(FontID aID, nsString& aFontName, gfxFontStyle& aFontStyle) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  // hack for now
  if (aID == eFont_Window || aID == eFont_Document) {
    aFontStyle.style = mozilla::FontSlantStyle::Normal();
    aFontStyle.weight = mozilla::FontWeight::Normal();
    aFontStyle.stretch = mozilla::FontStretch::Normal();
    aFontStyle.size = 14;
    aFontStyle.systemFont = true;

    aFontName.AssignLiteral("sans-serif");
    return true;
  }

  nsAutoCString name;
  gfxPlatformMac::LookupSystemFont(aID, name, aFontStyle);
  aFontName.Append(NS_ConvertUTF8toUTF16(name));

  return true;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
}

nsTArray<LookAndFeelInt> nsLookAndFeel::GetIntCacheImpl() {
  nsTArray<LookAndFeelInt> lookAndFeelIntCache = nsXPLookAndFeel::GetIntCacheImpl();

  LookAndFeelInt useOverlayScrollbars;
  useOverlayScrollbars.id = eIntID_UseOverlayScrollbars;
  useOverlayScrollbars.value = GetInt(eIntID_UseOverlayScrollbars);
  lookAndFeelIntCache.AppendElement(useOverlayScrollbars);

  LookAndFeelInt allowOverlayScrollbarsOverlap;
  allowOverlayScrollbarsOverlap.id = eIntID_AllowOverlayScrollbarsOverlap;
  allowOverlayScrollbarsOverlap.value = GetInt(eIntID_AllowOverlayScrollbarsOverlap);
  lookAndFeelIntCache.AppendElement(allowOverlayScrollbarsOverlap);

  LookAndFeelInt prefersReducedMotion;
  prefersReducedMotion.id = eIntID_PrefersReducedMotion;
  prefersReducedMotion.value = GetInt(eIntID_PrefersReducedMotion);
  lookAndFeelIntCache.AppendElement(prefersReducedMotion);

  LookAndFeelInt systemUsesDarkTheme;
  systemUsesDarkTheme.id = eIntID_SystemUsesDarkTheme;
  systemUsesDarkTheme.value = GetInt(eIntID_SystemUsesDarkTheme);
  lookAndFeelIntCache.AppendElement(systemUsesDarkTheme);

  return lookAndFeelIntCache;
}

void nsLookAndFeel::SetIntCacheImpl(const nsTArray<LookAndFeelInt>& aLookAndFeelIntCache) {
  for (auto entry : aLookAndFeelIntCache) {
    switch (entry.id) {
      case eIntID_UseOverlayScrollbars:
        mUseOverlayScrollbars = entry.value;
        mUseOverlayScrollbarsCached = true;
        break;
      case eIntID_AllowOverlayScrollbarsOverlap:
        mAllowOverlayScrollbarsOverlap = entry.value;
        mAllowOverlayScrollbarsOverlapCached = true;
        break;
      case eIntID_SystemUsesDarkTheme:
        mSystemUsesDarkTheme = entry.value;
        mSystemUsesDarkThemeCached = true;
        break;
      case eIntID_PrefersReducedMotion:
        mPrefersReducedMotion = entry.value;
        mPrefersReducedMotionCached = true;
        break;
    }
  }
}

void nsLookAndFeel::EnsureInit() {
  if (mInitialized) {
    return;
  }
  mInitialized = true;

  NS_OBJC_BEGIN_TRY_ABORT_BLOCK

  nscolor color;

  mColorTextSelectBackground = GetColorFromNSColor([NSColor selectedTextBackgroundColor]);
  mColorTextSelectBackgroundDisabled = GetColorFromNSColor([NSColor secondarySelectedControlColor]);

  mColorHighlight = GetColorFromNSColor([NSColor alternateSelectedControlColor]);
  mColorMenuHover = GetColorFromNSColor([NSColor alternateSelectedControlColor]);

  GetColor(ColorID::TextSelectBackground, color);
  if (color == 0x000000) {
    mColorTextSelectForeground = NS_RGB(0xff, 0xff, 0xff);
  } else {
    mColorTextSelectForeground = NS_DONT_CHANGE_COLOR;
  }

  mColorMenuHoverText = GetColorFromNSColor([NSColor alternateSelectedControlTextColor]);

  if (nsCocoaFeatures::OnYosemiteOrLater()) {
    mColorButtonText = NS_RGB(0xFF, 0xFF, 0xFF);
    mHasColorButtonText = true;
  }

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

  NS_OBJC_END_TRY_ABORT_BLOCK
}
