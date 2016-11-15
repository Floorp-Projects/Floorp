/* -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nscore.h"

#include "nsXPLookAndFeel.h"
#include "nsLookAndFeel.h"
#include "nsCRT.h"
#include "nsFont.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/widget/WidgetMessageUtils.h"

#include "gfxPlatform.h"
#include "qcms.h"

#ifdef DEBUG
#include "nsSize.h"
#endif

using namespace mozilla;

nsLookAndFeelIntPref nsXPLookAndFeel::sIntPrefs[] =
{
  { "ui.caretBlinkTime",
    eIntID_CaretBlinkTime,
    false, 0 },
  { "ui.caretWidth",
    eIntID_CaretWidth,
    false, 0 },
  { "ui.caretVisibleWithSelection",
    eIntID_ShowCaretDuringSelection,
    false, 0 },
  { "ui.submenuDelay",
    eIntID_SubmenuDelay,
    false, 0 },
  { "ui.dragThresholdX",
    eIntID_DragThresholdX,
    false, 0 },
  { "ui.dragThresholdY",
    eIntID_DragThresholdY,
    false, 0 },
  { "ui.useAccessibilityTheme",
    eIntID_UseAccessibilityTheme,
    false, 0 },
  { "ui.menusCanOverlapOSBar",
    eIntID_MenusCanOverlapOSBar,
    false, 0 },
  { "ui.useOverlayScrollbars",
    eIntID_UseOverlayScrollbars,
    false, 0 },
  { "ui.scrollbarDisplayOnMouseMove",
    eIntID_ScrollbarDisplayOnMouseMove,
    false, 0 },
  { "ui.scrollbarFadeBeginDelay",
    eIntID_ScrollbarFadeBeginDelay,
    false, 0 },
  { "ui.scrollbarFadeDuration",
    eIntID_ScrollbarFadeDuration,
    false, 0 },
  { "ui.showHideScrollbars",
    eIntID_ShowHideScrollbars,
    false, 0 },
  { "ui.skipNavigatingDisabledMenuItem",
    eIntID_SkipNavigatingDisabledMenuItem,
    false, 0 },
  { "ui.treeOpenDelay",
    eIntID_TreeOpenDelay,
    false, 0 },
  { "ui.treeCloseDelay",
    eIntID_TreeCloseDelay,
    false, 0 },
  { "ui.treeLazyScrollDelay",
    eIntID_TreeLazyScrollDelay,
    false, 0 },
  { "ui.treeScrollDelay",
    eIntID_TreeScrollDelay,
    false, 0 },
  { "ui.treeScrollLinesMax",
    eIntID_TreeScrollLinesMax,
    false, 0 },
  { "accessibility.tabfocus",
    eIntID_TabFocusModel,
    false, 0 },
  { "ui.alertNotificationOrigin",
    eIntID_AlertNotificationOrigin,
    false, 0 },
  { "ui.scrollToClick",
    eIntID_ScrollToClick,
    false, 0 },
  { "ui.IMERawInputUnderlineStyle",
    eIntID_IMERawInputUnderlineStyle,
    false, 0 },
  { "ui.IMESelectedRawTextUnderlineStyle",
    eIntID_IMESelectedRawTextUnderlineStyle,
    false, 0 },
  { "ui.IMEConvertedTextUnderlineStyle",
    eIntID_IMEConvertedTextUnderlineStyle,
    false, 0 },
  { "ui.IMESelectedConvertedTextUnderlineStyle",
    eIntID_IMESelectedConvertedTextUnderline,
    false, 0 },
  { "ui.SpellCheckerUnderlineStyle",
    eIntID_SpellCheckerUnderlineStyle,
    false, 0 },
  { "ui.scrollbarButtonAutoRepeatBehavior",
    eIntID_ScrollbarButtonAutoRepeatBehavior,
    false, 0 },
  { "ui.tooltipDelay",
    eIntID_TooltipDelay,
    false, 0 },
  { "ui.physicalHomeButton",
    eIntID_PhysicalHomeButton,
    false, 0 },
  { "ui.contextMenuOffsetVertical",
    eIntID_ContextMenuOffsetVertical,
    false, 0 },
  { "ui.contextMenuOffsetHorizontal",
    eIntID_ContextMenuOffsetHorizontal,
    false, 0 }
};

nsLookAndFeelFloatPref nsXPLookAndFeel::sFloatPrefs[] =
{
  { "ui.IMEUnderlineRelativeSize",
    eFloatID_IMEUnderlineRelativeSize,
    false, 0 },
  { "ui.SpellCheckerUnderlineRelativeSize",
    eFloatID_SpellCheckerUnderlineRelativeSize,
    false, 0 },
  { "ui.caretAspectRatio",
    eFloatID_CaretAspectRatio,
    false, 0 },
};


// This array MUST be kept in the same order as the color list in LookAndFeel.h.
/* XXX If you add any strings longer than
 * "ui.IMESelectedConvertedTextBackground"
 * to the following array then you MUST update the
 * sizes of the sColorPrefs array in nsXPLookAndFeel.h
 */
const char nsXPLookAndFeel::sColorPrefs[][38] =
{
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
  "ui.-moz-win-mediatext",
  "ui.-moz-win-communicationstext",
  "ui.-moz-nativehyperlinktext",
  "ui.-moz-comboboxtext",
  "ui.-moz-combobox"
};

int32_t nsXPLookAndFeel::sCachedColors[LookAndFeel::eColorID_LAST_COLOR] = {0};
int32_t nsXPLookAndFeel::sCachedColorBits[COLOR_CACHE_SIZE] = {0};

bool nsXPLookAndFeel::sInitialized = false;
bool nsXPLookAndFeel::sUseNativeColors = true;
bool nsXPLookAndFeel::sUseStandinsForNativeColors = false;
bool nsXPLookAndFeel::sFindbarModalHighlight = false;

nsLookAndFeel* nsXPLookAndFeel::sInstance = nullptr;
bool nsXPLookAndFeel::sShutdown = false;

// static
nsLookAndFeel*
nsXPLookAndFeel::GetInstance()
{
  if (sInstance) {
    return sInstance;
  }

  NS_ENSURE_TRUE(!sShutdown, nullptr);

  sInstance = new nsLookAndFeel();
  return sInstance;
}

// static
void
nsXPLookAndFeel::Shutdown()
{
  if (sShutdown) {
    return;
  }
  sShutdown = true;
  delete sInstance;
  sInstance = nullptr;
}

nsXPLookAndFeel::nsXPLookAndFeel() : LookAndFeel()
{
}

// static
void
nsXPLookAndFeel::IntPrefChanged(nsLookAndFeelIntPref *data)
{
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
void
nsXPLookAndFeel::FloatPrefChanged(nsLookAndFeelFloatPref *data)
{
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
void
nsXPLookAndFeel::ColorPrefChanged (unsigned int index, const char *prefName)
{
  nsAutoString colorStr;
  nsresult rv = Preferences::GetString(prefName, &colorStr);
  if (NS_FAILED(rv)) {
    return;
  }
  if (!colorStr.IsEmpty()) {
    nscolor thecolor;
    if (colorStr[0] == char16_t('#')) {
      if (NS_HexToRGBA(nsDependentString(colorStr, 1),
                       nsHexColorType::NoAlpha, &thecolor)) {
        int32_t id = NS_PTR_TO_INT32(index);
        CACHE_COLOR(id, thecolor);
      }
    } else if (NS_ColorNameToRGB(colorStr, &thecolor)) {
      int32_t id = NS_PTR_TO_INT32(index);
      CACHE_COLOR(id, thecolor);
#ifdef DEBUG_akkana
      printf("====== Changed color pref %s to 0x%lx\n",
             prefName, thecolor);
#endif
    }
  } else {
    // Reset to the default color, by clearing the cache
    // to force lookup when the color is next used
    int32_t id = NS_PTR_TO_INT32(index);
    CLEAR_COLOR_CACHE(id);
  }
}

void
nsXPLookAndFeel::InitFromPref(nsLookAndFeelIntPref* aPref)
{
  int32_t intpref;
  nsresult rv = Preferences::GetInt(aPref->name, &intpref);
  if (NS_SUCCEEDED(rv)) {
    aPref->isSet = true;
    aPref->intVar = intpref;
  }
}

void
nsXPLookAndFeel::InitFromPref(nsLookAndFeelFloatPref* aPref)
{
  int32_t intpref;
  nsresult rv = Preferences::GetInt(aPref->name, &intpref);
  if (NS_SUCCEEDED(rv)) {
    aPref->isSet = true;
    aPref->floatVar = (float)intpref / 100.0f;
  }
}

void
nsXPLookAndFeel::InitColorFromPref(int32_t i)
{
  nsAutoString colorStr;
  nsresult rv = Preferences::GetString(sColorPrefs[i], &colorStr);
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
void
nsXPLookAndFeel::OnPrefChanged(const char* aPref, void* aClosure)
{

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
void
nsXPLookAndFeel::Init()
{
  // Say we're already initialized, and take the chance that it might fail;
  // protects against some other process writing to our static variables.
  sInitialized = true;

  // XXX If we could reorganize the pref names, we should separate the branch
  //     for each types.  Then, we could reduce the unnecessary loop from
  //     nsXPLookAndFeel::OnPrefChanged().
  Preferences::RegisterCallback(OnPrefChanged, "ui.");
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

  Preferences::AddBoolVarCache(&sUseNativeColors,
                               "ui.use_native_colors",
                               sUseNativeColors);
  Preferences::AddBoolVarCache(&sUseStandinsForNativeColors,
                               "ui.use_standins_for_native_colors",
                               sUseStandinsForNativeColors);
  Preferences::AddBoolVarCache(&sFindbarModalHighlight,
                               "findbar.modalHighlight",
                               sFindbarModalHighlight);

  if (XRE_IsContentProcess()) {
    mozilla::dom::ContentChild* cc =
      mozilla::dom::ContentChild::GetSingleton();

    nsTArray<LookAndFeelInt> lookAndFeelIntCache;
    cc->SendGetLookAndFeelCache(&lookAndFeelIntCache);
    LookAndFeel::SetIntCache(lookAndFeelIntCache);
  }
}

nsXPLookAndFeel::~nsXPLookAndFeel()
{
  NS_ASSERTION(sInstance == this,
               "This destroying instance isn't the singleton instance");
  sInstance = nullptr;
}

bool
nsXPLookAndFeel::IsSpecialColor(ColorID aID, nscolor &aColor)
{
  switch (aID) {
    case eColorID_TextSelectForeground:
      return (aColor == NS_DONT_CHANGE_COLOR);
    case eColorID_IMESelectedRawTextBackground:
    case eColorID_IMESelectedConvertedTextBackground:
    case eColorID_IMERawInputBackground:
    case eColorID_IMEConvertedTextBackground:
    case eColorID_IMESelectedRawTextForeground:
    case eColorID_IMESelectedConvertedTextForeground:
    case eColorID_IMERawInputForeground:
    case eColorID_IMEConvertedTextForeground:
    case eColorID_IMERawInputUnderline:
    case eColorID_IMEConvertedTextUnderline:
    case eColorID_IMESelectedRawTextUnderline:
    case eColorID_IMESelectedConvertedTextUnderline:
    case eColorID_SpellCheckerUnderline:
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

bool
nsXPLookAndFeel::ColorIsNotCSSAccessible(ColorID aID)
{
  bool result = false;

  switch (aID) {
    case eColorID_WindowBackground:
    case eColorID_WindowForeground:
    case eColorID_WidgetBackground:
    case eColorID_WidgetForeground:
    case eColorID_WidgetSelectBackground:
    case eColorID_WidgetSelectForeground:
    case eColorID_Widget3DHighlight:
    case eColorID_Widget3DShadow:
    case eColorID_TextBackground:
    case eColorID_TextForeground:
    case eColorID_TextSelectBackground:
    case eColorID_TextSelectForeground:
    case eColorID_TextSelectBackgroundDisabled:
    case eColorID_TextSelectBackgroundAttention:
    case eColorID_TextHighlightBackground:
    case eColorID_TextHighlightForeground:
    case eColorID_IMERawInputBackground:
    case eColorID_IMERawInputForeground:
    case eColorID_IMERawInputUnderline:
    case eColorID_IMESelectedRawTextBackground:
    case eColorID_IMESelectedRawTextForeground:
    case eColorID_IMESelectedRawTextUnderline:
    case eColorID_IMEConvertedTextBackground:
    case eColorID_IMEConvertedTextForeground:
    case eColorID_IMEConvertedTextUnderline:
    case eColorID_IMESelectedConvertedTextBackground:
    case eColorID_IMESelectedConvertedTextForeground:
    case eColorID_IMESelectedConvertedTextUnderline:
    case eColorID_SpellCheckerUnderline:
      result = true;
      break;
    default:
      break;
  }

  return result;
}

nscolor
nsXPLookAndFeel::GetStandinForNativeColor(ColorID aID)
{
  nscolor result = NS_RGB(0xFF, 0xFF, 0xFF);

  // The stand-in colors are taken from the Windows 7 Aero theme
  // except Mac-specific colors which are taken from Mac OS 10.7.
  switch (aID) {
    // CSS 2 colors:
    case eColorID_activeborder:      result = NS_RGB(0xB4, 0xB4, 0xB4); break;
    case eColorID_activecaption:     result = NS_RGB(0x99, 0xB4, 0xD1); break;
    case eColorID_appworkspace:      result = NS_RGB(0xAB, 0xAB, 0xAB); break;
    case eColorID_background:        result = NS_RGB(0x00, 0x00, 0x00); break;
    case eColorID_buttonface:        result = NS_RGB(0xF0, 0xF0, 0xF0); break;
    case eColorID_buttonhighlight:   result = NS_RGB(0xFF, 0xFF, 0xFF); break;
    case eColorID_buttonshadow:      result = NS_RGB(0xA0, 0xA0, 0xA0); break;
    case eColorID_buttontext:        result = NS_RGB(0x00, 0x00, 0x00); break;
    case eColorID_captiontext:       result = NS_RGB(0x00, 0x00, 0x00); break;
    case eColorID_graytext:          result = NS_RGB(0x6D, 0x6D, 0x6D); break;
    case eColorID_highlight:         result = NS_RGB(0x33, 0x99, 0xFF); break;
    case eColorID_highlighttext:     result = NS_RGB(0xFF, 0xFF, 0xFF); break;
    case eColorID_inactiveborder:    result = NS_RGB(0xF4, 0xF7, 0xFC); break;
    case eColorID_inactivecaption:   result = NS_RGB(0xBF, 0xCD, 0xDB); break;
    case eColorID_inactivecaptiontext:
      result = NS_RGB(0x43, 0x4E, 0x54); break;
    case eColorID_infobackground:    result = NS_RGB(0xFF, 0xFF, 0xE1); break;
    case eColorID_infotext:          result = NS_RGB(0x00, 0x00, 0x00); break;
    case eColorID_menu:              result = NS_RGB(0xF0, 0xF0, 0xF0); break;
    case eColorID_menutext:          result = NS_RGB(0x00, 0x00, 0x00); break;
    case eColorID_scrollbar:         result = NS_RGB(0xC8, 0xC8, 0xC8); break;
    case eColorID_threeddarkshadow:  result = NS_RGB(0x69, 0x69, 0x69); break;
    case eColorID_threedface:        result = NS_RGB(0xF0, 0xF0, 0xF0); break;
    case eColorID_threedhighlight:   result = NS_RGB(0xFF, 0xFF, 0xFF); break;
    case eColorID_threedlightshadow: result = NS_RGB(0xE3, 0xE3, 0xE3); break;
    case eColorID_threedshadow:      result = NS_RGB(0xA0, 0xA0, 0xA0); break;
    case eColorID_window:            result = NS_RGB(0xFF, 0xFF, 0xFF); break;
    case eColorID_windowframe:       result = NS_RGB(0x64, 0x64, 0x64); break;
    case eColorID_windowtext:        result = NS_RGB(0x00, 0x00, 0x00); break;
    case eColorID__moz_buttondefault:
      result = NS_RGB(0x69, 0x69, 0x69); break;
    case eColorID__moz_field:        result = NS_RGB(0xFF, 0xFF, 0xFF); break;
    case eColorID__moz_fieldtext:    result = NS_RGB(0x00, 0x00, 0x00); break;
    case eColorID__moz_dialog:       result = NS_RGB(0xF0, 0xF0, 0xF0); break;
    case eColorID__moz_dialogtext:   result = NS_RGB(0x00, 0x00, 0x00); break;
    case eColorID__moz_dragtargetzone:
      result = NS_RGB(0xFF, 0xFF, 0xFF); break;
    case eColorID__moz_cellhighlight:
      result = NS_RGB(0xF0, 0xF0, 0xF0); break;
    case eColorID__moz_cellhighlighttext:
      result = NS_RGB(0x00, 0x00, 0x00); break;
    case eColorID__moz_html_cellhighlight:
      result = NS_RGB(0x33, 0x99, 0xFF); break;
    case eColorID__moz_html_cellhighlighttext:
      result = NS_RGB(0xFF, 0xFF, 0xFF); break;
    case eColorID__moz_buttonhoverface:
      result = NS_RGB(0xF0, 0xF0, 0xF0); break;
    case eColorID__moz_buttonhovertext:
      result = NS_RGB(0x00, 0x00, 0x00); break;
    case eColorID__moz_menuhover:
      result = NS_RGB(0x33, 0x99, 0xFF); break;
    case eColorID__moz_menuhovertext:
      result = NS_RGB(0x00, 0x00, 0x00); break;
    case eColorID__moz_menubartext:
      result = NS_RGB(0x00, 0x00, 0x00); break;
    case eColorID__moz_menubarhovertext:
      result = NS_RGB(0x00, 0x00, 0x00); break;
    case eColorID__moz_oddtreerow:
      result = NS_RGB(0xFF, 0xFF, 0xFF); break;
    case eColorID__moz_mac_chrome_active:
      result = NS_RGB(0xB2, 0xB2, 0xB2); break;
    case eColorID__moz_mac_chrome_inactive:
      result = NS_RGB(0xE1, 0xE1, 0xE1); break;
    case eColorID__moz_mac_focusring:
      result = NS_RGB(0x60, 0x9D, 0xD7); break;
    case eColorID__moz_mac_menuselect:
      result = NS_RGB(0x38, 0x75, 0xD7); break;
    case eColorID__moz_mac_menushadow:
      result = NS_RGB(0xA3, 0xA3, 0xA3); break;
    case eColorID__moz_mac_menutextdisable:
      result = NS_RGB(0x88, 0x88, 0x88); break;
    case eColorID__moz_mac_menutextselect:
      result = NS_RGB(0xFF, 0xFF, 0xFF); break;
    case eColorID__moz_mac_disabledtoolbartext:
      result = NS_RGB(0x3F, 0x3F, 0x3F); break;
    case eColorID__moz_mac_secondaryhighlight:
      result = NS_RGB(0xD4, 0xD4, 0xD4); break;
    case eColorID__moz_win_mediatext:
      result = NS_RGB(0xFF, 0xFF, 0xFF); break;
    case eColorID__moz_win_communicationstext:
      result = NS_RGB(0xFF, 0xFF, 0xFF); break;
    case eColorID__moz_nativehyperlinktext:
      result = NS_RGB(0x00, 0x66, 0xCC); break;
    case eColorID__moz_comboboxtext:
      result = NS_RGB(0x00, 0x00, 0x00); break;
    case eColorID__moz_combobox:
      result = NS_RGB(0xFF, 0xFF, 0xFF); break;
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
nsresult
nsXPLookAndFeel::GetColorImpl(ColorID aID, bool aUseStandinsForNativeColors,
                              nscolor &aResult)
{
  if (!sInitialized)
    Init();

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
      case eColorID_activecaption:
          // active window caption background
      case eColorID_captiontext:
          // text in active window caption
        aResult = NS_RGB(0xff, 0x00, 0x00);
        break;

      case eColorID_highlight:
          // background of selected item
      case eColorID_highlighttext:
          // text of selected item
        aResult = NS_RGB(0xff, 0xff, 0x00);
        break;

      case eColorID_inactivecaption:
          // inactive window caption
      case eColorID_inactivecaptiontext:
          // text in inactive window caption
        aResult = NS_RGB(0x66, 0x66, 0x00);
        break;

      case eColorID_infobackground:
          // tooltip background color
      case eColorID_infotext:
          // tooltip text color
        aResult = NS_RGB(0x00, 0xff, 0x00);
        break;

      case eColorID_menu:
          // menu background
      case eColorID_menutext:
          // menu text
        aResult = NS_RGB(0x00, 0xff, 0xff);
        break;

      case eColorID_threedface:
      case eColorID_buttonface:
          // 3-D face color
      case eColorID_buttontext:
          // text on push buttons
        aResult = NS_RGB(0x00, 0x66, 0x66);
        break;

      case eColorID_window:
      case eColorID_windowtext:
        aResult = NS_RGB(0x00, 0x00, 0xff);
        break;

      // from the CSS3 working draft (not yet finalized)
      // http://www.w3.org/tr/2000/wd-css3-userint-20000216.html#color

      case eColorID__moz_field:
      case eColorID__moz_fieldtext:
        aResult = NS_RGB(0xff, 0x00, 0xff);
        break;

      case eColorID__moz_dialog:
      case eColorID__moz_dialogtext:
        aResult = NS_RGB(0x66, 0x00, 0x66);
        break;

      default:
        rv = NS_ERROR_NOT_AVAILABLE;
    }
    if (NS_SUCCEEDED(rv))
      return rv;
  }
#endif // DEBUG_SYSTEM_COLOR_USE

  if (aUseStandinsForNativeColors &&
      (ColorIsNotCSSAccessible(aID) || !sUseStandinsForNativeColors)) {
    aUseStandinsForNativeColors = false;
  }

  if (!aUseStandinsForNativeColors && IS_COLOR_CACHED(aID)) {
    aResult = sCachedColors[aID];
    return NS_OK;
  }

  // There are no system color settings for these, so set them manually
#ifndef XP_MACOSX
  if (aID == eColorID_TextSelectBackgroundDisabled) {
    // This is used to gray out the selection when it's not focused
    // Used with nsISelectionController::SELECTION_DISABLED
    aResult = NS_RGB(0xb0, 0xb0, 0xb0);
    return NS_OK;
  }
#endif

  if (aID == eColorID_TextSelectBackgroundAttention) {
    if (sFindbarModalHighlight) {
      aResult = NS_RGBA(0, 0, 0, 0);
      return NS_OK;
    }

    // This makes the selection stand out when typeaheadfind is on
    // Used with nsISelectionController::SELECTION_ATTENTION
    aResult = NS_RGB(0x38, 0xd8, 0x78);
    return NS_OK;
  }

  if (aID == eColorID_TextHighlightBackground) {
    // This makes the matched text stand out when findbar highlighting is on
    // Used with nsISelectionController::SELECTION_FIND
    aResult = NS_RGB(0xef, 0x0f, 0xff);
    return NS_OK;
  }

  if (aID == eColorID_TextHighlightForeground) {
    // The foreground color for the matched text in findbar highlighting
    // Used with nsISelectionController::SELECTION_FIND
    aResult = NS_RGB(0xff, 0xff, 0xff);
    return NS_OK;
  }

  if (sUseNativeColors && aUseStandinsForNativeColors) {
    aResult = GetStandinForNativeColor(aID);
    return NS_OK;
  }

  if (sUseNativeColors && NS_SUCCEEDED(NativeGetColor(aID, aResult))) {
    if ((gfxPlatform::GetCMSMode() == eCMSMode_All) &&
         !IsSpecialColor(aID, aResult)) {
      qcms_transform *transform = gfxPlatform::GetCMSInverseRGBTransform();
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
    return NS_OK;
  }

  return NS_ERROR_NOT_AVAILABLE;
}

nsresult
nsXPLookAndFeel::GetIntImpl(IntID aID, int32_t &aResult)
{
  if (!sInitialized)
    Init();

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

nsresult
nsXPLookAndFeel::GetFloatImpl(FloatID aID, float &aResult)
{
  if (!sInitialized)
    Init();

  for (unsigned int i = 0; i < ArrayLength(sFloatPrefs); ++i) {
    if (sFloatPrefs[i].isSet && sFloatPrefs[i].id == aID) {
      aResult = sFloatPrefs[i].floatVar;
      return NS_OK;
    }
  }

  return NS_ERROR_NOT_AVAILABLE;
}

void
nsXPLookAndFeel::RefreshImpl()
{
  // Wipe out our color cache.
  uint32_t i;
  for (i = 0; i < eColorID_LAST_COLOR; i++)
    sCachedColors[i] = 0;
  for (i = 0; i < COLOR_CACHE_SIZE; i++)
    sCachedColorBits[i] = 0;
}

nsTArray<LookAndFeelInt>
nsXPLookAndFeel::GetIntCacheImpl()
{
  return nsTArray<LookAndFeelInt>();
}

namespace mozilla {

// static
nsresult
LookAndFeel::GetColor(ColorID aID, nscolor* aResult)
{
  return nsLookAndFeel::GetInstance()->GetColorImpl(aID, false, *aResult);
}

nsresult
LookAndFeel::GetColor(ColorID aID, bool aUseStandinsForNativeColors,
                      nscolor* aResult)
{
  return nsLookAndFeel::GetInstance()->GetColorImpl(aID,
                                       aUseStandinsForNativeColors, *aResult);
}

// static
nsresult
LookAndFeel::GetInt(IntID aID, int32_t* aResult)
{
  return nsLookAndFeel::GetInstance()->GetIntImpl(aID, *aResult);
}

// static
nsresult
LookAndFeel::GetFloat(FloatID aID, float* aResult)
{
  return nsLookAndFeel::GetInstance()->GetFloatImpl(aID, *aResult);
}

// static
bool
LookAndFeel::GetFont(FontID aID, nsString& aName, gfxFontStyle& aStyle,
                     float aDevPixPerCSSPixel)
{
  return nsLookAndFeel::GetInstance()->GetFontImpl(aID, aName, aStyle,
                                                   aDevPixPerCSSPixel);
}

// static
char16_t
LookAndFeel::GetPasswordCharacter()
{
  return nsLookAndFeel::GetInstance()->GetPasswordCharacterImpl();
}

// static
bool
LookAndFeel::GetEchoPassword()
{
  return nsLookAndFeel::GetInstance()->GetEchoPasswordImpl();
}

// static
uint32_t
LookAndFeel::GetPasswordMaskDelay()
{
  return nsLookAndFeel::GetInstance()->GetPasswordMaskDelayImpl();
}

// static
void
LookAndFeel::Refresh()
{
  nsLookAndFeel::GetInstance()->RefreshImpl();
}

// static
nsTArray<LookAndFeelInt>
LookAndFeel::GetIntCache()
{
  return nsLookAndFeel::GetInstance()->GetIntCacheImpl();
}

// static
void
LookAndFeel::SetIntCache(const nsTArray<LookAndFeelInt>& aLookAndFeelIntCache)
{
  return nsLookAndFeel::GetInstance()->SetIntCacheImpl(aLookAndFeelIntCache);
}

} // namespace mozilla
