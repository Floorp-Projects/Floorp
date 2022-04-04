/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/scalability_structure_full_svc.h"

#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

constexpr int ScalabilityStructureFullSvc::kMaxNumSpatialLayers;
constexpr int ScalabilityStructureFullSvc::kMaxNumTemporalLayers;

ScalabilityStructureFullSvc::ScalabilityStructureFullSvc(
    int num_spatial_layers,
    int num_temporal_layers)
    : num_spatial_layers_(num_spatial_layers),
      num_temporal_layers_(num_temporal_layers),
      active_decode_targets_(
          (uint32_t{1} << (num_spatial_layers * num_temporal_layers)) - 1) {
  RTC_DCHECK_LE(num_spatial_layers, kMaxNumSpatialLayers);
  RTC_DCHECK_LE(num_temporal_layers, kMaxNumTemporalLayers);
}

ScalabilityStructureFullSvc::~ScalabilityStructureFullSvc() = default;

ScalabilityStructureFullSvc::StreamLayersConfig
ScalabilityStructureFullSvc::StreamConfig() const {
  StreamLayersConfig result;
  result.num_spatial_layers = num_spatial_layers_;
  result.num_temporal_layers = num_temporal_layers_;
  result.scaling_factor_num[num_spatial_layers_ - 1] = 1;
  result.scaling_factor_den[num_spatial_layers_ - 1] = 1;
  for (int sid = num_spatial_layers_ - 1; sid > 0; --sid) {
    result.scaling_factor_num[sid - 1] = 1;
    result.scaling_factor_den[sid - 1] = 2 * result.scaling_factor_den[sid];
  }
  return result;
}

bool ScalabilityStructureFullSvc::TemporalLayerIsActive(int tid) const {
  RTC_DCHECK_LT(tid, num_temporal_layers_);
  for (int sid = 0; sid < num_spatial_layers_; ++sid) {
    if (DecodeTargetIsActive(sid, tid)) {
      return true;
    }
  }
  return false;
}

DecodeTargetIndication ScalabilityStructureFullSvc::Dti(
    int sid,
    int tid,
    const LayerFrameConfig& config) {
  if (sid < config.SpatialId() || tid < config.TemporalId()) {
    return DecodeTargetIndication::kNotPresent;
  }
  if (sid == config.SpatialId()) {
    if (tid == 0) {
      RTC_DCHECK_EQ(config.TemporalId(), 0);
      return DecodeTargetIndication::kSwitch;
    }
    if (tid == config.TemporalId()) {
      return DecodeTargetIndication::kDiscardable;
    }
    if (tid > config.TemporalId()) {
      RTC_DCHECK_GT(tid, config.TemporalId());
      return DecodeTargetIndication::kSwitch;
    }
  }
  RTC_DCHECK_GT(sid, config.SpatialId());
  RTC_DCHECK_GE(tid, config.TemporalId());
  if (config.IsKeyframe() || config.Id() == kKey) {
    return DecodeTargetIndication::kSwitch;
  }
  return DecodeTargetIndication::kRequired;
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalabilityStructureFullSvc::NextFrameConfig(bool restart) {
  std::vector<LayerFrameConfig> configs;
  configs.reserve(num_spatial_layers_);

  if (next_pattern_ == kKey || restart) {
    can_depend_on_t0_frame_for_spatial_id_.reset();
    next_pattern_ = kKey;
  }
  // T1 could have been disabled after previous call to NextFrameConfig,
  // thus need to check it here rather than when setting next_pattern_ below.
  if (next_pattern_ == kDeltaT1 && !TemporalLayerIsActive(/*tid=*/1)) {
    next_pattern_ = kDeltaT0;
  }

  absl::optional<int> spatial_dependency_buffer_id;
  switch (next_pattern_) {
    case kKey:
    case kDeltaT0:
      for (int sid = 0; sid < num_spatial_layers_; ++sid) {
        if (!DecodeTargetIsActive(sid, /*tid=*/0)) {
          // Next frame from the spatial layer `sid` shouldn't depend on
          // potentially old previous frame from the spatial layer `sid`.
          can_depend_on_t0_frame_for_spatial_id_.reset(sid);
          continue;
        }
        configs.emplace_back();
        ScalableVideoController::LayerFrameConfig& config = configs.back();
        config.Id(next_pattern_).S(sid).T(0);

        if (spatial_dependency_buffer_id) {
          config.Reference(*spatial_dependency_buffer_id);
        } else if (next_pattern_ == kKey) {
          config.Keyframe();
        }

        if (can_depend_on_t0_frame_for_spatial_id_[sid]) {
          config.ReferenceAndUpdate(BufferIndex(sid, /*tid=*/0));
        } else {
          // TODO(bugs.webrtc.org/11999): Propagate chain restart on delta frame
          // to ChainDiffCalculator
          config.Update(BufferIndex(sid, /*tid=*/0));
        }

        can_depend_on_t0_frame_for_spatial_id_.set(sid);
        spatial_dependency_buffer_id = BufferIndex(sid, /*tid=*/0);
      }

      next_pattern_ = num_temporal_layers_ == 2 ? kDeltaT1 : kDeltaT0;
      break;
    case kDeltaT1:
      for (int sid = 0; sid < num_spatial_layers_; ++sid) {
        if (!DecodeTargetIsActive(sid, /*tid=*/1) ||
            !can_depend_on_t0_frame_for_spatial_id_[sid]) {
          continue;
        }
        configs.emplace_back();
        ScalableVideoController::LayerFrameConfig& config = configs.back();
        config.Id(next_pattern_).S(sid).T(1);
        // Temporal reference.
        RTC_DCHECK(DecodeTargetIsActive(sid, /*tid=*/0));
        config.Reference(BufferIndex(sid, /*tid=*/0));
        // Spatial reference unless this is the lowest active spatial layer.
        if (spatial_dependency_buffer_id) {
          config.Reference(*spatial_dependency_buffer_id);
        }
        // No frame reference top layer frame, so no need save it into a buffer.
        if (sid < num_spatial_layers_ - 1) {
          config.Update(BufferIndex(sid, /*tid=*/1));
        }
        spatial_dependency_buffer_id = BufferIndex(sid, /*tid=*/1);
      }
      next_pattern_ = kDeltaT0;
      break;
  }
  return configs;
}

absl::optional<GenericFrameInfo> ScalabilityStructureFullSvc::OnEncodeDone(
    LayerFrameConfig config) {
  absl::optional<GenericFrameInfo> frame_info(absl::in_place);
  frame_info->spatial_id = config.SpatialId();
  frame_info->temporal_id = config.TemporalId();
  frame_info->encoder_buffers = config.Buffers();
  frame_info->decode_target_indications.reserve(num_spatial_layers_ *
                                                num_temporal_layers_);
  for (int sid = 0; sid < num_spatial_layers_; ++sid) {
    for (int tid = 0; tid < num_temporal_layers_; ++tid) {
      frame_info->decode_target_indications.push_back(Dti(sid, tid, config));
    }
  }
  if (config.TemporalId() == 0) {
    frame_info->part_of_chain.resize(num_spatial_layers_);
    for (int sid = 0; sid < num_spatial_layers_; ++sid) {
      frame_info->part_of_chain[sid] = config.SpatialId() <= sid;
    }
  } else {
    frame_info->part_of_chain.assign(num_spatial_layers_, false);
  }
  frame_info->active_decode_targets = active_decode_targets_;
  return frame_info;
}

void ScalabilityStructureFullSvc::OnRatesUpdated(
    const VideoBitrateAllocation& bitrates) {
  for (int sid = 0; sid < num_spatial_layers_; ++sid) {
    // Enable/disable spatial layers independetely.
    bool active = true;
    for (int tid = 0; tid < num_temporal_layers_; ++tid) {
      // To enable temporal layer, require bitrates for lower temporal layers.
      active = active && bitrates.GetBitrate(sid, tid) > 0;
      SetDecodeTargetIsActive(sid, tid, active);
    }
  }
}

}  // namespace webrtc
