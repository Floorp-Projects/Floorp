/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/screencast_portal.h"

#include <gio/gunixfdlist.h>
#include <glib-object.h>

#include "modules/desktop_capture/linux/wayland/scoped_glib.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

const char kDesktopBusName[] = "org.freedesktop.portal.Desktop";
const char kDesktopObjectPath[] = "/org/freedesktop/portal/desktop";
const char kDesktopRequestObjectPath[] =
    "/org/freedesktop/portal/desktop/request";
const char kSessionInterfaceName[] = "org.freedesktop.portal.Session";
const char kRequestInterfaceName[] = "org.freedesktop.portal.Request";
const char kScreenCastInterfaceName[] = "org.freedesktop.portal.ScreenCast";

ScreenCastPortal::ScreenCastPortal(CaptureSourceType source_type,
                                   PortalNotifier* notifier)
    : notifier_(notifier), capture_source_type_(source_type) {}

ScreenCastPortal::~ScreenCastPortal() {
  if (start_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_, start_request_signal_id_);
  }
  if (sources_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_,
                                         sources_request_signal_id_);
  }
  if (session_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_,
                                         session_request_signal_id_);
  }

  if (!session_handle_.empty()) {
    Scoped<GDBusMessage> message(
        g_dbus_message_new_method_call(kDesktopBusName, session_handle_.c_str(),
                                       kSessionInterfaceName, "Close"));
    if (message.get()) {
      Scoped<GError> error;
      g_dbus_connection_send_message(connection_, message.get(),
                                     G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                     /*out_serial=*/nullptr, error.receive());
      if (error.get()) {
        RTC_LOG(LS_ERROR) << "Failed to close the session: " << error->message;
      }
    }
  }

  if (cancellable_) {
    g_cancellable_cancel(cancellable_);
    g_object_unref(cancellable_);
    cancellable_ = nullptr;
  }

  if (proxy_) {
    g_object_unref(proxy_);
    proxy_ = nullptr;
  }

  if (pw_fd_ != -1) {
    close(pw_fd_);
  }
}

void ScreenCastPortal::Start() {
  cancellable_ = g_cancellable_new();
  g_dbus_proxy_new_for_bus(
      G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, /*info=*/nullptr,
      kDesktopBusName, kDesktopObjectPath, kScreenCastInterfaceName,
      cancellable_, reinterpret_cast<GAsyncReadyCallback>(OnProxyRequested),
      this);
}

void ScreenCastPortal::PortalFailed(RequestResponse result) {
  notifier_->OnScreenCastRequestResult(result, pw_stream_node_id_, pw_fd_);
}

uint32_t ScreenCastPortal::SetupRequestResponseSignal(
    const char* object_path,
    GDBusSignalCallback callback) {
  return g_dbus_connection_signal_subscribe(
      connection_, kDesktopBusName, kRequestInterfaceName, "Response",
      object_path, /*arg0=*/nullptr, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
      callback, this, /*user_data_free_func=*/nullptr);
}

// static
void ScreenCastPortal::OnProxyRequested(GObject* /*object*/,
                                        GAsyncResult* result,
                                        gpointer user_data) {
  ScreenCastPortal* that = static_cast<ScreenCastPortal*>(user_data);
  RTC_DCHECK(that);

  Scoped<GError> error;
  GDBusProxy* proxy = g_dbus_proxy_new_finish(result, error.receive());
  if (!proxy) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to create a proxy for the screen cast portal: "
                      << error->message;
    that->PortalFailed(RequestResponse::kError);
    return;
  }
  that->proxy_ = proxy;
  that->connection_ = g_dbus_proxy_get_connection(that->proxy_);

  RTC_LOG(LS_INFO) << "Created proxy for the screen cast portal.";

  that->SessionRequest();
}

// static
std::string ScreenCastPortal::PrepareSignalHandle(GDBusConnection* connection,
                                                  const char* token) {
  Scoped<char> sender(
      g_strdup(g_dbus_connection_get_unique_name(connection) + 1));
  for (int i = 0; sender.get()[i]; ++i) {
    if (sender.get()[i] == '.') {
      sender.get()[i] = '_';
    }
  }

  const char* handle = g_strconcat(kDesktopRequestObjectPath, "/", sender.get(),
                                   "/", token, /*end of varargs*/ nullptr);

  return handle;
}

void ScreenCastPortal::SessionRequest() {
  GVariantBuilder builder;
  Scoped<char> variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  variant_string =
      g_strdup_printf("webrtc_session%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "session_handle_token",
                        g_variant_new_string(variant_string.get()));
  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string.get()));

  portal_handle_ = PrepareSignalHandle(connection_, variant_string.get());
  session_request_signal_id_ = SetupRequestResponseSignal(
      portal_handle_.c_str(), OnSessionRequestResponseSignal);

  RTC_LOG(LS_INFO) << "Screen cast session requested.";
  g_dbus_proxy_call(proxy_, "CreateSession", g_variant_new("(a{sv})", &builder),
                    G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
                    reinterpret_cast<GAsyncReadyCallback>(OnSessionRequested),
                    this);
}

// static
void ScreenCastPortal::OnSessionRequested(GDBusProxy* proxy,
                                          GAsyncResult* result,
                                          gpointer user_data) {
  ScreenCastPortal* that = static_cast<ScreenCastPortal*>(user_data);
  RTC_DCHECK(that);

  Scoped<GError> error;
  Scoped<GVariant> variant(
      g_dbus_proxy_call_finish(proxy, result, error.receive()));
  if (!variant) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to create a screen cast session: "
                      << error->message;
    that->PortalFailed(RequestResponse::kError);
    return;
  }
  RTC_LOG(LS_INFO) << "Initializing the screen cast session.";

  Scoped<char> handle;
  g_variant_get_child(variant.get(), 0, "o", &handle);
  if (!handle) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the screen cast session.";
    if (that->session_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(that->connection_,
                                           that->session_request_signal_id_);
      that->session_request_signal_id_ = 0;
    }
    that->PortalFailed(RequestResponse::kError);
    return;
  }

  RTC_LOG(LS_INFO) << "Subscribing to the screen cast session.";
}

// static
void ScreenCastPortal::OnSessionRequestResponseSignal(
    GDBusConnection* connection,
    const char* sender_name,
    const char* object_path,
    const char* interface_name,
    const char* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  ScreenCastPortal* that = static_cast<ScreenCastPortal*>(user_data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO)
      << "Received response for the screen cast session subscription.";

  uint32_t portal_response;
  Scoped<GVariant> response_data;
  g_variant_get(parameters, "(u@a{sv})", &portal_response,
                response_data.receive());
  Scoped<GVariant> session_handle(
      g_variant_lookup_value(response_data.get(), "session_handle", nullptr));
  that->session_handle_ = g_variant_dup_string(session_handle.get(), nullptr);

  if (that->session_handle_.empty() || portal_response) {
    RTC_LOG(LS_ERROR)
        << "Failed to request the screen cast session subscription.";
    that->PortalFailed(RequestResponse::kError);
    return;
  }

  that->session_closed_signal_id_ = g_dbus_connection_signal_subscribe(
      that->connection_, kDesktopBusName, kSessionInterfaceName, "Closed",
      that->session_handle_.c_str(), /*arg0=*/nullptr, G_DBUS_SIGNAL_FLAGS_NONE,
      OnSessionClosedSignal, that, /*user_data_free_func=*/nullptr);

  that->SourcesRequest();
}

// static
void ScreenCastPortal::OnSessionClosedSignal(GDBusConnection* connection,
                                             const char* sender_name,
                                             const char* object_path,
                                             const char* interface_name,
                                             const char* signal_name,
                                             GVariant* parameters,
                                             gpointer user_data) {
  ScreenCastPortal* that = static_cast<ScreenCastPortal*>(user_data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "Received closed signal from session.";

  that->notifier_->OnScreenCastSessionClosed();

  // Unsubscribe from the signal and free the session handle to avoid calling
  // Session::Close from the destructor since it's already closed
  g_dbus_connection_signal_unsubscribe(that->connection_,
                                       that->session_closed_signal_id_);
}

void ScreenCastPortal::SourcesRequest() {
  GVariantBuilder builder;
  Scoped<char> variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  // We want to record monitor content.
  g_variant_builder_add(
      &builder, "{sv}", "types",
      g_variant_new_uint32(static_cast<uint32_t>(capture_source_type_)));
  // We don't want to allow selection of multiple sources.
  g_variant_builder_add(&builder, "{sv}", "multiple",
                        g_variant_new_boolean(false));

  Scoped<GVariant> variant(
      g_dbus_proxy_get_cached_property(proxy_, "AvailableCursorModes"));
  if (variant.get()) {
    uint32_t modes = 0;
    g_variant_get(variant.get(), "u", &modes);
    // Make request only if this mode is advertised by the portal
    // implementation.
    if (modes & static_cast<uint32_t>(cursor_mode_)) {
      g_variant_builder_add(
          &builder, "{sv}", "cursor_mode",
          g_variant_new_uint32(static_cast<uint32_t>(cursor_mode_)));
    }
  }

  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string.get()));

  sources_handle_ = PrepareSignalHandle(connection_, variant_string.get());
  sources_request_signal_id_ = SetupRequestResponseSignal(
      sources_handle_.c_str(), OnSourcesRequestResponseSignal);

  RTC_LOG(LS_INFO) << "Requesting sources from the screen cast session.";
  g_dbus_proxy_call(
      proxy_, "SelectSources",
      g_variant_new("(oa{sv})", session_handle_.c_str(), &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnSourcesRequested), this);
}

// static
void ScreenCastPortal::OnSourcesRequested(GDBusProxy* proxy,
                                          GAsyncResult* result,
                                          gpointer user_data) {
  ScreenCastPortal* that = static_cast<ScreenCastPortal*>(user_data);
  RTC_DCHECK(that);

  Scoped<GError> error;
  Scoped<GVariant> variant(
      g_dbus_proxy_call_finish(proxy, result, error.receive()));
  if (!variant) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to request the sources: " << error->message;
    that->PortalFailed(RequestResponse::kError);
    return;
  }

  RTC_LOG(LS_INFO) << "Sources requested from the screen cast session.";

  Scoped<char> handle;
  g_variant_get_child(variant.get(), 0, "o", handle.receive());
  if (!handle) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the screen cast session.";
    if (that->sources_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(that->connection_,
                                           that->sources_request_signal_id_);
      that->sources_request_signal_id_ = 0;
    }
    that->PortalFailed(RequestResponse::kError);
    return;
  }

  RTC_LOG(LS_INFO) << "Subscribed to sources signal.";
}

// static
void ScreenCastPortal::OnSourcesRequestResponseSignal(
    GDBusConnection* connection,
    const char* sender_name,
    const char* object_path,
    const char* interface_name,
    const char* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  ScreenCastPortal* that = static_cast<ScreenCastPortal*>(user_data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "Received sources signal from session.";

  uint32_t portal_response;
  g_variant_get(parameters, "(u@a{sv})", &portal_response, nullptr);
  if (portal_response) {
    RTC_LOG(LS_ERROR)
        << "Failed to select sources for the screen cast session.";
    that->PortalFailed(RequestResponse::kError);
    return;
  }

  that->StartRequest();
}

void ScreenCastPortal::StartRequest() {
  GVariantBuilder builder;
  Scoped<char> variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string.get()));

  start_handle_ = PrepareSignalHandle(connection_, variant_string.get());
  start_request_signal_id_ = SetupRequestResponseSignal(
      start_handle_.c_str(), OnStartRequestResponseSignal);

  // "Identifier for the application window", this is Wayland, so not "x11:...".
  const char parent_window[] = "";

  RTC_LOG(LS_INFO) << "Starting the screen cast session.";
  g_dbus_proxy_call(proxy_, "Start",
                    g_variant_new("(osa{sv})", session_handle_.c_str(),
                                  parent_window, &builder),
                    G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
                    reinterpret_cast<GAsyncReadyCallback>(OnStartRequested),
                    this);
}

// static
void ScreenCastPortal::OnStartRequested(GDBusProxy* proxy,
                                        GAsyncResult* result,
                                        gpointer user_data) {
  ScreenCastPortal* that = static_cast<ScreenCastPortal*>(user_data);
  RTC_DCHECK(that);

  Scoped<GError> error;
  Scoped<GVariant> variant(
      g_dbus_proxy_call_finish(proxy, result, error.receive()));
  if (!variant) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to start the screen cast session: "
                      << error->message;
    that->PortalFailed(RequestResponse::kError);
    return;
  }

  RTC_LOG(LS_INFO) << "Initializing the start of the screen cast session.";

  Scoped<char> handle;
  g_variant_get_child(variant.get(), 0, "o", handle.receive());
  if (!handle) {
    RTC_LOG(LS_ERROR)
        << "Failed to initialize the start of the screen cast session.";
    if (that->start_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(that->connection_,
                                           that->start_request_signal_id_);
      that->start_request_signal_id_ = 0;
    }
    that->PortalFailed(RequestResponse::kError);
    return;
  }

  RTC_LOG(LS_INFO) << "Subscribed to the start signal.";
}

// static
void ScreenCastPortal::OnStartRequestResponseSignal(GDBusConnection* connection,
                                                    const char* sender_name,
                                                    const char* object_path,
                                                    const char* interface_name,
                                                    const char* signal_name,
                                                    GVariant* parameters,
                                                    gpointer user_data) {
  ScreenCastPortal* that = static_cast<ScreenCastPortal*>(user_data);
  RTC_DCHECK(that);

  RTC_LOG(LS_INFO) << "Start signal received.";
  uint32_t portal_response;
  Scoped<GVariant> response_data;
  Scoped<GVariantIter> iter;
  g_variant_get(parameters, "(u@a{sv})", &portal_response,
                response_data.receive());
  if (portal_response || !response_data) {
    RTC_LOG(LS_ERROR) << "Failed to start the screen cast session.";
    that->PortalFailed(static_cast<RequestResponse>(portal_response));
    return;
  }

  // Array of PipeWire streams. See
  // https://github.com/flatpak/xdg-desktop-portal/blob/master/data/org.freedesktop.portal.ScreenCast.xml
  // documentation for <method name="Start">.
  if (g_variant_lookup(response_data.get(), "streams", "a(ua{sv})",
                       iter.receive())) {
    Scoped<GVariant> variant;

    while (g_variant_iter_next(iter.get(), "@(ua{sv})", variant.receive())) {
      uint32_t stream_id;
      uint32_t type;
      Scoped<GVariant> options;

      g_variant_get(variant.get(), "(u@a{sv})", &stream_id, options.receive());
      RTC_DCHECK(options.get());

      if (g_variant_lookup(options.get(), "source_type", "u", &type)) {
        that->capture_source_type_ =
            static_cast<ScreenCastPortal::CaptureSourceType>(type);
      }

      that->pw_stream_node_id_ = stream_id;

      break;
    }
  }

  that->OpenPipeWireRemote();
}

void ScreenCastPortal::OpenPipeWireRemote() {
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

  RTC_LOG(LS_INFO) << "Opening the PipeWire remote.";

  g_dbus_proxy_call_with_unix_fd_list(
      proxy_, "OpenPipeWireRemote",
      g_variant_new("(oa{sv})", session_handle_.c_str(), &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, /*fd_list=*/nullptr, cancellable_,
      reinterpret_cast<GAsyncReadyCallback>(OnOpenPipeWireRemoteRequested),
      this);
}

// static
void ScreenCastPortal::OnOpenPipeWireRemoteRequested(GDBusProxy* proxy,
                                                     GAsyncResult* result,
                                                     gpointer user_data) {
  ScreenCastPortal* that = static_cast<ScreenCastPortal*>(user_data);
  RTC_DCHECK(that);

  Scoped<GError> error;
  Scoped<GUnixFDList> outlist;
  Scoped<GVariant> variant(g_dbus_proxy_call_with_unix_fd_list_finish(
      proxy, outlist.receive(), result, error.receive()));
  if (!variant) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;
    RTC_LOG(LS_ERROR) << "Failed to open the PipeWire remote: "
                      << error->message;
    that->PortalFailed(RequestResponse::kError);
    return;
  }

  int32_t index;
  g_variant_get(variant.get(), "(h)", &index);

  that->pw_fd_ = g_unix_fd_list_get(outlist.get(), index, error.receive());

  if (that->pw_fd_ == -1) {
    RTC_LOG(LS_ERROR) << "Failed to get file descriptor from the list: "
                      << error->message;
    that->PortalFailed(RequestResponse::kError);
    return;
  }

  that->notifier_->OnScreenCastRequestResult(
      ScreenCastPortal::RequestResponse::kSuccess, that->pw_stream_node_id_,
      that->pw_fd_);
}

}  // namespace webrtc
