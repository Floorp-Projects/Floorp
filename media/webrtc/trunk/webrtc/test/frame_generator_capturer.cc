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

#include "webrtc/test/frame_generator.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {
namespace test {

FrameGeneratorCapturer* FrameGeneratorCapturer::Create(
    VideoSendStreamInput* input,
    size_t width,
    size_t height,
    int target_fps,
    Clock* clock) {
  FrameGeneratorCapturer* capturer = new FrameGeneratorCapturer(
      clock, input, FrameGenerator::Create(width, height), target_fps);
  if (!capturer->Init()) {
    delete capturer;
    return NULL;
  }

  return capturer;
}

FrameGeneratorCapturer* FrameGeneratorCapturer::CreateFromYuvFile(
    VideoSendStreamInput* input,
    const char* file_name,
    size_t width,
    size_t height,
    int target_fps,
    Clock* clock) {
  FrameGeneratorCapturer* capturer = new FrameGeneratorCapturer(
      clock,
      input,
      FrameGenerator::CreateFromYuvFile(file_name, width, height),
      target_fps);
  if (!capturer->Init()) {
    delete capturer;
    return NULL;
  }

  return capturer;
}

FrameGeneratorCapturer::FrameGeneratorCapturer(Clock* clock,
                                               VideoSendStreamInput* input,
                                               FrameGenerator* frame_generator,
                                               int target_fps)
    : VideoCapturer(input),
      clock_(clock),
      sending_(false),
      tick_(EventWrapper::Create()),
      lock_(CriticalSectionWrapper::CreateCriticalSection()),
      frame_generator_(frame_generator),
      target_fps_(target_fps),
      first_frame_capture_time_(-1) {
  assert(input != NULL);
  assert(frame_generator != NULL);
  assert(target_fps > 0);
}

FrameGeneratorCapturer::~FrameGeneratorCapturer() {
  Stop();

  if (thread_.get() != NULL)
    thread_->Stop();
}

bool FrameGeneratorCapturer::Init() {
  // This check is added because frame_generator_ might be file based and should
  // not crash because a file moved.
  if (frame_generator_.get() == NULL)
    return false;

  if (!tick_->StartTimer(true, 1000 / target_fps_))
    return false;
  thread_.reset(ThreadWrapper::CreateThread(FrameGeneratorCapturer::Run,
                                            this,
                                            webrtc::kHighPriority,
                                            "FrameGeneratorCapturer"));
  if (thread_.get() == NULL)
    return false;
  unsigned int thread_id;
  if (!thread_->Start(thread_id)) {
    thread_.reset();
    return false;
  }
  return true;
}

bool FrameGeneratorCapturer::Run(void* obj) {
  static_cast<FrameGeneratorCapturer*>(obj)->InsertFrame();
  return true;
}

void FrameGeneratorCapturer::InsertFrame() {
  {
    CriticalSectionScoped cs(lock_.get());
    if (sending_) {
      I420VideoFrame* frame = frame_generator_->NextFrame();
      frame->set_render_time_ms(clock_->CurrentNtpInMilliseconds());
      if (first_frame_capture_time_ == -1) {
        first_frame_capture_time_ = frame->render_time_ms();
      }
      input_->SwapFrame(frame);
    }
  }
  tick_->Wait(WEBRTC_EVENT_INFINITE);
}

void FrameGeneratorCapturer::Start() {
  CriticalSectionScoped cs(lock_.get());
  sending_ = true;
}

void FrameGeneratorCapturer::Stop() {
  CriticalSectionScoped cs(lock_.get());
  sending_ = false;
}
}  // test
}  // webrtc
