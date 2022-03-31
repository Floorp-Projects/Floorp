/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_CODECS_AV1_SCALABILITY_STRUCTURE_L2T2_H_
#define MODULES_VIDEO_CODING_CODECS_AV1_SCALABILITY_STRUCTURE_L2T2_H_

#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "modules/video_coding/codecs/av1/scalable_video_controller.h"

namespace webrtc {

// S1T1     0   0
//         /|  /|  /
// S1T0   0-+-0-+-0
//        | | | | | ...
// S0T1   | 0 | 0 |
//        |/  |/  |/
// S0T0   0---0---0--
// Time-> 0 1 2 3 4
class ScalabilityStructureL2T2 : public ScalableVideoController {
 public:
  ~ScalabilityStructureL2T2() override;

  StreamLayersConfig StreamConfig() const override;
  FrameDependencyStructure DependencyStructure() const override;

  std::vector<LayerFrameConfig> NextFrameConfig(bool restart) override;
  absl::optional<GenericFrameInfo> OnEncodeDone(
      LayerFrameConfig config) override;

  void OnRatesUpdated(const VideoBitrateAllocation& bitrates) override;

 private:
  enum FramePattern {
    kKey,
    kDeltaT1,
    kDeltaT0,
  };
  static constexpr int kNumSpatialLayers = 2;
  static constexpr int kNumTemporalLayers = 2;

  // Index of the buffer to store last frame for layer (`sid`, `tid`)
  static constexpr int BufferIndex(int sid, int tid) {
    return tid * kNumSpatialLayers + sid;
  }
  bool DecodeTargetIsActive(int sid, int tid) const {
    return active_decode_targets_[sid * kNumTemporalLayers + tid];
  }
  void SetDecodeTargetIsActive(int sid, int tid, bool value) {
    active_decode_targets_.set(sid * kNumTemporalLayers + tid, value);
  }

  FramePattern next_pattern_ = kKey;
  bool use_temporal_dependency_on_t0_[kNumSpatialLayers] = {false, false};
  std::bitset<32> active_decode_targets_ = 0b1111;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_AV1_SCALABILITY_STRUCTURE_L2T2_H_
