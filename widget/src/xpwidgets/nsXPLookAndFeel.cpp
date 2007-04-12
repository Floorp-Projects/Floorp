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
#include "nsIServiceManager.h"
#include "nsIPrefBranch2.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIObserver.h"
#include "nsCRT.h"
#include "nsFont.h"

#ifdef DEBUG
#include "nsSize.h"
#endif
 
NS_IMPL_ISUPPORTS2(nsXPLookAndFeel, nsILookAndFeel, nsIObserver)

nsLookAndFeelIntPref nsXPLookAndFeel::sIntPrefs[] =
{
  { "ui.windowTitleHeight", eMetric_WindowTitleHeight, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.windowBorderWidth", eMetric_WindowBorderWidth, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.windowBorderHeight", eMetric_WindowBorderHeight, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.widget3DBorder", eMetric_Widget3DBorder, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.textFieldBorder", eMetric_TextFieldBorder, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.textFieldHeight", eMetric_TextFieldHeight, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.buttonHorizontalInsidePaddingNavQuirks",
    eMetric_ButtonHorizontalInsidePaddingNavQuirks, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.buttonHorizontalInsidePaddingOffsetNavQuirks",
    eMetric_ButtonHorizontalInsidePaddingOffsetNavQuirks, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.checkboxSize", eMetric_CheckboxSize, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.radioboxSize", eMetric_RadioboxSize, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.textHorizontalInsideMinimumPadding",
    eMetric_TextHorizontalInsideMinimumPadding, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.textVerticalInsidePadding", eMetric_TextVerticalInsidePadding,
    PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.textShouldUseVerticalInsidePadding",
    eMetric_TextShouldUseVerticalInsidePadding, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.textShouldUseHorizontalInsideMinimumPadding",
    eMetric_TextShouldUseHorizontalInsideMinimumPadding, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.listShouldUseHorizontalInsideMinimumPadding",
    eMetric_ListShouldUseHorizontalInsideMinimumPadding, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.listHorizontalInsideMinimumPadding",
    eMetric_ListHorizontalInsideMinimumPadding, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.listShouldUseVerticalInsidePadding",
    eMetric_ListShouldUseVerticalInsidePadding, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.listVerticalInsidePadding", eMetric_ListVerticalInsidePadding,
    PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.caretBlinkTime", eMetric_CaretBlinkTime, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.caretWidth", eMetric_CaretWidth, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.caretVisibleWithSelection", eMetric_ShowCaretDuringSelection, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.submenuDelay", eMetric_SubmenuDelay, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.dragFullWindow", eMetric_DragFullWindow, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.dragThresholdX", eMetric_DragThresholdX, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.dragThresholdY", eMetric_DragThresholdY, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.useAccessibilityTheme", eMetric_UseAccessibilityTheme, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.isScreenReaderActive", eMetric_IsScreenReaderActive, PR_FALSE, nsLookAndFeelTypeInt, 0 },
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
};

nsLookAndFeelFloatPref nsXPLookAndFeel::sFloatPrefs[] =
{
  { "ui.textFieldVerticalInsidePadding",
    eMetricFloat_TextFieldVerticalInsidePadding, PR_FALSE, nsLookAndFeelTypeFloat, 0 },
  { "ui.textFieldHorizontalInsidePadding",
    eMetricFloat_TextFieldHorizontalInsidePadding, PR_FALSE, nsLookAndFeelTypeFloat, 0 },
  { "ui.textAreaVerticalInsidePadding", eMetricFloat_TextAreaVerticalInsidePadding,
    PR_FALSE, nsLookAndFeelTypeFloat, 0 },
  { "ui.textAreaHorizontalInsidePadding",
    eMetricFloat_TextAreaHorizontalInsidePadding, PR_FALSE, nsLookAndFeelTypeFloat, 0 },
  { "ui.listVerticalInsidePadding",
    eMetricFloat_ListVerticalInsidePadding, PR_FALSE, nsLookAndFeelTypeFloat, 0 },
  { "ui.listHorizontalInsidePadding",
    eMetricFloat_ListHorizontalInsidePadding, PR_FALSE, nsLookAndFeelTypeFloat, 0 },
  { "ui.buttonVerticalInsidePadding", eMetricFloat_ButtonVerticalInsidePadding,
    PR_FALSE, nsLookAndFeelTypeFloat, 0 },
  { "ui.buttonHorizontalInsidePadding", eMetricFloat_ButtonHorizontalInsidePadding,
    PR_FALSE, nsLookAndFeelTypeFloat, 0 },
  { "ui.IMEUnderlineRelativeSize", eMetricFloat_IMEUnderlineRelativeSize,
    PR_FALSE, nsLookAndFeelTypeFloat, 0 },
};


// This array MUST be kept in the same order as the color list in nsILookAndFeel.h.
/* XXX If you add any strings longer than
 * "ui.-moz-mac-accentlightesthighlight"
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
  "ui.-moz-mac-focusring",
  "ui.-moz-mac-menuselect",
  "ui.-moz-mac-menushadow",
  "ui.-moz-mac-menutextdisable",
  "ui.-moz-mac-menutextselect",
  "ui.-moz-mac-accentlightesthighlight",
  "ui.-moz-mac-accentregularhighlight",
  "ui.-moz-mac-accentface",
  "ui.-moz-mac-accentlightshadow",
  "ui.-moz-mac-accentregularshadow",
  "ui.-moz-mac-accentdarkshadow",
  "ui.-moz-mac-accentdarkestshadow"
};

PRInt32 nsXPLookAndFeel::sCachedColors[nsILookAndFeel::eColor_LAST_COLOR] = {0};
PRInt32 nsXPLookAndFeel::sCachedColorBits[COLOR_CACHE_SIZE] = {0};

PRBool nsXPLookAndFeel::sInitialized = PR_FALSE;

nsXPLookAndFeel::nsXPLookAndFeel() : nsILookAndFeel()
{
}

void
nsXPLookAndFeel::IntPrefChanged (nsLookAndFeelIntPref *data)
{
  if (data)
  {
    nsCOMPtr<nsIPrefBranch> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (prefService)
    {
      PRInt32 intpref;
      nsresult rv = prefService->GetIntPref(data->name, &intpref);
      if (NS_SUCCEEDED(rv))
      {
        data->intVar = intpref;
        data->isSet = PR_TRUE;
#ifdef DEBUG_akkana
        printf("====== Changed int pref %s to %d\n", data->name, data->intVar);
#endif
      }
    }
  }
}

void
nsXPLookAndFeel::FloatPrefChanged (nsLookAndFeelFloatPref *data)
{
  if (data)
  {
    nsCOMPtr<nsIPrefBranch> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (prefService)
    {
      PRInt32 intpref;
      nsresult rv = prefService->GetIntPref(data->name, &intpref);
      if (NS_SUCCEEDED(rv))
      {
        data->floatVar = (float)intpref / 100.;
        data->isSet = PR_TRUE;
#ifdef DEBUG_akkana
        printf("====== Changed float pref %s to %f\n", data->name, data->floatVar);
#endif
      }
    }
  }
}

void
nsXPLookAndFeel::ColorPrefChanged (unsigned int index, const char *prefName)
{
  nsCOMPtr<nsIPrefBranch> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefService) {
    nsXPIDLCString colorStr;
    nsresult rv = prefService->GetCharPref(prefName, getter_Copies(colorStr));
    if (NS_SUCCEEDED(rv) && !colorStr.IsEmpty()) {
      nscolor thecolor;
      if (colorStr[0] == '#') {
        if (NS_SUCCEEDED(NS_HexToRGB(NS_ConvertASCIItoUTF16(Substring(colorStr, 1, colorStr.Length() - 1)),
                                     &thecolor))) {
          PRInt32 id = NS_PTR_TO_INT32(index);
          CACHE_COLOR(id, thecolor);
        }
      }
      else if (NS_SUCCEEDED(NS_ColorNameToRGB(NS_ConvertASCIItoUTF16(colorStr),
                                         &thecolor))) {
        PRInt32 id = NS_PTR_TO_INT32(index);
        CACHE_COLOR(id, thecolor);
#ifdef DEBUG_akkana
        printf("====== Changed color pref %s to 0x%lx\n",
               prefName, thecolor);
#endif
      }
    }
  }
}

void
nsXPLookAndFeel::InitFromPref(nsLookAndFeelIntPref* aPref, nsIPrefBranch* aPrefBranch)
{
  PRInt32 intpref;
  nsresult rv = aPrefBranch->GetIntPref(aPref->name, &intpref);
  if (NS_SUCCEEDED(rv))
  {
    aPref->isSet = PR_TRUE;
    aPref->intVar = intpref;
  }
}

void
nsXPLookAndFeel::InitFromPref(nsLookAndFeelFloatPref* aPref, nsIPrefBranch* aPrefBranch)
{
  PRInt32 intpref;
  nsresult rv = aPrefBranch->GetIntPref(aPref->name, &intpref);
  if (NS_SUCCEEDED(rv))
  {
    aPref->isSet = PR_TRUE;
    aPref->floatVar = (float)intpref / 100.;
  }
}

void
nsXPLookAndFeel::InitColorFromPref(PRInt32 i, nsIPrefBranch* aPrefBranch)
{
  nsXPIDLCString colorStr;
  nsresult rv = aPrefBranch->GetCharPref(sColorPrefs[i], getter_Copies(colorStr));
  if (NS_SUCCEEDED(rv) && !colorStr.IsEmpty())
  {
    nsAutoString colorNSStr; colorNSStr.AssignWithConversion(colorStr);
    nscolor thecolor;
    if (colorNSStr[0] == '#') {
      nsAutoString hexString;
      colorNSStr.Right(hexString, colorNSStr.Length() - 1);
      if (NS_SUCCEEDED(NS_HexToRGB(hexString, &thecolor))) {
        CACHE_COLOR(i, thecolor);
      }
    }
    else if (NS_SUCCEEDED(NS_ColorNameToRGB(colorNSStr, &thecolor)))
    {
      CACHE_COLOR(i, thecolor);
    }
  }
}

NS_IMETHODIMP
nsXPLookAndFeel::Observe(nsISupports*     aSubject,
                         const char*      aTopic,
                         const PRUnichar* aData)
{

  // looping in the same order as in ::Init

  unsigned int i;
  for (i = 0; i < NS_ARRAY_LENGTH(sIntPrefs); ++i) {
    if (nsDependentString(aData).EqualsASCII(sIntPrefs[i].name)) {
      IntPrefChanged(&sIntPrefs[i]);
      return NS_OK;
    }
  }

  for (i = 0; i < NS_ARRAY_LENGTH(sFloatPrefs); ++i) {
    if (nsDependentString(aData).EqualsASCII(sFloatPrefs[i].name)) {
      FloatPrefChanged(&sFloatPrefs[i]);
      return NS_OK;
    }
  }

  for (i = 0; i < NS_ARRAY_LENGTH(sColorPrefs); ++i) {
    if (nsDependentString(aData).EqualsASCII(sColorPrefs[i])) {
      ColorPrefChanged(i, sColorPrefs[i]);
      return NS_OK;
    }
  }

  return NS_OK;
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

  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (!prefs)
    return;
  nsCOMPtr<nsIPrefBranch2> prefBranchInternal(do_QueryInterface(prefs));
  if (!prefBranchInternal)
    return;

  unsigned int i;
  for (i = 0; i < NS_ARRAY_LENGTH(sIntPrefs); ++i) {
    InitFromPref(&sIntPrefs[i], prefs);
    prefBranchInternal->AddObserver(sIntPrefs[i].name, this, PR_FALSE);
  }

  for (i = 0; i < NS_ARRAY_LENGTH(sFloatPrefs); ++i) {
    InitFromPref(&sFloatPrefs[i], prefs);
    prefBranchInternal->AddObserver(sFloatPrefs[i].name, this, PR_FALSE);
  }

  for (i = 0; i < NS_ARRAY_LENGTH(sColorPrefs); ++i) {
    InitColorFromPref(i, prefs);
    prefBranchInternal->AddObserver(sColorPrefs[i], this, PR_FALSE);
  }
}

nsXPLookAndFeel::~nsXPLookAndFeel()
{
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

  if (NS_SUCCEEDED(NativeGetColor(aID, aColor))) {
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
  }

  for (unsigned int i = 0; i < ((sizeof (sIntPrefs) / sizeof (*sIntPrefs))); ++i)
    if (sIntPrefs[i].isSet && (sIntPrefs[i].id == aID))
    {
      aMetric = sIntPrefs[i].intVar;
      return NS_OK;
    }

  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPLookAndFeel::GetMetric(const nsMetricFloatID aID, float& aMetric)
{
  if (!sInitialized)
    Init();

  for (unsigned int i = 0; i < ((sizeof (sFloatPrefs) / sizeof (*sFloatPrefs))); ++i)
    if (sFloatPrefs[i].isSet && sFloatPrefs[i].id == aID)
    {
      aMetric = sFloatPrefs[i].floatVar;
      return NS_OK;
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
