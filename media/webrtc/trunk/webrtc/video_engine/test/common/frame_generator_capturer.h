/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_ENGINE_TEST_COMMON_FRAME_GENERATOR_CAPTURER_H_
#define WEBRTC_VIDEO_ENGINE_TEST_COMMON_FRAME_GENERATOR_CAPTURER_H_

#include "webrtc/video_engine/test/common/video_capturer.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class Clock;
class CriticalSectionWrapper;
class ThreadWrapper;

namespace test {

class FrameGenerator;

class FrameGeneratorCapturer : public VideoCapturer {
 public:
  static FrameGeneratorCapturer* Create(newapi::VideoSendStreamInput* input,
                                        FrameGenerator* frame_generator,
                                        int target_fps,
                                        Clock* clock);
  virtual ~FrameGeneratorCapturer();

  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  FrameGeneratorCapturer(newapi::VideoSendStreamInput* input,
                         FrameGenerator* frame_generator,
                         int target_fps,
                         Clock* clock);
  bool Init();
  void InsertFrame();
  static bool Run(void* obj);

  bool sending_;

  Clock* clock_;
  CriticalSectionWrapper* lock_;
  ThreadWrapper* thread_;
  FrameGenerator* frame_generator_;

  int target_fps_;
};
}  // test
}  // webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_COMMON_FRAME_GENERATOR_CAPTURER_H_
