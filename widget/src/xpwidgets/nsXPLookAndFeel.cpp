/* -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsXPLookAndFeel.h"
#include "nsIServiceManager.h"
 
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

NS_IMPL_ISUPPORTS1(nsXPLookAndFeel, nsILookAndFeel)

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
  { "ui.caretWidthTwips", eMetric_SingleLineCaretWidth, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.submenuDelay", eMetric_SubmenuDelay, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.dragFullWindow", eMetric_DragFullWindow, PR_FALSE, nsLookAndFeelTypeInt, 0 },
  { "ui.menusCanOverlapOSBar", eMetric_MenusCanOverlapOSBar,
    PR_FALSE, nsLookAndFeelTypeInt, 0 },
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
};

nsLookAndFeelColorPref nsXPLookAndFeel::sColorPrefs[] =
{
  { "ui.windowBackground",
    eColor_WindowBackground, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.windowForeground",
    eColor_WindowForeground, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.widgetBackground",
    eColor_WidgetBackground, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.widgetForeground",
    eColor_WidgetForeground, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.widgetSelectBackground",
    eColor_WidgetSelectBackground, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.widgetSelectForeground",
    eColor_WidgetSelectForeground, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.widget3DHighlight",
    eColor_Widget3DHighlight, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.widget3DShadow",
    eColor_Widget3DShadow, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.textBackground",
    eColor_TextBackground, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.textForeground",
    eColor_TextForeground, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.textSelectBackground",
    eColor_TextSelectBackground, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.textSelectForeground",
    eColor_TextSelectForeground, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.activeborder",
    eColor_activeborder, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.activecaption",
    eColor_activecaption, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.appworkspace",
    eColor_appworkspace, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.background",
    eColor_background, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.captiontext",
    eColor_captiontext, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.graytext",
    eColor_graytext, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.highlight",
    eColor_highlight, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.highlighttext",
    eColor_highlighttext, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.inactiveborder",
    eColor_inactiveborder, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.inactivecaption",
    eColor_inactivecaption, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.inactivecaptiontext",
    eColor_inactivecaptiontext, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.infobackground",
    eColor_infobackground, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.infotext",
    eColor_infotext, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.menu",
    eColor_menu, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.menutext",
    eColor_menutext, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.scrollbar",
    eColor_scrollbar, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.threedface",
    eColor_threedface, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.buttonface",
    eColor_buttonface, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.buttonhighlight",
    eColor_buttonhighlight, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.threedhighlight",
    eColor_threedhighlight, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.buttontext",
    eColor_buttontext, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.buttonshadow",
    eColor_buttonshadow, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.threeddarkshadow",
    eColor_threeddarkshadow, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.threedshadow",
    eColor_threedshadow, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.threedlightshadow",
    eColor_threedlightshadow, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.window",
    eColor_window, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.windowframe",
    eColor_windowframe, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.windowtext",
    eColor_windowtext, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.-moz-field",
    eColor__moz_field, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.-moz-fieldtext",
    eColor__moz_fieldtext, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.-moz-dialog",
    eColor__moz_dialog, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.-moz-dialogtext",
    eColor__moz_dialogtext, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.-moz-dragtargetzone",
    eColor__moz_dragtargetzone, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.-moz-mac-focusring",
    eColor__moz_mac_focusring, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.-moz-mac-menuselect",
    eColor__moz_mac_menuselect, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.-moz-mac-menushadow",
    eColor__moz_mac_menushadow, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },
  { "ui.-moz-mac-menutextselect",
    eColor__moz_mac_menutextselect, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },     
  { "ui.-moz-mac-accentlightesthighlight",
    eColor__moz_mac_accentlightesthighlight, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },       
  { "ui.-moz-mac-accentregularhighlight",
    eColor__moz_mac_accentregularhighlight, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },  
  { "ui.-moz-mac-accentface",
    eColor__moz_mac_accentface, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },  
  { "ui.-moz-mac-accentlightshadow",
    eColor__moz_mac_accentlightshadow, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },  
  { "ui.-moz-mac-accentregularshadow",
    eColor__moz_mac_accentregularshadow, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },  
  { "ui.-moz-mac-accentdarkshadow",
    eColor__moz_mac_accentdarkshadow, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },  
  { "ui.-moz-mac-accentdarkestshadow",
    eColor__moz_mac_accentdarkestshadow, PR_FALSE, nsLookAndFeelTypeColor, (nscolor)0 },  
        
};

PRBool nsXPLookAndFeel::sInitialized = PR_FALSE;

nsXPLookAndFeel::nsXPLookAndFeel() : nsILookAndFeel()
{
  NS_INIT_REFCNT();
}

static int PR_CALLBACK intPrefChanged (const char *newpref, void *data)
{
  nsLookAndFeelIntPref* np = (nsLookAndFeelIntPref*)data;
  if (np)
  {
    nsresult rv;
    nsCOMPtr<nsIPref> prefService(do_GetService(kPrefServiceCID, &rv));
    if (NS_SUCCEEDED(rv) && prefService)
    {
      PRInt32 intpref;
      rv = prefService->GetIntPref(np->name, &intpref);
      if (NS_SUCCEEDED(rv))
      {
        np->intVar = intpref;
        np->isSet = PR_TRUE;
#ifdef DEBUG_akkana
        printf("====== Changed int pref %s to %d\n", np->name, np->intVar);
#endif
      }
    }
  }
  return 0;
}

static int PR_CALLBACK floatPrefChanged (const char *newpref, void *data)
{
  nsLookAndFeelFloatPref* np = (nsLookAndFeelFloatPref*)data;
  if (np)
  {
    nsresult rv;
    nsCOMPtr<nsIPref> prefService(do_GetService(kPrefServiceCID, &rv));
    if (NS_SUCCEEDED(rv) && prefService)
    {
      PRInt32 intpref;
      rv = prefService->GetIntPref(np->name, &intpref);
      if (NS_SUCCEEDED(rv))
      {
        np->floatVar = (float)intpref / 100.;
        np->isSet = PR_TRUE;
#ifdef DEBUG_akkana
        printf("====== Changed float pref %s to %f\n", np->name, np->floatVar);
#endif
      }
    }
  }
  return 0;
}

static int PR_CALLBACK colorPrefChanged (const char *newpref, void *data)
{
  nsLookAndFeelColorPref* np = (nsLookAndFeelColorPref*)data;
  if (np)
  {
    nsresult rv;
    nsCOMPtr<nsIPref> prefService(do_GetService(kPrefServiceCID, &rv));
    if (NS_SUCCEEDED(rv) && prefService)
    {
      char *colorStr = 0;
      rv = prefService->CopyCharPref(np->name, &colorStr);
      if (NS_SUCCEEDED(rv) && colorStr[0])
      {
        nsString colorNSStr; colorNSStr.AssignWithConversion(colorStr);
        nscolor thecolor;
        if (NS_SUCCEEDED(NS_ColorNameToRGB(colorNSStr, &thecolor)))
        {
          np->isSet = PR_TRUE;
          np->colorVar = (nscolor)thecolor;
#ifdef DEBUG_akkana
          printf("====== Changed color pref %s to 0x%lx\n",
                 np->name, (long)np->colorVar);
#endif
        }
        PL_strfree(colorStr);
      }
    }
  }
  return 0;
}

nsresult
nsXPLookAndFeel::InitFromPref(nsLookAndFeelIntPref* aPref, nsIPref* aPrefService)
{
  PRInt32 intpref;
  nsresult rv = aPrefService->GetIntPref(aPref->name, &intpref);
  if (NS_SUCCEEDED(rv))
  {
    aPref->isSet = PR_TRUE;
    aPref->intVar = intpref;
  }
  aPrefService->RegisterCallback(aPref->name, intPrefChanged, aPref);
  return rv;
}

nsresult
nsXPLookAndFeel::InitFromPref(nsLookAndFeelFloatPref* aPref, nsIPref* aPrefService)
{
  PRInt32 intpref;
  nsresult rv = aPrefService->GetIntPref(aPref->name, &intpref);
  if (NS_SUCCEEDED(rv))
  {
    aPref->isSet = PR_TRUE;
    aPref->floatVar = (float)intpref / 100.;
  }
  aPrefService->RegisterCallback(aPref->name, floatPrefChanged, aPref);
  return rv;
}

nsresult
nsXPLookAndFeel::InitFromPref(nsLookAndFeelColorPref* aPref, nsIPref* aPrefService)
{
  char *colorStr = 0;
  nsresult rv = aPrefService->CopyCharPref(aPref->name, &colorStr);
  if (NS_SUCCEEDED(rv) && colorStr[0])
  {
    nsAutoString colorNSStr; colorNSStr.AssignWithConversion(colorStr);
    nscolor thecolor;
    if (NS_SUCCEEDED(NS_ColorNameToRGB(colorNSStr, &thecolor)))
    {
      aPref->isSet = PR_TRUE;
      aPref->colorVar = (nscolor)thecolor;
      PL_strfree(colorStr);
    }
  }

  aPrefService->RegisterCallback(aPref->name, colorPrefChanged, aPref);
  return rv;
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

  nsresult rv;
  nsCOMPtr<nsIPref> prefService(do_GetService(kPrefServiceCID, &rv));
  if (NS_FAILED(rv) || !prefService)
    return;

  unsigned int i;
  for (i = 0; i < ((sizeof (sIntPrefs) / sizeof (*sIntPrefs))); ++i)
    InitFromPref(&sIntPrefs[i], prefService);

  for (i = 0; i < ((sizeof (sFloatPrefs) / sizeof (*sFloatPrefs))); ++i)
    InitFromPref(&sFloatPrefs[i], prefService);

  for (i = 0; i < ((sizeof (sColorPrefs) / sizeof (*sColorPrefs))); ++i)
    InitFromPref(&sColorPrefs[i], prefService);
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
NS_IMETHODIMP nsXPLookAndFeel::GetColor(const nsColorID aID, nscolor &aColor)
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

  for (unsigned int i = 0; i < ((sizeof (sIntPrefs) / sizeof (*sIntPrefs))); ++i)
    if (sColorPrefs[i].isSet && (sColorPrefs[i].id == aID))
    {
      aColor = sColorPrefs[i].colorVar;
      return NS_OK;
    }

  return NS_ERROR_NOT_AVAILABLE;
}
  
NS_IMETHODIMP nsXPLookAndFeel::GetMetric(const nsMetricID aID, PRInt32& aMetric)
{
  if (!sInitialized)
    Init();

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

#ifdef NS_DEBUG
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
  return NS_ERROR_NOT_AVAILABLE;
}
#endif

nsresult
NS_NewXPLookAndFeel(nsILookAndFeel** aXPLookAndFeel)
{
  if (!aXPLookAndFeel)
    return NS_ERROR_INVALID_ARG;
  *aXPLookAndFeel = new nsXPLookAndFeel;
  if (!*aXPLookAndFeel)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aXPLookAndFeel);
  return NS_OK;
}


