/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Brian Ryner <bryner@brianryner.com>
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

#include "nsLookAndFeel.h"
#include <gtk/gtkinvisible.h>

#define GDK_COLOR_TO_NS_RGB(c) \
    ((nscolor) NS_RGB(c.red>>8, c.green>>8, c.blue>>8))

nscolor nsLookAndFeel::sInfoText = 0;
nscolor nsLookAndFeel::sInfoBackground = 0;
nscolor nsLookAndFeel::sMenuText = 0;
nscolor nsLookAndFeel::sMenuBackground = 0;
nscolor nsLookAndFeel::sButtonBackground = 0;
nscolor nsLookAndFeel::sButtonText = 0;
nscolor nsLookAndFeel::sButtonOuterLightBorder = 0;
nscolor nsLookAndFeel::sButtonInnerDarkBorder = 0;
PRBool nsLookAndFeel::sColorsInitialized = PR_FALSE;

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsLookAndFeel::nsLookAndFeel() : nsXPLookAndFeel()
{
  mWidget = gtk_invisible_new();
  gtk_widget_ensure_style(mWidget);
  mStyle = gtk_widget_get_style(mWidget);

  if (!sColorsInitialized)
    InitColors();
}

nsLookAndFeel::~nsLookAndFeel()
{
  //  gtk_widget_destroy(mWidget);
  gtk_widget_unref(mWidget);
}

nsresult nsLookAndFeel::NativeGetColor(const nsColorID aID, nscolor& aColor)
{
  nsresult res = NS_OK;
  aColor = 0; // default color black

  switch (aID) {
    // These colors don't seem to be used for anything anymore
    // in Mozilla.  The CSS2 colors below are used.
  case eColor_WindowBackground:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;
  case eColor_WindowForeground:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
    break;
  case eColor_WidgetBackground:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;
  case eColor_WidgetForeground:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
    break;
  case eColor_WidgetSelectBackground:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_SELECTED]);
    break;
  case eColor_WidgetSelectForeground:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_SELECTED]);
    break;
  case eColor_Widget3DHighlight:
    aColor = NS_RGB(0xa0,0xa0,0xa0);
    break;
  case eColor_Widget3DShadow:
    aColor = NS_RGB(0x40,0x40,0x40);
    break;
  case eColor_TextBackground:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;
  case eColor_TextForeground: 
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
    break;
  case eColor_TextSelectBackground:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_SELECTED]);
    break;
  case eColor_TextSelectForeground:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_SELECTED]);
    break;

    // css2  http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
  case eColor_activeborder:
      // active window border
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;
  case eColor_activecaption:
      // active window caption background
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;
  case eColor_appworkspace:
      // MDI background color
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;
  case eColor_background:
      // desktop background
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;
  case eColor_captiontext:
      // text in active window caption, size box, and scrollbar arrow box (!)
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
    break;
  case eColor_graytext:
      // disabled text
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_INSENSITIVE]);
    // or maybe mStyle->text?
    break;
  case eColor_highlight:
      // background of selected item
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_SELECTED]);
    break;
  case eColor_highlighttext:
      // text of selected item
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_SELECTED]);
    break;
  case eColor_inactiveborder:
      // inactive window border
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;
  case eColor_inactivecaption:
      // inactive window caption
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_INSENSITIVE]);
    break;
  case eColor_inactivecaptiontext:
      // text in inactive window caption
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_INSENSITIVE]);
    break;
  case eColor_infobackground:
      // tooltip background color
    aColor = sInfoBackground;
    break;
  case eColor_infotext:
      // tooltip text color
    aColor = sInfoText;
    break;
  case eColor_menu:
      // menu background
    aColor = sMenuBackground;
    break;
  case eColor_menutext:
      // menu text
    aColor = sMenuText;
    break;
  case eColor_scrollbar:
      // scrollbar gray area
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_ACTIVE]);
    break;

  case eColor_threedface:
  case eColor_buttonface:
      // 3-D face color
    aColor = sButtonBackground;
    break;

  case eColor_buttontext:
      // text on push buttons
    aColor = sButtonText;
    break;

  case eColor_buttonhighlight:
      // 3-D highlighted edge color
  case eColor_threedhighlight:
      // 3-D highlighted outer edge color
    aColor = sButtonOuterLightBorder;
    break;

  case eColor_threedlightshadow:
      // 3-D highlighted inner edge color
    aColor = sButtonBackground; // always same as background in GTK code
    break;

  case eColor_buttonshadow:
      // 3-D shadow edge color
  case eColor_threedshadow:
      // 3-D shadow inner edge color
    aColor = sButtonInnerDarkBorder;
    break;

  case eColor_threeddarkshadow:
      // 3-D shadow outer edge color
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->black);
    break;

  case eColor_window:
  case eColor_windowframe:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;

  case eColor_windowtext:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
    break;

  // from the CSS3 working draft (not yet finalized)
  // http://www.w3.org/tr/2000/wd-css3-userint-20000216.html#color

  case eColor__moz_field:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->base[GTK_STATE_NORMAL]);
    break;
  case eColor__moz_fieldtext:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
    break;
  case eColor__moz_dialog:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;
  case eColor__moz_dialogtext:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
    break;
  case eColor__moz_dragtargetzone:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_SELECTED]);
    break; 
  case eColor__moz_buttondefault:
      // default button border color
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->black);
    break;
  default:
    /* default color is BLACK */
    aColor = 0;
    res    = NS_ERROR_FAILURE;
    break;
  }

  //  printf("%i, %i, %i\n", NS_GET_R(aColor), NS_GET_B(aColor), NS_GET_G(aColor));

  return res;
}

NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricID aID, PRInt32 & aMetric)
{
  nsresult res = NS_OK;

  res = nsXPLookAndFeel::GetMetric(aID, aMetric);
  if (NS_SUCCEEDED(res))
    return res;
  res = NS_OK;

  switch (aID) {
  case eMetric_WindowTitleHeight:
    aMetric = 0;
    break;
  case eMetric_WindowBorderWidth:
    // XXXldb Why is this commented out?
    //    aMetric = mStyle->klass->xthickness;
    break;
  case eMetric_WindowBorderHeight:
    // XXXldb Why is this commented out?
    //    aMetric = mStyle->klass->ythickness;
    break;
  case eMetric_Widget3DBorder:
    // XXXldb Why is this commented out?
    //    aMetric = 4;
    break;
  case eMetric_TextFieldHeight:
    {
      GtkRequisition req;
      GtkWidget *text = gtk_entry_new();
      // needed to avoid memory leak
      gtk_widget_ref(text);
      gtk_object_sink(GTK_OBJECT(text));
      gtk_widget_size_request(text,&req);
      aMetric = req.height;
      gtk_widget_destroy(text);
      gtk_widget_unref(text);
    }
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
  case eMetric_DragThresholdX:
  case eMetric_DragThresholdY:
    // hardcoded in gtkdnd.c
    aMetric = 3;
    break;
  case eMetric_ScrollArrowStyle:
    aMetric = eMetric_ScrollArrowStyleSingle;
    break;
  case eMetric_ScrollSliderStyle:
    aMetric = eMetric_ScrollThumbStyleProportional;
    break;
  case eMetric_TreeOpenDelay:
    aMetric = 1000;
    break;
  case eMetric_TreeCloseDelay:
    aMetric = 1000;
    break;
  case eMetric_TreeLazyScrollDelay:
    aMetric = 150;
    break;
  case eMetric_TreeScrollDelay:
    aMetric = 100;
    break;
  case eMetric_TreeScrollLinesMax:
    aMetric = 3;
    break;

  default:
    aMetric = 0;
    res     = NS_ERROR_FAILURE;
  }

  return res;
}

NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricFloatID aID, float & aMetric)
{
  nsresult res = NS_OK;
  res = nsXPLookAndFeel::GetMetric(aID, aMetric);
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

void
nsLookAndFeel::InitColors()
{
  sColorsInitialized = PR_TRUE;
  GtkStyle *style;
  
  // tooltip foreground and background
  GtkTooltips *tooltips = gtk_tooltips_new();
  gtk_tooltips_force_window(tooltips);
  GtkWidget *tip_window = tooltips->tip_window;
  gtk_widget_set_rc_style(tip_window);

  style = gtk_widget_get_style(tip_window);
  sInfoBackground = GDK_COLOR_TO_NS_RGB(style->bg[GTK_STATE_NORMAL]);
  sInfoText = GDK_COLOR_TO_NS_RGB(style->fg[GTK_STATE_NORMAL]);

  gtk_object_unref(GTK_OBJECT(tooltips));


  // menu foreground & menu background
  GtkWidget *accel_label = gtk_accel_label_new("M");
  GtkWidget *menuitem = gtk_menu_item_new();
  GtkWidget *menu = gtk_menu_new();

  gtk_container_add(GTK_CONTAINER(menuitem), accel_label);
  gtk_menu_append(GTK_MENU(menu), menuitem);

  gtk_widget_set_rc_style(accel_label);
  gtk_widget_set_rc_style(menu);
  gtk_widget_realize(menu);
  gtk_widget_realize(accel_label);

  style = gtk_widget_get_style(accel_label);
  sMenuText = GDK_COLOR_TO_NS_RGB(style->fg[GTK_STATE_NORMAL]);

  style = gtk_widget_get_style(menu);
  sMenuBackground = GDK_COLOR_TO_NS_RGB(style->bg[GTK_STATE_NORMAL]);

  gtk_widget_unref(menu);


  // button styles
  GtkWidget *parent = gtk_fixed_new();
  GtkWidget *button = gtk_button_new();
  GtkWidget *label = gtk_label_new("M");
  GtkWidget *window = gtk_window_new(GTK_WINDOW_POPUP);
  
  gtk_container_add(GTK_CONTAINER(button), label);
  gtk_container_add(GTK_CONTAINER(parent), button);
  gtk_container_add(GTK_CONTAINER(window), parent);

  gtk_widget_set_rc_style(button);
  gtk_widget_set_rc_style(label);

  gtk_widget_realize(button);
  gtk_widget_realize(label);

  style = gtk_widget_get_style(label);
  sButtonText = GDK_COLOR_TO_NS_RGB(style->fg[GTK_STATE_NORMAL]);

  style = gtk_widget_get_style(button);
  sButtonBackground = GDK_COLOR_TO_NS_RGB(style->bg[GTK_STATE_NORMAL]);
  sButtonOuterLightBorder =
    GDK_COLOR_TO_NS_RGB(style->light[GTK_STATE_NORMAL]);
  sButtonInnerDarkBorder =
    GDK_COLOR_TO_NS_RGB(style->dark[GTK_STATE_NORMAL]);

  gtk_widget_destroy(window);

}
