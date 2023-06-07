/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sender_video_frame_transformer_delegate.h"

#include <utility>

#include "rtc_base/event.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_frame_transformer.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::SaveArg;
using ::testing::WithoutArgs;

class MockRTPVideoFrameSenderInterface : public RTPVideoFrameSenderInterface {
 public:
  MOCK_METHOD(bool,
              SendVideo,
              (int payload_type,
               absl::optional<VideoCodecType> codec_type,
               uint32_t rtp_timestamp,
               int64_t capture_time_ms,
               rtc::ArrayView<const uint8_t> payload,
               RTPVideoHeader video_header,
               absl::optional<int64_t> expected_retransmission_time_ms,
               std::vector<uint32_t> csrcs),
              (override));

  MOCK_METHOD(void,
              SetVideoStructureAfterTransformation,
              (const FrameDependencyStructure* video_structure),
              (override));
  MOCK_METHOD(void,
              SetVideoLayersAllocationAfterTransformation,
              (VideoLayersAllocation allocation),
              (override));
};

class RtpSenderVideoFrameTransformerDelegateTest : public ::testing::Test {
 protected:
  RtpSenderVideoFrameTransformerDelegateTest()
      : frame_transformer_(rtc::make_ref_counted<MockFrameTransformer>()),
        time_controller_(Timestamp::Seconds(0)) {}

  ~RtpSenderVideoFrameTransformerDelegateTest() override = default;

  std::unique_ptr<TransformableFrameInterface> GetTransformableFrame(
      rtc::scoped_refptr<RTPSenderVideoFrameTransformerDelegate> delegate) {
    EncodedImage encoded_image;
    encoded_image.SetEncodedData(EncodedImageBuffer::Create(1));
    std::unique_ptr<TransformableFrameInterface> frame = nullptr;
    EXPECT_CALL(*frame_transformer_, Transform)
        .WillOnce([&](std::unique_ptr<TransformableFrameInterface>
                          frame_to_transform) {
          frame = std::move(frame_to_transform);
        });
    delegate->TransformFrame(
        /*payload_type=*/1, VideoCodecType::kVideoCodecVP8, /*rtp_timestamp=*/2,
        encoded_image, RTPVideoHeader(),
        /*expected_retransmission_time_ms=*/absl::nullopt);
    return frame;
  }

  MockRTPVideoFrameSenderInterface test_sender_;
  rtc::scoped_refptr<MockFrameTransformer> frame_transformer_;
  GlobalSimulatedTimeController time_controller_;
};

TEST_F(RtpSenderVideoFrameTransformerDelegateTest,
       RegisterTransformedFrameCallbackSinkOnInit) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, /*csrcs=*/std::vector<uint32_t>(),
      time_controller_.CreateTaskQueueFactory().get());
  EXPECT_CALL(*frame_transformer_,
              RegisterTransformedFrameSinkCallback(_, 1111));
  delegate->Init();
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest,
       UnregisterTransformedFrameSinkCallbackOnReset) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, /*csrcs=*/std::vector<uint32_t>(),
      time_controller_.CreateTaskQueueFactory().get());
  EXPECT_CALL(*frame_transformer_,
              UnregisterTransformedFrameSinkCallback(1111));
  delegate->Reset();
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest,
       TransformFrameCallsTransform) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, /*csrcs=*/std::vector<uint32_t>(),
      time_controller_.CreateTaskQueueFactory().get());

  EncodedImage encoded_image;
  EXPECT_CALL(*frame_transformer_, Transform);
  delegate->TransformFrame(
      /*payload_type=*/1, VideoCodecType::kVideoCodecVP8, /*rtp_timestamp=*/2,
      encoded_image, RTPVideoHeader(),
      /*expected_retransmission_time_ms=*/absl::nullopt);
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest,
       OnTransformedFrameCallsSenderSendVideo) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, /*csrcs=*/std::vector<uint32_t>(),
      time_controller_.CreateTaskQueueFactory().get());

  rtc::scoped_refptr<TransformedFrameCallback> callback;
  EXPECT_CALL(*frame_transformer_, RegisterTransformedFrameSinkCallback)
      .WillOnce(SaveArg<0>(&callback));
  delegate->Init();
  ASSERT_TRUE(callback);

  std::unique_ptr<TransformableFrameInterface> frame =
      GetTransformableFrame(delegate);
  ASSERT_TRUE(frame);

  rtc::Event event;
  EXPECT_CALL(test_sender_, SendVideo).WillOnce(WithoutArgs([&] {
    event.Set();
    return true;
  }));

  callback->OnTransformedFrame(std::move(frame));

  event.Wait(TimeDelta::Seconds(1));
}

TEST_F(RtpSenderVideoFrameTransformerDelegateTest, CloneSenderVideoFrame) {
  auto delegate = rtc::make_ref_counted<RTPSenderVideoFrameTransformerDelegate>(
      &test_sender_, frame_transformer_,
      /*ssrc=*/1111, /*csrcs=*/std::vector<uint32_t>(),
      time_controller_.CreateTaskQueueFactory().get());

  std::unique_ptr<TransformableFrameInterface> frame =
      GetTransformableFrame(delegate);
  ASSERT_TRUE(frame);

  TransformableVideoFrameInterface* video_frame =
      static_cast<TransformableVideoFrameInterface*>(frame.get());
  std::unique_ptr<TransformableVideoFrameInterface> clone =
      CloneSenderVideoFrame(video_frame);

  EXPECT_EQ(video_frame->IsKeyFrame(), clone->IsKeyFrame());
  EXPECT_EQ(video_frame->GetPayloadType(), clone->GetPayloadType());
  EXPECT_EQ(video_frame->GetSsrc(), clone->GetSsrc());
  EXPECT_EQ(video_frame->GetTimestamp(), clone->GetTimestamp());
  // TODO(bugs.webrtc.org/14708): Expect equality of GetMetadata() once we have
  // an equality operator defined.
}

}  // namespace
}  // namespace webrtc
