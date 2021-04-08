
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

#include "nspr.h"
#include "nss.h"
#include "ssl.h"

#include "VideoConduit.h"
#include "RtpRtcpConfig.h"
#include "WebrtcCallWrapper.h"
#include "WebrtcGmpVideoCodec.h"

#include "api/video/video_sink_interface.h"
#include "media/base/video_adapter.h"

#include "MockCall.h"

using namespace mozilla;

namespace test {

class MockVideoSink : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  MockVideoSink() : mVideoFrame(nullptr, kVideoRotation_0, 0) {}

  ~MockVideoSink() override = default;

  void OnFrame(const webrtc::VideoFrame& frame) override {
    mVideoFrame = frame;
    ++mOnFrameCount;
  }

  size_t mOnFrameCount = 0;
  webrtc::VideoFrame mVideoFrame;
};

class VideoConduitTest : public ::testing::Test {
 public:
  VideoConduitTest()
      : mCall(new MockCall()),
        mCallWrapper(WebrtcCallWrapper::Create(UniquePtr<MockCall>(mCall))),
        mVideoConduit(MakeRefPtr<WebrtcVideoConduit>(
            mCallWrapper, GetCurrentSerialEventTarget(), "")) {
    NSS_NoDB_Init(nullptr);

    mVideoConduit->SetLocalSSRCs({42}, {43});
  }

  ~VideoConduitTest() override {
    mVideoConduit->DeleteStreams();
    mCallWrapper->Destroy();
  }

  MediaConduitErrorCode SendVideoFrame(unsigned short width,
                                       unsigned short height,
                                       uint64_t capture_time_ms) {
    rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        webrtc::I420Buffer::Create(width, height);
    memset(buffer->MutableDataY(), 0x10, buffer->StrideY() * buffer->height());
    memset(buffer->MutableDataU(), 0x80,
           buffer->StrideU() * ((buffer->height() + 1) / 2));
    memset(buffer->MutableDataV(), 0x80,
           buffer->StrideV() * ((buffer->height() + 1) / 2));

    webrtc::VideoFrame frame(buffer, capture_time_ms, capture_time_ms,
                             webrtc::kVideoRotation_0);
    return mVideoConduit->SendVideoFrame(frame);
  }

  MockCall* const mCall;
  const RefPtr<WebrtcCallWrapper> mCallWrapper;
  const RefPtr<mozilla::WebrtcVideoConduit> mVideoConduit;
};

TEST_F(VideoConduitTest, TestConfigureReceiveMediaCodecs) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  // Defaults
  std::vector<UniquePtr<mozilla::VideoCodecConfig>> codecs;
  codecs.emplace_back(new VideoCodecConfig(120, "VP8", constraints));
  ec = mVideoConduit->ConfigureRecvMediaCodecs(codecs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders.size(), 1U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].payload_type, 120);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].video_format.name, "VP8");
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.local_ssrc, 0U);
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.remote_ssrc, 0U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.nack.rtp_history_ms, 0);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.remb);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.tmmbr);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.red_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtx_associated_payload_types.size(),
            0U);

  // No codecs
  codecs.clear();
  ec = mVideoConduit->ConfigureRecvMediaCodecs(codecs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);

  // null codec
  codecs.clear();
  codecs.push_back(nullptr);
  ec = mVideoConduit->ConfigureRecvMediaCodecs(codecs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);

  // empty codec name
  codecs.clear();
  codecs.emplace_back(new VideoCodecConfig(120, "", constraints));
  ec = mVideoConduit->ConfigureRecvMediaCodecs(codecs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);
}

TEST_F(VideoConduitTest, TestConfigureReceiveMediaCodecsFEC) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  std::vector<UniquePtr<mozilla::VideoCodecConfig>> codecs;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  UniquePtr<VideoCodecConfig> codecConfig(
      new VideoCodecConfig(120, "VP8", constraints));
  codecConfig->mFECFbSet = true;
  codecs.push_back(std::move(codecConfig));
  codecs.emplace_back(new VideoCodecConfig(1, "ulpfec", constraints));
  codecs.emplace_back(new VideoCodecConfig(2, "red", constraints));

  ec = mVideoConduit->ConfigureRecvMediaCodecs(codecs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders.size(), 1U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].payload_type, 120);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].video_format.name, "VP8");
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.local_ssrc, 0U);
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.remote_ssrc, 0U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.nack.rtp_history_ms, 0);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.remb);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.tmmbr);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.ulpfec_payload_type, 1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.red_payload_type, 2);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtx_associated_payload_types.size(),
            0U);
}

TEST_F(VideoConduitTest, TestConfigureReceiveMediaCodecsH264) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  std::vector<UniquePtr<mozilla::VideoCodecConfig>> codecs;
  codecs.emplace_back(new VideoCodecConfig(120, "H264", constraints));
  // Insert twice to test that only one H264 codec is used at a time
  codecs.emplace_back(new VideoCodecConfig(120, "H264", constraints));

  ec = mVideoConduit->ConfigureRecvMediaCodecs(codecs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders.size(), 1U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].payload_type, 120);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].video_format.name, "H264");
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.local_ssrc, 0U);
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.remote_ssrc, 0U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.nack.rtp_history_ms, 0);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.remb);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.tmmbr);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.red_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtx_associated_payload_types.size(),
            0U);
}

TEST_F(VideoConduitTest, TestConfigureReceiveMediaCodecsKeyframeRequestType) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  std::vector<UniquePtr<mozilla::VideoCodecConfig>> codecs;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  // PLI should be preferred to FIR
  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mNackFbTypes.push_back("pli");
  codecConfig.mCcmFbTypes.push_back("fir");
  codecs.emplace_back(new VideoCodecConfig(codecConfig));

  ec = mVideoConduit->ConfigureRecvMediaCodecs(codecs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders.size(), 1U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kPliRtcp);

  // Just FIR
  codecs.clear();
  codecConfig.mNackFbTypes.clear();
  codecs.emplace_back(new VideoCodecConfig(codecConfig));
  ec = mVideoConduit->ConfigureRecvMediaCodecs(codecs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders.size(), 1U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kFirRtcp);
}

TEST_F(VideoConduitTest, TestConfigureReceiveMediaCodecsNack) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  std::vector<UniquePtr<mozilla::VideoCodecConfig>> codecs;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mNackFbTypes.push_back("");
  codecs.emplace_back(new VideoCodecConfig(codecConfig));

  ec = mVideoConduit->ConfigureRecvMediaCodecs(codecs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders.size(), 1U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].payload_type, 120);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].video_format.name, "VP8");
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.local_ssrc, 0U);
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.remote_ssrc, 0U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.nack.rtp_history_ms, 1000);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.remb);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.tmmbr);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.red_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtx_associated_payload_types.size(),
            0U);
}

TEST_F(VideoConduitTest, TestConfigureReceiveMediaCodecsRemb) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  std::vector<UniquePtr<mozilla::VideoCodecConfig>> codecs;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mRembFbSet = true;
  codecs.emplace_back(new VideoCodecConfig(codecConfig));

  ec = mVideoConduit->ConfigureRecvMediaCodecs(codecs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders.size(), 1U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].payload_type, 120);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].video_format.name, "VP8");
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.local_ssrc, 0U);
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.remote_ssrc, 0U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.nack.rtp_history_ms, 0);
  ASSERT_TRUE(mCall->mVideoReceiveConfig.rtp.remb);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.tmmbr);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.red_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtx_associated_payload_types.size(),
            0U);
}

TEST_F(VideoConduitTest, TestConfigureReceiveMediaCodecsTmmbr) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  std::vector<UniquePtr<mozilla::VideoCodecConfig>> codecs;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mCcmFbTypes.push_back("tmmbr");
  codecs.emplace_back(new VideoCodecConfig(codecConfig));

  ec = mVideoConduit->ConfigureRecvMediaCodecs(codecs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders.size(), 1U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].payload_type, 120);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].video_format.name, "VP8");
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.local_ssrc, 0U);
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.remote_ssrc, 0U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.nack.rtp_history_ms, 0);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.remb);
  ASSERT_TRUE(mCall->mVideoReceiveConfig.rtp.tmmbr);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.red_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtx_associated_payload_types.size(),
            0U);
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodec) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  // defaults
  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.payload_name, "VP8");
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.payload_type, 120);
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.rtcp_mode, webrtc::RtcpMode::kCompound);
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.max_packet_size, kVideoMtu);
  ASSERT_EQ(mCall->mCurrentVideoSendStream->mEncoderConfig.content_type,
            VideoEncoderConfig::ContentType::kRealtimeVideo);
  ASSERT_EQ(
      mCall->mCurrentVideoSendStream->mEncoderConfig.min_transmit_bitrate_bps,
      0);
  ASSERT_EQ(mCall->mCurrentVideoSendStream->mEncoderConfig.max_bitrate_bps,
            KBPS(10000));
  ASSERT_EQ(mCall->mCurrentVideoSendStream->mEncoderConfig.number_of_streams,
            1U);
  mVideoConduit->StopTransmitting();

  // null codec
  ec = mVideoConduit->ConfigureSendMediaCodec(nullptr, rtpConf);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);

  // empty codec name
  VideoCodecConfig codecConfigBadName(120, "", constraints);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigBadName, rtpConf);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecMaxFps) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  constraints.maxFps = 0;
  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  std::vector<webrtc::VideoStream> videoStreams;
  videoStreams = mCall->CreateEncoderStreams(640, 480);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].max_framerate, 30);  // DEFAULT_VIDEO_MAX_FRAMERATE
  mVideoConduit->StopTransmitting();

  constraints.maxFps = 42;
  VideoCodecConfig codecConfig2(120, "VP8", constraints);
  codecConfig2.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig2, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  videoStreams = mCall->CreateEncoderStreams(640, 480);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].max_framerate, 42);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecMaxMbps) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  constraints.maxMbps = 0;
  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  SendVideoFrame(640, 480, 1);
  std::vector<webrtc::VideoStream> videoStreams;
  videoStreams = mCall->CreateEncoderStreams(640, 480);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].max_framerate, 30);  // DEFAULT_VIDEO_MAX_FRAMERATE
  mVideoConduit->StopTransmitting();

  constraints.maxMbps = 10000;
  VideoCodecConfig codecConfig2(120, "VP8", constraints);
  codecConfig2.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig2, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  SendVideoFrame(640, 480, 1);
  videoStreams = mCall->CreateEncoderStreams(640, 480);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].max_framerate, 8);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecDefaults) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  videoStreams = mCall->CreateEncoderStreams(640, 480);
  EXPECT_EQ(videoStreams.size(), 1U);
  EXPECT_EQ(videoStreams[0].min_bitrate_bps, 150000);
  EXPECT_EQ(videoStreams[0].target_bitrate_bps, 500000);
  EXPECT_EQ(videoStreams[0].max_bitrate_bps, 2000000);

  // SelectBitrates not called until we send a frame
  SendVideoFrame(1280, 720, 1);
  videoStreams = mCall->CreateEncoderStreams(1280, 720);
  EXPECT_EQ(videoStreams.size(), 1U);
  EXPECT_EQ(videoStreams[0].min_bitrate_bps, 200000);
  EXPECT_EQ(videoStreams[0].target_bitrate_bps, 800000);
  EXPECT_EQ(videoStreams[0].max_bitrate_bps, 2500000);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecTias) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  // TIAS
  VideoCodecConfig codecConfigTias(120, "VP8", constraints);
  codecConfigTias.mEncodings.push_back(encoding);
  codecConfigTias.mTias = 1000000;
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigTias, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  SendVideoFrame(1280, 720, 1);
  videoStreams = mCall->CreateEncoderStreams(1280, 720);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].min_bitrate_bps, 200000);
  ASSERT_EQ(videoStreams[0].target_bitrate_bps, 800000);
  ASSERT_EQ(videoStreams[0].max_bitrate_bps, 1000000);
  mVideoConduit->StopTransmitting();

  // TIAS (too low)
  VideoCodecConfig codecConfigTiasLow(120, "VP8", constraints);
  codecConfigTiasLow.mEncodings.push_back(encoding);
  codecConfigTiasLow.mTias = 1000;
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigTiasLow, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  SendVideoFrame(1280, 720, 1);
  videoStreams = mCall->CreateEncoderStreams(1280, 720);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].min_bitrate_bps, 30000);
  ASSERT_EQ(videoStreams[0].target_bitrate_bps, 30000);
  ASSERT_EQ(videoStreams[0].max_bitrate_bps, 30000);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecMaxBr) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  encoding.constraints.maxBr = 50000;
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  SendVideoFrame(1280, 720, 1);
  videoStreams = mCall->CreateEncoderStreams(1280, 720);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_LE(videoStreams[0].min_bitrate_bps, 50000);
  ASSERT_LE(videoStreams[0].target_bitrate_bps, 50000);
  ASSERT_EQ(videoStreams[0].max_bitrate_bps, 50000);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecScaleResolutionBy) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  mVideoConduit->SetLocalSSRCs({42, 1729}, {43, 1730});

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  encoding.constraints.scaleDownBy = 2;
  codecConfig.mEncodings.push_back(encoding);
  encoding.constraints.scaleDownBy = 4;
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  mVideoConduit->StartTransmitting();
  SendVideoFrame(640, 360, 1);
  videoStreams = mCall->CreateEncoderStreams(sink->mVideoFrame.width(),
                                             sink->mVideoFrame.height());
  ASSERT_EQ(videoStreams.size(), 2U);
  ASSERT_EQ(videoStreams[0].width, 160U);
  ASSERT_EQ(videoStreams[0].height, 88U);
  ASSERT_EQ(videoStreams[1].width, 320U);
  ASSERT_EQ(videoStreams[1].height, 176U);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecCodecMode) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  mVideoConduit->ConfigureCodecMode(webrtc::VideoCodecMode::kScreensharing);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mCurrentVideoSendStream->mEncoderConfig.content_type,
            VideoEncoderConfig::ContentType::kScreen);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecFEC) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  {
    // H264 + FEC
    VideoCodecConfig codecConfig(120, "H264", constraints);
    codecConfig.mEncodings.push_back(encoding);
    codecConfig.mFECFbSet = true;
    codecConfig.mULPFECPayloadType = 1;
    codecConfig.mREDPayloadType = 2;
    codecConfig.mREDRTXPayloadType = 3;
    ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
    ASSERT_EQ(ec, kMediaConduitNoError);
    mVideoConduit->StartTransmitting();
    ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.ulpfec_payload_type,
              codecConfig.mULPFECPayloadType);
    ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.red_payload_type,
              codecConfig.mREDPayloadType);
    ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.red_rtx_payload_type,
              codecConfig.mREDRTXPayloadType);
    mVideoConduit->StopTransmitting();
  }

  {
    // H264 + FEC + Nack
    VideoCodecConfig codecConfig(120, "H264", constraints);
    codecConfig.mEncodings.push_back(encoding);
    codecConfig.mFECFbSet = true;
    codecConfig.mNackFbTypes.push_back("");
    codecConfig.mULPFECPayloadType = 1;
    codecConfig.mREDPayloadType = 2;
    codecConfig.mREDRTXPayloadType = 3;
    ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
    ASSERT_EQ(ec, kMediaConduitNoError);
    mVideoConduit->StartTransmitting();
    ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.ulpfec_payload_type, -1);
    ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.red_payload_type, -1);
    ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.red_rtx_payload_type, -1);
    mVideoConduit->StopTransmitting();
  }

  {
    // VP8 + FEC + Nack
    VideoCodecConfig codecConfig(120, "VP8", constraints);
    codecConfig.mEncodings.push_back(encoding);
    codecConfig.mFECFbSet = true;
    codecConfig.mNackFbTypes.push_back("");
    codecConfig.mULPFECPayloadType = 1;
    codecConfig.mREDPayloadType = 2;
    codecConfig.mREDRTXPayloadType = 3;
    ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
    ASSERT_EQ(ec, kMediaConduitNoError);
    mVideoConduit->StartTransmitting();
    ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.ulpfec_payload_type,
              codecConfig.mULPFECPayloadType);
    ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.red_payload_type,
              codecConfig.mREDPayloadType);
    ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.red_rtx_payload_type,
              codecConfig.mREDRTXPayloadType);
    mVideoConduit->StopTransmitting();
  }
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecNack) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.nack.rtp_history_ms, 0);
  mVideoConduit->StopTransmitting();

  codecConfig.mNackFbTypes.push_back("");
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.nack.rtp_history_ms, 1000);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecRids) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.rids.size(), 0U);
  mVideoConduit->StopTransmitting();

  mVideoConduit->SetLocalSSRCs({42, 1729}, {43, 1730});

  codecConfig.mEncodings.clear();
  encoding.rid = "1";
  codecConfig.mEncodings.push_back(encoding);
  encoding.rid = "2";
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.rids.size(), 2U);
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.rids[0], "2");
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.rids[1], "1");
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestOnSinkWantsChanged) {
  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  wants.max_pixel_count = 256000;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  std::vector<webrtc::VideoStream> videoStreams;

  codecConfig.mEncodingConstraints.maxFs = 0;
  mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  mVideoConduit->StartTransmitting();
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);
  SendVideoFrame(1920, 1080, 1);
  EXPECT_LE(sink->mVideoFrame.width() * sink->mVideoFrame.height(), 256000);
  videoStreams = mCall->CreateEncoderStreams(sink->mVideoFrame.width(),
                                             sink->mVideoFrame.height());
  ASSERT_EQ(videoStreams.size(), 1U);
  EXPECT_EQ(videoStreams[0].width, 480U);
  EXPECT_EQ(videoStreams[0].height, 270U);

  codecConfig.mEncodingConstraints.maxFs = 500;
  mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);
  SendVideoFrame(1920, 1080, 2);
  EXPECT_LE(sink->mVideoFrame.width() * sink->mVideoFrame.height(),
            500 * 16 * 16);
  videoStreams = mCall->CreateEncoderStreams(sink->mVideoFrame.width(),
                                             sink->mVideoFrame.height());
  ASSERT_EQ(videoStreams.size(), 1U);
  EXPECT_EQ(videoStreams[0].width, 360U);
  EXPECT_EQ(videoStreams[0].height, 201U);

  codecConfig.mEncodingConstraints.maxFs = 1000;
  mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);
  SendVideoFrame(1920, 1080, 3);
  EXPECT_LE(sink->mVideoFrame.width() * sink->mVideoFrame.height(),
            1000 * 16 * 16);
  videoStreams = mCall->CreateEncoderStreams(sink->mVideoFrame.width(),
                                             sink->mVideoFrame.height());
  ASSERT_EQ(videoStreams.size(), 1U);
  EXPECT_EQ(videoStreams[0].width, 480U);
  EXPECT_EQ(videoStreams[0].height, 270U);

  wants.max_pixel_count = 64000;
  codecConfig.mEncodingConstraints.maxFs = 500;
  mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);
  SendVideoFrame(1920, 1080, 4);
  EXPECT_LE(sink->mVideoFrame.width() * sink->mVideoFrame.height(), 64000);
  videoStreams = mCall->CreateEncoderStreams(sink->mVideoFrame.width(),
                                             sink->mVideoFrame.height());
  ASSERT_EQ(videoStreams.size(), 1U);
  EXPECT_EQ(videoStreams[0].width, 240U);
  EXPECT_EQ(videoStreams[0].height, 135U);

  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecSimulcastOddScreen) {
  mVideoConduit->SetLocalSSRCs({42, 43, 44}, {45, 46, 47});
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  VideoCodecConfig::Encoding encoding2;
  VideoCodecConfig::Encoding encoding3;
  encoding2.constraints.scaleDownBy = 2;
  encoding3.constraints.scaleDownBy = 4;

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  codecConfig.mEncodings.push_back(encoding2);
  codecConfig.mEncodings.push_back(encoding3);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);
  mVideoConduit->StartTransmitting();

  // This should crop to 16-alignment to help with scaling
  SendVideoFrame(26, 24, 1);
  std::vector<webrtc::VideoStream> videoStreams;
  videoStreams = mCall->CreateEncoderStreams(sink->mVideoFrame.width(),
                                             sink->mVideoFrame.height());
  ASSERT_EQ(videoStreams.size(), 3U);
  EXPECT_EQ(videoStreams[2].width, 16U);
  EXPECT_EQ(videoStreams[2].height, 16U);
  EXPECT_EQ(videoStreams[1].width, 8U);
  EXPECT_EQ(videoStreams[1].height, 8U);
  EXPECT_EQ(videoStreams[0].width, 4U);
  EXPECT_EQ(videoStreams[0].height, 4U);

  // Test that we are able to remove the 16-alignment cropping (non-simulcast)
  codecConfig.mEncodings.clear();
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  mVideoConduit->SetLocalSSRCs({42}, {43});
  ASSERT_EQ(ec, kMediaConduitNoError);
  SendVideoFrame(26, 24, 2);
  videoStreams = mCall->CreateEncoderStreams(sink->mVideoFrame.width(),
                                             sink->mVideoFrame.height());
  ASSERT_EQ(videoStreams.size(), 1U);
  EXPECT_EQ(videoStreams[0].width, 26U);
  EXPECT_EQ(videoStreams[0].height, 24U);

  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecSimulcastAllScaling) {
  mVideoConduit->SetLocalSSRCs({42, 43, 44}, {45, 46, 47});
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  VideoCodecConfig::Encoding encoding2;
  VideoCodecConfig::Encoding encoding3;
  encoding.constraints.scaleDownBy = 2;
  encoding2.constraints.scaleDownBy = 4;
  encoding3.constraints.scaleDownBy = 6;

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  codecConfig.mEncodings.push_back(encoding2);
  codecConfig.mEncodings.push_back(encoding3);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  mVideoConduit->StartTransmitting();
  std::vector<webrtc::VideoStream> videoStreams;

  SendVideoFrame(1281, 721, 1);
  videoStreams = mCall->CreateEncoderStreams(sink->mVideoFrame.width(),
                                             sink->mVideoFrame.height());
  ASSERT_EQ(videoStreams.size(), 3U);
  EXPECT_EQ(videoStreams[2].width, 640U);
  EXPECT_EQ(videoStreams[2].height, 352U);
  EXPECT_EQ(videoStreams[1].width, 320U);
  EXPECT_EQ(videoStreams[1].height, 176U);
  EXPECT_EQ(videoStreams[0].width, 160U);
  EXPECT_EQ(videoStreams[0].height, 88U);

  SendVideoFrame(1281, 721, 2);
  videoStreams = mCall->CreateEncoderStreams(sink->mVideoFrame.width(),
                                             sink->mVideoFrame.height());
  ASSERT_EQ(videoStreams.size(), 3U);
  EXPECT_EQ(videoStreams[2].width, 640U);
  EXPECT_EQ(videoStreams[2].height, 352U);
  EXPECT_EQ(videoStreams[1].width, 320U);
  EXPECT_EQ(videoStreams[1].height, 176U);
  EXPECT_EQ(videoStreams[0].width, 160U);
  EXPECT_EQ(videoStreams[0].height, 88U);

  SendVideoFrame(1280, 720, 3);
  videoStreams = mCall->CreateEncoderStreams(sink->mVideoFrame.width(),
                                             sink->mVideoFrame.height());
  ASSERT_EQ(videoStreams.size(), 3U);
  EXPECT_EQ(videoStreams[2].width, 640U);
  EXPECT_EQ(videoStreams[2].height, 352U);
  EXPECT_EQ(videoStreams[1].width, 320U);
  EXPECT_EQ(videoStreams[1].height, 176U);
  EXPECT_EQ(videoStreams[0].width, 160U);
  EXPECT_EQ(videoStreams[0].height, 88U);

  codecConfig.mEncodings[0].constraints.scaleDownBy = 1;
  codecConfig.mEncodings[1].constraints.scaleDownBy = 2;
  codecConfig.mEncodings[2].constraints.scaleDownBy = 4;
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  SendVideoFrame(1280, 720, 4);
  videoStreams = mCall->CreateEncoderStreams(sink->mVideoFrame.width(),
                                             sink->mVideoFrame.height());
  ASSERT_EQ(videoStreams.size(), 3U);
  EXPECT_EQ(videoStreams[2].width, 1280U);
  EXPECT_EQ(videoStreams[2].height, 720U);
  EXPECT_EQ(videoStreams[1].width, 640U);
  EXPECT_EQ(videoStreams[1].height, 360U);
  EXPECT_EQ(videoStreams[0].width, 320U);
  EXPECT_EQ(videoStreams[0].height, 180U);

  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecSimulcastScreenshare) {
  mVideoConduit->SetLocalSSRCs({42, 43, 44}, {45, 46, 47});
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  VideoCodecConfig::Encoding encoding2;
  VideoCodecConfig::Encoding encoding3;
  encoding2.constraints.scaleDownBy = 2;
  encoding3.constraints.scaleDownBy = 4;

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  codecConfig.mEncodings.push_back(encoding2);
  codecConfig.mEncodings.push_back(encoding3);
  ec =
      mVideoConduit->ConfigureCodecMode(webrtc::VideoCodecMode::kScreensharing);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  std::vector<webrtc::VideoStream> videoStreams;
  videoStreams = mCall->CreateEncoderStreams(640, 480);
  ASSERT_EQ(videoStreams.size(), 1U);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestReconfigureReceiveMediaCodecs) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  std::vector<UniquePtr<mozilla::VideoCodecConfig>> codecs;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  // Defaults
  codecs.emplace_back(new VideoCodecConfig(120, "VP8", constraints));
  ec = mVideoConduit->ConfigureRecvMediaCodecs(codecs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders.size(), 1U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].payload_type, 120);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].video_format.name, "VP8");
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.local_ssrc, 0U);
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.remote_ssrc, 0U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.nack.rtp_history_ms, 0);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.remb);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.tmmbr);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.red_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtx_associated_payload_types.size(),
            0U);

  // FEC
  codecs.clear();
  VideoCodecConfig codecConfigFecFb(120, "VP8", constraints);
  codecConfigFecFb.mFECFbSet = true;
  codecs.emplace_back(new VideoCodecConfig(codecConfigFecFb));
  VideoCodecConfig codecConfigFEC(1, "ulpfec", constraints);
  codecs.emplace_back(new VideoCodecConfig(codecConfigFEC));
  VideoCodecConfig codecConfigRED(2, "red", constraints);
  codecs.emplace_back(new VideoCodecConfig(codecConfigRED));

  ec = mVideoConduit->ConfigureRecvMediaCodecs(codecs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders.size(), 1U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].payload_type, 120);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].video_format.name, "VP8");
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.local_ssrc, 0U);
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.remote_ssrc, 0U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.nack.rtp_history_ms, 0);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.remb);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.tmmbr);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.ulpfec_payload_type, 1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.red_payload_type, 2);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtx_associated_payload_types.size(),
            0U);

  // H264
  codecs.clear();
  codecs.emplace_back(new VideoCodecConfig(120, "H264", constraints));

  ec = mVideoConduit->ConfigureRecvMediaCodecs(codecs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders.size(), 1U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].payload_type, 120);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].video_format.name, "H264");
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.local_ssrc, 0U);
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.remote_ssrc, 0U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.nack.rtp_history_ms, 0);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.remb);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.tmmbr);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.red_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtx_associated_payload_types.size(),
            0U);

  // Nack
  codecs.clear();
  VideoCodecConfig codecConfigNack(120, "VP8", constraints);
  codecConfigNack.mNackFbTypes.push_back("");
  codecs.emplace_back(new VideoCodecConfig(codecConfigNack));

  ec = mVideoConduit->ConfigureRecvMediaCodecs(codecs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders.size(), 1U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].payload_type, 120);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].video_format.name, "VP8");
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.local_ssrc, 0U);
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.remote_ssrc, 0U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.nack.rtp_history_ms, 1000);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.remb);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.tmmbr);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.red_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtx_associated_payload_types.size(),
            0U);

  // Remb
  codecs.clear();
  VideoCodecConfig codecConfigRemb(120, "VP8", constraints);
  codecConfigRemb.mRembFbSet = true;
  codecs.emplace_back(new VideoCodecConfig(codecConfigRemb));

  ec = mVideoConduit->ConfigureRecvMediaCodecs(codecs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders.size(), 1U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].payload_type, 120);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].video_format.name, "VP8");
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.local_ssrc, 0U);
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.remote_ssrc, 0U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.nack.rtp_history_ms, 0);
  ASSERT_TRUE(mCall->mVideoReceiveConfig.rtp.remb);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.tmmbr);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.red_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtx_associated_payload_types.size(),
            0U);

  // Tmmbr
  codecs.clear();
  VideoCodecConfig codecConfigTmmbr(120, "VP8", constraints);
  codecConfigTmmbr.mCcmFbTypes.push_back("tmmbr");
  codecs.emplace_back(new VideoCodecConfig(codecConfigTmmbr));

  ec = mVideoConduit->ConfigureRecvMediaCodecs(codecs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders.size(), 1U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].payload_type, 120);
  ASSERT_EQ(mCall->mVideoReceiveConfig.decoders[0].video_format.name, "VP8");
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.local_ssrc, 0U);
  ASSERT_NE(mCall->mVideoReceiveConfig.rtp.remote_ssrc, 0U);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.nack.rtp_history_ms, 0);
  ASSERT_FALSE(mCall->mVideoReceiveConfig.rtp.remb);
  ASSERT_TRUE(mCall->mVideoReceiveConfig.rtp.tmmbr);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.red_payload_type, -1);
  ASSERT_EQ(mCall->mVideoReceiveConfig.rtp.rtx_associated_payload_types.size(),
            0U);
}

TEST_F(VideoConduitTest, TestReconfigureSendMediaCodec) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  // Defaults
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.payload_name, "VP8");
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.payload_type, 120);
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.rtcp_mode, webrtc::RtcpMode::kCompound);
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.max_packet_size, kVideoMtu);
  ASSERT_EQ(mCall->mCurrentVideoSendStream->mEncoderConfig.content_type,
            VideoEncoderConfig::ContentType::kRealtimeVideo);
  ASSERT_EQ(
      mCall->mCurrentVideoSendStream->mEncoderConfig.min_transmit_bitrate_bps,
      0);
  ASSERT_EQ(mCall->mCurrentVideoSendStream->mEncoderConfig.max_bitrate_bps,
            KBPS(10000));
  ASSERT_EQ(mCall->mCurrentVideoSendStream->mEncoderConfig.number_of_streams,
            1U);
  mVideoConduit->StopTransmitting();

  // FEC
  VideoCodecConfig codecConfigFEC(120, "VP8", constraints);
  codecConfigFEC.mEncodings.push_back(encoding);
  codecConfigFEC.mFECFbSet = true;
  codecConfigFEC.mNackFbTypes.push_back("");
  codecConfigFEC.mULPFECPayloadType = 1;
  codecConfigFEC.mREDPayloadType = 2;
  codecConfigFEC.mREDRTXPayloadType = 3;
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigFEC, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.ulpfec_payload_type,
            codecConfigFEC.mULPFECPayloadType);
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.red_payload_type,
            codecConfigFEC.mREDPayloadType);
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.red_rtx_payload_type,
            codecConfigFEC.mREDRTXPayloadType);
  mVideoConduit->StopTransmitting();

  // H264
  VideoCodecConfig codecConfigH264(120, "H264", constraints);
  codecConfigH264.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigH264, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.payload_name, "H264");
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.payload_type, 120);
  mVideoConduit->StopTransmitting();

  // TIAS
  VideoCodecConfig codecConfigTias(120, "VP8", constraints);
  codecConfigTias.mEncodings.push_back(encoding);
  codecConfigTias.mTias = 1000000;
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigTias, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  SendVideoFrame(1280, 720, 1);

  videoStreams = mCall->CreateEncoderStreams(1280, 720);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].min_bitrate_bps, 200000);
  ASSERT_EQ(videoStreams[0].target_bitrate_bps, 800000);
  ASSERT_EQ(videoStreams[0].max_bitrate_bps, 1000000);
  mVideoConduit->StopTransmitting();

  // MaxBr
  VideoCodecConfig codecConfigMaxBr(120, "VP8", constraints);
  encoding.constraints.maxBr = 50000;
  codecConfigMaxBr.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigMaxBr, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  SendVideoFrame(1280, 720, 1);
  videoStreams = mCall->CreateEncoderStreams(1280, 720);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_LE(videoStreams[0].min_bitrate_bps, 50000);
  ASSERT_LE(videoStreams[0].target_bitrate_bps, 50000);
  ASSERT_EQ(videoStreams[0].max_bitrate_bps, 50000);
  mVideoConduit->StopTransmitting();

  // MaxFs
  VideoCodecConfig codecConfigMaxFs(120, "VP8", constraints);
  codecConfigMaxFs.mEncodingConstraints.maxFs = 3600;
  encoding.constraints.maxBr = 0;
  codecConfigMaxFs.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigMaxFs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  mVideoConduit->StartTransmitting();
  SendVideoFrame(1280, 720, 1);
  ASSERT_EQ(sink->mVideoFrame.width(), 1280);
  ASSERT_EQ(sink->mVideoFrame.height(), 720);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 1000U);
  ASSERT_EQ(sink->mOnFrameCount, 1U);

  SendVideoFrame(640, 360, 2);
  ASSERT_EQ(sink->mVideoFrame.width(), 640);
  ASSERT_EQ(sink->mVideoFrame.height(), 360);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 2000U);
  ASSERT_EQ(sink->mOnFrameCount, 2U);

  SendVideoFrame(1920, 1280, 3);
  ASSERT_EQ(sink->mVideoFrame.width(), 960);
  ASSERT_EQ(sink->mVideoFrame.height(), 640);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 3000U);
  ASSERT_EQ(sink->mOnFrameCount, 3U);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestReconfigureSendMediaCodecWhileTransmitting) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  // Defaults
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.payload_name, "VP8");
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.payload_type, 120);
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.rtcp_mode, webrtc::RtcpMode::kCompound);
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.max_packet_size, kVideoMtu);
  ASSERT_EQ(mCall->mCurrentVideoSendStream->mEncoderConfig.content_type,
            VideoEncoderConfig::ContentType::kRealtimeVideo);
  ASSERT_EQ(
      mCall->mCurrentVideoSendStream->mEncoderConfig.min_transmit_bitrate_bps,
      0);
  ASSERT_EQ(mCall->mCurrentVideoSendStream->mEncoderConfig.max_bitrate_bps,
            KBPS(10000));
  ASSERT_EQ(mCall->mCurrentVideoSendStream->mEncoderConfig.number_of_streams,
            1U);

  // Changing these parameters should not require a call to StartTransmitting
  // for the changes to take effect.

  // TIAS
  VideoCodecConfig codecConfigTias(120, "VP8", constraints);
  codecConfigTias.mEncodings.push_back(encoding);
  codecConfigTias.mTias = 1000000;
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigTias, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  SendVideoFrame(1280, 720, 1);

  videoStreams = mCall->CreateEncoderStreams(1280, 720);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].min_bitrate_bps, 200000);
  ASSERT_EQ(videoStreams[0].target_bitrate_bps, 800000);
  ASSERT_EQ(videoStreams[0].max_bitrate_bps, 1000000);

  // MaxBr
  VideoCodecConfig codecConfigMaxBr(120, "VP8", constraints);
  encoding.constraints.maxBr = 50000;
  codecConfigMaxBr.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigMaxBr, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
  SendVideoFrame(1280, 720, 1);
  videoStreams = mCall->CreateEncoderStreams(1280, 720);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_LE(videoStreams[0].min_bitrate_bps, 50000);
  ASSERT_LE(videoStreams[0].target_bitrate_bps, 50000);
  ASSERT_EQ(videoStreams[0].max_bitrate_bps, 50000);

  // MaxFs
  VideoCodecConfig codecConfigMaxFs(120, "VP8", constraints);
  codecConfigMaxFs.mEncodingConstraints.maxFs = 3600;
  encoding.constraints.maxBr = 0;
  codecConfigMaxFs.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigMaxFs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  SendVideoFrame(1280, 720, 1);
  ASSERT_EQ(sink->mVideoFrame.width(), 1280);
  ASSERT_EQ(sink->mVideoFrame.height(), 720);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 1000U);
  ASSERT_EQ(sink->mOnFrameCount, 1U);

  SendVideoFrame(640, 360, 2);
  ASSERT_EQ(sink->mVideoFrame.width(), 640);
  ASSERT_EQ(sink->mVideoFrame.height(), 360);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 2000U);
  ASSERT_EQ(sink->mOnFrameCount, 2U);

  SendVideoFrame(1920, 1280, 3);
  ASSERT_EQ(sink->mVideoFrame.width(), 960);
  ASSERT_EQ(sink->mVideoFrame.height(), 640);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 3000U);
  ASSERT_EQ(sink->mOnFrameCount, 3U);

  // ScaleResolutionDownBy
  VideoCodecConfig codecConfigScaleDownBy(120, "VP8", constraints);
  encoding.constraints.maxFs = 0;
  encoding.constraints.scaleDownBy = 3.7;
  codecConfigScaleDownBy.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigScaleDownBy, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  SendVideoFrame(1280, 720, 4);
  ASSERT_EQ(sink->mVideoFrame.width(), 320);
  ASSERT_EQ(sink->mVideoFrame.height(), 180);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 4000U);
  ASSERT_EQ(sink->mOnFrameCount, 4U);

  codecConfigScaleDownBy.mEncodings[0].constraints.scaleDownBy = 1.3;
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigScaleDownBy, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  SendVideoFrame(641, 359, 5);
  ASSERT_EQ(sink->mVideoFrame.width(), 480);
  ASSERT_EQ(sink->mVideoFrame.height(), 267);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 5000U);
  ASSERT_EQ(sink->mOnFrameCount, 5U);

  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestVideoEncode) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  mVideoConduit->StartTransmitting();
  SendVideoFrame(1280, 720, 1);
  ASSERT_EQ(sink->mVideoFrame.width(), 1280);
  ASSERT_EQ(sink->mVideoFrame.height(), 720);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 1000U);
  ASSERT_EQ(sink->mOnFrameCount, 1U);

  SendVideoFrame(640, 360, 2);
  ASSERT_EQ(sink->mVideoFrame.width(), 640);
  ASSERT_EQ(sink->mVideoFrame.height(), 360);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 2000U);
  ASSERT_EQ(sink->mOnFrameCount, 2U);

  SendVideoFrame(1920, 1280, 3);
  ASSERT_EQ(sink->mVideoFrame.width(), 1920);
  ASSERT_EQ(sink->mVideoFrame.height(), 1280);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 3000U);
  ASSERT_EQ(sink->mOnFrameCount, 3U);

  mVideoConduit->StopTransmitting();
  mVideoConduit->RemoveSink(sink.get());
}

TEST_F(VideoConduitTest, TestVideoEncodeMaxFs) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodingConstraints.maxFs = 3600;
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  mVideoConduit->StartTransmitting();
  SendVideoFrame(1280, 720, 1);
  ASSERT_EQ(sink->mVideoFrame.width(), 1280);
  ASSERT_EQ(sink->mVideoFrame.height(), 720);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 1000U);
  ASSERT_EQ(sink->mOnFrameCount, 1U);

  SendVideoFrame(640, 360, 2);
  ASSERT_EQ(sink->mVideoFrame.width(), 640);
  ASSERT_EQ(sink->mVideoFrame.height(), 360);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 2000U);
  ASSERT_EQ(sink->mOnFrameCount, 2U);

  SendVideoFrame(1920, 1280, 3);
  ASSERT_EQ(sink->mVideoFrame.width(), 960);
  ASSERT_EQ(sink->mVideoFrame.height(), 640);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 3000U);
  ASSERT_EQ(sink->mOnFrameCount, 3U);

  // maxFs should not force pixel count above what a sink has requested.
  // We set 3600 macroblocks (16x16 pixels), so we request 3500 here.
  wants.max_pixel_count = 3500 * 16 * 16;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  SendVideoFrame(1280, 720, 4);
  ASSERT_EQ(sink->mVideoFrame.width(), 960);
  ASSERT_EQ(sink->mVideoFrame.height(), 540);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 4000U);
  ASSERT_EQ(sink->mOnFrameCount, 4U);

  SendVideoFrame(640, 360, 5);
  ASSERT_EQ(sink->mVideoFrame.width(), 640);
  ASSERT_EQ(sink->mVideoFrame.height(), 360);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 5000U);
  ASSERT_EQ(sink->mOnFrameCount, 5U);

  SendVideoFrame(1920, 1280, 6);
  ASSERT_EQ(sink->mVideoFrame.width(), 960);
  ASSERT_EQ(sink->mVideoFrame.height(), 640);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 6000U);
  ASSERT_EQ(sink->mOnFrameCount, 6U);

  mVideoConduit->StopTransmitting();
  mVideoConduit->RemoveSink(sink.get());
}

TEST_F(VideoConduitTest, TestVideoEncodeMaxFsNegotiatedThenSinkWants) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  codecConfig.mEncodingConstraints.maxFs = 3500;
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  unsigned int frame = 0;
  mVideoConduit->StartTransmitting();

  SendVideoFrame(1280, 720, frame++);
  ASSERT_EQ(sink->mVideoFrame.width(), 960);
  ASSERT_EQ(sink->mVideoFrame.height(), 540);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), (frame - 1) * 1000);
  ASSERT_EQ(sink->mOnFrameCount, frame);

  wants.max_pixel_count = 3600 * 16 * 16;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  SendVideoFrame(1280, 720, frame++);
  ASSERT_EQ(sink->mVideoFrame.width(), 960);
  ASSERT_EQ(sink->mVideoFrame.height(), 540);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), (frame - 1) * 1000);
  ASSERT_EQ(sink->mOnFrameCount, frame);

  mVideoConduit->StopTransmitting();
  mVideoConduit->RemoveSink(sink.get());
}

TEST_F(VideoConduitTest, TestVideoEncodeMaxFsCodecChange) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  codecConfig.mEncodingConstraints.maxFs = 3500;
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  unsigned int frame = 0;
  mVideoConduit->StartTransmitting();

  SendVideoFrame(1280, 720, frame++);
  ASSERT_EQ(sink->mVideoFrame.width(), 960);
  ASSERT_EQ(sink->mVideoFrame.height(), 540);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), (frame - 1) * 1000);
  ASSERT_EQ(sink->mOnFrameCount, frame);

  VideoCodecConfig codecConfigVP9(121, "VP9", constraints);
  codecConfigVP9.mEncodings.push_back(encoding);
  codecConfigVP9.mEncodingConstraints.maxFs = 3500;
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigVP9, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  SendVideoFrame(1280, 720, frame++);
  ASSERT_EQ(sink->mVideoFrame.width(), 960);
  ASSERT_EQ(sink->mVideoFrame.height(), 540);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), (frame - 1) * 1000);
  ASSERT_EQ(sink->mOnFrameCount, frame);

  mVideoConduit->StopTransmitting();
  mVideoConduit->RemoveSink(sink.get());
}

TEST_F(VideoConduitTest, TestVideoEncodeMaxFsSinkWantsThenCodecChange) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  wants.max_pixel_count = 3500 * 16 * 16;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  unsigned int frame = 0;
  mVideoConduit->StartTransmitting();

  SendVideoFrame(1280, 720, frame++);
  ASSERT_EQ(sink->mVideoFrame.width(), 960);
  ASSERT_EQ(sink->mVideoFrame.height(), 540);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), (frame - 1) * 1000);
  ASSERT_EQ(sink->mOnFrameCount, frame);

  VideoCodecConfig codecConfigVP9(121, "VP9", constraints);
  codecConfigVP9.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigVP9, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  SendVideoFrame(1280, 720, frame++);
  ASSERT_EQ(sink->mVideoFrame.width(), 960);
  ASSERT_EQ(sink->mVideoFrame.height(), 540);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), (frame - 1) * 1000);
  ASSERT_EQ(sink->mOnFrameCount, frame);

  mVideoConduit->StopTransmitting();
  mVideoConduit->RemoveSink(sink.get());
}

TEST_F(VideoConduitTest, TestVideoEncodeMaxFsNegotiated) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  unsigned int frame = 0;
  mVideoConduit->StartTransmitting();
  SendVideoFrame(1280, 720, frame++);
  ASSERT_EQ(sink->mVideoFrame.width(), 1280);
  ASSERT_EQ(sink->mVideoFrame.height(), 720);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), (frame - 1) * 1000);
  ASSERT_EQ(sink->mOnFrameCount, frame);

  // Ensure that negotiating a new max-fs works
  codecConfig.mEncodingConstraints.maxFs = 3500;
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  SendVideoFrame(1280, 720, frame++);
  ASSERT_EQ(sink->mVideoFrame.width(), 960);
  ASSERT_EQ(sink->mVideoFrame.height(), 540);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), (frame - 1) * 1000);
  ASSERT_EQ(sink->mOnFrameCount, frame);

  // Ensure that negotiating max-fs away works
  codecConfig.mEncodingConstraints.maxFs = 0;
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  SendVideoFrame(1280, 720, frame++);
  ASSERT_EQ(sink->mVideoFrame.width(), 1280);
  ASSERT_EQ(sink->mVideoFrame.height(), 720);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), (frame - 1) * 1000);
  ASSERT_EQ(sink->mOnFrameCount, frame);

  mVideoConduit->StopTransmitting();
  mVideoConduit->RemoveSink(sink.get());
}

// Disabled: See Bug 1420493
TEST_F(VideoConduitTest, DISABLED_TestVideoEncodeMaxWidthAndHeight) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodingConstraints.maxWidth = 1280;
  codecConfig.mEncodingConstraints.maxHeight = 720;
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  mVideoConduit->StartTransmitting();
  SendVideoFrame(1280, 720, 1);
  ASSERT_EQ(sink->mVideoFrame.width(), 1280);
  ASSERT_EQ(sink->mVideoFrame.height(), 720);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 1000U);
  ASSERT_EQ(sink->mOnFrameCount, 1U);

  SendVideoFrame(640, 360, 2);
  ASSERT_EQ(sink->mVideoFrame.width(), 640);
  ASSERT_EQ(sink->mVideoFrame.height(), 360);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 2000U);
  ASSERT_EQ(sink->mOnFrameCount, 2U);

  SendVideoFrame(1920, 1280, 3);
  ASSERT_EQ(sink->mVideoFrame.width(), 1080);
  ASSERT_EQ(sink->mVideoFrame.height(), 720);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 3000U);
  ASSERT_EQ(sink->mOnFrameCount, 3U);

  mVideoConduit->StopTransmitting();
  mVideoConduit->RemoveSink(sink.get());
}

TEST_F(VideoConduitTest, TestVideoEncodeScaleResolutionBy) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  encoding.constraints.scaleDownBy = 2;
  std::vector<webrtc::VideoStream> videoStreams;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodingConstraints.maxFs = 3600;
  codecConfig.mEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  mVideoConduit->StartTransmitting();
  SendVideoFrame(1280, 720, 1);
  ASSERT_EQ(sink->mVideoFrame.width(), 640);
  ASSERT_EQ(sink->mVideoFrame.height(), 360);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 1000U);
  ASSERT_EQ(sink->mOnFrameCount, 1U);

  SendVideoFrame(640, 360, 2);
  ASSERT_EQ(sink->mVideoFrame.width(), 320);
  ASSERT_EQ(sink->mVideoFrame.height(), 180);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 2000U);
  ASSERT_EQ(sink->mOnFrameCount, 2U);
  mVideoConduit->StopTransmitting();
  mVideoConduit->RemoveSink(sink.get());
}

TEST_F(VideoConduitTest, TestVideoEncodeSimulcastScaleResolutionBy) {
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kCompound);
  mVideoConduit->SetLocalSSRCs({42, 43, 44}, {45, 46, 47});

  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::Encoding encoding;
  VideoCodecConfig::Encoding encoding2;
  VideoCodecConfig::Encoding encoding3;
  encoding.constraints.scaleDownBy = 2;
  encoding2.constraints.scaleDownBy = 3;
  encoding3.constraints.scaleDownBy = 4;

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mEncodings.push_back(encoding);
  codecConfig.mEncodings.push_back(encoding2);
  codecConfig.mEncodings.push_back(encoding3);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  mVideoConduit->StartTransmitting();
  SendVideoFrame(640, 480, 1);
  // Check actually configured streams in encoder sink.
  ASSERT_EQ(sink->mVideoFrame.width(), 320);
  ASSERT_EQ(sink->mVideoFrame.height(), 240);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 1000U);
  ASSERT_EQ(sink->mOnFrameCount, 1U);

  SendVideoFrame(1280, 720, 2);
  ASSERT_EQ(sink->mVideoFrame.width(), 640);
  ASSERT_EQ(sink->mVideoFrame.height(), 352);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 2000U);
  ASSERT_EQ(sink->mOnFrameCount, 2U);
  mVideoConduit->StopTransmitting();
  mVideoConduit->RemoveSink(sink.get());
}

TEST_F(VideoConduitTest, TestSettingRtpRtcpRsize) {
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  std::vector<UniquePtr<mozilla::VideoCodecConfig>> codecs;
  RtpRtcpConfig rtpConf(webrtc::RtcpMode::kReducedSize);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  VideoCodecConfig::Encoding encoding;
  codecConfig.mEncodings.push_back(encoding);

  codecs.emplace_back(new VideoCodecConfig(codecConfig));

  ec = mVideoConduit->ConfigureRecvMediaCodecs(codecs, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);

  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig, rtpConf);
  ASSERT_EQ(ec, kMediaConduitNoError);
}

}  // End namespace test.
