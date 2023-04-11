/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_video_header.h"

#include "api/video/video_frame_metadata.h"
#include "api/video/video_frame_type.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(RTPVideoHeaderTest, GetAsMetadataGetFrameType) {
  RTPVideoHeader video_header;
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetFrameType(), VideoFrameType::kVideoFrameKey);
}

TEST(RTPVideoHeaderTest, GetAsMetadataGetWidth) {
  RTPVideoHeader video_header;
  video_header.width = 1280u;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetWidth(), video_header.width);
}

TEST(RTPVideoHeaderTest, GetAsMetadataGetHeight) {
  RTPVideoHeader video_header;
  video_header.height = 720u;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetHeight(), video_header.height);
}

TEST(RTPVideoHeaderTest, GetAsMetadataGetRotation) {
  RTPVideoHeader video_header;
  video_header.rotation = VideoRotation::kVideoRotation_90;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetRotation(), VideoRotation::kVideoRotation_90);
}

TEST(RTPVideoHeaderTest, GetAsMetadataGetContentType) {
  RTPVideoHeader video_header;
  video_header.content_type = VideoContentType::SCREENSHARE;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetContentType(), VideoContentType::SCREENSHARE);
}

TEST(RTPVideoHeaderTest, GetAsMetadataGetFrameId) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.frame_id = 10;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetFrameId().value(), 10);
}

TEST(RTPVideoHeaderTest, GetAsMetadataHasNoFrameIdForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  ASSERT_FALSE(video_header.generic);
  EXPECT_FALSE(metadata.GetFrameId().has_value());
}

TEST(RTPVideoHeaderTest, GetAsMetadataGetSpatialIndex) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.spatial_index = 2;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetSpatialIndex(), 2);
}

TEST(RTPVideoHeaderTest,
     GetAsMetadataSpatialIndexIsZeroForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  ASSERT_FALSE(video_header.generic);
  EXPECT_EQ(metadata.GetSpatialIndex(), 0);
}

TEST(RTPVideoHeaderTest, GetAsMetadataGetTemporalIndex) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.temporal_index = 3;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetTemporalIndex(), 3);
}

TEST(RTPVideoHeaderTest,
     GetAsMetadataTemporalIndexIsZeroForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  ASSERT_FALSE(video_header.generic);
  EXPECT_EQ(metadata.GetTemporalIndex(), 0);
}

TEST(RTPVideoHeaderTest, GetAsMetadataGetFrameDependencies) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.dependencies = {5, 6, 7};
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_THAT(metadata.GetFrameDependencies(), ElementsAre(5, 6, 7));
}

TEST(RTPVideoHeaderTest,
     GetAsMetadataFrameDependencyIsEmptyForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  ASSERT_FALSE(video_header.generic);
  EXPECT_THAT(metadata.GetFrameDependencies(), IsEmpty());
}

TEST(RTPVideoHeaderTest, GetAsMetadataGetDecodeTargetIndications) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.decode_target_indications = {DecodeTargetIndication::kSwitch};
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_THAT(metadata.GetDecodeTargetIndications(),
              ElementsAre(DecodeTargetIndication::kSwitch));
}

TEST(RTPVideoHeaderTest,
     GetAsMetadataGetDecodeTargetIndicationsIsEmptyForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  ASSERT_FALSE(video_header.generic);
  EXPECT_THAT(metadata.GetDecodeTargetIndications(), IsEmpty());
}

TEST(RTPVideoHeaderTest, GetAsMetadataGetIsLastFrameInPicture) {
  RTPVideoHeader video_header;
  video_header.is_last_frame_in_picture = false;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_FALSE(metadata.GetIsLastFrameInPicture());
}

TEST(RTPVideoHeaderTest, GetAsMetadataGetSimulcastIdx) {
  RTPVideoHeader video_header;
  video_header.simulcastIdx = 123;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetSimulcastIdx(), 123);
}

TEST(RTPVideoHeaderTest, GetAsMetadataGetCodec) {
  RTPVideoHeader video_header;
  video_header.codec = VideoCodecType::kVideoCodecVP9;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetCodec(), VideoCodecType::kVideoCodecVP9);
}

}  // namespace
}  // namespace webrtc
