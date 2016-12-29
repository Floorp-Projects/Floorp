/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/frame_generator_capturer.h"

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/test/frame_generator.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {
namespace test {

FrameGeneratorCapturer* FrameGeneratorCapturer::Create(VideoCaptureInput* input,
                                                       size_t width,
                                                       size_t height,
                                                       int target_fps,
                                                       Clock* clock) {
  FrameGeneratorCapturer* capturer = new FrameGeneratorCapturer(
      clock, input, FrameGenerator::CreateChromaGenerator(width, height),
      target_fps);
  if (!capturer->Init()) {
    delete capturer;
    return NULL;
  }

  return capturer;
}

FrameGeneratorCapturer* FrameGeneratorCapturer::CreateFromYuvFile(
    VideoCaptureInput* input,
    const std::string& file_name,
    size_t width,
    size_t height,
    int target_fps,
    Clock* clock) {
  FrameGeneratorCapturer* capturer = new FrameGeneratorCapturer(
      clock, input,
      FrameGenerator::CreateFromYuvFile(std::vector<std::string>(1, file_name),
                                        width, height, 1),
      target_fps);
  if (!capturer->Init()) {
    delete capturer;
    return NULL;
  }

  return capturer;
}

FrameGeneratorCapturer::FrameGeneratorCapturer(Clock* clock,
                                               VideoCaptureInput* input,
                                               FrameGenerator* frame_generator,
                                               int target_fps)
    : VideoCapturer(input),
      clock_(clock),
      sending_(false),
      tick_(EventTimerWrapper::Create()),
      thread_(FrameGeneratorCapturer::Run, this, "FrameGeneratorCapturer"),
      frame_generator_(frame_generator),
      target_fps_(target_fps),
      first_frame_capture_time_(-1) {
  assert(input != NULL);
  assert(frame_generator != NULL);
  assert(target_fps > 0);
}

FrameGeneratorCapturer::~FrameGeneratorCapturer() {
  Stop();

  thread_.Stop();
}

bool FrameGeneratorCapturer::Init() {
  // This check is added because frame_generator_ might be file based and should
  // not crash because a file moved.
  if (frame_generator_.get() == NULL)
    return false;

  if (!tick_->StartTimer(true, 1000 / target_fps_))
    return false;
  thread_.Start();
  thread_.SetPriority(rtc::kHighPriority);
  return true;
}

bool FrameGeneratorCapturer::Run(void* obj) {
  static_cast<FrameGeneratorCapturer*>(obj)->InsertFrame();
  return true;
}

void FrameGeneratorCapturer::InsertFrame() {
  {
    rtc::CritScope cs(&lock_);
    if (sending_) {
      VideoFrame* frame = frame_generator_->NextFrame();
      frame->set_ntp_time_ms(clock_->CurrentNtpInMilliseconds());
      if (first_frame_capture_time_ == -1) {
        first_frame_capture_time_ = frame->ntp_time_ms();
      }
      input_->IncomingCapturedFrame(*frame);
    }
  }
  tick_->Wait(WEBRTC_EVENT_INFINITE);
}

void FrameGeneratorCapturer::Start() {
  rtc::CritScope cs(&lock_);
  sending_ = true;
}

void FrameGeneratorCapturer::Stop() {
  rtc::CritScope cs(&lock_);
  sending_ = false;
}

void FrameGeneratorCapturer::ForceFrame() {
  tick_->Set();
}
}  // test
}  // webrtc
