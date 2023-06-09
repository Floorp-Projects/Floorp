/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_STATS_IMPL_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_STATS_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/test/video_codec_stats.h"

namespace webrtc {
namespace test {

// Implementation of `VideoCodecStats`. This class is not thread-safe.
class VideoCodecStatsImpl : public VideoCodecStats {
 public:
  std::vector<Frame> Slice(
      absl::optional<Filter> filter = absl::nullopt) const override;

  Stream Aggregate(
      const std::vector<Frame>& frames,
      absl::optional<DataRate> bitrate = absl::nullopt,
      absl::optional<Frequency> framerate = absl::nullopt) const override;

  void LogMetrics(MetricsLogger* logger,
                  const Stream& stream,
                  std::string test_case_name) const override;

  // Creates new frame, caches it and returns raw pointer to it.
  Frame* AddFrame(int frame_num, uint32_t timestamp_rtp, int spatial_idx);

  // Returns raw pointers to requested frame. If frame does not exist, returns
  // `nullptr`.
  Frame* GetFrame(uint32_t timestamp_rtp, int spatial_idx);

 private:
  struct FrameId {
    int frame_num;
    int spatial_idx;

    bool operator==(const FrameId& o) const {
      return frame_num == o.frame_num && spatial_idx == o.spatial_idx;
    }

    bool operator<(const FrameId& o) const {
      if (frame_num < o.frame_num)
        return true;
      if (spatial_idx < o.spatial_idx)
        return true;
      return false;
    }
  };

  // Merges frame stats from different spatial layers and returns vector of
  // superframes.
  std::vector<Frame> Merge(const std::vector<Frame>& frames) const;

  // Map from RTP timestamp to frame number (`Frame::frame_num`).
  std::map<uint32_t, int> frame_num_;

  std::map<FrameId, Frame> frames_;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_STATS_IMPL_H_
