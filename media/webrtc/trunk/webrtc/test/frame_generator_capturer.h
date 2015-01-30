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

#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/video_capturer.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class CriticalSectionWrapper;
class EventWrapper;
class ThreadWrapper;

namespace test {

class FrameGenerator;

class FrameGeneratorCapturer : public VideoCapturer {
 public:
  static FrameGeneratorCapturer* Create(VideoSendStreamInput* input,
                                        size_t width,
                                        size_t height,
                                        int target_fps,
                                        Clock* clock);

  static FrameGeneratorCapturer* CreateFromYuvFile(VideoSendStreamInput* input,
                                                   const char* file_name,
                                                   size_t width,
                                                   size_t height,
                                                   int target_fps,
                                                   Clock* clock);
  virtual ~FrameGeneratorCapturer();

  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;

  int64_t first_frame_capture_time() const { return first_frame_capture_time_; }

 private:
  FrameGeneratorCapturer(Clock* clock,
                         VideoSendStreamInput* input,
                         FrameGenerator* frame_generator,
                         int target_fps);
  bool Init();
  void InsertFrame();
  static bool Run(void* obj);

  Clock* const clock_;
  bool sending_;

  scoped_ptr<EventWrapper> tick_;
  scoped_ptr<CriticalSectionWrapper> lock_;
  scoped_ptr<ThreadWrapper> thread_;
  scoped_ptr<FrameGenerator> frame_generator_;

  int target_fps_;

  int64_t first_frame_capture_time_;
};
}  // test
}  // webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_COMMON_FRAME_GENERATOR_CAPTURER_H_
