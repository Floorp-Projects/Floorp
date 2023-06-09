/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_VIDEO_CODEC_STATS_H_
#define API_TEST_VIDEO_CODEC_STATS_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/test/metrics/metric.h"
#include "api/test/metrics/metrics_logger.h"
#include "api/units/data_rate.h"
#include "api/units/frequency.h"

namespace webrtc {
namespace test {

// Interface for encoded and/or decoded video frame and stream statistics.
class VideoCodecStats {
 public:
  // Filter for slicing frames.
  struct Filter {
    absl::optional<int> first_frame;
    absl::optional<int> last_frame;
    absl::optional<int> spatial_idx;
    absl::optional<int> temporal_idx;
  };

  struct Frame {
    int frame_num = 0;
    uint32_t timestamp_rtp = 0;

    int spatial_idx = 0;
    int temporal_idx = 0;

    int width = 0;
    int height = 0;
    int size_bytes = 0;
    bool keyframe = false;
    absl::optional<int> qp = absl::nullopt;
    absl::optional<int> base_spatial_idx = absl::nullopt;

    Timestamp encode_start = Timestamp::Zero();
    TimeDelta encode_time = TimeDelta::Zero();
    Timestamp decode_start = Timestamp::Zero();
    TimeDelta decode_time = TimeDelta::Zero();

    struct Psnr {
      double y = 0.0;
      double u = 0.0;
      double v = 0.0;
    };
    absl::optional<Psnr> psnr = absl::nullopt;

    bool encoded = false;
    bool decoded = false;
  };

  struct Stream {
    int num_frames = 0;
    int num_keyframes = 0;

    SamplesStatsCounter width;
    SamplesStatsCounter height;
    SamplesStatsCounter size_bytes;
    SamplesStatsCounter qp;

    SamplesStatsCounter encode_time_us;
    SamplesStatsCounter decode_time_us;

    DataRate bitrate = DataRate::Zero();
    Frequency framerate = Frequency::Zero();
    int bitrate_mismatch_pct = 0;
    int framerate_mismatch_pct = 0;
    SamplesStatsCounter transmission_time_us;

    struct Psnr {
      SamplesStatsCounter y;
      SamplesStatsCounter u;
      SamplesStatsCounter v;
    } psnr;
  };

  virtual ~VideoCodecStats() = default;

  // Returns frames from interval, spatial and temporal layer specified by given
  // `filter`.
  virtual std::vector<Frame> Slice(
      absl::optional<Filter> filter = absl::nullopt) const = 0;

  // Returns video statistics aggregated for given `frames`. If `bitrate` is
  // provided, also performs rate control analysis. If `framerate` is provided,
  // also calculates frame rate mismatch.
  virtual Stream Aggregate(
      const std::vector<Frame>& frames,
      absl::optional<DataRate> bitrate = absl::nullopt,
      absl::optional<Frequency> framerate = absl::nullopt) const = 0;

  // Logs `Stream` metrics to provided `MetricsLogger`.
  virtual void LogMetrics(MetricsLogger* logger,
                          const Stream& stream,
                          std::string test_case_name) const = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_VIDEO_CODEC_STATS_H_
