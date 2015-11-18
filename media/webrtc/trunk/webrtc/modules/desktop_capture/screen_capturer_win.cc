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
#include "webrtc/modules/desktop_capture/win/screen_capturer_win_gdi.h"
#include "webrtc/modules/desktop_capture/win/screen_capturer_win_magnifier.h"

namespace webrtc {

// static
ScreenCapturer* ScreenCapturer::Create(const DesktopCaptureOptions& options) {
  rtc::scoped_ptr<ScreenCapturer> gdi_capturer(
      new ScreenCapturerWinGdi(options));

  if (options.allow_use_magnification_api())
    return new ScreenCapturerWinMagnifier(gdi_capturer.Pass());

  return gdi_capturer.release();
}

}  // namespace webrtc
