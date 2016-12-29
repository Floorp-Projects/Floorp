/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/video_capturer.h"

#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/frame_generator_capturer.h"
#include "webrtc/test/vcm_capturer.h"

namespace webrtc {
namespace test {

class NullCapturer : public VideoCapturer {
 public:
  NullCapturer() : VideoCapturer(NULL) {}
  virtual ~NullCapturer() {}

  virtual void Start() {}
  virtual void Stop() {}
};

VideoCapturer::VideoCapturer(VideoCaptureInput* input) : input_(input) {
}

VideoCapturer* VideoCapturer::Create(VideoCaptureInput* input,
                                     size_t width,
                                     size_t height,
                                     int fps,
                                     Clock* clock) {
  VcmCapturer* vcm_capturer = VcmCapturer::Create(input, width, height, fps);

  if (vcm_capturer != NULL) {
    return vcm_capturer;
  }
  // TODO(pbos): Log a warning that this failed.

  FrameGeneratorCapturer* frame_generator_capturer =
      FrameGeneratorCapturer::Create(input, width, height, fps, clock);
  if (frame_generator_capturer != NULL) {
    return frame_generator_capturer;
  }
  // TODO(pbos): Log a warning that this failed.

  return new NullCapturer();
}
}  // test
}  // webrtc
