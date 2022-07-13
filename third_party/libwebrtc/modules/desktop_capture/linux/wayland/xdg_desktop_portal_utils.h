/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_XDG_DESKTOP_PORTAL_UTILS_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_XDG_DESKTOP_PORTAL_UTILS_H_

#include <gio/gio.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "modules/desktop_capture/linux/wayland/scoped_glib.h"
#include "modules/desktop_capture/linux/wayland/xdg_session_details.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace xdg_portal {

constexpr char kDesktopBusName[] = "org.freedesktop.portal.Desktop";
constexpr char kDesktopObjectPath[] = "/org/freedesktop/portal/desktop";
constexpr char kDesktopRequestObjectPath[] =
    "/org/freedesktop/portal/desktop/request";
constexpr char kSessionInterfaceName[] = "org.freedesktop.portal.Session";
constexpr char kRequestInterfaceName[] = "org.freedesktop.portal.Request";
constexpr char kScreenCastInterfaceName[] = "org.freedesktop.portal.ScreenCast";

using ProxyRequestCallback = void (*)(GObject*, GAsyncResult*, gpointer);
using SessionRequestCallback = void (*)(GDBusProxy*, GAsyncResult*, gpointer);
using SessionRequestResponseSignalHandler = void (*)(GDBusConnection*,
                                                     const char*,
                                                     const char*,
                                                     const char*,
                                                     const char*,
                                                     GVariant*,
                                                     gpointer);
using SessionRequestResponseSignalCallback = void (*)(std::string);
using SessionClosedSignalHandler = void (*)(GDBusConnection*,
                                            const char*,
                                            const char*,
                                            const char*,
                                            const char*,
                                            GVariant*,
                                            gpointer);
using StartRequestResponseSignalHandler = void (*)(GDBusConnection*,
                                                   const char*,
                                                   const char*,
                                                   const char*,
                                                   const char*,
                                                   GVariant*,
                                                   gpointer);
using SessionStartRequestedHandler = void (*)(GDBusProxy*,
                                              GAsyncResult*,
                                              gpointer);

// Contains type of responses that can be observed when making a request to
// a desktop portal interface.
enum class RequestResponse {
  // Success, the request is carried out.
  kSuccess,
  // The user cancelled the interaction.
  kUserCancelled,
  // The user interaction was ended in some other way.
  kError,

  kMaxValue = kError,
};

std::string RequestResponseToString(RequestResponse request);

// Returns a string path for signal handle based on the provided connection and
// token.
std::string PrepareSignalHandle(const char* token, GDBusConnection* connection);

// Sets up the callback to execute when a response signal is received for the
// given object.
uint32_t SetupRequestResponseSignal(const char* object_path,
                                    const GDBusSignalCallback callback,
                                    gpointer user_data,
                                    GDBusConnection* connection);

void RequestSessionProxy(const char* interface_name,
                         const ProxyRequestCallback proxy_request_callback,
                         GCancellable* cancellable,
                         gpointer user_data);

void SetupSessionRequestHandlers(
    const std::string& portal_prefix,
    const SessionRequestCallback session_request_callback,
    const SessionRequestResponseSignalHandler request_response_signale_handler,
    GDBusConnection* connection,
    GDBusProxy* proxy,
    GCancellable* cancellable,
    std::string& portal_handle,
    guint& session_request_signal_id,
    gpointer user_data);

void StartSessionRequest(
    const std::string& prefix,
    const std::string session_handle,
    const StartRequestResponseSignalHandler signal_handler,
    const SessionStartRequestedHandler session_started_handler,
    GDBusProxy* proxy,
    GDBusConnection* connection,
    GCancellable* cancellable,
    guint& start_request_signal_id,
    std::string& start_handle,
    gpointer user_data);

// Tears down the portal session and cleans up related objects.
void TearDownSession(std::string session_handle,
                     GDBusProxy* proxy,
                     GCancellable* cancellable,
                     GDBusConnection* connection);

template <typename T>
void RequestSessionUsingProxy(T* portal,
                              GObject* gobject,
                              GAsyncResult* result) {
  RTC_DCHECK(portal);
  Scoped<GError> error;
  GDBusProxy* proxy = g_dbus_proxy_new_finish(result, error.receive());
  if (!proxy) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to get a proxy for the portal: "
                      << error->message;
    portal->PortalFailed(RequestResponse::kError);
    return;
  }

  RTC_LOG(LS_INFO) << "Successfully created proxy for the portal.";
  portal->SessionRequest(proxy);
}

template <typename T>
void SessionRequestHandler(T* portal,
                           GDBusProxy* proxy,
                           GAsyncResult* result,
                           gpointer user_data) {
  RTC_DCHECK(portal);

  Scoped<GError> error;
  Scoped<GVariant> variant(
      g_dbus_proxy_call_finish(proxy, result, error.receive()));
  if (!variant) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to session: " << error->message;
    portal->PortalFailed(RequestResponse::kError);
    return;
  }

  RTC_LOG(LS_INFO) << "Initializing the session.";

  Scoped<char> handle;
  g_variant_get_child(variant.get(), /*index=*/0, /*format_string=*/"o",
                      &handle);
  if (!handle) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the session.";
    portal->UnsubscribeSignalHandlers();
    portal->PortalFailed(RequestResponse::kError);
    return;
  }
}

template <typename T>
void SessionRequestResponseSignalHelper(
    const SessionClosedSignalHandler session_close_signal_handler,
    T* portal,
    GDBusConnection* connection,
    std::string& session_handle,
    GVariant* parameters,
    guint& session_closed_signal_id) {
  uint32_t portal_response;
  Scoped<GVariant> response_data;
  g_variant_get(parameters, /*format_string=*/"(u@a{sv})", &portal_response,
                response_data.receive());
  Scoped<GVariant> g_session_handle(
      g_variant_lookup_value(response_data.get(), /*key=*/"session_handle",
                             /*expected_type=*/nullptr));
  session_handle = g_variant_dup_string(
      /*value=*/g_session_handle.get(), /*length=*/nullptr);

  if (session_handle.empty() || portal_response) {
    RTC_LOG(LS_ERROR) << "Failed to request the session subscription.";
    portal->PortalFailed(RequestResponse::kError);
    return;
  }

  session_closed_signal_id = g_dbus_connection_signal_subscribe(
      connection, kDesktopBusName, kSessionInterfaceName, /*member=*/"Closed",
      session_handle.c_str(), /*arg0=*/nullptr, G_DBUS_SIGNAL_FLAGS_NONE,
      session_close_signal_handler, portal, /*user_data_free_func=*/nullptr);
}

template <typename T>
void StartRequestedHandler(T* portal, GDBusProxy* proxy, GAsyncResult* result) {
  RTC_DCHECK(portal);
  Scoped<GError> error;
  Scoped<GVariant> variant(
      g_dbus_proxy_call_finish(proxy, result, error.receive()));
  if (!variant) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to start the portal session: "
                      << error->message;
    portal->PortalFailed(RequestResponse::kError);
    return;
  }

  Scoped<char> handle;
  g_variant_get_child(variant.get(), 0, "o", handle.receive());
  if (!handle) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the start portal session.";
    portal->UnsubscribeSignalHandlers();
    portal->PortalFailed(RequestResponse::kError);
    return;
  }

  RTC_LOG(LS_INFO) << "Subscribed to the start signal.";
}

}  // namespace xdg_portal
}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_XDG_DESKTOP_PORTAL_UTILS_H_
