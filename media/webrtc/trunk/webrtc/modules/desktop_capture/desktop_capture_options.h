/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURE_OPTIONS_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURE_OPTIONS_H_

#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/system_wrappers/interface/scoped_refptr.h"

#if defined(USE_X11)
#include "webrtc/modules/desktop_capture/x11/shared_x_display.h"
#endif

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include "webrtc/modules/desktop_capture/mac/desktop_configuration_monitor.h"
#endif

namespace webrtc {

// An object that stores initialization parameters for screen and window
// capturers.
class DesktopCaptureOptions {
 public:
  // Creates an empty Options instance (e.g. without X display).
  DesktopCaptureOptions();
  ~DesktopCaptureOptions();

  // Returns instance of DesktopCaptureOptions with default parameters. On Linux
  // also initializes X window connection. x_display() will be set to null if
  // X11 connection failed (e.g. DISPLAY isn't set).
  static DesktopCaptureOptions CreateDefault();

#if defined(USE_X11)
  SharedXDisplay* x_display() const { return x_display_; }
  void set_x_display(scoped_refptr<SharedXDisplay> x_display) {
    x_display_ = x_display;
  }
#endif

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  DesktopConfigurationMonitor* configuration_monitor() const {
    return configuration_monitor_;
  }
  void set_configuration_monitor(scoped_refptr<DesktopConfigurationMonitor> m) {
    configuration_monitor_ = m;
  }
#endif

  // Flag indicating that the capturer should use screen change notifications.
  // Enables/disables use of XDAMAGE in the X11 capturer.
  bool use_update_notifications() const { return use_update_notifications_; }
  void set_use_update_notifications(bool use_update_notifications) {
    use_update_notifications_ = use_update_notifications;
  }

  // Flag indicating if desktop effects (e.g. Aero) should be disabled when the
  // capturer is active. Currently used only on Windows.
  bool disable_effects() const { return disable_effects_; }
  void set_disable_effects(bool disable_effects) {
    disable_effects_ = disable_effects;
  }

 private:
#if defined(USE_X11)
  scoped_refptr<SharedXDisplay> x_display_;
#endif

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  scoped_refptr<DesktopConfigurationMonitor> configuration_monitor_;
#endif
  bool use_update_notifications_;
  bool disable_effects_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURE_OPTIONS_H_
