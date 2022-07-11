/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/frame_decode_timing.h"

#include <stdint.h>

#include "absl/types/optional.h"
#include "api/units/time_delta.h"
#include "modules/video_coding/timing.h"
#include "rtc_base/containers/flat_map.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Optional;

namespace {

class FakeVCMTiming : public webrtc::VCMTiming {
 public:
  explicit FakeVCMTiming(Clock* clock) : webrtc::VCMTiming(clock) {}

  int64_t RenderTimeMs(uint32_t frame_timestamp,
                       int64_t now_ms) const override {
    RTC_DCHECK(render_time_map_.contains(frame_timestamp));
    auto it = render_time_map_.find(frame_timestamp);
    return it->second.ms();
  }

  int64_t MaxWaitingTime(int64_t render_time_ms,
                         int64_t now_ms,
                         bool too_many_frames_queued) const override {
    auto render_time = Timestamp::Millis(render_time_ms);
    RTC_DCHECK(wait_time_map_.contains(render_time));
    auto it = wait_time_map_.find(render_time);
    return it->second.ms();
  }

  void SetTimes(uint32_t frame_timestamp,
                Timestamp render_time,
                TimeDelta max_decode_wait) {
    render_time_map_.insert_or_assign(frame_timestamp, render_time);
    wait_time_map_.insert_or_assign(render_time, max_decode_wait);
  }

 protected:
  flat_map<uint32_t, Timestamp> render_time_map_;
  flat_map<Timestamp, TimeDelta> wait_time_map_;
};
}  // namespace

class FrameDecodeTimingTest : public ::testing::Test {
 public:
  FrameDecodeTimingTest()
      : clock_(Timestamp::Millis(1000)),
        timing_(&clock_),
        frame_decode_scheduler_(&clock_, &timing_) {}

 protected:
  SimulatedClock clock_;
  FakeVCMTiming timing_;
  FrameDecodeTiming frame_decode_scheduler_;
};

TEST_F(FrameDecodeTimingTest, ReturnsWaitTimesWhenValid) {
  const TimeDelta decode_delay = TimeDelta::Millis(42);
  const Timestamp render_time = clock_.CurrentTime() + TimeDelta::Millis(60);
  timing_.SetTimes(90000, render_time, decode_delay);

  EXPECT_THAT(
      frame_decode_scheduler_.OnFrameBufferUpdated(90000, 180000, false),
      Optional(
          AllOf(Field(&FrameDecodeTiming::FrameSchedule::latest_decode_time,
                      Eq(clock_.CurrentTime() + decode_delay)),
                Field(&FrameDecodeTiming::FrameSchedule::render_time,
                      Eq(render_time)))));
}

TEST_F(FrameDecodeTimingTest, FastForwardsFrameTooFarInThePast) {
  const TimeDelta decode_delay = TimeDelta::Millis(-6);
  const Timestamp render_time = clock_.CurrentTime();
  timing_.SetTimes(90000, render_time, decode_delay);

  EXPECT_THAT(
      frame_decode_scheduler_.OnFrameBufferUpdated(90000, 180000, false),
      Eq(absl::nullopt));
}

TEST_F(FrameDecodeTimingTest, NoFastForwardIfOnlyFrameToDecode) {
  const TimeDelta decode_delay = TimeDelta::Millis(-6);
  const Timestamp render_time = clock_.CurrentTime();
  timing_.SetTimes(90000, render_time, decode_delay);

  EXPECT_THAT(frame_decode_scheduler_.OnFrameBufferUpdated(90000, 90000, false),
              Optional(AllOf(
                  Field(&FrameDecodeTiming::FrameSchedule::latest_decode_time,
                        Eq(clock_.CurrentTime() + decode_delay)),
                  Field(&FrameDecodeTiming::FrameSchedule::render_time,
                        Eq(render_time)))));
}

}  // namespace webrtc
