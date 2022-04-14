/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/svc/scalability_structure_key_svc.h"

#include <vector>

#include "api/array_view.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "modules/video_coding/svc/scalability_structure_test_helpers.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SizeIs;

TEST(ScalabilityStructureL3T3KeyTest,
     SkipingT1FrameOnOneSpatialLayerKeepsStructureValid) {
  ScalabilityStructureL3T3Key structure;
  ScalabilityStructureWrapper wrapper(structure);
  std::vector<GenericFrameInfo> frames;

  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/3, /*s1=*/3));
  wrapper.GenerateFrames(/*num_temporal_units=*/2, frames);
  EXPECT_THAT(frames, SizeIs(4));
  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/3, /*s1=*/1));
  wrapper.GenerateFrames(/*num_temporal_units=*/1, frames);
  EXPECT_THAT(frames, SizeIs(5));
  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/3, /*s1=*/3));
  wrapper.GenerateFrames(/*num_temporal_units=*/1, frames);
  ASSERT_THAT(frames, SizeIs(7));

  EXPECT_EQ(frames[0].temporal_id, 0);
  EXPECT_EQ(frames[1].temporal_id, 0);
  EXPECT_EQ(frames[2].temporal_id, 2);
  EXPECT_EQ(frames[3].temporal_id, 2);
  EXPECT_EQ(frames[4].temporal_id, 1);
  EXPECT_EQ(frames[5].temporal_id, 2);
  EXPECT_EQ(frames[6].temporal_id, 2);
  EXPECT_TRUE(wrapper.FrameReferencesAreValid(frames));
}

TEST(ScalabilityStructureL3T3KeyTest,
     ReenablingSpatialLayerBeforeMissedT0FrameDoesntTriggerAKeyFrame) {
  ScalabilityStructureL3T3Key structure;
  ScalabilityStructureWrapper wrapper(structure);
  std::vector<GenericFrameInfo> frames;

  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/2, /*s1=*/2));
  wrapper.GenerateFrames(1, frames);
  EXPECT_THAT(frames, SizeIs(2));
  // Drop a spatial layer.
  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/2, /*s1=*/0));
  wrapper.GenerateFrames(1, frames);
  EXPECT_THAT(frames, SizeIs(3));
  // Reenable a spatial layer before T0 frame is encoded.
  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/2, /*s1=*/2));
  wrapper.GenerateFrames(1, frames);
  EXPECT_THAT(frames, SizeIs(5));

  EXPECT_EQ(frames[0].temporal_id, 0);
  EXPECT_EQ(frames[1].temporal_id, 0);
  EXPECT_EQ(frames[2].temporal_id, 1);
  EXPECT_EQ(frames[3].temporal_id, 0);
  EXPECT_EQ(frames[4].temporal_id, 0);
  EXPECT_THAT(frames[3].frame_diffs, SizeIs(1));
  EXPECT_THAT(frames[4].frame_diffs, SizeIs(1));
  EXPECT_TRUE(wrapper.FrameReferencesAreValid(frames));
}

TEST(ScalabilityStructureL3T3KeyTest, ReenablingSpatialLayerTriggersKeyFrame) {
  ScalabilityStructureL3T3Key structure;
  ScalabilityStructureWrapper wrapper(structure);
  std::vector<GenericFrameInfo> frames;

  // Start with all spatial layers enabled.
  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/2, /*s1=*/2, /*s2=*/2));
  wrapper.GenerateFrames(3, frames);
  EXPECT_THAT(frames, SizeIs(9));
  // Drop a spatial layer. Two remaining spatial layers should just continue.
  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/2, /*s1=*/0, /*s2=*/2));
  wrapper.GenerateFrames(2, frames);
  EXPECT_THAT(frames, SizeIs(13));
  // Reenable spatial layer, expect a full restart.
  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/2, /*s1=*/2, /*s2=*/2));
  wrapper.GenerateFrames(1, frames);
  ASSERT_THAT(frames, SizeIs(16));

  // First 3 temporal units with all spatial layers enabled.
  EXPECT_EQ(frames[0].temporal_id, 0);
  EXPECT_EQ(frames[3].temporal_id, 1);
  EXPECT_EQ(frames[6].temporal_id, 0);
  // 2 temporal units with spatial layer 1 disabled.
  EXPECT_EQ(frames[9].spatial_id, 0);
  EXPECT_EQ(frames[9].temporal_id, 1);
  EXPECT_EQ(frames[10].spatial_id, 2);
  EXPECT_EQ(frames[10].temporal_id, 1);
  // T0 frames were encoded while spatial layer 1 is disabled.
  EXPECT_EQ(frames[11].spatial_id, 0);
  EXPECT_EQ(frames[11].temporal_id, 0);
  EXPECT_EQ(frames[12].spatial_id, 2);
  EXPECT_EQ(frames[12].temporal_id, 0);
  // Key frame to reenable spatial layer 1.
  EXPECT_THAT(frames[13].frame_diffs, IsEmpty());
  EXPECT_THAT(frames[14].frame_diffs, ElementsAre(1));
  EXPECT_THAT(frames[15].frame_diffs, ElementsAre(1));
  EXPECT_EQ(frames[13].temporal_id, 0);
  EXPECT_EQ(frames[14].temporal_id, 0);
  EXPECT_EQ(frames[15].temporal_id, 0);
  auto all_frames = rtc::MakeArrayView(frames.data(), frames.size());
  EXPECT_TRUE(wrapper.FrameReferencesAreValid(all_frames.subview(0, 13)));
  // Frames starting from the frame#13 should not reference any earlier frames.
  EXPECT_TRUE(wrapper.FrameReferencesAreValid(all_frames.subview(13)));
}

}  // namespace
}  // namespace webrtc
