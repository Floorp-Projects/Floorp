/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/video_codec_stats_impl.h"

#include "absl/types/optional.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {
using ::testing::Return;
using ::testing::Values;
}  // namespace

TEST(VideoCodecStatsImpl, AddFrame) {
  VideoCodecStatsImpl stats;
  VideoCodecStatsImpl::Frame* fs =
      stats.AddFrame(/*frame_num=*/0, /*timestamp_rtp=*/0, /*spatial_idx=*/0);
  EXPECT_NE(nullptr, fs);
  fs = stats.GetFrame(/*timestamp_rtp=*/0, /*spatial_idx=*/0);
  EXPECT_NE(nullptr, fs);
}

TEST(VideoCodecStatsImpl, GetFrame) {
  VideoCodecStatsImpl stats;
  stats.AddFrame(/*frame_num=*/0, /*timestamp_rtp=*/0, /*spatial_idx=*/0);
  VideoCodecStatsImpl::Frame* fs =
      stats.GetFrame(/*timestamp_rtp=*/0, /*spatial_idx=*/0);
  EXPECT_NE(nullptr, fs);
}

struct VideoCodecStatsSlicingTestParams {
  VideoCodecStats::Filter slicer;
  std::vector<VideoCodecStats::Frame> expected;
};

class VideoCodecStatsSlicingTest
    : public ::testing::TestWithParam<VideoCodecStatsSlicingTestParams> {
 public:
  void SetUp() {
    // TODO(ssikin): Hard codec 2x2 table would be better.
    for (int frame_num = 0; frame_num < 2; ++frame_num) {
      for (int spatial_idx = 0; spatial_idx < 2; ++spatial_idx) {
        uint32_t timestamp_rtp = 3000 * frame_num;
        VideoCodecStats::Frame* f =
            stats_.AddFrame(frame_num, timestamp_rtp, spatial_idx);
        f->temporal_idx = frame_num;
      }
    }
  }

 protected:
  VideoCodecStatsImpl stats_;
};

TEST_P(VideoCodecStatsSlicingTest, Slice) {
  VideoCodecStatsSlicingTestParams test_params = GetParam();
  std::vector<VideoCodecStats::Frame> frames = stats_.Slice(test_params.slicer);
  EXPECT_EQ(frames.size(), test_params.expected.size());
}

INSTANTIATE_TEST_SUITE_P(All,
                         VideoCodecStatsSlicingTest,
                         ::testing::ValuesIn({VideoCodecStatsSlicingTestParams(
                             {.slicer = {.first_frame = 0, .last_frame = 1},
                              .expected = {{.frame_num = 0},
                                           {.frame_num = 1},
                                           {.frame_num = 0},
                                           {.frame_num = 1}}})}));

struct VideoCodecStatsAggregationTestParams {
  VideoCodecStats::Filter slicer;
  struct Expected {
    double decode_time_us;
  } expected;
};

class VideoCodecStatsAggregationTest
    : public ::testing::TestWithParam<VideoCodecStatsAggregationTestParams> {
 public:
  void SetUp() {
    // TODO(ssikin): Hard codec 2x2 table would be better. Share with
    // VideoCodecStatsSlicingTest
    for (int frame_num = 0; frame_num < 2; ++frame_num) {
      for (int spatial_idx = 0; spatial_idx < 2; ++spatial_idx) {
        uint32_t timestamp_rtp = 3000 * frame_num;
        VideoCodecStats::Frame* f =
            stats_.AddFrame(frame_num, timestamp_rtp, spatial_idx);
        f->temporal_idx = frame_num;
        f->decode_time = TimeDelta::Micros(spatial_idx * 10 + frame_num);
        f->encoded = true;
        f->decoded = true;
      }
    }
  }

 protected:
  VideoCodecStatsImpl stats_;
};

TEST_P(VideoCodecStatsAggregationTest, Aggregate) {
  VideoCodecStatsAggregationTestParams test_params = GetParam();
  std::vector<VideoCodecStats::Frame> frames = stats_.Slice(test_params.slicer);
  VideoCodecStats::Stream stream = stats_.Aggregate(frames);
  EXPECT_EQ(stream.decode_time_us.GetAverage(),
            test_params.expected.decode_time_us);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    VideoCodecStatsAggregationTest,
    ::testing::ValuesIn(
        {VideoCodecStatsAggregationTestParams(
             {.slicer = {},
              .expected = {.decode_time_us = (0.0 + 1.0 + 10.0 + 11.0) / 2}}),
         // Slicing on frame number
         VideoCodecStatsAggregationTestParams(
             {.slicer = {.first_frame = 1, .last_frame = 1},
              .expected = {.decode_time_us = 1.0 + 11.0}}),
         // Slice on spatial index
         VideoCodecStatsAggregationTestParams(
             {.slicer = {.spatial_idx = 1},
              .expected = {.decode_time_us = (10.0 + 11.0) / 2}}),
         // Slice on temporal index
         VideoCodecStatsAggregationTestParams(
             {.slicer = {.temporal_idx = 0},
              .expected = {.decode_time_us = 0.0 + 10.0}})}));

}  // namespace test
}  // namespace webrtc
