/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_WINDOWS_VIDEO_CAPTURE_MF_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_WINDOWS_VIDEO_CAPTURE_MF_H_

#include "modules/video_capture/video_capture_impl.h"

namespace webrtc {
namespace videocapturemodule {

// VideoCapture implementation that uses the Media Foundation API on Windows.
// This will replace the DirectShow based implementation on Vista and higher.
// TODO(tommi): Finish implementing and switch out the DS in the factory method
// for supported platforms.
class VideoCaptureMF : public VideoCaptureImpl {
 public:
  explicit VideoCaptureMF(const WebRtc_Word32 id);

  WebRtc_Word32 Init(const WebRtc_Word32 id, const char* device_id);

  // Overrides from VideoCaptureImpl.
  virtual WebRtc_Word32 StartCapture(const VideoCaptureCapability& capability);
  virtual WebRtc_Word32 StopCapture();
  virtual bool CaptureStarted();
  virtual WebRtc_Word32 CaptureSettings(
      VideoCaptureCapability& settings);  // NOLINT

 protected:
  virtual ~VideoCaptureMF();
};

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_WINDOWS_VIDEO_CAPTURE_MF_H_
