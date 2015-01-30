/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_IOS_VIDEO_CAPTURE_IOS_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_IOS_VIDEO_CAPTURE_IOS_H_

#include "webrtc/modules/video_capture/video_capture_impl.h"

@class RTCVideoCaptureIosObjC;

namespace webrtc {
namespace videocapturemodule {
class VideoCaptureIos : public VideoCaptureImpl {
 public:
  explicit VideoCaptureIos(const int32_t capture_id);
  virtual ~VideoCaptureIos();

  static VideoCaptureModule* Create(const int32_t capture_id,
                                    const char* device_unique_id_utf8);

  // Implementation of VideoCaptureImpl.
  virtual int32_t StartCapture(
      const VideoCaptureCapability& capability) OVERRIDE;
  virtual int32_t StopCapture() OVERRIDE;
  virtual bool CaptureStarted() OVERRIDE;
  virtual int32_t CaptureSettings(VideoCaptureCapability& settings) OVERRIDE;

 private:
  RTCVideoCaptureIosObjC* capture_device_;
  bool is_capturing_;
  int32_t id_;
  VideoCaptureCapability capability_;
};

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_IOS_VIDEO_CAPTURE_IOS_H_
