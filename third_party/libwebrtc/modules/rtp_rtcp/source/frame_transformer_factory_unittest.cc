/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/frame_transformer_factory.h"

#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/call/transport.h"
#include "call/video_receive_stream.h"
#include "modules/rtp_rtcp/source/rtp_descriptor_authentication.h"
#include "rtc_base/event.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_frame_transformer.h"

namespace webrtc {
namespace {

using testing::NiceMock;
using testing::Return;

class MockTransformableVideoFrame
    : public webrtc::TransformableVideoFrameInterface {
 public:
  MOCK_METHOD(rtc::ArrayView<const uint8_t>, GetData, (), (const override));
  MOCK_METHOD(void, SetData, (rtc::ArrayView<const uint8_t> data), (override));
  MOCK_METHOD(uint8_t, GetPayloadType, (), (const, override));
  MOCK_METHOD(uint32_t, GetSsrc, (), (const, override));
  MOCK_METHOD(uint32_t, GetTimestamp, (), (const, override));
  MOCK_METHOD(TransformableFrameInterface::Direction,
              GetDirection,
              (),
              (const, override));
  MOCK_METHOD(bool, IsKeyFrame, (), (const, override));
  MOCK_METHOD(std::vector<uint8_t>, GetAdditionalData, (), (const, override));
  MOCK_METHOD(const webrtc::VideoFrameMetadata&,
              GetMetadata,
              (),
              (const, override));
  MOCK_METHOD(void,
              SetMetadata,
              (const webrtc::VideoFrameMetadata&),
              (override));
};

TEST(FrameTransformerFactory, CloneVideoFrame) {
  NiceMock<MockTransformableVideoFrame> original_frame;
  uint8_t data[10];
  std::fill_n(data, 10, 5);
  rtc::ArrayView<uint8_t> data_view(data);
  EXPECT_CALL(original_frame, GetData()).WillRepeatedly(Return(data_view));
  auto cloned_frame = CloneVideoFrame(&original_frame);
  EXPECT_EQ(cloned_frame->GetData().size(), 10u);
  EXPECT_THAT(cloned_frame->GetData(), testing::Each(5u));
}

}  // namespace
}  // namespace webrtc
