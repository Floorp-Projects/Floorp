/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsLookAndFeel.h"

#define GDK_COLOR_TO_NS_RGB(c) \
  ((nscolor) NS_RGB(c.green, c.blue, c.red))
//    ((nscolor) NS_RGB(c.red, c.green, c.blue))


NS_IMPL_ISUPPORTS1(nsLookAndFeel, nsILookAndFeel)

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsLookAndFeel::nsLookAndFeel()
{
  NS_INIT_REFCNT();
  mStyle = gtk_style_new();
  gdk_rgb_init();
  //  mWindow = gdk_pixmap_new(nsnull, 1, 1, 1);

  GdkWindowAttr attributes;
  gint attributes_mask;

  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.x = 0;
  attributes.y = 0;
  attributes.width = 1;
  attributes.height = 1;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gdk_rgb_get_visual();
  attributes.colormap = gdk_rgb_get_cmap();
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  mWindow = gdk_window_new(nsnull, &attributes, attributes_mask);

  gtk_style_attach(mStyle, mWindow);
}

nsLookAndFeel::~nsLookAndFeel()
{
  gtk_style_unref(mStyle);
  gdk_window_unref(mWindow);
}

NS_IMETHODIMP nsLookAndFeel::GetColor(const nsColorID aID, nscolor &aColor)
{
  nsresult res = NS_OK;
  GdkGCValues values;
  aColor = 0;

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
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;
  case eColor_infotext:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
    break;
  case eColor_menu:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;
  case eColor_menutext:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
    break;
  case eColor_scrollbar:
    break;

  case eColor_threedface:
  case eColor_buttonface:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;
  case eColor_buttonhighlight:
    // ?
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_ACTIVE]);
    break;
  case eColor_buttonshadow:
    gdk_gc_get_values(mStyle->dark_gc[GTK_STATE_NORMAL], &values);
    aColor = GDK_COLOR_TO_NS_RGB(values.foreground);
    break;
  case eColor_buttontext:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
    break;

  case eColor_threeddarkshadow:
  case eColor_threedshadow: // i think these should be the same
    gdk_gc_get_values(mStyle->dark_gc[GTK_STATE_NORMAL], &values);
    aColor = GDK_COLOR_TO_NS_RGB(values.foreground);
    break;

  case eColor_threedhighlight:
    //    aColor = GDK_COLOR_TO_NS_RGB();
    break;
  case eColor_threedlightshadow:
    gdk_gc_get_values(mStyle->light_gc[GTK_STATE_NORMAL], &values);
    aColor = GDK_COLOR_TO_NS_RGB(values.foreground);
    //    aColor = GDK_COLOR_TO_NS_RGB(mStyle->light[GTK_STATE_NORMAL]);
    break;
  case eColor_window:
  case eColor_windowframe:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->bg[GTK_STATE_NORMAL]);
    break;
  case eColor_windowtext:
    aColor = GDK_COLOR_TO_NS_RGB(mStyle->fg[GTK_STATE_NORMAL]);
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
  case eMetric_CaretWidthTwips:
    aMetric = 20;
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

#ifdef NS_DEBUG
NS_IMETHODIMP nsLookAndFeel::GetNavSize(const nsMetricNavWidgetID aWidgetID,
                                        const nsMetricNavFontID   aFontID, 
                                        const PRInt32             aFontSize, 
                                        nsSize &aSize)
{
  aSize.width  = 0;
  aSize.height = 0;
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif
