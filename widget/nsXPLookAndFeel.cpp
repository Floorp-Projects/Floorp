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
#include "nsIXULRuntime.h"
#include "nsNativeBasicTheme.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/StaticPrefs_editor.h"
#include "mozilla/StaticPrefs_findbar.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/widget/WidgetMessageUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryScalarEnums.h"

#include "gfxPlatform.h"

#include "qcms.h"

#ifdef DEBUG
#  include "nsSize.h"
#endif

using namespace mozilla;

// To make one of these prefs toggleable from a reftest add a user
// pref in testing/profiles/reftest/user.js. For example, to make
// ui.useAccessibilityTheme toggleable, add:
//
// user_pref("ui.useAccessibilityTheme", 0);
//
nsLookAndFeelIntPref nsXPLookAndFeel::sIntPrefs[] = {
    {"ui.caretBlinkTime", IntID::CaretBlinkTime, false, 0},
    {"ui.caretWidth", IntID::CaretWidth, false, 0},
    {"ui.caretVisibleWithSelection", IntID::ShowCaretDuringSelection, false, 0},
    {"ui.submenuDelay", IntID::SubmenuDelay, false, 0},
    {"ui.dragThresholdX", IntID::DragThresholdX, false, 0},
    {"ui.dragThresholdY", IntID::DragThresholdY, false, 0},
    {"ui.useAccessibilityTheme", IntID::UseAccessibilityTheme, false, 0},
    {"ui.menusCanOverlapOSBar", IntID::MenusCanOverlapOSBar, false, 0},
    {"ui.useOverlayScrollbars", IntID::UseOverlayScrollbars, false, 0},
    {"ui.scrollbarDisplayOnMouseMove", IntID::ScrollbarDisplayOnMouseMove,
     false, 0},
    {"ui.scrollbarFadeBeginDelay", IntID::ScrollbarFadeBeginDelay, false, 0},
    {"ui.scrollbarFadeDuration", IntID::ScrollbarFadeDuration, false, 0},
    {"ui.showHideScrollbars", IntID::ShowHideScrollbars, false, 0},
    {"ui.skipNavigatingDisabledMenuItem", IntID::SkipNavigatingDisabledMenuItem,
     false, 0},
    {"ui.treeOpenDelay", IntID::TreeOpenDelay, false, 0},
    {"ui.treeCloseDelay", IntID::TreeCloseDelay, false, 0},
    {"ui.treeLazyScrollDelay", IntID::TreeLazyScrollDelay, false, 0},
    {"ui.treeScrollDelay", IntID::TreeScrollDelay, false, 0},
    {"ui.treeScrollLinesMax", IntID::TreeScrollLinesMax, false, 0},
    {"accessibility.tabfocus", IntID::TabFocusModel, false, 0},
    {"ui.alertNotificationOrigin", IntID::AlertNotificationOrigin, false, 0},
    {"ui.scrollToClick", IntID::ScrollToClick, false, 0},
    {"ui.IMERawInputUnderlineStyle", IntID::IMERawInputUnderlineStyle, false,
     0},
    {"ui.IMESelectedRawTextUnderlineStyle",
     IntID::IMESelectedRawTextUnderlineStyle, false, 0},
    {"ui.IMEConvertedTextUnderlineStyle", IntID::IMEConvertedTextUnderlineStyle,
     false, 0},
    {"ui.IMESelectedConvertedTextUnderlineStyle",
     IntID::IMESelectedConvertedTextUnderline, false, 0},
    {"ui.SpellCheckerUnderlineStyle", IntID::SpellCheckerUnderlineStyle, false,
     0},
    {"ui.scrollbarButtonAutoRepeatBehavior",
     IntID::ScrollbarButtonAutoRepeatBehavior, false, 0},
    {"ui.tooltipDelay", IntID::TooltipDelay, false, 0},
    {"ui.contextMenuOffsetVertical", IntID::ContextMenuOffsetVertical, false,
     0},
    {"ui.contextMenuOffsetHorizontal", IntID::ContextMenuOffsetHorizontal,
     false, 0},
    {"ui.GtkCSDAvailable", IntID::GTKCSDAvailable, false, 0},
    {"ui.GtkCSDHideTitlebarByDefault", IntID::GTKCSDHideTitlebarByDefault,
     false, 0},
    {"ui.GtkCSDTransparentBackground", IntID::GTKCSDTransparentBackground,
     false, 0},
    {"ui.GtkCSDMinimizeButton", IntID::GTKCSDMinimizeButton, false, 0},
    {"ui.GtkCSDMaximizeButton", IntID::GTKCSDMaximizeButton, false, 0},
    {"ui.GtkCSDCloseButton", IntID::GTKCSDCloseButton, false, 0},
    {"ui.systemUsesDarkTheme", IntID::SystemUsesDarkTheme, false, 0},
    {"ui.prefersReducedMotion", IntID::PrefersReducedMotion, false, 0},
    {"ui.primaryPointerCapabilities", IntID::PrimaryPointerCapabilities, false,
     6 /* fine and hover-capable pointer, i.e. mouse-type */},
    {"ui.allPointerCapabilities", IntID::AllPointerCapabilities, false,
     6 /* fine and hover-capable pointer, i.e. mouse-type */},
    {"ui.scrollArrowStyle", IntID::ScrollArrowStyle, false, 0},
};

nsLookAndFeelFloatPref nsXPLookAndFeel::sFloatPrefs[] = {
    {"ui.IMEUnderlineRelativeSize", FloatID::IMEUnderlineRelativeSize, false,
     0},
    {"ui.SpellCheckerUnderlineRelativeSize",
     FloatID::SpellCheckerUnderlineRelativeSize, false, 0},
    {"ui.caretAspectRatio", FloatID::CaretAspectRatio, false, 0},
};

// This array MUST be kept in the same order as the color list in
// specified/color.rs
/* XXX If you add any strings longer than
 * "ui.-moz-mac-active-source-list-selection"
 * to the following array then you MUST update the
 * sizes of the sColorPrefs array in nsXPLookAndFeel.h
 */
const char nsXPLookAndFeel::sColorPrefs[][41] = {
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
    "ui.textSelectForegroundCustom",
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
    "ui.-moz-mac-vibrancy-light",
    "ui.-moz-mac-vibrancy-dark",
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
    "ui.-moz-win-accentcolor",
    "ui.-moz-win-accentcolortext",
    "ui.-moz-win-mediatext",
    "ui.-moz-win-communicationstext",
    "ui.-moz-nativehyperlinktext",
    "ui.-moz-hyperlinktext",
    "ui.-moz-activehyperlinktext",
    "ui.-moz-visitedhyperlinktext",
    "ui.-moz-comboboxtext",
    "ui.-moz-combobox",
    "ui.-moz-gtk-info-bar-text",
    "ui.-moz-colheadertext",
    "ui.-moz-colheaderhovertext"};

int32_t nsXPLookAndFeel::sCachedColors[size_t(LookAndFeel::ColorID::End)] = {0};
int32_t nsXPLookAndFeel::sCachedColorBits[COLOR_CACHE_SIZE] = {0};

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
  // us with an initial FullLookAndFeel object (for when the RemoteLookAndFeel
  // is to be used) or an initial LookAndFeelCache object (for regular
  // LookAndFeel implementations).  We grab this data from the ContentChild,
  // where it's been temporarily stashed, and initialize our new LookAndFeel
  // object with it.

  LookAndFeelCache* lnfCache = nullptr;
  FullLookAndFeel* fullLnf = nullptr;
  widget::LookAndFeelData* lnfData = nullptr;

  if (auto* cc = mozilla::dom::ContentChild::GetSingleton()) {
    lnfData = &cc->BorrowLookAndFeelData();
    switch (lnfData->type()) {
      case widget::LookAndFeelData::TLookAndFeelCache:
        lnfCache = &lnfData->get_LookAndFeelCache();
        break;
      case widget::LookAndFeelData::TFullLookAndFeel:
        fullLnf = &lnfData->get_FullLookAndFeel();
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("unexpected LookAndFeelData type");
    }
  }

  if (fullLnf) {
    sInstance = new widget::RemoteLookAndFeel(std::move(*fullLnf));
  } else if (gfxPlatform::IsHeadless()) {
    sInstance = new widget::HeadlessLookAndFeel(lnfCache);
  } else {
    sInstance = new nsLookAndFeel(lnfCache);
  }

  // This is only ever used once during initialization, and can be cleared now.
  if (lnfData) {
    *lnfData = widget::LookAndFeelData{};
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
  nsNativeBasicTheme::Shutdown();
}

// static
void nsXPLookAndFeel::IntPrefChanged(nsLookAndFeelIntPref* data) {
  if (!data) {
    return;
  }

  int32_t intpref;
  nsresult rv = Preferences::GetInt(data->name, &intpref);
  if (NS_FAILED(rv)) {
    data->isSet = false;

#ifdef DEBUG_akkana
    printf("====== Cleared int pref %s\n", data->name);
#endif
  } else {
    data->intVar = intpref;
    data->isSet = true;

#ifdef DEBUG_akkana
    printf("====== Changed int pref %s to %d\n", data->name, data->intVar);
#endif
  }

  // Int prefs can't change our system colors or fonts.
  NotifyChangedAllWindows(widget::ThemeChangeKind::MediaQueriesOnly);
}

// static
void nsXPLookAndFeel::FloatPrefChanged(nsLookAndFeelFloatPref* data) {
  if (!data) {
    return;
  }

  int32_t intpref;
  nsresult rv = Preferences::GetInt(data->name, &intpref);
  if (NS_FAILED(rv)) {
    data->isSet = false;

#ifdef DEBUG_akkana
    printf("====== Cleared float pref %s\n", data->name);
#endif
  } else {
    data->floatVar = (float)intpref / 100.0f;
    data->isSet = true;

#ifdef DEBUG_akkana
    printf("====== Changed float pref %s to %f\n", data->name);
#endif
  }

  // Float prefs can't change our system colors or fonts.
  NotifyChangedAllWindows(widget::ThemeChangeKind::MediaQueriesOnly);
}

// static
void nsXPLookAndFeel::ColorPrefChanged(unsigned int index,
                                       const char* prefName) {
  nsAutoString colorStr;
  nsresult rv = Preferences::GetString(prefName, colorStr);
  if (NS_SUCCEEDED(rv) && !colorStr.IsEmpty()) {
    nscolor thecolor;
    if (colorStr[0] == char16_t('#')) {
      if (NS_HexToRGBA(nsDependentString(colorStr, 1), nsHexColorType::NoAlpha,
                       &thecolor)) {
        int32_t id = NS_PTR_TO_INT32(index);
        CACHE_COLOR(id, thecolor);
      }
    } else if (NS_ColorNameToRGB(colorStr, &thecolor)) {
      int32_t id = NS_PTR_TO_INT32(index);
      CACHE_COLOR(id, thecolor);
#ifdef DEBUG_akkana
      printf("====== Changed color pref %s to 0x%lx\n", prefName, thecolor);
#endif
    }
  } else {
    // Reset to the default color, by clearing the cache
    // to force lookup when the color is next used
    int32_t id = NS_PTR_TO_INT32(index);
    CLEAR_COLOR_CACHE(id);

#ifdef DEBUG_akkana
    printf("====== Cleared color pref %s\n", prefName);
#endif
  }

  // Color prefs affect style, because they by definition change system colors.
  NotifyChangedAllWindows(widget::ThemeChangeKind::Style);
}

void nsXPLookAndFeel::InitFromPref(nsLookAndFeelIntPref* aPref) {
  int32_t intpref;
  nsresult rv = Preferences::GetInt(aPref->name, &intpref);
  if (NS_SUCCEEDED(rv)) {
    aPref->isSet = true;
    aPref->intVar = intpref;
  }
}

void nsXPLookAndFeel::InitFromPref(nsLookAndFeelFloatPref* aPref) {
  int32_t intpref;
  nsresult rv = Preferences::GetInt(aPref->name, &intpref);
  if (NS_SUCCEEDED(rv)) {
    aPref->isSet = true;
    aPref->floatVar = (float)intpref / 100.0f;
  }
}

void nsXPLookAndFeel::InitColorFromPref(int32_t i) {
  static_assert(ArrayLength(sColorPrefs) == size_t(ColorID::End),
                "Should have a pref for each color value");

  nsAutoString colorStr;
  nsresult rv = Preferences::GetString(sColorPrefs[i], colorStr);
  if (NS_FAILED(rv) || colorStr.IsEmpty()) {
    return;
  }
  nscolor thecolor;
  if (colorStr[0] == char16_t('#')) {
    nsAutoString hexString;
    colorStr.Right(hexString, colorStr.Length() - 1);
    if (NS_HexToRGBA(hexString, nsHexColorType::NoAlpha, &thecolor)) {
      CACHE_COLOR(i, thecolor);
    }
  } else if (NS_ColorNameToRGB(colorStr, &thecolor)) {
    CACHE_COLOR(i, thecolor);
  }
}

// static
void nsXPLookAndFeel::OnPrefChanged(const char* aPref, void* aClosure) {
  // looping in the same order as in ::Init

  nsDependentCString prefName(aPref);
  unsigned int i;
  for (i = 0; i < ArrayLength(sIntPrefs); ++i) {
    if (prefName.Equals(sIntPrefs[i].name)) {
      IntPrefChanged(&sIntPrefs[i]);
      return;
    }
  }

  for (i = 0; i < ArrayLength(sFloatPrefs); ++i) {
    if (prefName.Equals(sFloatPrefs[i].name)) {
      FloatPrefChanged(&sFloatPrefs[i]);
      return;
    }
  }

  for (i = 0; i < ArrayLength(sColorPrefs); ++i) {
    if (prefName.Equals(sColorPrefs[i])) {
      ColorPrefChanged(i, sColorPrefs[i]);
      return;
    }
  }
}

//
// Read values from the user's preferences.
// This is done once at startup, but since the user's preferences
// haven't actually been read yet at that time, we also have to
// set a callback to inform us of changes to each pref.
//
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

  unsigned int i;
  for (i = 0; i < ArrayLength(sIntPrefs); ++i) {
    InitFromPref(&sIntPrefs[i]);
  }

  for (i = 0; i < ArrayLength(sFloatPrefs); ++i) {
    InitFromPref(&sFloatPrefs[i]);
  }

  for (i = 0; i < ArrayLength(sColorPrefs); ++i) {
    InitColorFromPref(i);
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
      return aColor == NS_DONT_CHANGE_COLOR;
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

// Returns whether there is a CSS color name for this color.
static bool ColorIsCSSAccessible(LookAndFeel::ColorID aID) {
  using ColorID = LookAndFeel::ColorID;

  switch (aID) {
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
    COLOR(MozMacVibrancyLight, 0xf7, 0xf7, 0xf7)
    COLOR(MozMacVibrantTitlebarLight, 0xf7, 0xf7, 0xf7)
    COLOR(MozMacVibrancyDark, 0x28, 0x28, 0x28)
    COLOR(MozMacVibrantTitlebarDark, 0x28, 0x28, 0x28)
    COLOR(MozMacMenupopup, 0xe6, 0xe6, 0xe6)
    COLOR(MozMacMenuitem, 0xe6, 0xe6, 0xe6)
    COLOR(MozMacActiveMenuitem, 0x0a, 0x64, 0xdc)
    COLOR(MozMacSourceList, 0xf7, 0xf7, 0xf7)
    COLOR(MozMacSourceListSelection, 0xc8, 0xc8, 0xc8)
    COLOR(MozMacActiveSourceListSelection, 0x0a, 0x64, 0xdc)
    COLOR(MozMacTooltip, 0xf7, 0xf7, 0xf7)
    // Seems to be the default color (hardcoded because of bug 1065998)
    COLOR(MozWinAccentcolor, 0x9E, 0x9E, 0x9E)
    COLOR(MozWinAccentcolortext, 0x00, 0x00, 0x00)
    COLOR(MozWinMediatext, 0xFF, 0xFF, 0xFF)
    COLOR(MozWinCommunicationstext, 0xFF, 0xFF, 0xFF)
    COLOR(MozNativehyperlinktext, 0x00, 0x66, 0xCC)
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

// All these routines will return NS_OK if they have a value,
// in which case the nsLookAndFeel should use that value;
// otherwise we'll return NS_ERROR_NOT_AVAILABLE, in which case, the
// platform-specific nsLookAndFeel should use its own values instead.
nsresult nsXPLookAndFeel::GetColorValue(ColorID aID,
                                        bool aUseStandinsForNativeColors,
                                        nscolor& aResult) {
  if (!sInitialized) {
    Init();
  }

#ifdef DEBUG_SYSTEM_COLOR_USE
  if (NS_SUCCEEDED(SystemColorUseDebuggingColor(aID, aResult))) {
    return NS_OK;
  }
#endif

  // We only use standins for colors that we can access via CSS.
  aUseStandinsForNativeColors =
      aUseStandinsForNativeColors && ColorIsCSSAccessible(aID);

  if (!aUseStandinsForNativeColors && IS_COLOR_CACHED(aID)) {
    aResult = sCachedColors[uint32_t(aID)];
    return NS_OK;
  }

  // There are no system color settings for these, so set them manually
#ifndef XP_MACOSX
  if (aID == ColorID::TextSelectBackgroundDisabled) {
    // This is used to gray out the selection when it's not focused
    // Used with nsISelectionController::SELECTION_DISABLED
    aResult = NS_RGB(0xb0, 0xb0, 0xb0);
    return NS_OK;
  }
#endif

  if (aID == ColorID::TextSelectBackgroundAttention) {
    if (StaticPrefs::findbar_modalHighlight() && !mozilla::FissionAutostart()) {
      aResult = NS_RGBA(0, 0, 0, 0);
      return NS_OK;
    }

    // This makes the selection stand out when typeaheadfind is on
    // Used with nsISelectionController::SELECTION_ATTENTION
    aResult = NS_RGB(0x38, 0xd8, 0x78);
    return NS_OK;
  }

  if (aID == ColorID::TextHighlightBackground) {
    // This makes the matched text stand out when findbar highlighting is on
    // Used with nsISelectionController::SELECTION_FIND
    aResult = NS_RGB(0xef, 0x0f, 0xff);
    return NS_OK;
  }

  if (aID == ColorID::TextHighlightForeground) {
    // The foreground color for the matched text in findbar highlighting
    // Used with nsISelectionController::SELECTION_FIND
    aResult = NS_RGB(0xff, 0xff, 0xff);
    return NS_OK;
  }

  if (aUseStandinsForNativeColors) {
    aResult = GetStandinForNativeColor(aID);
    return NS_OK;
  }

  if (NS_SUCCEEDED(NativeGetColor(aID, aResult))) {
    // TODO(bug 1678487): We should color-correct style colors as well when in
    // the traversal.
    if (!mozilla::ServoStyleSet::IsInServoTraversal()) {
      MOZ_ASSERT(NS_IsMainThread());
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

      CACHE_COLOR(aID, aResult);
    }
    return NS_OK;
  }

  return NS_ERROR_NOT_AVAILABLE;
}

nsresult nsXPLookAndFeel::GetIntValue(IntID aID, int32_t& aResult) {
  if (!sInitialized) Init();

  for (unsigned int i = 0; i < ArrayLength(sIntPrefs); ++i) {
    if (sIntPrefs[i].isSet && (sIntPrefs[i].id == aID)) {
      aResult = sIntPrefs[i].intVar;
      return NS_OK;
    }
  }

  return NativeGetInt(aID, aResult);
}

nsresult nsXPLookAndFeel::GetFloatValue(FloatID aID, float& aResult) {
  if (!sInitialized) Init();

  for (unsigned int i = 0; i < ArrayLength(sFloatPrefs); ++i) {
    if (sFloatPrefs[i].isSet && sFloatPrefs[i].id == aID) {
      aResult = sFloatPrefs[i].floatVar;
      return NS_OK;
    }
  }

  return NativeGetFloat(aID, aResult);
}

void nsXPLookAndFeel::RefreshImpl() {
  // Wipe out our color cache.
  uint32_t i;
  for (i = 0; i < uint32_t(ColorID::End); i++) {
    sCachedColors[i] = 0;
  }
  for (i = 0; i < COLOR_CACHE_SIZE; i++) {
    sCachedColorBits[i] = 0;
  }

  // Reinit color cache from prefs.
  for (i = 0; i < uint32_t(ColorID::End); ++i) {
    InitColorFromPref(i);
  }

  // Clear any cached FullLookAndFeel data, which is now invalid.
  if (XRE_IsParentProcess()) {
    widget::RemoteLookAndFeel::ClearCachedData();
  }
}

widget::LookAndFeelCache nsXPLookAndFeel::GetCacheImpl() {
  return LookAndFeelCache{};
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
    obs->NotifyObservers(nullptr, "look-and-feel-changed",
                         reinterpret_cast<char16_t*>(uintptr_t(aKind)));
  }
}

// static
nsresult LookAndFeel::GetColor(ColorID aID, nscolor* aResult) {
  return nsLookAndFeel::GetInstance()->GetColorValue(aID, false, *aResult);
}

nsresult LookAndFeel::GetColor(ColorID aID, bool aUseStandinsForNativeColors,
                               nscolor* aResult) {
  return nsLookAndFeel::GetInstance()->GetColorValue(
      aID, aUseStandinsForNativeColors, *aResult);
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

// static
void LookAndFeel::Refresh() {
  nsLookAndFeel::GetInstance()->RefreshImpl();
  nsNativeBasicTheme::LookAndFeelChanged();
}

// static
void LookAndFeel::NativeInit() { nsLookAndFeel::GetInstance()->NativeInit(); }

// static
widget::LookAndFeelCache LookAndFeel::GetCache() {
  return nsLookAndFeel::GetInstance()->GetCacheImpl();
}

// static
void LookAndFeel::SetCache(const widget::LookAndFeelCache& aCache) {
  nsLookAndFeel::GetInstance()->SetCacheImpl(aCache);
}

// static
void LookAndFeel::SetData(widget::FullLookAndFeel&& aTables) {
  nsLookAndFeel::GetInstance()->SetDataImpl(std::move(aTables));
}

}  // namespace mozilla
