/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_MAGNIFIER_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_MAGNIFIER_H_

#include <windows.h>
#include <magnification.h>
#include <wincodec.h>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/desktop_capture/screen_capture_frame_queue.h"
#include "webrtc/modules/desktop_capture/screen_capturer.h"
#include "webrtc/modules/desktop_capture/screen_capturer_helper.h"
#include "webrtc/modules/desktop_capture/win/scoped_thread_desktop.h"
#include "webrtc/system_wrappers/include/atomic32.h"

namespace webrtc {

class DesktopFrame;
class DesktopRect;
class Differ;

// Captures the screen using the Magnification API to support window exclusion.
// Each capturer must run on a dedicated thread because it uses thread local
// storage for redirecting the library callback. Also the thread must have a UI
// message loop to handle the window messages for the magnifier window.
class ScreenCapturerWinMagnifier : public ScreenCapturer {
 public:
  // |fallback_capturer| will be used to capture the screen if a non-primary
  // screen is being captured, or the OS does not support Magnification API, or
  // the magnifier capturer fails (e.g. in Windows8 Metro mode).
  explicit ScreenCapturerWinMagnifier(
      rtc::scoped_ptr<ScreenCapturer> fallback_capturer);
  virtual ~ScreenCapturerWinMagnifier();

  // Overridden from ScreenCapturer:
  void Start(Callback* callback) override;
  void Stop() override;
  void Capture(const DesktopRegion& region) override;
  bool GetScreenList(ScreenList* screens) override;
  bool SelectScreen(ScreenId id) override;
  void SetExcludedWindow(WindowId window) override;

 private:
  typedef BOOL(WINAPI* MagImageScalingCallback)(HWND hwnd,
                                                void* srcdata,
                                                MAGIMAGEHEADER srcheader,
                                                void* destdata,
                                                MAGIMAGEHEADER destheader,
                                                RECT unclipped,
                                                RECT clipped,
                                                HRGN dirty);
  typedef BOOL(WINAPI* MagInitializeFunc)(void);
  typedef BOOL(WINAPI* MagUninitializeFunc)(void);
  typedef BOOL(WINAPI* MagSetWindowSourceFunc)(HWND hwnd, RECT rect);
  typedef BOOL(WINAPI* MagSetWindowFilterListFunc)(HWND hwnd,
                                                   DWORD dwFilterMode,
                                                   int count,
                                                   HWND* pHWND);
  typedef BOOL(WINAPI* MagSetImageScalingCallbackFunc)(
      HWND hwnd,
      MagImageScalingCallback callback);

  static BOOL WINAPI OnMagImageScalingCallback(HWND hwnd,
                                               void* srcdata,
                                               MAGIMAGEHEADER srcheader,
                                               void* destdata,
                                               MAGIMAGEHEADER destheader,
                                               RECT unclipped,
                                               RECT clipped,
                                               HRGN dirty);

  // Captures the screen within |rect| in the desktop coordinates. Returns true
  // if succeeded.
  // It can only capture the primary screen for now. The magnification library
  // crashes under some screen configurations (e.g. secondary screen on top of
  // primary screen) if it tries to capture a non-primary screen. The caller
  // must make sure not calling it on non-primary screens.
  bool CaptureImage(const DesktopRect& rect);

  // Helper method for setting up the magnifier control. Returns true if
  // succeeded.
  bool InitializeMagnifier();

  // Called by OnMagImageScalingCallback to output captured data.
  void OnCaptured(void* data, const MAGIMAGEHEADER& header);

  // Makes sure the current frame exists and matches |size|.
  void CreateCurrentFrameIfNecessary(const DesktopSize& size);

  // Start the fallback capturer and select the screen.
  void StartFallbackCapturer();

  static Atomic32 tls_index_;

  rtc::scoped_ptr<ScreenCapturer> fallback_capturer_;
  bool fallback_capturer_started_;
  Callback* callback_;
  ScreenId current_screen_id_;
  std::wstring current_device_key_;
  HWND excluded_window_;

  // A thread-safe list of invalid rectangles, and the size of the most
  // recently captured screen.
  ScreenCapturerHelper helper_;

  // Queue of the frames buffers.
  ScreenCaptureFrameQueue queue_;

  // Class to calculate the difference between two screen bitmaps.
  rtc::scoped_ptr<Differ> differ_;

  // Used to suppress duplicate logging of SetThreadExecutionState errors.
  bool set_thread_execution_state_failed_;

  ScopedThreadDesktop desktop_;

  // Used for getting the screen dpi.
  HDC desktop_dc_;

  HMODULE mag_lib_handle_;
  MagInitializeFunc mag_initialize_func_;
  MagUninitializeFunc mag_uninitialize_func_;
  MagSetWindowSourceFunc set_window_source_func_;
  MagSetWindowFilterListFunc set_window_filter_list_func_;
  MagSetImageScalingCallbackFunc set_image_scaling_callback_func_;

  // The hidden window hosting the magnifier control.
  HWND host_window_;
  // The magnifier control that captures the screen.
  HWND magnifier_window_;

  // True if the magnifier control has been successfully initialized.
  bool magnifier_initialized_;

  // True if the last OnMagImageScalingCallback was called and handled
  // successfully. Reset at the beginning of each CaptureImage call.
  bool magnifier_capture_succeeded_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ScreenCapturerWinMagnifier);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_MAGNIFIER_H_
