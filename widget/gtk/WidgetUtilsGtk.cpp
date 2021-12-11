/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WidgetUtilsGtk.h"

#include "mozilla/UniquePtr.h"
#include "nsReadableUtils.h"
#include "nsWindow.h"

#include <gtk/gtk.h>
#include <dlfcn.h>
#include <glib.h>

namespace mozilla::widget {

int32_t WidgetUtilsGTK::IsTouchDeviceSupportPresent() {
  int32_t result = 0;
  GdkDisplay* display = gdk_display_get_default();
  if (!display) {
    return 0;
  }

  GdkDeviceManager* manager = gdk_display_get_device_manager(display);
  if (!manager) {
    return 0;
  }

  GList* devices =
      gdk_device_manager_list_devices(manager, GDK_DEVICE_TYPE_SLAVE);
  GList* list = devices;

  while (devices) {
    GdkDevice* device = static_cast<GdkDevice*>(devices->data);
    if (gdk_device_get_source(device) == GDK_SOURCE_TOUCHSCREEN) {
      result = 1;
      break;
    }
    devices = devices->next;
  }

  if (list) {
    g_list_free(list);
  }

  return result;
}

bool IsMainWindowTransparent() {
  return nsWindow::IsToplevelWindowTransparent();
}

// We avoid linking gdk_*_display_get_type directly in order to avoid a runtime
// dependency on GTK built with both backends. Other X11- and Wayland-specific
// functions get stubbed out by libmozgtk and crash when called, but those
// should only be called when the matching backend is already in use.

bool GdkIsWaylandDisplay(GdkDisplay* display) {
  static auto sGdkWaylandDisplayGetType =
      (GType(*)())dlsym(RTLD_DEFAULT, "gdk_wayland_display_get_type");
  return sGdkWaylandDisplayGetType &&
         G_TYPE_CHECK_INSTANCE_TYPE(display, sGdkWaylandDisplayGetType());
}

bool GdkIsX11Display(GdkDisplay* display) {
  static auto sGdkX11DisplayGetType =
      (GType(*)())dlsym(RTLD_DEFAULT, "gdk_x11_display_get_type");
  return sGdkX11DisplayGetType &&
         G_TYPE_CHECK_INSTANCE_TYPE(display, sGdkX11DisplayGetType());
}

bool GdkIsWaylandDisplay() {
  static bool isWaylandDisplay =
      gdk_display_get_default() ? GdkIsWaylandDisplay(gdk_display_get_default())
                                : false;
  return isWaylandDisplay;
}

bool GdkIsX11Display() {
  static bool isX11Display = gdk_display_get_default()
                                 ? GdkIsX11Display(gdk_display_get_default())
                                 : false;
  return isX11Display;
}

nsTArray<nsCString> ParseTextURIList(const nsACString& aData) {
  UniquePtr<char[]> data(ToNewCString(aData));
  gchar** uris = g_uri_list_extract_uris(data.get());

  nsTArray<nsCString> result;
  for (size_t i = 0; i < g_strv_length(uris); i++) {
    result.AppendElement(nsCString(uris[i]));
  }

  g_strfreev(uris);
  return result;
}

}  // namespace mozilla::widget
