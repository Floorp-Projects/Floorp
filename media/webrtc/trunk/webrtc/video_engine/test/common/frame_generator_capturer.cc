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

#include <math.h>
#include <string.h>

#include "webrtc/common_video/test/frame_generator.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/video_engine/new_include/video_send_stream.h"

namespace webrtc {
namespace test {
namespace {
class ChromaGenerator : public FrameGenerator {
 public:
  ChromaGenerator(size_t width, size_t height, Clock* clock) : clock_(clock) {
    assert(width > 0);
    assert(height > 0);
    frame_.CreateEmptyFrame(static_cast<int>(width),
                            static_cast<int>(height),
                            static_cast<int>(width),
                            static_cast<int>((width + 1) / 2),
                            static_cast<int>((width + 1) / 2));
    memset(frame_.buffer(kYPlane), 0x80, frame_.allocated_size(kYPlane));
  }

  virtual I420VideoFrame& NextFrame() OVERRIDE {
    double angle =
        static_cast<double>(clock_->CurrentNtpInMilliseconds()) / 1000.0;
    uint8_t u = fabs(sin(angle)) * 0xFF;
    uint8_t v = fabs(cos(angle)) * 0xFF;

    memset(frame_.buffer(kUPlane), u, frame_.allocated_size(kUPlane));
    memset(frame_.buffer(kVPlane), v, frame_.allocated_size(kVPlane));
    return frame_;
  }

 private:
  Clock* clock_;
  I420VideoFrame frame_;
};
}  // namespace

FrameGeneratorCapturer* FrameGeneratorCapturer::Create(
    VideoSendStreamInput* input,
    size_t width,
    size_t height,
    int target_fps,
    Clock* clock) {
  FrameGeneratorCapturer* capturer = new FrameGeneratorCapturer(
      clock, input, new ChromaGenerator(width, height, clock), target_fps);
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
      thread_(NULL),
      frame_generator_(frame_generator),
      target_fps_(target_fps) {
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
      int64_t time_before = clock_->CurrentNtpInMilliseconds();
      I420VideoFrame& frame = frame_generator_->NextFrame();
      frame.set_render_time_ms(time_before);
      int64_t time_after = clock_->CurrentNtpInMilliseconds();
      input_->PutFrame(frame, static_cast<uint32_t>(time_after - time_before));
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
