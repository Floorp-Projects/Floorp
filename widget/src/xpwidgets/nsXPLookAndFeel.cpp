/* -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"

#include "nsXPLookAndFeel.h"
#include "nsLookAndFeel.h"
#include "nsCRT.h"
#include "nsFont.h"
#include "mozilla/Preferences.h"

#include "gfxPlatform.h"
#include "qcms.h"

#ifdef DEBUG
#include "nsSize.h"
#endif

using namespace mozilla;

NS_IMPL_ISUPPORTS1(nsXPLookAndFeel, nsILookAndFeel)

nsLookAndFeelIntPref nsXPLookAndFeel::sIntPrefs[] =
{
  { "ui.caretBlinkTime", eMetric_CaretBlinkTime, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.caretWidth", eMetric_CaretWidth, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.caretVisibleWithSelection", eMetric_ShowCaretDuringSelection, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.submenuDelay", eMetric_SubmenuDelay, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.dragThresholdX", eMetric_DragThresholdX, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.dragThresholdY", eMetric_DragThresholdY, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.useAccessibilityTheme", eMetric_UseAccessibilityTheme, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.scrollbarsCanOverlapContent", eMetric_ScrollbarsCanOverlapContent,
    PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.menusCanOverlapOSBar", eMetric_MenusCanOverlapOSBar,
    PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.skipNavigatingDisabledMenuItem", eMetric_SkipNavigatingDisabledMenuItem, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.treeOpenDelay",
    eMetric_TreeOpenDelay, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.treeCloseDelay",
    eMetric_TreeCloseDelay, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.treeLazyScrollDelay",
    eMetric_TreeLazyScrollDelay, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.treeScrollDelay",
    eMetric_TreeScrollDelay, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.treeScrollLinesMax",
    eMetric_TreeScrollLinesMax, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "accessibility.tabfocus",
    eMetric_TabFocusModel, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.alertNotificationOrigin",
    eMetric_AlertNotificationOrigin, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.scrollToClick",
    eMetric_ScrollToClick, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.IMERawInputUnderlineStyle",
    eMetric_IMERawInputUnderlineStyle, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.IMESelectedRawTextUnderlineStyle",
    eMetric_IMESelectedRawTextUnderlineStyle, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.IMEConvertedTextUnderlineStyle",
    eMetric_IMEConvertedTextUnderlineStyle, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.IMESelectedConvertedTextUnderlineStyle",
    eMetric_IMESelectedConvertedTextUnderline, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.SpellCheckerUnderlineStyle",
    eMetric_SpellCheckerUnderlineStyle, PR_FALSE, nsLookAndFeelTypeInt, 0 },
};

nsLookAndFeelFloatPref nsXPLookAndFeel::sFloatPrefs[] =
{
  { "ui.IMEUnderlineRelativeSize", eMetricFloat_IMEUnderlineRelativeSize,
    PR_FALSE, nsLookAndFeelTypeFloat, 0 },
  { "ui.SpellCheckerUnderlineRelativeSize",
    eMetricFloat_SpellCheckerUnderlineRelativeSize, PR_FALSE,
    nsLookAndFeelTypeFloat, 0 },
  { "ui.caretAspectRatio", eMetricFloat_CaretAspectRatio, PR_FALSE,
    nsLookAndFeelTypeFloat, 0 },
};


// This array MUST be kept in the same order as the color list in nsILookAndFeel.h.
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
  "ui.-moz_mac_chrome_active",
  "ui.-moz_mac_chrome_inactive",
  "ui.-moz-mac-focusring",
  "ui.-moz-mac-menuselect",
  "ui.-moz-mac-menushadow",
  "ui.-moz-mac-menutextdisable",
  "ui.-moz-mac-menutextselect",
  "ui.-moz_mac_disabledtoolbartext",
  "ui.-moz-mac-alternateprimaryhighlight",
  "ui.-moz-mac-secondaryhighlight",
  "ui.-moz-win-mediatext",
  "ui.-moz-win-communicationstext",
  "ui.-moz-nativehyperlinktext",
  "ui.-moz-comboboxtext",
  "ui.-moz-combobox"
};

PRInt32 nsXPLookAndFeel::sCachedColors[nsILookAndFeel::eColor_LAST_COLOR] = {0};
PRInt32 nsXPLookAndFeel::sCachedColorBits[COLOR_CACHE_SIZE] = {0};

PRBool nsXPLookAndFeel::sInitialized = PR_FALSE;
PRBool nsXPLookAndFeel::sUseNativeColors = PR_TRUE;

nsXPLookAndFeel* nsXPLookAndFeel::sInstance = nsnull;
PRBool nsXPLookAndFeel::sShutdown = PR_FALSE;

// static
nsLookAndFeel*
nsXPLookAndFeel::GetAddRefedInstance()
{
  nsLookAndFeel* lookAndFeel = GetInstance();
  NS_IF_ADDREF(lookAndFeel);
  return lookAndFeel;
}

// static
nsLookAndFeel*
nsXPLookAndFeel::GetInstance()
{
  if (sInstance) {
    return static_cast<nsLookAndFeel*>(sInstance);
  }

  NS_ENSURE_TRUE(!sShutdown, nsnull);

  NS_ADDREF(sInstance = new nsLookAndFeel());
  return static_cast<nsLookAndFeel*>(sInstance);
}

// static
void
nsXPLookAndFeel::Shutdown()
{
  if (sShutdown) {
    return;
  }
  sShutdown = PR_TRUE;
  if (sInstance) {
    sInstance->Release();
  }
}

nsXPLookAndFeel::nsXPLookAndFeel() : nsILookAndFeel()
{
}

// static
void
nsXPLookAndFeel::IntPrefChanged (nsLookAndFeelIntPref *data)
{
  if (!data) {
    return;
  }

  PRInt32 intpref;
  nsresult rv = Preferences::GetInt(data->name, &intpref);
  if (NS_FAILED(rv)) {
    return;
  }
  data->intVar = intpref;
  data->isSet = PR_TRUE;
#ifdef DEBUG_akkana
  printf("====== Changed int pref %s to %d\n", data->name, data->intVar);
#endif
}

// static
void
nsXPLookAndFeel::FloatPrefChanged (nsLookAndFeelFloatPref *data)
{
  if (!data) {
    return;
  }

  PRInt32 intpref;
  nsresult rv = Preferences::GetInt(data->name, &intpref);
  if (NS_FAILED(rv)) {
    return;
  }
  data->floatVar = (float)intpref / 100.;
  data->isSet = PR_TRUE;
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
    if (colorStr[0] == PRUnichar('#')) {
      if (NS_HexToRGB(nsDependentString(
                        Substring(colorStr, 1, colorStr.Length() - 1)),
                      &thecolor)) {
        PRInt32 id = NS_PTR_TO_INT32(index);
        CACHE_COLOR(id, thecolor);
      }
    } else if (NS_ColorNameToRGB(colorStr, &thecolor)) {
      PRInt32 id = NS_PTR_TO_INT32(index);
      CACHE_COLOR(id, thecolor);
#ifdef DEBUG_akkana
      printf("====== Changed color pref %s to 0x%lx\n",
             prefName, thecolor);
#endif
    }
  } else {
    // Reset to the default color, by clearing the cache
    // to force lookup when the color is next used
    PRInt32 id = NS_PTR_TO_INT32(index);
    CLEAR_COLOR_CACHE(id);
  }
}

void
nsXPLookAndFeel::InitFromPref(nsLookAndFeelIntPref* aPref)
{
  PRInt32 intpref;
  nsresult rv = Preferences::GetInt(aPref->name, &intpref);
  if (NS_SUCCEEDED(rv)) {
    aPref->isSet = PR_TRUE;
    aPref->intVar = intpref;
  }
}

void
nsXPLookAndFeel::InitFromPref(nsLookAndFeelFloatPref* aPref)
{
  PRInt32 intpref;
  nsresult rv = Preferences::GetInt(aPref->name, &intpref);
  if (NS_SUCCEEDED(rv)) {
    aPref->isSet = PR_TRUE;
    aPref->floatVar = (float)intpref / 100.;
  }
}

void
nsXPLookAndFeel::InitColorFromPref(PRInt32 i)
{
  nsAutoString colorStr;
  nsresult rv = Preferences::GetString(sColorPrefs[i], &colorStr);
  if (NS_FAILED(rv) || colorStr.IsEmpty()) {
    return;
  }
  nscolor thecolor;
  if (colorStr[0] == PRUnichar('#')) {
    nsAutoString hexString;
    colorStr.Right(hexString, colorStr.Length() - 1);
    if (NS_HexToRGB(hexString, &thecolor)) {
      CACHE_COLOR(i, thecolor);
    }
  } else if (NS_ColorNameToRGB(colorStr, &thecolor)) {
    CACHE_COLOR(i, thecolor);
  }
}

// static
int
nsXPLookAndFeel::OnPrefChanged(const char* aPref, void* aClosure)
{

  // looping in the same order as in ::Init

  nsDependentCString prefName(aPref);
  unsigned int i;
  for (i = 0; i < NS_ARRAY_LENGTH(sIntPrefs); ++i) {
    if (prefName.Equals(sIntPrefs[i].name)) {
      IntPrefChanged(&sIntPrefs[i]);
      return 0;
    }
  }

  for (i = 0; i < NS_ARRAY_LENGTH(sFloatPrefs); ++i) {
    if (prefName.Equals(sFloatPrefs[i].name)) {
      FloatPrefChanged(&sFloatPrefs[i]);
      return 0;
    }
  }

  for (i = 0; i < NS_ARRAY_LENGTH(sColorPrefs); ++i) {
    if (prefName.Equals(sColorPrefs[i])) {
      ColorPrefChanged(i, sColorPrefs[i]);
      return 0;
    }
  }

  return 0;
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
  sInitialized = PR_TRUE;

  // XXX If we could reorganize the pref names, we should separate the branch
  //     for each types.  Then, we could reduce the unnecessary loop from
  //     nsXPLookAndFeel::OnPrefChanged().
  Preferences::RegisterCallback(OnPrefChanged, "ui.");
  Preferences::RegisterCallback(OnPrefChanged, "accessibility.tabfocus");

  unsigned int i;
  for (i = 0; i < NS_ARRAY_LENGTH(sIntPrefs); ++i) {
    InitFromPref(&sIntPrefs[i]);
  }

  for (i = 0; i < NS_ARRAY_LENGTH(sFloatPrefs); ++i) {
    InitFromPref(&sFloatPrefs[i]);
  }

  for (i = 0; i < NS_ARRAY_LENGTH(sColorPrefs); ++i) {
    InitColorFromPref(i);
  }

  PRBool val;
  if (NS_SUCCEEDED(Preferences::GetBool("ui.use_native_colors", &val))) {
    sUseNativeColors = val;
  }
}

nsXPLookAndFeel::~nsXPLookAndFeel()
{
  NS_ASSERTION(sInstance == this,
               "This destroying instance isn't the singleton instance");
  sInstance = nsnull;
}

PRBool
nsXPLookAndFeel::IsSpecialColor(const nsColorID aID, nscolor &aColor)
{
  switch (aID) {
    case eColor_TextSelectForeground:
      return (aColor == NS_DONT_CHANGE_COLOR);
    case eColor_IMESelectedRawTextBackground:
    case eColor_IMESelectedConvertedTextBackground:
    case eColor_IMERawInputBackground:
    case eColor_IMEConvertedTextBackground:
    case eColor_IMESelectedRawTextForeground:
    case eColor_IMESelectedConvertedTextForeground:
    case eColor_IMERawInputForeground:
    case eColor_IMEConvertedTextForeground:
    case eColor_IMERawInputUnderline:
    case eColor_IMEConvertedTextUnderline:
    case eColor_IMESelectedRawTextUnderline:
    case eColor_IMESelectedConvertedTextUnderline:
    case eColor_SpellCheckerUnderline:
      return NS_IS_SELECTION_SPECIAL_COLOR(aColor);
    default:
      /*
       * In GetColor(), every color that is not a special color is color
       * corrected. Use PR_FALSE to make other colors color corrected.
       */
      return PR_FALSE;
  }
  return PR_FALSE;
}

//
// All these routines will return NS_OK if they have a value,
// in which case the nsLookAndFeel should use that value;
// otherwise we'll return NS_ERROR_NOT_AVAILABLE, in which case, the
// platform-specific nsLookAndFeel should use its own values instead.
//
NS_IMETHODIMP
nsXPLookAndFeel::GetColor(const nsColorID aID, nscolor &aColor)
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
      case eColor_activecaption:
          // active window caption background
      case eColor_captiontext:
          // text in active window caption
        aColor = NS_RGB(0xff, 0x00, 0x00);
        break;

      case eColor_highlight:
          // background of selected item
      case eColor_highlighttext:
          // text of selected item
        aColor = NS_RGB(0xff, 0xff, 0x00);
        break;

      case eColor_inactivecaption:
          // inactive window caption
      case eColor_inactivecaptiontext:
          // text in inactive window caption
        aColor = NS_RGB(0x66, 0x66, 0x00);
        break;

      case eColor_infobackground:
          // tooltip background color
      case eColor_infotext:
          // tooltip text color
        aColor = NS_RGB(0x00, 0xff, 0x00);
        break;

      case eColor_menu:
          // menu background
      case eColor_menutext:
          // menu text
        aColor = NS_RGB(0x00, 0xff, 0xff);
        break;

      case eColor_threedface:
      case eColor_buttonface:
          // 3-D face color
      case eColor_buttontext:
          // text on push buttons
        aColor = NS_RGB(0x00, 0x66, 0x66);
        break;

      case eColor_window:
      case eColor_windowtext:
        aColor = NS_RGB(0x00, 0x00, 0xff);
        break;

      // from the CSS3 working draft (not yet finalized)
      // http://www.w3.org/tr/2000/wd-css3-userint-20000216.html#color

      case eColor__moz_field:
      case eColor__moz_fieldtext:
        aColor = NS_RGB(0xff, 0x00, 0xff);
        break;

      case eColor__moz_dialog:
      case eColor__moz_dialogtext:
        aColor = NS_RGB(0x66, 0x00, 0x66);
        break;

      default:
        rv = NS_ERROR_NOT_AVAILABLE;
    }
    if (NS_SUCCEEDED(rv))
      return rv;
  }
#endif // DEBUG_SYSTEM_COLOR_USE

  if (IS_COLOR_CACHED(aID)) {
    aColor = sCachedColors[aID];
    return NS_OK;
  }

  // There are no system color settings for these, so set them manually
  if (aID == eColor_TextSelectBackgroundDisabled) {
    // This is used to gray out the selection when it's not focused
    // Used with nsISelectionController::SELECTION_DISABLED
    aColor = NS_RGB(0xb0, 0xb0, 0xb0);
    return NS_OK;
  }

  if (aID == eColor_TextSelectBackgroundAttention) {
    // This makes the selection stand out when typeaheadfind is on
    // Used with nsISelectionController::SELECTION_ATTENTION
    aColor = NS_RGB(0x38, 0xd8, 0x78);
    return NS_OK;
  }

  if (aID == eColor_TextHighlightBackground) {
    // This makes the matched text stand out when findbar highlighting is on
    // Used with nsISelectionController::SELECTION_FIND
    aColor = NS_RGB(0xef, 0x0f, 0xff);
    return NS_OK;
  }

  if (aID == eColor_TextHighlightForeground) {
    // The foreground color for the matched text in findbar highlighting
    // Used with nsISelectionController::SELECTION_FIND
    aColor = NS_RGB(0xff, 0xff, 0xff);
    return NS_OK;
  }

  if (sUseNativeColors && NS_SUCCEEDED(NativeGetColor(aID, aColor))) {
    if ((gfxPlatform::GetCMSMode() == eCMSMode_All) && !IsSpecialColor(aID, aColor)) {
      qcms_transform *transform = gfxPlatform::GetCMSInverseRGBTransform();
      if (transform) {
        PRUint8 color[3];
        color[0] = NS_GET_R(aColor);
        color[1] = NS_GET_G(aColor);
        color[2] = NS_GET_B(aColor);
        qcms_transform_data(transform, color, color, 1);
        aColor = NS_RGB(color[0], color[1], color[2]);
      }
    }

    CACHE_COLOR(aID, aColor);
    return NS_OK;
  }

  return NS_ERROR_NOT_AVAILABLE;
}
  
NS_IMETHODIMP
nsXPLookAndFeel::GetMetric(const nsMetricID aID, PRInt32& aMetric)
{
  if (!sInitialized)
    Init();

  // Set the default values for these prefs. but allow different platforms
  // to override them in their nsLookAndFeel if desired.
  switch (aID) {
    case eMetric_ScrollButtonLeftMouseButtonAction:
      aMetric = 0;
      return NS_OK;
    case eMetric_ScrollButtonMiddleMouseButtonAction:
      aMetric = 3;
      return NS_OK;
    case eMetric_ScrollButtonRightMouseButtonAction:
      aMetric = 3;
      return NS_OK;
    default:
      /*
       * The metrics above are hardcoded platform defaults. All the other
       * metrics are stored in sIntPrefs and can be changed at runtime.
       */
    break;
  }

  for (unsigned int i = 0; i < NS_ARRAY_LENGTH(sIntPrefs); ++i) {
    if (sIntPrefs[i].isSet && (sIntPrefs[i].id == aID)) {
      aMetric = sIntPrefs[i].intVar;
      return NS_OK;
    }
  }

  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPLookAndFeel::GetMetric(const nsMetricFloatID aID, float& aMetric)
{
  if (!sInitialized)
    Init();

  for (unsigned int i = 0; i < NS_ARRAY_LENGTH(sFloatPrefs); ++i) {
    if (sFloatPrefs[i].isSet && sFloatPrefs[i].id == aID) {
      aMetric = sFloatPrefs[i].floatVar;
      return NS_OK;
    }
  }

  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPLookAndFeel::LookAndFeelChanged()
{
  // Wipe out our color cache.
  PRUint32 i;
  for (i = 0; i < nsILookAndFeel::eColor_LAST_COLOR; i++)
    sCachedColors[i] = 0;
  for (i = 0; i < COLOR_CACHE_SIZE; i++)
    sCachedColorBits[i] = 0;
  return NS_OK;
}


#ifdef DEBUG
  // This method returns the actual (or nearest estimate) 
  // of the Navigator size for a given form control for a given font
  // and font size. This is used in NavQuirks mode to see how closely
  // we match its size
NS_IMETHODIMP
nsXPLookAndFeel::GetNavSize(const nsMetricNavWidgetID aWidgetID,
                            const nsMetricNavFontID   aFontID, 
                            const PRInt32             aFontSize, 
                            nsSize &aSize)
{
  aSize.width  = 0;
  aSize.height = 0;
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

namespace mozilla {

// static
nsresult
LookAndFeel::GetColor(ColorID aID, nscolor* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  return nsLookAndFeel::GetInstance()->GetColor((nsILookAndFeel::nsColorID)aID, *aResult);
}

// static
nsresult
LookAndFeel::GetInt(IntID aID, PRInt32* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  return nsLookAndFeel::GetInstance()->GetMetric((nsILookAndFeel::nsMetricID)aID, *aResult);
}

// static
nsresult
LookAndFeel::GetFloat(FloatID aID, float* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  return nsLookAndFeel::GetInstance()->GetMetric((nsILookAndFeel::nsMetricFloatID)aID, *aResult);
}

// static
PRUnichar
LookAndFeel::GetPasswordCharacter()
{
  return nsLookAndFeel::GetInstance()->GetPasswordCharacter();
}

// static
PRBool
LookAndFeel::GetEchoPassword()
{
  return nsLookAndFeel::GetInstance()->GetEchoPassword();
}

// static
void
LookAndFeel::Refresh()
{
  nsLookAndFeel::GetInstance()->LookAndFeelChanged();
}

} // namespace mozilla
