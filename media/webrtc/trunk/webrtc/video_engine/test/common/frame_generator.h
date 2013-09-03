/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_ENGINE_TEST_COMMON_FRAME_GENERATOR_H_
#define WEBRTC_VIDEO_ENGINE_TEST_COMMON_FRAME_GENERATOR_H_

#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_engine/new_include/video_send_stream.h"

namespace webrtc {

class Clock;

namespace test {

// A set of classes that generate sequences of I420VideoFrames for testing
// without using the webcam.
class FrameGenerator {
 public:
  static FrameGenerator* Create(size_t width, size_t height, Clock* clock);
  virtual ~FrameGenerator() {}

  void InsertFrame(newapi::VideoSendStreamInput* input);

 protected:
  FrameGenerator(size_t width, size_t height, Clock* clock);
  virtual void GenerateNextFrame() = 0;

  size_t width_, height_;
  I420VideoFrame frame_;
  Clock* clock_;
};

class BlackFrameGenerator : public FrameGenerator {
 public:
  BlackFrameGenerator(size_t width, size_t height, Clock* clock);

 private:
  virtual void GenerateNextFrame() OVERRIDE;
};

class WhiteFrameGenerator : public FrameGenerator {
 public:
  WhiteFrameGenerator(size_t width, size_t height, Clock* clock);

 private:
  virtual void GenerateNextFrame() OVERRIDE;
};

class ChromaFrameGenerator : public FrameGenerator {
 public:
  ChromaFrameGenerator(size_t width, size_t height, Clock* clock);

 private:
  virtual void GenerateNextFrame() OVERRIDE;
};
}  // test
}  // webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_COMMON_FRAME_GENERATOR_H_
