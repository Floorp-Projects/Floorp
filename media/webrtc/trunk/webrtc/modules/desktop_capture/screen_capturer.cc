/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/screen_capturer.h"

#include "webrtc/modules/desktop_capture/desktop_capture_options.h"

namespace webrtc {

ScreenCapturer* ScreenCapturer::Create() {
  return Create(DesktopCaptureOptions::CreateDefault());
}

#if defined(WEBRTC_LINUX)
ScreenCapturer* ScreenCapturer::CreateWithXDamage(
    bool use_update_notifications) {
  DesktopCaptureOptions options;
  options.set_use_update_notifications(use_update_notifications);
  return Create(options);
}
#elif defined(WEBRTC_WIN)
ScreenCapturer* ScreenCapturer::CreateWithDisableAero(bool disable_effects) {
  DesktopCaptureOptions options;
  options.set_disable_effects(disable_effects);
  return Create(options);
}
#endif

}  // namespace webrtc
