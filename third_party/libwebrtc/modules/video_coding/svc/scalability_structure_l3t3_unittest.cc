/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/svc/scalability_structure_l3t3.h"

#include "modules/video_coding/svc/scalability_structure_test_helpers.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::IsEmpty;
using ::testing::SizeIs;

TEST(ScalabilityStructureL3T3Test, SkipS1T1FrameKeepsStructureValid) {
  ScalabilityStructureL3T3 structure;
  ScalabilityStructureWrapper wrapper(structure);

  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/3, /*s1=*/3));
  auto frames = wrapper.GenerateFrames(/*num_temporal_units=*/1);
  EXPECT_THAT(frames, SizeIs(2));
  EXPECT_EQ(frames[0].temporal_id, 0);

  frames = wrapper.GenerateFrames(/*num_temporal_units=*/1);
  EXPECT_THAT(frames, SizeIs(2));
  EXPECT_EQ(frames[0].temporal_id, 2);

  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/3, /*s1=*/0));
  frames = wrapper.GenerateFrames(/*num_temporal_units=*/1);
  EXPECT_THAT(frames, SizeIs(1));
  EXPECT_EQ(frames[0].temporal_id, 1);

  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/3, /*s1=*/3));
  // Rely on checks inside GenerateFrames frame references are valid.
  frames = wrapper.GenerateFrames(/*num_temporal_units=*/1);
  EXPECT_THAT(frames, SizeIs(2));
  EXPECT_EQ(frames[0].temporal_id, 2);
}

TEST(ScalabilityStructureL3T3Test, SwitchSpatialLayerBeforeT1Frame) {
  ScalabilityStructureL3T3 structure;
  ScalabilityStructureWrapper wrapper(structure);

  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/2, /*s1=*/0));
  EXPECT_THAT(wrapper.GenerateFrames(1), SizeIs(1));
  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/0, /*s1=*/2));
  auto frames = wrapper.GenerateFrames(1);
  ASSERT_THAT(frames, SizeIs(1));
  EXPECT_THAT(frames[0].frame_diffs, IsEmpty());
  EXPECT_EQ(frames[0].temporal_id, 0);
}

}  // namespace
}  // namespace webrtc
