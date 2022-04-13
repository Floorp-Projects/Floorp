/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/scalability_structure_test_helpers.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_frame_type.h"
#include "modules/video_coding/chain_diff_calculator.h"
#include "modules/video_coding/codecs/av1/scalable_video_controller.h"
#include "modules/video_coding/frame_dependencies_calculator.h"
#include "test/gtest.h"

namespace webrtc {

VideoBitrateAllocation EnableTemporalLayers(int s0, int s1, int s2) {
  VideoBitrateAllocation bitrate;
  for (int tid = 0; tid < s0; ++tid) {
    bitrate.SetBitrate(0, tid, 1'000'000);
  }
  for (int tid = 0; tid < s1; ++tid) {
    bitrate.SetBitrate(1, tid, 1'000'000);
  }
  for (int tid = 0; tid < s2; ++tid) {
    bitrate.SetBitrate(2, tid, 1'000'000);
  }
  return bitrate;
}

std::vector<GenericFrameInfo> ScalabilityStructureWrapper::GenerateFrames(
    int num_temporal_units,
    bool restart) {
  std::vector<GenericFrameInfo> frames;
  for (int i = 0; i < num_temporal_units; ++i) {
    for (auto& layer_frame : structure_controller_.NextFrameConfig(restart)) {
      int64_t frame_id = ++frame_id_;
      bool is_keyframe = layer_frame.IsKeyframe();

      absl::optional<GenericFrameInfo> frame_info =
          structure_controller_.OnEncodeDone(std::move(layer_frame));
      EXPECT_TRUE(frame_info.has_value());
      if (is_keyframe) {
        chain_diff_calculator_.Reset(frame_info->part_of_chain);
      }
      frame_info->chain_diffs =
          chain_diff_calculator_.From(frame_id, frame_info->part_of_chain);
      for (int64_t base_frame_id : frame_deps_calculator_.FromBuffersUsage(
               is_keyframe ? VideoFrameType::kVideoFrameKey
                           : VideoFrameType::kVideoFrameDelta,
               frame_id, frame_info->encoder_buffers)) {
        EXPECT_LT(base_frame_id, frame_id);
        EXPECT_GE(base_frame_id, 0);
        frame_info->frame_diffs.push_back(frame_id - base_frame_id);
      }

      frames.push_back(*std::move(frame_info));
    }
    restart = false;
  }

  if (restart) {
    buffer_contains_frame_.reset();
  }
  for (const GenericFrameInfo& frame : frames) {
    for (const CodecBufferUsage& buffer_usage : frame.encoder_buffers) {
      if (buffer_usage.id < 0 || buffer_usage.id >= 8) {
        ADD_FAILURE() << "Invalid buffer id " << buffer_usage.id
                      << ". Up to 8 buffers are supported.";
        continue;
      }
      if (buffer_usage.referenced && !buffer_contains_frame_[buffer_usage.id]) {
        ADD_FAILURE() << "buffer " << buffer_usage.id
                      << " was reference before updated.";
      }
      if (buffer_usage.updated) {
        buffer_contains_frame_.set(buffer_usage.id);
      }
    }
  }

  return frames;
}

}  // namespace webrtc
