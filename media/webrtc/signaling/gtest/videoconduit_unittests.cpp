
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

#include "VideoConduit.h"
#include "WebrtcGmpVideoCodec.h"

#include "webrtc/media/base/videoadapter.h"

#include "MockCall.h"

using namespace mozilla;

namespace test {

class MockVideoAdapter : public cricket::VideoAdapter {
public:

  bool AdaptFrameResolution(int in_width,
                            int in_height,
                            int64_t in_timestamp_ns,
                            int* cropped_width,
                            int* cropped_height,
                            int* out_width,
                            int* out_height) override
  {
    mInWidth = in_width;
    mInHeight = in_height;
    mInTimestampNs = in_timestamp_ns;
    return true;
  }

  void OnResolutionRequest(rtc::Optional<int> max_pixel_count,
                           rtc::Optional<int> max_pixel_count_step_up) override
  {
    mMaxPixelCount = max_pixel_count.value_or(-1);
    mMaxPixelCountStepUp = max_pixel_count_step_up.value_or(-1);
  }

  void OnScaleResolutionBy(rtc::Optional<float> scale_resolution_by) override
  {
    mScaleResolutionBy = scale_resolution_by.value_or(-1.0);
  }

  int mInWidth;
  int mInHeight;
  int64_t mInTimestampNs;
  int mMaxPixelCount;
  int mMaxPixelCountStepUp;
  int mScaleResolutionBy;
};

class VideoConduitTest : public ::testing::Test {
public:

  VideoConduitTest()
    : mCall(new MockCall())
    , mAdapter(new MockVideoAdapter)
  {
    mVideoConduit = new WebrtcVideoConduit(WebRtcCallWrapper::Create(UniquePtr<MockCall>(mCall)),
                                           UniquePtr<cricket::VideoAdapter>(mAdapter));
    std::vector<unsigned int> ssrcs = {42};
    mVideoConduit->SetLocalSSRCs(ssrcs);
  }

  ~VideoConduitTest() override = default;

  MediaConduitErrorCode SendVideoFrame(unsigned short width,
                                       unsigned short height)
  {
    unsigned int yplane_length = width*height;
    unsigned int cbcrplane_length = (width*height + 1)/2;
    unsigned int video_length = yplane_length + cbcrplane_length;
    uint8_t* buffer = new uint8_t[video_length];
    memset(buffer, 0x10, yplane_length);
    memset(buffer + yplane_length, 0x80, cbcrplane_length);
    return mVideoConduit->SendVideoFrame(buffer, video_length, width, height,
                                         VideoType::kVideoI420, 1);
  }

  MockCall* mCall;
  MockVideoAdapter* mAdapter;
  RefPtr<mozilla::WebrtcVideoConduit> mVideoConduit;
};

TEST_F(VideoConduitTest, TestConfigureSendMediaCodec)
{
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::SimulcastEncoding encoding;

  // defaults
  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mSimulcastEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mVideoSendConfig.encoder_settings.payload_name, "VP8");
  ASSERT_EQ(mCall->mVideoSendConfig.encoder_settings.payload_type, 120);
  ASSERT_EQ(mCall->mVideoSendConfig.encoder_settings.internal_source, false);
  ASSERT_EQ(mCall->mVideoSendConfig.encoder_settings.full_overuse_time, false);
  ASSERT_NE(mCall->mVideoSendConfig.encoder_settings.encoder, nullptr);
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.rtcp_mode, webrtc::RtcpMode::kCompound);
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.max_packet_size, kVideoMtu);
  ASSERT_EQ(mCall->mEncoderConfig.content_type,
            VideoEncoderConfig::ContentType::kRealtimeVideo);
  ASSERT_EQ(mCall->mEncoderConfig.min_transmit_bitrate_bps, 0);
  ASSERT_EQ(mCall->mEncoderConfig.max_bitrate_bps, 0);
  ASSERT_EQ(mCall->mEncoderConfig.number_of_streams, 1U);
  ASSERT_EQ(mCall->mEncoderConfig.resolution_divisor, 1);
  mVideoConduit->StopTransmitting();

  // null codec
  ec = mVideoConduit->ConfigureSendMediaCodec(nullptr);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);

  // empty codec name
  VideoCodecConfig codecConfigBadName(120, "", constraints);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigBadName);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);

  // long codec name
  size_t longNameLength = WebrtcVideoConduit::CODEC_PLNAME_SIZE + 2;
  char* longName = new char[longNameLength];
  memset(longName, 'A', longNameLength - 2);
  longName[longNameLength - 1] = 0;
  VideoCodecConfig codecConfigLongName(120, longName, constraints);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigBadName);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);
  delete[] longName;
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecMaxFps)
{
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::SimulcastEncoding encoding;

  constraints.maxFps = 0;
  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mSimulcastEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  std::vector<webrtc::VideoStream> videoStreams;
  videoStreams = mCall->mEncoderConfig.video_stream_factory->CreateEncoderStreams(640, 480, mCall->mEncoderConfig);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].max_framerate, 30); // DEFAULT_VIDEO_MAX_FRAMERATE
  mVideoConduit->StopTransmitting();

  constraints.maxFps = 42;
  VideoCodecConfig codecConfig2(120, "VP8", constraints);
  codecConfig.mSimulcastEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig2);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  videoStreams = mCall->mEncoderConfig.video_stream_factory->CreateEncoderStreams(640, 480, mCall->mEncoderConfig);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].max_framerate, 42);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecMaxMbps)
{
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::SimulcastEncoding encoding;

  constraints.maxMbps = 0;
  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mSimulcastEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  SendVideoFrame(640, 480);
  std::vector<webrtc::VideoStream> videoStreams;
  videoStreams = mCall->mEncoderConfig.video_stream_factory->CreateEncoderStreams(640, 480, mCall->mEncoderConfig);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].max_framerate, 30); // DEFAULT_VIDEO_MAX_FRAMERATE
  mVideoConduit->StopTransmitting();

  constraints.maxMbps = 10000;
  VideoCodecConfig codecConfig2(120, "VP8", constraints);
  codecConfig.mSimulcastEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig2);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  SendVideoFrame(640, 480);
  videoStreams = mCall->mEncoderConfig.video_stream_factory->CreateEncoderStreams(640, 480, mCall->mEncoderConfig);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].max_framerate, 8);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecDefaults)
{
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::SimulcastEncoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mSimulcastEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  videoStreams = mCall->mEncoderConfig.video_stream_factory->CreateEncoderStreams(640, 480, mCall->mEncoderConfig);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].min_bitrate_bps, static_cast<int32_t>(WebrtcVideoConduit::kDefaultMinBitrate_bps));
  ASSERT_EQ(videoStreams[0].target_bitrate_bps, static_cast<int32_t>(WebrtcVideoConduit::kDefaultStartBitrate_bps));
  ASSERT_EQ(videoStreams[0].max_bitrate_bps, static_cast<int32_t>(WebrtcVideoConduit::kDefaultMaxBitrate_bps));

  // SelectBitrates not called until we send a frame
  SendVideoFrame(1280, 720);
  videoStreams = mCall->mEncoderConfig.video_stream_factory->CreateEncoderStreams(1280, 720, mCall->mEncoderConfig);
  ASSERT_EQ(videoStreams.size(), 1U);
  // These values come from a table and are determined by resolution
  ASSERT_EQ(videoStreams[0].min_bitrate_bps, 600000);
  ASSERT_EQ(videoStreams[0].target_bitrate_bps, 800000);
  ASSERT_EQ(videoStreams[0].max_bitrate_bps, 2500000);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecTias)
{
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::SimulcastEncoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;

  // TIAS
  VideoCodecConfig codecConfigTias(120, "VP8", constraints);
  codecConfigTias.mSimulcastEncodings.push_back(encoding);
  codecConfigTias.mTias = 1000000;
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigTias);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  SendVideoFrame(1280, 720);
  videoStreams = mCall->mEncoderConfig.video_stream_factory->CreateEncoderStreams(1280, 720, mCall->mEncoderConfig);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].min_bitrate_bps, 600000);
  ASSERT_EQ(videoStreams[0].target_bitrate_bps, 800000);
  ASSERT_EQ(videoStreams[0].max_bitrate_bps, 1000000);
  mVideoConduit->StopTransmitting();

  // TIAS (too low)
  VideoCodecConfig codecConfigTiasLow(120, "VP8", constraints);
  codecConfigTiasLow.mSimulcastEncodings.push_back(encoding);
  codecConfigTiasLow.mTias = 1000;
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfigTiasLow);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  SendVideoFrame(1280, 720);
  videoStreams = mCall->mEncoderConfig.video_stream_factory->CreateEncoderStreams(1280, 720, mCall->mEncoderConfig);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_EQ(videoStreams[0].min_bitrate_bps, 30000);
  ASSERT_EQ(videoStreams[0].target_bitrate_bps, 30000);
  ASSERT_EQ(videoStreams[0].max_bitrate_bps, 30000);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecMaxBr)
{
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::SimulcastEncoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  encoding.constraints.maxBr = 50000;
  codecConfig.mSimulcastEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  SendVideoFrame(1280, 720);
  videoStreams = mCall->mEncoderConfig.video_stream_factory->CreateEncoderStreams(1280, 720, mCall->mEncoderConfig);
  ASSERT_EQ(videoStreams.size(), 1U);
  ASSERT_LE(videoStreams[0].min_bitrate_bps, 50000);
  ASSERT_LE(videoStreams[0].target_bitrate_bps, 50000);
  ASSERT_EQ(videoStreams[0].max_bitrate_bps, 50000);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecScaleResolutionBy)
{
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::SimulcastEncoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;

  std::vector<unsigned int> ssrcs = {42, 1729};
  mVideoConduit->SetLocalSSRCs(ssrcs);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  encoding.constraints.scaleDownBy = 2;
  codecConfig.mSimulcastEncodings.push_back(encoding);
  encoding.constraints.scaleDownBy = 4;
  codecConfig.mSimulcastEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mAdapter->mScaleResolutionBy, 2);
  SendVideoFrame(640, 360);
  videoStreams = mCall->mEncoderConfig.video_stream_factory->CreateEncoderStreams(640, 360, mCall->mEncoderConfig);
  ASSERT_EQ(videoStreams.size(), 2U);
  ASSERT_EQ(videoStreams[0].width, 320U);
  ASSERT_EQ(videoStreams[0].height, 180U);
  ASSERT_EQ(videoStreams[1].width, 640U);
  ASSERT_EQ(videoStreams[1].height, 360U);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecCodecMode)
{
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::SimulcastEncoding encoding;
  std::vector<webrtc::VideoStream> videoStreams;

  mVideoConduit->ConfigureCodecMode(webrtc::VideoCodecMode::kScreensharing);

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mSimulcastEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mEncoderConfig.content_type,
            VideoEncoderConfig::ContentType::kScreen);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecFEC)
{
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::SimulcastEncoding encoding;

  WebrtcGmpPCHandleSetter setter("hi there");

  // H264 + FEC
  VideoCodecConfig codecConfig(120, "H264", constraints);
  codecConfig.mSimulcastEncodings.push_back(encoding);
  codecConfig.mFECFbSet = true;
  codecConfig.mULPFECPayloadType = 1;
  codecConfig.mREDPayloadType = 2;
  codecConfig.mREDRTXPayloadType = 3;
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.ulpfec_payload_type, codecConfig.mULPFECPayloadType);
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.red_payload_type, codecConfig.mREDPayloadType);
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.red_rtx_payload_type, codecConfig.mREDRTXPayloadType);
  mVideoConduit->StopTransmitting();

  // H264 + FEC + Nack
  codecConfig.mFECFbSet = true;
  codecConfig.mNackFbTypes.push_back("");
  codecConfig.mULPFECPayloadType = 1;
  codecConfig.mREDPayloadType = 2;
  codecConfig.mREDRTXPayloadType = 3;
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.ulpfec_payload_type, -1);
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.red_payload_type, -1);
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.red_rtx_payload_type, -1);
  mVideoConduit->StopTransmitting();

  // VP8 + FEC + Nack
  VideoCodecConfig codecConfig2(120, "VP8", constraints);
  codecConfig2.mSimulcastEncodings.push_back(encoding);
  codecConfig2.mFECFbSet = true;
  codecConfig2.mNackFbTypes.push_back("");
  codecConfig2.mULPFECPayloadType = 1;
  codecConfig2.mREDPayloadType = 2;
  codecConfig2.mREDRTXPayloadType = 3;
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig2);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.ulpfec_payload_type, codecConfig.mULPFECPayloadType);
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.red_payload_type, codecConfig.mREDPayloadType);
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.ulpfec.red_rtx_payload_type, codecConfig.mREDRTXPayloadType);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecNack)
{
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::SimulcastEncoding encoding;

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mSimulcastEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.nack.rtp_history_ms, 0);
  mVideoConduit->StopTransmitting();

  codecConfig.mNackFbTypes.push_back("");
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.nack.rtp_history_ms, 1000);
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestConfigureSendMediaCodecRids)
{
  MediaConduitErrorCode ec;
  EncodingConstraints constraints;
  VideoCodecConfig::SimulcastEncoding encoding;

  VideoCodecConfig codecConfig(120, "VP8", constraints);
  codecConfig.mSimulcastEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.rids.size(), 0U);
  mVideoConduit->StopTransmitting();

  std::vector<unsigned int> ssrcs = {42, 1729};
  mVideoConduit->SetLocalSSRCs(ssrcs);

  codecConfig.mSimulcastEncodings.clear();
  encoding.rid = "1";
  codecConfig.mSimulcastEncodings.push_back(encoding);
  encoding.rid = "2";
  codecConfig.mSimulcastEncodings.push_back(encoding);
  ec = mVideoConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mVideoConduit->StartTransmitting();
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.rids.size(), 2U);
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.rids[0], "2");
  ASSERT_EQ(mCall->mVideoSendConfig.rtp.rids[1], "1");
  mVideoConduit->StopTransmitting();
}

TEST_F(VideoConduitTest, TestOnSinkWantsChanged)
{
  rtc::VideoSinkWants wants;
  wants.max_pixel_count = rtc::Optional<int>(256000);
  EncodingConstraints constraints;
  VideoCodecConfig codecConfig(120, "VP8", constraints);

  codecConfig.mEncodingConstraints.maxFs = 0;
  mVideoConduit->ConfigureSendMediaCodec(&codecConfig);
  mVideoConduit->OnSinkWantsChanged(wants);
  ASSERT_EQ(mAdapter->mMaxPixelCount, 256000);

  codecConfig.mEncodingConstraints.maxFs = 500;
  mVideoConduit->ConfigureSendMediaCodec(&codecConfig);
  mVideoConduit->OnSinkWantsChanged(wants);
  ASSERT_EQ(mAdapter->mMaxPixelCount, 500*16*16); //convert macroblocks to pixels

  codecConfig.mEncodingConstraints.maxFs = 1000;
  mVideoConduit->ConfigureSendMediaCodec(&codecConfig);
  mVideoConduit->OnSinkWantsChanged(wants);
  ASSERT_EQ(mAdapter->mMaxPixelCount, 256000);

  wants.max_pixel_count = rtc::Optional<int>(64000);
  codecConfig.mEncodingConstraints.maxFs = 500;
  mVideoConduit->ConfigureSendMediaCodec(&codecConfig);
  mVideoConduit->OnSinkWantsChanged(wants);
  ASSERT_EQ(mAdapter->mMaxPixelCount, 64000);
}

} // End namespace test.
