/* -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "mozilla/LookAndFeel.h"
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
#include "nsLayoutUtils.h"
#include "Theme.h"
#include "SurfaceCacheUtils.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/ServoCSSParser.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_editor.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/dom/Document.h"
#include "mozilla/PreferenceSheet.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/widget/WidgetMessageUtils.h"
#include "mozilla/dom/KeyboardEventBinding.h"
#include "mozilla/RelativeLuminanceUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryScalarEnums.h"
#include "mozilla/Try.h"

#include "gfxPlatform.h"
#include "gfxFont.h"

#include "qcms.h"

#include <bitset>

using namespace mozilla;

using IntID = mozilla::LookAndFeel::IntID;
using FloatID = mozilla::LookAndFeel::FloatID;
using ColorID = mozilla::LookAndFeel::ColorID;
using FontID = mozilla::LookAndFeel::FontID;

template <typename Index, typename Value, Index kEnd>
class EnumeratedCache {
  mozilla::EnumeratedArray<Index, kEnd, Value> mEntries;
  std::bitset<size_t(kEnd)> mValidity;

 public:
  constexpr EnumeratedCache() = default;

  bool IsValid(Index aIndex) const { return mValidity[size_t(aIndex)]; }

  const Value* Get(Index aIndex) const {
    return IsValid(aIndex) ? &mEntries[aIndex] : nullptr;
  }

  void Insert(Index aIndex, Value aValue) {
    mValidity[size_t(aIndex)] = true;
    mEntries[aIndex] = aValue;
  }

  void Remove(Index aIndex) {
    mValidity[size_t(aIndex)] = false;
    mEntries[aIndex] = Value();
  }

  void Clear() {
    mValidity.reset();
    for (auto& entry : mEntries) {
      entry = Value();
    }
  }
};

using ColorCache = EnumeratedCache<ColorID, Maybe<nscolor>, ColorID::End>;

struct ColorCaches {
  using UseStandins = LookAndFeel::UseStandins;

  ColorCache mCaches[2][2];

  constexpr ColorCaches() = default;

  ColorCache& Get(ColorScheme aScheme, UseStandins aUseStandins) {
    return mCaches[aScheme == ColorScheme::Dark]
                  [aUseStandins == UseStandins::Yes];
  }

  void Clear() {
    for (auto& c : mCaches) {
      for (auto& cache : c) {
        cache.Clear();
      }
    }
  }
};

static ColorCaches sColorCaches;

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
static const char sIntPrefs[][45] = {
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
    "ui.macBigSurTheme",
    "ui.macRTL",
    "ui.alertNotificationOrigin",
    "ui.scrollToClick",
    "ui.IMERawInputUnderlineStyle",
    "ui.IMESelectedRawTextUnderlineStyle",
    "ui.IMEConvertedTextUnderlineStyle",
    "ui.IMESelectedConvertedTextUnderlineStyle",
    "ui.SpellCheckerUnderlineStyle",
    "ui.menuBarDrag",
    "ui.scrollbarButtonAutoRepeatBehavior",
    "ui.tooltipDelay",
    "ui.swipeAnimationEnabled",
    "ui.scrollbarDisplayOnMouseMove",
    "ui.scrollbarFadeBeginDelay",
    "ui.scrollbarFadeDuration",
    "ui.contextMenuOffsetVertical",
    "ui.contextMenuOffsetHorizontal",
    "ui.GtkCSDAvailable",
    "ui.GtkCSDMinimizeButton",
    "ui.GtkCSDMaximizeButton",
    "ui.GtkCSDCloseButton",
    "ui.GtkCSDMinimizeButtonPosition",
    "ui.GtkCSDMaximizeButtonPosition",
    "ui.GtkCSDCloseButtonPosition",
    "ui.GtkCSDReversedPlacement",
    "ui.systemUsesDarkTheme",
    "ui.prefersReducedMotion",
    "ui.prefersReducedTransparency",
    "ui.invertedColors",
    "ui.primaryPointerCapabilities",
    "ui.allPointerCapabilities",
    "ui.systemScrollbarSize",
    "ui.touchDeviceSupportPresent",
    "ui.titlebarRadius",
    "ui.dynamicRange",
    "ui.videoDynamicRange",
    "ui.panelAnimations",
    "ui.hideCursorWhileTyping",
    "ui.gtkThemeFamily",
};

static_assert(ArrayLength(sIntPrefs) == size_t(LookAndFeel::IntID::End),
              "Should have a pref for each int value");

// This array MUST be kept in the same order as the float id list in
// LookAndFeel.h
// clang-format off
static const char sFloatPrefs[][37] = {
    "ui.IMEUnderlineRelativeSize",
    "ui.SpellCheckerUnderlineRelativeSize",
    "ui.caretAspectRatio",
    "ui.textScaleFactor",
    "ui.cursorScale",
};
// clang-format on

static_assert(ArrayLength(sFloatPrefs) == size_t(LookAndFeel::FloatID::End),
              "Should have a pref for each float value");

// This array MUST be kept in the same order as the color list in
// specified/color.rs
static const char sColorPrefs[][41] = {
    "ui.activeborder",
    "ui.activecaption",
    "ui.appworkspace",
    "ui.background",
    "ui.buttonface",
    "ui.buttonhighlight",
    "ui.buttonshadow",
    "ui.buttontext",
    "ui.buttonborder",
    "ui.captiontext",
    "ui.-moz-field",
    "ui.-moz-disabledfield",
    "ui.-moz-fieldtext",
    "ui.mark",
    "ui.marktext",
    "ui.-moz-comboboxtext",
    "ui.-moz-combobox",
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
    "ui.-moz-default-color",
    "ui.-moz-default-background-color",
    "ui.-moz-dialog",
    "ui.-moz-dialogtext",
    "ui.-moz-cellhighlight",
    "ui.-moz_cellhighlighttext",
    "ui.selecteditem",
    "ui.selecteditemtext",
    "ui.-moz-buttonhoverface",
    "ui.-moz_buttonhovertext",
    "ui.-moz_menuhover",
    "ui.-moz_menuhoverdisabled",
    "ui.-moz_menuhovertext",
    "ui.-moz_menubarhovertext",
    "ui.-moz_eventreerow",
    "ui.-moz_oddtreerow",
    "ui.-moz-buttonactivetext",
    "ui.-moz-buttonactiveface",
    "ui.-moz-buttondisabledface",
    "ui.-moz-headerbar",
    "ui.-moz-headerbartext",
    "ui.-moz-headerbarinactive",
    "ui.-moz-headerbarinactivetext",
    "ui.-moz-mac-defaultbuttontext",
    "ui.-moz-mac-focusring",
    "ui.-moz_mac_disabledtoolbartext",
    "ui.-moz-sidebar",
    "ui.-moz-sidebartext",
    "ui.-moz-sidebarborder",
    "ui.accentcolor",
    "ui.accentcolortext",
    "ui.-moz-autofill-background",
    "ui.-moz-nativehyperlinktext",
    "ui.-moz-nativevisitedhyperlinktext",
    "ui.-moz-hyperlinktext",
    "ui.-moz-activehyperlinktext",
    "ui.-moz-visitedhyperlinktext",
    "ui.-moz-colheader",
    "ui.-moz-colheadertext",
    "ui.-moz-colheaderhover",
    "ui.-moz-colheaderhovertext",
    "ui.-moz-colheaderactive",
    "ui.-moz-colheaderactivetext",
    "ui.textSelectDisabledBackground",
    "ui.textSelectAttentionBackground",
    "ui.textSelectAttentionForeground",
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
};

static_assert(ArrayLength(sColorPrefs) == size_t(LookAndFeel::ColorID::End),
              "Should have a pref for each color value");

// This array MUST be kept in the same order as the SystemFont enum.
static const char sFontPrefs[][41] = {
    "ui.font.caption",
    "ui.font.icon",
    "ui.font.menu",
    "ui.font.message-box",
    "ui.font.small-caption",
    "ui.font.status-bar",
    "ui.font.-moz-pull-down-menu",
    "ui.font.-moz-button",
    "ui.font.-moz-list",
    "ui.font.-moz-field",
};

static_assert(ArrayLength(sFontPrefs) == size_t(LookAndFeel::FontID::End),
              "Should have a pref for each font value");

const char* nsXPLookAndFeel::GetColorPrefName(ColorID aId) {
  return sColorPrefs[size_t(aId)];
}

bool nsXPLookAndFeel::sInitialized = false;

nsXPLookAndFeel* nsXPLookAndFeel::sInstance = nullptr;
bool nsXPLookAndFeel::sShutdown = false;

auto LookAndFeel::SystemZoomSettings() -> ZoomSettings {
  ZoomSettings settings;
  switch (StaticPrefs::browser_display_os_zoom_behavior()) {
    case 0:
    default:
      break;
    case 1:
      settings.mFullZoom = GetTextScaleFactor();
      break;
    case 2:
      settings.mTextZoom = GetTextScaleFactor();
      break;
  }
  return settings;
}

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

  widget::Theme::Init();
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

  widget::Theme::Shutdown();
}

static void IntPrefChanged(const nsACString& aPref) {
  // Most Int prefs can't change our system colors or fonts, but
  // ui.systemUsesDarkTheme can, since it affects the effective color-scheme
  // (affecting system colors).
  auto changeKind = aPref.EqualsLiteral("ui.systemUsesDarkTheme")
                        ? widget::ThemeChangeKind::Style
                        : widget::ThemeChangeKind::MediaQueriesOnly;
  LookAndFeel::NotifyChangedAllWindows(changeKind);
}

static void FloatPrefChanged(const nsACString& aPref) {
  // Most float prefs can't change our system colors or fonts, but
  // textScaleFactor affects layout.
  auto changeKind = aPref.EqualsLiteral("ui.textScaleFactor")
                        ? widget::ThemeChangeKind::StyleAndLayout
                        : widget::ThemeChangeKind::MediaQueriesOnly;
  LookAndFeel::NotifyChangedAllWindows(changeKind);
}

static void ColorPrefChanged() {
  // Color prefs affect style, because they by definition change system colors.
  LookAndFeel::NotifyChangedAllWindows(widget::ThemeChangeKind::Style);
}

static void FontPrefChanged() {
  // Color prefs affect style, because they by definition change system fonts.
  LookAndFeel::NotifyChangedAllWindows(widget::ThemeChangeKind::Style);
}

// static
void nsXPLookAndFeel::OnPrefChanged(const char* aPref, void* aClosure) {
  nsDependentCString prefName(aPref);
  for (const char* pref : sIntPrefs) {
    if (prefName.Equals(pref)) {
      IntPrefChanged(prefName);
      return;
    }
  }

  for (const char* pref : sFloatPrefs) {
    if (prefName.Equals(pref)) {
      FloatPrefChanged(prefName);
      return;
    }
  }

  for (const char* pref : sColorPrefs) {
    // We use StringBeginsWith to handle .dark prefs too.
    if (StringBeginsWith(prefName, nsDependentCString(pref))) {
      ColorPrefChanged();
      return;
    }
  }

  for (const char* pref : sFontPrefs) {
    if (StringBeginsWith(prefName, nsDependentCString(pref))) {
      FontPrefChanged();
      return;
    }
  }
}

static constexpr struct {
  nsLiteralCString mName;
  widget::ThemeChangeKind mChangeKind =
      widget::ThemeChangeKind::MediaQueriesOnly;
} kMediaQueryPrefs[] = {
    // Affects whether standins are used for the accent color.
    {"widget.non-native-theme.use-theme-accent"_ns,
     widget::ThemeChangeKind::Style},
    // These three affect system colors on Windows.
    {"widget.windows.uwp-system-colors.enabled"_ns,
     widget::ThemeChangeKind::Style},
    {"widget.windows.uwp-system-colors.highlight-accent"_ns,
     widget::ThemeChangeKind::Style},
    // Affects env().
    {"layout.css.prefers-color-scheme.content-override"_ns,
     widget::ThemeChangeKind::Style},
    // Affects media queries and scrollbar sizes, so gotta relayout.
    {"widget.gtk.overlay-scrollbars.enabled"_ns,
     widget::ThemeChangeKind::StyleAndLayout},
    // Affects zoom settings which includes text and full zoom.
    {"browser.display.os-zoom-behavior"_ns,
     widget::ThemeChangeKind::StyleAndLayout},
    // This affects system colors on Linux.
    {"widget.gtk.libadwaita-colors.enabled"_ns, widget::ThemeChangeKind::Style},
    // This affects not only the media query, but also the native theme, so we
    // need to re-layout.
    {"browser.theme.toolbar-theme"_ns, widget::ThemeChangeKind::AllBits},
    {"browser.theme.content-theme"_ns},
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

  if (XRE_IsParentProcess()) {
    nsLayoutUtils::RecomputeSmoothScrollDefault();
  }

  // XXX If we could reorganize the pref names, we should separate the branch
  //     for each types.  Then, we could reduce the unnecessary loop from
  //     nsXPLookAndFeel::OnPrefChanged().
  Preferences::RegisterPrefixCallback(OnPrefChanged, "ui.");
  // We really do just want the accessibility.tabfocus pref, not other prefs
  // that start with that string.
  Preferences::RegisterCallback(OnPrefChanged, "accessibility.tabfocus");

  for (const auto& pref : kMediaQueryPrefs) {
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

  if (aColor == NS_SAME_AS_FOREGROUND_COLOR) {
    return true;
  }

  switch (aID) {
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

nscolor nsXPLookAndFeel::GetStandinForNativeColor(ColorID aID,
                                                  ColorScheme aScheme) {
  if (aScheme == ColorScheme::Dark) {
    if (auto color = GenericDarkColor(aID)) {
      return *color;
    }
  }

  // The stand-in colors are taken from what the non-native theme needs (for
  // field/button colors), the Windows 7 Aero theme except Mac-specific colors
  // which are taken from Mac OS 10.7.

#define COLOR(name_, r, g, b) \
  case ColorID::name_:        \
    return NS_RGB(r, g, b);

#define COLORA(name_, r, g, b, a) \
  case ColorID::name_:            \
    return NS_RGBA(r, g, b, a);

  switch (aID) {
    // These are here for the purposes of headless mode.
    case ColorID::IMESelectedRawTextBackground:
    case ColorID::IMESelectedConvertedTextBackground:
    case ColorID::IMERawInputBackground:
    case ColorID::IMEConvertedTextBackground:
      return NS_TRANSPARENT;
    case ColorID::IMESelectedRawTextForeground:
    case ColorID::IMESelectedConvertedTextForeground:
    case ColorID::IMERawInputForeground:
    case ColorID::IMEConvertedTextForeground:
      return NS_SAME_AS_FOREGROUND_COLOR;
    case ColorID::IMERawInputUnderline:
    case ColorID::IMEConvertedTextUnderline:
      return NS_40PERCENT_FOREGROUND_COLOR;
    case ColorID::Accentcolor:
      return widget::sDefaultAccent.ToABGR();
    case ColorID::Accentcolortext:
      return widget::sDefaultAccentText.ToABGR();
      COLOR(SpellCheckerUnderline, 0xff, 0x00, 0x00)
      COLOR(TextSelectDisabledBackground, 0xAA, 0xAA, 0xAA)

      // Titlebar colors
      // deprecated in CSS Color Level 4, same as Buttonborder:
      COLOR(Activeborder, 0xE3, 0xE3, 0xE3)
      // deprecated in CSS Color Level 4, same as Buttonborder:
      COLOR(Inactiveborder, 0xE3, 0xE3, 0xE3)
      // deprecated in CSS Color Level 4, same as Canvas/Window:
      COLOR(Activecaption, 0xFF, 0xFF, 0xFF)
      // deprecated in CSS Color Level 4, same as Canvas/Window:
      COLOR(Inactivecaption, 0xFF, 0xFF, 0xFF)
      // deprecated in CSS Color Level 4, same as Canvastext/Windowtext:
      COLOR(Captiontext, 0x00, 0x00, 0x00)
      // deprecated in CSS Color Level 4, same as Graytext:
      COLOR(Inactivecaptiontext, 0x6D, 0x6D, 0x6D)

      // CSS 2 colors:
      // deprecated in CSS Color Level 4, same as Canvas/Window:
      COLOR(Appworkspace, 0xFF, 0xFF, 0xFF)
      // deprecated in CSS Color Level 4, same as Canvas/Window:
      COLOR(Background, 0xFF, 0xFF, 0xFF)
      // deprecated in CSS Color Level 4, same as Buttonface:
      COLOR(Buttonhighlight, 0xE9, 0xE9, 0xED)
      // deprecated in CSS Color Level 4, same as Buttonface:
      COLOR(Buttonshadow, 0xE9, 0xE9, 0xED)

      // Buttons and comboboxes should be kept in sync since they are drawn with
      // the same colors by the non-native theme.
      COLOR(Buttonface, 0xE9, 0xE9, 0xED)
      COLORA(MozButtondisabledface, 0xE9, 0xE9, 0xED, 128)

      COLOR(MozCombobox, 0xE9, 0xE9, 0xED)

      COLOR(Buttontext, 0x00, 0x00, 0x00)
      COLOR(MozComboboxtext, 0x00, 0x00, 0x00)

      COLOR(Graytext, 0x6D, 0x6D, 0x6D)
      COLOR(Highlight, 0x33, 0x99, 0xFF)
      COLOR(Highlighttext, 0xFF, 0xFF, 0xFF)
      // deprecated in CSS Color Level 4, same as Canvas/Window:
      COLOR(Infobackground, 0xFF, 0xFF, 0xFF)
      // deprecated in CSS Color Level 4, same as Canvastext/Windowtext:
      COLOR(Infotext, 0x00, 0x00, 0x00)
      // deprecated in CSS Color Level 4, same as Canvas/Window:
      COLOR(Menu, 0xFF, 0xFF, 0xFF)
      // deprecated in CSS Color Level 4, same as Canvastext/Windowtext:
      COLOR(Menutext, 0x00, 0x00, 0x00)
      // deprecated in CSS Color Level 4, same as Canvas/Window:
      COLOR(Scrollbar, 0xFF, 0xFF, 0xFF)
      // deprecated in CSS Color Level 4, same as Buttonborder:
      COLOR(Threeddarkshadow, 0xE3, 0xE3, 0xE3)
      // deprecated in CSS Color Level 4, same as Buttonface:
      COLOR(Threedface, 0xE9, 0xE9, 0xED)
      // deprecated in CSS Color Level 4, same as Buttonborder:
      COLOR(Threedhighlight, 0xE3, 0xE3, 0xE3)
      COLOR(Threedlightshadow, 0xE3, 0xE3, 0xE3)
      // deprecated in CSS Color Level 4, same as Buttonborder:
      COLOR(Threedshadow, 0xE3, 0xE3, 0xE3)
      COLOR(Buttonborder, 0xE3, 0xE3, 0xE3)
      COLOR(Mark, 0xFF, 0xFF, 0x00)
      COLOR(Marktext, 0x00, 0x00, 0x00)
      COLOR(Window, 0xFF, 0xFF, 0xFF)
      // deprecated in CSS Color Level 4, same as Buttonborder:
      COLOR(Windowframe, 0xE3, 0xE3, 0xE3)
      COLOR(Windowtext, 0x00, 0x00, 0x00)
      COLOR(Field, 0xFF, 0xFF, 0xFF)
      COLORA(MozDisabledfield, 0xFF, 0xFF, 0xFF, 128)
      COLOR(Fieldtext, 0x00, 0x00, 0x00)
      COLOR(MozDialog, 0xF0, 0xF0, 0xF0)
      COLOR(MozDialogtext, 0x00, 0x00, 0x00)
      COLOR(MozColheadertext, 0x00, 0x00, 0x00)
      COLOR(MozColheaderhovertext, 0x00, 0x00, 0x00)
      COLOR(MozCellhighlight, 0xF0, 0xF0, 0xF0)
      COLOR(MozCellhighlighttext, 0x00, 0x00, 0x00)
      COLOR(Selecteditem, 0x33, 0x99, 0xFF)
      COLOR(Selecteditemtext, 0xFF, 0xFF, 0xFF)
      COLOR(MozButtonhoverface, 0xd0, 0xd0, 0xd7)
      COLOR(MozButtonhovertext, 0x00, 0x00, 0x00)
      COLOR(MozButtonactiveface, 0xb1, 0xb1, 0xb9)
      COLOR(MozButtonactivetext, 0x00, 0x00, 0x00)
      COLOR(MozMenuhover, 0x33, 0x99, 0xFF)
      COLOR(MozMenuhovertext, 0x00, 0x00, 0x00)
      COLOR(MozMenubarhovertext, 0x00, 0x00, 0x00)
      COLOR(MozMenuhoverdisabled, 0xF0, 0xF0, 0xF0)
      COLOR(MozEventreerow, 0xFF, 0xFF, 0xFF)
      COLOR(MozOddtreerow, 0xFF, 0xFF, 0xFF)
      COLOR(MozMacFocusring, 0x60, 0x9D, 0xD7)
      COLOR(MozMacDisabledtoolbartext, 0x3F, 0x3F, 0x3F)
      // Seems to be the default color (hardcoded because of bug 1065998)
      COLOR(MozNativehyperlinktext, 0x00, 0x66, 0xCC)
      COLOR(MozNativevisitedhyperlinktext, 0x55, 0x1A, 0x8B)
    default:
      break;
  }
  return NS_RGB(0xFF, 0xFF, 0xFF);
}

#undef COLOR
#undef COLORA

// Taken from in-content/common.inc.css's dark theme.
Maybe<nscolor> nsXPLookAndFeel::GenericDarkColor(ColorID aID) {
  nscolor color = NS_RGB(0, 0, 0);
  static constexpr nscolor kWindowBackground = NS_RGB(28, 27, 34);
  static constexpr nscolor kWindowText = NS_RGB(251, 251, 254);
  switch (aID) {
    case ColorID::Window:  // --in-content-page-background
    case ColorID::Background:
      color = kWindowBackground;
      break;

    case ColorID::Menu:
      color = NS_RGB(0x2b, 0x2a, 0x33);
      break;

    case ColorID::MozMenuhovertext:
    case ColorID::MozMenubarhovertext:
    case ColorID::Menutext:
      color = NS_RGB(0xfb, 0xfb, 0xfe);
      break;

    case ColorID::MozMenuhover:
      color = NS_RGB(0x52, 0x52, 0x5e);
      break;

    case ColorID::MozMenuhoverdisabled:
      color = NS_RGB(0x3a, 0x39, 0x44);
      break;

    case ColorID::MozOddtreerow:
    case ColorID::MozDialog:  // --in-content-box-background
      color = NS_RGB(35, 34, 43);
      break;
    case ColorID::Windowtext:  // --in-content-page-color
    case ColorID::MozDialogtext:
    case ColorID::MozSidebartext:
    case ColorID::Fieldtext:
    case ColorID::Buttontext:  // --in-content-button-text-color (via
                               // --in-content-page-color)
    case ColorID::MozComboboxtext:
    case ColorID::MozButtonhovertext:
    case ColorID::MozButtonactivetext:
    case ColorID::MozHeaderbartext:
    case ColorID::MozHeaderbarinactivetext:
    case ColorID::Captiontext:
    case ColorID::Inactivecaptiontext:  // TODO(emilio): Maybe make
                                        // Inactivecaptiontext Graytext?
    case ColorID::MozColheadertext:
    case ColorID::MozColheaderhovertext:
    case ColorID::MozColheaderactivetext:
      color = kWindowText;
      break;
    case ColorID::Buttonshadow:
    case ColorID::Threedshadow:
    case ColorID::MozSidebarborder:
    case ColorID::Threedlightshadow:
    case ColorID::Buttonborder:  // --in-content-box-border-color computed
                                 // with kWindowText above
                                 // kWindowBackground.
    case ColorID::Graytext:      // opacity: 0.4 of kWindowText blended over the
                             // "Window" background color, which happens to be
                             // the same :-)
      color = NS_ComposeColors(kWindowBackground, NS_RGBA(251, 251, 254, 102));
      break;
    case ColorID::MozCellhighlight:
    case ColorID::Selecteditem:  // --in-content-primary-button-background /
                                 // --in-content-item-selected
      color = NS_RGB(0, 221, 255);
      break;
    case ColorID::MozSidebar:
    case ColorID::Field:
    case ColorID::Buttonface:  // --in-content-button-background
    case ColorID::MozColheader:
    case ColorID::Threedface:
    case ColorID::MozCombobox:
    case ColorID::MozCellhighlighttext:
    case ColorID::Selecteditemtext:  // --in-content-primary-button-text-color /
                                     // --in-content-item-selected-text
      color = NS_RGB(43, 42, 51);
      break;
    case ColorID::Threeddarkshadow:  // Same as Threedlightshadow but with the
                                     // background.
    case ColorID::MozDisabledfield:  // opacity: 0.4 of the face above blended
                                     // over the "Window" background color.
    case ColorID::MozButtondisabledface:
      color = NS_ComposeColors(kWindowBackground, NS_RGBA(43, 42, 51, 102));
      break;
    case ColorID::MozButtonhoverface:  // --in-content-button-background-hover
    case ColorID::MozColheaderhover:
      color = NS_RGB(82, 82, 94);
      break;
    case ColorID::MozButtonactiveface:  // --in-content-button-background-active
    case ColorID::MozColheaderactive:
      color = NS_RGB(91, 91, 102);
      break;
    case ColorID::Highlight:
      color = NS_RGBA(0, 221, 255, 78);
      break;
    case ColorID::Highlighttext:
      color = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::MozNativehyperlinktext:
      // If you change this color, you probably also want to change the default
      // value of browser.anchor_color.dark.
      color = NS_RGB(0x8c, 0x8c, 0xff);
      break;
    case ColorID::MozNativevisitedhyperlinktext:
      // If you change this color, you probably also want to change the default
      // value of browser.visited_color.dark.
      color = NS_RGB(0xff, 0xad, 0xff);
      break;
    case ColorID::SpellCheckerUnderline:
      // This is the default for active links in dark mode as well
      // (browser.active_color.dark). See bug 1755564 for some analysis and
      // other options too.
      color = NS_RGB(0xff, 0x66, 0x66);
      break;
    case ColorID::Activeborder:
    case ColorID::Inactiveborder:
      color = NS_RGB(57, 57, 57);
      break;
    case ColorID::MozHeaderbar:
    case ColorID::MozHeaderbarinactive:
    case ColorID::Activecaption:
    case ColorID::Inactivecaption:
      color = NS_RGB(28, 27, 34);
      break;
    default:
      return Nothing();
  }
  return Some(color);
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

static nsresult GetPrefColor(const char* aPref, nscolor& aResult) {
  nsAutoCString colorStr;
  MOZ_TRY(Preferences::GetCString(aPref, colorStr));
  if (!ServoCSSParser::ComputeColor(nullptr, NS_RGB(0, 0, 0), colorStr,
                                    &aResult)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

static nsresult GetColorFromPref(LookAndFeel::ColorID aID, ColorScheme aScheme,
                                 nscolor& aResult) {
  const char* prefName = sColorPrefs[size_t(aID)];
  if (aScheme == ColorScheme::Dark) {
    nsAutoCString darkPrefName(prefName);
    darkPrefName.Append(".dark");
    if (NS_SUCCEEDED(GetPrefColor(darkPrefName.get(), aResult))) {
      return NS_OK;
    }
  }
  return GetPrefColor(prefName, aResult);
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

  auto& cache = sColorCaches.Get(aScheme, aUseStandins);
  if (const auto* cached = cache.Get(aID)) {
    if (cached->isNothing()) {
      return NS_ERROR_FAILURE;
    }
    aResult = cached->value();
    return NS_OK;
  }

  // NOTE: Servo holds a lock and the main thread is paused, so writing to the
  // global cache here is fine.
  auto result = GetUncachedColor(aID, aScheme, aUseStandins);
  cache.Insert(aID, result);
  if (!result) {
    return NS_ERROR_FAILURE;
  }
  aResult = *result;
  return NS_OK;
}

Maybe<nscolor> nsXPLookAndFeel::GetUncachedColor(ColorID aID,
                                                 ColorScheme aScheme,
                                                 UseStandins aUseStandins) {
  if (aUseStandins == UseStandins::Yes) {
    return Some(GetStandinForNativeColor(aID, aScheme));
  }
  nscolor r;
  if (NS_SUCCEEDED(GetColorFromPref(aID, aScheme, r))) {
    return Some(r);
  }
  if (NS_SUCCEEDED(NativeGetColor(aID, aScheme, r))) {
    if (gfxPlatform::GetCMSMode() == CMSMode::All && !IsSpecialColor(aID, r)) {
      qcms_transform* transform = gfxPlatform::GetCMSInverseRGBTransform();
      if (transform) {
        uint8_t color[4];
        color[0] = NS_GET_R(r);
        color[1] = NS_GET_G(r);
        color[2] = NS_GET_B(r);
        color[3] = NS_GET_A(r);
        qcms_transform_data(transform, color, color, 1);
        r = NS_RGBA(color[0], color[1], color[2], color[3]);
      }
    }

    return Some(r);
  }
  return Nothing();
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
  aStyle.weight = FontWeight::FromInt(aFont.weight());
  aStyle.style =
      aFont.italic() ? FontSlantStyle::ITALIC : FontSlantStyle::NORMAL;
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
  auto GetFontsFromPrefs = [&]() -> bool {
    nsDependentCString pref(sFontPrefs[size_t(aID)]);
    if (NS_FAILED(Preferences::GetString(pref.get(), aName))) {
      return false;
    }
    font.haveFont() = true;
    font.name() = aName;
    font.size() = Preferences::GetFloat(nsAutoCString(pref + ".size"_ns).get());
    // This is written this way rather than using the fallback so that an empty
    // pref (such like the one about:config creates) doesn't cause system fonts
    // to have zero-size.
    if (font.size() < 1.0f) {
      font.size() = StyleFONT_MEDIUM_PX;
    }
    font.weight() = Preferences::GetFloat(
        nsAutoCString(pref + ".weight"_ns).get(), FontWeight::NORMAL.ToFloat());
    font.italic() =
        Preferences::GetBool(nsAutoCString(pref + ".italic"_ns).get());
    return true;
  };

  if (GetFontsFromPrefs()) {
    LookAndFeelFontToStyle(font, aName, aStyle);
  } else if (NativeGetFont(aID, aName, aStyle)) {
    font = StyleToLookAndFeelFont(aName, aStyle);
  } else {
    MOZ_ASSERT(!font.haveFont());
  }
  bool success = font.haveFont();
  sFontCache.Insert(aID, std::move(font));
  return success;
}

void nsXPLookAndFeel::RefreshImpl() {
  // Wipe out our caches.
  sColorCaches.Clear();
  sFontCache.Clear();
  sFloatCache.Clear();
  sIntCache.Clear();

  if (XRE_IsParentProcess()) {
    nsLayoutUtils::RecomputeSmoothScrollDefault();
    // Clear any cached FullLookAndFeel data, which is now invalid.
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

bool LookAndFeel::sGlobalThemeChanged;
static widget::ThemeChangeKind sGlobalThemeChangeKind{0};

void LookAndFeel::NotifyChangedAllWindows(widget::ThemeChangeKind aKind) {
  sGlobalThemeChanged = true;
  sGlobalThemeChangeKind |= aKind;

  if (nsCOMPtr<nsIObserverService> obs = services::GetObserverService()) {
    const char16_t kind[] = {char16_t(aKind), 0};
    obs->NotifyObservers(nullptr, "internal-look-and-feel-changed", kind);
  }
}

void LookAndFeel::DoHandleGlobalThemeChange() {
  MOZ_ASSERT(sGlobalThemeChanged);
  sGlobalThemeChanged = false;
  auto kind = std::exchange(sGlobalThemeChangeKind, widget::ThemeChangeKind(0));

  // Tell the theme that it changed, so it can flush any handles to stale theme
  // data.
  //
  // We can use the *DoNotUseDirectly functions directly here, because we want
  // to notify all possible themes in a given process (but just once).
  if (XRE_IsParentProcess() ||
      !StaticPrefs::widget_non_native_theme_enabled()) {
    if (nsCOMPtr<nsITheme> theme = do_GetNativeThemeDoNotUseDirectly()) {
      theme->ThemeChanged();
    }
  }
  if (nsCOMPtr<nsITheme> theme = do_GetBasicNativeThemeDoNotUseDirectly()) {
    theme->ThemeChanged();
  }

  // Clear all cached LookAndFeel colors.
  LookAndFeel::Refresh();

  // Reset default background and foreground colors for the document since they
  // may be using system colors, color scheme, etc.
  PreferenceSheet::Refresh();

  // Vector images (SVG) may be using theme colors so we discard all cached
  // surfaces. (We could add a vector image only version of DiscardAll, but
  // in bug 940625 we decided theme changes are rare enough not to bother.)
  image::SurfaceCacheUtils::DiscardAll();

  if (XRE_IsParentProcess()) {
    dom::ContentParent::BroadcastThemeUpdate(kind);
  }

  nsContentUtils::AddScriptRunner(
      NS_NewRunnableFunction("HandleGlobalThemeChange", [] {
        if (nsCOMPtr<nsIObserverService> obs = services::GetObserverService()) {
          obs->NotifyObservers(nullptr, "look-and-feel-changed", nullptr);
        }
      }));
}

#define BIT_FOR(_c) (1ull << size_t(ColorID::_c))

// We want to use a non-native color scheme for the non-native theme (except in
// high-contrast mode), so spoof some of the colors with stand-ins to prevent
// lack of contrast.
static constexpr std::bitset<size_t(ColorID::End)> sNonNativeThemeStandinColors{
    // Used by default button styles.
    BIT_FOR(Buttonface) | BIT_FOR(Buttontext) | BIT_FOR(MozButtonhoverface) |
    BIT_FOR(MozButtonhovertext) | BIT_FOR(MozButtonactiveface) |
    BIT_FOR(MozButtonactivetext) | BIT_FOR(MozButtondisabledface) |
    BIT_FOR(Buttonborder) |
    // Used by select elements.
    BIT_FOR(MozCombobox) | BIT_FOR(MozComboboxtext) |
    BIT_FOR(Threedlightshadow) |
    // For symmetry with the above.
    BIT_FOR(Threeddarkshadow) |
    // Used by fieldset borders.
    BIT_FOR(Threedface) |
    // Used by input / textarea.
    BIT_FOR(Field) | BIT_FOR(Fieldtext) |
    // Used by disabled form controls.
    BIT_FOR(MozDisabledfield) | BIT_FOR(Graytext) |
    // Per spec, the following colors are deprecated, see
    // https://drafts.csswg.org/css-color-4/#deprecated-system-colors
    // should match ButtonFace:
    BIT_FOR(Buttonhighlight) | BIT_FOR(Buttonshadow) | BIT_FOR(Threedface) |
    // should match ButtonBorder:
    BIT_FOR(Activeborder) | BIT_FOR(Inactiveborder) |
    BIT_FOR(Threeddarkshadow) | BIT_FOR(Threedhighlight) |
    BIT_FOR(Threedshadow) | BIT_FOR(Windowframe) |
    // should match GrayText:
    BIT_FOR(Inactivecaptiontext) |
    // should match Canvas/Window:
    BIT_FOR(Appworkspace) | BIT_FOR(Background) | BIT_FOR(Inactivecaption) |
    BIT_FOR(Infobackground) | BIT_FOR(Menu) | BIT_FOR(Scrollbar) |
    // should match CanvasText/WindowText:
    BIT_FOR(Activecaption) | BIT_FOR(Captiontext) | BIT_FOR(Infotext) |
    BIT_FOR(Menutext) |
    // Some pages expect these to return windows-like colors, see bug 1773795.
    // Also, per spec, these should match Canvas/CanvasText, see
    // https://drafts.csswg.org/css-color-4/#valdef-color-window and
    // https://drafts.csswg.org/css-color-4/#valdef-color-windowtext
    BIT_FOR(Window) | BIT_FOR(Windowtext)};
#undef BIT_FOR

static bool ShouldUseStandinsForNativeColorForNonNativeTheme(
    const dom::Document& aDoc, LookAndFeel::ColorID aColor,
    const PreferenceSheet::Prefs& aPrefs) {
  const bool shouldUseStandinsForColor = [&] {
    if (sNonNativeThemeStandinColors[size_t(aColor)]) {
      return true;
    }
    // There are platforms where we want the content-exposed accent color to be
    // the windows blue rather than the system accent color, for now.
    return !StaticPrefs::widget_non_native_theme_use_theme_accent() &&
           (aColor == LookAndFeel::ColorID::Accentcolor ||
            aColor == LookAndFeel::ColorID::Accentcolortext);
  }();

  return shouldUseStandinsForColor && aDoc.ShouldAvoidNativeTheme() &&
         !aPrefs.NonNativeThemeShouldBeHighContrast();
}

bool LookAndFeel::IsDarkColor(nscolor aColor) {
  // Given https://www.w3.org/TR/WCAG20/#contrast-ratiodef, this is the
  // threshold that tells us whether contrast is better against white or black.
  //
  // Contrast ratio against black is: (L + 0.05) / 0.05
  // Contrast ratio against white is: 1.05 / (L + 0.05)
  //
  // So the intersection is:
  //
  //   (L + 0.05) / 0.05 = 1.05 / (L + 0.05)
  //
  // And the solution to that equation is:
  //
  //   sqrt(1.05 * 0.05) - 0.05
  //
  // So we consider a color dark if the contrast is below this threshold, and
  // it's at least half-opaque.
  constexpr float kThreshold = 0.179129;
  return NS_GET_A(aColor) > 127 &&
         RelativeLuminanceUtils::Compute(aColor) < kThreshold;
}

ColorScheme LookAndFeel::ColorSchemeForStyle(
    const dom::Document& aDoc, const StyleColorSchemeFlags& aFlags,
    ColorSchemeMode aMode) {
  const auto& prefs = PreferenceSheet::PrefsFor(aDoc);
  StyleColorSchemeFlags style(aFlags);
  if (!style) {
    style._0 = aDoc.GetColorSchemeBits();
  }
  const bool supportsDark = bool(style & StyleColorSchemeFlags::DARK);
  const bool supportsLight = bool(style & StyleColorSchemeFlags::LIGHT);
  if (supportsLight && supportsDark) {
    // Both color-schemes are explicitly supported, use the preferred one.
    return aDoc.PreferredColorScheme();
  }
  if (supportsDark || supportsLight) {
    // One color-scheme is explicitly supported and one isn't, so use the one
    // the content supports.
    return supportsDark ? ColorScheme::Dark : ColorScheme::Light;
  }
  // No value specified. Chrome docs, and forced-colors mode always supports
  // both, so use the preferred color-scheme.
  if (aMode == ColorSchemeMode::Preferred || aDoc.ChromeRulesEnabled() ||
      !prefs.mUseDocumentColors) {
    return aDoc.PreferredColorScheme();
  }
  // Otherwise default content to light.
  return ColorScheme::Light;
}

LookAndFeel::ColorScheme LookAndFeel::ColorSchemeForFrame(
    const nsIFrame* aFrame, ColorSchemeMode aMode) {
  return ColorSchemeForStyle(*aFrame->PresContext()->Document(),
                             aFrame->StyleUI()->mColorScheme.bits, aMode);
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
    case ColorID::TextSelectDisabledBackground:
    case ColorID::TextSelectAttentionBackground:
    case ColorID::TextSelectAttentionForeground:
    case ColorID::TextHighlightBackground:
    case ColorID::TextHighlightForeground:
    case ColorID::ThemedScrollbar:
    case ColorID::ThemedScrollbarInactive:
    case ColorID::ThemedScrollbarThumb:
    case ColorID::ThemedScrollbarThumbActive:
    case ColorID::ThemedScrollbarThumbInactive:
    case ColorID::ThemedScrollbarThumbHover:
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
  const auto& prefs = PreferenceSheet::PrefsFor(aDoc);
  if (ShouldUseStandinsForNativeColorForNonNativeTheme(aDoc, aId, prefs)) {
    return UseStandins::Yes;
  }
  if (prefs.mUseStandins && ColorIsCSSAccessible(aId)) {
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

bool LookAndFeel::DrawInTitlebar() {
  switch (StaticPrefs::browser_tabs_inTitlebar()) {
    case 0:
      return false;
    case 1:
      return true;
    default:
      break;
  }
  return nsLookAndFeel::GetInstance()->GetDefaultDrawInTitlebar();
}

void LookAndFeel::GetThemeInfo(nsACString& aOut) {
  nsLookAndFeel::GetInstance()->GetThemeInfo(aOut);
}

uint32_t LookAndFeel::GetMenuAccessKey() {
  return StaticPrefs::ui_key_menuAccessKey();
}

Modifiers LookAndFeel::GetMenuAccessKeyModifiers() {
  switch (GetMenuAccessKey()) {
    case dom::KeyboardEvent_Binding::DOM_VK_SHIFT:
      return MODIFIER_SHIFT;
    case dom::KeyboardEvent_Binding::DOM_VK_CONTROL:
      return MODIFIER_CONTROL;
    case dom::KeyboardEvent_Binding::DOM_VK_ALT:
      return MODIFIER_ALT;
    case dom::KeyboardEvent_Binding::DOM_VK_META:
    case dom::KeyboardEvent_Binding::DOM_VK_WIN:
      return MODIFIER_META;
    default:
      return 0;
  }
}

// static
void LookAndFeel::Refresh() {
  nsLookAndFeel::GetInstance()->RefreshImpl();
  widget::Theme::LookAndFeelChanged();
}

// static
void LookAndFeel::NativeInit() { nsLookAndFeel::GetInstance()->NativeInit(); }

// static
void LookAndFeel::SetData(widget::FullLookAndFeel&& aTables) {
  nsLookAndFeel::GetInstance()->SetDataImpl(std::move(aTables));
}

}  // namespace mozilla
