/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_frames_comparator.h"

#include <map>

#include "api/test/create_frame_generator.h"
#include "api/units/timestamp.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_cpu_measurer.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_shared_objects.h"

namespace webrtc {
namespace {

using StatsSample = ::webrtc::SamplesStatsCounter::StatsSample;

constexpr int kMaxFramesInFlightPerStream = 10;

webrtc_pc_e2e::DefaultVideoQualityAnalyzerOptions AnalyzerOptionsForTest() {
  webrtc_pc_e2e::DefaultVideoQualityAnalyzerOptions options;
  options.heavy_metrics_computation_enabled = false;
  options.adjust_cropping_before_comparing_frames = false;
  options.max_frames_in_flight_per_stream_count = kMaxFramesInFlightPerStream;
  return options;
}

webrtc_pc_e2e::StreamCodecInfo Vp8CodecForOneFrame(uint16_t frame_id,
                                                   Timestamp time) {
  webrtc_pc_e2e::StreamCodecInfo info;
  info.codec_name = "VP8";
  info.first_frame_id = frame_id;
  info.last_frame_id = frame_id;
  info.switched_on_at = time;
  info.switched_from_at = time;
  return info;
}

FrameStats FrameStatsWith10msDeltaBetweenPhasesAnd10x10Frame(
    Timestamp captured_time) {
  FrameStats frame_stats(captured_time);
  frame_stats.pre_encode_time = captured_time + TimeDelta::Millis(10);
  frame_stats.encoded_time = captured_time + TimeDelta::Millis(20);
  frame_stats.received_time = captured_time + TimeDelta::Millis(30);
  frame_stats.decode_start_time = captured_time + TimeDelta::Millis(40);
  frame_stats.decode_end_time = captured_time + TimeDelta::Millis(50);
  frame_stats.rendered_time = captured_time + TimeDelta::Millis(60);
  frame_stats.used_encoder = Vp8CodecForOneFrame(1, frame_stats.encoded_time);
  frame_stats.used_encoder =
      Vp8CodecForOneFrame(1, frame_stats.decode_end_time);
  frame_stats.rendered_frame_width = 10;
  frame_stats.rendered_frame_height = 10;
  return frame_stats;
}

double GetFirstOrDie(const SamplesStatsCounter& counter) {
  EXPECT_TRUE(!counter.IsEmpty()) << "Counter has to be not empty";
  return counter.GetSamples()[0];
}

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     StatsPresentedAfterAddingOneComparison) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer, AnalyzerOptionsForTest());

  Timestamp stream_start_time = Clock::GetRealTimeClock()->CurrentTime();
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  size_t peers_count = 2;
  InternalStatsKey stats_key(stream, sender, receiver);

  FrameStats frame_stats =
      FrameStatsWith10msDeltaBetweenPhasesAnd10x10Frame(stream_start_time);

  comparator.Start(1);
  comparator.EnsureStatsForStream(stream, sender, peers_count,
                                  stream_start_time, stream_start_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/absl::nullopt,
                           /*rendered=*/absl::nullopt, /*dropped=*/false,
                           frame_stats);
  comparator.Stop({});

  std::map<InternalStatsKey, webrtc_pc_e2e::StreamStats> stats =
      comparator.stream_stats();
  EXPECT_DOUBLE_EQ(GetFirstOrDie(stats.at(stats_key).transport_time_ms), 20.0);
  EXPECT_DOUBLE_EQ(
      GetFirstOrDie(stats.at(stats_key).total_delay_incl_transport_ms), 60.0);
  EXPECT_DOUBLE_EQ(GetFirstOrDie(stats.at(stats_key).encode_time_ms), 10.0);
  EXPECT_DOUBLE_EQ(GetFirstOrDie(stats.at(stats_key).decode_time_ms), 10.0);
  EXPECT_DOUBLE_EQ(GetFirstOrDie(stats.at(stats_key).receive_to_render_time_ms),
                   30.0);
  EXPECT_DOUBLE_EQ(
      GetFirstOrDie(stats.at(stats_key).resolution_of_rendered_frame), 100.0);
}

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     MultiFrameStatsPresentedAfterAddingTwoComparisonWith10msDelay) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer, AnalyzerOptionsForTest());

  Timestamp stream_start_time = Clock::GetRealTimeClock()->CurrentTime();
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  size_t peers_count = 2;
  InternalStatsKey stats_key(stream, sender, receiver);

  FrameStats frame_stats1 =
      FrameStatsWith10msDeltaBetweenPhasesAnd10x10Frame(stream_start_time);
  FrameStats frame_stats2 = FrameStatsWith10msDeltaBetweenPhasesAnd10x10Frame(
      stream_start_time + TimeDelta::Millis(15));
  frame_stats2.prev_frame_rendered_time = frame_stats1.rendered_time;

  comparator.Start(1);
  comparator.EnsureStatsForStream(stream, sender, peers_count,
                                  stream_start_time, stream_start_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/absl::nullopt,
                           /*rendered=*/absl::nullopt, /*dropped=*/false,
                           frame_stats1);
  comparator.AddComparison(stats_key,
                           /*captured=*/absl::nullopt,
                           /*rendered=*/absl::nullopt, /*dropped=*/false,
                           frame_stats2);
  comparator.Stop({});

  std::map<InternalStatsKey, webrtc_pc_e2e::StreamStats> stats =
      comparator.stream_stats();
  EXPECT_DOUBLE_EQ(
      GetFirstOrDie(stats.at(stats_key).time_between_rendered_frames_ms), 15.0);
  EXPECT_DOUBLE_EQ(stats.at(stats_key).encode_frame_rate.GetEventsPerSecond(),
                   2.0 / 15 * 1000);
}

}  // namespace
}  // namespace webrtc
