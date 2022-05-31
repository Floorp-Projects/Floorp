/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace vp9 {

TEST(Vp9UncompressedHeaderParserTest, FrameWithSegmentation) {
  // Uncompressed header from a frame generated with libvpx.
  // Encoded QVGA frame (SL0 of a VGA frame) that includes a segmentation.
  const uint8_t kHeader[] = {
      0x87, 0x01, 0x00, 0x00, 0x02, 0x7e, 0x01, 0xdf, 0x02, 0x7f, 0x01, 0xdf,
      0xc6, 0x87, 0x04, 0x83, 0x83, 0x2e, 0x46, 0x60, 0x20, 0x38, 0x0c, 0x06,
      0x03, 0xcd, 0x80, 0xc0, 0x60, 0x9f, 0xc5, 0x46, 0x00, 0x00, 0x00, 0x00,
      0x2e, 0x73, 0xb7, 0xee, 0x22, 0x06, 0x81, 0x82, 0xd4, 0xef, 0xc3, 0x58,
      0x1f, 0x12, 0xd2, 0x7b, 0x28, 0x1f, 0x80, 0xfc, 0x07, 0xe0, 0x00, 0x00};

  absl::optional<FrameInfo> frame_info =
      ParseIntraFrameInfo(kHeader, sizeof(kHeader));
  // Segmentation info is not actually populated in FrameInfo struct, but it
  // needs to be parsed otherwise we end up on the wrong offset. The check for
  // segmentation is thus that we have a valid return value.
  ASSERT_TRUE(frame_info.has_value());

  EXPECT_EQ(frame_info->is_keyframe, false);
  EXPECT_EQ(frame_info->error_resilient, true);
  EXPECT_EQ(frame_info->show_frame, true);
  EXPECT_EQ(frame_info->base_qp, 185);
  EXPECT_EQ(frame_info->frame_width, 320);
  EXPECT_EQ(frame_info->frame_height, 240);
  EXPECT_EQ(frame_info->render_width, 640);
  EXPECT_EQ(frame_info->render_height, 480);
}

}  // namespace vp9
}  // namespace webrtc
