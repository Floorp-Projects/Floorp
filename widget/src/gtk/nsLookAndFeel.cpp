/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsLookAndFeel.h"
#include <gtk/gtkinvisible.h>

#include "nsXPLookAndFeel.h"

#define GDK_COLOR_TO_NS_RGB(c) \
    ((nscolor) NS_RGB(c.red>>8, c.green>>8, c.blue>>8))

nscolor nsLookAndFeel::sInfoText = 0;
nscolor nsLookAndFeel::sInfoBackground = 0;
PRBool nsLookAndFeel::sHaveInfoColors = PR_FALSE;

NS_IMPL_ISUPPORTS1(nsLookAndFeel, nsILookAndFeel)

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsLookAndFeel::nsLookAndFeel()
{
  NS_INIT_REFCNT();
  mWidget = gtk_invisible_new();
  gtk_widget_ensure_style(mWidget);
  mStyle = gtk_widget_get_style(mWidget);

  (void)NS_NewXPLookAndFeel(getter_AddRefs(mXPLookAndFeel));
}

nsLookAndFeel::~nsLookAndFeel()
{
  //  gtk_widget_destroy(mWidget);
  gtk_widget_unref(mWidget);
}

NS_IMETHODIMP nsLookAndFeel::GetColor(const nsColorID aID, nscolor &aColor)
{
  nsresult res = NS_OK;
  if (mXPLookAndFeel)
  {
    res = mXPLookAndFeel->GetColor(aID, aColor);
    if (NS_SUCCEEDED(res))
      return res;
    res = NS_OK;
  }

  aColor = 0; // default color black

  switch (aID) {
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
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->text[GTK_STATE_SELECTED]);
    break;

    // css2  http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
  case eColor_activeborder:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;
  case eColor_activecaption:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;
  case eColor_appworkspace:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;
  case eColor_background:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;

  case eColor_captiontext:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
    break;
  case eColor_graytext:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_INSENSITIVE]);
    break;
  case eColor_highlight:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_SELECTED]);
    break;
  case eColor_highlighttext:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_SELECTED]);
    break;
  case eColor_inactiveborder:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;
  case eColor_inactivecaption:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_INSENSITIVE]);
    break;
  case eColor_inactivecaptiontext:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_INSENSITIVE]);
    break;
  case eColor_infobackground:
    if (!sHaveInfoColors)
      GetInfoColors();
    aColor = sInfoBackground;
    break;
  case eColor_infotext:
    if (!sHaveInfoColors)
      GetInfoColors();
    aColor = sInfoText;
    break;
  case eColor_menu:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;
  case eColor_menutext:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
    break;
  case eColor_scrollbar:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_ACTIVE]);
    break;

  case eColor_threedface:
  case eColor_buttonface:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;

  case eColor_buttonhighlight:
  case eColor_threedhighlight:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->light[GTK_STATE_NORMAL]);
    break;

  case eColor_buttontext:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
    break;

  case eColor_buttonshadow:
  case eColor_threedshadow: // i think these should be the same
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->dark[GTK_STATE_NORMAL]);
    break;

  case eColor_threeddarkshadow:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->black);
    break;

  case eColor_threedlightshadow:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->light[GTK_STATE_NORMAL]);
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

  if (mXPLookAndFeel)
  {
    res = mXPLookAndFeel->GetMetric(aID, aMetric);
    if (NS_SUCCEEDED(res))
      return res;
    res = NS_OK;
  }

  switch (aID) {
  case eMetric_WindowTitleHeight:
    aMetric = 0;
    break;
  case eMetric_WindowBorderWidth:
    //    aMetric = mStyle->klass->xthickness;
    break;
  case eMetric_WindowBorderHeight:
    //    aMetric = mStyle->klass->ythickness;
    break;
  case eMetric_Widget3DBorder:
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
  case eMetric_SingleLineCaretWidth:
  case eMetric_MultiLineCaretWidth:
    aMetric = 1;
    break;
  case eMetric_SubmenuDelay:
    aMetric = 200;
    break;
  case eMetric_MenusCanOverlapOSBar:
    // we want XUL popups to be able to overlap the task bar.
    aMetric = 1;
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

  if (mXPLookAndFeel)
  {
    res = mXPLookAndFeel->GetMetric(aID, aMetric);
    if (NS_SUCCEEDED(res))
      return res;
    res = NS_OK;
  }

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
nsLookAndFeel::GetInfoColors()
{
  GtkTooltips *tooltips = gtk_tooltips_new();
  gtk_tooltips_force_window(tooltips);
  GtkWidget *tip_window = tooltips->tip_window;
  gtk_widget_set_rc_style(tip_window);

  GtkStyle *tipstyle = gtk_widget_get_style(tip_window);
  sInfoBackground = GDK_COLOR_TO_NS_RGB(tipstyle->bg[GTK_STATE_NORMAL]);
  sInfoText = GDK_COLOR_TO_NS_RGB(tipstyle->fg[GTK_STATE_NORMAL]);
  sHaveInfoColors = PR_TRUE;

  gtk_object_unref(GTK_OBJECT(tooltips));
}

#ifdef NS_DEBUG
NS_IMETHODIMP nsLookAndFeel::GetNavSize(const nsMetricNavWidgetID aWidgetID,
                                        const nsMetricNavFontID   aFontID, 
                                        const PRInt32             aFontSize, 
                                        nsSize &aSize)
{
  if (mXPLookAndFeel)
  {
    nsresult rv = mXPLookAndFeel->GetNavSize(aWidgetID, aFontID, aFontSize, aSize);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  aSize.width  = 0;
  aSize.height = 0;
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif
