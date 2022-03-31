/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURE_SESSION_H_
#define MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURE_SESSION_H_

#include <Windows.Graphics.Capture.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>

#include "modules/desktop_capture/desktop_frame.h"

namespace webrtc {

class WgcCaptureSession final {
 public:
  WgcCaptureSession(Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
                    HWND window);

  // Disallow copy and assign
  WgcCaptureSession(const WgcCaptureSession&) = delete;
  WgcCaptureSession& operator=(const WgcCaptureSession&) = delete;

  ~WgcCaptureSession();

  HRESULT StartCapture();
  HRESULT GetMostRecentFrame(std::unique_ptr<DesktopFrame>* output_frame);
  bool IsCaptureStarted() const { return is_capture_started_; }

 private:
  // A Direct3D11 Device provided by the caller. We use this to create an
  // IDirect3DDevice, and also to create textures that will hold the image data.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  HWND window_;
  bool is_capture_started_ = false;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURE_SESSION_H_
