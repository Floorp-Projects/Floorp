/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gtk/gtk.h>

#include "nsColor.h"
#include "nsColorPicker.h"
#include "nsGtkUtils.h"
#include "nsIWidget.h"
#include "WidgetUtils.h"
#include "nsPIDOMWindow.h"

NS_IMPL_ISUPPORTS(nsColorPicker, nsIColorPicker)

#if defined(ACTIVATE_GTK3_COLOR_PICKER) && GTK_CHECK_VERSION(3,4,0)
int nsColorPicker::convertGdkRgbaComponent(gdouble color_component) {
  // GdkRGBA value is in range [0.0..1.0]. We need something in range [0..255]
  return color_component * 255 + 0.5;
}

gdouble nsColorPicker::convertToGdkRgbaComponent(int color_component) { 
  return color_component / 255.0;
}  

GdkRGBA nsColorPicker::convertToRgbaColor(nscolor color) {
  GdkRGBA result = { convertToGdkRgbaComponent(NS_GET_R(color)),
                     convertToGdkRgbaComponent(NS_GET_G(color)),
                     convertToGdkRgbaComponent(NS_GET_B(color)),
                     convertToGdkRgbaComponent(NS_GET_A(color)) };
       
  return result;
}
#else
int nsColorPicker::convertGdkColorComponent(guint16 color_component) {
  // GdkColor value is in range [0..65535]. We need something in range [0..255]
  return (color_component * 255 + 127) / 65535;
}

guint16 nsColorPicker::convertToGdkColorComponent(int color_component) {
  return color_component * 65535 / 255;
}

GdkColor nsColorPicker::convertToGdkColor(nscolor color) {
  GdkColor result = { 0 /* obsolete, unused 'pixel' value */,
      convertToGdkColorComponent(NS_GET_R(color)),
      convertToGdkColorComponent(NS_GET_G(color)),
      convertToGdkColorComponent(NS_GET_B(color)) };

  return result;
}

GtkColorSelection* nsColorPicker::WidgetGetColorSelection(GtkWidget* widget)
{
  return GTK_COLOR_SELECTION(gtk_color_selection_dialog_get_color_selection(
                             GTK_COLOR_SELECTION_DIALOG(widget)));
}
#endif

NS_IMETHODIMP nsColorPicker::Init(mozIDOMWindowProxy *aParent,
                                  const nsAString& title,
                                  const nsAString& initialColor)
{
  auto* parent = nsPIDOMWindowOuter::From(aParent);
  mParentWidget = mozilla::widget::WidgetUtils::DOMWindowToWidget(parent);
  mTitle = title;
  mInitialColor = initialColor;

  return NS_OK;
}

NS_IMETHODIMP nsColorPicker::Open(nsIColorPickerShownCallback *aColorPickerShownCallback)
{

  // Input color string should be 7 length (i.e. a string representing a valid
  // simple color)
  if (mInitialColor.Length() != 7) {
    return NS_ERROR_FAILURE;
  }

  const nsAString& withoutHash  = StringTail(mInitialColor, 6);
  nscolor color;
  if (!NS_HexToRGBA(withoutHash, nsHexColorType::NoAlpha, &color)) {
    return NS_ERROR_FAILURE;
  }

  if (mCallback) {
    // It means Open has already been called: this is not allowed
    NS_WARNING("mCallback is already set. Open called twice?");
    return NS_ERROR_FAILURE;
  }
  mCallback = aColorPickerShownCallback;

  nsXPIDLCString title;
  title.Adopt(ToNewUTF8String(mTitle));
  GtkWindow *parent_window = GTK_WINDOW(mParentWidget->GetNativeData(NS_NATIVE_SHELLWIDGET));
  
#if defined(ACTIVATE_GTK3_COLOR_PICKER) && GTK_CHECK_VERSION(3,4,0)
  GtkWidget* color_chooser = gtk_color_chooser_dialog_new(title, parent_window);
    
  if (parent_window) {
      gtk_window_set_destroy_with_parent(GTK_WINDOW(color_chooser), TRUE);
  }
  
  gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(color_chooser), FALSE);
  GdkRGBA color_rgba = convertToRgbaColor(color);    
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color_chooser),
                             &color_rgba);
  
  g_signal_connect(GTK_COLOR_CHOOSER(color_chooser), "color-activated",
                   G_CALLBACK(OnColorChanged), this);
#else
  GtkWidget *color_chooser = gtk_color_selection_dialog_new(title);
  
  if (parent_window) {
    GtkWindow *window = GTK_WINDOW(color_chooser);
    gtk_window_set_transient_for(window, parent_window);
    gtk_window_set_destroy_with_parent(window, TRUE);
  }

  GdkColor color_gdk = convertToGdkColor(color);
  gtk_color_selection_set_current_color(WidgetGetColorSelection(color_chooser),
                                        &color_gdk);
  
  g_signal_connect(WidgetGetColorSelection(color_chooser), "color-changed",
                   G_CALLBACK(OnColorChanged), this);
#endif

  NS_ADDREF_THIS();
  
  g_signal_connect(color_chooser, "response", G_CALLBACK(OnResponse), this);
  g_signal_connect(color_chooser, "destroy", G_CALLBACK(OnDestroy), this);
  gtk_widget_show(color_chooser);

  return NS_OK;
}

#if defined(ACTIVATE_GTK3_COLOR_PICKER) && GTK_CHECK_VERSION(3,4,0)
/* static */ void
nsColorPicker::OnColorChanged(GtkColorChooser* color_chooser, GdkRGBA* color,
                              gpointer user_data)
{
  static_cast<nsColorPicker*>(user_data)->Update(color);
}

void
nsColorPicker::Update(GdkRGBA* color)
{
  SetColor(color);
  if (mCallback) {
    mCallback->Update(mColor);
  }
}

void nsColorPicker::SetColor(const GdkRGBA* color)
{
  mColor.Assign('#');
  mColor += ToHexString(convertGdkRgbaComponent(color->red));
  mColor += ToHexString(convertGdkRgbaComponent(color->green));
  mColor += ToHexString(convertGdkRgbaComponent(color->blue));
}
#else
/* static */ void
nsColorPicker::OnColorChanged(GtkColorSelection* colorselection,
                              gpointer user_data)
{
  static_cast<nsColorPicker*>(user_data)->Update(colorselection);
}

void
nsColorPicker::Update(GtkColorSelection* colorselection)
{
  ReadValueFromColorSelection(colorselection);
  if (mCallback) {
    mCallback->Update(mColor);
  }
}

void nsColorPicker::ReadValueFromColorSelection(GtkColorSelection* colorselection)
{
  GdkColor rgba;
  gtk_color_selection_get_current_color(colorselection, &rgba);

  mColor.Assign('#');
  mColor += ToHexString(convertGdkColorComponent(rgba.red));
  mColor += ToHexString(convertGdkColorComponent(rgba.green));
  mColor += ToHexString(convertGdkColorComponent(rgba.blue));
}
#endif

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
#if defined(ACTIVATE_GTK3_COLOR_PICKER) && GTK_CHECK_VERSION(3,4,0)
      GdkRGBA color;
      gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(color_chooser), &color);
      SetColor(&color);
#else
      ReadValueFromColorSelection(WidgetGetColorSelection(color_chooser));
#endif
      break;
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_CLOSE:
    case GTK_RESPONSE_DELETE_EVENT:
      mColor = mInitialColor;
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
