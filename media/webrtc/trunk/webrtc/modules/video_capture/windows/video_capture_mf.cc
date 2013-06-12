/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_capture/windows/video_capture_mf.h"

namespace webrtc {
namespace videocapturemodule {

VideoCaptureMF::VideoCaptureMF(const WebRtc_Word32 id) : VideoCaptureImpl(id) {}
VideoCaptureMF::~VideoCaptureMF() {}

WebRtc_Word32 VideoCaptureMF::Init(const WebRtc_Word32 id,
                                   const char* device_id) {
  return 0;
}

WebRtc_Word32 VideoCaptureMF::StartCapture(
    const VideoCaptureCapability& capability) {
  return -1;
}

WebRtc_Word32 VideoCaptureMF::StopCapture() {
  return -1;
}

bool VideoCaptureMF::CaptureStarted() {
  return false;
}

WebRtc_Word32 VideoCaptureMF::CaptureSettings(
    VideoCaptureCapability& settings) {
  return -1;
}

}  // namespace videocapturemodule
}  // namespace webrtc
