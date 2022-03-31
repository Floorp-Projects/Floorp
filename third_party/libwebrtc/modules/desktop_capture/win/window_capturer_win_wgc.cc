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

#include <utility>

#include "rtc_base/logging.h"

namespace webrtc {

WindowCapturerWinWgc::WindowCapturerWinWgc
    (bool enumerate_current_process_windows)
    : enumerate_current_process_windows_(enumerate_current_process_windows) {}
WindowCapturerWinWgc::~WindowCapturerWinWgc() = default;

bool WindowCapturerWinWgc::GetSourceList(SourceList* sources) {
  return window_capture_helper_.EnumerateCapturableWindows(
      sources, enumerate_current_process_windows_);
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

  // Create a Direct3D11 device to share amongst the WgcCaptureSessions. Many
  // parameters are nullptr as the implemention uses defaults that work well for
  // us.
  HRESULT hr = D3D11CreateDevice(
      /*adapter=*/nullptr, D3D_DRIVER_TYPE_HARDWARE,
      /*software_rasterizer=*/nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
      /*feature_levels=*/nullptr, /*feature_levels_size=*/0, D3D11_SDK_VERSION,
      &d3d11_device_, /*feature_level=*/nullptr, /*device_context=*/nullptr);
  if (hr == DXGI_ERROR_UNSUPPORTED) {
    // If a hardware device could not be created, use WARP which is a high speed
    // software device.
    hr = D3D11CreateDevice(
        /*adapter=*/nullptr, D3D_DRIVER_TYPE_WARP,
        /*software_rasterizer=*/nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        /*feature_levels=*/nullptr, /*feature_levels_size=*/0,
        D3D11_SDK_VERSION, &d3d11_device_, /*feature_level=*/nullptr,
        /*device_context=*/nullptr);
  }

  if (FAILED(hr)) {
    RTC_LOG(LS_ERROR) << "Failed to create D3D11Device: " << hr;
  }
}

void WindowCapturerWinWgc::CaptureFrame() {
  RTC_DCHECK(callback_);

  if (!window_) {
    RTC_LOG(LS_ERROR) << "Window hasn't been selected";
    callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                               /*frame=*/nullptr);
    return;
  }

  if (!d3d11_device_) {
    RTC_LOG(LS_ERROR) << "No D3D11D3evice, cannot capture.";
    callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                               /*frame=*/nullptr);
    return;
  }

  WgcCaptureSession* capture_session = nullptr;
  auto iter = ongoing_captures_.find(window_);
  if (iter == ongoing_captures_.end()) {
    auto iter_success_pair = ongoing_captures_.emplace(
        std::piecewise_construct, std::forward_as_tuple(window_),
        std::forward_as_tuple(d3d11_device_, window_));
    if (iter_success_pair.second) {
      capture_session = &iter_success_pair.first->second;
    } else {
      RTC_LOG(LS_ERROR) << "Failed to create new WgcCaptureSession.";
      callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                                 /*frame=*/nullptr);
      return;
    }
  } else {
    capture_session = &iter->second;
  }

  HRESULT hr;
  if (!capture_session->IsCaptureStarted()) {
    hr = capture_session->StartCapture();
    if (FAILED(hr)) {
      RTC_LOG(LS_ERROR) << "Failed to start capture: " << hr;
      callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                                 /*frame=*/nullptr);
      return;
    }
  }

  std::unique_ptr<DesktopFrame> frame;
  hr = capture_session->GetMostRecentFrame(&frame);
  if (FAILED(hr)) {
    RTC_LOG(LS_ERROR) << "GetMostRecentFrame failed: " << hr;
    callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                               /*frame=*/nullptr);
    return;
  }

  if (!frame) {
    RTC_LOG(LS_WARNING) << "GetMostRecentFrame returned an empty frame.";
    callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_TEMPORARY,
                               /*frame=*/nullptr);
    return;
  }

  callback_->OnCaptureResult(DesktopCapturer::Result::SUCCESS,
                             std::move(frame));
}

// static
std::unique_ptr<DesktopCapturer> WindowCapturerWinWgc::CreateRawWindowCapturer(
    const DesktopCaptureOptions& options) {
  return std::unique_ptr<DesktopCapturer>(
      new WindowCapturerWinWgc(options.enumerate_current_process_windows()));
}

}  // namespace webrtc
