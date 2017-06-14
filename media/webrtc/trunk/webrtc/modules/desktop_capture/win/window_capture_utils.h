/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_WINDOW_CAPTURE_UTILS_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_WINDOW_CAPTURE_UTILS_H_

#include <windows.h>
#include <dwmapi.h>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/desktop_capture/desktop_geometry.h"

namespace webrtc {

// Output the window rect, with the left/right/bottom frame border cropped if
// the window is maximized. |cropped_rect| is the cropped rect relative to the
// desktop. |original_rect| is the original rect returned from GetWindowRect.
// Returns true if all API calls succeeded.
bool GetCroppedWindowRect(HWND window,
                          DesktopRect* cropped_rect,
                          DesktopRect* original_rect);

typedef HRESULT (WINAPI *DwmIsCompositionEnabledFunc)(BOOL*);

class AeroChecker {
 public:
  AeroChecker();
  ~AeroChecker();

  bool IsAeroEnabled();

 private:
  HMODULE dwmapi_library_;
  DwmIsCompositionEnabledFunc func_;

  RTC_DISALLOW_COPY_AND_ASSIGN(AeroChecker);
};

}  // namespace webrtc

#endif
