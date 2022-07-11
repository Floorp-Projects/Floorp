/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_PORTAL_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_PORTAL_H_

#include <gio/gio.h>

#include <string>

#include "absl/types/optional.h"

namespace webrtc {

class ScreenCastPortal {
 public:
  // Values are set based on source type property in
  // xdg-desktop-portal/screencast
  // https://github.com/flatpak/xdg-desktop-portal/blob/master/data/org.freedesktop.portal.ScreenCast.xml
  enum class CaptureSourceType : uint32_t {
    kScreen = 0b01,
    kWindow = 0b10,
    kAnyScreenContent = kScreen | kWindow
  };

  // Values are set based on cursor mode property in
  // xdg-desktop-portal/screencast
  // https://github.com/flatpak/xdg-desktop-portal/blob/master/data/org.freedesktop.portal.ScreenCast.xml
  enum class CursorMode : uint32_t {
    // Mouse cursor will not be included in any form
    kHidden = 0b01,
    // Mouse cursor will be part of the screen content
    kEmbedded = 0b10,
    // Mouse cursor information will be send separately in form of metadata
    kMetadata = 0b100
  };

  // Interface that must be implemented by the ScreenCastPortal consumers.
  enum class RequestResponse {
    // Success, the request is carried out.
    kSuccess,
    // The user cancelled the interaction.
    kUserCancelled,
    // The user interaction was ended in some other way.
    kError,

    kMaxValue = kError
  };

  class PortalNotifier {
   public:
    virtual void OnScreenCastRequestResult(RequestResponse result,
                                           uint32_t stream_node_id,
                                           int fd) = 0;
    virtual void OnScreenCastSessionClosed() = 0;

   protected:
    PortalNotifier() = default;
    virtual ~PortalNotifier() = default;
  };

  explicit ScreenCastPortal(CaptureSourceType source_type,
                            PortalNotifier* notifier);
  ~ScreenCastPortal();

  // Initialize ScreenCastPortal with series of DBus calls where we try to
  // obtain all the required information, like PipeWire file descriptor and
  // PipeWire stream node ID.
  //
  // The observer will return whether the communication with xdg-desktop-portal
  // was successful and only then you will be able to get all the required
  // information in order to continue working with PipeWire.
  void Start();

 private:
  PortalNotifier* notifier_;

  // A PipeWire stream ID of stream we will be connecting to
  uint32_t pw_stream_node_id_ = 0;
  // A file descriptor of PipeWire socket
  int pw_fd_ = -1;

  CaptureSourceType capture_source_type_ =
      ScreenCastPortal::CaptureSourceType::kScreen;

  CursorMode cursor_mode_ = ScreenCastPortal::CursorMode::kMetadata;

  GDBusConnection* connection_ = nullptr;
  GDBusProxy* proxy_ = nullptr;
  GCancellable* cancellable_ = nullptr;
  std::string portal_handle_;
  std::string session_handle_;
  std::string sources_handle_;
  std::string start_handle_;
  guint session_request_signal_id_ = 0;
  guint sources_request_signal_id_ = 0;
  guint start_request_signal_id_ = 0;
  guint session_closed_signal_id_ = 0;

  void PortalFailed(RequestResponse result);

  uint32_t SetupRequestResponseSignal(const char* object_path,
                                      GDBusSignalCallback callback);

  static void OnProxyRequested(GObject* object,
                               GAsyncResult* result,
                               gpointer user_data);

  static std::string PrepareSignalHandle(GDBusConnection* connection,
                                         const char* token);

  void SessionRequest();
  static void OnSessionRequested(GDBusProxy* proxy,
                                 GAsyncResult* result,
                                 gpointer user_data);
  static void OnSessionRequestResponseSignal(GDBusConnection* connection,
                                             const char* sender_name,
                                             const char* object_path,
                                             const char* interface_name,
                                             const char* signal_name,
                                             GVariant* parameters,
                                             gpointer user_data);
  static void OnSessionClosedSignal(GDBusConnection* connection,
                                    const char* sender_name,
                                    const char* object_path,
                                    const char* interface_name,
                                    const char* signal_name,
                                    GVariant* parameters,
                                    gpointer user_data);
  void SourcesRequest();
  static void OnSourcesRequested(GDBusProxy* proxy,
                                 GAsyncResult* result,
                                 gpointer user_data);
  static void OnSourcesRequestResponseSignal(GDBusConnection* connection,
                                             const char* sender_name,
                                             const char* object_path,
                                             const char* interface_name,
                                             const char* signal_name,
                                             GVariant* parameters,
                                             gpointer user_data);

  void StartRequest();
  static void OnStartRequested(GDBusProxy* proxy,
                               GAsyncResult* result,
                               gpointer user_data);
  static void OnStartRequestResponseSignal(GDBusConnection* connection,
                                           const char* sender_name,
                                           const char* object_path,
                                           const char* interface_name,
                                           const char* signal_name,
                                           GVariant* parameters,
                                           gpointer user_data);

  void OpenPipeWireRemote();
  static void OnOpenPipeWireRemoteRequested(GDBusProxy* proxy,
                                            GAsyncResult* result,
                                            gpointer user_data);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_PORTAL_H_
