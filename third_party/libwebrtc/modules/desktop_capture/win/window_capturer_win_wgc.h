/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_WINDOW_CAPTURER_WIN_WGC_H_
#define MODULES_DESKTOP_CAPTURE_WIN_WINDOW_CAPTURER_WIN_WGC_H_

#include <memory>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/win/window_capture_utils.h"

namespace webrtc {

class WindowCapturerWinWgc : public DesktopCapturer {
 public:
  WindowCapturerWinWgc();

  // Disallow copy and assign
  WindowCapturerWinWgc(const WindowCapturerWinWgc&) = delete;
  WindowCapturerWinWgc& operator=(const WindowCapturerWinWgc&) = delete;

  ~WindowCapturerWinWgc() override;

  static std::unique_ptr<DesktopCapturer> CreateRawWindowCapturer(
      const DesktopCaptureOptions& options);

  // DesktopCapturer interface.
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

 private:
  Callback* callback_ = nullptr;

  // HWND for the currently selected window or nullptr if window is not
  // selected.
  HWND window_ = nullptr;
  WindowCaptureHelperWin window_capture_helper_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_WIN_WINDOW_CAPTURER_WIN_WGC_H_
