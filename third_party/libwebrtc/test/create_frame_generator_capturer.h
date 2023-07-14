/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_CREATE_FRAME_GENERATOR_CAPTURER_H_
#define TEST_CREATE_FRAME_GENERATOR_CAPTURER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/frame_generator_interface.h"
#include "api/units/time_delta.h"
#include "system_wrappers/include/clock.h"
#include "test/frame_generator_capturer.h"

namespace webrtc {
namespace test {

std::unique_ptr<FrameGeneratorCapturer> CreateFrameGeneratorCapturer(
    Clock* clock,
    TaskQueueFactory& task_queue_factory,
    FrameGeneratorCapturerConfig::SquaresVideo config) {
  return FrameGeneratorCapturer::Create(clock, task_queue_factory, config);
}

std::unique_ptr<FrameGeneratorCapturer> CreateFrameGeneratorCapturer(
    Clock* clock,
    TaskQueueFactory& task_queue_factory,
    FrameGeneratorCapturerConfig::SquareSlides config) {
  return FrameGeneratorCapturer::Create(clock, task_queue_factory, config);
}

std::unique_ptr<FrameGeneratorCapturer> CreateFrameGeneratorCapturer(
    Clock* clock,
    TaskQueueFactory& task_queue_factory,
    FrameGeneratorCapturerConfig::VideoFile config) {
  return FrameGeneratorCapturer::Create(clock, task_queue_factory, config);
}

std::unique_ptr<FrameGeneratorCapturer> CreateFrameGeneratorCapturer(
    Clock* clock,
    TaskQueueFactory& task_queue_factory,
    FrameGeneratorCapturerConfig::ImageSlides config) {
  return FrameGeneratorCapturer::Create(clock, task_queue_factory, config);
}

std::unique_ptr<FrameGeneratorCapturer> CreateFrameGeneratorCapturer(
    Clock* clock,
    TaskQueueFactory& task_queue_factory,
    const FrameGeneratorCapturerConfig& config) {
  return FrameGeneratorCapturer::Create(clock, task_queue_factory, config);
}

}  // namespace test
}  // namespace webrtc

#endif  // TEST_CREATE_FRAME_GENERATOR_CAPTURER_H_
