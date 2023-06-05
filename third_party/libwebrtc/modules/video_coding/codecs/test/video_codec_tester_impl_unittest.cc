/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/video_codec_tester_impl.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "api/task_queue/task_queue_factory.h"
#include "api/task_queue/test/mock_task_queue_base.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/gunit.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/time_utils.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {
using ::testing::_;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

using Decoder = VideoCodecTester::Decoder;
using Encoder = VideoCodecTester::Encoder;
using CodedVideoSource = VideoCodecTester::CodedVideoSource;
using RawVideoSource = VideoCodecTester::RawVideoSource;
using DecoderSettings = VideoCodecTester::DecoderSettings;
using EncoderSettings = VideoCodecTester::EncoderSettings;
using PacingSettings = VideoCodecTester::PacingSettings;
using PacingMode = PacingSettings::PacingMode;

constexpr Frequency k90kHz = Frequency::Hertz(90000);

VideoFrame CreateVideoFrame(uint32_t timestamp_rtp) {
  rtc::scoped_refptr<I420Buffer> buffer(I420Buffer::Create(2, 2));
  return VideoFrame::Builder()
      .set_video_frame_buffer(buffer)
      .set_timestamp_rtp(timestamp_rtp)
      .build();
}

EncodedImage CreateEncodedImage(uint32_t timestamp_rtp) {
  EncodedImage encoded_image;
  encoded_image.SetTimestamp(timestamp_rtp);
  return encoded_image;
}

class MockRawVideoSource : public RawVideoSource {
 public:
  MOCK_METHOD(absl::optional<VideoFrame>, PullFrame, (), (override));
  MOCK_METHOD(VideoFrame,
              GetFrame,
              (uint32_t timestamp_rtp, Resolution),
              (override));
};

class MockCodedVideoSource : public CodedVideoSource {
 public:
  MOCK_METHOD(absl::optional<EncodedImage>, PullFrame, (), (override));
};

class MockDecoder : public Decoder {
 public:
  MOCK_METHOD(void,
              Decode,
              (const EncodedImage& frame, DecodeCallback callback),
              (override));
};

class MockEncoder : public Encoder {
 public:
  MOCK_METHOD(void,
              Encode,
              (const VideoFrame& frame, EncodeCallback callback),
              (override));
};

class MockTaskQueueFactory : public TaskQueueFactory {
 public:
  explicit MockTaskQueueFactory(TaskQueueBase& task_queue)
      : task_queue_(task_queue) {}

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      absl::string_view name,
      Priority priority) const override {
    return std::unique_ptr<TaskQueueBase, TaskQueueDeleter>(&task_queue_);
  }

 protected:
  TaskQueueBase& task_queue_;
};
}  // namespace

class VideoCodecTesterImplPacingTest
    : public ::testing::TestWithParam<std::tuple<PacingSettings,
                                                 std::vector<int>,
                                                 std::vector<int>,
                                                 std::vector<int>>> {
 public:
  VideoCodecTesterImplPacingTest()
      : pacing_settings_(std::get<0>(GetParam())),
        frame_timestamp_ms_(std::get<1>(GetParam())),
        frame_capture_delay_ms_(std::get<2>(GetParam())),
        expected_frame_start_ms_(std::get<3>(GetParam())),
        num_frames_(frame_timestamp_ms_.size()),
        task_queue_factory_(task_queue_) {}

  void SetUp() override {
    ON_CALL(task_queue_, PostTask)
        .WillByDefault(Invoke(
            [](absl::AnyInvocable<void() &&> task) { std::move(task)(); }));

    ON_CALL(task_queue_, PostDelayedTask)
        .WillByDefault(
            Invoke([&](absl::AnyInvocable<void() &&> task, TimeDelta delay) {
              clock_.AdvanceTime(delay);
              std::move(task)();
            }));
  }

 protected:
  PacingSettings pacing_settings_;
  std::vector<int> frame_timestamp_ms_;
  std::vector<int> frame_capture_delay_ms_;
  std::vector<int> expected_frame_start_ms_;
  size_t num_frames_;

  rtc::ScopedFakeClock clock_;
  MockTaskQueueBase task_queue_;
  MockTaskQueueFactory task_queue_factory_;
};

TEST_P(VideoCodecTesterImplPacingTest, PaceEncode) {
  auto video_source = std::make_unique<MockRawVideoSource>();

  size_t frame_num = 0;
  EXPECT_CALL(*video_source, PullFrame).WillRepeatedly(Invoke([&]() mutable {
    if (frame_num >= num_frames_) {
      return absl::optional<VideoFrame>();
    }
    clock_.AdvanceTime(TimeDelta::Millis(frame_capture_delay_ms_[frame_num]));

    uint32_t timestamp_rtp = frame_timestamp_ms_[frame_num] * k90kHz.hertz() /
                             rtc::kNumMillisecsPerSec;
    ++frame_num;
    return absl::optional<VideoFrame>(CreateVideoFrame(timestamp_rtp));
  }));

  auto encoder = std::make_unique<MockEncoder>();
  EncoderSettings encoder_settings;
  encoder_settings.pacing = pacing_settings_;

  VideoCodecTesterImpl tester(&task_queue_factory_);
  auto fs = tester
                .RunEncodeTest(std::move(video_source), std::move(encoder),
                               encoder_settings)
                ->Slice();
  ASSERT_EQ(fs.size(), num_frames_);

  for (size_t i = 0; i < fs.size(); ++i) {
    int encode_start_ms = (fs[i].encode_start - fs[0].encode_start).ms();
    EXPECT_NEAR(encode_start_ms, expected_frame_start_ms_[i], 10);
  }
}

TEST_P(VideoCodecTesterImplPacingTest, PaceDecode) {
  auto video_source = std::make_unique<MockCodedVideoSource>();

  size_t frame_num = 0;
  EXPECT_CALL(*video_source, PullFrame).WillRepeatedly(Invoke([&]() mutable {
    if (frame_num >= num_frames_) {
      return absl::optional<EncodedImage>();
    }
    clock_.AdvanceTime(TimeDelta::Millis(frame_capture_delay_ms_[frame_num]));

    uint32_t timestamp_rtp = frame_timestamp_ms_[frame_num] * k90kHz.hertz() /
                             rtc::kNumMillisecsPerSec;
    ++frame_num;
    return absl::optional<EncodedImage>(CreateEncodedImage(timestamp_rtp));
  }));

  auto decoder = std::make_unique<MockDecoder>();
  DecoderSettings decoder_settings;
  decoder_settings.pacing = pacing_settings_;

  VideoCodecTesterImpl tester(&task_queue_factory_);
  auto fs = tester
                .RunDecodeTest(std::move(video_source), std::move(decoder),
                               decoder_settings)
                ->Slice();
  ASSERT_EQ(fs.size(), num_frames_);

  for (size_t i = 0; i < fs.size(); ++i) {
    int decode_start_ms = (fs[i].decode_start - fs[0].decode_start).ms();
    EXPECT_NEAR(decode_start_ms, expected_frame_start_ms_[i], 10);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    VideoCodecTesterImplPacingTest,
    ::testing::ValuesIn(
        {std::make_tuple(PacingSettings({.mode = PacingMode::kNoPacing}),
                         /*frame_timestamp_ms=*/std::vector<int>{0, 100},
                         /*frame_capture_delay_ms=*/std::vector<int>{0, 0},
                         /*expected_frame_start_ms=*/std::vector<int>{0, 0}),
         // Pace with rate equal to the source frame rate. Frames are captured
         // instantly. Verify that frames are paced with the source frame rate.
         std::make_tuple(PacingSettings({.mode = PacingMode::kRealTime}),
                         /*frame_timestamp_ms=*/std::vector<int>{0, 100},
                         /*frame_capture_delay_ms=*/std::vector<int>{0, 0},
                         /*expected_frame_start_ms=*/std::vector<int>{0, 100}),
         // Pace with rate equal to the source frame rate. Frame capture is
         // delayed by more than pacing time. Verify that no extra delay is
         // added.
         std::make_tuple(PacingSettings({.mode = PacingMode::kRealTime}),
                         /*frame_timestamp_ms=*/std::vector<int>{0, 100},
                         /*frame_capture_delay_ms=*/std::vector<int>{0, 200},
                         /*expected_frame_start_ms=*/std::vector<int>{0, 200}),
         // Pace with constant rate less then source frame rate. Frames are
         // captured instantly. Verify that frames are paced with the requested
         // constant rate.
         std::make_tuple(
             PacingSettings({.mode = PacingMode::kConstantRate,
                             .constant_rate = Frequency::Hertz(20)}),
             /*frame_timestamp_ms=*/std::vector<int>{0, 100},
             /*frame_capture_delay_ms=*/std::vector<int>{0, 0},
             /*expected_frame_start_ms=*/std::vector<int>{0, 50}),
         // Pace with constant rate less then source frame rate. Frame capture
         // is delayed by more than the pacing time. Verify that no extra delay
         // is added.
         std::make_tuple(
             PacingSettings({.mode = PacingMode::kConstantRate,
                             .constant_rate = Frequency::Hertz(20)}),
             /*frame_timestamp_ms=*/std::vector<int>{0, 100},
             /*frame_capture_delay_ms=*/std::vector<int>{0, 200},
             /*expected_frame_start_ms=*/std::vector<int>{0, 200})}));
}  // namespace test
}  // namespace webrtc
