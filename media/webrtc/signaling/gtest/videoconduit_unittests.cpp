
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

#include "nspr.h"
#include "nss.h"
#include "ssl.h"

#include "Canonicals.h"
#include "VideoConduit.h"
#include "RtpRtcpConfig.h"
#include "WebrtcCallWrapper.h"
#include "WebrtcGmpVideoCodec.h"

#include "api/video/video_sink_interface.h"
#include "media/base/media_constants.h"
#include "media/base/video_adapter.h"

#include "MockCall.h"
#include "MockConduit.h"

using namespace mozilla;
using namespace testing;
using namespace webrtc;

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

class VideoConduitTest : public Test {
 public:
  VideoConduitTest(
      VideoSessionConduit::Options aOptions = VideoSessionConduit::Options())
      : mCallWrapper(MockCallWrapper::Create()),
        mVideoConduit(MakeRefPtr<WebrtcVideoConduit>(
            mCallWrapper, GetCurrentSerialEventTarget(), std::move(aOptions),
            "", TrackingId(TrackingId::Source::Unimplemented, 0))),
        mControl(GetCurrentSerialEventTarget()) {
    NSS_NoDB_Init(nullptr);

    EXPECT_EQ(mVideoConduit->Init(), kMediaConduitNoError);
    mControl.Update([&](auto& aControl) {
      mVideoConduit->InitControl(&mControl);
      aControl.mLocalSsrcs = {42};
      aControl.mLocalVideoRtxSsrcs = {43};
    });
  }

  ~VideoConduitTest() override {
    mozilla::Unused << WaitFor(mVideoConduit->Shutdown());
    mCallWrapper->Destroy();
  }

  MockCall* Call() { return mCallWrapper->GetMockCall(); }

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

  const RefPtr<MockCallWrapper> mCallWrapper;
  const RefPtr<mozilla::WebrtcVideoConduit> mVideoConduit;
  ConcreteControl mControl;
};

TEST_F(VideoConduitTest, TestConfigureReceiveMediaCodecs) {
  // No codecs
  mControl.Update([&](auto& aControl) {
    aControl.mReceiving = true;
    aControl.mVideoRecvCodecs = {};
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders.size(), 0U);

  // empty codec name
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codec(120, "", EncodingConstraints());
    aControl.mVideoRecvCodecs = {codec};
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders.size(), 0U);

  // Defaults
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codec(120, "VP8", EncodingConstraints());
    aControl.mVideoRecvCodecs = {codec};
    aControl.mVideoRecvRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders.size(), 1U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].payload_type, 120);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].video_format.name, "VP8");
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.local_ssrc, 0U);
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.remote_ssrc, 0U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.nack.rtp_history_ms, 0);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.remb);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.tmmbr);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.red_payload_type, -1);
  ASSERT_EQ(
      Call()->mVideoReceiveConfig->rtp.rtx_associated_payload_types.size(), 0U);
}

TEST_F(VideoConduitTest, TestConfigureReceiveMediaCodecsFEC) {
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mFECFbSet = true;
    aControl.mVideoRecvCodecs = {
        codecConfig, VideoCodecConfig(1, "ulpfec", EncodingConstraints()),
        VideoCodecConfig(2, "red", EncodingConstraints())};
    aControl.mVideoRecvRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
    aControl.mReceiving = true;
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders.size(), 1U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].payload_type, 120);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].video_format.name, "VP8");
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.local_ssrc, 0U);
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.remote_ssrc, 0U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.nack.rtp_history_ms, 0);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.remb);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.tmmbr);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.ulpfec_payload_type, 1);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.red_payload_type, 2);
  ASSERT_EQ(
      Call()->mVideoReceiveConfig->rtp.rtx_associated_payload_types.size(), 0U);
}

TEST_F(VideoConduitTest, TestConfigureReceiveMediaCodecsH264) {
  mControl.Update([&](auto& aControl) {
    // Insert twice to test that only one H264 codec is used at a time
    aControl.mReceiving = true;
    aControl.mVideoRecvCodecs = {
        VideoCodecConfig(120, "H264", EncodingConstraints()),
        VideoCodecConfig(120, "H264", EncodingConstraints())};
    aControl.mVideoRecvRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders.size(), 1U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].payload_type, 120);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].video_format.name, "H264");
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.local_ssrc, 0U);
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.remote_ssrc, 0U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.nack.rtp_history_ms, 0);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.remb);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.tmmbr);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.red_payload_type, -1);
  ASSERT_EQ(
      Call()->mVideoReceiveConfig->rtp.rtx_associated_payload_types.size(), 0U);
}

TEST_F(VideoConduitTest, TestConfigureReceiveMediaCodecsKeyframeRequestType) {
  // PLI should be preferred to FIR, same codec.
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mNackFbTypes.push_back("pli");
    codecConfig.mCcmFbTypes.push_back("fir");
    aControl.mReceiving = true;
    aControl.mVideoRecvCodecs = {codecConfig};
    aControl.mVideoRecvRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders.size(), 1U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kPliRtcp);

  // Just FIR
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mCcmFbTypes.push_back("fir");
    aControl.mVideoRecvCodecs = {codecConfig};
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders.size(), 1U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kFirRtcp);

  // PLI should be preferred to FIR, multiple codecs.
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig pliCodec(120, "VP8", EncodingConstraints());
    pliCodec.mNackFbTypes.push_back("pli");
    VideoCodecConfig firCodec(120, "VP8", EncodingConstraints());
    firCodec.mCcmFbTypes.push_back("fir");
    aControl.mVideoRecvCodecs = {pliCodec, firCodec};
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders.size(), 2U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kPliRtcp);
}

TEST_F(VideoConduitTest, TestConfigureReceiveMediaCodecsNack) {
  mControl.Update([&](auto& aControl) {
    aControl.mReceiving = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mNackFbTypes.push_back("");
    aControl.mVideoRecvCodecs = {codecConfig};
    aControl.mVideoRecvRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders.size(), 1U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].payload_type, 120);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].video_format.name, "VP8");
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.local_ssrc, 0U);
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.remote_ssrc, 0U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.nack.rtp_history_ms, 1000);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.remb);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.tmmbr);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.red_payload_type, -1);
  ASSERT_EQ(
      Call()->mVideoReceiveConfig->rtp.rtx_associated_payload_types.size(), 0U);
}

TEST_F(VideoConduitTest, TestConfigureReceiveMediaCodecsRemb) {
  mControl.Update([&](auto& aControl) {
    aControl.mReceiving = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mRembFbSet = true;
    aControl.mVideoRecvCodecs = {codecConfig};
    aControl.mVideoRecvRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders.size(), 1U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].payload_type, 120);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].video_format.name, "VP8");
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.local_ssrc, 0U);
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.remote_ssrc, 0U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.nack.rtp_history_ms, 0);
  ASSERT_TRUE(Call()->mVideoReceiveConfig->rtp.remb);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.tmmbr);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.red_payload_type, -1);
  ASSERT_EQ(
      Call()->mVideoReceiveConfig->rtp.rtx_associated_payload_types.size(), 0U);
}

TEST_F(VideoConduitTest, TestConfigureReceiveMediaCodecsTmmbr) {
  mControl.Update([&](auto& aControl) {
    aControl.mReceiving = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mCcmFbTypes.push_back("tmmbr");
    aControl.mVideoRecvCodecs = {codecConfig};
    aControl.mVideoRecvRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders.size(), 1U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].payload_type, 120);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].video_format.name, "VP8");
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.local_ssrc, 0U);
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.remote_ssrc, 0U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.nack.rtp_history_ms, 0);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.remb);
  ASSERT_TRUE(Call()->mVideoReceiveConfig->rtp.tmmbr);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.red_payload_type, -1);
  ASSERT_EQ(
      Call()->mVideoReceiveConfig->rtp.rtx_associated_payload_types.size(), 0U);
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodec) {
  // defaults
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoSendConfig);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.payload_name, "VP8");
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.payload_type, 120);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.max_packet_size, kVideoMtu);
  ASSERT_EQ(Call()->mVideoSendEncoderConfig->content_type,
            VideoEncoderConfig::ContentType::kRealtimeVideo);
  ASSERT_EQ(Call()->mVideoSendEncoderConfig->min_transmit_bitrate_bps, 0);
  ASSERT_EQ(Call()->mVideoSendEncoderConfig->max_bitrate_bps, KBPS(10000));
  ASSERT_EQ(Call()->mVideoSendEncoderConfig->number_of_streams, 1U);

  // empty codec name
  mControl.Update([&](auto& aControl) {
    aControl.mVideoSendCodec =
        Some(VideoCodecConfig(120, "", EncodingConstraints()));
  });
  // Bad codec gets ignored
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.payload_name, "VP8");
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecMaxFps) {
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    EncodingConstraints constraints;
    VideoCodecConfig codecConfig(120, "VP8", constraints);
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
  std::vector<webrtc::VideoStream> videoStreams;
  videoStreams = Call()->CreateEncoderStreams(640, 480);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].max_framerate, 30);  // DEFAULT_VIDEO_MAX_FRAMERATE

  mControl.Update([&](auto& aControl) {
    EncodingConstraints constraints;
    constraints.maxFps = Some(42);
    VideoCodecConfig codecConfig(120, "VP8", constraints);
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
  videoStreams = Call()->CreateEncoderStreams(640, 480);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].max_framerate, 42);
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecMaxMbps) {
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    EncodingConstraints constraints;
    constraints.maxMbps = 0;
    VideoCodecConfig codecConfig(120, "VP8", constraints);
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
  SendVideoFrame(640, 480, 1);
  std::vector<webrtc::VideoStream> videoStreams;
  videoStreams = Call()->CreateEncoderStreams(640, 480);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].max_framerate, 30);  // DEFAULT_VIDEO_MAX_FRAMERATE

  mControl.Update([&](auto& aControl) {
    EncodingConstraints constraints;
    constraints.maxMbps = 10000;
    VideoCodecConfig codecConfig(120, "VP8", constraints);
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
  SendVideoFrame(640, 480, 1);
  videoStreams = Call()->CreateEncoderStreams(640, 480);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].max_framerate, 8);
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecDefaults) {
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });

  {
    ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(640, 480);
    EXPECT_EQ(videoStreams.size(), 1U);
    EXPECT_EQ(videoStreams[0].min_bitrate_bps, 150000);
    EXPECT_EQ(videoStreams[0].target_bitrate_bps, 500000);
    EXPECT_EQ(videoStreams[0].max_bitrate_bps, 2000000);
  }

  {
    // SelectBitrates not called until we send a frame
    SendVideoFrame(1280, 720, 1);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(1280, 720);
    EXPECT_EQ(videoStreams.size(), 1U);
    EXPECT_EQ(videoStreams[0].min_bitrate_bps, 200000);
    EXPECT_EQ(videoStreams[0].target_bitrate_bps, 800000);
    EXPECT_EQ(videoStreams[0].max_bitrate_bps, 2500000);
  }
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecTias) {
  // TIAS
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfigTias(120, "VP8", EncodingConstraints());
    codecConfigTias.mEncodings.emplace_back();
    codecConfigTias.mTias = 1000000;
    aControl.mVideoSendCodec = Some(codecConfigTias);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  {
    ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
    SendVideoFrame(1280, 720, 1);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(1280, 720);
    ASSERT_EQ(videoStreams.size(), 1U);
    ASSERT_EQ(videoStreams[0].min_bitrate_bps, 200000);
    ASSERT_EQ(videoStreams[0].target_bitrate_bps, 800000);
    ASSERT_EQ(videoStreams[0].max_bitrate_bps, 1000000);
  }

  // TIAS (too low)
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfigTiasLow(120, "VP8", EncodingConstraints());
    codecConfigTiasLow.mEncodings.emplace_back();
    codecConfigTiasLow.mTias = 1000;
    aControl.mVideoSendCodec = Some(codecConfigTiasLow);
  });
  {
    ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
    SendVideoFrame(1280, 720, 1);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(1280, 720);
    ASSERT_EQ(videoStreams.size(), 1U);
    ASSERT_EQ(videoStreams[0].min_bitrate_bps, 30000);
    ASSERT_EQ(videoStreams[0].target_bitrate_bps, 30000);
    ASSERT_EQ(videoStreams[0].max_bitrate_bps, 30000);
  }
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecMaxBr) {
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    auto& encoding = codecConfig.mEncodings.emplace_back();
    encoding.constraints.maxBr = 50000;
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
  SendVideoFrame(1280, 720, 1);
  const std::vector<webrtc::VideoStream> videoStreams =
      Call()->CreateEncoderStreams(1280, 720);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_LE(videoStreams[0].min_bitrate_bps, 50000);
  ASSERT_LE(videoStreams[0].target_bitrate_bps, 50000);
  ASSERT_EQ(videoStreams[0].max_bitrate_bps, 50000);
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecScaleResolutionBy) {
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    {
      auto& encoding = codecConfig.mEncodings.emplace_back();
      encoding.constraints.scaleDownBy = 2;
    }
    {
      auto& encoding = codecConfig.mEncodings.emplace_back();
      encoding.constraints.scaleDownBy = 4;
    }
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
    aControl.mLocalSsrcs = {42, 1729};
    aControl.mLocalVideoRtxSsrcs = {43, 1730};
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  SendVideoFrame(640, 360, 1);
  const std::vector<webrtc::VideoStream> videoStreams =
      Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                   sink->mVideoFrame.height());
  ASSERT_EQ(videoStreams.size(), 2U);
  ASSERT_EQ(videoStreams[0].width, 320U);
  ASSERT_EQ(videoStreams[0].height, 180U);
  ASSERT_EQ(videoStreams[1].width, 160U);
  ASSERT_EQ(videoStreams[1].height, 90U);
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecCodecMode) {
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
    aControl.mVideoCodecMode = webrtc::VideoCodecMode::kScreensharing;
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
  ASSERT_EQ(Call()->mVideoSendEncoderConfig->content_type,
            VideoEncoderConfig::ContentType::kScreen);
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecFEC) {
  {
    // H264 + FEC
    mControl.Update([&](auto& aControl) {
      aControl.mTransmitting = true;
      VideoCodecConfig codecConfig(120, "H264", EncodingConstraints());
      codecConfig.mEncodings.emplace_back();
      codecConfig.mFECFbSet = true;
      codecConfig.mULPFECPayloadType = 1;
      codecConfig.mREDPayloadType = 2;
      codecConfig.mREDRTXPayloadType = 3;
      aControl.mVideoSendCodec = Some(codecConfig);
      aControl.mVideoSendRtpRtcpConfig =
          Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
    });
    ASSERT_TRUE(Call()->mVideoSendConfig);
    ASSERT_EQ(Call()->mVideoSendConfig->rtp.ulpfec.ulpfec_payload_type, 1);
    ASSERT_EQ(Call()->mVideoSendConfig->rtp.ulpfec.red_payload_type, 2);
    ASSERT_EQ(Call()->mVideoSendConfig->rtp.ulpfec.red_rtx_payload_type, 3);
  }

  {
    // H264 + FEC + Nack
    mControl.Update([&](auto& aControl) {
      VideoCodecConfig codecConfig(120, "H264", EncodingConstraints());
      codecConfig.mEncodings.emplace_back();
      codecConfig.mFECFbSet = true;
      codecConfig.mNackFbTypes.push_back("");
      codecConfig.mULPFECPayloadType = 1;
      codecConfig.mREDPayloadType = 2;
      codecConfig.mREDRTXPayloadType = 3;
      aControl.mVideoSendCodec = Some(codecConfig);
    });
    ASSERT_TRUE(Call()->mVideoSendConfig);
    ASSERT_EQ(Call()->mVideoSendConfig->rtp.ulpfec.ulpfec_payload_type, -1);
    ASSERT_EQ(Call()->mVideoSendConfig->rtp.ulpfec.red_payload_type, -1);
    ASSERT_EQ(Call()->mVideoSendConfig->rtp.ulpfec.red_rtx_payload_type, -1);
  }

  {
    // VP8 + FEC + Nack
    mControl.Update([&](auto& aControl) {
      VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
      codecConfig.mEncodings.emplace_back();
      codecConfig.mFECFbSet = true;
      codecConfig.mNackFbTypes.push_back("");
      codecConfig.mULPFECPayloadType = 1;
      codecConfig.mREDPayloadType = 2;
      codecConfig.mREDRTXPayloadType = 3;
      aControl.mVideoSendCodec = Some(codecConfig);
    });
    ASSERT_TRUE(Call()->mVideoSendConfig);
    ASSERT_EQ(Call()->mVideoSendConfig->rtp.ulpfec.ulpfec_payload_type, 1);
    ASSERT_EQ(Call()->mVideoSendConfig->rtp.ulpfec.red_payload_type, 2);
    ASSERT_EQ(Call()->mVideoSendConfig->rtp.ulpfec.red_rtx_payload_type, 3);
  }
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecNack) {
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoSendConfig);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.nack.rtp_history_ms, 0);

  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig = *aControl.mVideoSendCodec.Ref();
    codecConfig.mNackFbTypes.push_back("");
    aControl.mVideoSendCodec = Some(codecConfig);
  });
  ASSERT_TRUE(Call()->mVideoSendConfig);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.nack.rtp_history_ms, 1000);
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecRids) {
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoSendConfig);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.rids.size(), 0U);

  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    {
      auto& encoding = codecConfig.mEncodings.emplace_back();
      encoding.rid = "1";
    }
    {
      auto& encoding = codecConfig.mEncodings.emplace_back();
      encoding.rid = "2";
    }
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mLocalSsrcs = {42, 1729};
    aControl.mLocalVideoRtxSsrcs = {43, 1730};
  });
  ASSERT_TRUE(Call()->mVideoSendConfig);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.rids.size(), 2U);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.rids[0], "1");
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.rids[1], "2");
}

TEST_F(VideoConduitTest, TestOnSinkWantsChanged) {
  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    codecConfig.mEncodingConstraints.maxFs = 0;
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
  wants.max_pixel_count = 256000;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);
  SendVideoFrame(1920, 1080, 1);
  {
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(1920, 1080);
    EXPECT_LE(videoStreams[0].width * videoStreams[0].height, 256000U);
    ASSERT_EQ(videoStreams.size(), 1U);
    EXPECT_EQ(videoStreams[0].width, 673U);
    EXPECT_EQ(videoStreams[0].height, 379U);
  }

  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig = *aControl.mVideoSendCodec.Ref();
    codecConfig.mEncodingConstraints.maxFs = 500;
    aControl.mVideoSendCodec = Some(codecConfig);
  });
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);
  SendVideoFrame(1920, 1080, 2);
  {
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    EXPECT_LE(videoStreams[0].width * videoStreams[0].height, 500U * 16U * 16U);
    ASSERT_EQ(videoStreams.size(), 1U);
    EXPECT_EQ(videoStreams[0].width, 476U);
    EXPECT_EQ(videoStreams[0].height, 268U);
  }

  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig = *aControl.mVideoSendCodec.Ref();
    codecConfig.mEncodingConstraints.maxFs = 1000;
    aControl.mVideoSendCodec = Some(codecConfig);
  });
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);
  SendVideoFrame(1920, 1080, 3);
  {
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    EXPECT_LE(videoStreams[0].width * videoStreams[0].height,
              1000U * 16U * 16U);
    ASSERT_EQ(videoStreams.size(), 1U);
    EXPECT_EQ(videoStreams[0].width, 673U);
    EXPECT_EQ(videoStreams[0].height, 379U);
  }

  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig = *aControl.mVideoSendCodec.Ref();
    codecConfig.mEncodingConstraints.maxFs = 500;
    aControl.mVideoSendCodec = Some(codecConfig);
  });
  wants.max_pixel_count = 64000;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);
  SendVideoFrame(1920, 1080, 4);
  {
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    EXPECT_LE(videoStreams[0].width * videoStreams[0].height, 64000U);
    ASSERT_EQ(videoStreams.size(), 1U);
    EXPECT_EQ(videoStreams[0].width, 336U);
    EXPECT_EQ(videoStreams[0].height, 189U);
  }
}

class VideoConduitTestScalingLocked : public VideoConduitTest {
 public:
  static VideoSessionConduit::Options CreateOptions() {
    VideoSessionConduit::Options options;
    options.mLockScaling = true;
    return options;
  }
  VideoConduitTestScalingLocked() : VideoConduitTest(CreateOptions()) {}
};

TEST_F(VideoConduitTestScalingLocked, TestOnSinkWantsChanged) {
  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodingConstraints.maxFs = 0;
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
  wants.max_pixel_count = 256000;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);
  SendVideoFrame(1920, 1080, 1);
  EXPECT_EQ(sink->mVideoFrame.width(), 1920);
  EXPECT_EQ(sink->mVideoFrame.height(), 1080);
  {
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams.size(), 1U);
    EXPECT_EQ(videoStreams[0].width, 1920U);
    EXPECT_EQ(videoStreams[0].height, 1080U);
  }

  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig = *aControl.mVideoSendCodec.Ref();
    codecConfig.mEncodingConstraints.maxFs = 500;
    aControl.mVideoSendCodec = Some(codecConfig);
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
  SendVideoFrame(1920, 1080, 2);
  {
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    EXPECT_LE(videoStreams[0].width * videoStreams[0].height, 500U * 16U * 16U);
    ASSERT_EQ(videoStreams.size(), 1U);
    EXPECT_EQ(videoStreams[0].width, 476U);
    EXPECT_EQ(videoStreams[0].height, 268U);
  }
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecSimulcastOddScreen) {
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    {
      VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
      codecConfig.mEncodings.emplace_back();
      {
        auto& encoding = codecConfig.mEncodings.emplace_back();
        encoding.constraints.scaleDownBy = 2;
      }
      {
        auto& encoding = codecConfig.mEncodings.emplace_back();
        encoding.constraints.scaleDownBy = 4;
      }
      aControl.mVideoSendCodec = Some(codecConfig);
    }
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
    aControl.mLocalSsrcs = {42, 43, 44};
    aControl.mLocalVideoRtxSsrcs = {45, 46, 47};
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  SendVideoFrame(26, 24, 1);
  {
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams.size(), 3U);
    EXPECT_EQ(videoStreams[0].width, 26U);
    EXPECT_EQ(videoStreams[0].height, 24U);
    EXPECT_EQ(videoStreams[1].width, 13U);
    EXPECT_EQ(videoStreams[1].height, 12U);
    EXPECT_EQ(videoStreams[2].width, 6U);
    EXPECT_EQ(videoStreams[2].height, 6U);
  }

  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig = *aControl.mVideoSendCodec.Ref();
    codecConfig.mEncodings.clear();
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mLocalSsrcs = {42};
    aControl.mLocalVideoRtxSsrcs = {43};
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
  SendVideoFrame(26, 24, 2);
  {
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams.size(), 1U);
    EXPECT_EQ(videoStreams[0].width, 26U);
    EXPECT_EQ(videoStreams[0].height, 24U);
  }
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecSimulcastAllScaling) {
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    {
      auto& encoding = codecConfig.mEncodings.emplace_back();
      encoding.constraints.scaleDownBy = 2;
    }
    {
      auto& encoding = codecConfig.mEncodings.emplace_back();
      encoding.constraints.scaleDownBy = 4;
    }
    {
      auto& encoding = codecConfig.mEncodings.emplace_back();
      encoding.constraints.scaleDownBy = 6;
    }
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
    aControl.mLocalSsrcs = {42, 43, 44};
    aControl.mLocalVideoRtxSsrcs = {45, 46, 47};
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  SendVideoFrame(1281, 721, 1);
  {
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams.size(), 3U);
    EXPECT_EQ(videoStreams[0].width, 640U);
    EXPECT_EQ(videoStreams[0].height, 360U);
    EXPECT_EQ(videoStreams[1].width, 320U);
    EXPECT_EQ(videoStreams[1].height, 180U);
    EXPECT_EQ(videoStreams[2].width, 213U);
    EXPECT_EQ(videoStreams[2].height, 120U);
  }

  SendVideoFrame(1281, 721, 2);
  {
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams.size(), 3U);
    EXPECT_EQ(videoStreams[0].width, 640U);
    EXPECT_EQ(videoStreams[0].height, 360U);
    EXPECT_EQ(videoStreams[1].width, 320U);
    EXPECT_EQ(videoStreams[1].height, 180U);
    EXPECT_EQ(videoStreams[2].width, 213U);
    EXPECT_EQ(videoStreams[2].height, 120U);
  }

  SendVideoFrame(1280, 720, 3);
  {
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams.size(), 3U);
    EXPECT_EQ(videoStreams[0].width, 640U);
    EXPECT_EQ(videoStreams[0].height, 360U);
    EXPECT_EQ(videoStreams[1].width, 320U);
    EXPECT_EQ(videoStreams[1].height, 180U);
    EXPECT_EQ(videoStreams[2].width, 213U);
    EXPECT_EQ(videoStreams[2].height, 120U);
  }

  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig = *aControl.mVideoSendCodec.Ref();
    codecConfig.mEncodings[0].constraints.scaleDownBy = 1;
    codecConfig.mEncodings[1].constraints.scaleDownBy = 2;
    codecConfig.mEncodings[2].constraints.scaleDownBy = 4;
    aControl.mVideoSendCodec = Some(codecConfig);
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
  SendVideoFrame(1280, 720, 4);
  {
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams.size(), 3U);
    EXPECT_EQ(videoStreams[0].width, 1280U);
    EXPECT_EQ(videoStreams[0].height, 720U);
    EXPECT_EQ(videoStreams[1].width, 640U);
    EXPECT_EQ(videoStreams[1].height, 360U);
    EXPECT_EQ(videoStreams[2].width, 320U);
    EXPECT_EQ(videoStreams[2].height, 180U);
  }
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecSimulcastScreenshare) {
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    {
      auto& encoding = codecConfig.mEncodings.emplace_back();
      encoding.constraints.scaleDownBy = 2;
    }
    {
      auto& encoding = codecConfig.mEncodings.emplace_back();
      encoding.constraints.scaleDownBy = 4;
    }

    aControl.mTransmitting = true;
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
    aControl.mLocalSsrcs = {42, 43, 44};
    aControl.mLocalVideoRtxSsrcs = {45, 46, 47};
    aControl.mVideoCodecMode = webrtc::VideoCodecMode::kScreensharing;
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
  const std::vector<webrtc::VideoStream> videoStreams =
      Call()->CreateEncoderStreams(640, 480);
  ASSERT_EQ(videoStreams.size(), 1U);
}

TEST_F(VideoConduitTest, TestReconfigureReceiveMediaCodecs) {
  // Defaults
  mControl.Update([&](auto& aControl) {
    aControl.mReceiving = true;
    aControl.mVideoRecvCodecs = {
        VideoCodecConfig(120, "VP8", EncodingConstraints())};
    aControl.mVideoRecvRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders.size(), 1U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].payload_type, 120);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].video_format.name, "VP8");
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.local_ssrc, 0U);
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.remote_ssrc, 0U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.nack.rtp_history_ms, 0);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.remb);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.tmmbr);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.red_payload_type, -1);
  ASSERT_EQ(
      Call()->mVideoReceiveConfig->rtp.rtx_associated_payload_types.size(), 0U);

  // FEC
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfigFecFb(120, "VP8", EncodingConstraints());
    codecConfigFecFb.mFECFbSet = true;
    VideoCodecConfig codecConfigFEC(1, "ulpfec", EncodingConstraints());
    VideoCodecConfig codecConfigRED(2, "red", EncodingConstraints());
    aControl.mVideoRecvCodecs = {codecConfigFecFb, codecConfigFEC,
                                 codecConfigRED};
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders.size(), 1U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].payload_type, 120);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].video_format.name, "VP8");
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.local_ssrc, 0U);
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.remote_ssrc, 0U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.nack.rtp_history_ms, 0);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.remb);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.tmmbr);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.ulpfec_payload_type, 1);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.red_payload_type, 2);
  ASSERT_EQ(
      Call()->mVideoReceiveConfig->rtp.rtx_associated_payload_types.size(), 0U);

  // H264
  mControl.Update([&](auto& aControl) {
    aControl.mVideoRecvCodecs = {
        VideoCodecConfig(120, "H264", EncodingConstraints())};
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders.size(), 1U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].payload_type, 120);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].video_format.name, "H264");
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.local_ssrc, 0U);
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.remote_ssrc, 0U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.nack.rtp_history_ms, 0);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.remb);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.tmmbr);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.red_payload_type, -1);
  ASSERT_EQ(
      Call()->mVideoReceiveConfig->rtp.rtx_associated_payload_types.size(), 0U);

  // Nack
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfigNack(120, "VP8", EncodingConstraints());
    codecConfigNack.mNackFbTypes.push_back("");
    aControl.mVideoRecvCodecs = {codecConfigNack};
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders.size(), 1U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].payload_type, 120);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].video_format.name, "VP8");
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.local_ssrc, 0U);
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.remote_ssrc, 0U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.nack.rtp_history_ms, 1000);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.remb);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.tmmbr);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.red_payload_type, -1);
  ASSERT_EQ(
      Call()->mVideoReceiveConfig->rtp.rtx_associated_payload_types.size(), 0U);

  // Remb
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfigRemb(120, "VP8", EncodingConstraints());
    codecConfigRemb.mRembFbSet = true;
    aControl.mVideoRecvCodecs = {codecConfigRemb};
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders.size(), 1U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].payload_type, 120);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].video_format.name, "VP8");
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.local_ssrc, 0U);
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.remote_ssrc, 0U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.nack.rtp_history_ms, 0);
  ASSERT_TRUE(Call()->mVideoReceiveConfig->rtp.remb);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.tmmbr);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.red_payload_type, -1);
  ASSERT_EQ(
      Call()->mVideoReceiveConfig->rtp.rtx_associated_payload_types.size(), 0U);

  // Tmmbr
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfigTmmbr(120, "VP8", EncodingConstraints());
    codecConfigTmmbr.mCcmFbTypes.push_back("tmmbr");
    aControl.mVideoRecvCodecs = {codecConfigTmmbr};
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders.size(), 1U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].payload_type, 120);
  ASSERT_EQ(Call()->mVideoReceiveConfig->decoders[0].video_format.name, "VP8");
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.local_ssrc, 0U);
  ASSERT_NE(Call()->mVideoReceiveConfig->rtp.remote_ssrc, 0U);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.nack.rtp_history_ms, 0);
  ASSERT_FALSE(Call()->mVideoReceiveConfig->rtp.remb);
  ASSERT_TRUE(Call()->mVideoReceiveConfig->rtp.tmmbr);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.keyframe_method,
            webrtc::KeyFrameReqMethod::kNone);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.ulpfec_payload_type, -1);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.red_payload_type, -1);
  ASSERT_EQ(
      Call()->mVideoReceiveConfig->rtp.rtx_associated_payload_types.size(), 0U);
}

TEST_F(VideoConduitTest, TestReconfigureSendMediaCodec) {
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_FALSE(Call()->mVideoSendConfig);

  // Defaults
  mControl.Update([&](auto& aControl) { aControl.mTransmitting = true; });
  ASSERT_TRUE(Call()->mVideoSendConfig);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.payload_name, "VP8");
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.payload_type, 120);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.max_packet_size, kVideoMtu);
  ASSERT_EQ(Call()->mVideoSendEncoderConfig->content_type,
            VideoEncoderConfig::ContentType::kRealtimeVideo);
  ASSERT_EQ(Call()->mVideoSendEncoderConfig->min_transmit_bitrate_bps, 0);
  ASSERT_EQ(Call()->mVideoSendEncoderConfig->max_bitrate_bps, KBPS(10000));
  ASSERT_EQ(Call()->mVideoSendEncoderConfig->number_of_streams, 1U);
  mControl.Update([&](auto& aControl) { aControl.mTransmitting = false; });

  // FEC
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfigFEC(120, "VP8", EncodingConstraints());
    codecConfigFEC.mEncodings.emplace_back();
    codecConfigFEC.mFECFbSet = true;
    codecConfigFEC.mNackFbTypes.push_back("");
    codecConfigFEC.mULPFECPayloadType = 1;
    codecConfigFEC.mREDPayloadType = 2;
    codecConfigFEC.mREDRTXPayloadType = 3;
    aControl.mVideoSendCodec = Some(codecConfigFEC);
  });
  ASSERT_TRUE(Call()->mVideoSendConfig);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.ulpfec.ulpfec_payload_type, 1);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.ulpfec.red_payload_type, 2);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.ulpfec.red_rtx_payload_type, 3);
  mControl.Update([&](auto& aControl) { aControl.mTransmitting = false; });

  // H264
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfigH264(120, "H264", EncodingConstraints());
    codecConfigH264.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfigH264);
  });
  ASSERT_TRUE(Call()->mVideoSendConfig);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.payload_name, "H264");
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.payload_type, 120);
  mControl.Update([&](auto& aControl) { aControl.mTransmitting = false; });

  // TIAS
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfigTias(120, "VP8", EncodingConstraints());
    codecConfigTias.mEncodings.emplace_back();
    codecConfigTias.mTias = 1000000;
    aControl.mVideoSendCodec = Some(codecConfigTias);
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
  SendVideoFrame(1280, 720, 1);

  {
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(1280, 720);
    ASSERT_EQ(videoStreams.size(), 1U);
    ASSERT_EQ(videoStreams[0].min_bitrate_bps, 200000);
    ASSERT_EQ(videoStreams[0].target_bitrate_bps, 800000);
    ASSERT_EQ(videoStreams[0].max_bitrate_bps, 1000000);
  }
  mControl.Update([&](auto& aControl) { aControl.mTransmitting = false; });

  // MaxBr
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    VideoCodecConfig::Encoding encoding;
    {
      auto& encoding = codecConfig.mEncodings.emplace_back();
      encoding.constraints.maxBr = 50000;
    }
    aControl.mVideoSendCodec = Some(codecConfig);
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
  SendVideoFrame(1280, 720, 1);
  {
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(1280, 720);
    ASSERT_EQ(videoStreams.size(), 1U);
    ASSERT_LE(videoStreams[0].min_bitrate_bps, 50000);
    ASSERT_LE(videoStreams[0].target_bitrate_bps, 50000);
    ASSERT_EQ(videoStreams[0].max_bitrate_bps, 50000);
  }
  mControl.Update([&](auto& aControl) { aControl.mTransmitting = false; });

  // MaxFs
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfigMaxFs(120, "VP8", EncodingConstraints());
    codecConfigMaxFs.mEncodingConstraints.maxFs = 3600;
    VideoCodecConfig::Encoding encoding;
    encoding.constraints.maxBr = 0;
    codecConfigMaxFs.mEncodings.push_back(encoding);
    aControl.mVideoSendCodec = Some(codecConfigMaxFs);
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  {
    SendVideoFrame(1280, 720, 1);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 1280U);
    ASSERT_EQ(videoStreams[0].height, 720U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 1000U);
    ASSERT_EQ(sink->mOnFrameCount, 1U);
  }

  {
    SendVideoFrame(640, 360, 2);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 640U);
    ASSERT_EQ(videoStreams[0].height, 360U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 2000U);
    ASSERT_EQ(sink->mOnFrameCount, 2U);
  }

  {
    SendVideoFrame(1920, 1280, 3);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 1174U);
    ASSERT_EQ(videoStreams[0].height, 783U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 3000U);
    ASSERT_EQ(sink->mOnFrameCount, 3U);
  }
}

TEST_F(VideoConduitTest, TestReconfigureSendMediaCodecWhileTransmitting) {
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_FALSE(Call()->mVideoSendConfig);

  // Defaults
  mControl.Update([&](auto& aControl) { aControl.mTransmitting = true; });
  ASSERT_TRUE(Call()->mVideoSendConfig);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.payload_name, "VP8");
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.payload_type, 120);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.rtcp_mode,
            webrtc::RtcpMode::kCompound);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.max_packet_size, kVideoMtu);
  ASSERT_EQ(Call()->mVideoSendEncoderConfig->content_type,
            VideoEncoderConfig::ContentType::kRealtimeVideo);
  ASSERT_EQ(Call()->mVideoSendEncoderConfig->min_transmit_bitrate_bps, 0);
  ASSERT_EQ(Call()->mVideoSendEncoderConfig->max_bitrate_bps, KBPS(10000));
  ASSERT_EQ(Call()->mVideoSendEncoderConfig->number_of_streams, 1U);

  // Changing these parameters should not require flipping mTransmitting for the
  // changes to take effect.

  // TIAS
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfigTias(120, "VP8", EncodingConstraints());
    codecConfigTias.mEncodings.emplace_back();
    codecConfigTias.mTias = 1000000;
    aControl.mVideoSendCodec = Some(codecConfigTias);
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
  SendVideoFrame(1280, 720, 1);

  {
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(1280, 720);
    ASSERT_EQ(videoStreams.size(), 1U);
    ASSERT_EQ(videoStreams[0].min_bitrate_bps, 200000);
    ASSERT_EQ(videoStreams[0].target_bitrate_bps, 800000);
    ASSERT_EQ(videoStreams[0].max_bitrate_bps, 1000000);
  }

  // MaxBr
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    {
      auto& encoding = codecConfig.mEncodings.emplace_back();
      encoding.constraints.maxBr = 50000;
    }
    aControl.mVideoSendCodec = Some(codecConfig);
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
  SendVideoFrame(1280, 720, 1);
  {
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(1280, 720);
    ASSERT_EQ(videoStreams.size(), 1U);
    ASSERT_LE(videoStreams[0].min_bitrate_bps, 50000);
    ASSERT_LE(videoStreams[0].target_bitrate_bps, 50000);
    ASSERT_EQ(videoStreams[0].max_bitrate_bps, 50000);
  }

  // MaxFs
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodingConstraints.maxFs = 3600;
    {
      auto& encoding = codecConfig.mEncodings.emplace_back();
      encoding.constraints.maxBr = 0;
    }
    aControl.mVideoSendCodec = Some(codecConfig);
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  {
    SendVideoFrame(1280, 720, 1);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 1280U);
    ASSERT_EQ(videoStreams[0].height, 720U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 1000U);
    ASSERT_EQ(sink->mOnFrameCount, 1U);
  }

  {
    SendVideoFrame(640, 360, 2);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 640U);
    ASSERT_EQ(videoStreams[0].height, 360U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 2000U);
    ASSERT_EQ(sink->mOnFrameCount, 2U);
  }

  {
    SendVideoFrame(1920, 1280, 3);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 1174U);
    ASSERT_EQ(videoStreams[0].height, 783U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 3000U);
    ASSERT_EQ(sink->mOnFrameCount, 3U);
  }

  // ScaleResolutionDownBy
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    {
      auto& encoding = codecConfig.mEncodings.emplace_back();
      encoding.constraints.maxFs = 0;
      encoding.constraints.scaleDownBy = 3.7;
    }
    aControl.mVideoSendCodec = Some(codecConfig);
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

  {
    SendVideoFrame(1280, 720, 4);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 345U);
    ASSERT_EQ(videoStreams[0].height, 194U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 4000U);
    ASSERT_EQ(sink->mOnFrameCount, 4U);
  }

  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfigScaleDownBy = *aControl.mVideoSendCodec.Ref();
    codecConfigScaleDownBy.mEncodings[0].constraints.scaleDownBy = 1.3;
    aControl.mVideoSendCodec = Some(codecConfigScaleDownBy);
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

  {
    SendVideoFrame(641, 359, 5);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 493U);
    ASSERT_EQ(videoStreams[0].height, 276U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 5000U);
    ASSERT_EQ(sink->mOnFrameCount, 5U);
  }
}

TEST_F(VideoConduitTest, TestVideoEncode) {
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

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
  ASSERT_EQ(sink->mVideoFrame.width(), 1920);
  ASSERT_EQ(sink->mVideoFrame.height(), 1280);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 3000U);
  ASSERT_EQ(sink->mOnFrameCount, 3U);

  mVideoConduit->RemoveSink(sink.get());
}

TEST_F(VideoConduitTest, TestVideoEncodeMaxFs) {
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodingConstraints.maxFs = 3600;
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  {
    SendVideoFrame(1280, 720, 1);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 1280U);
    ASSERT_EQ(videoStreams[0].height, 720U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 1000U);
    ASSERT_EQ(sink->mOnFrameCount, 1U);
  }

  {
    SendVideoFrame(640, 360, 2);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 640U);
    ASSERT_EQ(videoStreams[0].height, 360U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 2000U);
    ASSERT_EQ(sink->mOnFrameCount, 2U);
  }

  {
    SendVideoFrame(1920, 1280, 3);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 1174U);
    ASSERT_EQ(videoStreams[0].height, 783U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 3000U);
    ASSERT_EQ(sink->mOnFrameCount, 3U);
  }

  // maxFs should not force pixel count above what a sink has requested.
  // We set 3600 macroblocks (16x16 pixels), so we request 3500 here.
  wants.max_pixel_count = 3500 * 16 * 16;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  {
    SendVideoFrame(1280, 720, 4);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 1260U);
    ASSERT_EQ(videoStreams[0].height, 709U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 4000U);
    ASSERT_EQ(sink->mOnFrameCount, 4U);
  }

  {
    SendVideoFrame(640, 360, 5);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 640U);
    ASSERT_EQ(videoStreams[0].height, 360U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 5000U);
    ASSERT_EQ(sink->mOnFrameCount, 5U);
  }

  {
    SendVideoFrame(1920, 1280, 6);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 1158U);
    ASSERT_EQ(videoStreams[0].height, 772U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 6000U);
    ASSERT_EQ(sink->mOnFrameCount, 6U);
  }

  mVideoConduit->RemoveSink(sink.get());
}

TEST_F(VideoConduitTest, TestVideoEncodeMaxFsNegotiatedThenSinkWants) {
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    codecConfig.mEncodingConstraints.maxFs = 3500;
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  unsigned int frame = 0;

  {
    SendVideoFrame(1280, 720, frame++);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 1260U);
    ASSERT_EQ(videoStreams[0].height, 709U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), (frame - 1) * 1000);
    ASSERT_EQ(sink->mOnFrameCount, frame);
  }

  wants.max_pixel_count = 3600 * 16 * 16;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  {
    SendVideoFrame(1280, 720, frame++);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 1260U);
    ASSERT_EQ(videoStreams[0].height, 709U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), (frame - 1) * 1000);
    ASSERT_EQ(sink->mOnFrameCount, frame);
  }

  mVideoConduit->RemoveSink(sink.get());
}

TEST_F(VideoConduitTest, TestVideoEncodeMaxFsCodecChange) {
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    codecConfig.mEncodingConstraints.maxFs = 3500;
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  unsigned int frame = 0;

  {
    SendVideoFrame(1280, 720, frame++);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 1260U);
    ASSERT_EQ(videoStreams[0].height, 709U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), (frame - 1) * 1000);
    ASSERT_EQ(sink->mOnFrameCount, frame);
  }

  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig(121, "VP9", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    codecConfig.mEncodingConstraints.maxFs = 3500;
    aControl.mVideoSendCodec = Some(codecConfig);
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

  {
    SendVideoFrame(1280, 720, frame++);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 1260U);
    ASSERT_EQ(videoStreams[0].height, 709U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), (frame - 1) * 1000);
    ASSERT_EQ(sink->mOnFrameCount, frame);
  }

  mVideoConduit->RemoveSink(sink.get());
}

TEST_F(VideoConduitTest, TestVideoEncodeMaxFsSinkWantsThenCodecChange) {
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  wants.max_pixel_count = 3500 * 16 * 16;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  unsigned int frame = 0;

  SendVideoFrame(1280, 720, frame++);
  const std::vector<webrtc::VideoStream> videoStreams =
      Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                   sink->mVideoFrame.height());
  ASSERT_EQ(videoStreams[0].width, 1260U);
  ASSERT_EQ(videoStreams[0].height, 709U);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), (frame - 1) * 1000);
  ASSERT_EQ(sink->mOnFrameCount, frame);

  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig(121, "VP9", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

  {
    SendVideoFrame(1280, 720, frame++);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 1260U);
    ASSERT_EQ(videoStreams[0].height, 709U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), (frame - 1) * 1000);
    ASSERT_EQ(sink->mOnFrameCount, frame);
  }

  mVideoConduit->RemoveSink(sink.get());
}

TEST_F(VideoConduitTest, TestVideoEncodeMaxFsNegotiated) {
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  unsigned int frame = 0;
  SendVideoFrame(1280, 720, frame++);
  ASSERT_EQ(sink->mVideoFrame.width(), 1280);
  ASSERT_EQ(sink->mVideoFrame.height(), 720);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), (frame - 1) * 1000);
  ASSERT_EQ(sink->mOnFrameCount, frame);

  // Ensure that negotiating a new max-fs works
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig = *aControl.mVideoSendCodec.Ref();
    codecConfig.mEncodingConstraints.maxFs = 3500;
    aControl.mVideoSendCodec = Some(codecConfig);
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

  {
    SendVideoFrame(1280, 720, frame++);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 1260U);
    ASSERT_EQ(videoStreams[0].height, 709U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), (frame - 1) * 1000);
    ASSERT_EQ(sink->mOnFrameCount, frame);
  }

  // Ensure that negotiating max-fs away works
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig = *aControl.mVideoSendCodec.Ref();
    codecConfig.mEncodingConstraints.maxFs = 0;
    aControl.mVideoSendCodec = Some(codecConfig);
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

  SendVideoFrame(1280, 720, frame++);
  ASSERT_EQ(sink->mVideoFrame.width(), 1280);
  ASSERT_EQ(sink->mVideoFrame.height(), 720);
  ASSERT_EQ(sink->mVideoFrame.timestamp_us(), (frame - 1) * 1000);
  ASSERT_EQ(sink->mOnFrameCount, frame);

  mVideoConduit->RemoveSink(sink.get());
}

TEST_F(VideoConduitTest, TestVideoEncodeMaxWidthAndHeight) {
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodingConstraints.maxWidth = 1280;
    codecConfig.mEncodingConstraints.maxHeight = 720;
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

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

  {
    SendVideoFrame(1920, 1280, 3);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 1080U);
    ASSERT_EQ(videoStreams[0].height, 720U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 3000U);
    ASSERT_EQ(sink->mOnFrameCount, 3U);
  }

  mVideoConduit->RemoveSink(sink.get());
}

TEST_F(VideoConduitTest, TestVideoEncodeScaleResolutionBy) {
  mControl.Update([&](auto& aControl) {
    aControl.mTransmitting = true;
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodingConstraints.maxFs = 3600;
    auto& encoding = codecConfig.mEncodings.emplace_back();
    encoding.constraints.scaleDownBy = 2;
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  {
    SendVideoFrame(1280, 720, 1);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 640U);
    ASSERT_EQ(videoStreams[0].height, 360U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 1000U);
    ASSERT_EQ(sink->mOnFrameCount, 1U);
  }

  {
    SendVideoFrame(640, 360, 2);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 320U);
    ASSERT_EQ(videoStreams[0].height, 180U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 2000U);
    ASSERT_EQ(sink->mOnFrameCount, 2U);
    mVideoConduit->RemoveSink(sink.get());
  }
}

TEST_F(VideoConduitTest, TestVideoEncodeSimulcastScaleResolutionBy) {
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    {
      auto& encoding = codecConfig.mEncodings.emplace_back();
      encoding.constraints.scaleDownBy = 2;
    }
    {
      auto& encoding = codecConfig.mEncodings.emplace_back();
      encoding.constraints.scaleDownBy = 3;
    }
    {
      auto& encoding = codecConfig.mEncodings.emplace_back();
      encoding.constraints.scaleDownBy = 4;
    }

    aControl.mTransmitting = true;
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
    aControl.mLocalSsrcs = {42, 43, 44};
    aControl.mLocalVideoRtxSsrcs = {45, 46, 47};
  });
  ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

  UniquePtr<MockVideoSink> sink(new MockVideoSink());
  rtc::VideoSinkWants wants;
  mVideoConduit->AddOrUpdateSink(sink.get(), wants);

  {
    SendVideoFrame(640, 480, 1);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 320U);
    ASSERT_EQ(videoStreams[0].height, 240U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 1000U);
    ASSERT_EQ(sink->mOnFrameCount, 1U);
  }

  {
    SendVideoFrame(1280, 720, 2);
    const std::vector<webrtc::VideoStream> videoStreams =
        Call()->CreateEncoderStreams(sink->mVideoFrame.width(),
                                     sink->mVideoFrame.height());
    ASSERT_EQ(videoStreams[0].width, 640U);
    ASSERT_EQ(videoStreams[0].height, 360U);
    ASSERT_EQ(sink->mVideoFrame.timestamp_us(), 2000U);
    ASSERT_EQ(sink->mOnFrameCount, 2U);
    mVideoConduit->RemoveSink(sink.get());
  }
}

TEST_F(VideoConduitTest, TestVideoEncodeLargeScaleResolutionByFrameDropping) {
  for (const auto& scales :
       {std::vector{200U}, std::vector{200U, 300U}, std::vector{300U, 200U}}) {
    mControl.Update([&](auto& aControl) {
      aControl.mTransmitting = true;
      VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
      for (const auto& scale : scales) {
        auto& encoding = codecConfig.mEncodings.emplace_back();
        encoding.constraints.scaleDownBy = scale;
      }
      aControl.mVideoSendCodec = Some(codecConfig);
      aControl.mVideoSendRtpRtcpConfig =
          Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
      aControl.mLocalSsrcs = scales;
    });
    ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

    UniquePtr<MockVideoSink> sink(new MockVideoSink());
    rtc::VideoSinkWants wants;
    mVideoConduit->AddOrUpdateSink(sink.get(), wants);

    {
      // If all layers' scaleDownBy is larger than any input dimension, that
      // dimension becomes zero and we drop it.
      // NB: libwebrtc doesn't CreateEncoderStreams() unless there's a real
      //     frame, so no reason to call it here.
      SendVideoFrame(199, 199, 1);
      EXPECT_EQ(sink->mOnFrameCount, 0U);
    }

    {
      // If only width becomes zero, we drop.
      SendVideoFrame(199, 200, 2);
      EXPECT_EQ(sink->mOnFrameCount, 0U);
    }

    {
      // If only height becomes zero, we drop.
      SendVideoFrame(200, 199, 3);
      EXPECT_EQ(sink->mOnFrameCount, 0U);
    }

    {
      // If dimensions are non-zero, we pass through.
      SendVideoFrame(200, 200, 4);
      EXPECT_EQ(sink->mOnFrameCount, 1U);
    }

    mVideoConduit->RemoveSink(sink.get());
  }
}

TEST_F(VideoConduitTest, TestVideoEncodeLargeScaleResolutionByStreamCreation) {
  for (const auto& scales :
       {std::vector{200U}, std::vector{200U, 300U}, std::vector{300U, 200U}}) {
    mControl.Update([&](auto& aControl) {
      aControl.mTransmitting = true;
      VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
      for (const auto& scale : scales) {
        auto& encoding = codecConfig.mEncodings.emplace_back();
        encoding.constraints.scaleDownBy = scale;
      }
      aControl.mVideoSendCodec = Some(codecConfig);
      aControl.mVideoSendRtpRtcpConfig =
          Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
      aControl.mLocalSsrcs = scales;
    });
    ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

    {
      // If dimensions scale to <1, we create a 1x1 stream.
      const std::vector<webrtc::VideoStream> videoStreams =
          Call()->CreateEncoderStreams(199, 199);
      ASSERT_EQ(videoStreams.size(), scales.size());
      for (const auto& stream : videoStreams) {
        EXPECT_EQ(stream.width, 1U);
        EXPECT_EQ(stream.height, 1U);
      }
    }

    {
      // If width scales to <1, we create a 1x1 stream.
      const std::vector<webrtc::VideoStream> videoStreams =
          Call()->CreateEncoderStreams(199, 200);
      ASSERT_EQ(videoStreams.size(), scales.size());
      for (const auto& stream : videoStreams) {
        EXPECT_EQ(stream.width, 1U);
        EXPECT_EQ(stream.height, 1U);
      }
    }

    {
      // If height scales to <1, we create a 1x1 stream.
      const std::vector<webrtc::VideoStream> videoStreams =
          Call()->CreateEncoderStreams(200, 199);
      ASSERT_EQ(videoStreams.size(), scales.size());
      for (const auto& stream : videoStreams) {
        EXPECT_EQ(stream.width, 1U);
        EXPECT_EQ(stream.height, 1U);
      }
    }

    {
      // If dimensions scale to 1, we create a 1x1 stream.
      const std::vector<webrtc::VideoStream> videoStreams =
          Call()->CreateEncoderStreams(200, 200);
      ASSERT_EQ(videoStreams.size(), scales.size());
      for (const auto& stream : videoStreams) {
        EXPECT_EQ(stream.width, 1U);
        EXPECT_EQ(stream.height, 1U);
      }
    }

    {
      // If one dimension scales to 0 and the other >1, we create a 1x1 stream.
      const std::vector<webrtc::VideoStream> videoStreams =
          Call()->CreateEncoderStreams(400, 199);
      ASSERT_EQ(videoStreams.size(), scales.size());
      for (const auto& stream : videoStreams) {
        EXPECT_EQ(stream.width, 1U);
        EXPECT_EQ(stream.height, 1U);
      }
    }

    {
      // Legit case scaling down to more than 1x1.
      const std::vector<webrtc::VideoStream> videoStreams =
          Call()->CreateEncoderStreams(600, 400);
      ASSERT_EQ(videoStreams.size(), scales.size());
      for (size_t i = 0; i < scales.size(); ++i) {
        // Streams are backwards for some reason
        const auto& stream = videoStreams[i];
        const auto& scale = scales[i];
        if (scale == 200U) {
          EXPECT_EQ(stream.width, 3U);
          EXPECT_EQ(stream.height, 2U);
        } else {
          EXPECT_EQ(stream.width, 2U);
          EXPECT_EQ(stream.height, 1U);
        }
      }
    }
  }
}

TEST_F(VideoConduitTest, TestVideoEncodeResolutionAlignment) {
  UniquePtr<MockVideoSink> sink(new MockVideoSink());

  for (const auto& scales : {std::vector{1U}, std::vector{1U, 9U}}) {
    mControl.Update([&](auto& aControl) {
      aControl.mTransmitting = true;
      VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
      for (const auto& scale : scales) {
        auto& encoding = codecConfig.mEncodings.emplace_back();
        encoding.constraints.scaleDownBy = scale;
      }
      aControl.mVideoSendCodec = Some(codecConfig);
      aControl.mVideoSendRtpRtcpConfig =
          Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
      aControl.mLocalSsrcs = scales;
    });
    ASSERT_TRUE(Call()->mVideoSendEncoderConfig);

    for (const auto& alignment : {2, 16, 39, 400, 1000}) {
      // Test that requesting specific alignment always results in the expected
      // number of layers and valid alignment.
      rtc::VideoSinkWants wants;
      wants.resolution_alignment = alignment;
      mVideoConduit->AddOrUpdateSink(sink.get(), wants);

      const std::vector<webrtc::VideoStream> videoStreams =
          Call()->CreateEncoderStreams(640, 480);
      ASSERT_EQ(videoStreams.size(), scales.size());
      for (size_t i = 0; i < videoStreams.size(); ++i) {
        // videoStreams is backwards
        const auto& stream = videoStreams[i];
        const auto& scale = scales[i];
        uint32_t expectation =
            480 / scale < static_cast<uint32_t>(alignment) ? 1 : 0;
        EXPECT_EQ(stream.width % alignment, expectation)
            << " for scale " << scale << " and alignment " << alignment;
        EXPECT_EQ(stream.height % alignment, expectation);
      }
    }
  }

  mVideoConduit->RemoveSink(sink.get());
}

TEST_F(VideoConduitTest, TestSettingRtpRtcpRsize) {
  mControl.Update([&](auto& aControl) {
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    RtpRtcpConfig rtcpConf(webrtc::RtcpMode::kReducedSize);

    aControl.mReceiving = true;
    aControl.mVideoRecvCodecs = {codecConfig};
    aControl.mVideoRecvRtpRtcpConfig = Some(rtcpConf);
    aControl.mTransmitting = true;
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig = Some(rtcpConf);
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.rtcp_mode,
            webrtc::RtcpMode::kReducedSize);
  ASSERT_TRUE(Call()->mVideoSendConfig);
  ASSERT_EQ(Call()->mVideoSendConfig->rtp.rtcp_mode,
            webrtc::RtcpMode::kReducedSize);
}

TEST_F(VideoConduitTest, TestRemoteSsrcDefault) {
  mControl.Update([&](auto& aControl) {
    aControl.mRemoteSsrc = 0;
    aControl.mLocalSsrcs = {1};
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
    aControl.mReceiving = true;
    aControl.mTransmitting = true;
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_THAT(Call()->mVideoReceiveConfig->rtp.remote_ssrc,
              Not(testing::AnyOf(0U, 1U)));
  ASSERT_TRUE(Call()->mVideoSendConfig);
  ASSERT_THAT(Call()->mVideoSendConfig->rtp.ssrcs, ElementsAre(1U));
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.local_ssrc,
            Call()->mVideoSendConfig->rtp.ssrcs[0]);
}

TEST_F(VideoConduitTest, TestRemoteSsrcCollision) {
  mControl.Update([&](auto& aControl) {
    aControl.mRemoteSsrc = 1;
    aControl.mLocalSsrcs = {1};
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
    aControl.mReceiving = true;
    aControl.mTransmitting = true;
  });
  EXPECT_TRUE(Call()->mVideoReceiveConfig);
  EXPECT_EQ(Call()->mVideoReceiveConfig->rtp.remote_ssrc, 1U);
  EXPECT_TRUE(Call()->mVideoSendConfig);
  EXPECT_THAT(Call()->mVideoSendConfig->rtp.ssrcs,
              ElementsAre(Not(testing::AnyOf(0U, 1U))));
  EXPECT_EQ(Call()->mVideoReceiveConfig->rtp.local_ssrc,
            Call()->mVideoSendConfig->rtp.ssrcs[0]);
}

TEST_F(VideoConduitTest, TestLocalSsrcDefault) {
  mControl.Update([&](auto& aControl) {
    aControl.mRemoteSsrc = 1;
    aControl.mLocalSsrcs = {};
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
    aControl.mReceiving = true;
    aControl.mTransmitting = true;
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.remote_ssrc, 1U);
  ASSERT_TRUE(Call()->mVideoSendConfig);
  ASSERT_THAT(Call()->mVideoSendConfig->rtp.ssrcs,
              ElementsAre(Not(testing::AnyOf(0U, 1U))));
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.local_ssrc,
            Call()->mVideoSendConfig->rtp.ssrcs[0]);
}

TEST_F(VideoConduitTest, TestLocalSsrcCollision) {
  mControl.Update([&](auto& aControl) {
    aControl.mRemoteSsrc = 1;
    aControl.mLocalSsrcs = {2, 2};
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    codecConfig.mEncodings.emplace_back();
    codecConfig.mEncodings.emplace_back();
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
    aControl.mReceiving = true;
    aControl.mTransmitting = true;
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.remote_ssrc, 1U);
  ASSERT_TRUE(Call()->mVideoSendConfig);
  ASSERT_THAT(Call()->mVideoSendConfig->rtp.ssrcs,
              ElementsAre(2U, Not(testing::AnyOf(0U, 2U))));
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.local_ssrc,
            Call()->mVideoSendConfig->rtp.ssrcs[0]);
}

TEST_F(VideoConduitTest, TestLocalSsrcUnorderedCollision) {
  mControl.Update([&](auto& aControl) {
    aControl.mRemoteSsrc = 1;
    aControl.mLocalSsrcs = {2, 3, 2};
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    for (int i = 0; i < 3; ++i) {
      codecConfig.mEncodings.emplace_back();
    }
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
    aControl.mReceiving = true;
    aControl.mTransmitting = true;
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.remote_ssrc, 1U);
  ASSERT_TRUE(Call()->mVideoSendConfig);
  ASSERT_THAT(Call()->mVideoSendConfig->rtp.ssrcs,
              ElementsAre(2U, 3U, Not(testing::AnyOf(0U, 2U))));
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.local_ssrc,
            Call()->mVideoSendConfig->rtp.ssrcs[0]);
}

TEST_F(VideoConduitTest, TestLocalAndRemoteSsrcCollision) {
  mControl.Update([&](auto& aControl) {
    aControl.mRemoteSsrc = 1;
    aControl.mLocalSsrcs = {1, 2, 2};
    VideoCodecConfig codecConfig(120, "VP8", EncodingConstraints());
    for (int i = 0; i < 3; ++i) {
      codecConfig.mEncodings.emplace_back();
    }
    aControl.mVideoSendCodec = Some(codecConfig);
    aControl.mVideoSendRtpRtcpConfig =
        Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
    aControl.mReceiving = true;
    aControl.mTransmitting = true;
  });
  ASSERT_TRUE(Call()->mVideoReceiveConfig);
  ASSERT_THAT(Call()->mVideoReceiveConfig->rtp.remote_ssrc, 1U);
  ASSERT_TRUE(Call()->mVideoSendConfig);
  ASSERT_THAT(Call()->mVideoSendConfig->rtp.ssrcs,
              ElementsAre(Not(testing::AnyOf(0U, 1U, 2U)), 2U,
                          Not(testing::AnyOf(
                              0U, 1U, 2U,
                              Call()->mVideoReceiveConfig->rtp.remote_ssrc))));
  ASSERT_EQ(Call()->mVideoReceiveConfig->rtp.local_ssrc,
            Call()->mVideoSendConfig->rtp.ssrcs[0]);
}

TEST_F(VideoConduitTest, TestExternalRemoteSsrcCollision) {
  auto other = MakeRefPtr<MockConduit>();
  mCallWrapper->RegisterConduit(other);

  // First the mControl update should trigger an UnsetRemoteSSRC(1) from us.
  // Then we simulate another conduit using that same ssrc, which should trigger
  // us to generate a fresh ssrc that is not 0 and not 1.
  {
    InSequence s;
    EXPECT_CALL(*other, UnsetRemoteSSRC(1U)).Times(2);
    EXPECT_CALL(*other, UnsetRemoteSSRC(Not(testing::AnyOf(0U, 1U))));
  }

  mControl.Update([&](auto& aControl) {
    aControl.mRemoteSsrc = 1;
    aControl.mReceiving = true;
  });
  EXPECT_TRUE(Call()->mVideoReceiveConfig);
  EXPECT_EQ(Call()->mVideoReceiveConfig->rtp.remote_ssrc, 1U);

  mozilla::Unused << WaitFor(InvokeAsync(
      GetCurrentSerialEventTarget(), __func__, [wrapper = mCallWrapper] {
        wrapper->UnsetRemoteSSRC(1);
        return GenericPromise::CreateAndResolve(true, __func__);
      }));

  EXPECT_TRUE(Call()->mVideoReceiveConfig);
  EXPECT_THAT(Call()->mVideoReceiveConfig->rtp.remote_ssrc,
              Not(testing::AnyOf(0U, 1U)));
}

TEST_F(VideoConduitTest, TestVideoConfigurationH264) {
  const int profileLevelId1 = 0x42E01F;
  const int profileLevelId2 = 0x64000C;
  const char* sprop1 = "foo bar";
  const char* sprop2 = "baz";

  // Test that VideoConduit propagates H264 configuration data properly.
  // We do two tests:
  // - Test valid data in packetization mode 0 (SingleNALU)
  // - Test different valid data in packetization mode 1 (NonInterleaved)

  {
    mControl.Update([&](auto& aControl) {
      aControl.mTransmitting = true;
      VideoCodecConfigH264 h264{};
      h264.packetization_mode = 0;
      h264.profile_level_id = profileLevelId1;
      strncpy(h264.sprop_parameter_sets, sprop1,
              sizeof(h264.sprop_parameter_sets) - 1);
      VideoCodecConfig codecConfig(97, "H264", EncodingConstraints(), &h264);
      codecConfig.mEncodings.emplace_back();
      aControl.mVideoSendCodec = Some(codecConfig);
      aControl.mVideoSendRtpRtcpConfig =
          Some(RtpRtcpConfig(webrtc::RtcpMode::kCompound));
    });

    ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
    auto& params = Call()->mVideoSendEncoderConfig->video_format.parameters;
    EXPECT_EQ(params[cricket::kH264FmtpPacketizationMode], "0");
    EXPECT_EQ(params[cricket::kH264FmtpProfileLevelId], "42e01f");
    EXPECT_EQ(params[cricket::kH264FmtpSpropParameterSets], sprop1);
  }

  {
    mControl.Update([&](auto& aControl) {
      VideoCodecConfigH264 h264{};
      h264.packetization_mode = 1;
      h264.profile_level_id = profileLevelId2;
      strncpy(h264.sprop_parameter_sets, sprop2,
              sizeof(h264.sprop_parameter_sets) - 1);
      VideoCodecConfig codecConfig(126, "H264", EncodingConstraints(), &h264);
      codecConfig.mEncodings.emplace_back();
      aControl.mVideoSendCodec = Some(codecConfig);
    });

    ASSERT_TRUE(Call()->mVideoSendEncoderConfig);
    auto& params = Call()->mVideoSendEncoderConfig->video_format.parameters;
    EXPECT_EQ(params[cricket::kH264FmtpPacketizationMode], "1");
    EXPECT_EQ(params[cricket::kH264FmtpProfileLevelId], "64000c");
    EXPECT_EQ(params[cricket::kH264FmtpSpropParameterSets], sprop2);
  }
}

}  // End namespace test.
