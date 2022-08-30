/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppearanceOverride.h"
#include "mozilla/widget/ThemeChangeKind.h"
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
#include "SDKDeclarations.h"

#import <Cocoa/Cocoa.h>
#import <AppKit/NSColor.h>

// This must be included last:
#include "nsObjCExceptions.h"

using namespace mozilla;

@interface MOZLookAndFeelDynamicChangeObserver : NSObject
+ (void)startObserving;
@end

nsLookAndFeel::nsLookAndFeel() = default;

nsLookAndFeel::~nsLookAndFeel() = default;

void nsLookAndFeel::NativeInit() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK

  [MOZLookAndFeelDynamicChangeObserver startObserving];
  RecordTelemetry();

  NS_OBJC_END_TRY_ABORT_BLOCK
}

static nscolor GetColorFromNSColor(NSColor* aColor) {
  NSColor* deviceColor = [aColor colorUsingColorSpaceName:NSDeviceRGBColorSpace];
  return NS_RGBA((unsigned int)(deviceColor.redComponent * 255.0),
                 (unsigned int)(deviceColor.greenComponent * 255.0),
                 (unsigned int)(deviceColor.blueComponent * 255.0),
                 (unsigned int)(deviceColor.alphaComponent * 255.0));
}

static nscolor GetColorFromNSColorWithCustomAlpha(NSColor* aColor, float alpha) {
  NSColor* deviceColor = [aColor colorUsingColorSpaceName:NSDeviceRGBColorSpace];
  return NS_RGBA((unsigned int)(deviceColor.redComponent * 255.0),
                 (unsigned int)(deviceColor.greenComponent * 255.0),
                 (unsigned int)(deviceColor.blueComponent * 255.0), (unsigned int)(alpha * 255.0));
}

// Turns an opaque selection color into a partially transparent selection color,
// which usually leads to better contrast with the text color and which should
// look more visually appealing in most contexts.
// The idea is that the text and its regular, non-selected background are
// usually chosen in such a way that they contrast well. Making the selection
// color partially transparent causes the selection color to mix with the text's
// regular background, so the end result will often have better contrast with
// the text than an arbitrary opaque selection color.
// The motivating example for this is the light selection color on dark web
// pages: White text on a light blue selection color has very bad contrast,
// whereas white text on dark blue (which what you get if you mix
// partially-transparent light blue with the black textbox background) has much
// better contrast.
nscolor nsLookAndFeel::ProcessSelectionBackground(nscolor aColor, ColorScheme aScheme) {
  if (aScheme == ColorScheme::Dark) {
    // When we use a dark selection color, we do not change alpha because we do
    // not use dark selection in content. The dark system color is appropriate for
    // Firefox UI without needing to adjust its alpha.
    return aColor;
  }
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

nsresult nsLookAndFeel::NativeGetColor(ColorID aID, ColorScheme aScheme, nscolor& aColor) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK

  if (@available(macOS 10.14, *)) {
    // No-op. macOS 10.14+ supports dark mode, so currentAppearance can be set
    // to either Light or Dark.
  } else {
    // System colors before 10.14 are always Light.
    aScheme = ColorScheme::Light;
  }

  NSAppearance.currentAppearance = NSAppearanceForColorScheme(aScheme);

  nscolor color = 0;
  switch (aID) {
    case ColorID::Infobackground:
      color = NS_RGB(0xdd, 0xdd, 0xdd);
      break;
    case ColorID::Highlight:
      color = ProcessSelectionBackground(GetColorFromNSColor(NSColor.selectedTextBackgroundColor),
                                         aScheme);
      break;
    // This is used to gray out the selection when it's not focused. Used with
    // nsISelectionController::SELECTION_DISABLED.
    case ColorID::TextSelectDisabledBackground:
      color = ProcessSelectionBackground(GetColorFromNSColor(NSColor.secondarySelectedControlColor),
                                         aScheme);
      break;
    case ColorID::MozMenuhoverdisabled:
      aColor = NS_TRANSPARENT;
      break;
    case ColorID::MozMenuhover:
    case ColorID::Selecteditem:
      color = GetColorFromNSColor(NSColor.alternateSelectedControlColor);
      break;
    case ColorID::Accentcolortext:
    case ColorID::MozMenuhovertext:
    case ColorID::Selecteditemtext:
      color = GetColorFromNSColor(NSColor.alternateSelectedControlTextColor);
      break;
    case ColorID::IMESelectedRawTextBackground:
    case ColorID::IMESelectedConvertedTextBackground:
    case ColorID::IMERawInputBackground:
    case ColorID::IMEConvertedTextBackground:
      color = NS_TRANSPARENT;
      break;
    case ColorID::IMESelectedRawTextForeground:
    case ColorID::IMESelectedConvertedTextForeground:
    case ColorID::IMERawInputForeground:
    case ColorID::IMEConvertedTextForeground:
    case ColorID::Highlighttext:
      color = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::IMERawInputUnderline:
    case ColorID::IMEConvertedTextUnderline:
      color = NS_40PERCENT_FOREGROUND_COLOR;
      break;
    case ColorID::IMESelectedRawTextUnderline:
    case ColorID::IMESelectedConvertedTextUnderline:
      color = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::SpellCheckerUnderline:
      color = NS_RGB(0xff, 0, 0);
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
    case ColorID::MozMacDefaultbuttontext:
      color = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::MozButtonactivetext:
      // Pre-macOS 12, pressed buttons were filled with the highlight color and the text was white.
      // Starting with macOS 12, pressed (non-default) buttons are filled with medium gray and the
      // text color is the same as in the non-pressed state.
      color = nsCocoaFeatures::OnMontereyOrLater() ? GetColorFromNSColor(NSColor.controlTextColor)
                                                   : NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::Captiontext:
    case ColorID::Menutext:
    case ColorID::Infotext:
    case ColorID::MozMenubartext:
      color = GetColorFromNSColor(NSColor.textColor);
      break;
    case ColorID::Windowtext:
      color = GetColorFromNSColor(NSColor.windowFrameTextColor);
      break;
    case ColorID::Activecaption:
      color = GetColorFromNSColor(NSColor.gridColor);
      break;
    case ColorID::Activeborder:
      color = GetColorFromNSColor(NSColor.keyboardFocusIndicatorColor);
      break;
    case ColorID::Appworkspace:
      color = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::Background:
      color = NS_RGB(0x63, 0x63, 0xCE);
      break;
    case ColorID::Buttonface:
    case ColorID::MozButtonhoverface:
    case ColorID::MozButtonactiveface:
    case ColorID::MozButtondisabledface:
      color = GetColorFromNSColor(NSColor.controlColor);
      if (!NS_GET_A(color)) {
        color = GetColorFromNSColor(NSColor.controlBackgroundColor);
      }
      break;
    case ColorID::Buttonhighlight:
      color = GetColorFromNSColor(NSColor.selectedControlColor);
      break;
    case ColorID::Inactivecaptiontext:
      color = NS_RGB(0x45, 0x45, 0x45);
      break;
    case ColorID::Scrollbar:
      color = GetColorFromNSColor(NSColor.scrollBarColor);
      break;
    case ColorID::Threedhighlight:
      color = GetColorFromNSColor(NSColor.highlightColor);
      break;
    case ColorID::Buttonshadow:
    case ColorID::Threeddarkshadow:
      color = aScheme == ColorScheme::Dark ? *GenericDarkColor(aID) : NS_RGB(0xDC, 0xDC, 0xDC);
      break;
    case ColorID::Threedshadow:
      color = aScheme == ColorScheme::Dark ? *GenericDarkColor(aID) : NS_RGB(0xE0, 0xE0, 0xE0);
      break;
    case ColorID::Threedface:
      color = aScheme == ColorScheme::Dark ? *GenericDarkColor(aID) : NS_RGB(0xF0, 0xF0, 0xF0);
      break;
    case ColorID::Threedlightshadow:
    case ColorID::MozDisabledfield:
      color = aScheme == ColorScheme::Dark ? *GenericDarkColor(aID) : NS_RGB(0xDA, 0xDA, 0xDA);
      break;
    case ColorID::Menu:
      color = GetColorFromNSColor(NSColor.textBackgroundColor);
      break;
    case ColorID::Windowframe:
      color = GetColorFromNSColor(NSColor.windowFrameColor);
      break;
    case ColorID::Window: {
      if (@available(macOS 10.14, *)) {
        color = GetColorFromNSColor(NSColor.windowBackgroundColor);
      } else {
        // On 10.13 and below, NSColor.windowBackgroundColor is transparent black.
        // Use a light grey instead (taken from macOS 11.5).
        color = NS_RGB(0xF6, 0xF6, 0xF6);
      }
      break;
    }
    case ColorID::Field:
    case ColorID::MozCombobox:
    case ColorID::Inactiveborder:
    case ColorID::Inactivecaption:
    case ColorID::MozDialog:
      color = GetColorFromNSColor(NSColor.controlBackgroundColor);
      break;
    case ColorID::Fieldtext:
    case ColorID::MozComboboxtext:
    case ColorID::Buttontext:
    case ColorID::MozButtonhovertext:
    case ColorID::MozDialogtext:
    case ColorID::MozCellhighlighttext:
    case ColorID::MozColheadertext:
    case ColorID::MozColheaderhovertext:
      color = GetColorFromNSColor(NSColor.controlTextColor);
      break;
    case ColorID::MozDragtargetzone:
      color = GetColorFromNSColor(NSColor.selectedControlColor);
      break;
    case ColorID::MozMacChromeActive: {
      int grey = NativeGreyColorAsInt(toolbarFillGrey, true);
      color = NS_RGB(grey, grey, grey);
      break;
    }
    case ColorID::MozMacChromeInactive: {
      int grey = NativeGreyColorAsInt(toolbarFillGrey, false);
      color = NS_RGB(grey, grey, grey);
      break;
    }
    case ColorID::MozMacFocusring:
      color = GetColorFromNSColorWithCustomAlpha(NSColor.keyboardFocusIndicatorColor, 0.48);
      break;
    case ColorID::MozMacMenushadow:
      color = NS_RGB(0xA3, 0xA3, 0xA3);
      break;
    case ColorID::MozMacMenutextdisable:
      color = NS_RGB(0x98, 0x98, 0x98);
      break;
    case ColorID::MozMacMenutextselect:
      color = GetColorFromNSColor(NSColor.selectedMenuItemTextColor);
      break;
    case ColorID::MozMacDisabledtoolbartext:
    case ColorID::Graytext:
      color = GetColorFromNSColor(NSColor.disabledControlTextColor);
      break;
    case ColorID::MozMacMenuselect:
      color = GetColorFromNSColor(NSColor.alternateSelectedControlColor);
      break;
    case ColorID::MozButtondefault:
      color = NS_RGB(0xDC, 0xDC, 0xDC);
      break;
    case ColorID::MozCellhighlight:
    case ColorID::MozMacSecondaryhighlight:
      // For inactive list selection
      color = GetColorFromNSColor(NSColor.secondarySelectedControlColor);
      break;
    case ColorID::MozEventreerow:
      // Background color of even list rows.
      color = GetColorFromNSColor(NSColor.controlAlternatingRowBackgroundColors[0]);
      break;
    case ColorID::MozOddtreerow:
      // Background color of odd list rows.
      color = GetColorFromNSColor(NSColor.controlAlternatingRowBackgroundColors[1]);
      break;
    case ColorID::MozNativehyperlinktext:
      color = GetColorFromNSColor(NSColor.linkColor);
      break;
    case ColorID::MozNativevisitedhyperlinktext:
      color = GetColorFromNSColor(NSColor.systemPurpleColor);
      break;
    case ColorID::MozMacTooltip:
    case ColorID::MozMacMenupopup:
    case ColorID::MozMacMenuitem:
      color = aScheme == ColorScheme::Light ? NS_RGB(0xf6, 0xf6, 0xf6) : NS_RGB(0x28, 0x28, 0x28);
      break;
    case ColorID::MozMacSourceList:
      color = aScheme == ColorScheme::Light ? NS_RGB(0xf6, 0xf6, 0xf6) : NS_RGB(0x2d, 0x2d, 0x2d);
      break;
    case ColorID::MozMacSourceListSelection:
      color = aScheme == ColorScheme::Light ? NS_RGB(0xd3, 0xd3, 0xd3) : NS_RGB(0x2d, 0x2d, 0x2d);
      break;
    case ColorID::MozMacActiveMenuitem:
    case ColorID::MozMacActiveSourceListSelection:
    case ColorID::Accentcolor:
      color = GetColorFromNSColor(ControlAccentColor());
      break;
    default:
      aColor = NS_RGB(0xff, 0xff, 0xff);
      return NS_ERROR_FAILURE;
  }

  aColor = color;
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK
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
    case IntID::UseOverlayScrollbars:
    case IntID::AllowOverlayScrollbarsOverlap:
      aResult = NSScroller.preferredScrollerStyle == NSScrollerStyleOverlay;
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
    case IntID::MacGraphiteTheme:
      aResult = NSColor.currentControlTint == NSGraphiteControlTint;
      break;
    case IntID::MacBigSurTheme:
      aResult = nsCocoaFeatures::OnBigSurOrLater();
      break;
    case IntID::MacRTL:
      aResult = IsSystemOrientationRTL();
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
      aResult = NSEvent.isSwipeTrackingFromScrollEventsEnabled;
      break;
    case IntID::ContextMenuOffsetVertical:
      aResult = -6;
      break;
    case IntID::ContextMenuOffsetHorizontal:
      aResult = 1;
      break;
    case IntID::SystemUsesDarkTheme:
      aResult = SystemWantsDarkTheme();
      break;
    case IntID::PrefersReducedMotion:
      aResult = NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion;
      break;
    case IntID::UseAccessibilityTheme:
      aResult = NSWorkspace.sharedWorkspace.accessibilityDisplayShouldIncreaseContrast;
      break;
    case IntID::VideoDynamicRange:
#ifdef EARLY_BETA_OR_EARLIER
      aResult = nsCocoaFeatures::OnCatalinaOrLater();
#else
      aResult = nsCocoaFeatures::OnBigSurOrLater();
#endif
      break;
    default:
      aResult = 0;
      res = NS_ERROR_FAILURE;
  }
  return res;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult nsLookAndFeel::NativeGetFloat(FloatID aID, float& aResult) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  nsresult res = NS_OK;

  switch (aID) {
    case FloatID::IMEUnderlineRelativeSize:
      aResult = 2.0f;
      break;
    case FloatID::SpellCheckerUnderlineRelativeSize:
      aResult = 2.0f;
      break;
    case FloatID::CursorScale: {
      id uaDefaults = [[NSUserDefaults alloc] initWithSuiteName:@"com.apple.universalaccess"];
      float f = [uaDefaults floatForKey:@"mouseDriverCursorSize"];
      [uaDefaults release];
      aResult = f > 0.0 ? f : 1.0;  // default to 1.0 if value not available
      break;
    }
    default:
      aResult = -1.0;
      res = NS_ERROR_FAILURE;
  }

  return res;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

bool nsLookAndFeel::SystemWantsDarkTheme() {
  // This returns true if the macOS system appearance is set to dark mode on
  // 10.14+, false otherwise.
  if (@available(macOS 10.14, *)) {
    NSAppearanceName aquaOrDarkAqua = [NSApp.effectiveAppearance
        bestMatchFromAppearancesWithNames:@[ NSAppearanceNameAqua, NSAppearanceNameDarkAqua ]];
    return [aquaOrDarkAqua isEqualToString:NSAppearanceNameDarkAqua];
  }
  return false;
}

/*static*/
bool nsLookAndFeel::IsSystemOrientationRTL() {
  NSWindow* window = [[NSWindow alloc] initWithContentRect:NSZeroRect
                                                 styleMask:NSWindowStyleMaskBorderless
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];
  auto direction = window.windowTitlebarLayoutDirection;
  [window release];
  return direction == NSUserInterfaceLayoutDirectionRightToLeft;
}

bool nsLookAndFeel::NativeGetFont(FontID aID, nsString& aFontName, gfxFontStyle& aFontStyle) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // hack for now
  if (aID == FontID::MozWindow || aID == FontID::MozDocument) {
    aFontStyle.style = mozilla::FontSlantStyle::NORMAL;
    aFontStyle.weight = mozilla::FontWeight::NORMAL;
    aFontStyle.stretch = mozilla::FontStretch::NORMAL;
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

@implementation MOZLookAndFeelDynamicChangeObserver

+ (void)startObserving {
  static MOZLookAndFeelDynamicChangeObserver* gInstance = nil;
  if (!gInstance) {
    gInstance = [[MOZLookAndFeelDynamicChangeObserver alloc] init];  // leaked
  }
}

- (instancetype)init {
  self = [super init];

  [NSNotificationCenter.defaultCenter addObserver:self
                                         selector:@selector(colorsChanged)
                                             name:NSControlTintDidChangeNotification
                                           object:nil];
  [NSNotificationCenter.defaultCenter addObserver:self
                                         selector:@selector(colorsChanged)
                                             name:NSSystemColorsDidChangeNotification
                                           object:nil];

  if (@available(macOS 10.14, *)) {
    [NSWorkspace.sharedWorkspace.notificationCenter
        addObserver:self
           selector:@selector(mediaQueriesChanged)
               name:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
             object:nil];
  } else {
    [NSNotificationCenter.defaultCenter
        addObserver:self
           selector:@selector(mediaQueriesChanged)
               name:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
             object:nil];
  }

  [NSNotificationCenter.defaultCenter addObserver:self
                                         selector:@selector(scrollbarsChanged)
                                             name:NSPreferredScrollerStyleDidChangeNotification
                                           object:nil];
  [NSDistributedNotificationCenter.defaultCenter
             addObserver:self
                selector:@selector(scrollbarsChanged)
                    name:@"AppleAquaScrollBarVariantChanged"
                  object:nil
      suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];
  [NSDistributedNotificationCenter.defaultCenter
             addObserver:self
                selector:@selector(cachedValuesChanged)
                    name:@"AppleNoRedisplayAppearancePreferenceChanged"
                  object:nil
      suspensionBehavior:NSNotificationSuspensionBehaviorCoalesce];
  [NSDistributedNotificationCenter.defaultCenter
             addObserver:self
                selector:@selector(cachedValuesChanged)
                    name:@"com.apple.KeyboardUIModeDidChange"
                  object:nil
      suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];

  [MOZGlobalAppearance.sharedInstance addObserver:self
                                       forKeyPath:@"effectiveAppearance"
                                          options:0
                                          context:nil];
  [NSApp addObserver:self forKeyPath:@"effectiveAppearance" options:0 context:nil];

  return self;
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context {
  if ([keyPath isEqualToString:@"effectiveAppearance"]) {
    [self entireThemeChanged];
  } else {
    [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
  }
}

- (void)entireThemeChanged {
  LookAndFeel::NotifyChangedAllWindows(widget::ThemeChangeKind::StyleAndLayout);
}

- (void)scrollbarsChanged {
  LookAndFeel::NotifyChangedAllWindows(widget::ThemeChangeKind::StyleAndLayout);
}

- (void)mediaQueriesChanged {
  LookAndFeel::NotifyChangedAllWindows(widget::ThemeChangeKind::MediaQueriesOnly);
}

- (void)colorsChanged {
  LookAndFeel::NotifyChangedAllWindows(widget::ThemeChangeKind::Style);
}

- (void)cachedValuesChanged {
  // We only need to re-cache (and broadcast) updated LookAndFeel values, so that they're up-to-date
  // the next time they're queried. No further change handling is needed.
  // TODO: Add a change hint for this which avoids the unnecessary media query invalidation.
  LookAndFeel::NotifyChangedAllWindows(widget::ThemeChangeKind::MediaQueriesOnly);
}
@end
