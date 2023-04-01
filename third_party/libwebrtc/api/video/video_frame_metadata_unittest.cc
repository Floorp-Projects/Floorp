/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/video_frame_metadata.h"

#include "api/video/video_frame_type.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

// TODO(https://crbug.com/webrtc/14709): Move all of these tests to
// rtp_video_header_unittest.cc, they're excercising GetAsMetadata().

TEST(VideoFrameMetadata, GetFrameTypeReturnsCorrectValue) {
  RTPVideoHeader video_header;
  video_header.frame_type = VideoFrameType::kVideoFrameKey;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetFrameType(), VideoFrameType::kVideoFrameKey);
}

TEST(VideoFrameMetadata, GetWidthReturnsCorrectValue) {
  RTPVideoHeader video_header;
  video_header.width = 1280u;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetWidth(), video_header.width);
}

TEST(VideoFrameMetadata, GetHeightReturnsCorrectValue) {
  RTPVideoHeader video_header;
  video_header.height = 720u;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetHeight(), video_header.height);
}

TEST(VideoFrameMetadata, GetRotationReturnsCorrectValue) {
  RTPVideoHeader video_header;
  video_header.rotation = VideoRotation::kVideoRotation_90;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetRotation(), VideoRotation::kVideoRotation_90);
}

TEST(VideoFrameMetadata, GetContentTypeReturnsCorrectValue) {
  RTPVideoHeader video_header;
  video_header.content_type = VideoContentType::SCREENSHARE;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetContentType(), VideoContentType::SCREENSHARE);
}

TEST(VideoFrameMetadata, GetFrameIdReturnsCorrectValue) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.frame_id = 10;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetFrameId().value(), 10);
}

TEST(VideoFrameMetadata, HasNoFrameIdForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  ASSERT_FALSE(video_header.generic);
  EXPECT_EQ(metadata.GetFrameId(), absl::nullopt);
}

TEST(VideoFrameMetadata, GetSpatialIndexReturnsCorrectValue) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.spatial_index = 2;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetSpatialIndex(), 2);
}

TEST(VideoFrameMetadata, SpatialIndexIsZeroForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  ASSERT_FALSE(video_header.generic);
  EXPECT_EQ(metadata.GetSpatialIndex(), 0);
}

TEST(VideoFrameMetadata, GetTemporalIndexReturnsCorrectValue) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.temporal_index = 3;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetTemporalIndex(), 3);
}

TEST(VideoFrameMetadata, TemporalIndexIsZeroForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  ASSERT_FALSE(video_header.generic);
  EXPECT_EQ(metadata.GetTemporalIndex(), 0);
}

TEST(VideoFrameMetadata, GetFrameDependenciesReturnsCorrectValue) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.dependencies = {5, 6, 7};
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_THAT(metadata.GetFrameDependencies(), ElementsAre(5, 6, 7));
}

TEST(VideoFrameMetadata, FrameDependencyVectorIsEmptyForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  ASSERT_FALSE(video_header.generic);
  EXPECT_THAT(metadata.GetFrameDependencies(), IsEmpty());
}

TEST(VideoFrameMetadata, GetDecodeTargetIndicationsReturnsCorrectValue) {
  RTPVideoHeader video_header;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.decode_target_indications = {DecodeTargetIndication::kSwitch};
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_THAT(metadata.GetDecodeTargetIndications(),
              ElementsAre(DecodeTargetIndication::kSwitch));
}

TEST(VideoFrameMetadata,
     DecodeTargetIndicationsVectorIsEmptyForHeaderWithoutGeneric) {
  RTPVideoHeader video_header;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  ASSERT_FALSE(video_header.generic);
  EXPECT_THAT(metadata.GetDecodeTargetIndications(), IsEmpty());
}

TEST(VideoFrameMetadata, GetIsLastFrameInPictureReturnsCorrectValue) {
  RTPVideoHeader video_header;
  video_header.is_last_frame_in_picture = false;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_FALSE(metadata.GetIsLastFrameInPicture());
}

TEST(VideoFrameMetadata, GetSimulcastIdxReturnsCorrectValue) {
  RTPVideoHeader video_header;
  video_header.simulcastIdx = 123;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetSimulcastIdx(), 123);
}

TEST(VideoFrameMetadata, GetCodecReturnsCorrectValue) {
  RTPVideoHeader video_header;
  video_header.codec = VideoCodecType::kVideoCodecVP9;
  VideoFrameMetadata metadata = video_header.GetAsMetadata();
  EXPECT_EQ(metadata.GetCodec(), VideoCodecType::kVideoCodecVP9);
}

}  // namespace
}  // namespace webrtc
