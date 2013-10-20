/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gtk/gtk.h>

#include "mozilla/Util.h"
#include "nsColor.h"
#include "nsColorPicker.h"
#include "nsGtkUtils.h"
#include "nsIWidget.h"
#include "WidgetUtils.h"

NS_IMPL_ISUPPORTS1(nsColorPicker, nsIColorPicker)

int nsColorPicker::convertGdkColorComponent(guint16 color_component) {
  // GdkColor value is in range [0..65535]. We need something in range [0..255]
  return (int(color_component)*255 + 127)/65535;
}

guint16 nsColorPicker::convertToGdkColorComponent(int color_component) {
  return color_component*65535/255;
}

GdkColor nsColorPicker::convertToGdkColor(nscolor color) {
  GdkColor result = { 0 /* obsolete, unused 'pixel' value */,
      convertToGdkColorComponent(NS_GET_R(color)),
      convertToGdkColorComponent(NS_GET_G(color)),
      convertToGdkColorComponent(NS_GET_B(color)) };

  return result;
}

/* void init (in nsIDOMWindow parent, in AString title, in short mode); */
NS_IMETHODIMP nsColorPicker::Init(nsIDOMWindow *parent,
                                  const nsAString& title,
                                  const nsAString& initialColor)
{
  // Input color string should be 7 length (i.e. a string representing a valid
  // simple color)
  if (initialColor.Length() != 7) {
    return NS_ERROR_INVALID_ARG;
  }

  const nsAString& withoutHash  = StringTail(initialColor, 6);
  nscolor color;
  if (!NS_HexToRGB(withoutHash, &color)) {
    return NS_ERROR_INVALID_ARG;
  }

  mDefaultColor = convertToGdkColor(color);

  mParentWidget = mozilla::widget::WidgetUtils::DOMWindowToWidget(parent);
  mTitle.Assign(title);

  return NS_OK;
}

/* void open (in nsIColorPickerShownCallback aColorPickerShownCallback); */
NS_IMETHODIMP nsColorPicker::Open(nsIColorPickerShownCallback *aColorPickerShownCallback)
{
  if (mCallback) {
    // It means Open has already been called: this is not allowed
    NS_WARNING("mCallback is already set. Open called twice?");
    return NS_ERROR_FAILURE;
  }
  mCallback = aColorPickerShownCallback;

  nsXPIDLCString title;
  title.Adopt(ToNewUTF8String(mTitle));
  GtkWidget *color_chooser = gtk_color_selection_dialog_new(title);

  GtkWindow *window = GTK_WINDOW(color_chooser);
  gtk_window_set_modal(window, TRUE);

  GtkWindow *parent_window = GTK_WINDOW(mParentWidget->GetNativeData(NS_NATIVE_SHELLWIDGET));
  if (parent_window) {
    gtk_window_set_transient_for(window, parent_window);
    gtk_window_set_destroy_with_parent(window, TRUE);
  }

  gtk_color_selection_set_current_color(
      GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(
        GTK_COLOR_SELECTION_DIALOG(color_chooser))),
      &mDefaultColor);

  NS_ADDREF_THIS();
  g_signal_connect(color_chooser, "response", G_CALLBACK(OnResponse), this);
  g_signal_connect(color_chooser, "destroy", G_CALLBACK(OnDestroy), this);
  gtk_widget_show(color_chooser);

  return NS_OK;
}

/* static */ void
nsColorPicker::OnResponse(GtkWidget* color_chooser, gint response_id,
                          gpointer user_data)
{
  static_cast<nsColorPicker*>(user_data)->
    Done(color_chooser, response_id);
}

/* static */ void
nsColorPicker::OnDestroy(GtkWidget* color_chooser, gpointer user_data)
{
  static_cast<nsColorPicker*>(user_data)->
    Done(color_chooser, GTK_RESPONSE_CANCEL);
}

void
nsColorPicker::Done(GtkWidget* color_chooser, gint response)
{
  switch (response) {
    case GTK_RESPONSE_OK:
    case GTK_RESPONSE_ACCEPT:
      ReadValueFromColorChooser(color_chooser);
      break;
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_CLOSE:
    case GTK_RESPONSE_DELETE_EVENT:
      break;
    default:
      NS_WARNING("Unexpected response");
      break;
  }

  // A "response" signal won't be sent again but "destroy" will be.
  g_signal_handlers_disconnect_by_func(color_chooser,
                                       FuncToGpointer(OnDestroy), this);

  gtk_widget_destroy(color_chooser);
  if (mCallback) {
    mCallback->Done(mColor);
    mCallback = nullptr;
  }

  NS_RELEASE_THIS();
}

nsString nsColorPicker::ToHexString(int n)
{
  nsString result;
  if (n <= 0x0F) {
    result.Append('0');
  }
  result.AppendInt(n, 16);
  return result;
}

void nsColorPicker::ReadValueFromColorChooser(GtkWidget* color_chooser)
{
  GdkColor rgba;
  gtk_color_selection_get_current_color(
    GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(
      GTK_COLOR_SELECTION_DIALOG(color_chooser))),
    &rgba);

  mColor.AssignLiteral("#");
  mColor += ToHexString(convertGdkColorComponent(rgba.red));
  mColor += ToHexString(convertGdkColorComponent(rgba.green));
  mColor += ToHexString(convertGdkColorComponent(rgba.blue));
}
