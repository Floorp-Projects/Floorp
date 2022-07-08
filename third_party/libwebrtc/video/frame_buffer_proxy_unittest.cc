/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/frame_buffer_proxy.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/video_content_type.h"
#include "rtc_base/event.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/run_loop.h"
#include "test/time_controller/simulated_time_controller.h"

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Matches;
using ::testing::Ne;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::SizeIs;

namespace webrtc {

// For test printing.
void PrintTo(const EncodedFrame& frame, std::ostream* os) {
  *os << "EncodedFrame with id=" << frame.Id() << " rtp=" << frame.Timestamp()
      << " size=" << frame.size() << " refs=[";
  for (size_t ref = 0; ref < frame.num_references; ++ref) {
    *os << frame.references[ref] << ",";
  }
  *os << "]";
}

namespace {

constexpr size_t kFrameSize = 10;
constexpr uint32_t kFps30Rtp = 90000 / 30;
constexpr TimeDelta kFps30Delay = 1 / Frequency::Hertz(30);
const VideoPlayoutDelay kZeroPlayoutDelay = {0, 0};
constexpr Timestamp kClockStart = Timestamp::Millis(1000);

class FakeEncodedFrame : public EncodedFrame {
 public:
  // Always 10ms delay and on time.
  int64_t ReceivedTime() const override {
    if (Timestamp() == 0)
      return kClockStart.ms();
    return TimeDelta::Seconds(Timestamp() / 90000.0).ms() + kClockStart.ms();
  }
  int64_t RenderTime() const override { return _renderTimeMs; }
};

MATCHER_P(FrameWithId, id, "") {
  return Matches(Eq(id))(arg.Id());
}

MATCHER_P(FrameWithSize, id, "") {
  return Matches(Eq(id))(arg.size());
}

class Builder {
 public:
  Builder& Time(uint32_t rtp_timestamp) {
    rtp_timestamp_ = rtp_timestamp;
    return *this;
  }
  Builder& Id(int64_t frame_id) {
    frame_id_ = frame_id;
    return *this;
  }
  Builder& AsLast() {
    last_spatial_layer_ = true;
    return *this;
  }
  Builder& Refs(const std::vector<int64_t>& references) {
    references_ = references;
    return *this;
  }
  Builder& PlayoutDelay(VideoPlayoutDelay playout_delay) {
    playout_delay_ = playout_delay;
    return *this;
  }
  Builder& SpatialLayer(int spatial_layer) {
    spatial_layer_ = spatial_layer;
    return *this;
  }

  std::unique_ptr<FakeEncodedFrame> Build() {
    RTC_CHECK_LE(references_.size(), EncodedFrame::kMaxFrameReferences);
    RTC_CHECK(rtp_timestamp_);
    RTC_CHECK(frame_id_);

    auto frame = std::make_unique<FakeEncodedFrame>();
    frame->SetTimestamp(*rtp_timestamp_);
    frame->SetId(*frame_id_);
    frame->is_last_spatial_layer = last_spatial_layer_;
    frame->SetEncodedData(EncodedImageBuffer::Create(kFrameSize));

    if (playout_delay_)
      frame->SetPlayoutDelay(*playout_delay_);

    for (int64_t ref : references_) {
      frame->references[frame->num_references] = ref;
      frame->num_references++;
    }
    if (spatial_layer_) {
      frame->SetSpatialIndex(spatial_layer_);
    }

    return frame;
  }

 private:
  absl::optional<uint32_t> rtp_timestamp_;
  absl::optional<int64_t> frame_id_;
  absl::optional<VideoPlayoutDelay> playout_delay_;
  absl::optional<int> spatial_layer_;
  bool last_spatial_layer_ = false;
  std::vector<int64_t> references_;
};

class VCMReceiveStatisticsCallbackMock : public VCMReceiveStatisticsCallback {
 public:
  MOCK_METHOD(void,
              OnCompleteFrame,
              (bool is_keyframe,
               size_t size_bytes,
               VideoContentType content_type),
              (override));
  MOCK_METHOD(void, OnDroppedFrames, (uint32_t num_dropped), (override));
  MOCK_METHOD(void,
              OnFrameBufferTimingsUpdated,
              (int max_decode_ms,
               int current_delay_ms,
               int target_delay_ms,
               int jitter_buffer_ms,
               int min_playout_delay_ms,
               int render_delay_ms),
              (override));
  MOCK_METHOD(void,
              OnTimingFrameInfoUpdated,
              (const TimingFrameInfo& info),
              (override));
};
}  // namespace

constexpr auto kMaxWaitForKeyframe = TimeDelta::Millis(500);
constexpr auto kMaxWaitForFrame = TimeDelta::Millis(1500);
class FrameBufferProxyTest : public ::testing::TestWithParam<std::string>,
                             public FrameSchedulingReceiver {
 public:
  FrameBufferProxyTest()
      : field_trials_(GetParam()),
        time_controller_(kClockStart),
        clock_(time_controller_.GetClock()),
        decode_queue_(time_controller_.GetTaskQueueFactory()->CreateTaskQueue(
            "decode_queue",
            TaskQueueFactory::Priority::NORMAL)),
        timing_(clock_),
        proxy_(FrameBufferProxy::CreateFromFieldTrial(clock_,
                                                      run_loop_.task_queue(),
                                                      &timing_,
                                                      &stats_callback_,
                                                      &decode_queue_,
                                                      this,
                                                      kMaxWaitForKeyframe,
                                                      kMaxWaitForFrame)) {
    // Avoid starting with negative render times.
    timing_.set_min_playout_delay(10);

    ON_CALL(stats_callback_, OnDroppedFrames)
        .WillByDefault(
            [this](auto num_dropped) { dropped_frames_ += num_dropped; });
  }

  ~FrameBufferProxyTest() override {
    if (proxy_) {
      proxy_->StopOnWorker();
    }
  }

  void OnEncodedFrame(std::unique_ptr<EncodedFrame> frame) override {
    last_frame_ = std::move(frame);
    run_loop_.Quit();
  }

  void OnDecodableFrameTimeout(TimeDelta wait_time) override {
    timeouts_++;
    run_loop_.Quit();
  }

  bool WaitForFrameOrTimeout(TimeDelta wait) {
    if (NewFrameOrTimeout()) {
      return true;
    }
    run_loop_.PostTask([&] { time_controller_.AdvanceTime(wait); });
    run_loop_.PostTask([&] {
      // If run loop posted to a task queue, flush that.
      time_controller_.AdvanceTime(TimeDelta::Zero());

      run_loop_.PostTask([&] {
        time_controller_.AdvanceTime(TimeDelta::Zero());
        run_loop_.Quit();
      });
    });
    run_loop_.Run();
    return NewFrameOrTimeout();
  }

  void StartNextDecode() {
    ResetLastResult();
    proxy_->StartNextDecode(false);
    time_controller_.AdvanceTime(TimeDelta::Zero());
  }

  void StartNextDecodeForceKeyframe() {
    ResetLastResult();
    proxy_->StartNextDecode(true);
    time_controller_.AdvanceTime(TimeDelta::Zero());
  }

  void ResetLastResult() {
    last_frame_.reset();
    last_timeouts_ = timeouts_;
  }

  int timeouts() const { return timeouts_; }
  EncodedFrame* last_frame() const { return last_frame_.get(); }
  int dropped_frames() const { return dropped_frames_; }

 protected:
  test::ScopedFieldTrials field_trials_;
  GlobalSimulatedTimeController time_controller_;
  Clock* const clock_;
  test::RunLoop run_loop_;
  rtc::TaskQueue decode_queue_;
  VCMTiming timing_;
  ::testing::NiceMock<VCMReceiveStatisticsCallbackMock> stats_callback_;
  std::unique_ptr<FrameBufferProxy> proxy_;

 private:
  bool NewFrameOrTimeout() const {
    return last_frame_ || timeouts_ != last_timeouts_;
  }

  int timeouts_ = 0;
  int last_timeouts_ = 0;
  std::unique_ptr<EncodedFrame> last_frame_;
  uint32_t dropped_frames_ = 0;
};

TEST_P(FrameBufferProxyTest, InitialTimeoutAfterKeyframeTimeoutPeriod) {
  StartNextDecodeForceKeyframe();
  // No frame insterted. Timeout expected.
  EXPECT_TRUE(WaitForFrameOrTimeout(kMaxWaitForKeyframe));
  EXPECT_EQ(timeouts(), 1);

  // No new timeout set since receiver has not started new decode.
  ResetLastResult();
  EXPECT_FALSE(WaitForFrameOrTimeout(kMaxWaitForKeyframe));
  EXPECT_EQ(timeouts(), 1);

  // Now that receiver has asked for new frame, a new timeout can occur.
  StartNextDecodeForceKeyframe();
  EXPECT_TRUE(WaitForFrameOrTimeout(kMaxWaitForKeyframe));
  EXPECT_EQ(timeouts(), 2);
}

TEST_P(FrameBufferProxyTest, KeyFramesAreScheduled) {
  StartNextDecodeForceKeyframe();
  time_controller_.AdvanceTime(TimeDelta::Millis(50));

  auto frame = Builder().Id(0).Time(0).AsLast().Build();
  proxy_->InsertFrame(std::move(frame));

  EXPECT_TRUE(WaitForFrameOrTimeout(TimeDelta::Zero()));

  ASSERT_THAT(last_frame(), Pointee(FrameWithId(0)));
  EXPECT_EQ(timeouts(), 0);
}

TEST_P(FrameBufferProxyTest, DeltaFrameTimeoutAfterKeyframeExtracted) {
  StartNextDecodeForceKeyframe();

  time_controller_.AdvanceTime(TimeDelta::Millis(50));
  auto frame = Builder().Id(0).Time(0).AsLast().Build();
  proxy_->InsertFrame(std::move(frame));
  EXPECT_TRUE(WaitForFrameOrTimeout(kMaxWaitForKeyframe));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(0)));

  StartNextDecode();
  time_controller_.AdvanceTime(TimeDelta::Millis(50));

  // Timeouts should now happen at the normal frequency.
  const int expected_timeouts = 5;
  for (int i = 0; i < expected_timeouts; ++i) {
    EXPECT_TRUE(WaitForFrameOrTimeout(kMaxWaitForFrame));
    StartNextDecode();
  }

  EXPECT_EQ(timeouts(), expected_timeouts);
}

TEST_P(FrameBufferProxyTest, DependantFramesAreScheduled) {
  StartNextDecodeForceKeyframe();
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  EXPECT_TRUE(WaitForFrameOrTimeout(TimeDelta::Zero()));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(0)));

  StartNextDecode();

  time_controller_.AdvanceTime(kFps30Delay);
  proxy_->InsertFrame(
      Builder().Id(1).Time(kFps30Rtp).AsLast().Refs({0}).Build());
  EXPECT_TRUE(WaitForFrameOrTimeout(kFps30Delay));

  EXPECT_THAT(last_frame(), Pointee(FrameWithId(1)));
  EXPECT_EQ(timeouts(), 0);
}

TEST_P(FrameBufferProxyTest, SpatialLayersAreScheduled) {
  StartNextDecodeForceKeyframe();
  proxy_->InsertFrame(Builder().Id(0).SpatialLayer(0).Time(0).Build());
  proxy_->InsertFrame(Builder().Id(1).SpatialLayer(1).Time(0).Build());
  proxy_->InsertFrame(Builder().Id(2).SpatialLayer(2).Time(0).AsLast().Build());
  EXPECT_TRUE(WaitForFrameOrTimeout(TimeDelta::Zero()));
  EXPECT_THAT(last_frame(),
              Pointee(AllOf(FrameWithId(0), FrameWithSize(3 * kFrameSize))));

  proxy_->InsertFrame(Builder().Id(3).Time(kFps30Rtp).SpatialLayer(0).Build());
  proxy_->InsertFrame(Builder().Id(4).Time(kFps30Rtp).SpatialLayer(1).Build());
  proxy_->InsertFrame(
      Builder().Id(5).Time(kFps30Rtp).SpatialLayer(2).AsLast().Build());

  StartNextDecode();
  EXPECT_TRUE(WaitForFrameOrTimeout(kFps30Delay * 10));
  EXPECT_THAT(last_frame(),
              Pointee(AllOf(FrameWithId(3), FrameWithSize(3 * kFrameSize))));
  EXPECT_EQ(timeouts(), 0);
}

TEST_P(FrameBufferProxyTest, OutstandingFrameTasksAreCancelledAfterDeletion) {
  StartNextDecodeForceKeyframe();
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  // Get keyframe. Delta frame should now be scheduled.
  EXPECT_TRUE(WaitForFrameOrTimeout(TimeDelta::Zero()));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(0)));

  StartNextDecode();
  proxy_->InsertFrame(
      Builder().Id(1).Time(kFps30Rtp).AsLast().Refs({0}).Build());
  proxy_->StopOnWorker();
  // Wait for 2x max wait time. Since we stopped, this should cause no timeouts
  // or frame-ready callbacks.
  EXPECT_FALSE(WaitForFrameOrTimeout(kMaxWaitForFrame * 2));
  EXPECT_EQ(timeouts(), 0);
}

TEST_P(FrameBufferProxyTest, FramesWaitForDecoderToComplete) {
  StartNextDecodeForceKeyframe();

  // Start with a keyframe.
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  EXPECT_TRUE(WaitForFrameOrTimeout(TimeDelta::Zero()));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(0)));

  ResetLastResult();
  // Insert a delta frame.
  proxy_->InsertFrame(
      Builder().Id(1).Time(kFps30Rtp).AsLast().Refs({0}).Build());

  // Advancing time should not result in a frame since the scheduler has not
  // been signalled that we are ready.
  EXPECT_FALSE(WaitForFrameOrTimeout(kFps30Delay));
  // Signal ready.
  StartNextDecode();
  EXPECT_TRUE(WaitForFrameOrTimeout(kFps30Delay));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(1)));
}

TEST_P(FrameBufferProxyTest, LateFrameDropped) {
  StartNextDecodeForceKeyframe();
  //   F1
  //   /
  // F0 --> F2
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  // Start with a keyframe.
  EXPECT_TRUE(WaitForFrameOrTimeout(TimeDelta::Zero()));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(0)));

  StartNextDecode();

  // Simulate late F1 which arrives after F2.
  time_controller_.AdvanceTime(kFps30Delay * 2);
  proxy_->InsertFrame(
      Builder().Id(2).Time(2 * kFps30Rtp).AsLast().Refs({0}).Build());
  EXPECT_TRUE(WaitForFrameOrTimeout(kFps30Delay));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(2)));

  StartNextDecode();

  proxy_->InsertFrame(
      Builder().Id(1).Time(1 * kFps30Rtp).AsLast().Refs({0}).Build());
  EXPECT_TRUE(WaitForFrameOrTimeout(kMaxWaitForFrame));
  // Confirm frame 1 is never scheduled by timing out.
  EXPECT_EQ(timeouts(), 1);
}

TEST_P(FrameBufferProxyTest, FramesFastForwardOnSystemHalt) {
  StartNextDecodeForceKeyframe();
  //   F1
  //   /
  // F0 --> F2
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());

  // Start with a keyframe.
  EXPECT_TRUE(WaitForFrameOrTimeout(TimeDelta::Zero()));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(0)));

  time_controller_.AdvanceTime(kFps30Delay);
  proxy_->InsertFrame(
      Builder().Id(1).Time(kFps30Rtp).AsLast().Refs({0}).Build());
  time_controller_.AdvanceTime(kFps30Delay);
  proxy_->InsertFrame(
      Builder().Id(2).Time(2 * kFps30Rtp).AsLast().Refs({0}).Build());

  // Halting time should result in F1 being skipped.
  time_controller_.AdvanceTime(kFps30Delay * 2);
  StartNextDecode();
  EXPECT_TRUE(WaitForFrameOrTimeout(kFps30Delay));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(2)));
  EXPECT_EQ(dropped_frames(), 1);
}

TEST_P(FrameBufferProxyTest, ForceKeyFrame) {
  StartNextDecodeForceKeyframe();
  // Initial keyframe.
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  EXPECT_TRUE(WaitForFrameOrTimeout(TimeDelta::Zero()));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(0)));

  StartNextDecodeForceKeyframe();

  // F2 is the next keyframe, and should be extracted since a keyframe was
  // forced.
  proxy_->InsertFrame(
      Builder().Id(1).Time(kFps30Rtp).AsLast().Refs({0}).Build());
  proxy_->InsertFrame(Builder().Id(2).Time(kFps30Rtp * 2).AsLast().Build());

  EXPECT_TRUE(WaitForFrameOrTimeout(kMaxWaitForFrame));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(2)));
}

TEST_P(FrameBufferProxyTest, SlowDecoderDropsTemporalLayers) {
  StartNextDecodeForceKeyframe();
  // 2 temporal layers, at 15fps per layer to make 30fps total.
  // Decoder is slower than 30fps, so last_frame() will be skipped.
  //   F1 --> F3 --> F5
  //   /      /     /
  // F0 --> F2 --> F4
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  // Keyframe received.
  EXPECT_TRUE(WaitForFrameOrTimeout(TimeDelta::Zero()));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(0)));
  // Don't start next decode until slow delay.

  time_controller_.AdvanceTime(kFps30Delay);
  proxy_->InsertFrame(
      Builder().Id(1).Time(1 * kFps30Rtp).Refs({0}).AsLast().Build());
  time_controller_.AdvanceTime(kFps30Delay);
  proxy_->InsertFrame(
      Builder().Id(2).Time(2 * kFps30Rtp).Refs({0}).AsLast().Build());

  // Simulate decode taking 3x FPS rate.
  time_controller_.AdvanceTime(kFps30Delay * 1.5);
  StartNextDecode();
  EXPECT_TRUE(WaitForFrameOrTimeout(kFps30Delay * 2));
  // F2 is the best frame since decoding was so slow that F1 is too old.
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(2)));
  EXPECT_EQ(dropped_frames(), 1);
  time_controller_.AdvanceTime(kFps30Delay / 2);

  proxy_->InsertFrame(
      Builder().Id(3).Time(3 * kFps30Rtp).Refs({1, 2}).AsLast().Build());
  time_controller_.AdvanceTime(kFps30Delay / 2);
  proxy_->InsertFrame(
      Builder().Id(4).Time(4 * kFps30Rtp).Refs({2}).AsLast().Build());
  time_controller_.AdvanceTime(kFps30Delay / 2);

  // F4 is the best frame since decoding was so slow that F1 is too old.
  time_controller_.AdvanceTime(kFps30Delay);
  StartNextDecode();
  EXPECT_TRUE(WaitForFrameOrTimeout(kFps30Delay));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(4)));

  proxy_->InsertFrame(
      Builder().Id(5).Time(5 * kFps30Rtp).Refs({3, 4}).AsLast().Build());
  time_controller_.AdvanceTime(kFps30Delay / 2);

  // F5 is not decodable since F4 was decoded, so a timeout is expected.
  time_controller_.AdvanceTime(TimeDelta::Millis(10));
  StartNextDecode();
  EXPECT_TRUE(WaitForFrameOrTimeout(kMaxWaitForFrame));
  EXPECT_EQ(timeouts(), 1);
  // TODO(bugs.webrtc.org/13343): This should be 2 dropped frames since frames 1
  // and 3 were dropped. However, frame_buffer2 does not mark frame 3 as dropped
  // which is a bug. Uncomment below when that is fixed for frame_buffer2 is
  // deleted.
  // EXPECT_EQ(dropped_frames(), 2);
}

TEST_P(FrameBufferProxyTest, OldTimestampNotDecodable) {
  StartNextDecodeForceKeyframe();

  proxy_->InsertFrame(Builder().Id(0).Time(kFps30Rtp).AsLast().Build());
  EXPECT_TRUE(WaitForFrameOrTimeout(kFps30Delay));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(0)));

  // Timestamp is before previous frame's.
  proxy_->InsertFrame(Builder().Id(1).Time(0).AsLast().Build());
  StartNextDecode();
  // F1 should be dropped since its timestamp went backwards.
  EXPECT_TRUE(WaitForFrameOrTimeout(kMaxWaitForFrame));
  EXPECT_EQ(timeouts(), 1);
}

TEST_P(FrameBufferProxyTest, NewFrameInsertedWhileWaitingToReleaseFrame) {
  StartNextDecodeForceKeyframe();
  // Initial keyframe.
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  EXPECT_TRUE(WaitForFrameOrTimeout(TimeDelta::Zero()));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(0)));

  time_controller_.AdvanceTime(kFps30Delay);
  proxy_->InsertFrame(
      Builder().Id(1).Time(kFps30Rtp).Refs({0}).AsLast().Build());
  StartNextDecode();
  EXPECT_FALSE(WaitForFrameOrTimeout(TimeDelta::Zero()));

  // Scheduler is waiting to deliver Frame 1 now. Insert Frame 2. Frame 1 should
  // be delivered still.
  proxy_->InsertFrame(
      Builder().Id(2).Time(kFps30Rtp * 2).Refs({0}).AsLast().Build());

  EXPECT_TRUE(WaitForFrameOrTimeout(kFps30Delay));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(1)));
}

TEST_P(FrameBufferProxyTest, SameFrameNotScheduledTwice) {
  // A frame could be scheduled twice if last_frame() arrive out-of-order but
  // the older frame is old enough to be fast forwarded.
  //
  // 1. F2 arrives and is scheduled.
  // 2. F3 arrives, but scheduling will not change since F2 is next.
  // 3. F1 arrives late and scheduling is checked since it is before F2. F1
  // fast-forwarded since it is older.
  //
  // F2 is the best frame, but should only be scheduled once, followed by F3.
  StartNextDecodeForceKeyframe();

  // First keyframe.
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  EXPECT_TRUE(WaitForFrameOrTimeout(TimeDelta::Millis(15)));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(0)));

  StartNextDecode();

  // Warmup VCMTiming for 30fps.
  for (int i = 1; i <= 30; ++i) {
    proxy_->InsertFrame(Builder().Id(i).Time(i * kFps30Rtp).AsLast().Build());
    EXPECT_TRUE(WaitForFrameOrTimeout(kFps30Delay));
    EXPECT_THAT(last_frame(), Pointee(FrameWithId(i)));
    StartNextDecode();
  }

  // F2 arrives and is scheduled.
  proxy_->InsertFrame(Builder().Id(32).Time(32 * kFps30Rtp).AsLast().Build());

  // F3 arrives before F2 is extracted.
  time_controller_.AdvanceTime(kFps30Delay);
  proxy_->InsertFrame(Builder().Id(33).Time(33 * kFps30Rtp).AsLast().Build());

  // F1 arrives and is fast-forwarded since it is too late.
  // F2 is already scheduled and should not be rescheduled.
  time_controller_.AdvanceTime(kFps30Delay / 2);
  proxy_->InsertFrame(Builder().Id(31).Time(31 * kFps30Rtp).AsLast().Build());

  EXPECT_TRUE(WaitForFrameOrTimeout(kFps30Delay));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(32)));
  StartNextDecode();

  EXPECT_TRUE(WaitForFrameOrTimeout(kFps30Delay));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(33)));
  EXPECT_EQ(dropped_frames(), 1);
}

TEST_P(FrameBufferProxyTest, TestStatsCallback) {
  EXPECT_CALL(stats_callback_,
              OnCompleteFrame(true, kFrameSize, VideoContentType::UNSPECIFIED));
  EXPECT_CALL(stats_callback_, OnFrameBufferTimingsUpdated);

  // Fake timing having received decoded frame.
  timing_.StopDecodeTimer(clock_->TimeInMicroseconds() + 1,
                          clock_->TimeInMilliseconds());
  StartNextDecodeForceKeyframe();
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  EXPECT_TRUE(WaitForFrameOrTimeout(TimeDelta::Zero()));
  EXPECT_THAT(last_frame(), Pointee(FrameWithId(0)));

  // Flush stats posted on the decode queue.
  time_controller_.AdvanceTime(TimeDelta::Zero());
}

INSTANTIATE_TEST_SUITE_P(FrameBufferProxy,
                         FrameBufferProxyTest,
                         ::testing::Values("WebRTC-FrameBuffer3/Disabled/"));

}  // namespace webrtc
