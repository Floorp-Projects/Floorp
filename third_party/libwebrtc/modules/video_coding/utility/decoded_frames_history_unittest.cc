/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/decoded_frames_history.h"

#include "test/gtest.h"

namespace webrtc {
namespace video_coding {
namespace {

constexpr int kHistorySize = 1 << 13;

TEST(DecodedFramesHistory, RequestOnEmptyHistory) {
  DecodedFramesHistory history(kHistorySize);
  EXPECT_EQ(history.WasDecoded({1234, 0}), false);
}

TEST(DecodedFramesHistory, FindsLastDecodedFrame) {
  DecodedFramesHistory history(kHistorySize);
  history.InsertDecoded({1234, 0}, 0);
  EXPECT_EQ(history.WasDecoded({1234, 0}), true);
}

TEST(DecodedFramesHistory, FindsPreviousFrame) {
  DecodedFramesHistory history(kHistorySize);
  history.InsertDecoded({1234, 0}, 0);
  history.InsertDecoded({1235, 0}, 0);
  EXPECT_EQ(history.WasDecoded({1234, 0}), true);
}

TEST(DecodedFramesHistory, ReportsMissingFrame) {
  DecodedFramesHistory history(kHistorySize);
  history.InsertDecoded({1234, 0}, 0);
  history.InsertDecoded({1236, 0}, 0);
  EXPECT_EQ(history.WasDecoded({1235, 0}), false);
}

TEST(DecodedFramesHistory, ClearsHistory) {
  DecodedFramesHistory history(kHistorySize);
  history.InsertDecoded({1234, 0}, 0);
  history.Clear();
  EXPECT_EQ(history.WasDecoded({1234, 0}), false);
  EXPECT_EQ(history.GetLastDecodedFrameId(), absl::nullopt);
  EXPECT_EQ(history.GetLastDecodedFrameTimestamp(), absl::nullopt);
}

TEST(DecodedFramesHistory, HandlesMultipleLayers) {
  DecodedFramesHistory history(kHistorySize);
  history.InsertDecoded({1234, 0}, 0);
  history.InsertDecoded({1234, 1}, 0);
  history.InsertDecoded({1235, 0}, 0);
  history.InsertDecoded({1236, 0}, 0);
  history.InsertDecoded({1236, 1}, 0);
  EXPECT_EQ(history.WasDecoded({1235, 0}), true);
  EXPECT_EQ(history.WasDecoded({1235, 1}), false);
}

TEST(DecodedFramesHistory, HandlesNewLayer) {
  DecodedFramesHistory history(kHistorySize);
  history.InsertDecoded({1234, 0}, 0);
  history.InsertDecoded({1234, 1}, 0);
  history.InsertDecoded({1235, 0}, 0);
  history.InsertDecoded({1235, 1}, 0);
  history.InsertDecoded({1236, 0}, 0);
  history.InsertDecoded({1236, 1}, 0);
  EXPECT_EQ(history.WasDecoded({1234, 2}), false);
}

TEST(DecodedFramesHistory, HandlesSkippedLayer) {
  DecodedFramesHistory history(kHistorySize);
  history.InsertDecoded({1234, 0}, 0);
  history.InsertDecoded({1234, 2}, 0);
  history.InsertDecoded({1235, 0}, 0);
  history.InsertDecoded({1235, 1}, 0);
  EXPECT_EQ(history.WasDecoded({1234, 1}), false);
  EXPECT_EQ(history.WasDecoded({1235, 1}), true);
}

TEST(DecodedFramesHistory, HandlesBigJumpInPictureId) {
  DecodedFramesHistory history(kHistorySize);
  history.InsertDecoded({1234, 0}, 0);
  history.InsertDecoded({1235, 0}, 0);
  history.InsertDecoded({1236, 0}, 0);
  history.InsertDecoded({1236 + kHistorySize / 2, 0}, 0);
  EXPECT_EQ(history.WasDecoded({1234, 0}), true);
  EXPECT_EQ(history.WasDecoded({1237, 0}), false);
}

TEST(DecodedFramesHistory, ForgetsTooOldHistory) {
  DecodedFramesHistory history(kHistorySize);
  history.InsertDecoded({1234, 0}, 0);
  history.InsertDecoded({1235, 0}, 0);
  history.InsertDecoded({1236, 0}, 0);
  history.InsertDecoded({1236 + kHistorySize * 2, 0}, 0);
  EXPECT_EQ(history.WasDecoded({1234, 0}), false);
  EXPECT_EQ(history.WasDecoded({1237, 0}), false);
}

TEST(DecodedFramesHistory, ReturnsLastDecodedFrameId) {
  DecodedFramesHistory history(kHistorySize);
  EXPECT_EQ(history.GetLastDecodedFrameId(), absl::nullopt);
  history.InsertDecoded({1234, 0}, 0);
  EXPECT_EQ(history.GetLastDecodedFrameId(), VideoLayerFrameId(1234, 0));
  history.InsertDecoded({1235, 0}, 0);
  EXPECT_EQ(history.GetLastDecodedFrameId(), VideoLayerFrameId(1235, 0));
}

TEST(DecodedFramesHistory, ReturnsLastDecodedFrameTimestamp) {
  DecodedFramesHistory history(kHistorySize);
  EXPECT_EQ(history.GetLastDecodedFrameTimestamp(), absl::nullopt);
  history.InsertDecoded({1234, 0}, 12345);
  EXPECT_EQ(history.GetLastDecodedFrameTimestamp(), 12345u);
  history.InsertDecoded({1235, 0}, 12366);
  EXPECT_EQ(history.GetLastDecodedFrameTimestamp(), 12366u);
}

TEST(DecodedFramesHistory, NegativePictureIds) {
  DecodedFramesHistory history(kHistorySize);
  history.InsertDecoded({-1234, 0}, 12345);
  history.InsertDecoded({-1233, 0}, 12366);
  EXPECT_EQ(history.GetLastDecodedFrameId()->picture_id, -1233);

  history.InsertDecoded({-1, 0}, 12377);
  history.InsertDecoded({0, 0}, 12388);
  EXPECT_EQ(history.GetLastDecodedFrameId()->picture_id, 0);

  history.InsertDecoded({1, 0}, 12399);
  EXPECT_EQ(history.GetLastDecodedFrameId()->picture_id, 1);

  EXPECT_EQ(history.WasDecoded({-1234, 0}), true);
  EXPECT_EQ(history.WasDecoded({-1, 0}), true);
  EXPECT_EQ(history.WasDecoded({0, 0}), true);
  EXPECT_EQ(history.WasDecoded({1, 0}), true);
}

}  // namespace
}  // namespace video_coding
}  // namespace webrtc
