/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_COMMON_VIDEO_TEST_FRAME_GENERATOR_H_
#define WEBRTC_COMMON_VIDEO_TEST_FRAME_GENERATOR_H_

#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace test {

class FrameGenerator {
 public:
  FrameGenerator() {}
  virtual ~FrameGenerator() {}

  // Returns video frame that remains valid until next call.
  virtual I420VideoFrame* NextFrame() = 0;

  static FrameGenerator* Create(size_t width, size_t height);
  static FrameGenerator* CreateFromYuvFile(const char* file,
                                           size_t width,
                                           size_t height);
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_COMMON_VIDEO_TEST_FRAME_GENERATOR_H_
