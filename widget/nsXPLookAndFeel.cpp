/* -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nscore.h"

#include "nsXPLookAndFeel.h"
#include "nsLookAndFeel.h"
#include "HeadlessLookAndFeel.h"
#include "RemoteLookAndFeel.h"
#include "nsContentUtils.h"
#include "nsCRT.h"
#include "nsFont.h"
#include "nsIFrame.h"
#include "nsIXULRuntime.h"
#include "nsNativeBasicTheme.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/ServoCSSParser.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_editor.h"
#include "mozilla/StaticPrefs_findbar.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/dom/Document.h"
#include "mozilla/PreferenceSheet.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/widget/WidgetMessageUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryScalarEnums.h"

#include "gfxPlatform.h"
#include "gfxFont.h"

#include "qcms.h"

#ifdef DEBUG
#  include "nsSize.h"
#endif

using namespace mozilla;

using IntID = mozilla::LookAndFeel::IntID;
using FloatID = mozilla::LookAndFeel::FloatID;
using ColorID = mozilla::LookAndFeel::ColorID;
using FontID = mozilla::LookAndFeel::FontID;

template <typename Index, typename Value, Index kEnd>
class EnumeratedCache {
  static constexpr uint32_t ChunkFor(Index aIndex) {
    return uint32_t(aIndex) >> 5;  // >> 5 is the same as / 32.
  }
  static constexpr uint32_t BitFor(Index aIndex) {
    return 1u << (uint32_t(aIndex) & 31);
  }
  static constexpr uint32_t kChunks = ChunkFor(kEnd) + 1;

  mozilla::EnumeratedArray<Index, kEnd, Value> mEntries;
  uint32_t mValidity[kChunks] = {0};

 public:
  constexpr EnumeratedCache() = default;

  bool IsValid(Index aIndex) const {
    return mValidity[ChunkFor(aIndex)] & BitFor(aIndex);
  }

  const Value* Get(Index aIndex) const {
    return IsValid(aIndex) ? &mEntries[aIndex] : nullptr;
  }

  void Insert(Index aIndex, Value aValue) {
    mValidity[ChunkFor(aIndex)] |= BitFor(aIndex);
    mEntries[aIndex] = aValue;
  }

  void Remove(Index aIndex) {
    mValidity[ChunkFor(aIndex)] &= ~BitFor(aIndex);
    mEntries[aIndex] = Value();
  }

  void Clear() {
    for (auto& chunk : mValidity) {
      chunk = 0;
    }
    for (auto& entry : mEntries) {
      entry = Value();
    }
  }
};

static EnumeratedCache<ColorID, Maybe<nscolor>, ColorID::End> sLightColorCache;
static EnumeratedCache<ColorID, Maybe<nscolor>, ColorID::End> sDarkColorCache;
static EnumeratedCache<FloatID, Maybe<float>, FloatID::End> sFloatCache;
static EnumeratedCache<IntID, Maybe<int32_t>, IntID::End> sIntCache;
static EnumeratedCache<FontID, widget::LookAndFeelFont, FontID::End> sFontCache;

// To make one of these prefs toggleable from a reftest add a user
// pref in testing/profiles/reftest/user.js. For example, to make
// ui.useAccessibilityTheme toggleable, add:
//
// user_pref("ui.useAccessibilityTheme", 0);
//
// This needs to be of the same length and in the same order as
// LookAndFeel::IntID values.
static const char sIntPrefs[][42] = {
    "ui.caretBlinkTime",
    "ui.caretBlinkCount",
    "ui.caretWidth",
    "ui.caretVisibleWithSelection",
    "ui.selectTextfieldsOnKeyFocus",
    "ui.submenuDelay",
    "ui.menusCanOverlapOSBar",
    "ui.useOverlayScrollbars",
    "ui.allowOverlayScrollbarsOverlap",
    "ui.skipNavigatingDisabledMenuItem",
    "ui.dragThresholdX",
    "ui.dragThresholdY",
    "ui.useAccessibilityTheme",
    "ui.scrollArrowStyle",
    "ui.scrollSliderStyle",
    "ui.scrollButtonLeftMouseButtonAction",
    "ui.scrollButtonMiddleMouseButtonAction",
    "ui.scrollButtonRightMouseButtonAction",
    "ui.treeOpenDelay",
    "ui.treeCloseDelay",
    "ui.treeLazyScrollDelay",
    "ui.treeScrollDelay",
    "ui.treeScrollLinesMax",
    "accessibility.tabfocus",  // Weird one...
    "ui.chosenMenuItemsShouldBlink",
    "ui.windowsAccentColorInTitlebar",
    "ui.windowsDefaultTheme",
    "ui.dwmCompositor",
    "ui.windowsClassic",
    "ui.windowsGlass",
    "ui.macGraphiteTheme",
    "ui.macBigSurTheme",
    "ui.alertNotificationOrigin",
    "ui.scrollToClick",
    "ui.IMERawInputUnderlineStyle",
    "ui.IMESelectedRawTextUnderlineStyle",
    "ui.IMEConvertedTextUnderlineStyle",
    "ui.IMESelectedConvertedTextUnderlineStyle",
    "ui.SpellCheckerUnderlineStyle",
    "ui.menuBarDrag",
    "ui.windowsThemeIdentifier",
    "ui.operatingSystemVersionIdentifier",
    "ui.scrollbarButtonAutoRepeatBehavior",
    "ui.tooltipDelay",
    "ui.swipeAnimationEnabled",
    "ui.scrollbarDisplayOnMouseMove",
    "ui.scrollbarFadeBeginDelay",
    "ui.scrollbarFadeDuration",
    "ui.contextMenuOffsetVertical",
    "ui.contextMenuOffsetHorizontal",
    "ui.GtkCSDAvailable",
    "ui.GtkCSDHideTitlebarByDefault",
    "ui.GtkCSDTransparentBackground",
    "ui.GtkCSDMinimizeButton",
    "ui.GtkCSDMaximizeButton",
    "ui.GtkCSDCloseButton",
    "ui.GtkCSDMinimizeButtonPosition",
    "ui.GtkCSDMaximizeButtonPosition",
    "ui.GtkCSDCloseButtonPosition",
    "ui.GtkCSDReversedPlacement",
    "ui.systemUsesDarkTheme",
    "ui.prefersReducedMotion",
    "ui.primaryPointerCapabilities",
    "ui.allPointerCapabilities",
    "ui.systemVerticalScrollbarWidth",
    "ui.systemHorizontalScrollbarHeight",
};

static_assert(ArrayLength(sIntPrefs) == size_t(LookAndFeel::IntID::End),
              "Should have a pref for each int value");

// This array MUST be kept in the same order as the float id list in
// LookAndFeel.h
static const char sFloatPrefs[][37] = {
    "ui.IMEUnderlineRelativeSize",
    "ui.SpellCheckerUnderlineRelativeSize",
    "ui.caretAspectRatio",
    "ui.textScaleFactor",
};

static_assert(ArrayLength(sFloatPrefs) == size_t(LookAndFeel::FloatID::End),
              "Should have a pref for each float value");

// This array MUST be kept in the same order as the color list in
// specified/color.rs
static const char sColorPrefs[][41] = {
    "ui.windowBackground",
    "ui.windowForeground",
    "ui.widgetBackground",
    "ui.widgetForeground",
    "ui.widgetSelectBackground",
    "ui.widgetSelectForeground",
    "ui.widget3DHighlight",
    "ui.widget3DShadow",
    "ui.textBackground",
    "ui.textForeground",
    "ui.textSelectBackground",
    "ui.textSelectForeground",
    "ui.textSelectBackgroundDisabled",
    "ui.textSelectBackgroundAttention",
    "ui.textHighlightBackground",
    "ui.textHighlightForeground",
    "ui.IMERawInputBackground",
    "ui.IMERawInputForeground",
    "ui.IMERawInputUnderline",
    "ui.IMESelectedRawTextBackground",
    "ui.IMESelectedRawTextForeground",
    "ui.IMESelectedRawTextUnderline",
    "ui.IMEConvertedTextBackground",
    "ui.IMEConvertedTextForeground",
    "ui.IMEConvertedTextUnderline",
    "ui.IMESelectedConvertedTextBackground",
    "ui.IMESelectedConvertedTextForeground",
    "ui.IMESelectedConvertedTextUnderline",
    "ui.SpellCheckerUnderline",
    "ui.themedScrollbar",
    "ui.themedScrollbarInactive",
    "ui.themedScrollbarThumb",
    "ui.themedScrollbarThumbHover",
    "ui.themedScrollbarThumbActive",
    "ui.themedScrollbarThumbInactive",
    "ui.activeborder",
    "ui.activecaption",
    "ui.appworkspace",
    "ui.background",
    "ui.buttonface",
    "ui.buttonhighlight",
    "ui.buttonshadow",
    "ui.buttontext",
    "ui.captiontext",
    "ui.-moz-field",
    "ui.-moz-fieldtext",
    "ui.graytext",
    "ui.highlight",
    "ui.highlighttext",
    "ui.inactiveborder",
    "ui.inactivecaption",
    "ui.inactivecaptiontext",
    "ui.infobackground",
    "ui.infotext",
    "ui.menu",
    "ui.menutext",
    "ui.scrollbar",
    "ui.threeddarkshadow",
    "ui.threedface",
    "ui.threedhighlight",
    "ui.threedlightshadow",
    "ui.threedshadow",
    "ui.window",
    "ui.windowframe",
    "ui.windowtext",
    "ui.-moz-buttondefault",
    "ui.-moz-default-color",
    "ui.-moz-default-background-color",
    "ui.-moz-dialog",
    "ui.-moz-dialogtext",
    "ui.-moz-dragtargetzone",
    "ui.-moz-cellhighlight",
    "ui.-moz_cellhighlighttext",
    "ui.-moz-html-cellhighlight",
    "ui.-moz-html-cellhighlighttext",
    "ui.-moz-buttonhoverface",
    "ui.-moz_buttonhovertext",
    "ui.-moz_menuhover",
    "ui.-moz_menuhovertext",
    "ui.-moz_menubartext",
    "ui.-moz_menubarhovertext",
    "ui.-moz_eventreerow",
    "ui.-moz_oddtreerow",
    "ui.-moz-gtk-buttonactivetext",
    "ui.-moz-mac-buttonactivetext",
    "ui.-moz_mac_chrome_active",
    "ui.-moz_mac_chrome_inactive",
    "ui.-moz-mac-defaultbuttontext",
    "ui.-moz-mac-focusring",
    "ui.-moz-mac-menuselect",
    "ui.-moz-mac-menushadow",
    "ui.-moz-mac-menutextdisable",
    "ui.-moz-mac-menutextselect",
    "ui.-moz_mac_disabledtoolbartext",
    "ui.-moz-mac-secondaryhighlight",
    "ui.-moz-mac-vibrant-titlebar-light",
    "ui.-moz-mac-vibrant-titlebar-dark",
    "ui.-moz-mac-menupopup",
    "ui.-moz-mac-menuitem",
    "ui.-moz-mac-active-menuitem",
    "ui.-moz-mac-source-list",
    "ui.-moz-mac-source-list-selection",
    "ui.-moz-mac-active-source-list-selection",
    "ui.-moz-mac-tooltip",
    "ui.-moz-accent-color",
    "ui.-moz-accent-color-foreground",
    "ui.-moz-win-mediatext",
    "ui.-moz-win-communicationstext",
    "ui.-moz-nativehyperlinktext",
    "ui.-moz-nativevisitedhyperlinktext",
    "ui.-moz-hyperlinktext",
    "ui.-moz-activehyperlinktext",
    "ui.-moz-visitedhyperlinktext",
    "ui.-moz-comboboxtext",
    "ui.-moz-combobox",
    "ui.-moz-colheadertext",
    "ui.-moz-colheaderhovertext",
    "ui.-moz-gtk-titlebar-text",
    "ui.-moz-gtk-titlebar-inactive-text",
};

static_assert(ArrayLength(sColorPrefs) == size_t(LookAndFeel::ColorID::End),
              "Should have a pref for each color value");

const char* nsXPLookAndFeel::GetColorPrefName(ColorID aId) {
  return sColorPrefs[size_t(aId)];
}

bool nsXPLookAndFeel::sInitialized = false;

nsXPLookAndFeel* nsXPLookAndFeel::sInstance = nullptr;
bool nsXPLookAndFeel::sShutdown = false;

// static
nsXPLookAndFeel* nsXPLookAndFeel::GetInstance() {
  if (sInstance) {
    return sInstance;
  }

  NS_ENSURE_TRUE(!sShutdown, nullptr);

  // If we're in a content process, then the parent process will have supplied
  // us with an initial FullLookAndFeel object.
  // We grab this data from the ContentChild,
  // where it's been temporarily stashed, and initialize our new LookAndFeel
  // object with it.

  FullLookAndFeel* lnf = nullptr;

  if (auto* cc = mozilla::dom::ContentChild::GetSingleton()) {
    lnf = &cc->BorrowLookAndFeelData();
  }

  if (lnf) {
    sInstance = new widget::RemoteLookAndFeel(std::move(*lnf));
  } else if (gfxPlatform::IsHeadless()) {
    sInstance = new widget::HeadlessLookAndFeel();
  } else {
    sInstance = new nsLookAndFeel();
  }

  // This is only ever used once during initialization, and can be cleared now.
  if (lnf) {
    *lnf = {};
  }

  nsNativeBasicTheme::Init();
  return sInstance;
}

// static
void nsXPLookAndFeel::Shutdown() {
  if (sShutdown) {
    return;
  }

  sShutdown = true;
  delete sInstance;
  sInstance = nullptr;

  // This keeps strings alive, so need to clear to make leak checking happy.
  sFontCache.Clear();

  nsNativeBasicTheme::Shutdown();
}

static void IntPrefChanged() {
  // Int prefs can't change our system colors or fonts.
  LookAndFeel::NotifyChangedAllWindows(
      widget::ThemeChangeKind::MediaQueriesOnly);
}

static void FloatPrefChanged() {
  // Float prefs can't change our system colors or fonts.
  LookAndFeel::NotifyChangedAllWindows(
      widget::ThemeChangeKind::MediaQueriesOnly);
}

static void ColorPrefChanged() {
  // Color prefs affect style, because they by definition change system colors.
  LookAndFeel::NotifyChangedAllWindows(widget::ThemeChangeKind::Style);
}

// static
void nsXPLookAndFeel::OnPrefChanged(const char* aPref, void* aClosure) {
  nsDependentCString prefName(aPref);
  for (const char* pref : sIntPrefs) {
    if (prefName.Equals(pref)) {
      IntPrefChanged();
      return;
    }
  }

  for (const char* pref : sFloatPrefs) {
    if (prefName.Equals(pref)) {
      FloatPrefChanged();
      return;
    }
  }

  for (const char* pref : sColorPrefs) {
    if (prefName.Equals(pref)) {
      ColorPrefChanged();
      return;
    }
  }
}

static constexpr struct {
  nsLiteralCString mName;
  widget::ThemeChangeKind mChangeKind =
      widget::ThemeChangeKind::MediaQueriesOnly;
} kMediaQueryPrefs[] = {
    {"browser.display.windows.native_menus"_ns},
    {"browser.proton.enabled"_ns},
    {"browser.proton.places-tooltip.enabled"_ns},
    // This affects not only the media query, but also the native theme, so we
    // need to re-layout.
    {"browser.theme.toolbar-theme"_ns, widget::ThemeChangeKind::AllBits},
};

// Read values from the user's preferences.
// This is done once at startup, but since the user's preferences
// haven't actually been read yet at that time, we also have to
// set a callback to inform us of changes to each pref.
void nsXPLookAndFeel::Init() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // Say we're already initialized, and take the chance that it might fail;
  // protects against some other process writing to our static variables.
  sInitialized = true;

  // XXX If we could reorganize the pref names, we should separate the branch
  //     for each types.  Then, we could reduce the unnecessary loop from
  //     nsXPLookAndFeel::OnPrefChanged().
  Preferences::RegisterPrefixCallback(OnPrefChanged, "ui.");
  // We really do just want the accessibility.tabfocus pref, not other prefs
  // that start with that string.
  Preferences::RegisterCallback(OnPrefChanged, "accessibility.tabfocus");

  for (auto& pref : kMediaQueryPrefs) {
    Preferences::RegisterCallback(
        [](const char*, void* aChangeKind) {
          auto changeKind =
              widget::ThemeChangeKind(reinterpret_cast<uintptr_t>(aChangeKind));
          LookAndFeel::NotifyChangedAllWindows(changeKind);
        },
        pref.mName, reinterpret_cast<void*>(uintptr_t(pref.mChangeKind)));
  }
}

nsXPLookAndFeel::~nsXPLookAndFeel() {
  NS_ASSERTION(sInstance == this,
               "This destroying instance isn't the singleton instance");
  sInstance = nullptr;
}

static bool IsSpecialColor(LookAndFeel::ColorID aID, nscolor aColor) {
  using ColorID = LookAndFeel::ColorID;

  switch (aID) {
    case ColorID::TextSelectForeground:
    case ColorID::IMESelectedRawTextBackground:
    case ColorID::IMESelectedConvertedTextBackground:
    case ColorID::IMERawInputBackground:
    case ColorID::IMEConvertedTextBackground:
    case ColorID::IMESelectedRawTextForeground:
    case ColorID::IMESelectedConvertedTextForeground:
    case ColorID::IMERawInputForeground:
    case ColorID::IMEConvertedTextForeground:
    case ColorID::IMERawInputUnderline:
    case ColorID::IMEConvertedTextUnderline:
    case ColorID::IMESelectedRawTextUnderline:
    case ColorID::IMESelectedConvertedTextUnderline:
    case ColorID::SpellCheckerUnderline:
      return NS_IS_SELECTION_SPECIAL_COLOR(aColor);
    default:
      break;
  }
  /*
   * In GetColor(), every color that is not a special color is color
   * corrected. Use false to make other colors color corrected.
   */
  return false;
}

nscolor nsXPLookAndFeel::GetStandinForNativeColor(ColorID aID) {
  // The stand-in colors are taken from the Windows 7 Aero theme
  // except Mac-specific colors which are taken from Mac OS 10.7.

#define COLOR(name_, r, g, b) \
  case ColorID::name_:        \
    return NS_RGB(r, g, b);

  switch (aID) {
    // CSS 2 colors:
    COLOR(Activeborder, 0xB4, 0xB4, 0xB4)
    COLOR(Activecaption, 0x99, 0xB4, 0xD1)
    COLOR(Appworkspace, 0xAB, 0xAB, 0xAB)
    COLOR(Background, 0x00, 0x00, 0x00)
    COLOR(Buttonface, 0xF0, 0xF0, 0xF0)
    COLOR(Buttonhighlight, 0xFF, 0xFF, 0xFF)
    COLOR(Buttonshadow, 0xA0, 0xA0, 0xA0)
    COLOR(Buttontext, 0x00, 0x00, 0x00)
    COLOR(Captiontext, 0x00, 0x00, 0x00)
    COLOR(Graytext, 0x6D, 0x6D, 0x6D)
    COLOR(Highlight, 0x33, 0x99, 0xFF)
    COLOR(Highlighttext, 0xFF, 0xFF, 0xFF)
    COLOR(Inactiveborder, 0xF4, 0xF7, 0xFC)
    COLOR(Inactivecaption, 0xBF, 0xCD, 0xDB)
    COLOR(Inactivecaptiontext, 0x43, 0x4E, 0x54)
    COLOR(Infobackground, 0xFF, 0xFF, 0xE1)
    COLOR(Infotext, 0x00, 0x00, 0x00)
    COLOR(Menu, 0xF0, 0xF0, 0xF0)
    COLOR(Menutext, 0x00, 0x00, 0x00)
    COLOR(Scrollbar, 0xC8, 0xC8, 0xC8)
    COLOR(Threeddarkshadow, 0x69, 0x69, 0x69)
    COLOR(Threedface, 0xF0, 0xF0, 0xF0)
    COLOR(Threedhighlight, 0xFF, 0xFF, 0xFF)
    COLOR(Threedlightshadow, 0xE3, 0xE3, 0xE3)
    COLOR(Threedshadow, 0xA0, 0xA0, 0xA0)
    COLOR(Window, 0xFF, 0xFF, 0xFF)
    COLOR(Windowframe, 0x64, 0x64, 0x64)
    COLOR(Windowtext, 0x00, 0x00, 0x00)
    COLOR(MozButtondefault, 0x69, 0x69, 0x69)
    COLOR(Field, 0xFF, 0xFF, 0xFF)
    COLOR(Fieldtext, 0x00, 0x00, 0x00)
    COLOR(MozDialog, 0xF0, 0xF0, 0xF0)
    COLOR(MozDialogtext, 0x00, 0x00, 0x00)
    COLOR(MozColheadertext, 0x00, 0x00, 0x00)
    COLOR(MozColheaderhovertext, 0x00, 0x00, 0x00)
    COLOR(MozDragtargetzone, 0xFF, 0xFF, 0xFF)
    COLOR(MozCellhighlight, 0xF0, 0xF0, 0xF0)
    COLOR(MozCellhighlighttext, 0x00, 0x00, 0x00)
    COLOR(MozHtmlCellhighlight, 0x33, 0x99, 0xFF)
    COLOR(MozHtmlCellhighlighttext, 0xFF, 0xFF, 0xFF)
    COLOR(MozButtonhoverface, 0xF0, 0xF0, 0xF0)
    COLOR(MozGtkButtonactivetext, 0x00, 0x00, 0x00)
    COLOR(MozButtonhovertext, 0x00, 0x00, 0x00)
    COLOR(MozMenuhover, 0x33, 0x99, 0xFF)
    COLOR(MozMenuhovertext, 0x00, 0x00, 0x00)
    COLOR(MozMenubartext, 0x00, 0x00, 0x00)
    COLOR(MozMenubarhovertext, 0x00, 0x00, 0x00)
    COLOR(MozOddtreerow, 0xFF, 0xFF, 0xFF)
    COLOR(MozMacChromeActive, 0xB2, 0xB2, 0xB2)
    COLOR(MozMacChromeInactive, 0xE1, 0xE1, 0xE1)
    COLOR(MozMacFocusring, 0x60, 0x9D, 0xD7)
    COLOR(MozMacMenuselect, 0x38, 0x75, 0xD7)
    COLOR(MozMacMenushadow, 0xA3, 0xA3, 0xA3)
    COLOR(MozMacMenutextdisable, 0x88, 0x88, 0x88)
    COLOR(MozMacMenutextselect, 0xFF, 0xFF, 0xFF)
    COLOR(MozMacDisabledtoolbartext, 0x3F, 0x3F, 0x3F)
    COLOR(MozMacSecondaryhighlight, 0xD4, 0xD4, 0xD4)
    COLOR(MozMacVibrantTitlebarLight, 0xf7, 0xf7, 0xf7)
    COLOR(MozMacVibrantTitlebarDark, 0x28, 0x28, 0x28)
    COLOR(MozMacMenupopup, 0xe6, 0xe6, 0xe6)
    COLOR(MozMacMenuitem, 0xe6, 0xe6, 0xe6)
    COLOR(MozMacActiveMenuitem, 0x0a, 0x64, 0xdc)
    COLOR(MozMacSourceList, 0xf7, 0xf7, 0xf7)
    COLOR(MozMacSourceListSelection, 0xc8, 0xc8, 0xc8)
    COLOR(MozMacActiveSourceListSelection, 0x0a, 0x64, 0xdc)
    COLOR(MozMacTooltip, 0xf7, 0xf7, 0xf7)
    // Seems to be the default color (hardcoded because of bug 1065998)
    COLOR(MozWinMediatext, 0xFF, 0xFF, 0xFF)
    COLOR(MozWinCommunicationstext, 0xFF, 0xFF, 0xFF)
    COLOR(MozNativehyperlinktext, 0x00, 0x66, 0xCC)
    COLOR(MozNativevisitedhyperlinktext, 0x55, 0x1A, 0x8B)
    COLOR(MozComboboxtext, 0x00, 0x00, 0x00)
    COLOR(MozCombobox, 0xFF, 0xFF, 0xFF)
    default:
      break;
  }
  return NS_RGB(0xFF, 0xFF, 0xFF);
}

// Uncomment the #define below if you want to debug system color use in a skin
// that uses them.  When set, it will make all system color pairs that are
// appropriate for foreground/background pairing the same.  This means if the
// skin is using system colors correctly you will not be able to see *any* text.
//
// #define DEBUG_SYSTEM_COLOR_USE

#ifdef DEBUG_SYSTEM_COLOR_USE
static nsresult SystemColorUseDebuggingColor(LookAndFeel::ColorID aID,
                                             nscolor& aResult) {
  using ColorID = LookAndFeel::ColorID;

  switch (aID) {
      // css2  http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
    case ColorID::Activecaption:
      // active window caption background
    case ColorID::Captiontext:
      // text in active window caption
      aResult = NS_RGB(0xff, 0x00, 0x00);
      break;

    case ColorID::Highlight:
      // background of selected item
    case ColorID::Highlighttext:
      // text of selected item
      aResult = NS_RGB(0xff, 0xff, 0x00);
      break;

    case ColorID::Inactivecaption:
      // inactive window caption
    case ColorID::Inactivecaptiontext:
      // text in inactive window caption
      aResult = NS_RGB(0x66, 0x66, 0x00);
      break;

    case ColorID::Infobackground:
      // tooltip background color
    case ColorID::Infotext:
      // tooltip text color
      aResult = NS_RGB(0x00, 0xff, 0x00);
      break;

    case ColorID::Menu:
      // menu background
    case ColorID::Menutext:
      // menu text
      aResult = NS_RGB(0x00, 0xff, 0xff);
      break;

    case ColorID::Threedface:
    case ColorID::Buttonface:
      // 3-D face color
    case ColorID::Buttontext:
      // text on push buttons
      aResult = NS_RGB(0x00, 0x66, 0x66);
      break;

    case ColorID::Window:
    case ColorID::Windowtext:
      aResult = NS_RGB(0x00, 0x00, 0xff);
      break;

      // from the CSS3 working draft (not yet finalized)
      // http://www.w3.org/tr/2000/wd-css3-userint-20000216.html#color

    case ColorID::Field:
    case ColorID::Fieldtext:
      aResult = NS_RGB(0xff, 0x00, 0xff);
      break;

    case ColorID::MozDialog:
    case ColorID::MozDialogtext:
      aResult = NS_RGB(0x66, 0x00, 0x66);
      break;

    default:
      return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}
#endif

static nsresult GetColorFromPref(LookAndFeel::ColorID aID, nscolor& aResult) {
  const char* prefName = sColorPrefs[size_t(aID)];
  nsAutoCString colorStr;
  MOZ_TRY(Preferences::GetCString(prefName, colorStr));
  if (!ServoCSSParser::ComputeColor(nullptr, NS_RGB(0, 0, 0), colorStr,
                                    &aResult)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// All these routines will return NS_OK if they have a value,
// in which case the nsLookAndFeel should use that value;
// otherwise we'll return NS_ERROR_NOT_AVAILABLE, in which case, the
// platform-specific nsLookAndFeel should use its own values instead.
nsresult nsXPLookAndFeel::GetColorValue(ColorID aID, ColorScheme aScheme,
                                        UseStandins aUseStandins,
                                        nscolor& aResult) {
  if (!sInitialized) {
    Init();
  }

#ifdef DEBUG_SYSTEM_COLOR_USE
  if (NS_SUCCEEDED(SystemColorUseDebuggingColor(aID, aResult))) {
    return NS_OK;
  }
#endif

  if (aUseStandins == UseStandins::Yes) {
    aResult = GetStandinForNativeColor(aID);
    return NS_OK;
  }

  auto& cache =
      aScheme == ColorScheme::Light ? sLightColorCache : sDarkColorCache;
  if (const auto* cached = cache.Get(aID)) {
    if (cached->isNothing()) {
      return NS_ERROR_FAILURE;
    }
    aResult = cached->value();
    return NS_OK;
  }

  if (NS_SUCCEEDED(GetColorFromPref(aID, aResult))) {
    cache.Insert(aID, Some(aResult));
    return NS_OK;
  }

  if (NS_SUCCEEDED(NativeGetColor(aID, aScheme, aResult))) {
    if (gfxPlatform::GetCMSMode() == CMSMode::All &&
        !IsSpecialColor(aID, aResult)) {
      qcms_transform* transform = gfxPlatform::GetCMSInverseRGBTransform();
      if (transform) {
        uint8_t color[4];
        color[0] = NS_GET_R(aResult);
        color[1] = NS_GET_G(aResult);
        color[2] = NS_GET_B(aResult);
        color[3] = NS_GET_A(aResult);
        qcms_transform_data(transform, color, color, 1);
        aResult = NS_RGBA(color[0], color[1], color[2], color[3]);
      }
    }

    // NOTE: Servo holds a lock and the main thread is paused, so writing to the
    // global cache here is fine.
    cache.Insert(aID, Some(aResult));
    return NS_OK;
  }

  cache.Insert(aID, Nothing());
  return NS_ERROR_FAILURE;
}

nsresult nsXPLookAndFeel::GetIntValue(IntID aID, int32_t& aResult) {
  if (!sInitialized) {
    Init();
  }

  if (const auto* cached = sIntCache.Get(aID)) {
    if (cached->isNothing()) {
      return NS_ERROR_FAILURE;
    }
    aResult = cached->value();
    return NS_OK;
  }

  if (NS_SUCCEEDED(Preferences::GetInt(sIntPrefs[size_t(aID)], &aResult))) {
    sIntCache.Insert(aID, Some(aResult));
    return NS_OK;
  }

  if (NS_FAILED(NativeGetInt(aID, aResult))) {
    sIntCache.Insert(aID, Nothing());
    return NS_ERROR_FAILURE;
  }

  sIntCache.Insert(aID, Some(aResult));
  return NS_OK;
}

nsresult nsXPLookAndFeel::GetFloatValue(FloatID aID, float& aResult) {
  if (!sInitialized) {
    Init();
  }

  if (const auto* cached = sFloatCache.Get(aID)) {
    if (cached->isNothing()) {
      return NS_ERROR_FAILURE;
    }
    aResult = cached->value();
    return NS_OK;
  }

  int32_t pref = 0;
  if (NS_SUCCEEDED(Preferences::GetInt(sFloatPrefs[size_t(aID)], &pref))) {
    aResult = float(pref) / 100.0f;
    sFloatCache.Insert(aID, Some(aResult));
    return NS_OK;
  }

  if (NS_FAILED(NativeGetFloat(aID, aResult))) {
    sFloatCache.Insert(aID, Nothing());
    return NS_ERROR_FAILURE;
  }

  sFloatCache.Insert(aID, Some(aResult));
  return NS_OK;
}

bool nsXPLookAndFeel::LookAndFeelFontToStyle(const LookAndFeelFont& aFont,
                                             nsString& aName,
                                             gfxFontStyle& aStyle) {
  if (!aFont.haveFont()) {
    return false;
  }
  aName = aFont.name();
  aStyle = gfxFontStyle();
  aStyle.size = aFont.size();
  aStyle.weight = FontWeight(aFont.weight());
  aStyle.style =
      aFont.italic() ? FontSlantStyle::Italic() : FontSlantStyle::Normal();
  aStyle.systemFont = true;
  return true;
}

widget::LookAndFeelFont nsXPLookAndFeel::StyleToLookAndFeelFont(
    const nsAString& aName, const gfxFontStyle& aStyle) {
  LookAndFeelFont font;
  font.haveFont() = true;
  font.name() = aName;
  font.size() = aStyle.size;
  font.weight() = aStyle.weight.ToFloat();
  font.italic() = aStyle.style.IsItalic();
  MOZ_ASSERT(aStyle.style.IsNormal() || aStyle.style.IsItalic(),
             "Cannot handle oblique font style");
#ifdef DEBUG
  {
    // Assert that all the remaining font style properties have their
    // default values.
    gfxFontStyle candidate = aStyle;
    gfxFontStyle defaults{};
    candidate.size = defaults.size;
    candidate.weight = defaults.weight;
    candidate.style = defaults.style;
    MOZ_ASSERT(candidate.Equals(defaults),
               "Some font style properties not supported");
  }
#endif
  return font;
}

bool nsXPLookAndFeel::GetFontValue(FontID aID, nsString& aName,
                                   gfxFontStyle& aStyle) {
  if (const LookAndFeelFont* cached = sFontCache.Get(aID)) {
    return LookAndFeelFontToStyle(*cached, aName, aStyle);
  }
  LookAndFeelFont font;
  const bool haveFont = NativeGetFont(aID, aName, aStyle);
  font.haveFont() = haveFont;
  if (haveFont) {
    font = StyleToLookAndFeelFont(aName, aStyle);
  }
  sFontCache.Insert(aID, std::move(font));
  return haveFont;
}

void nsXPLookAndFeel::RefreshImpl() {
  // Wipe out our caches.
  sLightColorCache.Clear();
  sDarkColorCache.Clear();
  sFontCache.Clear();
  sFloatCache.Clear();
  sIntCache.Clear();

  // Clear any cached FullLookAndFeel data, which is now invalid.
  if (XRE_IsParentProcess()) {
    widget::RemoteLookAndFeel::ClearCachedData();
  }
}

static bool sRecordedLookAndFeelTelemetry = false;

void nsXPLookAndFeel::RecordTelemetry() {
  if (!XRE_IsParentProcess()) {
    return;
  }

  if (sRecordedLookAndFeelTelemetry) {
    return;
  }

  sRecordedLookAndFeelTelemetry = true;

  int32_t i;
  Telemetry::ScalarSet(
      Telemetry::ScalarID::WIDGET_DARK_MODE,
      NS_SUCCEEDED(GetIntValue(IntID::SystemUsesDarkTheme, i)) && i != 0);

  RecordLookAndFeelSpecificTelemetry();
}

namespace mozilla {

// static
void LookAndFeel::NotifyChangedAllWindows(widget::ThemeChangeKind aKind) {
  if (nsCOMPtr<nsIObserverService> obs = services::GetObserverService()) {
    const char16_t kind[] = {char16_t(aKind), 0};
    obs->NotifyObservers(nullptr, "look-and-feel-changed", kind);
  }
}

static bool ShouldUseStandinsForNativeColorForNonNativeTheme(
    const dom::Document& aDoc, LookAndFeel::ColorID aColor) {
  using ColorID = LookAndFeel::ColorID;
  if (!aDoc.ShouldAvoidNativeTheme()) {
    return false;
  }

  // The native theme doesn't use system colors backgrounds etc, except when in
  // high-contrast mode, so spoof some of the colors with stand-ins to prevent
  // lack of contrast.
  switch (aColor) {
    case ColorID::Buttonface:
    case ColorID::Buttontext:
    case ColorID::MozButtonhoverface:
    case ColorID::MozButtonhovertext:
    case ColorID::MozGtkButtonactivetext:

    case ColorID::MozCombobox:
    case ColorID::MozComboboxtext:

    case ColorID::Field:
    case ColorID::Fieldtext:

    case ColorID::Graytext:

      return !PreferenceSheet::PrefsFor(aDoc)
                  .NonNativeThemeShouldUseSystemColors();

    default:
      break;
  }

  return false;
}

static bool ShouldRespectSystemColorSchemeForChromeDoc() {
#ifdef XP_MACOSX
  // macOS follows the global toolbar theme, not the system theme.
  // (If the global toolbar theme is set to System, then it *that* follows the
  // system theme.)
  return false;
#else
  // GTK historically has behaved like this. Other platforms don't have support
  // for light / dark color schemes yet so it doesn't matter for them.
  return true;
#endif
}

static bool ShouldRespectGlobalToolbarThemeAppearanceForChromeDoc() {
#ifdef XP_MACOSX
  // Need to be consistent with AppearanceOverride.mm on macOS, which respects
  // the browser.theme.toolbar-theme pref.
  // However, if widget.macos.support-dark-appearance is false, we need to
  // pretend everything's Light and not follow the toolbar theme.
  return StaticPrefs::widget_macos_support_dark_appearance();
#elif defined(MOZ_WIDGET_GTK)
  return StaticPrefs::widget_gtk_follow_firefox_theme();
#else
  return false;
#endif
}

static LookAndFeel::ColorScheme ColorSchemeForDocument(
    const dom::Document& aDoc, bool aContentSupportsDark) {
  using ColorScheme = LookAndFeel::ColorScheme;
  if (nsContentUtils::IsChromeDoc(&aDoc)) {
    if (ShouldRespectGlobalToolbarThemeAppearanceForChromeDoc()) {
      switch (StaticPrefs::browser_theme_toolbar_theme()) {
        case 0:  // Dark
          return ColorScheme::Dark;
        case 1:  // Light
          return ColorScheme::Light;
        case 2:  // System
          return LookAndFeel::SystemColorScheme();
        default:
          break;
      }
    }
    if (ShouldRespectSystemColorSchemeForChromeDoc()) {
      return LookAndFeel::SystemColorScheme();
    }
  }
#ifdef MOZ_WIDGET_GTK
  if (StaticPrefs::widget_content_allow_gtk_dark_theme()) {
    // If users manually tweak allow-gtk-dark-theme, allow content to use the
    // system color scheme rather than forcing it to light.
    return LookAndFeel::SystemColorScheme();
  }
#endif
  return aContentSupportsDark ? LookAndFeel::SystemColorScheme()
                              : ColorScheme::Light;
}

LookAndFeel::ColorScheme LookAndFeel::ColorSchemeForStyle(
    const dom::Document& aDoc, const StyleColorSchemeFlags& aFlags) {
  StyleColorSchemeFlags style(aFlags);
  if (!style) {
    style.bits = aDoc.GetColorSchemeBits();
  }
  const bool supportsDark = bool(style & StyleColorSchemeFlags::DARK);
  const bool supportsLight = bool(style & StyleColorSchemeFlags::LIGHT);
  if (supportsDark && !supportsLight) {
    return ColorScheme::Dark;
  }
  if (supportsLight && !supportsDark) {
    return ColorScheme::Light;
  }
  return ColorSchemeForDocument(aDoc, supportsDark);
}

LookAndFeel::ColorScheme LookAndFeel::ColorSchemeForFrame(
    const nsIFrame* aFrame) {
  return ColorSchemeForStyle(*aFrame->PresContext()->Document(),
                             aFrame->StyleUI()->mColorScheme.bits);
}

// static
Maybe<nscolor> LookAndFeel::GetColor(ColorID aId, ColorScheme aScheme,
                                     UseStandins aUseStandins) {
  nscolor result;
  nsresult rv = nsLookAndFeel::GetInstance()->GetColorValue(
      aId, aScheme, aUseStandins, result);
  if (NS_FAILED(rv)) {
    return Nothing();
  }
  return Some(result);
}

// Returns whether there is a CSS color name for this color.
static bool ColorIsCSSAccessible(LookAndFeel::ColorID aId) {
  using ColorID = LookAndFeel::ColorID;

  switch (aId) {
    case ColorID::WindowBackground:
    case ColorID::WindowForeground:
    case ColorID::WidgetBackground:
    case ColorID::WidgetForeground:
    case ColorID::WidgetSelectBackground:
    case ColorID::WidgetSelectForeground:
    case ColorID::Widget3DHighlight:
    case ColorID::Widget3DShadow:
    case ColorID::TextBackground:
    case ColorID::TextForeground:
    case ColorID::TextSelectBackground:
    case ColorID::TextSelectForeground:
    case ColorID::TextSelectBackgroundDisabled:
    case ColorID::TextSelectBackgroundAttention:
    case ColorID::TextHighlightBackground:
    case ColorID::TextHighlightForeground:
    case ColorID::IMERawInputBackground:
    case ColorID::IMERawInputForeground:
    case ColorID::IMERawInputUnderline:
    case ColorID::IMESelectedRawTextBackground:
    case ColorID::IMESelectedRawTextForeground:
    case ColorID::IMESelectedRawTextUnderline:
    case ColorID::IMEConvertedTextBackground:
    case ColorID::IMEConvertedTextForeground:
    case ColorID::IMEConvertedTextUnderline:
    case ColorID::IMESelectedConvertedTextBackground:
    case ColorID::IMESelectedConvertedTextForeground:
    case ColorID::IMESelectedConvertedTextUnderline:
    case ColorID::SpellCheckerUnderline:
      return false;
    default:
      break;
  }

  return true;
}

LookAndFeel::UseStandins LookAndFeel::ShouldUseStandins(
    const dom::Document& aDoc, ColorID aId) {
  if (ShouldUseStandinsForNativeColorForNonNativeTheme(aDoc, aId)) {
    return UseStandins::Yes;
  }
  if (nsContentUtils::UseStandinsForNativeColors() &&
      !nsContentUtils::IsChromeDoc(&aDoc) && ColorIsCSSAccessible(aId)) {
    return UseStandins::Yes;
  }
  if (aDoc.IsStaticDocument() &&
      !PreferenceSheet::ContentPrefs().mUseDocumentColors) {
    return UseStandins::Yes;
  }
  return UseStandins::No;
}

Maybe<nscolor> LookAndFeel::GetColor(ColorID aId, const nsIFrame* aFrame) {
  const auto* doc = aFrame->PresContext()->Document();
  return GetColor(aId, ColorSchemeForFrame(aFrame),
                  ShouldUseStandins(*doc, aId));
}

// static
nsresult LookAndFeel::GetInt(IntID aID, int32_t* aResult) {
  return nsLookAndFeel::GetInstance()->GetIntValue(aID, *aResult);
}

// static
nsresult LookAndFeel::GetFloat(FloatID aID, float* aResult) {
  return nsLookAndFeel::GetInstance()->GetFloatValue(aID, *aResult);
}

// static
bool LookAndFeel::GetFont(FontID aID, nsString& aName, gfxFontStyle& aStyle) {
  return nsLookAndFeel::GetInstance()->GetFontValue(aID, aName, aStyle);
}

// static
char16_t LookAndFeel::GetPasswordCharacter() {
  return nsLookAndFeel::GetInstance()->GetPasswordCharacterImpl();
}

// static
bool LookAndFeel::GetEchoPassword() {
  if (StaticPrefs::editor_password_mask_delay() >= 0) {
    return StaticPrefs::editor_password_mask_delay() > 0;
  }
  return nsLookAndFeel::GetInstance()->GetEchoPasswordImpl();
}

// static
uint32_t LookAndFeel::GetPasswordMaskDelay() {
  int32_t delay = StaticPrefs::editor_password_mask_delay();
  if (delay < 0) {
    return nsLookAndFeel::GetInstance()->GetPasswordMaskDelayImpl();
  }
  return delay;
}

void LookAndFeel::GetThemeInfo(nsACString& aOut) {
  nsLookAndFeel::GetInstance()->GetThemeInfo(aOut);
}

// static
void LookAndFeel::Refresh() {
  nsLookAndFeel::GetInstance()->RefreshImpl();
  nsNativeBasicTheme::LookAndFeelChanged();
}

// static
void LookAndFeel::NativeInit() { nsLookAndFeel::GetInstance()->NativeInit(); }

// static
void LookAndFeel::SetData(widget::FullLookAndFeel&& aTables) {
  nsLookAndFeel::GetInstance()->SetDataImpl(std::move(aTables));
}

}  // namespace mozilla
