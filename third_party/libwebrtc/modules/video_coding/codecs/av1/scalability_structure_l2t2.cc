/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/scalability_structure_l2t2.h"

#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

constexpr auto kNotPresent = DecodeTargetIndication::kNotPresent;
constexpr auto kDiscardable = DecodeTargetIndication::kDiscardable;
constexpr auto kSwitch = DecodeTargetIndication::kSwitch;
constexpr auto kRequired = DecodeTargetIndication::kRequired;

constexpr DecodeTargetIndication kDtis[3][2][4] = {
    {{kSwitch, kSwitch, kSwitch, kSwitch},                    // kKey, S0
     {kNotPresent, kNotPresent, kSwitch, kSwitch}},           // kKey, S1
    {{kNotPresent, kDiscardable, kNotPresent, kRequired},     // kDeltaT1, S0
     {kNotPresent, kNotPresent, kNotPresent, kDiscardable}},  // kDeltaT1, S1
    {{kSwitch, kSwitch, kRequired, kRequired},                // kDeltaT0, S0
     {kNotPresent, kNotPresent, kSwitch, kSwitch}}};          // kDeltaT0, S1

}  // namespace

constexpr int ScalabilityStructureL2T2::kNumSpatialLayers;
constexpr int ScalabilityStructureL2T2::kNumTemporalLayers;

ScalabilityStructureL2T2::~ScalabilityStructureL2T2() = default;

ScalableVideoController::StreamLayersConfig
ScalabilityStructureL2T2::StreamConfig() const {
  StreamLayersConfig result;
  result.num_spatial_layers = kNumSpatialLayers;
  result.num_temporal_layers = kNumTemporalLayers;
  result.scaling_factor_num[0] = 1;
  result.scaling_factor_den[0] = 2;
  return result;
}

FrameDependencyStructure ScalabilityStructureL2T2::DependencyStructure() const {
  FrameDependencyStructure structure;
  structure.num_decode_targets = kNumSpatialLayers * kNumTemporalLayers;
  structure.num_chains = kNumSpatialLayers;
  structure.decode_target_protected_by_chain = {0, 0, 1, 1};
  structure.templates.resize(6);
  auto& templates = structure.templates;
  templates[0].S(0).T(0).Dtis("SSSS").ChainDiffs({0, 0});
  templates[1].S(0).T(0).Dtis("SSRR").ChainDiffs({4, 3}).FrameDiffs({4});
  templates[2].S(0).T(1).Dtis("-D-R").ChainDiffs({2, 1}).FrameDiffs({2});
  templates[3].S(1).T(0).Dtis("--SS").ChainDiffs({1, 1}).FrameDiffs({1});
  templates[4].S(1).T(0).Dtis("--SS").ChainDiffs({1, 1}).FrameDiffs({4, 1});
  templates[5].S(1).T(1).Dtis("---D").ChainDiffs({3, 2}).FrameDiffs({2, 1});
  return structure;
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalabilityStructureL2T2::NextFrameConfig(bool restart) {
  if (restart || next_pattern_ == kKey) {
    for (int sid = 0; sid < kNumSpatialLayers; ++sid) {
      use_temporal_dependency_on_t0_[sid] = false;
    }
    next_pattern_ = kKey;
  }
  if (next_pattern_ == kDeltaT1 &&  //
      !DecodeTargetIsActive(/*sid=*/0, /*tid=*/1) &&
      !DecodeTargetIsActive(/*sid=*/1, /*tid=*/1)) {
    // T1 is inactive for both spatial layers, so do not generate T1 frames.
    // T1 could have been disabled after previous call to NextFrameConfig,
    // thus need to check it here rather than when setting next_pattern_ after
    // T0 frame.
    next_pattern_ = kDeltaT0;
  }
  std::vector<LayerFrameConfig> configs;
  configs.reserve(kNumSpatialLayers);

  switch (next_pattern_) {
    case kKey:
    case kDeltaT0: {
      for (int sid = 0; sid < kNumSpatialLayers; ++sid) {
        if (!DecodeTargetIsActive(sid, /*tid=*/0)) {
          // Next frame from the spatial layer `sid` shouldn't depend on
          // potentially old previous frame from the spatial layer `sid`.
          use_temporal_dependency_on_t0_[sid] = false;
          continue;
        }
        configs.emplace_back();
        ScalableVideoController::LayerFrameConfig& config = configs.back();
        config.Id(next_pattern_).S(sid).T(0);
        if (use_temporal_dependency_on_t0_[sid]) {
          config.ReferenceAndUpdate(BufferIndex(sid, /*tid=*/0));
        } else {
          config.Update(BufferIndex(sid, /*tid=*/0));
        }
        if (sid == 1 && DecodeTargetIsActive(/*sid=*/0, /*tid=*/0)) {
          config.Reference(BufferIndex(/*sid=*/0, /*tid=*/0));
        } else if (next_pattern_ == kKey) {
          config.Keyframe();
        }
        use_temporal_dependency_on_t0_[sid] = true;
      }

      next_pattern_ = kDeltaT1;
    } break;
    case kDeltaT1:
      if (DecodeTargetIsActive(/*sid=*/0, /*tid=*/1)) {
        configs.emplace_back();
        ScalableVideoController::LayerFrameConfig& config = configs.back();
        config.Id(next_pattern_)
            .S(0)
            .T(1)
            .Reference(BufferIndex(/*sid=*/0, /*tid=*/0))
            .Update(BufferIndex(/*sid=*/0, /*tid=*/1));
      }
      if (DecodeTargetIsActive(/*sid=*/1, /*tid=*/1)) {
        configs.emplace_back();
        ScalableVideoController::LayerFrameConfig& config = configs.back();
        config.Id(next_pattern_)
            .S(1)
            .T(1)
            .Reference(BufferIndex(/*sid=*/1, /*tid=*/0));
        if (DecodeTargetIsActive(/*sid=*/0, /*tid=*/1)) {
          config.Reference(BufferIndex(/*sid=*/0, /*tid=*/1));
        }
      }
      next_pattern_ = kDeltaT0;
      break;
  }
  return configs;
}

absl::optional<GenericFrameInfo> ScalabilityStructureL2T2::OnEncodeDone(
    LayerFrameConfig config) {
  absl::optional<GenericFrameInfo> frame_info;
  int pattern_idx = config.IsKeyframe() ? 0 : config.Id();
  if (pattern_idx < 0 || pattern_idx >= int{ABSL_ARRAYSIZE(kDtis)}) {
    RTC_LOG(LS_ERROR) << "Unexpected config id " << config.Id();
    return frame_info;
  }
  if (config.SpatialId() < 0 || config.SpatialId() >= kNumSpatialLayers) {
    RTC_LOG(LS_ERROR) << "Unexpected spatial id " << config.SpatialId();
    return frame_info;
  }

  frame_info.emplace();
  frame_info->spatial_id = config.SpatialId();
  frame_info->temporal_id = config.TemporalId();
  frame_info->encoder_buffers = config.Buffers();
  const auto& dtis = kDtis[pattern_idx][config.SpatialId()];
  frame_info->decode_target_indications.assign(std::begin(dtis),
                                               std::end(dtis));
  if (config.TemporalId() == 0) {
    frame_info->part_of_chain = {config.SpatialId() == 0, true};
  } else {
    frame_info->part_of_chain = {false, false};
  }
  frame_info->active_decode_targets = active_decode_targets_;
  return frame_info;
}

void ScalabilityStructureL2T2::OnRatesUpdated(
    const VideoBitrateAllocation& bitrates) {
  for (int sid = 0; sid < kNumSpatialLayers; ++sid) {
    bool active = bitrates.GetBitrate(sid, /*tid=*/0) > 0;
    SetDecodeTargetIsActive(sid, /*tid=*/0, active);
    SetDecodeTargetIsActive(sid, /*tid=*/1,
                            active && bitrates.GetBitrate(sid, /*tid=*/1) > 0);
  }
}

}  // namespace webrtc
