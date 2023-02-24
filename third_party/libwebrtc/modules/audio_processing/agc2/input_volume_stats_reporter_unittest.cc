/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/input_volume_stats_reporter.h"

#include "system_wrappers/include/metrics.h"
#include "test/gmock.h"

namespace webrtc {
namespace {

constexpr int kFramesIn60Seconds = 6000;

class InputVolumeStatsReporterTest : public ::testing::Test {
 public:
  InputVolumeStatsReporterTest() {}

 protected:
  void SetUp() override { metrics::Reset(); }
};

TEST_F(InputVolumeStatsReporterTest, CheckLogVolumeUpdateStatsEmpty) {
  InputVolumeStatsReporter stats_reporter;
  constexpr int kInputVolume = 10;
  stats_reporter.UpdateStatistics(kInputVolume);
  // Update almost until the periodic logging and reset.
  for (int i = 0; i < kFramesIn60Seconds - 2; i += 2) {
    stats_reporter.UpdateStatistics(kInputVolume + 2);
    stats_reporter.UpdateStatistics(kInputVolume);
  }
  EXPECT_METRIC_THAT(metrics::Samples("WebRTC.Audio.ApmAnalogGainUpdateRate"),
                     ::testing::ElementsAre());
  EXPECT_METRIC_THAT(metrics::Samples("WebRTC.Audio.ApmAnalogGainDecreaseRate"),
                     ::testing::ElementsAre());
  EXPECT_METRIC_THAT(metrics::Samples("WebRTC.Audio.ApmAnalogGainIncreaseRate"),
                     ::testing::ElementsAre());
  EXPECT_METRIC_THAT(
      metrics::Samples("WebRTC.Audio.ApmAnalogGainUpdateAverage"),
      ::testing::ElementsAre());
  EXPECT_METRIC_THAT(
      metrics::Samples("WebRTC.Audio.ApmAnalogGainDecreaseAverage"),
      ::testing::ElementsAre());
  EXPECT_METRIC_THAT(
      metrics::Samples("WebRTC.Audio.ApmAnalogGainIncreaseAverage"),
      ::testing::ElementsAre());
}

TEST_F(InputVolumeStatsReporterTest, CheckLogVolumeUpdateStatsNotEmpty) {
  InputVolumeStatsReporter stats_reporter;
  constexpr int kInputVolume = 10;
  stats_reporter.UpdateStatistics(kInputVolume);
  // Update until periodic logging.
  for (int i = 0; i < kFramesIn60Seconds; i += 2) {
    stats_reporter.UpdateStatistics(kInputVolume + 2);
    stats_reporter.UpdateStatistics(kInputVolume);
  }
  // Update until periodic logging.
  for (int i = 0; i < kFramesIn60Seconds; i += 2) {
    stats_reporter.UpdateStatistics(kInputVolume + 3);
    stats_reporter.UpdateStatistics(kInputVolume);
  }
  EXPECT_METRIC_THAT(
      metrics::Samples("WebRTC.Audio.ApmAnalogGainUpdateRate"),
      ::testing::ElementsAre(::testing::Pair(kFramesIn60Seconds - 1, 1),
                             ::testing::Pair(kFramesIn60Seconds, 1)));
  EXPECT_METRIC_THAT(
      metrics::Samples("WebRTC.Audio.ApmAnalogGainDecreaseRate"),
      ::testing::ElementsAre(::testing::Pair(kFramesIn60Seconds / 2 - 1, 1),
                             ::testing::Pair(kFramesIn60Seconds / 2, 1)));
  EXPECT_METRIC_THAT(
      metrics::Samples("WebRTC.Audio.ApmAnalogGainIncreaseRate"),
      ::testing::ElementsAre(::testing::Pair(kFramesIn60Seconds / 2, 2)));
  EXPECT_METRIC_THAT(
      metrics::Samples("WebRTC.Audio.ApmAnalogGainUpdateAverage"),
      ::testing::ElementsAre(::testing::Pair(2, 1), ::testing::Pair(3, 1)));
  EXPECT_METRIC_THAT(
      metrics::Samples("WebRTC.Audio.ApmAnalogGainDecreaseAverage"),
      ::testing::ElementsAre(::testing::Pair(2, 1), ::testing::Pair(3, 1)));
  EXPECT_METRIC_THAT(
      metrics::Samples("WebRTC.Audio.ApmAnalogGainIncreaseAverage"),
      ::testing::ElementsAre(::testing::Pair(2, 1), ::testing::Pair(3, 1)));
}
}  // namespace

TEST_F(InputVolumeStatsReporterTest, CheckVolumeUpdateStatsForEmptyStats) {
  InputVolumeStatsReporter stats_reporter;
  const auto& update_stats = stats_reporter.volume_update_stats();
  EXPECT_EQ(update_stats.num_decreases, 0);
  EXPECT_EQ(update_stats.sum_decreases, 0);
  EXPECT_EQ(update_stats.num_increases, 0);
  EXPECT_EQ(update_stats.sum_increases, 0);
}

TEST_F(InputVolumeStatsReporterTest,
       CheckVolumeUpdateStatsAfterNoVolumeChange) {
  constexpr int kInputVolume = 10;
  InputVolumeStatsReporter stats_reporter;
  stats_reporter.UpdateStatistics(kInputVolume);
  stats_reporter.UpdateStatistics(kInputVolume);
  stats_reporter.UpdateStatistics(kInputVolume);
  const auto& update_stats = stats_reporter.volume_update_stats();
  EXPECT_EQ(update_stats.num_decreases, 0);
  EXPECT_EQ(update_stats.sum_decreases, 0);
  EXPECT_EQ(update_stats.num_increases, 0);
  EXPECT_EQ(update_stats.sum_increases, 0);
}

TEST_F(InputVolumeStatsReporterTest,
       CheckVolumeUpdateStatsAfterVolumeIncrease) {
  constexpr int kInputVolume = 10;
  InputVolumeStatsReporter stats_reporter;
  stats_reporter.UpdateStatistics(kInputVolume);
  stats_reporter.UpdateStatistics(kInputVolume + 4);
  stats_reporter.UpdateStatistics(kInputVolume + 5);
  const auto& update_stats = stats_reporter.volume_update_stats();
  EXPECT_EQ(update_stats.num_decreases, 0);
  EXPECT_EQ(update_stats.sum_decreases, 0);
  EXPECT_EQ(update_stats.num_increases, 2);
  EXPECT_EQ(update_stats.sum_increases, 5);
}

TEST_F(InputVolumeStatsReporterTest,
       CheckVolumeUpdateStatsAfterVolumeDecrease) {
  constexpr int kInputVolume = 10;
  InputVolumeStatsReporter stats_reporter;
  stats_reporter.UpdateStatistics(kInputVolume);
  stats_reporter.UpdateStatistics(kInputVolume - 4);
  stats_reporter.UpdateStatistics(kInputVolume - 5);
  const auto& stats_update = stats_reporter.volume_update_stats();
  EXPECT_EQ(stats_update.num_decreases, 2);
  EXPECT_EQ(stats_update.sum_decreases, 5);
  EXPECT_EQ(stats_update.num_increases, 0);
  EXPECT_EQ(stats_update.sum_increases, 0);
}

TEST_F(InputVolumeStatsReporterTest, CheckVolumeUpdateStatsAfterReset) {
  InputVolumeStatsReporter stats_reporter;
  constexpr int kInputVolume = 10;
  stats_reporter.UpdateStatistics(kInputVolume);
  // Update until the periodic reset.
  for (int i = 0; i < kFramesIn60Seconds - 2; i += 2) {
    stats_reporter.UpdateStatistics(kInputVolume + 2);
    stats_reporter.UpdateStatistics(kInputVolume);
  }
  const auto& stats_before_reset = stats_reporter.volume_update_stats();
  EXPECT_EQ(stats_before_reset.num_decreases, kFramesIn60Seconds / 2 - 1);
  EXPECT_EQ(stats_before_reset.sum_decreases, kFramesIn60Seconds - 2);
  EXPECT_EQ(stats_before_reset.num_increases, kFramesIn60Seconds / 2 - 1);
  EXPECT_EQ(stats_before_reset.sum_increases, kFramesIn60Seconds - 2);
  stats_reporter.UpdateStatistics(kInputVolume + 2);
  const auto& stats_during_reset = stats_reporter.volume_update_stats();
  EXPECT_EQ(stats_during_reset.num_decreases, 0);
  EXPECT_EQ(stats_during_reset.sum_decreases, 0);
  EXPECT_EQ(stats_during_reset.num_increases, 0);
  EXPECT_EQ(stats_during_reset.sum_increases, 0);
  stats_reporter.UpdateStatistics(kInputVolume);
  stats_reporter.UpdateStatistics(kInputVolume + 3);
  const auto& stats_after_reset = stats_reporter.volume_update_stats();
  EXPECT_EQ(stats_after_reset.num_decreases, 1);
  EXPECT_EQ(stats_after_reset.sum_decreases, 2);
  EXPECT_EQ(stats_after_reset.num_increases, 1);
  EXPECT_EQ(stats_after_reset.sum_increases, 3);
}

}  // namespace webrtc
