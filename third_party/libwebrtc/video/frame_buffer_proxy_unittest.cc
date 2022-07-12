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

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "api/metronome/test/fake_metronome.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/video_content_type.h"
#include "api/video/video_timing.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/run_loop.h"
#include "test/scoped_key_value_config.h"
#include "test/time_controller/simulated_time_controller.h"
#include "video/decode_synchronizer.h"

using ::testing::_;
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
using ::testing::VariantWith;

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
  int64_t ReceivedTime() const override { return received_time_; }
  int64_t RenderTime() const override { return _renderTimeMs; }

  void SetReceivedTime(int64_t received_time) {
    received_time_ = received_time;
  }

 private:
  int64_t received_time_;
};

MATCHER_P(WithId, id, "") {
  return Matches(Eq(id))(arg.Id());
}

MATCHER_P(FrameWithSize, id, "") {
  return Matches(Eq(id))(arg.size());
}

auto TimedOut() {
  return Optional(VariantWith<TimeDelta>(_));
}

auto Frame(testing::Matcher<EncodedFrame> m) {
  return Optional(VariantWith<std::unique_ptr<EncodedFrame>>(Pointee(m)));
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
  Builder& ReceivedTime(Timestamp receive_time) {
    received_time_ = receive_time;
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
    if (received_time_) {
      frame->SetReceivedTime(received_time_->ms());
    } else {
      if (*rtp_timestamp_ == 0)
        frame->SetReceivedTime(kClockStart.ms());
      frame->SetReceivedTime(
          TimeDelta::Seconds(*rtp_timestamp_ / 90000.0).ms() +
          kClockStart.ms());
    }

    return frame;
  }

 private:
  absl::optional<uint32_t> rtp_timestamp_;
  absl::optional<int64_t> frame_id_;
  absl::optional<VideoPlayoutDelay> playout_delay_;
  absl::optional<int> spatial_layer_;
  absl::optional<Timestamp> received_time_;
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

bool IsFrameBuffer2Enabled(const FieldTrialsView& field_trials) {
  return field_trials.Lookup("WebRTC-FrameBuffer3").find("arm:FrameBuffer2") !=
         std::string::npos;
}

}  // namespace

constexpr auto kMaxWaitForKeyframe = TimeDelta::Millis(500);
constexpr auto kMaxWaitForFrame = TimeDelta::Millis(1500);
class FrameBufferProxyFixture
    : public ::testing::WithParamInterface<std::string>,
      public FrameSchedulingReceiver {
 public:
  FrameBufferProxyFixture()
      : field_trials_(GetParam()),
        time_controller_(kClockStart),
        clock_(time_controller_.GetClock()),
        decode_queue_(time_controller_.GetTaskQueueFactory()->CreateTaskQueue(
            "decode_queue",
            TaskQueueFactory::Priority::NORMAL)),
        fake_metronome_(time_controller_.GetTaskQueueFactory(),
                        TimeDelta::Millis(16)),
        decode_sync_(clock_, &fake_metronome_, run_loop_.task_queue()),
        timing_(clock_, field_trials_),
        proxy_(FrameBufferProxy::CreateFromFieldTrial(clock_,
                                                      run_loop_.task_queue(),
                                                      &timing_,
                                                      &stats_callback_,
                                                      &decode_queue_,
                                                      this,
                                                      kMaxWaitForKeyframe,
                                                      kMaxWaitForFrame,
                                                      &decode_sync_,
                                                      field_trials_)) {
    // Avoid starting with negative render times.
    timing_.set_min_playout_delay(TimeDelta::Millis(10));

    ON_CALL(stats_callback_, OnDroppedFrames)
        .WillByDefault(
            [this](auto num_dropped) { dropped_frames_ += num_dropped; });
  }

  ~FrameBufferProxyFixture() override {
    if (proxy_) {
      proxy_->StopOnWorker();
    }
    fake_metronome_.Stop();
    time_controller_.AdvanceTime(TimeDelta::Zero());
  }

  void OnEncodedFrame(std::unique_ptr<EncodedFrame> frame) override {
    RTC_DCHECK(frame);
    SetWaitResult(std::move(frame));
  }

  void OnDecodableFrameTimeout(TimeDelta wait_time) override {
    SetWaitResult(wait_time);
  }

  using WaitResult =
      absl::variant<std::unique_ptr<EncodedFrame>, TimeDelta /*wait_time*/>;

  absl::optional<WaitResult> WaitForFrameOrTimeout(TimeDelta wait) {
    if (wait_result_) {
      return std::move(wait_result_);
    }
    run_loop_.PostTask([&] { time_controller_.AdvanceTime(wait); });
    run_loop_.PostTask([&] {
      if (wait_result_)
        return;

      // If run loop posted to a task queue, flush that if there is no result.
      time_controller_.AdvanceTime(TimeDelta::Zero());
      if (wait_result_)
        return;

      run_loop_.PostTask([&] {
        time_controller_.AdvanceTime(TimeDelta::Zero());
        // Quit if there is no result set.
        if (!wait_result_)
          run_loop_.Quit();
      });
    });
    run_loop_.Run();
    return std::move(wait_result_);
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

  void ResetLastResult() { wait_result_.reset(); }

  int dropped_frames() const { return dropped_frames_; }

 protected:
  test::ScopedKeyValueConfig field_trials_;
  GlobalSimulatedTimeController time_controller_;
  Clock* const clock_;
  test::RunLoop run_loop_;
  rtc::TaskQueue decode_queue_;
  test::FakeMetronome fake_metronome_;
  DecodeSynchronizer decode_sync_;
  VCMTiming timing_;

  ::testing::NiceMock<VCMReceiveStatisticsCallbackMock> stats_callback_;
  std::unique_ptr<FrameBufferProxy> proxy_;

 private:
  void SetWaitResult(WaitResult result) {
    RTC_DCHECK(!wait_result_);
    if (absl::holds_alternative<std::unique_ptr<EncodedFrame>>(result)) {
      RTC_DCHECK(absl::get<std::unique_ptr<EncodedFrame>>(result));
    }
    wait_result_.emplace(std::move(result));
    run_loop_.Quit();
  }

  uint32_t dropped_frames_ = 0;
  absl::optional<WaitResult> wait_result_;
};

class FrameBufferProxyTest : public ::testing::Test,
                             public FrameBufferProxyFixture {};

TEST_P(FrameBufferProxyTest, InitialTimeoutAfterKeyframeTimeoutPeriod) {
  StartNextDecodeForceKeyframe();
  // No frame insterted. Timeout expected.
  EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForKeyframe), TimedOut());

  // No new timeout set since receiver has not started new decode.
  ResetLastResult();
  EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForKeyframe), Eq(absl::nullopt));

  // Now that receiver has asked for new frame, a new timeout can occur.
  StartNextDecodeForceKeyframe();
  EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForKeyframe), TimedOut());
}

TEST_P(FrameBufferProxyTest, KeyFramesAreScheduled) {
  StartNextDecodeForceKeyframe();
  time_controller_.AdvanceTime(TimeDelta::Millis(50));

  auto frame = Builder().Id(0).Time(0).AsLast().Build();
  proxy_->InsertFrame(std::move(frame));

  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(WithId(0)));
}

TEST_P(FrameBufferProxyTest, DeltaFrameTimeoutAfterKeyframeExtracted) {
  StartNextDecodeForceKeyframe();

  time_controller_.AdvanceTime(TimeDelta::Millis(50));
  auto frame = Builder().Id(0).Time(0).AsLast().Build();
  proxy_->InsertFrame(std::move(frame));
  EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForKeyframe), Frame(WithId(0)));

  StartNextDecode();
  time_controller_.AdvanceTime(TimeDelta::Millis(50));

  // Timeouts should now happen at the normal frequency.
  const int expected_timeouts = 5;
  for (int i = 0; i < expected_timeouts; ++i) {
    EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForFrame), TimedOut());
    StartNextDecode();
  }
}

TEST_P(FrameBufferProxyTest, DependantFramesAreScheduled) {
  StartNextDecodeForceKeyframe();
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(WithId(0)));

  StartNextDecode();

  time_controller_.AdvanceTime(kFps30Delay);
  proxy_->InsertFrame(
      Builder().Id(1).Time(kFps30Rtp).AsLast().Refs({0}).Build());
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(WithId(1)));
}

TEST_P(FrameBufferProxyTest, SpatialLayersAreScheduled) {
  StartNextDecodeForceKeyframe();
  proxy_->InsertFrame(Builder().Id(0).SpatialLayer(0).Time(0).Build());
  proxy_->InsertFrame(Builder().Id(1).SpatialLayer(1).Time(0).Build());
  proxy_->InsertFrame(Builder().Id(2).SpatialLayer(2).Time(0).AsLast().Build());
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()),
              Frame(AllOf(WithId(0), FrameWithSize(3 * kFrameSize))));

  proxy_->InsertFrame(Builder().Id(3).Time(kFps30Rtp).SpatialLayer(0).Build());
  proxy_->InsertFrame(Builder().Id(4).Time(kFps30Rtp).SpatialLayer(1).Build());
  proxy_->InsertFrame(
      Builder().Id(5).Time(kFps30Rtp).SpatialLayer(2).AsLast().Build());

  StartNextDecode();
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay * 10),
              Frame(AllOf(WithId(3), FrameWithSize(3 * kFrameSize))));
}

TEST_P(FrameBufferProxyTest, OutstandingFrameTasksAreCancelledAfterDeletion) {
  StartNextDecodeForceKeyframe();
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  // Get keyframe. Delta frame should now be scheduled.
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(WithId(0)));

  StartNextDecode();
  proxy_->InsertFrame(
      Builder().Id(1).Time(kFps30Rtp).AsLast().Refs({0}).Build());
  proxy_->StopOnWorker();
  // Wait for 2x max wait time. Since we stopped, this should cause no timeouts
  // or frame-ready callbacks.
  EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForFrame * 2), Eq(absl::nullopt));
}

TEST_P(FrameBufferProxyTest, FramesWaitForDecoderToComplete) {
  StartNextDecodeForceKeyframe();

  // Start with a keyframe.
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(WithId(0)));

  ResetLastResult();
  // Insert a delta frame.
  proxy_->InsertFrame(
      Builder().Id(1).Time(kFps30Rtp).AsLast().Refs({0}).Build());

  // Advancing time should not result in a frame since the scheduler has not
  // been signalled that we are ready.
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Eq(absl::nullopt));
  // Signal ready.
  StartNextDecode();
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(WithId(1)));
}

TEST_P(FrameBufferProxyTest, LateFrameDropped) {
  StartNextDecodeForceKeyframe();
  //   F1
  //   /
  // F0 --> F2
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  // Start with a keyframe.
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(WithId(0)));

  StartNextDecode();

  // Simulate late F1 which arrives after F2.
  time_controller_.AdvanceTime(kFps30Delay * 2);
  proxy_->InsertFrame(
      Builder().Id(2).Time(2 * kFps30Rtp).AsLast().Refs({0}).Build());
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(WithId(2)));

  StartNextDecode();

  proxy_->InsertFrame(
      Builder().Id(1).Time(1 * kFps30Rtp).AsLast().Refs({0}).Build());
  // Confirm frame 1 is never scheduled by timing out.
  EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForFrame), TimedOut());
}

TEST_P(FrameBufferProxyTest, FramesFastForwardOnSystemHalt) {
  StartNextDecodeForceKeyframe();
  //   F1
  //   /
  // F0 --> F2
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());

  // Start with a keyframe.
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(WithId(0)));

  time_controller_.AdvanceTime(kFps30Delay);
  proxy_->InsertFrame(
      Builder().Id(1).Time(kFps30Rtp).AsLast().Refs({0}).Build());
  time_controller_.AdvanceTime(kFps30Delay);
  proxy_->InsertFrame(
      Builder().Id(2).Time(2 * kFps30Rtp).AsLast().Refs({0}).Build());

  // Halting time should result in F1 being skipped.
  time_controller_.AdvanceTime(kFps30Delay * 2);
  StartNextDecode();
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(WithId(2)));
  EXPECT_EQ(dropped_frames(), 1);
}

TEST_P(FrameBufferProxyTest, ForceKeyFrame) {
  StartNextDecodeForceKeyframe();
  // Initial keyframe.
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(WithId(0)));

  StartNextDecodeForceKeyframe();

  // F2 is the next keyframe, and should be extracted since a keyframe was
  // forced.
  proxy_->InsertFrame(
      Builder().Id(1).Time(kFps30Rtp).AsLast().Refs({0}).Build());
  proxy_->InsertFrame(Builder().Id(2).Time(kFps30Rtp * 2).AsLast().Build());

  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay * 3), Frame(WithId(2)));
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
  // Don't start next decode until slow delay.
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(WithId(0)));

  time_controller_.AdvanceTime(kFps30Delay);
  proxy_->InsertFrame(
      Builder().Id(1).Time(1 * kFps30Rtp).Refs({0}).AsLast().Build());
  time_controller_.AdvanceTime(kFps30Delay);
  proxy_->InsertFrame(
      Builder().Id(2).Time(2 * kFps30Rtp).Refs({0}).AsLast().Build());

  // Simulate decode taking 3x FPS rate.
  time_controller_.AdvanceTime(kFps30Delay * 1.5);
  StartNextDecode();
  // F2 is the best frame since decoding was so slow that F1 is too old.
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay * 2), Frame(WithId(2)));
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
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(WithId(4)));

  proxy_->InsertFrame(
      Builder().Id(5).Time(5 * kFps30Rtp).Refs({3, 4}).AsLast().Build());
  time_controller_.AdvanceTime(kFps30Delay / 2);

  // F5 is not decodable since F4 was decoded, so a timeout is expected.
  time_controller_.AdvanceTime(TimeDelta::Millis(10));
  StartNextDecode();
  EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForFrame), TimedOut());
  // TODO(bugs.webrtc.org/13343): This should be 2 dropped frames since frames 1
  // and 3 were dropped. However, frame_buffer2 does not mark frame 3 as dropped
  // which is a bug. Uncomment below when that is fixed for frame_buffer2 is
  // deleted.
  // EXPECT_EQ(dropped_frames(), 2);
}

TEST_P(FrameBufferProxyTest, NewFrameInsertedWhileWaitingToReleaseFrame) {
  StartNextDecodeForceKeyframe();
  // Initial keyframe.
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(WithId(0)));

  time_controller_.AdvanceTime(kFps30Delay / 2);
  proxy_->InsertFrame(
      Builder().Id(1).Time(kFps30Rtp).Refs({0}).AsLast().Build());
  StartNextDecode();
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Eq(absl::nullopt));

  // Scheduler is waiting to deliver Frame 1 now. Insert Frame 2. Frame 1 should
  // be delivered still.
  proxy_->InsertFrame(
      Builder().Id(2).Time(kFps30Rtp * 2).Refs({0}).AsLast().Build());
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(WithId(1)));
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
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Millis(15)), Frame(WithId(0)));

  StartNextDecode();

  // Warmup VCMTiming for 30fps.
  for (int i = 1; i <= 30; ++i) {
    proxy_->InsertFrame(Builder().Id(i).Time(i * kFps30Rtp).AsLast().Build());
    EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(WithId(i)));
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

  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(WithId(32)));
  StartNextDecode();

  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(WithId(33)));
  StartNextDecode();
  EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForFrame), TimedOut());
  EXPECT_EQ(dropped_frames(), 1);
}

TEST_P(FrameBufferProxyTest, TestStatsCallback) {
  EXPECT_CALL(stats_callback_,
              OnCompleteFrame(true, kFrameSize, VideoContentType::UNSPECIFIED));
  EXPECT_CALL(stats_callback_, OnFrameBufferTimingsUpdated);

  // Fake timing having received decoded frame.
  timing_.StopDecodeTimer(TimeDelta::Millis(1), clock_->CurrentTime());
  StartNextDecodeForceKeyframe();
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(WithId(0)));

  // Flush stats posted on the decode queue.
  time_controller_.AdvanceTime(TimeDelta::Zero());
}

TEST_P(FrameBufferProxyTest, FrameCompleteCalledOnceForDuplicateFrame) {
  EXPECT_CALL(stats_callback_,
              OnCompleteFrame(true, kFrameSize, VideoContentType::UNSPECIFIED))
      .Times(1);

  StartNextDecodeForceKeyframe();
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  proxy_->InsertFrame(Builder().Id(0).Time(0).AsLast().Build());
  // Flush stats posted on the decode queue.
  time_controller_.AdvanceTime(TimeDelta::Zero());
}

TEST_P(FrameBufferProxyTest, FrameCompleteCalledOnceForSingleTemporalUnit) {
  StartNextDecodeForceKeyframe();

  // `OnCompleteFrame` should not be called for the first two frames since they
  // do not complete the temporal layer.
  EXPECT_CALL(stats_callback_, OnCompleteFrame(_, _, _)).Times(0);
  proxy_->InsertFrame(Builder().Id(0).Time(0).Build());
  proxy_->InsertFrame(Builder().Id(1).Time(0).Refs({0}).Build());
  time_controller_.AdvanceTime(TimeDelta::Zero());
  // Flush stats posted on the decode queue.
  ::testing::Mock::VerifyAndClearExpectations(&stats_callback_);

  // Note that this frame is not marked as a keyframe since the last spatial
  // layer has dependencies.
  EXPECT_CALL(stats_callback_,
              OnCompleteFrame(false, kFrameSize, VideoContentType::UNSPECIFIED))
      .Times(1);
  proxy_->InsertFrame(Builder().Id(2).Time(0).Refs({0, 1}).AsLast().Build());
  // Flush stats posted on the decode queue.
  time_controller_.AdvanceTime(TimeDelta::Zero());
}

TEST_P(FrameBufferProxyTest, FrameCompleteCalledOnceForCompleteTemporalUnit) {
  // FrameBuffer2 logs the complete frame on the arrival of the last layer.
  if (IsFrameBuffer2Enabled(field_trials_))
    return;
  StartNextDecodeForceKeyframe();

  // `OnCompleteFrame` should not be called for the first two frames since they
  // do not complete the temporal layer. Frame 1 arrives later, at which time
  // this frame can finally be considered complete.
  EXPECT_CALL(stats_callback_, OnCompleteFrame(_, _, _)).Times(0);
  proxy_->InsertFrame(Builder().Id(0).Time(0).Build());
  proxy_->InsertFrame(Builder().Id(2).Time(0).Refs({0, 1}).AsLast().Build());
  time_controller_.AdvanceTime(TimeDelta::Zero());
  // Flush stats posted on the decode queue.
  ::testing::Mock::VerifyAndClearExpectations(&stats_callback_);

  EXPECT_CALL(stats_callback_,
              OnCompleteFrame(false, kFrameSize, VideoContentType::UNSPECIFIED))
      .Times(1);
  proxy_->InsertFrame(Builder().Id(1).Time(0).Refs({0}).Build());
  // Flush stats posted on the decode queue.
  time_controller_.AdvanceTime(TimeDelta::Zero());
}

// Note: This test takes a long time to run if the fake metronome is active.
// Since the test needs to wait for the timestamp to rollover, it has a fake
// delay of around 6.5 hours. Even though time is simulated, this will be
// around 1,500,000 metronome tick invocations.
TEST_P(FrameBufferProxyTest, NextFrameWithOldTimestamp) {
  // Test inserting 31 frames and pause the stream for a long time before
  // frame 32.
  StartNextDecodeForceKeyframe();
  constexpr uint32_t kBaseRtp = std::numeric_limits<uint32_t>::max() / 2;

  // First keyframe. The receive time must be explicitly set in this test since
  // the RTP derived time used in all tests does not work when the long pause
  // happens later in the test.
  proxy_->InsertFrame(Builder()
                          .Id(0)
                          .Time(kBaseRtp)
                          .ReceivedTime(clock_->CurrentTime())
                          .AsLast()
                          .Build());
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(WithId(0)));

  // 1 more frame to warmup VCMTiming for 30fps.
  StartNextDecode();
  proxy_->InsertFrame(Builder()
                          .Id(1)
                          .Time(kBaseRtp + kFps30Rtp)
                          .ReceivedTime(clock_->CurrentTime())
                          .AsLast()
                          .Build());
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(WithId(1)));

  // Pause the stream for such a long time it incurs an RTP timestamp rollover
  // by over half.
  constexpr uint32_t kLastRtp = kBaseRtp + kFps30Rtp;
  constexpr uint32_t kRolloverRtp =
      kLastRtp + std::numeric_limits<uint32_t>::max() / 2 + 1;
  constexpr Frequency kRtpHz = Frequency::KiloHertz(90);
  // Pause for corresponding delay such that RTP timestamp would increase this
  // much at 30fps.
  constexpr TimeDelta kRolloverDelay =
      (std::numeric_limits<uint32_t>::max() / 2 + 1) / kRtpHz;

  // Avoid timeout being set while waiting for the frame and before the receiver
  // is ready.
  ResetLastResult();
  EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForFrame), Eq(absl::nullopt));
  time_controller_.AdvanceTime(kRolloverDelay - kMaxWaitForFrame);
  StartNextDecode();
  proxy_->InsertFrame(Builder()
                          .Id(2)
                          .Time(kRolloverRtp)
                          .ReceivedTime(clock_->CurrentTime())
                          .AsLast()
                          .Build());
  // FrameBuffer2 drops the frame, while FrameBuffer3 will continue the stream.
  if (!IsFrameBuffer2Enabled(field_trials_)) {
    EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(WithId(2)));
  } else {
    EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForFrame), TimedOut());
  }
}

INSTANTIATE_TEST_SUITE_P(
    FrameBufferProxy,
    FrameBufferProxyTest,
    ::testing::Values("WebRTC-FrameBuffer3/arm:FrameBuffer2/",
                      "WebRTC-FrameBuffer3/arm:FrameBuffer3/",
                      "WebRTC-FrameBuffer3/arm:SyncDecoding/"));

class LowLatencyFrameBufferProxyTest : public ::testing::Test,
                                       public FrameBufferProxyFixture {};

TEST_P(LowLatencyFrameBufferProxyTest,
       FramesDecodedInstantlyWithLowLatencyRendering) {
  // Initial keyframe.
  StartNextDecodeForceKeyframe();
  timing_.set_min_playout_delay(TimeDelta::Zero());
  timing_.set_max_playout_delay(TimeDelta::Millis(10));
  auto frame = Builder().Id(0).Time(0).AsLast().Build();
  // Playout delay of 0 implies low-latency rendering.
  frame->SetPlayoutDelay({0, 10});
  proxy_->InsertFrame(std::move(frame));
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(WithId(0)));

  // Delta frame would normally wait here, but should decode at the pacing rate
  // in low-latency mode.
  StartNextDecode();
  frame = Builder().Id(1).Time(kFps30Rtp).AsLast().Build();
  frame->SetPlayoutDelay({0, 10});
  proxy_->InsertFrame(std::move(frame));
  // Pacing is set to 16ms in the field trial so we should not decode yet.
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Eq(absl::nullopt));
  time_controller_.AdvanceTime(TimeDelta::Millis(16));
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(WithId(1)));
}

TEST_P(LowLatencyFrameBufferProxyTest, ZeroPlayoutDelayFullQueue) {
  // Initial keyframe.
  StartNextDecodeForceKeyframe();
  timing_.set_min_playout_delay(TimeDelta::Zero());
  timing_.set_max_playout_delay(TimeDelta::Millis(10));
  auto frame = Builder().Id(0).Time(0).AsLast().Build();
  // Playout delay of 0 implies low-latency rendering.
  frame->SetPlayoutDelay({0, 10});
  proxy_->InsertFrame(std::move(frame));
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(WithId(0)));

  // Queue up 5 frames (configured max queue size for 0-playout delay pacing).
  for (int id = 1; id <= 6; ++id) {
    frame = Builder().Id(id).Time(kFps30Rtp * id).AsLast().Build();
    frame->SetPlayoutDelay({0, 10});
    proxy_->InsertFrame(std::move(frame));
  }

  // The queue is at its max size for zero playout delay pacing, so the pacing
  // should be ignored and the next frame should be decoded instantly.
  StartNextDecode();
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(WithId(1)));
}

TEST_P(LowLatencyFrameBufferProxyTest, MinMaxDelayZeroLowLatencyMode) {
  // Initial keyframe.
  StartNextDecodeForceKeyframe();
  timing_.set_min_playout_delay(TimeDelta::Zero());
  timing_.set_max_playout_delay(TimeDelta::Zero());
  auto frame = Builder().Id(0).Time(0).AsLast().Build();
  // Playout delay of 0 implies low-latency rendering.
  frame->SetPlayoutDelay({0, 0});
  proxy_->InsertFrame(std::move(frame));
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(WithId(0)));

  // Delta frame would normally wait here, but should decode at the pacing rate
  // in low-latency mode.
  StartNextDecode();
  frame = Builder().Id(1).Time(kFps30Rtp).AsLast().Build();
  frame->SetPlayoutDelay({0, 0});
  proxy_->InsertFrame(std::move(frame));
  // The min/max=0 version of low-latency rendering will result in a large
  // negative decode wait time, so the frame should be ready right away.
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(WithId(1)));
}

INSTANTIATE_TEST_SUITE_P(
    FrameBufferProxy,
    LowLatencyFrameBufferProxyTest,
    ::testing::Values(
        "WebRTC-FrameBuffer3/arm:FrameBuffer2/"
        "WebRTC-ZeroPlayoutDelay/min_pacing:16ms,max_decode_queue_size:5/",
        "WebRTC-FrameBuffer3/arm:FrameBuffer3/"
        "WebRTC-ZeroPlayoutDelay/min_pacing:16ms,max_decode_queue_size:5/",
        "WebRTC-FrameBuffer3/arm:SyncDecoding/"
        "WebRTC-ZeroPlayoutDelay/min_pacing:16ms,max_decode_queue_size:5/"));

}  // namespace webrtc
