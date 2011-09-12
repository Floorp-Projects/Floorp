/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
 *   John C. Griggs <johng@corel.com>
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

#include <QPalette>
#include <QApplication>
#include <QStyle>

#include "nsLookAndFeel.h"
#include "nsStyleConsts.h"

#include <qglobal.h>

#undef NS_LOOKANDFEEL_DEBUG
#ifdef NS_LOOKANDFEEL_DEBUG
#include <QDebug>
#endif

#define QCOLOR_TO_NS_RGB(c) \
    ((nscolor)NS_RGB(c.red(),c.green(),c.blue()))

nsLookAndFeel::nsLookAndFeel() : nsXPLookAndFeel()
{
}

nsLookAndFeel::~nsLookAndFeel()
{
}

nsresult
nsLookAndFeel::NativeGetColor(ColorID aID, nscolor &aColor)
{
  nsresult res = NS_OK;

  if (!qApp)
    return NS_ERROR_FAILURE;

  QPalette palette = qApp->palette();

  switch (aID) {
    case eColorID_WindowBackground:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
      break;

    case eColorID_WindowForeground:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
      break;

    case eColorID_WidgetBackground:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
      break;

    case eColorID_WidgetForeground:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::WindowText));
      break;

    case eColorID_WidgetSelectBackground:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
      break;

    case eColorID_WidgetSelectForeground:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::WindowText));
      break;

    case eColorID_Widget3DHighlight:
      aColor = NS_RGB(0xa0,0xa0,0xa0);
      break;

    case eColorID_Widget3DShadow:
      aColor = NS_RGB(0x40,0x40,0x40);
      break;

    case eColorID_TextBackground:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
      break;

    case eColorID_TextForeground:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::WindowText));
      break;

    case eColorID_TextSelectBackground:
    case eColorID_IMESelectedRawTextBackground:
    case eColorID_IMESelectedConvertedTextBackground:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Highlight));
      break;

    case eColorID_TextSelectForeground:
    case eColorID_IMESelectedRawTextForeground:
    case eColorID_IMESelectedConvertedTextForeground:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::HighlightedText));
      break;

    case eColorID_IMERawInputBackground:
    case eColorID_IMEConvertedTextBackground:
      aColor = NS_TRANSPARENT;
      break;

    case eColorID_IMERawInputForeground:
    case eColorID_IMEConvertedTextForeground:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;

    case eColorID_IMERawInputUnderline:
    case eColorID_IMEConvertedTextUnderline:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;

    case eColorID_IMESelectedRawTextUnderline:
    case eColorID_IMESelectedConvertedTextUnderline:
      aColor = NS_TRANSPARENT;
      break;

    case eColorID_SpellCheckerUnderline:
      aColor = NS_RGB(0xff, 0, 0);
      break;

    case eColorID_activeborder:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
      break;

    case eColorID_activecaption:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
      break;

    case eColorID_appworkspace:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
      break;

    case eColorID_background:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
      break;

    case eColorID_captiontext:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
      break;

    case eColorID_graytext:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Disabled, QPalette::Text));
      break;

    case eColorID_highlight:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Highlight));
      break;

    case eColorID_highlighttext:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::HighlightedText));
      break;

    case eColorID_inactiveborder:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Disabled, QPalette::Window));
      break;

    case eColorID_inactivecaption:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Disabled, QPalette::Window));
      break;

    case eColorID_inactivecaptiontext:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Disabled, QPalette::Text));
      break;

    case eColorID_infobackground:
#if (QT_VERSION >= QT_VERSION_CHECK(4, 4, 0))
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::ToolTipBase));
#else
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Base));
#endif
      break;

    case eColorID_infotext:
#if (QT_VERSION >= QT_VERSION_CHECK(4, 4, 0))
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::ToolTipText));
#else
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
#endif
      break;

    case eColorID_menu:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
      break;

    case eColorID_menutext:
    case eColorID__moz_menubartext:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
      break;

    case eColorID_scrollbar:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Mid));
      break;

    case eColorID_threedface:
    case eColorID_buttonface:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Button));
      break;

    case eColorID_buttonhighlight:
    case eColorID_threedhighlight:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Dark));
      break;

    case eColorID_buttontext:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::ButtonText));
      break;

    case eColorID_buttonshadow:
    case eColorID_threedshadow:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Dark));
      break;

    case eColorID_threeddarkshadow:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Shadow));
      break;

    case eColorID_threedlightshadow:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Light));
      break;

    case eColorID_window:
    case eColorID_windowframe:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
      break;

    case eColorID_windowtext:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
      break;

     // from the CSS3 working draft (not yet finalized)
     // http://www.w3.org/tr/2000/wd-css3-userint-20000216.html#color

    case eColorID__moz_buttondefault:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Button));
      break;

    case eColorID__moz_field:
    case eColorID__moz_combobox:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Base));
      break;

    case eColorID__moz_fieldtext:
    case eColorID__moz_comboboxtext:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
      break;

    case eColorID__moz_dialog:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
      break;

    case eColorID__moz_dialogtext:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::WindowText));
      break;

    case eColorID__moz_dragtargetzone:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Window));
      break;

    case eColorID__moz_buttonhovertext:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::ButtonText));
      break;

    case eColorID__moz_menuhovertext:
    case eColorID__moz_menubarhovertext:
      aColor = QCOLOR_TO_NS_RGB(palette.color(QPalette::Normal, QPalette::Text));
      break;

    default:
      aColor = 0;
      res    = NS_ERROR_FAILURE;
      break;
  }
  return res;
}

#ifdef NS_LOOKANDFEEL_DEBUG
static const char *metricToString[] = {
    "eIntID_CaretBlinkTime",
    "eIntID_CaretWidth",
    "eIntID_ShowCaretDuringSelection",
    "eIntID_SelectTextfieldsOnKeyFocus",
    "eIntID_SubmenuDelay",
    "eIntID_MenusCanOverlapOSBar",
    "eIntID_SkipNavigatingDisabledMenuItem",
    "eIntID_DragThresholdX",
    "eIntID_DragThresholdY",
    "eIntID_UseAccessibilityTheme",
    "eIntID_ScrollArrowStyle",
    "eIntID_ScrollSliderStyle",
    "eIntID_ScrollButtonLeftMouseButtonAction",
    "eIntID_ScrollButtonMiddleMouseButtonAction",
    "eIntID_ScrollButtonRightMouseButtonAction",
    "eIntID_TreeOpenDelay",
    "eIntID_TreeCloseDelay",
    "eIntID_TreeLazyScrollDelay",
    "eIntID_TreeScrollDelay",
    "eIntID_TreeScrollLinesMax",
    "eIntID_TabFocusModel",
    "eIntID_WindowsDefaultTheme",
    "eIntID_AlertNotificationOrigin",
    "eIntID_ScrollToClick",
    "eIntID_IMERawInputUnderlineStyle",
    "eIntID_IMESelectedRawTextUnderlineStyle",
    "eIntID_IMEConvertedTextUnderlineStyle",
    "eIntID_IMESelectedConvertedTextUnderline",
    "eIntID_ImagesInMenus"
    };
#endif

nsresult
nsLookAndFeel::GetIntImpl(IntID aID, PRInt32 &aResult)
{
#ifdef NS_LOOKANDFEEL_DEBUG
  qDebug("nsLookAndFeel::GetIntImpl aID = %s", metricToString[aID]);
#endif

  nsresult res = nsXPLookAndFeel::GetIntImpl(aID, aResult);
  if (NS_SUCCEEDED(res))
      return res;

  res = NS_OK;

  switch (aID) {
    case eIntID_CaretBlinkTime:
      aResult = 500;
      break;

    case eIntID_CaretWidth:
      aResult = 1;
      break;

    case eIntID_ShowCaretDuringSelection:
      aResult = 0;
      break;

    case eIntID_SelectTextfieldsOnKeyFocus:
      // Select textfield content when focused by kbd
      // used by nsEventStateManager::sTextfieldSelectModel
      aResult = 1;
      break;

    case eIntID_SubmenuDelay:
      aResult = 200;
      break;

    case eIntID_MenusCanOverlapOSBar:
      // we want XUL popups to be able to overlap the task bar.
      aResult = 1;
      break;

    case eIntID_ScrollArrowStyle:
      aResult = eScrollArrowStyle_Single;
      break;

    case eIntID_ScrollSliderStyle:
      aResult = eScrollThumbStyle_Proportional;
      break;

    case eIntID_TouchEnabled:
#ifdef MOZ_PLATFORM_MAEMO
      // All known Maemo devices are touch enabled.
      aResult = 1;
#else
      aResult = 0;
      res = NS_ERROR_NOT_IMPLEMENTED;
#endif
      break;

    case eIntID_WindowsDefaultTheme:
    case eIntID_MaemoClassic:
      aResult = 0;
      res = NS_ERROR_NOT_IMPLEMENTED;
      break;

    case eIntID_SpellCheckerUnderlineStyle:
      aResult = NS_STYLE_TEXT_DECORATION_STYLE_WAVY;
      break;

    default:
      aResult = 0;
      res = NS_ERROR_FAILURE;
  }
  return res;
}

#ifdef NS_LOOKANDFEEL_DEBUG
static const char *floatMetricToString[] = {
    "eFloatID_IMEUnderlineRelativeSize"
};
#endif

nsresult
nsLookAndFeel::GetFloatImpl(FloatID aID, float &aResult)
{
#ifdef NS_LOOKANDFEEL_DEBUG
  qDebug("nsLookAndFeel::GetFloatImpl aID = %s", floatMetricToString[aID]);
#endif

  nsresult res = nsXPLookAndFeel::GetFloatImpl(aID, aResult);
  if (NS_SUCCEEDED(res))
      return res;
  res = NS_OK;

  switch (aID) {
    case eFloatID_IMEUnderlineRelativeSize:
      aResult = 1.0f;
      break;

    case eFloatID_SpellCheckerUnderlineRelativeSize:
      aResult = 1.0f;
      break;

    default:
      aResult = -1.0;
      res = NS_ERROR_FAILURE;
      break;
  }
  return res;
}
