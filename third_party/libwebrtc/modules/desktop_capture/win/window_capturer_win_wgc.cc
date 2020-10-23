/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/window_capturer_win_wgc.h"

#include <memory>

#include "modules/desktop_capture/desktop_capturer.h"

namespace webrtc {

WindowCapturerWinWgc::WindowCapturerWinWgc() = default;
WindowCapturerWinWgc::~WindowCapturerWinWgc() = default;

bool WindowCapturerWinWgc::GetSourceList(SourceList* sources) {
  return window_capture_helper_.EnumerateCapturableWindows(sources);
}

bool WindowCapturerWinWgc::SelectSource(SourceId id) {
  HWND window = reinterpret_cast<HWND>(id);
  if (!IsWindowValidAndVisible(window))
    return false;

  window_ = window;
  return true;
}

void WindowCapturerWinWgc::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  callback_ = callback;
}

void WindowCapturerWinWgc::CaptureFrame() {
  RTC_DCHECK(callback_);

  callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
}

// static
std::unique_ptr<DesktopCapturer> WindowCapturerWinWgc::CreateRawWindowCapturer(
    const DesktopCaptureOptions& options) {
  return std::unique_ptr<DesktopCapturer>(new WindowCapturerWinWgc());
}

}  // namespace webrtc
