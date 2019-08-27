/* -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nscore.h"

#include "nsXPLookAndFeel.h"
#include "nsLookAndFeel.h"
#include "HeadlessLookAndFeel.h"
#include "nsCRT.h"
#include "nsFont.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/StaticPrefs_editor.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/widget/WidgetMessageUtils.h"

#include "gfxPlatform.h"

#include "qcms.h"

#ifdef DEBUG
#  include "nsSize.h"
#endif

using namespace mozilla;

nsLookAndFeelIntPref nsXPLookAndFeel::sIntPrefs[] = {
    {"ui.caretBlinkTime", eIntID_CaretBlinkTime, false, 0},
    {"ui.caretWidth", eIntID_CaretWidth, false, 0},
    {"ui.caretVisibleWithSelection", eIntID_ShowCaretDuringSelection, false, 0},
    {"ui.submenuDelay", eIntID_SubmenuDelay, false, 0},
    {"ui.dragThresholdX", eIntID_DragThresholdX, false, 0},
    {"ui.dragThresholdY", eIntID_DragThresholdY, false, 0},
    {"ui.useAccessibilityTheme", eIntID_UseAccessibilityTheme, false, 0},
    {"ui.menusCanOverlapOSBar", eIntID_MenusCanOverlapOSBar, false, 0},
    {"ui.useOverlayScrollbars", eIntID_UseOverlayScrollbars, false, 0},
    {"ui.scrollbarDisplayOnMouseMove", eIntID_ScrollbarDisplayOnMouseMove,
     false, 0},
    {"ui.scrollbarFadeBeginDelay", eIntID_ScrollbarFadeBeginDelay, false, 0},
    {"ui.scrollbarFadeDuration", eIntID_ScrollbarFadeDuration, false, 0},
    {"ui.showHideScrollbars", eIntID_ShowHideScrollbars, false, 0},
    {"ui.skipNavigatingDisabledMenuItem", eIntID_SkipNavigatingDisabledMenuItem,
     false, 0},
    {"ui.treeOpenDelay", eIntID_TreeOpenDelay, false, 0},
    {"ui.treeCloseDelay", eIntID_TreeCloseDelay, false, 0},
    {"ui.treeLazyScrollDelay", eIntID_TreeLazyScrollDelay, false, 0},
    {"ui.treeScrollDelay", eIntID_TreeScrollDelay, false, 0},
    {"ui.treeScrollLinesMax", eIntID_TreeScrollLinesMax, false, 0},
    {"accessibility.tabfocus", eIntID_TabFocusModel, false, 0},
    {"ui.alertNotificationOrigin", eIntID_AlertNotificationOrigin, false, 0},
    {"ui.scrollToClick", eIntID_ScrollToClick, false, 0},
    {"ui.IMERawInputUnderlineStyle", eIntID_IMERawInputUnderlineStyle, false,
     0},
    {"ui.IMESelectedRawTextUnderlineStyle",
     eIntID_IMESelectedRawTextUnderlineStyle, false, 0},
    {"ui.IMEConvertedTextUnderlineStyle", eIntID_IMEConvertedTextUnderlineStyle,
     false, 0},
    {"ui.IMESelectedConvertedTextUnderlineStyle",
     eIntID_IMESelectedConvertedTextUnderline, false, 0},
    {"ui.SpellCheckerUnderlineStyle", eIntID_SpellCheckerUnderlineStyle, false,
     0},
    {"ui.scrollbarButtonAutoRepeatBehavior",
     eIntID_ScrollbarButtonAutoRepeatBehavior, false, 0},
    {"ui.tooltipDelay", eIntID_TooltipDelay, false, 0},
    {"ui.contextMenuOffsetVertical", eIntID_ContextMenuOffsetVertical, false,
     0},
    {"ui.contextMenuOffsetHorizontal", eIntID_ContextMenuOffsetHorizontal,
     false, 0},
    {"ui.GtkCSDAvailable", eIntID_GTKCSDAvailable, false, 0},
    {"ui.GtkCSDHideTitlebarByDefault", eIntID_GTKCSDHideTitlebarByDefault,
     false, 0},
    {"ui.GtkCSDTransparentBackground", eIntID_GTKCSDTransparentBackground,
     false, 0},
    {"ui.GtkCSDMinimizeButton", eIntID_GTKCSDMinimizeButton, false, 0},
    {"ui.GtkCSDMaximizeButton", eIntID_GTKCSDMaximizeButton, false, 0},
    {"ui.GtkCSDCloseButton", eIntID_GTKCSDCloseButton, false, 0},
    {"ui.systemUsesDarkTheme", eIntID_SystemUsesDarkTheme, false, 0},
    {"ui.prefersReducedMotion", eIntID_PrefersReducedMotion, false, 0},
    {"ui.primaryPointerCapabilities", eIntID_PrimaryPointerCapabilities, false,
     6 /* fine and hover-capable pointer, i.e. mouse-type */},
    {"ui.allPointerCapabilities", eIntID_AllPointerCapabilities, false,
     6 /* fine and hover-capable pointer, i.e. mouse-type */},
};

nsLookAndFeelFloatPref nsXPLookAndFeel::sFloatPrefs[] = {
    {"ui.IMEUnderlineRelativeSize", eFloatID_IMEUnderlineRelativeSize, false,
     0},
    {"ui.SpellCheckerUnderlineRelativeSize",
     eFloatID_SpellCheckerUnderlineRelativeSize, false, 0},
    {"ui.caretAspectRatio", eFloatID_CaretAspectRatio, false, 0},
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
    "ui.activeborder",
    "ui.activecaption",
    "ui.appworkspace",
    "ui.background",
    "ui.buttonface",
    "ui.buttonhighlight",
    "ui.buttonshadow",
    "ui.buttontext",
    "ui.captiontext",
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
    "ui.-moz-field",
    "ui.-moz-fieldtext",
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
    "ui.-moz-win-accentcolor",
    "ui.-moz-win-accentcolortext",
    "ui.-moz-win-mediatext",
    "ui.-moz-win-communicationstext",
    "ui.-moz-nativehyperlinktext",
    "ui.-moz-comboboxtext",
    "ui.-moz-combobox",
    "ui.-moz-gtk-info-bar-text"};

int32_t nsXPLookAndFeel::sCachedColors[size_t(LookAndFeel::ColorID::End)] = {0};
int32_t nsXPLookAndFeel::sCachedColorBits[COLOR_CACHE_SIZE] = {0};

bool nsXPLookAndFeel::sInitialized = false;
bool nsXPLookAndFeel::sFindbarModalHighlight = false;
bool nsXPLookAndFeel::sIsInPrefersReducedMotionForTest = false;
bool nsXPLookAndFeel::sPrefersReducedMotionForTest = false;

nsXPLookAndFeel* nsXPLookAndFeel::sInstance = nullptr;
bool nsXPLookAndFeel::sShutdown = false;

// static
nsXPLookAndFeel* nsXPLookAndFeel::GetInstance() {
  if (sInstance) {
    return sInstance;
  }

  NS_ENSURE_TRUE(!sShutdown, nullptr);

  if (gfxPlatform::IsHeadless()) {
    sInstance = new widget::HeadlessLookAndFeel();
  } else {
    sInstance = new nsLookAndFeel();
  }
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
}

nsXPLookAndFeel::nsXPLookAndFeel()
    : LookAndFeel(), mShouldRetainCacheForTest(false) {}

// static
void nsXPLookAndFeel::IntPrefChanged(nsLookAndFeelIntPref* data) {
  if (!data) {
    return;
  }

  int32_t intpref;
  nsresult rv = Preferences::GetInt(data->name, &intpref);
  if (NS_FAILED(rv)) {
    return;
  }
  data->intVar = intpref;
  data->isSet = true;
#ifdef DEBUG_akkana
  printf("====== Changed int pref %s to %d\n", data->name, data->intVar);
#endif
}

// static
void nsXPLookAndFeel::FloatPrefChanged(nsLookAndFeelFloatPref* data) {
  if (!data) {
    return;
  }

  int32_t intpref;
  nsresult rv = Preferences::GetInt(data->name, &intpref);
  if (NS_FAILED(rv)) {
    return;
  }
  data->floatVar = (float)intpref / 100.0f;
  data->isSet = true;
#ifdef DEBUG_akkana
  printf("====== Changed float pref %s to %f\n", data->name, data->floatVar);
#endif
}

// static
void nsXPLookAndFeel::ColorPrefChanged(unsigned int index,
                                       const char* prefName) {
  nsAutoString colorStr;
  nsresult rv = Preferences::GetString(prefName, colorStr);
  if (NS_FAILED(rv)) {
    return;
  }
  if (!colorStr.IsEmpty()) {
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
  }
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

  Preferences::AddBoolVarCache(&sFindbarModalHighlight,
                               "findbar.modalHighlight",
                               sFindbarModalHighlight);

  if (XRE_IsContentProcess()) {
    mozilla::dom::ContentChild* cc = mozilla::dom::ContentChild::GetSingleton();

    LookAndFeel::SetIntCache(cc->LookAndFeelCache());
    // This is only ever used once during initialization, and can be cleared
    // now.
    cc->LookAndFeelCache().Clear();
  }
}

nsXPLookAndFeel::~nsXPLookAndFeel() {
  NS_ASSERTION(sInstance == this,
               "This destroying instance isn't the singleton instance");
  sInstance = nullptr;
}

bool nsXPLookAndFeel::IsSpecialColor(ColorID aID, nscolor& aColor) {
  switch (aID) {
    case ColorID::TextSelectForeground:
      return (aColor == NS_DONT_CHANGE_COLOR);
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
      /*
       * In GetColor(), every color that is not a special color is color
       * corrected. Use false to make other colors color corrected.
       */
      return false;
  }
  return false;
}

bool nsXPLookAndFeel::ColorIsNotCSSAccessible(ColorID aID) {
  bool result = false;

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
      result = true;
      break;
    default:
      break;
  }

  return result;
}

nscolor nsXPLookAndFeel::GetStandinForNativeColor(ColorID aID) {
  nscolor result = NS_RGB(0xFF, 0xFF, 0xFF);

  // The stand-in colors are taken from the Windows 7 Aero theme
  // except Mac-specific colors which are taken from Mac OS 10.7.
  switch (aID) {
    // CSS 2 colors:
    case ColorID::Activeborder:
      result = NS_RGB(0xB4, 0xB4, 0xB4);
      break;
    case ColorID::Activecaption:
      result = NS_RGB(0x99, 0xB4, 0xD1);
      break;
    case ColorID::Appworkspace:
      result = NS_RGB(0xAB, 0xAB, 0xAB);
      break;
    case ColorID::Background:
      result = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::Buttonface:
      result = NS_RGB(0xF0, 0xF0, 0xF0);
      break;
    case ColorID::Buttonhighlight:
      result = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::Buttonshadow:
      result = NS_RGB(0xA0, 0xA0, 0xA0);
      break;
    case ColorID::Buttontext:
      result = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::Captiontext:
      result = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::Graytext:
      result = NS_RGB(0x6D, 0x6D, 0x6D);
      break;
    case ColorID::Highlight:
      result = NS_RGB(0x33, 0x99, 0xFF);
      break;
    case ColorID::Highlighttext:
      result = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::Inactiveborder:
      result = NS_RGB(0xF4, 0xF7, 0xFC);
      break;
    case ColorID::Inactivecaption:
      result = NS_RGB(0xBF, 0xCD, 0xDB);
      break;
    case ColorID::Inactivecaptiontext:
      result = NS_RGB(0x43, 0x4E, 0x54);
      break;
    case ColorID::Infobackground:
      result = NS_RGB(0xFF, 0xFF, 0xE1);
      break;
    case ColorID::Infotext:
      result = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::Menu:
      result = NS_RGB(0xF0, 0xF0, 0xF0);
      break;
    case ColorID::Menutext:
      result = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::Scrollbar:
      result = NS_RGB(0xC8, 0xC8, 0xC8);
      break;
    case ColorID::Threeddarkshadow:
      result = NS_RGB(0x69, 0x69, 0x69);
      break;
    case ColorID::Threedface:
      result = NS_RGB(0xF0, 0xF0, 0xF0);
      break;
    case ColorID::Threedhighlight:
      result = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::Threedlightshadow:
      result = NS_RGB(0xE3, 0xE3, 0xE3);
      break;
    case ColorID::Threedshadow:
      result = NS_RGB(0xA0, 0xA0, 0xA0);
      break;
    case ColorID::Window:
      result = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::Windowframe:
      result = NS_RGB(0x64, 0x64, 0x64);
      break;
    case ColorID::Windowtext:
      result = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::MozButtondefault:
      result = NS_RGB(0x69, 0x69, 0x69);
      break;
    case ColorID::MozField:
      result = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::MozFieldtext:
      result = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::MozDialog:
      result = NS_RGB(0xF0, 0xF0, 0xF0);
      break;
    case ColorID::MozDialogtext:
      result = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::MozDragtargetzone:
      result = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::MozCellhighlight:
      result = NS_RGB(0xF0, 0xF0, 0xF0);
      break;
    case ColorID::MozCellhighlighttext:
      result = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::MozHtmlCellhighlight:
      result = NS_RGB(0x33, 0x99, 0xFF);
      break;
    case ColorID::MozHtmlCellhighlighttext:
      result = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::MozButtonhoverface:
      result = NS_RGB(0xF0, 0xF0, 0xF0);
      break;
    case ColorID::MozButtonhovertext:
      result = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::MozMenuhover:
      result = NS_RGB(0x33, 0x99, 0xFF);
      break;
    case ColorID::MozMenuhovertext:
      result = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::MozMenubartext:
      result = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::MozMenubarhovertext:
      result = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::MozOddtreerow:
    case ColorID::MozGtkButtonactivetext:
      result = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::MozMacChromeActive:
      result = NS_RGB(0xB2, 0xB2, 0xB2);
      break;
    case ColorID::MozMacChromeInactive:
      result = NS_RGB(0xE1, 0xE1, 0xE1);
      break;
    case ColorID::MozMacFocusring:
      result = NS_RGB(0x60, 0x9D, 0xD7);
      break;
    case ColorID::MozMacMenuselect:
      result = NS_RGB(0x38, 0x75, 0xD7);
      break;
    case ColorID::MozMacMenushadow:
      result = NS_RGB(0xA3, 0xA3, 0xA3);
      break;
    case ColorID::MozMacMenutextdisable:
      result = NS_RGB(0x88, 0x88, 0x88);
      break;
    case ColorID::MozMacMenutextselect:
      result = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::MozMacDisabledtoolbartext:
      result = NS_RGB(0x3F, 0x3F, 0x3F);
      break;
    case ColorID::MozMacSecondaryhighlight:
      result = NS_RGB(0xD4, 0xD4, 0xD4);
      break;
    case ColorID::MozMacVibrancyLight:
    case ColorID::MozMacVibrantTitlebarLight:
      result = NS_RGB(0xf7, 0xf7, 0xf7);
      break;
    case ColorID::MozMacVibrancyDark:
    case ColorID::MozMacVibrantTitlebarDark:
      result = NS_RGB(0x28, 0x28, 0x28);
      break;
    case ColorID::MozMacMenupopup:
      result = NS_RGB(0xe6, 0xe6, 0xe6);
      break;
    case ColorID::MozMacMenuitem:
      result = NS_RGB(0xe6, 0xe6, 0xe6);
      break;
    case ColorID::MozMacActiveMenuitem:
      result = NS_RGB(0x0a, 0x64, 0xdc);
      break;
    case ColorID::MozMacSourceList:
      result = NS_RGB(0xf7, 0xf7, 0xf7);
      break;
    case ColorID::MozMacSourceListSelection:
      result = NS_RGB(0xc8, 0xc8, 0xc8);
      break;
    case ColorID::MozMacActiveSourceListSelection:
      result = NS_RGB(0x0a, 0x64, 0xdc);
      break;
    case ColorID::MozMacTooltip:
      result = NS_RGB(0xf7, 0xf7, 0xf7);
      break;
    case ColorID::MozWinAccentcolor:
      // Seems to be the default color (hardcoded because of bug 1065998)
      result = NS_RGB(0x9E, 0x9E, 0x9E);
      break;
    case ColorID::MozWinAccentcolortext:
      result = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::MozWinMediatext:
      result = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::MozWinCommunicationstext:
      result = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    case ColorID::MozNativehyperlinktext:
      result = NS_RGB(0x00, 0x66, 0xCC);
      break;
    case ColorID::MozComboboxtext:
      result = NS_RGB(0x00, 0x00, 0x00);
      break;
    case ColorID::MozCombobox:
      result = NS_RGB(0xFF, 0xFF, 0xFF);
      break;
    default:
      break;
  }

  return result;
}

//
// All these routines will return NS_OK if they have a value,
// in which case the nsLookAndFeel should use that value;
// otherwise we'll return NS_ERROR_NOT_AVAILABLE, in which case, the
// platform-specific nsLookAndFeel should use its own values instead.
//
nsresult nsXPLookAndFeel::GetColorImpl(ColorID aID,
                                       bool aUseStandinsForNativeColors,
                                       nscolor& aResult) {
  if (!sInitialized) Init();

    // define DEBUG_SYSTEM_COLOR_USE if you want to debug system color
    // use in a skin that uses them.  When set, it will make all system
    // color pairs that are appropriate for foreground/background
    // pairing the same.  This means if the skin is using system colors
    // correctly you will not be able to see *any* text.
#undef DEBUG_SYSTEM_COLOR_USE

#ifdef DEBUG_SYSTEM_COLOR_USE
  {
    nsresult rv = NS_OK;
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

      case ColorID::MozField:
      case ColorID::MozFieldtext:
        aResult = NS_RGB(0xff, 0x00, 0xff);
        break;

      case ColorID::MozDialog:
      case ColorID::MozDialogtext:
        aResult = NS_RGB(0x66, 0x00, 0x66);
        break;

      default:
        rv = NS_ERROR_NOT_AVAILABLE;
    }
    if (NS_SUCCEEDED(rv)) return rv;
  }
#endif  // DEBUG_SYSTEM_COLOR_USE

  if (aUseStandinsForNativeColors &&
      (ColorIsNotCSSAccessible(aID) ||
       !nsContentUtils::UseStandinsForNativeColors())) {
    aUseStandinsForNativeColors = false;
  }

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
    if (sFindbarModalHighlight) {
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

  if (StaticPrefs::ui_use_native_colors() && aUseStandinsForNativeColors) {
    aResult = GetStandinForNativeColor(aID);
    return NS_OK;
  }

  if (StaticPrefs::ui_use_native_colors() &&
      NS_SUCCEEDED(NativeGetColor(aID, aResult))) {
    if (!mozilla::ServoStyleSet::IsInServoTraversal()) {
      MOZ_ASSERT(NS_IsMainThread());
      if ((gfxPlatform::GetCMSMode() == eCMSMode_All) &&
          !IsSpecialColor(aID, aResult)) {
        qcms_transform* transform = gfxPlatform::GetCMSInverseRGBTransform();
        if (transform) {
          uint8_t color[3];
          color[0] = NS_GET_R(aResult);
          color[1] = NS_GET_G(aResult);
          color[2] = NS_GET_B(aResult);
          qcms_transform_data(transform, color, color, 1);
          aResult = NS_RGB(color[0], color[1], color[2]);
        }
      }

      CACHE_COLOR(aID, aResult);
    }
    return NS_OK;
  }

  return NS_ERROR_NOT_AVAILABLE;
}

nsresult nsXPLookAndFeel::GetIntImpl(IntID aID, int32_t& aResult) {
  if (!sInitialized) Init();

  // Set the default values for these prefs. but allow different platforms
  // to override them in their nsLookAndFeel if desired.
  switch (aID) {
    case eIntID_ScrollButtonLeftMouseButtonAction:
      aResult = 0;
      return NS_OK;
    case eIntID_ScrollButtonMiddleMouseButtonAction:
      aResult = 3;
      return NS_OK;
    case eIntID_ScrollButtonRightMouseButtonAction:
      aResult = 3;
      return NS_OK;
    default:
      /*
       * The metrics above are hardcoded platform defaults. All the other
       * metrics are stored in sIntPrefs and can be changed at runtime.
       */
      break;
  }

  for (unsigned int i = 0; i < ArrayLength(sIntPrefs); ++i) {
    if (sIntPrefs[i].isSet && (sIntPrefs[i].id == aID)) {
      aResult = sIntPrefs[i].intVar;
      return NS_OK;
    }
  }

  return NS_ERROR_NOT_AVAILABLE;
}

nsresult nsXPLookAndFeel::GetFloatImpl(FloatID aID, float& aResult) {
  if (!sInitialized) Init();

  for (unsigned int i = 0; i < ArrayLength(sFloatPrefs); ++i) {
    if (sFloatPrefs[i].isSet && sFloatPrefs[i].id == aID) {
      aResult = sFloatPrefs[i].floatVar;
      return NS_OK;
    }
  }

  return NS_ERROR_NOT_AVAILABLE;
}

void nsXPLookAndFeel::RefreshImpl() {
  // Wipe out our color cache.
  uint32_t i;
  for (i = 0; i < uint32_t(ColorID::End); i++) sCachedColors[i] = 0;
  for (i = 0; i < COLOR_CACHE_SIZE; i++) sCachedColorBits[i] = 0;
}

nsTArray<LookAndFeelInt> nsXPLookAndFeel::GetIntCacheImpl() {
  return nsTArray<LookAndFeelInt>();
}

namespace mozilla {

// static
nsresult LookAndFeel::GetColor(ColorID aID, nscolor* aResult) {
  return nsLookAndFeel::GetInstance()->GetColorImpl(aID, false, *aResult);
}

nsresult LookAndFeel::GetColor(ColorID aID, bool aUseStandinsForNativeColors,
                               nscolor* aResult) {
  return nsLookAndFeel::GetInstance()->GetColorImpl(
      aID, aUseStandinsForNativeColors, *aResult);
}

// static
nsresult LookAndFeel::GetInt(IntID aID, int32_t* aResult) {
  return nsLookAndFeel::GetInstance()->GetIntImpl(aID, *aResult);
}

// static
nsresult LookAndFeel::GetFloat(FloatID aID, float* aResult) {
  return nsLookAndFeel::GetInstance()->GetFloatImpl(aID, *aResult);
}

// static
bool LookAndFeel::GetFont(FontID aID, nsString& aName, gfxFontStyle& aStyle) {
  return nsLookAndFeel::GetInstance()->GetFontImpl(aID, aName, aStyle);
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
void LookAndFeel::Refresh() { nsLookAndFeel::GetInstance()->RefreshImpl(); }

// static
void LookAndFeel::NativeInit() { nsLookAndFeel::GetInstance()->NativeInit(); }

// static
nsTArray<LookAndFeelInt> LookAndFeel::GetIntCache() {
  return nsLookAndFeel::GetInstance()->GetIntCacheImpl();
}

// static
void LookAndFeel::SetIntCache(
    const nsTArray<LookAndFeelInt>& aLookAndFeelIntCache) {
  return nsLookAndFeel::GetInstance()->SetIntCacheImpl(aLookAndFeelIntCache);
}

// static
void LookAndFeel::SetShouldRetainCacheForTest(bool aValue) {
  nsLookAndFeel::GetInstance()->SetShouldRetainCacheImplForTest(aValue);
}

}  // namespace mozilla
