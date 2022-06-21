/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WidgetUtilsGtk.h"

#include "mozilla/StaticPrefs_widget.h"
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
  static bool isWaylandDisplay = gdk_display_get_default() &&
                                 GdkIsWaylandDisplay(gdk_display_get_default());
  return isWaylandDisplay;
}

bool GdkIsX11Display() {
  static bool isX11Display = gdk_display_get_default()
                                 ? GdkIsX11Display(gdk_display_get_default())
                                 : false;
  return isX11Display;
}

GdkDevice* GdkGetPointer() {
  GdkDisplay* display = gdk_display_get_default();
  GdkDeviceManager* deviceManager = gdk_display_get_device_manager(display);
  return gdk_device_manager_get_client_pointer(deviceManager);
}

static GdkEvent* sLastMousePressEvent = nullptr;
GdkEvent* GetLastMousePressEvent() {
  return sLastMousePressEvent;
}

void SetLastMousePressEvent(GdkEvent* aEvent) {
  if (sLastMousePressEvent) {
    GUniquePtr<GdkEvent> event(sLastMousePressEvent);
    sLastMousePressEvent = nullptr;
  }
  if (!aEvent) {
    return;
  }
  GUniquePtr<GdkEvent> event(gdk_event_copy(aEvent));
  sLastMousePressEvent = event.release();
}


bool IsRunningUnderFlatpak() {
  // https://gitlab.gnome.org/GNOME/gtk/-/blob/4300a5c609306ce77cbc8a3580c19201dccd8d13/gdk/gdk.c#L472
  static bool sRunning = [] {
    return g_file_test("/.flatpak-info", G_FILE_TEST_EXISTS);
  }();
  return sRunning;
}

const char* GetSnapInstanceName() {
  static const char* sInstanceName = []() -> const char* {
    // Intentionally leaked, as keeping a pointer to the environment forever is
    // a bit suspicious.
    if (const char* instanceName = g_getenv("SNAP_INSTANCE_NAME")) {
      return g_strdup(instanceName);
    }
    // Compatibility for snapd <= 2.35:
    if (const char* instanceName = g_getenv("SNAP_NAME")) {
      return g_strdup(instanceName);
    }
    return nullptr;
  }();

  return sInstanceName;
}

bool ShouldUsePortal(PortalKind aPortalKind) {
  static bool sPortalEnv = [] {
    if (IsRunningUnderFlatpakOrSnap()) {
      return true;
    }
    const char* portalEnvString = g_getenv("GTK_USE_PORTAL");
    return portalEnvString && atoi(portalEnvString) != 0;
  }();

  bool autoBehavior = sPortalEnv;
  const int32_t pref = [&] {
    switch (aPortalKind) {
      case PortalKind::FilePicker:
        return StaticPrefs::widget_use_xdg_desktop_portal_file_picker();
      case PortalKind::MimeHandler:
        // Mime portal breaks default browser handling, see bug 1516290.
        autoBehavior = IsRunningUnderFlatpakOrSnap();
        return StaticPrefs::widget_use_xdg_desktop_portal_mime_handler();
      case PortalKind::Settings:
        autoBehavior = true;
        return StaticPrefs::widget_use_xdg_desktop_portal_settings();
      case PortalKind::Location:
        return StaticPrefs::widget_use_xdg_desktop_portal_location();
      case PortalKind::OpenUri:
        return StaticPrefs::widget_use_xdg_desktop_portal_open_uri();
    }
    return 2;
  }();

  switch (pref) {
    case 0:
      return false;
    case 1:
      return true;
    default:
      return autoBehavior;
  }
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
