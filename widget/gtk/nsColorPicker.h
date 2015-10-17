/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsColorPicker_h__
#define nsColorPicker_h__

#include <gtk/gtk.h>

#include "nsCOMPtr.h"
#include "nsIColorPicker.h"
#include "nsString.h"

// Don't activate the GTK3 color picker for now, because it is missing a few
// things, mainly the ability to let the user select a color on the screen.
// See bug 1198256.
#undef ACTIVATE_GTK3_COLOR_PICKER

class nsIWidget;

class nsColorPicker final : public nsIColorPicker
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOLORPICKER

  nsColorPicker() {};

private:
  ~nsColorPicker() {};

  static nsString ToHexString(int n);
  
  static void OnResponse(GtkWidget* dialog, gint response_id,
                         gpointer user_data);
  static void OnDestroy(GtkWidget* dialog, gpointer user_data);
  
#if defined(ACTIVATE_GTK3_COLOR_PICKER) && GTK_CHECK_VERSION(3,4,0)
  static void OnColorChanged(GtkColorChooser* color_chooser, GdkRGBA* color,
                             gpointer user_data);
                             
  static int convertGdkRgbaComponent(gdouble color_component);
  static gdouble convertToGdkRgbaComponent(int color_component);
  static GdkRGBA convertToRgbaColor(nscolor color);
  
  void Update(GdkRGBA* color);
  void SetColor(const GdkRGBA* color);
#else
  static void OnColorChanged(GtkColorSelection* colorselection,
                             gpointer user_data);

  // Conversion functions for color
  static int convertGdkColorComponent(guint16 color_component);
  static guint16 convertToGdkColorComponent(int color_component);
  static GdkColor convertToGdkColor(nscolor color);  

  static GtkColorSelection* WidgetGetColorSelection(GtkWidget* widget);
  
  void Update(GtkColorSelection* colorselection);
  void ReadValueFromColorSelection(GtkColorSelection* colorselection);
#endif

  void Done(GtkWidget* dialog, gint response_id);

  nsCOMPtr<nsIWidget> mParentWidget;
  nsCOMPtr<nsIColorPickerShownCallback> mCallback;
  nsString mTitle;
  nsString mColor;
  nsString mInitialColor;
};

#endif // nsColorPicker_h__
