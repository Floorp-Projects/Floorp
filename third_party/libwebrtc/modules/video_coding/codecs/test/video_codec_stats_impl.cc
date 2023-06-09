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

#include <algorithm>

#include "api/numerics/samples_stats_counter.h"
#include "api/test/metrics/global_metrics_logger_and_exporter.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {
namespace {
using Frame = VideoCodecStats::Frame;
using Stream = VideoCodecStats::Stream;

constexpr Frequency k90kHz = Frequency::Hertz(90000);
}  // namespace

std::vector<Frame> VideoCodecStatsImpl::Slice(
    absl::optional<Filter> filter) const {
  std::vector<Frame> frames;
  for (const auto& [frame_id, f] : frames_) {
    if (filter.has_value()) {
      if (filter->first_frame.has_value() &&
          f.frame_num < *filter->first_frame) {
        continue;
      }
      if (filter->last_frame.has_value() && f.frame_num > *filter->last_frame) {
        continue;
      }
      if (filter->spatial_idx.has_value() &&
          f.spatial_idx != *filter->spatial_idx) {
        continue;
      }
      if (filter->temporal_idx.has_value() &&
          f.temporal_idx > *filter->temporal_idx) {
        continue;
      }
    }
    frames.push_back(f);
  }
  return frames;
}

Stream VideoCodecStatsImpl::Aggregate(
    const std::vector<Frame>& frames,
    absl::optional<DataRate> bitrate,
    absl::optional<Frequency> framerate) const {
  std::vector<Frame> superframes = Merge(frames);

  Stream stream;
  stream.num_frames = static_cast<int>(superframes.size());

  for (const auto& f : superframes) {
    Timestamp time = Timestamp::Micros((f.timestamp_rtp / k90kHz).us());
    // TODO(webrtc:14852): Add AddSample(double value, Timestamp time) method to
    // SamplesStatsCounter.
    stream.decode_time_us.AddSample(SamplesStatsCounter::StatsSample(
        {.value = static_cast<double>(f.decode_time.us()), .time = time}));

    if (f.psnr) {
      stream.psnr.y.AddSample(
          SamplesStatsCounter::StatsSample({.value = f.psnr->y, .time = time}));
      stream.psnr.u.AddSample(
          SamplesStatsCounter::StatsSample({.value = f.psnr->u, .time = time}));
      stream.psnr.v.AddSample(
          SamplesStatsCounter::StatsSample({.value = f.psnr->v, .time = time}));
    }

    if (f.keyframe) {
      ++stream.num_keyframes;
    }

    // TODO(webrtc:14852): Aggregate other metrics.
  }

  return stream;
}

void VideoCodecStatsImpl::LogMetrics(MetricsLogger* logger,
                                     const Stream& stream,
                                     std::string test_case_name) const {
  logger->LogMetric("width", test_case_name, stream.width, Unit::kCount,
                    webrtc::test::ImprovementDirection::kBiggerIsBetter);
  // TODO(webrtc:14852): Log other metrics.
}

Frame* VideoCodecStatsImpl::AddFrame(int frame_num,
                                     uint32_t timestamp_rtp,
                                     int spatial_idx) {
  Frame frame;
  frame.frame_num = frame_num;
  frame.timestamp_rtp = timestamp_rtp;
  frame.spatial_idx = spatial_idx;

  FrameId frame_id;
  frame_id.frame_num = frame_num;
  frame_id.spatial_idx = spatial_idx;

  RTC_CHECK(frames_.find(frame_id) == frames_.end())
      << "Frame with frame_num=" << frame_num
      << " and spatial_idx=" << spatial_idx << " already exists";

  frames_[frame_id] = frame;

  if (frame_num_.find(timestamp_rtp) == frame_num_.end()) {
    frame_num_[timestamp_rtp] = frame_num;
  }

  return &frames_[frame_id];
}

Frame* VideoCodecStatsImpl::GetFrame(uint32_t timestamp_rtp, int spatial_idx) {
  if (frame_num_.find(timestamp_rtp) == frame_num_.end()) {
    return nullptr;
  }

  FrameId frame_id;
  frame_id.frame_num = frame_num_[timestamp_rtp];
  frame_id.spatial_idx = spatial_idx;

  if (frames_.find(frame_id) == frames_.end()) {
    return nullptr;
  }

  return &frames_[frame_id];
}

std::vector<Frame> VideoCodecStatsImpl::Merge(
    const std::vector<Frame>& frames) const {
  std::vector<Frame> superframes;
  // Map from frame_num to index in `superframes` vector.
  std::map<int, int> index;

  for (const auto& f : frames) {
    if (f.encoded == false && f.decoded == false) {
      continue;
    }

    if (index.find(f.frame_num) == index.end()) {
      index[f.frame_num] = static_cast<int>(superframes.size());
      superframes.push_back(f);
      continue;
    }

    Frame& sf = superframes[index[f.frame_num]];

    sf.width = std::max(sf.width, f.width);
    sf.height = std::max(sf.height, f.height);
    sf.size_bytes += f.size_bytes;
    sf.keyframe |= f.keyframe;

    sf.encode_time = std::max(sf.encode_time, f.encode_time);
    sf.decode_time += f.decode_time;

    if (f.spatial_idx > sf.spatial_idx) {
      if (f.qp) {
        sf.qp = f.qp;
      }
      if (f.psnr) {
        sf.psnr = f.psnr;
      }
    }

    sf.spatial_idx = std::max(sf.spatial_idx, f.spatial_idx);
    sf.temporal_idx = std::max(sf.temporal_idx, f.temporal_idx);

    sf.encoded |= f.encoded;
    sf.decoded |= f.decoded;
  }

  return superframes;
}

}  // namespace test
}  // namespace webrtc
