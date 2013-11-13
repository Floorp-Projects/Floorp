/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/video_engine/test/libvietest/include/vie_file_capture_device.h"

#include <assert.h>

#include "webrtc/common_types.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/video_engine/include/vie_capture.h"

// This class ensures we are not exceeding the max FPS.
class FramePacemaker {
 public:
  explicit FramePacemaker(uint32_t max_fps)
      : time_per_frame_ms_(1000 / max_fps) {
    frame_start_ = webrtc::TickTime::MillisecondTimestamp();
  }

  void SleepIfNecessary(webrtc::EventWrapper* sleeper) {
    uint64_t now = webrtc::TickTime::MillisecondTimestamp();
    if (now - frame_start_ < time_per_frame_ms_) {
      sleeper->Wait(time_per_frame_ms_ - (now - frame_start_));
    }
  }

 private:
  uint64_t frame_start_;
  uint64_t time_per_frame_ms_;
};

ViEFileCaptureDevice::ViEFileCaptureDevice(
    webrtc::ViEExternalCapture* input_sink)
    : input_sink_(input_sink),
      input_file_(NULL) {
  mutex_ = webrtc::CriticalSectionWrapper::CreateCriticalSection();
}

ViEFileCaptureDevice::~ViEFileCaptureDevice() {
  delete mutex_;
}

bool ViEFileCaptureDevice::OpenI420File(const std::string& path,
                                        int width,
                                        int height) {
  webrtc::CriticalSectionScoped cs(mutex_);
  assert(input_file_ == NULL);

  input_file_ = fopen(path.c_str(), "rb");
  if (input_file_ == NULL) {
    return false;
  }

  frame_length_ = 3 * width * height / 2;
  width_  = width;
  height_ = height;
  return true;
}

void ViEFileCaptureDevice::ReadFileFor(uint64_t time_slice_ms,
                                       uint32_t max_fps) {
  webrtc::CriticalSectionScoped cs(mutex_);
  assert(input_file_ != NULL);

  unsigned char* frame_buffer = new unsigned char[frame_length_];

  webrtc::EventWrapper* sleeper = webrtc::EventWrapper::Create();

  uint64_t start_time_ms = webrtc::TickTime::MillisecondTimestamp();
  uint64_t elapsed_ms = 0;

  while (elapsed_ms < time_slice_ms) {
    FramePacemaker pacemaker(max_fps);
    int read = fread(frame_buffer, 1, frame_length_, input_file_);

    if (feof(input_file_)) {
      rewind(input_file_);
    }
    input_sink_->IncomingFrame(frame_buffer, read, width_, height_,
                               webrtc::kVideoI420,
                               webrtc::TickTime::MillisecondTimestamp());

    pacemaker.SleepIfNecessary(sleeper);
    elapsed_ms = webrtc::TickTime::MillisecondTimestamp() - start_time_ms;
  }

  delete sleeper;
  delete[] frame_buffer;
}

void ViEFileCaptureDevice::CloseFile() {
  webrtc::CriticalSectionScoped cs(mutex_);
  assert(input_file_ != NULL);

  fclose(input_file_);
}
