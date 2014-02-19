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

class nsIWidget;

class nsColorPicker MOZ_FINAL : public nsIColorPicker
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOLORPICKER

  nsColorPicker() {};

private:
  ~nsColorPicker() {};

  static void OnColorChanged(GtkColorSelection* colorselection,
                             gpointer user_data);
  static void OnResponse(GtkWidget* dialog, gint response_id,
                         gpointer user_data);
  static void OnDestroy(GtkWidget* dialog, gpointer user_data);

  // Conversion functions for color
  static int convertGdkColorComponent(guint16 color_component);
  static guint16 convertToGdkColorComponent(int color_component);
  static GdkColor convertToGdkColor(nscolor color);
  static nsString ToHexString(int n);

  static GtkColorSelection* WidgetGetColorSelection(GtkWidget* widget);

  void Done(GtkWidget* dialog, gint response_id);
  void Update(GtkColorSelection* colorselection);
  void ReadValueFromColorSelection(GtkColorSelection* colorselection);

  nsCOMPtr<nsIWidget> mParentWidget;
  nsCOMPtr<nsIColorPickerShownCallback> mCallback;
  nsString mTitle;
  nsString mColor;
  nsString mInitialColor;
};

#endif // nsColorPicker_h__
