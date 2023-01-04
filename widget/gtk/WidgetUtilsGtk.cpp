/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WidgetUtilsGtk.h"

#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/UniquePtr.h"
#include "nsReadableUtils.h"
#include "nsWindow.h"
#include "nsIGfxInfo.h"
#include "mozilla/Components.h"
#include "nsGtkKeyUtils.h"
#include "nsGtkUtils.h"

#include <gtk/gtk.h>
#include <dlfcn.h>
#include <glib.h>

#ifdef MOZ_LOGGING
#  include "mozilla/Logging.h"
extern mozilla::LazyLogModule gWidgetLog;
#  define LOGW(...) MOZ_LOG(gWidgetLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#else
#  define LOGW(...)
#endif /* MOZ_LOGGING */

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

bool IsXWaylandProtocol() {
  static bool isXwayland = [] {
    return !GdkIsWaylandDisplay() && !!getenv("WAYLAND_DISPLAY");
  }();
  return isXwayland;
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
GdkEvent* GetLastMousePressEvent() { return sLastMousePressEvent; }

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

bool IsRunningUnderSnap() { return !!GetSnapInstanceName(); }

bool IsRunningUnderFlatpak() {
  // https://gitlab.gnome.org/GNOME/gtk/-/blob/4300a5c609306ce77cbc8a3580c19201dccd8d13/gdk/gdk.c#L472
  static bool sRunning = [] {
    return g_file_test("/.flatpak-info", G_FILE_TEST_EXISTS);
  }();
  return sRunning;
}

const char* GetSnapInstanceName() {
  static const char* sInstanceName = []() -> const char* {
    const char* snapName = g_getenv("SNAP_NAME");
    if (!snapName) {
      return nullptr;
    }
    if (g_strcmp0(snapName, MOZ_APP_NAME)) {
      return nullptr;
    }
    // Intentionally leaked, as keeping a pointer to the environment forever is
    // a bit suspicious.
    if (const char* instanceName = g_getenv("SNAP_INSTANCE_NAME")) {
      return g_strdup(instanceName);
    }
    // Instance name didn't exist for snapd <= 2.35:
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

#ifdef MOZ_WAYLAND
static gboolean token_failed(gpointer aData);

class XDGTokenRequest {
 public:
  void SetTokenID(const char* aTokenID) {
    mTransferPromise->Resolve(aTokenID, __func__);
  }
  void Cancel() {
    mTransferPromise->Reject(false, __func__);
    mActivationTimeoutID = 0;
  }

  XDGTokenRequest(xdg_activation_token_v1* aXdgToken,
                  RefPtr<FocusRequestPromise::Private> aTransferPromise)
      : mXdgToken(aXdgToken), mTransferPromise(std::move(aTransferPromise)) {
    mActivationTimeoutID =
        g_timeout_add(sActivationTimeout, token_failed, this);
  }
  ~XDGTokenRequest() {
    MozClearPointer(mXdgToken, xdg_activation_token_v1_destroy);
    if (mActivationTimeoutID) {
      g_source_remove(mActivationTimeoutID);
    }
  }

 private:
  xdg_activation_token_v1* mXdgToken;
  RefPtr<FocusRequestPromise::Private> mTransferPromise;
  guint mActivationTimeoutID;
  // Reject FocusRequestPromise if we don't get XDG token in 0.5 sec.
  static constexpr int sActivationTimeout = 500;
};

// Failed to get token in time
static gboolean token_failed(gpointer data) {
  UniquePtr<XDGTokenRequest> request(static_cast<XDGTokenRequest*>(data));
  request->Cancel();
  return false;
}

// We've got activation token from Wayland compositor so it's time to use it.
static void token_done(gpointer data, struct xdg_activation_token_v1* provider,
                       const char* tokenID) {
  UniquePtr<XDGTokenRequest> request(static_cast<XDGTokenRequest*>(data));
  request->SetTokenID(tokenID);
}

static const struct xdg_activation_token_v1_listener token_listener = {
    token_done,
};
#endif

RefPtr<FocusRequestPromise> RequestWaylandFocusPromise() {
#ifdef MOZ_WAYLAND
  if (!GdkIsWaylandDisplay() || !nsWindow::GetFocusedWindow() ||
      nsWindow::GetFocusedWindow()->IsDestroyed()) {
    return nullptr;
  }

  RefPtr<nsWindow> sourceWindow = nsWindow::GetFocusedWindow();
  if (!sourceWindow) {
    return nullptr;
  }

  RefPtr<nsWaylandDisplay> display = WaylandDisplayGet();
  xdg_activation_v1* xdg_activation = display->GetXdgActivation();
  if (!xdg_activation) {
    return nullptr;
  }

  wl_surface* focusSurface;
  uint32_t focusSerial;
  KeymapWrapper::GetFocusInfo(&focusSurface, &focusSerial);
  if (!focusSurface) {
    return nullptr;
  }

  GdkWindow* gdkWindow = gtk_widget_get_window(sourceWindow->GetGtkWidget());
  if (!gdkWindow) {
    return nullptr;
  }
  wl_surface* surface = gdk_wayland_window_get_wl_surface(gdkWindow);
  if (focusSurface != surface) {
    return nullptr;
  }

  RefPtr<FocusRequestPromise::Private> transferPromise =
      new FocusRequestPromise::Private(__func__);

  xdg_activation_token_v1* aXdgToken =
      xdg_activation_v1_get_activation_token(xdg_activation);
  xdg_activation_token_v1_add_listener(
      aXdgToken, &token_listener,
      new XDGTokenRequest(aXdgToken, transferPromise));
  xdg_activation_token_v1_set_serial(aXdgToken, focusSerial,
                                     KeymapWrapper::GetSeat());
  xdg_activation_token_v1_set_surface(aXdgToken, focusSurface);
  xdg_activation_token_v1_commit(aXdgToken);

  return transferPromise.forget();
#else  // !defined(MOZ_WAYLAND)
  return nullptr;
#endif
}

}  // namespace mozilla::widget
