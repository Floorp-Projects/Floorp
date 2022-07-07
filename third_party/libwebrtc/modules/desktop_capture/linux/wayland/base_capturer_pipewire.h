/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_BASE_CAPTURER_PIPEWIRE_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_BASE_CAPTURER_PIPEWIRE_H_

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/linux/wayland/screencast_portal.h"
#include "modules/desktop_capture/linux/wayland/shared_screencast_stream.h"
#include "rtc_base/constructor_magic.h"

namespace webrtc {

class BaseCapturerPipeWire : public DesktopCapturer,
                             public ScreenCastPortal::PortalNotifier {
 public:
  BaseCapturerPipeWire(const DesktopCaptureOptions& options);
  ~BaseCapturerPipeWire() override;

  // DesktopCapturer interface.
  void Start(Callback* delegate) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

  // ScreenCastPortal::PortalNotifier interface.
  void OnScreenCastRequestResult(ScreenCastPortal::RequestResponse result,
                                 uint32_t stream_node_id,
                                 int fd) override;
  void OnScreenCastSessionClosed() override;

 private:
  DesktopCaptureOptions options_ = {};
  Callback* callback_ = nullptr;
  bool capturer_failed_ = false;
  std::unique_ptr<ScreenCastPortal> screencast_portal_;

  RTC_DISALLOW_COPY_AND_ASSIGN(BaseCapturerPipeWire);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_BASE_CAPTURER_PIPEWIRE_H_
