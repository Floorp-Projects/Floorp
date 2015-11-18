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

#include <string>
#include <vector>

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

  // Creates a test frame generator that creates fully saturated frames with
  // varying U, V values over time.
  static FrameGenerator* CreateChromaGenerator(size_t width, size_t height);

  // Creates a frame generator that repeatedly plays a set of yuv files.
  // The frame_repeat_count determines how many times each frame is shown,
  // with 1 = show each frame once, etc.
  static FrameGenerator* CreateFromYuvFile(std::vector<std::string> files,
                                           size_t width,
                                           size_t height,
                                           int frame_repeat_count);
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_COMMON_VIDEO_TEST_FRAME_GENERATOR_H_
