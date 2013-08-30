/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/test/common/frame_generator.h"

#include <cmath>
#include <cstring>

#include "webrtc/system_wrappers/interface/clock.h"

namespace webrtc {
namespace test {

FrameGenerator* FrameGenerator::Create(size_t width,
                                       size_t height,
                                       Clock* clock) {
  return new ChromaFrameGenerator(width, height, clock);
}

void FrameGenerator::InsertFrame(newapi::VideoSendStreamInput* input) {
  int64_t time_before = clock_->TimeInMilliseconds();
  frame_.set_render_time_ms(time_before);

  GenerateNextFrame();

  int64_t time_after = clock_->TimeInMilliseconds();
  input->PutFrame(frame_, static_cast<uint32_t>(time_after - time_before));
}

FrameGenerator::FrameGenerator(size_t width, size_t height, Clock* clock)
    : width_(width), height_(height), clock_(clock) {
  // Generate frame by constructor arguments
  assert(width > 0);
  assert(height > 0);
  frame_.CreateEmptyFrame(static_cast<int>(width),
                          static_cast<int>(height),
                          static_cast<int>(width),
                          static_cast<int>((width + 1) / 2),
                          static_cast<int>((width + 1) / 2));
}

BlackFrameGenerator::BlackFrameGenerator(size_t width,
                                         size_t height,
                                         Clock* clock)
    : FrameGenerator(width, height, clock) {
  memset(frame_.buffer(kYPlane), 0x00, frame_.allocated_size(kYPlane));
  memset(frame_.buffer(kUPlane), 0x80, frame_.allocated_size(kUPlane));
  memset(frame_.buffer(kVPlane), 0x80, frame_.allocated_size(kVPlane));
}

void BlackFrameGenerator::GenerateNextFrame() {}

WhiteFrameGenerator::WhiteFrameGenerator(size_t width,
                                         size_t height,
                                         Clock* clock)
    : FrameGenerator(width, height, clock) {
  memset(frame_.buffer(kYPlane), 0xFF, frame_.allocated_size(kYPlane));
  memset(frame_.buffer(kUPlane), 0x80, frame_.allocated_size(kUPlane));
  memset(frame_.buffer(kVPlane), 0x80, frame_.allocated_size(kVPlane));
}

void WhiteFrameGenerator::GenerateNextFrame() {}

ChromaFrameGenerator::ChromaFrameGenerator(size_t width,
                                           size_t height,
                                           Clock* clock)
    : FrameGenerator(width, height, clock) {
  memset(frame_.buffer(kYPlane), 0x80, frame_.allocated_size(kYPlane));
}

void ChromaFrameGenerator::GenerateNextFrame() {
  double angle = static_cast<double>(frame_.render_time_ms()) / 1000.0;
  uint8_t u = fabs(sin(angle)) * 0xFF;
  uint8_t v = fabs(cos(angle)) * 0xFF;

  memset(frame_.buffer(kUPlane), u, frame_.allocated_size(kUPlane));
  memset(frame_.buffer(kVPlane), v, frame_.allocated_size(kVPlane));
}
}  // test
}  // webrtc
