/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/wgc_capture_session.h"

#include <utility>

#include "rtc_base/checks.h"

using Microsoft::WRL::ComPtr;
namespace webrtc {

WgcCaptureSession::WgcCaptureSession(ComPtr<ID3D11Device> d3d11_device,
                                     HWND window)
    : d3d11_device_(std::move(d3d11_device)), window_(window) {}
WgcCaptureSession::~WgcCaptureSession() = default;

HRESULT WgcCaptureSession::StartCapture() {
  RTC_DCHECK(!is_capture_started_);
  RTC_DCHECK(d3d11_device_);
  RTC_DCHECK(window_);

  return E_NOTIMPL;
}

HRESULT WgcCaptureSession::GetMostRecentFrame(
    std::unique_ptr<DesktopFrame>* output_frame) {
  RTC_DCHECK(is_capture_started_);

  return E_NOTIMPL;
}

}  // namespace webrtc
