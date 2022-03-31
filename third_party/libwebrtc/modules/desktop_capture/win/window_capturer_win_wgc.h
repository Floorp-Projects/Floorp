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

#include <d3d11.h>
#include <wrl/client.h>
#include <map>
#include <memory>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/win/wgc_capture_session.h"
#include "modules/desktop_capture/win/window_capture_utils.h"

namespace webrtc {

class WindowCapturerWinWgc final : public DesktopCapturer {
 public:
  WindowCapturerWinWgc(bool enumerate_current_process_windows);

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
  // The callback that we deliver frames to, synchronously, before CaptureFrame
  // returns.
  Callback* callback_ = nullptr;

  // HWND for the currently selected window or nullptr if a window is not
  // selected. We may be capturing many other windows, but this is the window
  // that we will return a frame for when CaptureFrame is called.
  HWND window_ = nullptr;

  // This helps us enumerate the list of windows that we can capture.
  WindowCaptureHelperWin window_capture_helper_;

  // A Direct3D11 device that is shared amongst the WgcCaptureSessions, who
  // require one to perform the capture.
  Microsoft::WRL::ComPtr<::ID3D11Device> d3d11_device_;

  // A map of all the windows we are capturing and the associated
  // WgcCaptureSession. This is where we will get the frames for the window
  // from, when requested.
  std::map<HWND, WgcCaptureSession> ongoing_captures_;
  bool enumerate_current_process_windows_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_WIN_WINDOW_CAPTURER_WIN_WGC_H_
