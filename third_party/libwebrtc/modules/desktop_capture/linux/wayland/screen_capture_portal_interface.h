/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREEN_CAPTURE_PORTAL_INTERFACE_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREEN_CAPTURE_PORTAL_INTERFACE_H_

#include "modules/desktop_capture/linux/wayland/xdg_session_details.h"

namespace webrtc {
namespace xdg_portal {

// An interface for XDG desktop portals that can capture desktop/screen.
class ScreenCapturePortalInterface {
 public:
  virtual ~ScreenCapturePortalInterface() {}
  // Gets details about the session such as session handle.
  virtual xdg_portal::SessionDetails GetSessionDetails() = 0;
  // Starts the portal setup.
  virtual void Start() = 0;
};

}  // namespace xdg_portal
}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREEN_CAPTURE_PORTAL_INTERFACE_H_
