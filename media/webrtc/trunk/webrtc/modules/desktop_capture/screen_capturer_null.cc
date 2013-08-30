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

namespace webrtc {

// static
ScreenCapturer* ScreenCapturer::Create() {
  return NULL;
}

#if defined(OS_LINUX)
// static
ScreenCapturer* ScreenCapturer::CreateWithXDamage(bool use_x_damage) {
  return NULL;
}
#elif defined(OS_WIN)
// static
ScreenCapturer* ScreenCapturer::CreateWithDisableAero(bool disable_aero) {
  return NULL;
}
#endif  // defined(OS_WIN)

}  // namespace webrtc
