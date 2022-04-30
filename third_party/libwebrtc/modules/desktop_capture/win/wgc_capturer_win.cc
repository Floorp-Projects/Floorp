/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/wgc_capturer_win.h"

#include <utility>

#include "modules/desktop_capture/win/wgc_desktop_frame.h"
#include "rtc_base/logging.h"

namespace WGC = ABI::Windows::Graphics::Capture;
using Microsoft::WRL::ComPtr;

namespace webrtc {

WgcCapturerWin::WgcCapturerWin(
    std::unique_ptr<WgcCaptureSourceFactory> source_factory,
    std::unique_ptr<SourceEnumerator> source_enumerator)
    : source_factory_(std::move(source_factory)),
      source_enumerator_(std::move(source_enumerator)) {}
WgcCapturerWin::~WgcCapturerWin() = default;

// static
std::unique_ptr<DesktopCapturer> WgcCapturerWin::CreateRawWindowCapturer(
    const DesktopCaptureOptions& options) {
  return std::make_unique<WgcCapturerWin>(
      std::make_unique<WgcWindowSourceFactory>(),
      std::make_unique<WindowEnumerator>(
          options.enumerate_current_process_windows()));
}

// static
std::unique_ptr<DesktopCapturer> WgcCapturerWin::CreateRawScreenCapturer(
    const DesktopCaptureOptions& options) {
  return std::make_unique<WgcCapturerWin>(
      std::make_unique<WgcScreenSourceFactory>(),
      std::make_unique<ScreenEnumerator>());
}

bool WgcCapturerWin::GetSourceList(SourceList* sources) {
  return source_enumerator_->FindAllSources(sources);
}

bool WgcCapturerWin::SelectSource(DesktopCapturer::SourceId id) {
  capture_source_ = source_factory_->CreateCaptureSource(id);
  return capture_source_->IsCapturable();
}

void WgcCapturerWin::Start(Callback* callback) {
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

void WgcCapturerWin::CaptureFrame() {
  RTC_DCHECK(callback_);

  if (!capture_source_) {
    RTC_LOG(LS_ERROR) << "Source hasn't been selected";
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

  HRESULT hr;
  WgcCaptureSession* capture_session = nullptr;
  std::map<SourceId, WgcCaptureSession>::iterator session_iter =
      ongoing_captures_.find(capture_source_->GetSourceId());
  if (session_iter == ongoing_captures_.end()) {
    ComPtr<WGC::IGraphicsCaptureItem> item;
    hr = capture_source_->GetCaptureItem(&item);
    if (FAILED(hr)) {
      RTC_LOG(LS_ERROR) << "Failed to create a GraphicsCaptureItem: " << hr;
      callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                                 /*frame=*/nullptr);
      return;
    }

    std::pair<std::map<SourceId, WgcCaptureSession>::iterator, bool>
        iter_success_pair = ongoing_captures_.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(capture_source_->GetSourceId()),
            std::forward_as_tuple(d3d11_device_, item));
    RTC_DCHECK(iter_success_pair.second);
    capture_session = &iter_success_pair.first->second;
  } else {
    capture_session = &session_iter->second;
  }

  if (!capture_session->IsCaptureStarted()) {
    hr = capture_session->StartCapture();
    if (FAILED(hr)) {
      RTC_LOG(LS_ERROR) << "Failed to start capture: " << hr;
      ongoing_captures_.erase(capture_source_->GetSourceId());
      callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                                 /*frame=*/nullptr);
      return;
    }
  }

  std::unique_ptr<DesktopFrame> frame;
  hr = capture_session->GetFrame(&frame);
  if (FAILED(hr)) {
    RTC_LOG(LS_ERROR) << "GetFrame failed: " << hr;
    ongoing_captures_.erase(capture_source_->GetSourceId());
    callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_PERMANENT,
                               /*frame=*/nullptr);
    return;
  }

  if (!frame) {
    callback_->OnCaptureResult(DesktopCapturer::Result::ERROR_TEMPORARY,
                               /*frame=*/nullptr);
    return;
  }

  callback_->OnCaptureResult(DesktopCapturer::Result::SUCCESS,
                             std::move(frame));
}

bool WgcCapturerWin::IsSourceBeingCaptured(DesktopCapturer::SourceId id) {
  std::map<DesktopCapturer::SourceId, WgcCaptureSession>::iterator
      session_iter = ongoing_captures_.find(id);
  if (session_iter == ongoing_captures_.end())
    return false;

  return session_iter->second.IsCaptureStarted();
}

}  // namespace webrtc
