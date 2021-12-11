/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "test/gtest.h"
#include "test/gmock.h"

#include "api/video_codecs/video_decoder.h"
#include "call/rtp_stream_receiver_controller.h"
#include "media/base/fakevideorenderer.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "system_wrappers/include/clock.h"
#include "test/field_trial.h"
#include "video/call_stats.h"
#include "video/video_receive_stream.h"

namespace webrtc {
namespace {

using testing::_;
using testing::Invoke;

constexpr int kDefaultTimeOutMs = 50;

const char kNewJitterBufferFieldTrialEnabled[] =
    "WebRTC-NewVideoJitterBuffer/Enabled/";

class MockTransport : public Transport {
 public:
  MOCK_METHOD3(SendRtp,
               bool(const uint8_t* packet,
                    size_t length,
                    const PacketOptions& options));
  MOCK_METHOD2(SendRtcp, bool(const uint8_t* packet, size_t length));
};

class MockVideoDecoder : public VideoDecoder {
 public:
  MOCK_METHOD2(InitDecode,
               int32_t(const VideoCodec* config, int32_t number_of_cores));
  MOCK_METHOD5(Decode,
               int32_t(const EncodedImage& input,
                       bool missing_frames,
                       const RTPFragmentationHeader* fragmentation,
                       const CodecSpecificInfo* codec_specific_info,
                       int64_t render_time_ms));
  MOCK_METHOD1(RegisterDecodeCompleteCallback,
               int32_t(DecodedImageCallback* callback));
  MOCK_METHOD0(Release, int32_t(void));
  const char* ImplementationName() const { return "MockVideoDecoder"; }
};

}  // namespace

class VideoReceiveStreamTest : public testing::Test {
 public:
  VideoReceiveStreamTest()
      : override_field_trials_(kNewJitterBufferFieldTrialEnabled),
        config_(&mock_transport_),
        call_stats_(Clock::GetRealTimeClock()),
        process_thread_(ProcessThread::Create("TestThread")) {}

  void SetUp() {
    constexpr int kDefaultNumCpuCores = 2;
    config_.rtp.remote_ssrc = 1111;
    config_.rtp.local_ssrc = 2222;
    config_.renderer = &fake_renderer_;
    VideoReceiveStream::Decoder h264_decoder;
    h264_decoder.payload_type = 99;
    h264_decoder.payload_name = "H264";
    h264_decoder.codec_params.insert(
        {"sprop-parameter-sets", "Z0IACpZTBYmI,aMljiA=="});
    h264_decoder.decoder = &mock_h264_video_decoder_;
    config_.decoders.push_back(h264_decoder);
    VideoReceiveStream::Decoder null_decoder;
    null_decoder.payload_type = 98;
    null_decoder.payload_name = "null";
    null_decoder.decoder = &mock_null_video_decoder_;
    config_.decoders.push_back(null_decoder);

    video_receive_stream_.reset(new webrtc::internal::VideoReceiveStream(
        &rtp_stream_receiver_controller_, kDefaultNumCpuCores,
        &packet_router_, config_.Copy(), process_thread_.get(), &call_stats_));
  }

 protected:
  webrtc::test::ScopedFieldTrials override_field_trials_;
  VideoReceiveStream::Config config_;
  CallStats call_stats_;
  MockVideoDecoder mock_h264_video_decoder_;
  MockVideoDecoder mock_null_video_decoder_;
  cricket::FakeVideoRenderer fake_renderer_;
  MockTransport mock_transport_;
  PacketRouter packet_router_;
  std::unique_ptr<ProcessThread> process_thread_;
  RtpStreamReceiverController rtp_stream_receiver_controller_;
  std::unique_ptr<webrtc::internal::VideoReceiveStream> video_receive_stream_;
};

TEST_F(VideoReceiveStreamTest, CreateFrameFromH264FmtpSpropAndIdr) {
  constexpr uint8_t idr_nalu[] = {0x05, 0xFF, 0xFF, 0xFF};
  RtpPacketToSend rtppacket(nullptr);
  uint8_t* payload = rtppacket.AllocatePayload(sizeof(idr_nalu));
  memcpy(payload, idr_nalu, sizeof(idr_nalu));
  rtppacket.SetMarker(true);
  rtppacket.SetSsrc(1111);
  rtppacket.SetPayloadType(99);
  rtppacket.SetSequenceNumber(1);
  rtppacket.SetTimestamp(0);
  rtc::Event init_decode_event_(false, false);
  EXPECT_CALL(mock_h264_video_decoder_, InitDecode(_, _))
      .WillOnce(Invoke([&init_decode_event_](const VideoCodec* config,
                                             int32_t number_of_cores) {
        init_decode_event_.Set();
        return 0;
      }));
  EXPECT_CALL(mock_h264_video_decoder_, RegisterDecodeCompleteCallback(_));
  video_receive_stream_->Start();
  EXPECT_CALL(mock_h264_video_decoder_, Decode(_, false, _, _, _));
  RtpPacketReceived parsed_packet;
  ASSERT_TRUE(parsed_packet.Parse(rtppacket.data(), rtppacket.size()));
  rtp_stream_receiver_controller_.OnRtpPacket(parsed_packet);
  EXPECT_CALL(mock_h264_video_decoder_, Release());
  // Make sure the decoder thread had a chance to run.
  init_decode_event_.Wait(kDefaultTimeOutMs);
}

}  // namespace webrtc
