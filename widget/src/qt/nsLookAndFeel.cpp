/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *      Lars Knoll <knoll@kde.org>
 *      Zack Rusin <zack@kde.org>
 *		John C. Griggs <johng@corel.com>
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
#include "nsLookAndFeel.h"

#include <qpalette.h>
#include <qapplication.h>

#define QCOLOR_TO_NS_RGB(c) \
    ((nscolor)NS_RGB(c.red(),c.green(),c.blue()))

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsLookAndFeel::nsLookAndFeel() : nsXPLookAndFeel()
{
}

nsLookAndFeel::~nsLookAndFeel()
{
}

nsresult nsLookAndFeel::NativeGetColor(const nsColorID aID,nscolor &aColor)
{
  nsresult res = NS_OK;

  if (!qApp)
    return NS_ERROR_FAILURE;

  QPalette palette = qApp->palette();
  QColorGroup normalGroup = palette.inactive();
  QColorGroup activeGroup = palette.active();
  QColorGroup disabledGroup = palette.disabled();

  switch (aID) {
    case eColor_WindowBackground:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.background());
      break;

    case eColor_WindowForeground:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.foreground());
      break;

    case eColor_WidgetBackground:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.background());
      break;

    case eColor_WidgetForeground:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.foreground());
      break;

    case eColor_WidgetSelectBackground:
      aColor = QCOLOR_TO_NS_RGB(activeGroup.background());
      break;

    case eColor_WidgetSelectForeground:
      aColor = QCOLOR_TO_NS_RGB(activeGroup.foreground());
      break;

    case eColor_Widget3DHighlight:
      aColor = NS_RGB(0xa0,0xa0,0xa0);
      break;

    case eColor_Widget3DShadow:
      aColor = NS_RGB(0x40,0x40,0x40);
      break;

    case eColor_TextBackground:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.background());
      break;

    case eColor_TextForeground:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.text());
      break;

    case eColor_TextSelectBackground:
      aColor = QCOLOR_TO_NS_RGB(activeGroup.highlight());
      break;

    case eColor_TextSelectForeground:
      aColor = QCOLOR_TO_NS_RGB(activeGroup.highlightedText());
      break;

    case eColor_activeborder:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.background());
      break;

    case eColor_activecaption:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.background());
      break;

    case eColor_appworkspace:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.background());
      break;

    case eColor_background:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.background());
      break;

    case eColor_captiontext:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.text());
      break;

    case eColor_graytext:
      aColor = QCOLOR_TO_NS_RGB(disabledGroup.text());
      break;

    case eColor_highlight:
      aColor = QCOLOR_TO_NS_RGB(activeGroup.highlight());
      break;

    case eColor_highlighttext:
      aColor = QCOLOR_TO_NS_RGB(activeGroup.highlightedText());
      break;

    case eColor_inactiveborder:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.background());
      break;

    case eColor_inactivecaption:
      aColor = QCOLOR_TO_NS_RGB(disabledGroup.background());
      break;

    case eColor_inactivecaptiontext:
      aColor = QCOLOR_TO_NS_RGB(disabledGroup.text());
      break;

    case eColor_infobackground:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.background());
      break;

    case eColor_infotext:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.text());
      break;

    case eColor_menu:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.background());
      break;

    case eColor_menutext:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.text());
      break;

    case eColor_scrollbar:
      aColor = QCOLOR_TO_NS_RGB(activeGroup.background());
      break;

    case eColor_threedface:
    case eColor_buttonface:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.background());
      break;

    case eColor_buttonhighlight:
    case eColor_threedhighlight:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.light());
      break;

    case eColor_buttontext:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.text());
      break;

    case eColor_buttonshadow:
    case eColor_threedshadow:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.shadow());
      break;

    case eColor_threeddarkshadow:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.dark());
      break;

    case eColor_threedlightshadow:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.light());
      break;

    case eColor_window:
    case eColor_windowframe:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.background());
      break;

    case eColor_windowtext:
      aColor = QCOLOR_TO_NS_RGB(normalGroup.text());
      break;

     // from the CSS3 working draft (not yet finalized)
     // http://www.w3.org/tr/2000/wd-css3-userint-20000216.html#color
     case eColor__moz_field:
       aColor = QCOLOR_TO_NS_RGB(normalGroup.base());
       break;

     case eColor__moz_fieldtext:
       aColor = QCOLOR_TO_NS_RGB(normalGroup.text());
       break;

     case eColor__moz_dialog:
       aColor = QCOLOR_TO_NS_RGB(normalGroup.background());
       break;

     case eColor__moz_dialogtext:
       aColor = QCOLOR_TO_NS_RGB(normalGroup.text());
       break;

     case eColor__moz_dragtargetzone:
       aColor = QCOLOR_TO_NS_RGB(activeGroup.background());
       break;

    default:
      aColor = 0;
      res    = NS_ERROR_FAILURE;
      break;
  }
  return res;
}

static const char *metricToString[] = {
    "eMetric_WindowTitleHeight",
    "eMetric_WindowBorderWidth",
    "eMetric_WindowBorderHeight",
    "eMetric_Widget3DBorder",
    "eMetric_TextFieldBorder"
    "eMetric_TextFieldHeight",
    "eMetric_TextVerticalInsidePadding",
    "eMetric_TextShouldUseVerticalInsidePadding",
    "eMetric_TextHorizontalInsideMinimumPadding",
    "eMetric_TextShouldUseHorizontalInsideMinimumPadding",
    "eMetric_ButtonHorizontalInsidePaddingNavQuirks",
    "eMetric_ButtonHorizontalInsidePaddingOffsetNavQuirks",
    "eMetric_CheckboxSize",
    "eMetric_RadioboxSize",

    "eMetric_ListShouldUseHorizontalInsideMinimumPadding",
    "eMetric_ListHorizontalInsideMinimumPadding",

    "eMetric_ListShouldUseVerticalInsidePadding",
    "eMetric_ListVerticalInsidePadding",

    "eMetric_CaretBlinkTime",
    "eMetric_CaretWidth",
    "eMetric_ShowCaretDuringSelection",
    "eMetric_SelectTextfieldsOnKeyFocus",
    "eMetric_SubmenuDelay",
    "eMetric_MenusCanOverlapOSBar",
    "eMetric_DragFullWindow",
    "eMetric_DragThresholdX",
    "eMetric_DragThresholdY",
    "eMetric_UseAccessibilityTheme",

    "eMetric_ScrollArrowStyle",
    "eMetric_ScrollSliderStyle",

    "eMetric_TreeOpenDelay",
    "eMetric_TreeCloseDelay",
    "eMetric_TreeLazyScrollDelay",
    "eMetric_TreeScrollDelay",
    "eMetric_TreeScrollLinesMax"
    };


NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricID aID,PRInt32 &aMetric)
{
//     qDebug("nsLookAndFeel::GetMetric aID = %s", metricToString[aID]);
  nsresult res = nsXPLookAndFeel::GetMetric(aID, aMetric);
  if (NS_SUCCEEDED(res))
      return res;
//   qDebug("    checking ourselves");
  res = NS_OK;

  switch (aID) {
    case eMetric_WindowTitleHeight:
      aMetric = 0;
      break;

    case eMetric_WindowBorderWidth:
      // There was once code in nsDeviceContextQt::GetSystemAttribute to
      // use the border width obtained from a widget in its Init method.
      break;

    case eMetric_WindowBorderHeight:
      // There was once code in nsDeviceContextQt::GetSystemAttribute to
      // use the border width obtained from a widget in its Init method.
      break;

    case eMetric_Widget3DBorder:
      aMetric = 4;
      break;

    case eMetric_TextFieldHeight:
      aMetric = 15;
      break;

    case eMetric_TextFieldBorder:
      aMetric = 2;
      break;

    case eMetric_TextVerticalInsidePadding:
      aMetric = 0;
      break;

    case eMetric_TextShouldUseVerticalInsidePadding:
      aMetric = 0;
      break;

    case eMetric_TextHorizontalInsideMinimumPadding:
      aMetric = 15;
      break;

    case eMetric_TextShouldUseHorizontalInsideMinimumPadding:
      aMetric = 1;
      break;

    case eMetric_ButtonHorizontalInsidePaddingNavQuirks:
      aMetric = 10;
      break;

    case eMetric_ButtonHorizontalInsidePaddingOffsetNavQuirks:
      aMetric = 8;
      break;

    case eMetric_CheckboxSize:
      aMetric = 15;
      break;

    case eMetric_RadioboxSize:
      aMetric = 15;
      break;

    case eMetric_ListShouldUseHorizontalInsideMinimumPadding:
      aMetric = 15;
      break;

    case eMetric_ListHorizontalInsideMinimumPadding:
      aMetric = 15;
      break;

    case eMetric_ListShouldUseVerticalInsidePadding:
      aMetric = 1;
      break;

    case eMetric_ListVerticalInsidePadding:
      aMetric = 1;
      break;

    case eMetric_CaretBlinkTime:
      aMetric = 500;
      break;

    case eMetric_CaretWidth:
      aMetric = 1;
      break;

    case eMetric_ShowCaretDuringSelection:
      aMetric = 0;
      break;

    case eMetric_SelectTextfieldsOnKeyFocus:
      // Select textfield content when focused by kbd
      // used by nsEventStateManager::sTextfieldSelectModel
      aMetric = 1;
      break;

    case eMetric_SubmenuDelay:
      aMetric = 200;
      break;

    case eMetric_MenusCanOverlapOSBar:
      // we want XUL popups to be able to overlap the task bar.
      aMetric = 1;
      break;

    case eMetric_DragFullWindow:
      aMetric = 1;
      break;

    case eMetric_ScrollArrowStyle:
      aMetric = eMetric_ScrollArrowStyleSingle;
      break;

    case eMetric_ScrollSliderStyle:
      aMetric = eMetric_ScrollThumbStyleProportional;
      break;

    default:
      aMetric = 0;
      res = NS_ERROR_FAILURE;
  }
  return res;
}

NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricFloatID aID,
                                       float &aMetric)
{
  nsresult res = nsXPLookAndFeel::GetMetric(aID, aMetric);
  if (NS_SUCCEEDED(res))
      return res;
  res = NS_OK;

  switch (aID) {
    case eMetricFloat_TextFieldVerticalInsidePadding:
      aMetric = 0.25f;
      break;

    case eMetricFloat_TextFieldHorizontalInsidePadding:
      aMetric = 0.95f; // large number on purpose so minimum padding is used
      break;

    case eMetricFloat_TextAreaVerticalInsidePadding:
      aMetric = 0.40f;
      break;

    case eMetricFloat_TextAreaHorizontalInsidePadding:
      aMetric = 0.40f; // large number on purpose so minimum padding is used
      break;

    case eMetricFloat_ListVerticalInsidePadding:
      aMetric = 0.10f;
      break;

    case eMetricFloat_ListHorizontalInsidePadding:
      aMetric = 0.40f;
      break;

    case eMetricFloat_ButtonVerticalInsidePadding:
      aMetric = 0.25f;
      break;

    case eMetricFloat_ButtonHorizontalInsidePadding:
      aMetric = 0.25f;
      break;

    default:
      aMetric = -1.0;
      res = NS_ERROR_FAILURE;
  }
  return res;
}
