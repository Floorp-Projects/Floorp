/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_GDI_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_GDI_H_

#include "webrtc/modules/desktop_capture/screen_capturer.h"

#include <windows.h>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/desktop_capture/screen_capture_frame_queue.h"
#include "webrtc/modules/desktop_capture/screen_capturer_helper.h"
#include "webrtc/modules/desktop_capture/win/scoped_thread_desktop.h"

namespace webrtc {

class Differ;

// ScreenCapturerWinGdi captures 32bit RGB using GDI.
//
// ScreenCapturerWinGdi is double-buffered as required by ScreenCapturer.
class ScreenCapturerWinGdi : public ScreenCapturer {
 public:
  explicit ScreenCapturerWinGdi(const DesktopCaptureOptions& options);
  virtual ~ScreenCapturerWinGdi();

  // Overridden from ScreenCapturer:
  void Start(Callback* callback) override;
  void Capture(const DesktopRegion& region) override;
  bool GetScreenList(ScreenList* screens) override;
  bool SelectScreen(ScreenId id) override;

 private:
  typedef HRESULT (WINAPI * DwmEnableCompositionFunc)(UINT);
  typedef HRESULT (WINAPI * DwmIsCompositionEnabledFunc)(BOOL*);

  // Make sure that the device contexts match the screen configuration.
  void PrepareCaptureResources();

  // Captures the current screen contents into the current buffer. Returns true
  // if succeeded.
  bool CaptureImage();

  // Capture the current cursor shape.
  void CaptureCursor();

  Callback* callback_;
  ScreenId current_screen_id_;
  std::wstring current_device_key_;

  // A thread-safe list of invalid rectangles, and the size of the most
  // recently captured screen.
  ScreenCapturerHelper helper_;

  ScopedThreadDesktop desktop_;

  // GDI resources used for screen capture.
  HDC desktop_dc_;
  HDC memory_dc_;

  // Queue of the frames buffers.
  ScreenCaptureFrameQueue queue_;

  // Rectangle describing the bounds of the desktop device context, relative to
  // the primary display's top-left.
  DesktopRect desktop_dc_rect_;

  // Class to calculate the difference between two screen bitmaps.
  rtc::scoped_ptr<Differ> differ_;

  HMODULE dwmapi_library_;
  DwmEnableCompositionFunc composition_func_;
  DwmIsCompositionEnabledFunc composition_enabled_func_;

  bool disable_composition_;

  // Used to suppress duplicate logging of SetThreadExecutionState errors.
  bool set_thread_execution_state_failed_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCapturerWinGdi);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_GDI_H_
