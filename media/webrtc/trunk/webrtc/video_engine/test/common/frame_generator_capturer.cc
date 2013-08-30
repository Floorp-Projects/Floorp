/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/test/common/frame_generator_capturer.h"

#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/video_engine/test/common/frame_generator.h"

namespace webrtc {
namespace test {

FrameGeneratorCapturer* FrameGeneratorCapturer::Create(
    newapi::VideoSendStreamInput* input,
    FrameGenerator* frame_generator,
    int target_fps,
    Clock* clock) {
  FrameGeneratorCapturer* capturer =
      new FrameGeneratorCapturer(input, frame_generator, target_fps, clock);

  if (!capturer->Init()) {
    delete capturer;
    return NULL;
  }

  return capturer;
}

FrameGeneratorCapturer::FrameGeneratorCapturer(
    newapi::VideoSendStreamInput* input,
    FrameGenerator* frame_generator,
    int target_fps,
    Clock* clock)
    : VideoCapturer(input),
      sending_(false),
      clock_(clock),
      lock_(CriticalSectionWrapper::CreateCriticalSection()),
      thread_(NULL),
      frame_generator_(frame_generator),
      target_fps_(target_fps) {
  assert(input != NULL);
  assert(frame_generator != NULL);
  assert(target_fps > 0);
}

FrameGeneratorCapturer::~FrameGeneratorCapturer() {
  Stop();

  if (thread_ != NULL) {
    if (!thread_->Stop()) {
      // TODO(pbos): Log a warning. This will leak a thread.
    } else {
      delete thread_;
    }
  }
}

bool FrameGeneratorCapturer::Init() {
  thread_ = ThreadWrapper::CreateThread(FrameGeneratorCapturer::Run,
                                        this,
                                        webrtc::kHighPriority,
                                        "FrameGeneratorCapturer");
  if (thread_ == NULL)
    return false;
  unsigned int thread_id;
  if (!thread_->Start(thread_id)) {
    delete thread_;
    thread_ = NULL;
    return false;
  }
  return true;
}

bool FrameGeneratorCapturer::Run(void* obj) {
  static_cast<FrameGeneratorCapturer*>(obj)->InsertFrame();
  return true;
}

void FrameGeneratorCapturer::InsertFrame() {
  int64_t time_start = clock_->TimeInMilliseconds();

  {
    CriticalSectionScoped cs(lock_);
    if (sending_)
      frame_generator_->InsertFrame(input_);
  }

  int64_t remaining_sleep =
      1000 / target_fps_ - (clock_->TimeInMilliseconds() - time_start);

  if (remaining_sleep > 0) {
    SleepMs(static_cast<int>(remaining_sleep));
  }
}

void FrameGeneratorCapturer::Start() {
  CriticalSectionScoped cs(lock_);
  sending_ = true;
}

void FrameGeneratorCapturer::Stop() {
  CriticalSectionScoped cs(lock_);
  sending_ = false;
}
}  // test
}  // webrtc
