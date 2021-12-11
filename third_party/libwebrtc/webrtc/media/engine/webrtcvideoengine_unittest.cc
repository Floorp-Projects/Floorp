/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "api/test/mock_video_decoder_factory.h"
#include "api/test/mock_video_encoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "call/flexfec_receive_stream.h"
#include "common_video/h264/profile_level_id.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "media/base/mediaconstants.h"
#include "media/base/rtputils.h"
#include "media/base/testutils.h"
#include "media/base/videoengine_unittest.h"
#include "media/engine/constants.h"
#include "media/engine/fakewebrtccall.h"
#include "media/engine/fakewebrtcvideoengine.h"
#include "media/engine/internalencoderfactory.h"
#include "media/engine/simulcast.h"
#include "media/engine/webrtcvideoengine.h"
#include "media/engine/webrtcvoiceengine.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/gunit.h"
#include "rtc_base/stringutils.h"
#include "test/field_trial.h"
#include "test/gmock.h"

using webrtc::RtpExtension;

namespace {
static const int kDefaultQpMax = 56;

static const uint8_t kRedRtxPayloadType = 125;

static const uint32_t kSsrcs1[] = {1};
static const uint32_t kSsrcs3[] = {1, 2, 3};
static const uint32_t kRtxSsrcs1[] = {4};
static const uint32_t kFlexfecSsrc = 5;
static const uint32_t kIncomingUnsignalledSsrc = 0xC0FFEE;
static const uint32_t kDefaultRecvSsrc = 0;

static const char kUnsupportedExtensionName[] =
    "urn:ietf:params:rtp-hdrext:unsupported";

cricket::VideoCodec RemoveFeedbackParams(cricket::VideoCodec&& codec) {
  codec.feedback_params = cricket::FeedbackParams();
  return codec;
}

void VerifyCodecHasDefaultFeedbackParams(const cricket::VideoCodec& codec) {
  EXPECT_TRUE(codec.HasFeedbackParam(cricket::FeedbackParam(
      cricket::kRtcpFbParamNack, cricket::kParamValueEmpty)));
  EXPECT_TRUE(codec.HasFeedbackParam(cricket::FeedbackParam(
      cricket::kRtcpFbParamNack, cricket::kRtcpFbNackParamPli)));
  EXPECT_TRUE(codec.HasFeedbackParam(cricket::FeedbackParam(
      cricket::kRtcpFbParamRemb, cricket::kParamValueEmpty)));
  EXPECT_TRUE(codec.HasFeedbackParam(cricket::FeedbackParam(
      cricket::kRtcpFbParamTransportCc, cricket::kParamValueEmpty)));
  EXPECT_TRUE(codec.HasFeedbackParam(cricket::FeedbackParam(
      cricket::kRtcpFbParamCcm, cricket::kRtcpFbCcmParamFir)));
}

// Return true if any codec in |codecs| is an RTX codec with associated payload
// type |payload_type|.
bool HasRtxCodec(const std::vector<cricket::VideoCodec>& codecs,
                 int payload_type) {
  for (const cricket::VideoCodec& codec : codecs) {
    int associated_payload_type;
    if (cricket::CodecNamesEq(codec.name.c_str(), "rtx") &&
        codec.GetParam(cricket::kCodecParamAssociatedPayloadType,
                       &associated_payload_type) &&
        associated_payload_type == payload_type) {
      return true;
    }
  }
  return false;
}

// TODO(nisse): Duplicated in call.cc.
const int* FindKeyByValue(const std::map<int, int>& m, int v) {
  for (const auto& kv : m) {
    if (kv.second == v)
      return &kv.first;
  }
  return nullptr;
}

bool HasRtxReceiveAssociation(
    const webrtc::VideoReceiveStream::Config& config,
    int payload_type) {
  return FindKeyByValue(config.rtp.rtx_associated_payload_types,
                        payload_type) != nullptr;
}

// Check that there's an Rtx payload type for each decoder.
bool VerifyRtxReceiveAssociations(
    const webrtc::VideoReceiveStream::Config& config) {
  for (const auto& decoder : config.decoders) {
    if (!HasRtxReceiveAssociation(config, decoder.payload_type))
      return false;
  }
  return true;
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer> CreateBlackFrameBuffer(
    int width,
    int height) {
  rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(width, height);
  webrtc::I420Buffer::SetBlack(buffer);
  return buffer;
}

void VerifySendStreamHasRtxTypes(const webrtc::VideoSendStream::Config& config,
                                 const std::map<int, int>& rtx_types) {
  std::map<int, int>::const_iterator it;
  it = rtx_types.find(config.encoder_settings.payload_type);
  EXPECT_TRUE(it != rtx_types.end() &&
              it->second == config.rtp.rtx.payload_type);

  if (config.rtp.ulpfec.red_rtx_payload_type != -1) {
    it = rtx_types.find(config.rtp.ulpfec.red_payload_type);
    EXPECT_TRUE(it != rtx_types.end() &&
                it->second == config.rtp.ulpfec.red_rtx_payload_type);
  }
}

cricket::MediaConfig GetMediaConfig() {
  cricket::MediaConfig media_config;
  media_config.video.enable_cpu_overuse_detection = false;
  return media_config;
}

}  // namespace

namespace cricket {
class WebRtcVideoEngineTest : public ::testing::Test {
 public:
  WebRtcVideoEngineTest() : WebRtcVideoEngineTest("") {}
  explicit WebRtcVideoEngineTest(const char* field_trials)
      : override_field_trials_(field_trials),
        call_(webrtc::Call::Create(webrtc::Call::Config(&event_log_))),
        encoder_factory_(new cricket::FakeWebRtcVideoEncoderFactory),
        decoder_factory_(new cricket::FakeWebRtcVideoDecoderFactory),
        engine_(std::unique_ptr<cricket::WebRtcVideoEncoderFactory>(
                    encoder_factory_),
                std::unique_ptr<cricket::WebRtcVideoDecoderFactory>(
                    decoder_factory_)) {
    std::vector<VideoCodec> engine_codecs = engine_.codecs();
    RTC_DCHECK(!engine_codecs.empty());
    bool codec_set = false;
    for (const cricket::VideoCodec& codec : engine_codecs) {
      if (codec.name == "rtx") {
        int associated_payload_type;
        if (codec.GetParam(kCodecParamAssociatedPayloadType,
                           &associated_payload_type)) {
          default_apt_rtx_types_[associated_payload_type] = codec.id;
        }
      } else if (!codec_set && codec.name != "red" && codec.name != "ulpfec") {
        default_codec_ = codec;
        codec_set = true;
      }
    }

    RTC_DCHECK(codec_set);
  }

 protected:
  // Find the index of the codec in the engine with the given name. The codec
  // must be present.
  int GetEngineCodecIndex(const std::string& name) const;

  // Find the codec in the engine with the given name. The codec must be
  // present.
  cricket::VideoCodec GetEngineCodec(const std::string& name) const;

  VideoMediaChannel* SetUpForExternalEncoderFactory();

  VideoMediaChannel* SetUpForExternalDecoderFactory(
      const std::vector<VideoCodec>& codecs);

  void TestExtendedEncoderOveruse(bool use_external_encoder);

  webrtc::test::ScopedFieldTrials override_field_trials_;
  webrtc::RtcEventLogNullImpl event_log_;
  // Used in WebRtcVideoEngineVoiceTest, but defined here so it's properly
  // initialized when the constructor is called.
  std::unique_ptr<webrtc::Call> call_;
  // TODO(magjed): Update all tests to use new video codec factories once the
  // old factories are deprecated,
  // https://bugs.chromium.org/p/webrtc/issues/detail?id=7925.
  // These factories are owned by the video engine.
  cricket::FakeWebRtcVideoEncoderFactory* encoder_factory_;
  cricket::FakeWebRtcVideoDecoderFactory* decoder_factory_;
  WebRtcVideoEngine engine_;
  VideoCodec default_codec_;
  std::map<int, int> default_apt_rtx_types_;
};

TEST_F(WebRtcVideoEngineTest, AnnouncesVp9AccordingToBuildFlags) {
  bool claims_vp9_support = false;
  for (const cricket::VideoCodec& codec : engine_.codecs()) {
    if (codec.name == "VP9") {
      claims_vp9_support = true;
      break;
    }
  }
#if defined(RTC_DISABLE_VP9)
  EXPECT_FALSE(claims_vp9_support);
#else
  EXPECT_TRUE(claims_vp9_support);
#endif  // defined(RTC_DISABLE_VP9)
}

TEST_F(WebRtcVideoEngineTest, DefaultRtxCodecHasAssociatedPayloadTypeSet) {
  std::vector<VideoCodec> engine_codecs = engine_.codecs();
  for (size_t i = 0; i < engine_codecs.size(); ++i) {
    if (engine_codecs[i].name != kRtxCodecName)
      continue;
    int associated_payload_type;
    EXPECT_TRUE(engine_codecs[i].GetParam(kCodecParamAssociatedPayloadType,
                                          &associated_payload_type));
    EXPECT_EQ(default_codec_.id, associated_payload_type);
    return;
  }
  FAIL() << "No RTX codec found among default codecs.";
}

TEST_F(WebRtcVideoEngineTest, SupportsTimestampOffsetHeaderExtension) {
  RtpCapabilities capabilities = engine_.GetCapabilities();
  ASSERT_FALSE(capabilities.header_extensions.empty());
  for (const RtpExtension& extension : capabilities.header_extensions) {
    if (extension.uri == RtpExtension::kTimestampOffsetUri) {
      EXPECT_EQ(RtpExtension::kTimestampOffsetDefaultId, extension.id);
      return;
    }
  }
  FAIL() << "Timestamp offset extension not in header-extension list.";
}

TEST_F(WebRtcVideoEngineTest, SupportsAbsoluteSenderTimeHeaderExtension) {
  RtpCapabilities capabilities = engine_.GetCapabilities();
  ASSERT_FALSE(capabilities.header_extensions.empty());
  for (const RtpExtension& extension : capabilities.header_extensions) {
    if (extension.uri == RtpExtension::kAbsSendTimeUri) {
      EXPECT_EQ(RtpExtension::kAbsSendTimeDefaultId, extension.id);
      return;
    }
  }
  FAIL() << "Absolute Sender Time extension not in header-extension list.";
}

TEST_F(WebRtcVideoEngineTest, SupportsTransportSequenceNumberHeaderExtension) {
  RtpCapabilities capabilities = engine_.GetCapabilities();
  ASSERT_FALSE(capabilities.header_extensions.empty());
  for (const RtpExtension& extension : capabilities.header_extensions) {
    if (extension.uri == RtpExtension::kTransportSequenceNumberUri) {
      EXPECT_EQ(RtpExtension::kTransportSequenceNumberDefaultId, extension.id);
      return;
    }
  }
  FAIL() << "Transport sequence number extension not in header-extension list.";
}

TEST_F(WebRtcVideoEngineTest, SupportsVideoRotationHeaderExtension) {
  RtpCapabilities capabilities = engine_.GetCapabilities();
  ASSERT_FALSE(capabilities.header_extensions.empty());
  for (const RtpExtension& extension : capabilities.header_extensions) {
    if (extension.uri == RtpExtension::kVideoRotationUri) {
      EXPECT_EQ(RtpExtension::kVideoRotationDefaultId, extension.id);
      return;
    }
  }
  FAIL() << "Video Rotation extension not in header-extension list.";
}

TEST_F(WebRtcVideoEngineTest, CVOSetHeaderExtensionBeforeCapturer) {
  // Allocate the capturer first to prevent early destruction before channel's
  // dtor is called.
  cricket::FakeVideoCapturer capturer;

  encoder_factory_->AddSupportedVideoCodecType("VP8");

  std::unique_ptr<VideoMediaChannel> channel(SetUpForExternalEncoderFactory());
  EXPECT_TRUE(channel->AddSendStream(StreamParams::CreateLegacy(kSsrc)));

  // Add CVO extension.
  const int id = 1;
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.extensions.push_back(
      RtpExtension(RtpExtension::kVideoRotationUri, id));
  EXPECT_TRUE(channel->SetSendParameters(parameters));

  // Set capturer.
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, true, nullptr, &capturer));

  // Verify capturer has turned off applying rotation.
  EXPECT_FALSE(capturer.apply_rotation());

  // Verify removing header extension turns on applying rotation.
  parameters.extensions.clear();
  EXPECT_TRUE(channel->SetSendParameters(parameters));
  EXPECT_TRUE(capturer.apply_rotation());
}

TEST_F(WebRtcVideoEngineTest, CVOSetHeaderExtensionBeforeAddSendStream) {
  // Allocate the capturer first to prevent early destruction before channel's
  // dtor is called.
  cricket::FakeVideoCapturer capturer;

  encoder_factory_->AddSupportedVideoCodecType("VP8");

  std::unique_ptr<VideoMediaChannel> channel(SetUpForExternalEncoderFactory());
  // Add CVO extension.
  const int id = 1;
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.extensions.push_back(
      RtpExtension(RtpExtension::kVideoRotationUri, id));
  EXPECT_TRUE(channel->SetSendParameters(parameters));
  EXPECT_TRUE(channel->AddSendStream(StreamParams::CreateLegacy(kSsrc)));

  // Set capturer.
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, true, nullptr, &capturer));

  // Verify capturer has turned off applying rotation.
  EXPECT_FALSE(capturer.apply_rotation());
}

TEST_F(WebRtcVideoEngineTest, CVOSetHeaderExtensionAfterCapturer) {
  cricket::FakeVideoCapturer capturer;

  encoder_factory_->AddSupportedVideoCodecType("VP8");
  encoder_factory_->AddSupportedVideoCodecType("VP9");

  std::unique_ptr<VideoMediaChannel> channel(SetUpForExternalEncoderFactory());
  EXPECT_TRUE(channel->AddSendStream(StreamParams::CreateLegacy(kSsrc)));

  // Set capturer.
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, true, nullptr, &capturer));

  // Verify capturer has turned on applying rotation.
  EXPECT_TRUE(capturer.apply_rotation());

  // Add CVO extension.
  const int id = 1;
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  parameters.extensions.push_back(
      RtpExtension(RtpExtension::kVideoRotationUri, id));
  // Also remove the first codec to trigger a codec change as well.
  parameters.codecs.erase(parameters.codecs.begin());
  EXPECT_TRUE(channel->SetSendParameters(parameters));

  // Verify capturer has turned off applying rotation.
  EXPECT_FALSE(capturer.apply_rotation());

  // Verify removing header extension turns on applying rotation.
  parameters.extensions.clear();
  EXPECT_TRUE(channel->SetSendParameters(parameters));
  EXPECT_TRUE(capturer.apply_rotation());
}

TEST_F(WebRtcVideoEngineTest, SetSendFailsBeforeSettingCodecs) {
  std::unique_ptr<VideoMediaChannel> channel(
      engine_.CreateChannel(call_.get(), GetMediaConfig(), VideoOptions()));

  EXPECT_TRUE(channel->AddSendStream(StreamParams::CreateLegacy(123)));

  EXPECT_FALSE(channel->SetSend(true))
      << "Channel should not start without codecs.";
  EXPECT_TRUE(channel->SetSend(false))
      << "Channel should be stoppable even without set codecs.";
}

TEST_F(WebRtcVideoEngineTest, GetStatsWithoutSendCodecsSetDoesNotCrash) {
  std::unique_ptr<VideoMediaChannel> channel(
      engine_.CreateChannel(call_.get(), GetMediaConfig(), VideoOptions()));
  EXPECT_TRUE(channel->AddSendStream(StreamParams::CreateLegacy(123)));
  VideoMediaInfo info;
  channel->GetStats(&info);
}

TEST_F(WebRtcVideoEngineTest, UseExternalFactoryForVp8WhenSupported) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");

  std::unique_ptr<VideoMediaChannel> channel(SetUpForExternalEncoderFactory());
  channel->OnReadyToSend(true);

  EXPECT_TRUE(
      channel->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  EXPECT_EQ(0, encoder_factory_->GetNumCreatedEncoders());
  EXPECT_TRUE(channel->SetSend(true));
  cricket::FakeVideoCapturer capturer;
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, true, nullptr, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(capturer.CaptureFrame());
  // Sending one frame will have allocate the encoder.
  ASSERT_TRUE(encoder_factory_->WaitForCreatedVideoEncoders(1));
  EXPECT_TRUE_WAIT(encoder_factory_->encoders()[0]->GetNumEncodedFrames() > 0,
                   kTimeout);

  int num_created_encoders = encoder_factory_->GetNumCreatedEncoders();
  EXPECT_EQ(num_created_encoders, 1);

  // Setting codecs of the same type should not reallocate any encoders
  // (expecting a no-op).
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  EXPECT_TRUE(channel->SetSendParameters(parameters));
  EXPECT_EQ(num_created_encoders, encoder_factory_->GetNumCreatedEncoders());

  // Remove stream previously added to free the external encoder instance.
  EXPECT_TRUE(channel->RemoveSendStream(kSsrc));
  EXPECT_EQ(0u, encoder_factory_->encoders().size());
}

// Test that when an external encoder factory supports a codec we don't
// internally support, we still add an RTX codec for it.
// TODO(deadbeef): Currently this test is only effective if WebRTC is
// built with no internal H264 support. This test should be updated
// if/when we start adding RTX codecs for unrecognized codec names.
TEST_F(WebRtcVideoEngineTest, RtxCodecAddedForExternalCodec) {
  using webrtc::H264::ProfileLevelIdToString;
  using webrtc::H264::ProfileLevelId;
  using webrtc::H264::kLevel1;
  cricket::VideoCodec h264_constrained_baseline("H264");
  h264_constrained_baseline.params[kH264FmtpProfileLevelId] =
      *ProfileLevelIdToString(
          ProfileLevelId(webrtc::H264::kProfileConstrainedBaseline, kLevel1));
  cricket::VideoCodec h264_constrained_high("H264");
  h264_constrained_high.params[kH264FmtpProfileLevelId] =
      *ProfileLevelIdToString(
          ProfileLevelId(webrtc::H264::kProfileConstrainedHigh, kLevel1));
  cricket::VideoCodec h264_high("H264");
  h264_high.params[kH264FmtpProfileLevelId] = *ProfileLevelIdToString(
      ProfileLevelId(webrtc::H264::kProfileHigh, kLevel1));

  encoder_factory_->AddSupportedVideoCodec(h264_constrained_baseline);
  encoder_factory_->AddSupportedVideoCodec(h264_constrained_high);
  encoder_factory_->AddSupportedVideoCodec(h264_high);

  // First figure out what payload types the test codecs got assigned.
  const std::vector<cricket::VideoCodec> codecs = engine_.codecs();
  // Now search for RTX codecs for them. Expect that they all have associated
  // RTX codecs.
  EXPECT_TRUE(HasRtxCodec(
      codecs, FindMatchingCodec(codecs, h264_constrained_baseline)->id));
  EXPECT_TRUE(HasRtxCodec(
      codecs, FindMatchingCodec(codecs, h264_constrained_high)->id));
  EXPECT_TRUE(HasRtxCodec(
      codecs, FindMatchingCodec(codecs, h264_high)->id));
}

void WebRtcVideoEngineTest::TestExtendedEncoderOveruse(
    bool use_external_encoder) {
  std::unique_ptr<VideoMediaChannel> channel;
  FakeCall* fake_call = new FakeCall(webrtc::Call::Config(&event_log_));
  call_.reset(fake_call);
  if (use_external_encoder) {
    encoder_factory_->AddSupportedVideoCodecType("VP8");
    channel.reset(SetUpForExternalEncoderFactory());
  } else {
    channel.reset(
        engine_.CreateChannel(call_.get(), GetMediaConfig(), VideoOptions()));
  }
  ASSERT_TRUE(
      channel->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  EXPECT_TRUE(channel->SetSendParameters(parameters));
  EXPECT_TRUE(channel->SetSend(true));
  FakeVideoSendStream* stream = fake_call->GetVideoSendStreams()[0];

  EXPECT_EQ(use_external_encoder,
            stream->GetConfig().encoder_settings.full_overuse_time);
  // Remove stream previously added to free the external encoder instance.
  EXPECT_TRUE(channel->RemoveSendStream(kSsrc));
}

TEST_F(WebRtcVideoEngineTest, EnablesFullEncoderTimeForExternalEncoders) {
  TestExtendedEncoderOveruse(true);
}

TEST_F(WebRtcVideoEngineTest, DisablesFullEncoderTimeForNonExternalEncoders) {
  TestExtendedEncoderOveruse(false);
}

#if !defined(RTC_DISABLE_VP9)
TEST_F(WebRtcVideoEngineTest, CanConstructDecoderForVp9EncoderFactory) {
  encoder_factory_->AddSupportedVideoCodecType("VP9");

  std::unique_ptr<VideoMediaChannel> channel(SetUpForExternalEncoderFactory());

  EXPECT_TRUE(
      channel->AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrc)));
}
#endif  // !defined(RTC_DISABLE_VP9)

TEST_F(WebRtcVideoEngineTest, PropagatesInputFrameTimestamp) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");
  FakeCall* fake_call = new FakeCall(webrtc::Call::Config(&event_log_));
  call_.reset(fake_call);
  std::unique_ptr<VideoMediaChannel> channel(SetUpForExternalEncoderFactory());

  EXPECT_TRUE(
      channel->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));

  FakeVideoCapturer capturer;
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, true, nullptr, &capturer));
  capturer.Start(cricket::VideoFormat(1280, 720,
                                      cricket::VideoFormat::FpsToInterval(60),
                                      cricket::FOURCC_I420));
  channel->SetSend(true);

  FakeVideoSendStream* stream = fake_call->GetVideoSendStreams()[0];

  EXPECT_TRUE(capturer.CaptureFrame());
  int64_t last_timestamp = stream->GetLastTimestamp();
  for (int i = 0; i < 10; i++) {
    EXPECT_TRUE(capturer.CaptureFrame());
    int64_t timestamp = stream->GetLastTimestamp();
    int64_t interval = timestamp - last_timestamp;

    // Precision changes from nanosecond to millisecond.
    // Allow error to be no more than 1.
    EXPECT_NEAR(cricket::VideoFormat::FpsToInterval(60) / 1E6, interval, 1);

    last_timestamp = timestamp;
  }

  capturer.Start(cricket::VideoFormat(1280, 720,
                                      cricket::VideoFormat::FpsToInterval(30),
                                      cricket::FOURCC_I420));

  EXPECT_TRUE(capturer.CaptureFrame());
  last_timestamp = stream->GetLastTimestamp();
  for (int i = 0; i < 10; i++) {
    EXPECT_TRUE(capturer.CaptureFrame());
    int64_t timestamp = stream->GetLastTimestamp();
    int64_t interval = timestamp - last_timestamp;

    // Precision changes from nanosecond to millisecond.
    // Allow error to be no more than 1.
    EXPECT_NEAR(cricket::VideoFormat::FpsToInterval(30) / 1E6, interval, 1);

    last_timestamp = timestamp;
  }

  // Remove stream previously added to free the external encoder instance.
  EXPECT_TRUE(channel->RemoveSendStream(kSsrc));
}

int WebRtcVideoEngineTest::GetEngineCodecIndex(const std::string& name) const {
  const std::vector<cricket::VideoCodec> codecs = engine_.codecs();
  for (size_t i = 0; i < codecs.size(); ++i) {
    const cricket::VideoCodec engine_codec = codecs[i];
    if (!CodecNamesEq(name, engine_codec.name))
      continue;
    // The tests only use H264 Constrained Baseline. Make sure we don't return
    // an internal H264 codec from the engine with a different H264 profile.
    if (CodecNamesEq(name.c_str(), kH264CodecName)) {
      const rtc::Optional<webrtc::H264::ProfileLevelId> profile_level_id =
          webrtc::H264::ParseSdpProfileLevelId(engine_codec.params);
      if (profile_level_id->profile !=
          webrtc::H264::kProfileConstrainedBaseline) {
        continue;
      }
    }
    return i;
  }
  // This point should never be reached.
  ADD_FAILURE() << "Unrecognized codec name: " << name;
  return -1;
}

cricket::VideoCodec WebRtcVideoEngineTest::GetEngineCodec(
    const std::string& name) const {
  return engine_.codecs()[GetEngineCodecIndex(name)];
}

VideoMediaChannel* WebRtcVideoEngineTest::SetUpForExternalEncoderFactory() {
  VideoMediaChannel* channel =
      engine_.CreateChannel(call_.get(), GetMediaConfig(), VideoOptions());
  cricket::VideoSendParameters parameters;
  // We need to look up the codec in the engine to get the correct payload type.
  for (const VideoCodec& codec : encoder_factory_->supported_codecs())
    parameters.codecs.push_back(GetEngineCodec(codec.name));

  EXPECT_TRUE(channel->SetSendParameters(parameters));

  return channel;
}

VideoMediaChannel* WebRtcVideoEngineTest::SetUpForExternalDecoderFactory(
    const std::vector<VideoCodec>& codecs) {
  VideoMediaChannel* channel =
      engine_.CreateChannel(call_.get(), GetMediaConfig(), VideoOptions());
  cricket::VideoRecvParameters parameters;
  parameters.codecs = codecs;
  EXPECT_TRUE(channel->SetRecvParameters(parameters));

  return channel;
}

TEST_F(WebRtcVideoEngineTest, UsesSimulcastAdapterForVp8Factories) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");

  std::unique_ptr<VideoMediaChannel> channel(SetUpForExternalEncoderFactory());

  std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs3);

  EXPECT_TRUE(
      channel->AddSendStream(CreateSimStreamParams("cname", ssrcs)));
  EXPECT_TRUE(channel->SetSend(true));

  cricket::FakeVideoCapturer capturer;
  EXPECT_TRUE(channel->SetVideoSend(ssrcs.front(), true, nullptr, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(capturer.CaptureFrame());

  ASSERT_TRUE(encoder_factory_->WaitForCreatedVideoEncoders(2));

  // Verify that encoders are configured for simulcast through adapter
  // (increasing resolution and only configured to send one stream each).
  int prev_width = -1;
  for (size_t i = 0; i < encoder_factory_->encoders().size(); ++i) {
    ASSERT_TRUE(encoder_factory_->encoders()[i]->WaitForInitEncode());
    webrtc::VideoCodec codec_settings =
        encoder_factory_->encoders()[i]->GetCodecSettings();
    EXPECT_EQ(0, codec_settings.numberOfSimulcastStreams);
    EXPECT_GT(codec_settings.width, prev_width);
    prev_width = codec_settings.width;
  }

  EXPECT_TRUE(channel->SetVideoSend(ssrcs.front(), true, nullptr, nullptr));

  channel.reset();
  ASSERT_EQ(0u, encoder_factory_->encoders().size());
}

TEST_F(WebRtcVideoEngineTest, ChannelWithExternalH264CanChangeToInternalVp8) {
  encoder_factory_->AddSupportedVideoCodecType("H264");

  std::unique_ptr<VideoMediaChannel> channel(SetUpForExternalEncoderFactory());

  EXPECT_TRUE(
      channel->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  ASSERT_EQ(1u, encoder_factory_->encoders().size());

  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  EXPECT_TRUE(channel->SetSendParameters(parameters));
  ASSERT_EQ(0u, encoder_factory_->encoders().size());
}

TEST_F(WebRtcVideoEngineTest,
       DontUseExternalEncoderFactoryForUnsupportedCodecs) {
  encoder_factory_->AddSupportedVideoCodecType("H264");

  std::unique_ptr<VideoMediaChannel> channel(
      engine_.CreateChannel(call_.get(), GetMediaConfig(), VideoOptions()));
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  EXPECT_TRUE(channel->SetSendParameters(parameters));

  EXPECT_TRUE(
      channel->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  // Make sure DestroyVideoEncoder was called on the factory.
  ASSERT_EQ(0u, encoder_factory_->encoders().size());
}

TEST_F(WebRtcVideoEngineTest,
       UsesSimulcastAdapterForVp8WithCombinedVP8AndH264Factory) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");
  encoder_factory_->AddSupportedVideoCodecType("H264");

  std::unique_ptr<VideoMediaChannel> channel(
      engine_.CreateChannel(call_.get(), GetMediaConfig(), VideoOptions()));
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  EXPECT_TRUE(channel->SetSendParameters(parameters));

  std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs3);

  EXPECT_TRUE(
      channel->AddSendStream(CreateSimStreamParams("cname", ssrcs)));
  EXPECT_TRUE(channel->SetSend(true));

  // Send a fake frame, or else the media engine will configure the simulcast
  // encoder adapter at a low-enough size that it'll only create a single
  // encoder layer.
  cricket::FakeVideoCapturer capturer;
  EXPECT_TRUE(channel->SetVideoSend(ssrcs.front(), true, nullptr, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(capturer.CaptureFrame());

  ASSERT_TRUE(encoder_factory_->WaitForCreatedVideoEncoders(2));
  ASSERT_TRUE(encoder_factory_->encoders()[0]->WaitForInitEncode());
  EXPECT_EQ(webrtc::kVideoCodecVP8,
            encoder_factory_->encoders()[0]->GetCodecSettings().codecType);

  channel.reset();
  // Make sure DestroyVideoEncoder was called on the factory.
  EXPECT_EQ(0u, encoder_factory_->encoders().size());
}

TEST_F(WebRtcVideoEngineTest,
       DestroysNonSimulcastEncoderFromCombinedVP8AndH264Factory) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");
  encoder_factory_->AddSupportedVideoCodecType("H264");

  std::unique_ptr<VideoMediaChannel> channel(
      engine_.CreateChannel(call_.get(), GetMediaConfig(), VideoOptions()));
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("H264"));
  EXPECT_TRUE(channel->SetSendParameters(parameters));

  EXPECT_TRUE(
      channel->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  ASSERT_EQ(1u, encoder_factory_->encoders().size());

  // Send a frame of 720p. This should trigger a "real" encoder initialization.
  cricket::VideoFormat format(
      1280, 720, cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420);
  cricket::FakeVideoCapturer capturer;
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, true, nullptr, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(format));
  EXPECT_TRUE(capturer.CaptureFrame());
  ASSERT_TRUE(encoder_factory_->encoders()[0]->WaitForInitEncode());
  EXPECT_EQ(webrtc::kVideoCodecH264,
            encoder_factory_->encoders()[0]->GetCodecSettings().codecType);

  channel.reset();
  // Make sure DestroyVideoEncoder was called on the factory.
  ASSERT_EQ(0u, encoder_factory_->encoders().size());
}

TEST_F(WebRtcVideoEngineTest, SimulcastDisabledForH264) {
  encoder_factory_->AddSupportedVideoCodecType("H264");

  std::unique_ptr<VideoMediaChannel> channel(SetUpForExternalEncoderFactory());

  const std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs3);
  EXPECT_TRUE(
      channel->AddSendStream(cricket::CreateSimStreamParams("cname", ssrcs)));

  // Send a frame of 720p. This should trigger a "real" encoder initialization.
  cricket::VideoFormat format(
      1280, 720, cricket::VideoFormat::FpsToInterval(30), cricket::FOURCC_I420);
  cricket::FakeVideoCapturer capturer;
  EXPECT_TRUE(channel->SetVideoSend(ssrcs[0], true, nullptr, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(format));
  EXPECT_TRUE(capturer.CaptureFrame());

  ASSERT_EQ(1u, encoder_factory_->encoders().size());
  FakeWebRtcVideoEncoder* encoder = encoder_factory_->encoders()[0];
  ASSERT_TRUE(encoder_factory_->encoders()[0]->WaitForInitEncode());
  EXPECT_EQ(webrtc::kVideoCodecH264, encoder->GetCodecSettings().codecType);
  EXPECT_EQ(1u, encoder->GetCodecSettings().numberOfSimulcastStreams);
  EXPECT_TRUE(channel->SetVideoSend(ssrcs[0], true, nullptr, nullptr));
}

// Test that the FlexFEC field trial properly alters the output of
// WebRtcVideoEngine::codecs(), for an existing |engine_| object.
//
// TODO(brandtr): Remove this test, when the FlexFEC field trial is gone.
TEST_F(WebRtcVideoEngineTest,
       Flexfec03SupportedAsInternalCodecBehindFieldTrial) {
  auto is_flexfec = [](const VideoCodec& codec) {
    if (codec.name == "flexfec-03")
      return true;
    return false;
  };

  // FlexFEC is not active without field trial.
  const std::vector<VideoCodec> codecs_before = engine_.codecs();
  EXPECT_EQ(codecs_before.end(), std::find_if(codecs_before.begin(),
                                              codecs_before.end(), is_flexfec));

  // FlexFEC is active with field trial.
  webrtc::test::ScopedFieldTrials override_field_trials_(
      "WebRTC-FlexFEC-03-Advertised/Enabled/");
  const std::vector<VideoCodec> codecs_after = engine_.codecs();
  EXPECT_NE(codecs_after.end(),
            std::find_if(codecs_after.begin(), codecs_after.end(), is_flexfec));
}

// Test that external codecs are added after internal SW codecs.
TEST_F(WebRtcVideoEngineTest, ReportSupportedExternalCodecs) {
  const char* kFakeExternalCodecName = "FakeExternalCodec";
  encoder_factory_->AddSupportedVideoCodecType(kFakeExternalCodecName);

  // The external codec should appear after the internal codec in the vector.
  const int vp8_index = GetEngineCodecIndex("VP8");
  const int fake_external_codec_index =
      GetEngineCodecIndex(kFakeExternalCodecName);
  EXPECT_LT(vp8_index, fake_external_codec_index);
}

// Test that an external codec that was added after the engine was initialized
// does show up in the codec list after it was added.
TEST_F(WebRtcVideoEngineTest, ReportSupportedExternalCodecsWithAddedCodec) {
  const char* kFakeExternalCodecName1 = "FakeExternalCodec1";
  const char* kFakeExternalCodecName2 = "FakeExternalCodec2";

  // Set up external encoder factory with first codec, and initialize engine.
  encoder_factory_->AddSupportedVideoCodecType(kFakeExternalCodecName1);

  std::vector<cricket::VideoCodec> codecs_before(engine_.codecs());

  // Add second codec.
  encoder_factory_->AddSupportedVideoCodecType(kFakeExternalCodecName2);
  std::vector<cricket::VideoCodec> codecs_after(engine_.codecs());
  EXPECT_EQ(codecs_before.size() + 1, codecs_after.size());

  // Check that both fake codecs are present and that the second fake codec
  // appears after the first fake codec.
  const int fake_codec_index1 = GetEngineCodecIndex(kFakeExternalCodecName1);
  const int fake_codec_index2 = GetEngineCodecIndex(kFakeExternalCodecName2);
  EXPECT_LT(fake_codec_index1, fake_codec_index2);
}

TEST_F(WebRtcVideoEngineTest, RegisterExternalDecodersIfSupported) {
  decoder_factory_->AddSupportedVideoCodecType(webrtc::kVideoCodecVP8);
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));

  std::unique_ptr<VideoMediaChannel> channel(
      SetUpForExternalDecoderFactory(parameters.codecs));

  EXPECT_TRUE(
      channel->AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  ASSERT_EQ(1u, decoder_factory_->decoders().size());

  // Setting codecs of the same type should not reallocate the decoder.
  EXPECT_TRUE(channel->SetRecvParameters(parameters));
  EXPECT_EQ(1, decoder_factory_->GetNumCreatedDecoders());

  // Remove stream previously added to free the external decoder instance.
  EXPECT_TRUE(channel->RemoveRecvStream(kSsrc));
  EXPECT_EQ(0u, decoder_factory_->decoders().size());
}

// Verifies that we can set up decoders that are not internally supported.
TEST_F(WebRtcVideoEngineTest, RegisterExternalH264DecoderIfSupported) {
  // TODO(pbos): Do not assume that encoder/decoder support is symmetric. We
  // can't even query the WebRtcVideoDecoderFactory for supported codecs.
  // For now we add a FakeWebRtcVideoEncoderFactory to add H264 to supported
  // codecs.
  encoder_factory_->AddSupportedVideoCodecType("H264");
  decoder_factory_->AddSupportedVideoCodecType(webrtc::kVideoCodecH264);
  std::vector<cricket::VideoCodec> codecs;
  codecs.push_back(GetEngineCodec("H264"));

  std::unique_ptr<VideoMediaChannel> channel(
      SetUpForExternalDecoderFactory(codecs));

  EXPECT_TRUE(
      channel->AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  ASSERT_EQ(1u, decoder_factory_->decoders().size());
}

TEST(WebRtcVideoEngineNewVideoCodecFactoryTest, NullFactories) {
  std::unique_ptr<webrtc::VideoEncoderFactory> encoder_factory;
  std::unique_ptr<webrtc::VideoDecoderFactory> decoder_factory;
  WebRtcVideoEngine engine(std::move(encoder_factory),
                           std::move(decoder_factory));
  EXPECT_EQ(0u, engine.codecs().size());
}

TEST(WebRtcVideoEngineNewVideoCodecFactoryTest, EmptyFactories) {
  // |engine| take ownership of the factories.
  webrtc::MockVideoEncoderFactory* encoder_factory =
      new webrtc::MockVideoEncoderFactory();
  webrtc::MockVideoDecoderFactory* decoder_factory =
      new webrtc::MockVideoDecoderFactory();
  WebRtcVideoEngine engine(
      (std::unique_ptr<webrtc::VideoEncoderFactory>(encoder_factory)),
      (std::unique_ptr<webrtc::VideoDecoderFactory>(decoder_factory)));
  EXPECT_CALL(*encoder_factory, GetSupportedFormats());
  EXPECT_EQ(0u, engine.codecs().size());
  EXPECT_CALL(*encoder_factory, Die());
  EXPECT_CALL(*decoder_factory, Die());
}

// Test full behavior in the video engine when video codec factories of the new
// type are injected supporting the single codec Vp8. Check the returned codecs
// from the engine and that we will create a Vp8 encoder and decoder using the
// new factories.
TEST(WebRtcVideoEngineNewVideoCodecFactoryTest, Vp8) {
  // |engine| take ownership of the factories.
  webrtc::MockVideoEncoderFactory* encoder_factory =
      new webrtc::MockVideoEncoderFactory();
  webrtc::MockVideoDecoderFactory* decoder_factory =
      new webrtc::MockVideoDecoderFactory();
  WebRtcVideoEngine engine(
      (std::unique_ptr<webrtc::VideoEncoderFactory>(encoder_factory)),
      (std::unique_ptr<webrtc::VideoDecoderFactory>(decoder_factory)));
  const webrtc::SdpVideoFormat vp8_format("VP8");
  const std::vector<webrtc::SdpVideoFormat> supported_formats = {vp8_format};
  EXPECT_CALL(*encoder_factory, GetSupportedFormats())
      .WillRepeatedly(testing::Return(supported_formats));

  // Verify the codecs from the engine.
  const std::vector<VideoCodec> engine_codecs = engine.codecs();
  // Verify default codecs has been added correctly.
  EXPECT_EQ(5u, engine_codecs.size());
  EXPECT_EQ("VP8", engine_codecs.at(0).name);

  // RTX codec for VP8.
  EXPECT_EQ("rtx", engine_codecs.at(1).name);
  int vp8_associated_payload;
  EXPECT_TRUE(engine_codecs.at(1).GetParam(kCodecParamAssociatedPayloadType,
                                           &vp8_associated_payload));
  EXPECT_EQ(vp8_associated_payload, engine_codecs.at(0).id);

  EXPECT_EQ(kRedCodecName, engine_codecs.at(2).name);

  // RTX codec for RED.
  EXPECT_EQ("rtx", engine_codecs.at(3).name);
  int red_associated_payload;
  EXPECT_TRUE(engine_codecs.at(3).GetParam(kCodecParamAssociatedPayloadType,
                                           &red_associated_payload));
  EXPECT_EQ(red_associated_payload, engine_codecs.at(2).id);

  EXPECT_EQ(kUlpfecCodecName, engine_codecs.at(4).name);

  int associated_payload_type;
  EXPECT_TRUE(engine_codecs.at(1).GetParam(
      cricket::kCodecParamAssociatedPayloadType, &associated_payload_type));
  EXPECT_EQ(engine_codecs.at(0).id, associated_payload_type);
  // Verify default parameters has been added to the VP8 codec.
  VerifyCodecHasDefaultFeedbackParams(engine_codecs.at(0));

  // Mock encoder creation. |engine| take ownership of the encoder.
  webrtc::VideoEncoderFactory::CodecInfo codec_info;
  codec_info.is_hardware_accelerated = false;
  codec_info.has_internal_source = false;
  const webrtc::SdpVideoFormat format("VP8");
  EXPECT_CALL(*encoder_factory, QueryVideoEncoder(format))
      .WillRepeatedly(testing::Return(codec_info));
  FakeWebRtcVideoEncoder* const encoder = new FakeWebRtcVideoEncoder();
  EXPECT_CALL(*encoder_factory, CreateVideoEncoderProxy(format))
      .WillOnce(testing::Return(encoder));

  // Mock decoder creation. |engine| take ownership of the decoder.
  FakeWebRtcVideoDecoder* const decoder = new FakeWebRtcVideoDecoder();
  EXPECT_CALL(*decoder_factory, CreateVideoDecoderProxy(format))
      .WillOnce(testing::Return(decoder));

  // Create a call.
  webrtc::RtcEventLogNullImpl event_log;
  std::unique_ptr<webrtc::Call> call(
      webrtc::Call::Create(webrtc::Call::Config(&event_log)));

  // Create send channel.
  const int send_ssrc = 123;
  std::unique_ptr<VideoMediaChannel> send_channel(
      engine.CreateChannel(call.get(), GetMediaConfig(), VideoOptions()));
  cricket::VideoSendParameters send_parameters;
  send_parameters.codecs.push_back(engine_codecs.at(0));
  EXPECT_TRUE(send_channel->SetSendParameters(send_parameters));
  send_channel->OnReadyToSend(true);
  EXPECT_TRUE(
      send_channel->AddSendStream(StreamParams::CreateLegacy(send_ssrc)));
  EXPECT_TRUE(send_channel->SetSend(true));

  // Create recv channel.
  const int recv_ssrc = 321;
  std::unique_ptr<VideoMediaChannel> recv_channel(
      engine.CreateChannel(call.get(), GetMediaConfig(), VideoOptions()));
  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(engine_codecs.at(0));
  EXPECT_TRUE(recv_channel->SetRecvParameters(recv_parameters));
  EXPECT_TRUE(recv_channel->AddRecvStream(
      cricket::StreamParams::CreateLegacy(recv_ssrc)));

  // Remove streams previously added to free the encoder and decoder instance.
  EXPECT_CALL(*encoder_factory, Die());
  EXPECT_CALL(*decoder_factory, Die());
  EXPECT_TRUE(send_channel->RemoveSendStream(send_ssrc));
  EXPECT_TRUE(recv_channel->RemoveRecvStream(recv_ssrc));
}

// Test behavior when decoder factory fails to create a decoder (returns null).
TEST(WebRtcVideoEngineNewVideoCodecFactoryTest, NullDecoder) {
  // |engine| take ownership of the factories.
  webrtc::MockVideoEncoderFactory* encoder_factory =
      new testing::StrictMock<webrtc::MockVideoEncoderFactory>();
  webrtc::MockVideoDecoderFactory* decoder_factory =
      new testing::StrictMock<webrtc::MockVideoDecoderFactory>();
  WebRtcVideoEngine engine(
      (std::unique_ptr<webrtc::VideoEncoderFactory>(encoder_factory)),
      (std::unique_ptr<webrtc::VideoDecoderFactory>(decoder_factory)));
  const webrtc::SdpVideoFormat vp8_format("VP8");
  const std::vector<webrtc::SdpVideoFormat> supported_formats = {vp8_format};
  EXPECT_CALL(*encoder_factory, GetSupportedFormats())
      .WillRepeatedly(testing::Return(supported_formats));

  // Decoder creation fails.
  EXPECT_CALL(*decoder_factory, CreateVideoDecoderProxy(testing::_))
      .WillOnce(testing::Return(nullptr));

  // Create a call.
  webrtc::RtcEventLogNullImpl event_log;
  std::unique_ptr<webrtc::Call> call(
      webrtc::Call::Create(webrtc::Call::Config(&event_log)));

  // Create recv channel.
  const int recv_ssrc = 321;
  std::unique_ptr<VideoMediaChannel> recv_channel(
      engine.CreateChannel(call.get(), GetMediaConfig(), VideoOptions()));
  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(engine.codecs().front());
  EXPECT_TRUE(recv_channel->SetRecvParameters(recv_parameters));
  EXPECT_TRUE(recv_channel->AddRecvStream(
      cricket::StreamParams::CreateLegacy(recv_ssrc)));

  // Remove streams previously added to free the encoder and decoder instance.
  EXPECT_TRUE(recv_channel->RemoveRecvStream(recv_ssrc));
}

class WebRtcVideoChannelBaseTest
    : public VideoMediaChannelTest<WebRtcVideoEngine, WebRtcVideoChannel> {
 protected:
  typedef VideoMediaChannelTest<WebRtcVideoEngine, WebRtcVideoChannel> Base;

  cricket::VideoCodec GetEngineCodec(const std::string& name) {
    for (const cricket::VideoCodec& engine_codec : engine_.codecs()) {
      if (CodecNamesEq(name, engine_codec.name))
        return engine_codec;
    }
    // This point should never be reached.
    ADD_FAILURE() << "Unrecognized codec name: " << name;
    return cricket::VideoCodec();
  }

  cricket::VideoCodec DefaultCodec() override { return GetEngineCodec("VP8"); }
};

// Verifies that id given in stream params is passed to the decoder factory.
TEST_F(WebRtcVideoEngineTest, StreamParamsIdPassedToDecoderFactory) {
  decoder_factory_->AddSupportedVideoCodecType(webrtc::kVideoCodecVP8);
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));

  std::unique_ptr<VideoMediaChannel> channel(
      SetUpForExternalDecoderFactory(parameters.codecs));

  StreamParams sp = cricket::StreamParams::CreateLegacy(kSsrc);
  sp.id = "FakeStreamParamsId";
  EXPECT_TRUE(channel->AddRecvStream(sp));
  EXPECT_EQ(1u, decoder_factory_->decoders().size());

  std::vector<cricket::VideoDecoderParams> params = decoder_factory_->params();
  ASSERT_EQ(1u, params.size());
  EXPECT_EQ(sp.id, params[0].receive_stream_id);
}

TEST_F(WebRtcVideoEngineTest, DISABLED_RecreatesEncoderOnContentTypeChange) {
  encoder_factory_->AddSupportedVideoCodecType("VP8");
  std::unique_ptr<FakeCall> fake_call(
      new FakeCall(webrtc::Call::Config(&event_log_)));
  std::unique_ptr<VideoMediaChannel> channel(SetUpForExternalEncoderFactory());
  ASSERT_TRUE(
      channel->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc)));
  cricket::VideoCodec codec = GetEngineCodec("VP8");
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(codec);
  channel->OnReadyToSend(true);
  channel->SetSend(true);
  ASSERT_TRUE(channel->SetSendParameters(parameters));

  cricket::FakeVideoCapturer capturer;
  VideoOptions options;
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, true, &options, &capturer));

  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(capturer.CaptureFrame());
  ASSERT_TRUE(encoder_factory_->WaitForCreatedVideoEncoders(1));
  EXPECT_EQ(webrtc::kRealtimeVideo,
            encoder_factory_->encoders().back()->GetCodecSettings().mode);

  EXPECT_TRUE(channel->SetVideoSend(kSsrc, true, &options, &capturer));
  EXPECT_TRUE(capturer.CaptureFrame());
  // No change in content type, keep current encoder.
  EXPECT_EQ(1, encoder_factory_->GetNumCreatedEncoders());

  options.is_screencast.emplace(true);
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, true, &options, &capturer));
  EXPECT_TRUE(capturer.CaptureFrame());
  // Change to screen content, recreate encoder. For the simulcast encoder
  // adapter case, this will result in two calls since InitEncode triggers a
  // a new instance.
  ASSERT_TRUE(encoder_factory_->WaitForCreatedVideoEncoders(2));
  EXPECT_EQ(webrtc::kScreensharing,
            encoder_factory_->encoders().back()->GetCodecSettings().mode);

  EXPECT_TRUE(channel->SetVideoSend(kSsrc, true, &options, &capturer));
  EXPECT_TRUE(capturer.CaptureFrame());
  // Still screen content, no need to update encoder.
  EXPECT_EQ(2, encoder_factory_->GetNumCreatedEncoders());

  options.is_screencast.emplace(false);
  options.video_noise_reduction.emplace(false);
  EXPECT_TRUE(channel->SetVideoSend(kSsrc, true, &options, &capturer));
  // Change back to regular video content, update encoder. Also change
  // a non |is_screencast| option just to verify it doesn't affect recreation.
  EXPECT_TRUE(capturer.CaptureFrame());
  ASSERT_TRUE(encoder_factory_->WaitForCreatedVideoEncoders(3));
  EXPECT_EQ(webrtc::kRealtimeVideo,
            encoder_factory_->encoders().back()->GetCodecSettings().mode);

  // Remove stream previously added to free the external encoder instance.
  EXPECT_TRUE(channel->RemoveSendStream(kSsrc));
  EXPECT_EQ(0u, encoder_factory_->encoders().size());
}

#define WEBRTC_BASE_TEST(test) \
  TEST_F(WebRtcVideoChannelBaseTest, test) { Base::test(); }

#define WEBRTC_DISABLED_BASE_TEST(test) \
  TEST_F(WebRtcVideoChannelBaseTest, DISABLED_##test) { Base::test(); }

WEBRTC_BASE_TEST(SetSend);
WEBRTC_BASE_TEST(SetSendWithoutCodecs);
WEBRTC_BASE_TEST(SetSendSetsTransportBufferSizes);

WEBRTC_BASE_TEST(GetStats);
WEBRTC_BASE_TEST(GetStatsMultipleRecvStreams);
WEBRTC_BASE_TEST(GetStatsMultipleSendStreams);

WEBRTC_BASE_TEST(SetSendBandwidth);

WEBRTC_BASE_TEST(SetSendSsrc);
WEBRTC_BASE_TEST(SetSendSsrcAfterSetCodecs);

WEBRTC_BASE_TEST(SetSink);

WEBRTC_BASE_TEST(AddRemoveSendStreams);

WEBRTC_BASE_TEST(SimulateConference);

WEBRTC_DISABLED_BASE_TEST(AddRemoveCapturer);

WEBRTC_BASE_TEST(RemoveCapturerWithoutAdd);

WEBRTC_BASE_TEST(AddRemoveCapturerMultipleSources);

WEBRTC_BASE_TEST(RejectEmptyStreamParams);

WEBRTC_BASE_TEST(MultipleSendStreams);

TEST_F(WebRtcVideoChannelBaseTest, SendAndReceiveVp8Vga) {
  SendAndReceive(GetEngineCodec("VP8"));
}

TEST_F(WebRtcVideoChannelBaseTest, SendAndReceiveVp8Qvga) {
  SendAndReceive(GetEngineCodec("VP8"));
}

TEST_F(WebRtcVideoChannelBaseTest, SendAndReceiveVp8SvcQqvga) {
  SendAndReceive(GetEngineCodec("VP8"));
}

TEST_F(WebRtcVideoChannelBaseTest, TwoStreamsSendAndReceive) {
  // Set a high bitrate to not be downscaled by VP8 due to low initial start
  // bitrates. This currently happens at <250k, and two streams sharing 300k
  // initially will use QVGA instead of VGA.
  // TODO(pbos): Set up the quality scaler so that both senders reliably start
  // at QVGA, then verify that instead.
  cricket::VideoCodec codec = GetEngineCodec("VP8");
  codec.params[kCodecParamStartBitrate] = "1000000";
  Base::TwoStreamsSendAndReceive(codec);
}

class WebRtcVideoChannelTest : public WebRtcVideoEngineTest {
 public:
  WebRtcVideoChannelTest() : WebRtcVideoChannelTest("") {}
  explicit WebRtcVideoChannelTest(const char* field_trials)
      : WebRtcVideoEngineTest(field_trials), last_ssrc_(0) {}
  void SetUp() override {
    fake_call_.reset(new FakeCall(webrtc::Call::Config(&event_log_)));
    channel_.reset(engine_.CreateChannel(fake_call_.get(), GetMediaConfig(),
                                         VideoOptions()));
    channel_->OnReadyToSend(true);
    last_ssrc_ = 123;
    send_parameters_.codecs = engine_.codecs();
    recv_parameters_.codecs = engine_.codecs();
    ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));
  }

 protected:
  FakeVideoSendStream* AddSendStream() {
    return AddSendStream(StreamParams::CreateLegacy(++last_ssrc_));
  }

  FakeVideoSendStream* AddSendStream(const StreamParams& sp) {
    size_t num_streams = fake_call_->GetVideoSendStreams().size();
    EXPECT_TRUE(channel_->AddSendStream(sp));
    std::vector<FakeVideoSendStream*> streams =
        fake_call_->GetVideoSendStreams();
    EXPECT_EQ(num_streams + 1, streams.size());
    return streams[streams.size() - 1];
  }

  std::vector<FakeVideoSendStream*> GetFakeSendStreams() {
    return fake_call_->GetVideoSendStreams();
  }

  FakeVideoReceiveStream* AddRecvStream() {
    return AddRecvStream(StreamParams::CreateLegacy(++last_ssrc_));
  }

  FakeVideoReceiveStream* AddRecvStream(const StreamParams& sp) {
    size_t num_streams = fake_call_->GetVideoReceiveStreams().size();
    EXPECT_TRUE(channel_->AddRecvStream(sp));
    std::vector<FakeVideoReceiveStream*> streams =
        fake_call_->GetVideoReceiveStreams();
    EXPECT_EQ(num_streams + 1, streams.size());
    return streams[streams.size() - 1];
  }

  void SetSendCodecsShouldWorkForBitrates(const char* min_bitrate_kbps,
                                          int expected_min_bitrate_bps,
                                          const char* start_bitrate_kbps,
                                          int expected_start_bitrate_bps,
                                          const char* max_bitrate_kbps,
                                          int expected_max_bitrate_bps) {
    auto& codecs = send_parameters_.codecs;
    codecs.clear();
    codecs.push_back(GetEngineCodec("VP8"));
    codecs[0].params[kCodecParamMinBitrate] = min_bitrate_kbps;
    codecs[0].params[kCodecParamStartBitrate] = start_bitrate_kbps;
    codecs[0].params[kCodecParamMaxBitrate] = max_bitrate_kbps;
    EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

    EXPECT_EQ(expected_min_bitrate_bps,
              fake_call_->GetConfig().bitrate_config.min_bitrate_bps);
    EXPECT_EQ(expected_start_bitrate_bps,
              fake_call_->GetConfig().bitrate_config.start_bitrate_bps);
    EXPECT_EQ(expected_max_bitrate_bps,
              fake_call_->GetConfig().bitrate_config.max_bitrate_bps);
  }

  void TestSetSendRtpHeaderExtensions(const std::string& ext_uri) {
    // Enable extension.
    const int id = 1;
    cricket::VideoSendParameters parameters = send_parameters_;
    parameters.extensions.push_back(RtpExtension(ext_uri, id));
    EXPECT_TRUE(channel_->SetSendParameters(parameters));
    FakeVideoSendStream* send_stream =
        AddSendStream(cricket::StreamParams::CreateLegacy(123));

    // Verify the send extension id.
    ASSERT_EQ(1u, send_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(id, send_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(ext_uri, send_stream->GetConfig().rtp.extensions[0].uri);
    // Verify call with same set of extensions returns true.
    EXPECT_TRUE(channel_->SetSendParameters(parameters));
    // Verify that SetSendRtpHeaderExtensions doesn't implicitly add them for
    // receivers.
    EXPECT_TRUE(AddRecvStream(cricket::StreamParams::CreateLegacy(123))
                    ->GetConfig()
                    .rtp.extensions.empty());

    // Verify that existing RTP header extensions can be removed.
    EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
    ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());
    send_stream = fake_call_->GetVideoSendStreams()[0];
    EXPECT_TRUE(send_stream->GetConfig().rtp.extensions.empty());

    // Verify that adding receive RTP header extensions adds them for existing
    // streams.
    EXPECT_TRUE(channel_->SetSendParameters(parameters));
    send_stream = fake_call_->GetVideoSendStreams()[0];
    ASSERT_EQ(1u, send_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(id, send_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(ext_uri, send_stream->GetConfig().rtp.extensions[0].uri);
  }

  void TestSetRecvRtpHeaderExtensions(const std::string& ext_uri) {
    // Enable extension.
    const int id = 1;
    cricket::VideoRecvParameters parameters = recv_parameters_;
    parameters.extensions.push_back(RtpExtension(ext_uri, id));
    EXPECT_TRUE(channel_->SetRecvParameters(parameters));

    FakeVideoReceiveStream* recv_stream =
        AddRecvStream(cricket::StreamParams::CreateLegacy(123));

    // Verify the recv extension id.
    ASSERT_EQ(1u, recv_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(id, recv_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(ext_uri, recv_stream->GetConfig().rtp.extensions[0].uri);
    // Verify call with same set of extensions returns true.
    EXPECT_TRUE(channel_->SetRecvParameters(parameters));

    // Verify that SetRecvRtpHeaderExtensions doesn't implicitly add them for
    // senders.
    EXPECT_TRUE(AddSendStream(cricket::StreamParams::CreateLegacy(123))
                    ->GetConfig()
                    .rtp.extensions.empty());

    // Verify that existing RTP header extensions can be removed.
    EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
    ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
    recv_stream = fake_call_->GetVideoReceiveStreams()[0];
    EXPECT_TRUE(recv_stream->GetConfig().rtp.extensions.empty());

    // Verify that adding receive RTP header extensions adds them for existing
    // streams.
    EXPECT_TRUE(channel_->SetRecvParameters(parameters));
    recv_stream = fake_call_->GetVideoReceiveStreams()[0];
    ASSERT_EQ(1u, recv_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(id, recv_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(ext_uri, recv_stream->GetConfig().rtp.extensions[0].uri);
  }

  void TestExtensionFilter(const std::vector<std::string>& extensions,
                           const std::string& expected_extension) {
    cricket::VideoSendParameters parameters = send_parameters_;
    int expected_id = -1;
    int id = 1;
    for (const std::string& extension : extensions) {
      if (extension == expected_extension)
        expected_id = id;
      parameters.extensions.push_back(RtpExtension(extension, id++));
    }
    EXPECT_TRUE(channel_->SetSendParameters(parameters));
    FakeVideoSendStream* send_stream =
        AddSendStream(cricket::StreamParams::CreateLegacy(123));

    // Verify that only one of them has been set, and that it is the one with
    // highest priority (transport sequence number).
    ASSERT_EQ(1u, send_stream->GetConfig().rtp.extensions.size());
    EXPECT_EQ(expected_id, send_stream->GetConfig().rtp.extensions[0].id);
    EXPECT_EQ(expected_extension,
              send_stream->GetConfig().rtp.extensions[0].uri);
  }

  void TestDegradationPreference(bool resolution_scaling_enabled,
                                 bool fps_scaling_enabled);

  void TestCpuAdaptation(bool enable_overuse, bool is_screenshare);
  void TestReceiverLocalSsrcConfiguration(bool receiver_first);
  void TestReceiveUnsignaledSsrcPacket(uint8_t payload_type,
                                       bool expect_created_receive_stream);

  FakeVideoSendStream* SetDenoisingOption(
      uint32_t ssrc,
      cricket::FakeVideoCapturer* capturer,
      bool enabled) {
    cricket::VideoOptions options;
    options.video_noise_reduction = enabled;
    EXPECT_TRUE(channel_->SetVideoSend(ssrc, true, &options, capturer));
    // Options only take effect on the next frame.
    EXPECT_TRUE(capturer->CaptureFrame());

    return fake_call_->GetVideoSendStreams().back();
  }

  FakeVideoSendStream* SetUpSimulcast(bool enabled, bool with_rtx) {
    const int kRtxSsrcOffset = 0xDEADBEEF;
    last_ssrc_ += 3;
    std::vector<uint32_t> ssrcs;
    std::vector<uint32_t> rtx_ssrcs;
    uint32_t num_streams = enabled ? 3 : 1;
    for (uint32_t i = 0; i < num_streams; ++i) {
      uint32_t ssrc = last_ssrc_ + i;
      ssrcs.push_back(ssrc);
      if (with_rtx) {
        rtx_ssrcs.push_back(ssrc + kRtxSsrcOffset);
      }
    }
    if (with_rtx) {
      return AddSendStream(
          cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs));
    }
    return AddSendStream(CreateSimStreamParams("cname", ssrcs));
  }

  int GetMaxEncoderBitrate() {
    std::vector<FakeVideoSendStream*> streams =
        fake_call_->GetVideoSendStreams();
    EXPECT_EQ(1u, streams.size());
    FakeVideoSendStream* stream = streams[streams.size() - 1];
    EXPECT_EQ(1, stream->GetEncoderConfig().number_of_streams);
    return stream->GetVideoStreams()[0].max_bitrate_bps;
  }

  void SetAndExpectMaxBitrate(int global_max,
                              int stream_max,
                              int expected_encoder_bitrate) {
    VideoSendParameters limited_send_params = send_parameters_;
    limited_send_params.max_bandwidth_bps = global_max;
    EXPECT_TRUE(channel_->SetSendParameters(limited_send_params));
    webrtc::RtpParameters parameters =
        channel_->GetRtpSendParameters(last_ssrc_);
    EXPECT_EQ(1UL, parameters.encodings.size());
    parameters.encodings[0].max_bitrate_bps = stream_max;
    EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters));
    // Read back the parameteres and verify they have the correct value
    parameters = channel_->GetRtpSendParameters(last_ssrc_);
    EXPECT_EQ(1UL, parameters.encodings.size());
    EXPECT_EQ(stream_max, parameters.encodings[0].max_bitrate_bps);
    // Verify that the new value propagated down to the encoder
    EXPECT_EQ(expected_encoder_bitrate, GetMaxEncoderBitrate());
  }

  std::unique_ptr<FakeCall> fake_call_;
  std::unique_ptr<VideoMediaChannel> channel_;
  cricket::VideoSendParameters send_parameters_;
  cricket::VideoRecvParameters recv_parameters_;
  uint32_t last_ssrc_;
};

TEST_F(WebRtcVideoChannelTest, SetsSyncGroupFromSyncLabel) {
  const uint32_t kVideoSsrc = 123;
  const std::string kSyncLabel = "AvSyncLabel";

  cricket::StreamParams sp = cricket::StreamParams::CreateLegacy(kVideoSsrc);
  sp.sync_label = kSyncLabel;
  EXPECT_TRUE(channel_->AddRecvStream(sp));

  EXPECT_EQ(1, fake_call_->GetVideoReceiveStreams().size());
  EXPECT_EQ(kSyncLabel,
            fake_call_->GetVideoReceiveStreams()[0]->GetConfig().sync_group)
      << "SyncGroup should be set based on sync_label";
}

TEST_F(WebRtcVideoChannelTest, RecvStreamWithSimAndRtx) {
  cricket::VideoSendParameters parameters;
  parameters.codecs = engine_.codecs();
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(channel_->SetSend(true));
  parameters.conference_mode = true;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  // Send side.
  const std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32_t> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);
  FakeVideoSendStream* send_stream = AddSendStream(
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs));

  ASSERT_EQ(rtx_ssrcs.size(), send_stream->GetConfig().rtp.rtx.ssrcs.size());
  for (size_t i = 0; i < rtx_ssrcs.size(); ++i)
    EXPECT_EQ(rtx_ssrcs[i], send_stream->GetConfig().rtp.rtx.ssrcs[i]);

  // Receiver side.
  FakeVideoReceiveStream* recv_stream = AddRecvStream(
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs));
  EXPECT_FALSE(
      recv_stream->GetConfig().rtp.rtx_associated_payload_types.empty());
  EXPECT_TRUE(VerifyRtxReceiveAssociations(recv_stream->GetConfig()))
      << "RTX should be mapped for all decoders/payload types.";
  EXPECT_TRUE(HasRtxReceiveAssociation(recv_stream->GetConfig(),
                                          GetEngineCodec("red").id))
      << "RTX should be mapped for the RED payload type";

  EXPECT_EQ(rtx_ssrcs[0], recv_stream->GetConfig().rtp.rtx_ssrc);
}

TEST_F(WebRtcVideoChannelTest, RecvStreamWithRtx) {
  // Setup one channel with an associated RTX stream.
  cricket::StreamParams params =
      cricket::StreamParams::CreateLegacy(kSsrcs1[0]);
  params.AddFidSsrc(kSsrcs1[0], kRtxSsrcs1[0]);
  FakeVideoReceiveStream* recv_stream = AddRecvStream(params);
  EXPECT_EQ(kRtxSsrcs1[0], recv_stream->GetConfig().rtp.rtx_ssrc);

  EXPECT_TRUE(VerifyRtxReceiveAssociations(recv_stream->GetConfig()))
      << "RTX should be mapped for all decoders/payload types.";
  EXPECT_TRUE(HasRtxReceiveAssociation(recv_stream->GetConfig(),
                                          GetEngineCodec("red").id))
      << "RTX should be mapped for the RED payload type";
}

TEST_F(WebRtcVideoChannelTest, RecvStreamNoRtx) {
  // Setup one channel without an associated RTX stream.
  cricket::StreamParams params =
      cricket::StreamParams::CreateLegacy(kSsrcs1[0]);
  FakeVideoReceiveStream* recv_stream = AddRecvStream(params);
  ASSERT_EQ(0U, recv_stream->GetConfig().rtp.rtx_ssrc);
}

TEST_F(WebRtcVideoChannelTest, NoHeaderExtesionsByDefault) {
  FakeVideoSendStream* send_stream =
      AddSendStream(cricket::StreamParams::CreateLegacy(kSsrcs1[0]));
  ASSERT_TRUE(send_stream->GetConfig().rtp.extensions.empty());

  FakeVideoReceiveStream* recv_stream =
      AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrcs1[0]));
  ASSERT_TRUE(recv_stream->GetConfig().rtp.extensions.empty());
}

// Test support for RTP timestamp offset header extension.
TEST_F(WebRtcVideoChannelTest, SendRtpTimestampOffsetHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(RtpExtension::kTimestampOffsetUri);
}

TEST_F(WebRtcVideoChannelTest, RecvRtpTimestampOffsetHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(RtpExtension::kTimestampOffsetUri);
}

// Test support for absolute send time header extension.
TEST_F(WebRtcVideoChannelTest, SendAbsoluteSendTimeHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(RtpExtension::kAbsSendTimeUri);
}

TEST_F(WebRtcVideoChannelTest, RecvAbsoluteSendTimeHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(RtpExtension::kAbsSendTimeUri);
}

TEST_F(WebRtcVideoChannelTest, FiltersExtensionsPicksTransportSeqNum) {
  // Enable three redundant extensions.
  std::vector<std::string> extensions;
  extensions.push_back(RtpExtension::kAbsSendTimeUri);
  extensions.push_back(RtpExtension::kTimestampOffsetUri);
  extensions.push_back(RtpExtension::kTransportSequenceNumberUri);
  TestExtensionFilter(extensions, RtpExtension::kTransportSequenceNumberUri);
}

TEST_F(WebRtcVideoChannelTest, FiltersExtensionsPicksAbsSendTime) {
  // Enable two redundant extensions.
  std::vector<std::string> extensions;
  extensions.push_back(RtpExtension::kAbsSendTimeUri);
  extensions.push_back(RtpExtension::kTimestampOffsetUri);
  TestExtensionFilter(extensions, RtpExtension::kAbsSendTimeUri);
}

// Test support for transport sequence number header extension.
TEST_F(WebRtcVideoChannelTest, SendTransportSequenceNumberHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(RtpExtension::kTransportSequenceNumberUri);
}
TEST_F(WebRtcVideoChannelTest, RecvTransportSequenceNumberHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(RtpExtension::kTransportSequenceNumberUri);
}

// Test support for video rotation header extension.
TEST_F(WebRtcVideoChannelTest, SendVideoRotationHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(RtpExtension::kVideoRotationUri);
}
TEST_F(WebRtcVideoChannelTest, RecvVideoRotationHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(RtpExtension::kVideoRotationUri);
}

TEST_F(WebRtcVideoChannelTest, IdenticalSendExtensionsDoesntRecreateStream) {
  const int kAbsSendTimeId = 1;
  const int kVideoRotationId = 2;
  send_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTimeUri, kAbsSendTimeId));
  send_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kVideoRotationUri, kVideoRotationId));

  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  FakeVideoSendStream* send_stream =
      AddSendStream(cricket::StreamParams::CreateLegacy(123));

  EXPECT_EQ(1, fake_call_->GetNumCreatedSendStreams());
  ASSERT_EQ(2u, send_stream->GetConfig().rtp.extensions.size());

  // Setting the same extensions (even if in different order) shouldn't
  // reallocate the stream.
  std::reverse(send_parameters_.extensions.begin(),
               send_parameters_.extensions.end());
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  EXPECT_EQ(1, fake_call_->GetNumCreatedSendStreams());

  // Setting different extensions should recreate the stream.
  send_parameters_.extensions.resize(1);
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  EXPECT_EQ(2, fake_call_->GetNumCreatedSendStreams());
}

TEST_F(WebRtcVideoChannelTest, IdenticalRecvExtensionsDoesntRecreateStream) {
  const int kTOffsetId = 1;
  const int kAbsSendTimeId = 2;
  const int kVideoRotationId = 3;
  recv_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTimeUri, kAbsSendTimeId));
  recv_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kTimestampOffsetUri, kTOffsetId));
  recv_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kVideoRotationUri, kVideoRotationId));

  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
  FakeVideoReceiveStream* recv_stream =
      AddRecvStream(cricket::StreamParams::CreateLegacy(123));

  EXPECT_EQ(1, fake_call_->GetNumCreatedReceiveStreams());
  ASSERT_EQ(3u, recv_stream->GetConfig().rtp.extensions.size());

  // Setting the same extensions (even if in different order) shouldn't
  // reallocate the stream.
  std::reverse(recv_parameters_.extensions.begin(),
               recv_parameters_.extensions.end());
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));

  EXPECT_EQ(1, fake_call_->GetNumCreatedReceiveStreams());

  // Setting different extensions should recreate the stream.
  recv_parameters_.extensions.resize(1);
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));

  EXPECT_EQ(2, fake_call_->GetNumCreatedReceiveStreams());
}

TEST_F(WebRtcVideoChannelTest,
       SetSendRtpHeaderExtensionsExcludeUnsupportedExtensions) {
  const int kUnsupportedId = 1;
  const int kTOffsetId = 2;

  send_parameters_.extensions.push_back(
      RtpExtension(kUnsupportedExtensionName, kUnsupportedId));
  send_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kTimestampOffsetUri, kTOffsetId));
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  FakeVideoSendStream* send_stream =
      AddSendStream(cricket::StreamParams::CreateLegacy(123));

  // Only timestamp offset extension is set to send stream,
  // unsupported rtp extension is ignored.
  ASSERT_EQ(1u, send_stream->GetConfig().rtp.extensions.size());
  EXPECT_STREQ(RtpExtension::kTimestampOffsetUri,
               send_stream->GetConfig().rtp.extensions[0].uri.c_str());
}

TEST_F(WebRtcVideoChannelTest,
       SetRecvRtpHeaderExtensionsExcludeUnsupportedExtensions) {
  const int kUnsupportedId = 1;
  const int kTOffsetId = 2;

  recv_parameters_.extensions.push_back(
      RtpExtension(kUnsupportedExtensionName, kUnsupportedId));
  recv_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kTimestampOffsetUri, kTOffsetId));
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
  FakeVideoReceiveStream* recv_stream =
      AddRecvStream(cricket::StreamParams::CreateLegacy(123));

  // Only timestamp offset extension is set to receive stream,
  // unsupported rtp extension is ignored.
  ASSERT_EQ(1u, recv_stream->GetConfig().rtp.extensions.size());
  EXPECT_STREQ(RtpExtension::kTimestampOffsetUri,
               recv_stream->GetConfig().rtp.extensions[0].uri.c_str());
}

TEST_F(WebRtcVideoChannelTest, SetSendRtpHeaderExtensionsRejectsIncorrectIds) {
  const int kIncorrectIds[] = {-2, -1, 0, 15, 16};
  for (size_t i = 0; i < arraysize(kIncorrectIds); ++i) {
    send_parameters_.extensions.push_back(
        RtpExtension(RtpExtension::kTimestampOffsetUri, kIncorrectIds[i]));
    EXPECT_FALSE(channel_->SetSendParameters(send_parameters_))
        << "Bad extension id '" << kIncorrectIds[i] << "' accepted.";
  }
}

TEST_F(WebRtcVideoChannelTest, SetRecvRtpHeaderExtensionsRejectsIncorrectIds) {
  const int kIncorrectIds[] = {-2, -1, 0, 15, 16};
  for (size_t i = 0; i < arraysize(kIncorrectIds); ++i) {
    recv_parameters_.extensions.push_back(
        RtpExtension(RtpExtension::kTimestampOffsetUri, kIncorrectIds[i]));
    EXPECT_FALSE(channel_->SetRecvParameters(recv_parameters_))
        << "Bad extension id '" << kIncorrectIds[i] << "' accepted.";
  }
}

TEST_F(WebRtcVideoChannelTest, SetSendRtpHeaderExtensionsRejectsDuplicateIds) {
  const int id = 1;
  send_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kTimestampOffsetUri, id));
  send_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTimeUri, id));
  EXPECT_FALSE(channel_->SetSendParameters(send_parameters_));

  // Duplicate entries are also not supported.
  send_parameters_.extensions.clear();
  send_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kTimestampOffsetUri, id));
  send_parameters_.extensions.push_back(send_parameters_.extensions.back());
  EXPECT_FALSE(channel_->SetSendParameters(send_parameters_));
}

TEST_F(WebRtcVideoChannelTest, SetRecvRtpHeaderExtensionsRejectsDuplicateIds) {
  const int id = 1;
  recv_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kTimestampOffsetUri, id));
  recv_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kAbsSendTimeUri, id));
  EXPECT_FALSE(channel_->SetRecvParameters(recv_parameters_));

  // Duplicate entries are also not supported.
  recv_parameters_.extensions.clear();
  recv_parameters_.extensions.push_back(
      RtpExtension(RtpExtension::kTimestampOffsetUri, id));
  recv_parameters_.extensions.push_back(recv_parameters_.extensions.back());
  EXPECT_FALSE(channel_->SetRecvParameters(recv_parameters_));
}

TEST_F(WebRtcVideoChannelTest, AddRecvStreamOnlyUsesOneReceiveStream) {
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  EXPECT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
}

TEST_F(WebRtcVideoChannelTest, RtcpIsCompoundByDefault) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_EQ(webrtc::RtcpMode::kCompound, stream->GetConfig().rtp.rtcp_mode);
}

TEST_F(WebRtcVideoChannelTest, RembIsEnabledByDefault) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_TRUE(stream->GetConfig().rtp.remb);
}

TEST_F(WebRtcVideoChannelTest, TransportCcIsEnabledByDefault) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_TRUE(stream->GetConfig().rtp.transport_cc);
}

TEST_F(WebRtcVideoChannelTest, RembCanBeEnabledAndDisabled) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_TRUE(stream->GetConfig().rtp.remb);

  // Verify that REMB is turned off when send(!) codecs without REMB are set.
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(RemoveFeedbackParams(GetEngineCodec("VP8")));
  EXPECT_TRUE(parameters.codecs[0].feedback_params.params().empty());
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_FALSE(stream->GetConfig().rtp.remb);

  // Verify that REMB is turned on when setting default codecs since the
  // default codecs have REMB enabled.
  parameters.codecs = engine_.codecs();
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_TRUE(stream->GetConfig().rtp.remb);
}

TEST_F(WebRtcVideoChannelTest, TransportCcCanBeEnabledAndDisabled) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_TRUE(stream->GetConfig().rtp.transport_cc);

  // Verify that transport cc feedback is turned off when send(!) codecs without
  // transport cc feedback are set.
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(RemoveFeedbackParams(GetEngineCodec("VP8")));
  EXPECT_TRUE(parameters.codecs[0].feedback_params.params().empty());
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_FALSE(stream->GetConfig().rtp.transport_cc);

  // Verify that transport cc feedback is turned on when setting default codecs
  // since the default codecs have transport cc feedback enabled.
  parameters.codecs = engine_.codecs();
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_TRUE(stream->GetConfig().rtp.transport_cc);
}

TEST_F(WebRtcVideoChannelTest, NackIsEnabledByDefault) {
  VerifyCodecHasDefaultFeedbackParams(default_codec_);

  cricket::VideoSendParameters parameters;
  parameters.codecs = engine_.codecs();
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(channel_->SetSend(true));

  // Send side.
  FakeVideoSendStream* send_stream =
      AddSendStream(cricket::StreamParams::CreateLegacy(1));
  EXPECT_GT(send_stream->GetConfig().rtp.nack.rtp_history_ms, 0);

  // Receiver side.
  FakeVideoReceiveStream* recv_stream =
      AddRecvStream(cricket::StreamParams::CreateLegacy(1));
  EXPECT_GT(recv_stream->GetConfig().rtp.nack.rtp_history_ms, 0);

  // Nack history size should match between sender and receiver.
  EXPECT_EQ(send_stream->GetConfig().rtp.nack.rtp_history_ms,
            recv_stream->GetConfig().rtp.nack.rtp_history_ms);
}

TEST_F(WebRtcVideoChannelTest, NackCanBeEnabledAndDisabled) {
  FakeVideoSendStream* send_stream = AddSendStream();
  FakeVideoReceiveStream* recv_stream = AddRecvStream();

  EXPECT_GT(recv_stream->GetConfig().rtp.nack.rtp_history_ms, 0);
  EXPECT_GT(send_stream->GetConfig().rtp.nack.rtp_history_ms, 0);

  // Verify that NACK is turned off when send(!) codecs without NACK are set.
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(RemoveFeedbackParams(GetEngineCodec("VP8")));
  EXPECT_TRUE(parameters.codecs[0].feedback_params.params().empty());
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  recv_stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(0, recv_stream->GetConfig().rtp.nack.rtp_history_ms);
  send_stream = fake_call_->GetVideoSendStreams()[0];
  EXPECT_EQ(0, send_stream->GetConfig().rtp.nack.rtp_history_ms);

  // Verify that NACK is turned on when setting default codecs since the
  // default codecs have NACK enabled.
  parameters.codecs = engine_.codecs();
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  recv_stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_GT(recv_stream->GetConfig().rtp.nack.rtp_history_ms, 0);
  send_stream = fake_call_->GetVideoSendStreams()[0];
  EXPECT_GT(send_stream->GetConfig().rtp.nack.rtp_history_ms, 0);
}

// This test verifies that new frame sizes reconfigures encoders even though not
// (yet) sending. The purpose of this is to permit encoding as quickly as
// possible once we start sending. Likely the frames being input are from the
// same source that will be sent later, which just means that we're ready
// earlier.
TEST_F(WebRtcVideoChannelTest, ReconfiguresEncodersWhenNotSending) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  channel_->SetSend(false);

  FakeVideoSendStream* stream = AddSendStream();

  // No frames entered.
  std::vector<webrtc::VideoStream> streams = stream->GetVideoStreams();
  EXPECT_EQ(0u, streams[0].width);
  EXPECT_EQ(0u, streams[0].height);

  cricket::FakeVideoCapturer capturer;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, &capturer));
  VideoFormat capture_format = capturer.GetSupportedFormats()->front();
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(capture_format));
  EXPECT_TRUE(capturer.CaptureFrame());

  // Frame entered, should be reconfigured to new dimensions.
  streams = stream->GetVideoStreams();
  EXPECT_EQ(capture_format.width, streams[0].width);
  EXPECT_EQ(capture_format.height, streams[0].height);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest, UsesCorrectSettingsForScreencast) {
  static const int kScreenshareMinBitrateKbps = 800;
  cricket::VideoCodec codec = GetEngineCodec("VP8");
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(codec);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  AddSendStream();

  cricket::FakeVideoCapturer capturer;
  VideoOptions min_bitrate_options;
  min_bitrate_options.screencast_min_bitrate_kbps = kScreenshareMinBitrateKbps;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, &min_bitrate_options,
                                     &capturer));
  cricket::VideoFormat capture_format_hd =
      capturer.GetSupportedFormats()->front();
  EXPECT_EQ(1280, capture_format_hd.width);
  EXPECT_EQ(720, capture_format_hd.height);
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(capture_format_hd));

  EXPECT_TRUE(channel_->SetSend(true));

  EXPECT_TRUE(capturer.CaptureFrame());
  ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());
  FakeVideoSendStream* send_stream = fake_call_->GetVideoSendStreams().front();

  EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());

  // Verify non-screencast settings.
  webrtc::VideoEncoderConfig encoder_config =
      send_stream->GetEncoderConfig().Copy();
  EXPECT_EQ(webrtc::VideoEncoderConfig::ContentType::kRealtimeVideo,
            encoder_config.content_type);
  std::vector<webrtc::VideoStream> streams = send_stream->GetVideoStreams();
  EXPECT_EQ(capture_format_hd.width, streams.front().width);
  EXPECT_EQ(capture_format_hd.height, streams.front().height);
  EXPECT_EQ(0, encoder_config.min_transmit_bitrate_bps)
      << "Non-screenshare shouldn't use min-transmit bitrate.";

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, nullptr));
  EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());
  VideoOptions screencast_options;
  screencast_options.is_screencast = true;
  EXPECT_TRUE(
      channel_->SetVideoSend(last_ssrc_, true, &screencast_options, &capturer));
  EXPECT_TRUE(capturer.CaptureFrame());
  // Send stream recreated after option change.
  ASSERT_EQ(2, fake_call_->GetNumCreatedSendStreams());
  send_stream = fake_call_->GetVideoSendStreams().front();
  EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());

  // Verify screencast settings.
  encoder_config = send_stream->GetEncoderConfig().Copy();
  EXPECT_EQ(webrtc::VideoEncoderConfig::ContentType::kScreen,
            encoder_config.content_type);
  EXPECT_EQ(kScreenshareMinBitrateKbps * 1000,
            encoder_config.min_transmit_bitrate_bps);

  streams = send_stream->GetVideoStreams();
  EXPECT_EQ(capture_format_hd.width, streams.front().width);
  EXPECT_EQ(capture_format_hd.height, streams.front().height);
  EXPECT_TRUE(streams[0].temporal_layer_thresholds_bps.empty());
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest,
       ConferenceModeScreencastConfiguresTemporalLayer) {
  static const int kConferenceScreencastTemporalBitrateBps =
      ScreenshareLayerConfig::GetDefault().tl0_bitrate_kbps * 1000;
  send_parameters_.conference_mode = true;
  channel_->SetSendParameters(send_parameters_);

  AddSendStream();
  VideoOptions options;
  options.is_screencast = true;
  cricket::FakeVideoCapturer capturer;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, &options, &capturer));
  cricket::VideoFormat capture_format_hd =
      capturer.GetSupportedFormats()->front();
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(capture_format_hd));

  EXPECT_TRUE(channel_->SetSend(true));

  EXPECT_TRUE(capturer.CaptureFrame());
  ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());
  FakeVideoSendStream* send_stream = fake_call_->GetVideoSendStreams().front();

  webrtc::VideoEncoderConfig encoder_config =
      send_stream->GetEncoderConfig().Copy();

  // Verify screencast settings.
  encoder_config = send_stream->GetEncoderConfig().Copy();
  EXPECT_EQ(webrtc::VideoEncoderConfig::ContentType::kScreen,
            encoder_config.content_type);

  std::vector<webrtc::VideoStream> streams = send_stream->GetVideoStreams();
  ASSERT_EQ(1u, streams.size());
  ASSERT_EQ(1u, streams[0].temporal_layer_thresholds_bps.size());
  EXPECT_EQ(kConferenceScreencastTemporalBitrateBps,
            streams[0].temporal_layer_thresholds_bps[0]);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest, SuspendBelowMinBitrateDisabledByDefault) {
  FakeVideoSendStream* stream = AddSendStream();
  EXPECT_FALSE(stream->GetConfig().suspend_below_min_bitrate);
}

TEST_F(WebRtcVideoChannelTest, SetMediaConfigSuspendBelowMinBitrate) {
  MediaConfig media_config = GetMediaConfig();
  media_config.video.suspend_below_min_bitrate = true;

  channel_.reset(
      engine_.CreateChannel(fake_call_.get(), media_config, VideoOptions()));
  channel_->OnReadyToSend(true);

  channel_->SetSendParameters(send_parameters_);

  FakeVideoSendStream* stream = AddSendStream();
  EXPECT_TRUE(stream->GetConfig().suspend_below_min_bitrate);

  media_config.video.suspend_below_min_bitrate = false;
  channel_.reset(
      engine_.CreateChannel(fake_call_.get(), media_config, VideoOptions()));
  channel_->OnReadyToSend(true);

  channel_->SetSendParameters(send_parameters_);

  stream = AddSendStream();
  EXPECT_FALSE(stream->GetConfig().suspend_below_min_bitrate);
}

TEST_F(WebRtcVideoChannelTest, Vp8DenoisingEnabledByDefault) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoCodecVP8 vp8_settings;
  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_TRUE(vp8_settings.denoisingOn);
}

TEST_F(WebRtcVideoChannelTest, VerifyVp8SpecificSettings) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  // Single-stream settings should apply with RTX as well (verifies that we
  // check number of regular SSRCs and not StreamParams::ssrcs which contains
  // both RTX and regular SSRCs).
  FakeVideoSendStream* stream = SetUpSimulcast(false, true);

  cricket::FakeVideoCapturer capturer;
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, &capturer));
  channel_->SetSend(true);

  EXPECT_TRUE(capturer.CaptureFrame());

  webrtc::VideoCodecVP8 vp8_settings;
  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_TRUE(vp8_settings.denoisingOn)
      << "VP8 denoising should be on by default.";

  stream = SetDenoisingOption(last_ssrc_, &capturer, false);

  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_FALSE(vp8_settings.denoisingOn);
  EXPECT_TRUE(vp8_settings.automaticResizeOn);
  EXPECT_TRUE(vp8_settings.frameDroppingOn);

  stream = SetDenoisingOption(last_ssrc_, &capturer, true);

  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_TRUE(vp8_settings.denoisingOn);
  EXPECT_TRUE(vp8_settings.automaticResizeOn);
  EXPECT_TRUE(vp8_settings.frameDroppingOn);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, nullptr));
  stream = SetUpSimulcast(true, false);
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, &capturer));
  channel_->SetSend(true);
  EXPECT_TRUE(capturer.CaptureFrame());

  EXPECT_EQ(3, stream->GetVideoStreams().size());
  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  // Autmatic resize off when using simulcast.
  EXPECT_FALSE(vp8_settings.automaticResizeOn);
  EXPECT_TRUE(vp8_settings.frameDroppingOn);

  // In screen-share mode, denoising is forced off and simulcast disabled.
  VideoOptions options;
  options.is_screencast = true;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, &options, &capturer));

  stream = SetDenoisingOption(last_ssrc_, &capturer, false);

  EXPECT_EQ(1, stream->GetVideoStreams().size());
  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_FALSE(vp8_settings.denoisingOn);
  // Resizing and frame dropping always off for screen sharing.
  EXPECT_FALSE(vp8_settings.automaticResizeOn);
  EXPECT_FALSE(vp8_settings.frameDroppingOn);

  stream = SetDenoisingOption(last_ssrc_, &capturer, true);

  ASSERT_TRUE(stream->GetVp8Settings(&vp8_settings)) << "No VP8 config set.";
  EXPECT_FALSE(vp8_settings.denoisingOn);
  EXPECT_FALSE(vp8_settings.automaticResizeOn);
  EXPECT_FALSE(vp8_settings.frameDroppingOn);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, nullptr));
}

// Test that setting the same options doesn't result in the encoder being
// reconfigured.
TEST_F(WebRtcVideoChannelTest, SetIdenticalOptionsDoesntReconfigureEncoder) {
  VideoOptions options;
  cricket::FakeVideoCapturer capturer;

  AddSendStream();
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  FakeVideoSendStream* send_stream = fake_call_->GetVideoSendStreams().front();

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, &options, &capturer));
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, &options, &capturer));
  EXPECT_TRUE(capturer.CaptureFrame());
  // Expect 1 reconfigurations at this point from the initial configuration.
  EXPECT_EQ(1, send_stream->num_encoder_reconfigurations());

  // Set the options one more time and expect no additional reconfigurations.
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, &options, &capturer));
  EXPECT_EQ(1, send_stream->num_encoder_reconfigurations());

  // Change |options| and expect 2 reconfigurations.
  options.video_noise_reduction = true;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, &options, &capturer));
  EXPECT_EQ(2, send_stream->num_encoder_reconfigurations());

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, nullptr));
}

class Vp9SettingsTest : public WebRtcVideoChannelTest {
 public:
  Vp9SettingsTest() : Vp9SettingsTest("") {}
  explicit Vp9SettingsTest(const char* field_trials)
      : WebRtcVideoChannelTest(field_trials) {
    encoder_factory_->AddSupportedVideoCodecType("VP9");
  }
  virtual ~Vp9SettingsTest() {}

 protected:
  void TearDown() override {
    // Remove references to encoder_factory_ since this will be destroyed
    // before channel_ and engine_.
    ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));
  }
};

TEST_F(Vp9SettingsTest, VerifyVp9SpecificSettings) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  FakeVideoSendStream* stream = SetUpSimulcast(false, false);

  cricket::FakeVideoCapturer capturer;
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, &capturer));
  channel_->SetSend(true);

  EXPECT_TRUE(capturer.CaptureFrame());

  webrtc::VideoCodecVP9 vp9_settings;
  ASSERT_TRUE(stream->GetVp9Settings(&vp9_settings)) << "No VP9 config set.";
  EXPECT_TRUE(vp9_settings.denoisingOn)
      << "VP9 denoising should be on by default.";

  stream = SetDenoisingOption(last_ssrc_, &capturer, false);

  ASSERT_TRUE(stream->GetVp9Settings(&vp9_settings)) << "No VP9 config set.";
  EXPECT_FALSE(vp9_settings.denoisingOn);
  // Frame dropping always on for real time video.
  EXPECT_TRUE(vp9_settings.frameDroppingOn);

  stream = SetDenoisingOption(last_ssrc_, &capturer, true);

  ASSERT_TRUE(stream->GetVp9Settings(&vp9_settings)) << "No VP9 config set.";
  EXPECT_TRUE(vp9_settings.denoisingOn);
  EXPECT_TRUE(vp9_settings.frameDroppingOn);

  // In screen-share mode, denoising is forced off.
  VideoOptions options;
  options.is_screencast = true;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, &options, &capturer));

  stream = SetDenoisingOption(last_ssrc_, &capturer, false);

  ASSERT_TRUE(stream->GetVp9Settings(&vp9_settings)) << "No VP9 config set.";
  EXPECT_FALSE(vp9_settings.denoisingOn);
  // Frame dropping always off for screen sharing.
  EXPECT_FALSE(vp9_settings.frameDroppingOn);

  stream = SetDenoisingOption(last_ssrc_, &capturer, false);

  ASSERT_TRUE(stream->GetVp9Settings(&vp9_settings)) << "No VP9 config set.";
  EXPECT_FALSE(vp9_settings.denoisingOn);
  EXPECT_FALSE(vp9_settings.frameDroppingOn);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, nullptr));
}

class Vp9SettingsTestWithFieldTrial : public Vp9SettingsTest {
 public:
  explicit Vp9SettingsTestWithFieldTrial(const char* field_trials)
      : Vp9SettingsTest(field_trials) {}

 protected:
  void VerifySettings(int num_spatial_layers, int num_temporal_layers) {
    cricket::VideoSendParameters parameters;
    parameters.codecs.push_back(GetEngineCodec("VP9"));
    ASSERT_TRUE(channel_->SetSendParameters(parameters));

    FakeVideoSendStream* stream = SetUpSimulcast(false, false);

    cricket::FakeVideoCapturer capturer;
    EXPECT_EQ(cricket::CS_RUNNING,
              capturer.Start(capturer.GetSupportedFormats()->front()));
    EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, &capturer));
    channel_->SetSend(true);

    EXPECT_TRUE(capturer.CaptureFrame());

    webrtc::VideoCodecVP9 vp9_settings;
    ASSERT_TRUE(stream->GetVp9Settings(&vp9_settings)) << "No VP9 config set.";
    EXPECT_EQ(num_spatial_layers, vp9_settings.numberOfSpatialLayers);
    EXPECT_EQ(num_temporal_layers, vp9_settings.numberOfTemporalLayers);

    EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, nullptr));
  }
};

class Vp9SettingsTestWithNoFlag : public Vp9SettingsTestWithFieldTrial {
 public:
  Vp9SettingsTestWithNoFlag() : Vp9SettingsTestWithFieldTrial("") {}
};

TEST_F(Vp9SettingsTestWithNoFlag, VerifySettings) {
  const int kNumSpatialLayers = 1;
  const int kNumTemporalLayers = 1;
  VerifySettings(kNumSpatialLayers, kNumTemporalLayers);
}

class Vp9SettingsTestWithInvalidFlag : public Vp9SettingsTestWithFieldTrial {
 public:
  Vp9SettingsTestWithInvalidFlag()
      : Vp9SettingsTestWithFieldTrial("WebRTC-SupportVP9SVC/Default/") {}
};

TEST_F(Vp9SettingsTestWithInvalidFlag, VerifySettings) {
  const int kNumSpatialLayers = 1;
  const int kNumTemporalLayers = 1;
  VerifySettings(kNumSpatialLayers, kNumTemporalLayers);
}

class Vp9SettingsTestWith2SL3TLFlag : public Vp9SettingsTestWithFieldTrial {
 public:
  Vp9SettingsTestWith2SL3TLFlag()
      : Vp9SettingsTestWithFieldTrial(
            "WebRTC-SupportVP9SVC/EnabledByFlag_2SL3TL/") {}
};

TEST_F(Vp9SettingsTestWith2SL3TLFlag, VerifySettings) {
  const int kNumSpatialLayers = 2;
  const int kNumTemporalLayers = 3;
  VerifySettings(kNumSpatialLayers, kNumTemporalLayers);
}

TEST_F(WebRtcVideoChannelTest, VerifyMinBitrate) {
  std::vector<webrtc::VideoStream> streams = AddSendStream()->GetVideoStreams();
  ASSERT_EQ(1u, streams.size());
  EXPECT_EQ(cricket::kMinVideoBitrateBps, streams[0].min_bitrate_bps);
}

TEST_F(WebRtcVideoChannelTest, VerifyMinBitrateWithForcedFallbackFieldTrial) {
  webrtc::test::ScopedFieldTrials override_field_trials_(
      "WebRTC-VP8-Forced-Fallback-Encoder-v2/Enabled-1,2,34567/");
  std::vector<webrtc::VideoStream> streams = AddSendStream()->GetVideoStreams();
  ASSERT_EQ(1u, streams.size());
  EXPECT_EQ(34567, streams[0].min_bitrate_bps);
}

TEST_F(WebRtcVideoChannelTest,
       BalancedDegradationPreferenceNotSupportedWithoutFieldtrial) {
  webrtc::test::ScopedFieldTrials override_field_trials_(
      "WebRTC-Video-BalancedDegradation/Disabled/");
  const bool kResolutionScalingEnabled = true;
  const bool kFpsScalingEnabled = false;
  TestDegradationPreference(kResolutionScalingEnabled, kFpsScalingEnabled);
}

TEST_F(WebRtcVideoChannelTest,
       BalancedDegradationPreferenceSupportedBehindFieldtrial) {
  webrtc::test::ScopedFieldTrials override_field_trials_(
      "WebRTC-Video-BalancedDegradation/Enabled/");
  const bool kResolutionScalingEnabled = true;
  const bool kFpsScalingEnabled = true;
  TestDegradationPreference(kResolutionScalingEnabled, kFpsScalingEnabled);
}

TEST_F(WebRtcVideoChannelTest, AdaptsOnOveruse) {
  TestCpuAdaptation(true, false);
}

TEST_F(WebRtcVideoChannelTest, DoesNotAdaptOnOveruseWhenDisabled) {
  TestCpuAdaptation(false, false);
}

TEST_F(WebRtcVideoChannelTest, DoesNotAdaptOnOveruseWhenScreensharing) {
  TestCpuAdaptation(true, true);
}

TEST_F(WebRtcVideoChannelTest, AdaptsOnOveruseAndChangeResolution) {
  cricket::VideoCodec codec = GetEngineCodec("VP8");
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(codec);

  MediaConfig media_config = GetMediaConfig();
  channel_.reset(
      engine_.CreateChannel(fake_call_.get(), media_config, VideoOptions()));
  channel_->OnReadyToSend(true);
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  AddSendStream();

  cricket::FakeVideoCapturer capturer;
  ASSERT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, &capturer));
  ASSERT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  ASSERT_TRUE(channel_->SetSend(true));

  ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());
  FakeVideoSendStream* send_stream = fake_call_->GetVideoSendStreams().front();

  EXPECT_TRUE(capturer.CaptureCustomFrame(1280, 720, cricket::FOURCC_I420));
  EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1280, send_stream->GetLastWidth());
  EXPECT_EQ(720, send_stream->GetLastHeight());

  // Trigger overuse.
  rtc::VideoSinkWants wants;
  wants.max_pixel_count =
      send_stream->GetLastWidth() * send_stream->GetLastHeight() - 1;
  send_stream->InjectVideoSinkWants(wants);
  EXPECT_TRUE(capturer.CaptureCustomFrame(1280, 720, cricket::FOURCC_I420));
  EXPECT_EQ(2, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1280 * 3 / 4, send_stream->GetLastWidth());
  EXPECT_EQ(720 * 3 / 4, send_stream->GetLastHeight());

  // Trigger overuse again.
  wants.max_pixel_count =
      send_stream->GetLastWidth() * send_stream->GetLastHeight() - 1;
  send_stream->InjectVideoSinkWants(wants);
  EXPECT_TRUE(capturer.CaptureCustomFrame(1280, 720, cricket::FOURCC_I420));
  EXPECT_EQ(3, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1280 * 2 / 4, send_stream->GetLastWidth());
  EXPECT_EQ(720 * 2 / 4, send_stream->GetLastHeight());

  // Change input resolution.
  EXPECT_TRUE(capturer.CaptureCustomFrame(1284, 724, cricket::FOURCC_I420));
  EXPECT_EQ(4, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1284 / 2, send_stream->GetLastWidth());
  EXPECT_EQ(724 / 2, send_stream->GetLastHeight());

  // Trigger underuse which should go back up in resolution.
  int current_pixel_count =
      send_stream->GetLastWidth() * send_stream->GetLastHeight();
  // Cap the max to 4x the pixel count (assuming max 1/2 x 1/2 scale downs)
  // of the current stream, so we don't take too large steps.
  wants.max_pixel_count = current_pixel_count * 4;
  // Default step down is 3/5 pixel count, so go up by 5/3.
  wants.target_pixel_count = (current_pixel_count * 5 / 3);
  send_stream->InjectVideoSinkWants(wants);
  EXPECT_TRUE(capturer.CaptureCustomFrame(1284, 724, cricket::FOURCC_I420));
  EXPECT_EQ(5, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1284 * 3 / 4, send_stream->GetLastWidth());
  EXPECT_EQ(724 * 3 / 4, send_stream->GetLastHeight());

  // Trigger underuse again, should go back up to full resolution.
  current_pixel_count =
      send_stream->GetLastWidth() * send_stream->GetLastHeight();
  wants.max_pixel_count = current_pixel_count * 4;
  wants.target_pixel_count = (current_pixel_count * 5 / 3);
  send_stream->InjectVideoSinkWants(wants);
  EXPECT_TRUE(capturer.CaptureCustomFrame(1284, 724, cricket::FOURCC_I420));
  EXPECT_EQ(6, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1284, send_stream->GetLastWidth());
  EXPECT_EQ(724, send_stream->GetLastHeight());

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest, PreviousAdaptationDoesNotApplyToScreenshare) {
  cricket::VideoCodec codec = GetEngineCodec("VP8");
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(codec);

  MediaConfig media_config = GetMediaConfig();
  media_config.video.enable_cpu_overuse_detection = true;
  channel_.reset(
      engine_.CreateChannel(fake_call_.get(), media_config, VideoOptions()));
  channel_->OnReadyToSend(true);
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  AddSendStream();

  cricket::FakeVideoCapturer capturer;
  ASSERT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  ASSERT_TRUE(channel_->SetSend(true));
  cricket::VideoOptions camera_options;
  camera_options.is_screencast = false;
  channel_->SetVideoSend(last_ssrc_, true /* enable */, &camera_options,
                         &capturer);

  ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());
  FakeVideoSendStream* send_stream = fake_call_->GetVideoSendStreams().front();

  EXPECT_TRUE(capturer.CaptureCustomFrame(1280, 720, cricket::FOURCC_I420));
  EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1280, send_stream->GetLastWidth());
  EXPECT_EQ(720, send_stream->GetLastHeight());

  // Trigger overuse.
  rtc::VideoSinkWants wants;
  wants.max_pixel_count =
      send_stream->GetLastWidth() * send_stream->GetLastHeight() - 1;
  send_stream->InjectVideoSinkWants(wants);
  EXPECT_TRUE(capturer.CaptureCustomFrame(1280, 720, cricket::FOURCC_I420));
  EXPECT_EQ(2, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1280 * 3 / 4, send_stream->GetLastWidth());
  EXPECT_EQ(720 * 3 / 4, send_stream->GetLastHeight());

  // Switch to screen share. Expect no CPU adaptation.
  cricket::FakeVideoCapturer screen_share(true);
  ASSERT_EQ(cricket::CS_RUNNING,
            screen_share.Start(screen_share.GetSupportedFormats()->front()));
  cricket::VideoOptions screenshare_options;
  screenshare_options.is_screencast = true;
  channel_->SetVideoSend(last_ssrc_, true /* enable */, &screenshare_options,
                         &screen_share);
  EXPECT_TRUE(screen_share.CaptureCustomFrame(1284, 724, cricket::FOURCC_I420));
  ASSERT_EQ(2, fake_call_->GetNumCreatedSendStreams());
  send_stream = fake_call_->GetVideoSendStreams().front();
  EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1284, send_stream->GetLastWidth());
  EXPECT_EQ(724, send_stream->GetLastHeight());

  // Switch back to the normal capturer. Expect the frame to be CPU adapted.
  channel_->SetVideoSend(last_ssrc_, true /* enable */, &camera_options,
                         &capturer);
  send_stream = fake_call_->GetVideoSendStreams().front();
  // We have a new fake send stream, so it doesn't remember the old sink wants.
  // In practice, it will be populated from
  // VideoStreamEncoder::VideoSourceProxy::SetSource(), so simulate that here.
  send_stream->InjectVideoSinkWants(wants);
  EXPECT_TRUE(capturer.CaptureCustomFrame(1280, 720, cricket::FOURCC_I420));
  ASSERT_EQ(3, fake_call_->GetNumCreatedSendStreams());
  send_stream = fake_call_->GetVideoSendStreams().front();
  EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());
  EXPECT_EQ(1280 * 3 / 4, send_stream->GetLastWidth());
  EXPECT_EQ(720 * 3 / 4, send_stream->GetLastHeight());

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, nullptr));
}

// TODO(asapersson): Remove this test when the balanced field trial is removed.
void WebRtcVideoChannelTest::TestDegradationPreference(
    bool resolution_scaling_enabled,
    bool fps_scaling_enabled) {
  cricket::VideoCodec codec = GetEngineCodec("VP8");
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(codec);

  MediaConfig media_config = GetMediaConfig();
  media_config.video.enable_cpu_overuse_detection = true;
  channel_.reset(
      engine_.CreateChannel(fake_call_.get(), media_config, VideoOptions()));
  channel_->OnReadyToSend(true);

  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  AddSendStream();

  cricket::FakeVideoCapturer capturer;
  VideoOptions options;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, &options, &capturer));
  cricket::VideoFormat capture_format = capturer.GetSupportedFormats()->front();
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(capture_format));

  EXPECT_TRUE(channel_->SetSend(true));

  FakeVideoSendStream* send_stream = fake_call_->GetVideoSendStreams().front();
  EXPECT_EQ(resolution_scaling_enabled,
            send_stream->resolution_scaling_enabled());
  EXPECT_EQ(fps_scaling_enabled, send_stream->framerate_scaling_enabled());

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, nullptr));
}

void WebRtcVideoChannelTest::TestCpuAdaptation(bool enable_overuse,
                                               bool is_screenshare) {
  const int kDefaultFps = 30;
  cricket::VideoCodec codec = GetEngineCodec("VP8");
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(codec);

  MediaConfig media_config = GetMediaConfig();
  if (enable_overuse) {
    media_config.video.enable_cpu_overuse_detection = true;
  }
  channel_.reset(
      engine_.CreateChannel(fake_call_.get(), media_config, VideoOptions()));
  channel_->OnReadyToSend(true);

  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  AddSendStream();

  cricket::FakeVideoCapturer capturer;
  VideoOptions options;
  options.is_screencast = is_screenshare;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, &options, &capturer));
  cricket::VideoFormat capture_format = capturer.GetSupportedFormats()->front();
  capture_format.interval = rtc::kNumNanosecsPerSec / kDefaultFps;
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(capture_format));

  EXPECT_TRUE(channel_->SetSend(true));

  FakeVideoSendStream* send_stream = fake_call_->GetVideoSendStreams().front();

  if (!enable_overuse) {
    EXPECT_FALSE(send_stream->resolution_scaling_enabled());
    EXPECT_FALSE(send_stream->framerate_scaling_enabled());
    EXPECT_EQ(is_screenshare, send_stream->framerate_scaling_enabled());

    EXPECT_TRUE(capturer.CaptureFrame());
    EXPECT_EQ(1, send_stream->GetNumberOfSwappedFrames());

    EXPECT_EQ(capture_format.width, send_stream->GetLastWidth());
    EXPECT_EQ(capture_format.height, send_stream->GetLastHeight());

    EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, nullptr));
    return;
  }

  if (is_screenshare) {
    EXPECT_FALSE(send_stream->resolution_scaling_enabled());
    EXPECT_TRUE(send_stream->framerate_scaling_enabled());
  } else {
    EXPECT_TRUE(send_stream->resolution_scaling_enabled());
    EXPECT_FALSE(send_stream->framerate_scaling_enabled());
  }

  // Trigger overuse.
  ASSERT_EQ(1u, fake_call_->GetVideoSendStreams().size());

  rtc::VideoSinkWants wants;
  if (is_screenshare) {
    wants.max_framerate_fps = (kDefaultFps * 2) / 3;
  } else {
    wants.max_pixel_count = capture_format.width * capture_format.height - 1;
  }
  send_stream->InjectVideoSinkWants(wants);

  for (int i = 0; i < kDefaultFps; ++i)
    EXPECT_TRUE(capturer.CaptureFrame());

  if (is_screenshare) {
    // Drops every third frame.
    EXPECT_EQ(kDefaultFps * 2 / 3, send_stream->GetNumberOfSwappedFrames());
    EXPECT_EQ(send_stream->GetLastWidth(), capture_format.width);
    EXPECT_EQ(send_stream->GetLastHeight(), capture_format.height);
  } else {
    EXPECT_EQ(kDefaultFps, send_stream->GetNumberOfSwappedFrames());
    EXPECT_LT(send_stream->GetLastWidth(), capture_format.width);
    EXPECT_LT(send_stream->GetLastHeight(), capture_format.height);
  }

  // Trigger underuse which should go back to normal resolution.
  int last_pixel_count =
      send_stream->GetLastWidth() * send_stream->GetLastHeight();
  if (is_screenshare) {
    wants.max_framerate_fps = kDefaultFps;
  } else {
    wants.max_pixel_count = last_pixel_count * 4;
    wants.target_pixel_count.emplace((last_pixel_count * 5) / 3);
  }
  send_stream->InjectVideoSinkWants(wants);

  for (int i = 0; i < kDefaultFps; ++i)
    EXPECT_TRUE(capturer.CaptureFrame());

  if (is_screenshare) {
    EXPECT_EQ(kDefaultFps + (kDefaultFps * 2 / 3),
              send_stream->GetNumberOfSwappedFrames());
  } else {
    EXPECT_EQ(kDefaultFps * 2, send_stream->GetNumberOfSwappedFrames());
  }

  EXPECT_EQ(capture_format.width, send_stream->GetLastWidth());
  EXPECT_EQ(capture_format.height, send_stream->GetLastHeight());

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest, EstimatesNtpStartTimeCorrectly) {
  // Start at last timestamp to verify that wraparounds are estimated correctly.
  static const uint32_t kInitialTimestamp = 0xFFFFFFFFu;
  static const int64_t kInitialNtpTimeMs = 1247891230;
  static const int kFrameOffsetMs = 20;
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));

  FakeVideoReceiveStream* stream = AddRecvStream();
  cricket::FakeVideoRenderer renderer;
  EXPECT_TRUE(channel_->SetSink(last_ssrc_, &renderer));

  webrtc::VideoFrame video_frame(CreateBlackFrameBuffer(4, 4),
                                 kInitialTimestamp, 0,
                                 webrtc::kVideoRotation_0);
  // Initial NTP time is not available on the first frame, but should still be
  // able to be estimated.
  stream->InjectFrame(video_frame);

  EXPECT_EQ(1, renderer.num_rendered_frames());

  // This timestamp is kInitialTimestamp (-1) + kFrameOffsetMs * 90, which
  // triggers a constant-overflow warning, hence we're calculating it explicitly
  // here.
  video_frame.set_timestamp(kFrameOffsetMs * 90 - 1);
  video_frame.set_ntp_time_ms(kInitialNtpTimeMs + kFrameOffsetMs);
  stream->InjectFrame(video_frame);

  EXPECT_EQ(2, renderer.num_rendered_frames());

  // Verify that NTP time has been correctly deduced.
  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1u, info.receivers.size());
  EXPECT_EQ(kInitialNtpTimeMs, info.receivers[0].capture_start_ntp_time_ms);
}

TEST_F(WebRtcVideoChannelTest, SetDefaultSendCodecs) {
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));

  VideoCodec codec;
  EXPECT_TRUE(channel_->GetSendCodec(&codec));
  EXPECT_TRUE(codec.Matches(engine_.codecs()[0]));

  // Using a RTX setup to verify that the default RTX payload type is good.
  const std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32_t> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);
  FakeVideoSendStream* stream = AddSendStream(
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs));
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  // Make sure NACK and FEC are enabled on the correct payload types.
  EXPECT_EQ(1000, config.rtp.nack.rtp_history_ms);
  EXPECT_EQ(GetEngineCodec("ulpfec").id, config.rtp.ulpfec.ulpfec_payload_type);
  EXPECT_EQ(GetEngineCodec("red").id, config.rtp.ulpfec.red_payload_type);

  EXPECT_EQ(1u, config.rtp.rtx.ssrcs.size());
  EXPECT_EQ(kRtxSsrcs1[0], config.rtp.rtx.ssrcs[0]);
  VerifySendStreamHasRtxTypes(config, default_apt_rtx_types_);
  // TODO(juberti): Check RTCP, PLI, TMMBR.
}

// The following four tests ensures that FlexFEC is not activated by default
// when the field trials are not enabled.
// TODO(brandtr): Remove or update these tests when FlexFEC _is_ enabled by
// default.
TEST_F(WebRtcVideoChannelTest,
       FlexfecSendCodecWithoutSsrcNotExposedByDefault) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(-1, config.rtp.flexfec.payload_type);
  EXPECT_EQ(0U, config.rtp.flexfec.ssrc);
  EXPECT_TRUE(config.rtp.flexfec.protected_media_ssrcs.empty());
}

TEST_F(WebRtcVideoChannelTest, FlexfecSendCodecWithSsrcNotExposedByDefault) {
  FakeVideoSendStream* stream = AddSendStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(-1, config.rtp.flexfec.payload_type);
  EXPECT_EQ(0U, config.rtp.flexfec.ssrc);
  EXPECT_TRUE(config.rtp.flexfec.protected_media_ssrcs.empty());
}

TEST_F(WebRtcVideoChannelTest,
       FlexfecRecvCodecWithoutSsrcNotExposedByDefault) {
  AddRecvStream();

  const std::vector<FakeFlexfecReceiveStream*>& streams =
      fake_call_->GetFlexfecReceiveStreams();
  EXPECT_TRUE(streams.empty());
}

TEST_F(WebRtcVideoChannelTest, FlexfecRecvCodecWithSsrcNotExposedByDefault) {
  AddRecvStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));

  const std::vector<FakeFlexfecReceiveStream*>& streams =
      fake_call_->GetFlexfecReceiveStreams();
  EXPECT_TRUE(streams.empty());
}

// TODO(brandtr): When FlexFEC is no longer behind a field trial, merge all
// tests that use this test fixture into the corresponding "non-field trial"
// tests.
class WebRtcVideoChannelFlexfecRecvTest : public WebRtcVideoChannelTest {
 public:
  WebRtcVideoChannelFlexfecRecvTest()
      : WebRtcVideoChannelTest("WebRTC-FlexFEC-03-Advertised/Enabled/") {}
};

TEST_F(WebRtcVideoChannelFlexfecRecvTest,
       DefaultFlexfecCodecHasTransportCcAndRembFeedbackParam) {
  EXPECT_TRUE(cricket::HasTransportCc(GetEngineCodec("flexfec-03")));
  EXPECT_TRUE(cricket::HasRemb(GetEngineCodec("flexfec-03")));
}

TEST_F(WebRtcVideoChannelFlexfecRecvTest, SetDefaultRecvCodecsWithoutSsrc) {
  AddRecvStream();

  const std::vector<FakeFlexfecReceiveStream*>& streams =
      fake_call_->GetFlexfecReceiveStreams();
  EXPECT_TRUE(streams.empty());
}

TEST_F(WebRtcVideoChannelFlexfecRecvTest, SetDefaultRecvCodecsWithSsrc) {
  AddRecvStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));

  const std::vector<FakeFlexfecReceiveStream*>& streams =
      fake_call_->GetFlexfecReceiveStreams();
  ASSERT_EQ(1U, streams.size());
  const FakeFlexfecReceiveStream* stream = streams.front();
  const webrtc::FlexfecReceiveStream::Config& config = stream->GetConfig();
  EXPECT_EQ(GetEngineCodec("flexfec-03").id, config.payload_type);
  EXPECT_EQ(kFlexfecSsrc, config.remote_ssrc);
  ASSERT_EQ(1U, config.protected_media_ssrcs.size());
  EXPECT_EQ(kSsrcs1[0], config.protected_media_ssrcs[0]);

  const std::vector<FakeVideoReceiveStream*>& video_streams =
      fake_call_->GetVideoReceiveStreams();
  ASSERT_EQ(1U, video_streams.size());
  const webrtc::VideoReceiveStream::Config& video_config =
      video_streams.front()->GetConfig();
  EXPECT_TRUE(video_config.rtp.protected_by_flexfec);
}

TEST_F(WebRtcVideoChannelFlexfecRecvTest,
       EnablingFlexfecDoesNotRecreateVideoReceiveStream) {
  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));

  AddRecvStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));
  EXPECT_EQ(1, fake_call_->GetNumCreatedReceiveStreams());
  EXPECT_EQ(1U, fake_call_->GetVideoReceiveStreams().size());

  // Enable FlexFEC.
  recv_parameters.codecs.push_back(GetEngineCodec("flexfec-03"));
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));
  EXPECT_EQ(2, fake_call_->GetNumCreatedReceiveStreams())
      << "Enabling FlexFEC should create FlexfecReceiveStream.";
  EXPECT_EQ(1U, fake_call_->GetVideoReceiveStreams().size())
      << "Enabling FlexFEC should not create VideoReceiveStream.";
  EXPECT_EQ(1U, fake_call_->GetFlexfecReceiveStreams().size())
      << "Enabling FlexFEC should create a single FlexfecReceiveStream.";
}

TEST_F(WebRtcVideoChannelFlexfecRecvTest,
       DisablingFlexfecDoesNotRecreateVideoReceiveStream) {
  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(GetEngineCodec("VP8"));
  recv_parameters.codecs.push_back(GetEngineCodec("flexfec-03"));
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));

  AddRecvStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));
  EXPECT_EQ(2, fake_call_->GetNumCreatedReceiveStreams());
  EXPECT_EQ(1U, fake_call_->GetVideoReceiveStreams().size());
  EXPECT_EQ(1U, fake_call_->GetFlexfecReceiveStreams().size());

  // Disable FlexFEC.
  recv_parameters.codecs.clear();
  recv_parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));
  EXPECT_EQ(2, fake_call_->GetNumCreatedReceiveStreams())
      << "Disabling FlexFEC should not recreate VideoReceiveStream.";
  EXPECT_EQ(1U, fake_call_->GetVideoReceiveStreams().size())
      << "Disabling FlexFEC should not destroy VideoReceiveStream.";
  EXPECT_TRUE(fake_call_->GetFlexfecReceiveStreams().empty())
      << "Disabling FlexFEC should destroy FlexfecReceiveStream.";
}

// TODO(brandtr): When FlexFEC is no longer behind a field trial, merge all
// tests that use this test fixture into the corresponding "non-field trial"
// tests.
class WebRtcVideoChannelFlexfecSendRecvTest : public WebRtcVideoChannelTest {
 public:
  WebRtcVideoChannelFlexfecSendRecvTest()
      : WebRtcVideoChannelTest(
            "WebRTC-FlexFEC-03-Advertised/Enabled/WebRTC-FlexFEC-03/Enabled/") {
  }
};

TEST_F(WebRtcVideoChannelFlexfecSendRecvTest,
       SetDefaultSendCodecsWithoutSsrc) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(GetEngineCodec("flexfec-03").id, config.rtp.flexfec.payload_type);
  EXPECT_EQ(0U, config.rtp.flexfec.ssrc);
  EXPECT_TRUE(config.rtp.flexfec.protected_media_ssrcs.empty());
}

TEST_F(WebRtcVideoChannelFlexfecSendRecvTest, SetDefaultSendCodecsWithSsrc) {
  FakeVideoSendStream* stream = AddSendStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(GetEngineCodec("flexfec-03").id, config.rtp.flexfec.payload_type);
  EXPECT_EQ(kFlexfecSsrc, config.rtp.flexfec.ssrc);
  ASSERT_EQ(1U, config.rtp.flexfec.protected_media_ssrcs.size());
  EXPECT_EQ(kSsrcs1[0], config.rtp.flexfec.protected_media_ssrcs[0]);
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsWithoutFec) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(-1, config.rtp.ulpfec.ulpfec_payload_type);
  EXPECT_EQ(-1, config.rtp.ulpfec.red_payload_type);
}

TEST_F(WebRtcVideoChannelFlexfecSendRecvTest, SetSendCodecsWithoutFec) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(-1, config.rtp.flexfec.payload_type);
}

TEST_F(WebRtcVideoChannelFlexfecRecvTest, SetRecvCodecsWithFec) {
  AddRecvStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));

  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(GetEngineCodec("VP8"));
  recv_parameters.codecs.push_back(GetEngineCodec("flexfec-03"));
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));

  const std::vector<FakeFlexfecReceiveStream*>& flexfec_streams =
      fake_call_->GetFlexfecReceiveStreams();
  ASSERT_EQ(1U, flexfec_streams.size());
  const FakeFlexfecReceiveStream* flexfec_stream = flexfec_streams.front();
  const webrtc::FlexfecReceiveStream::Config& flexfec_stream_config =
      flexfec_stream->GetConfig();
  EXPECT_EQ(GetEngineCodec("flexfec-03").id,
            flexfec_stream_config.payload_type);
  EXPECT_EQ(kFlexfecSsrc, flexfec_stream_config.remote_ssrc);
  ASSERT_EQ(1U, flexfec_stream_config.protected_media_ssrcs.size());
  EXPECT_EQ(kSsrcs1[0], flexfec_stream_config.protected_media_ssrcs[0]);
  const std::vector<FakeVideoReceiveStream*>& video_streams =
      fake_call_->GetVideoReceiveStreams();
  const FakeVideoReceiveStream* video_stream = video_streams.front();
  const webrtc::VideoReceiveStream::Config& video_stream_config =
      video_stream->GetConfig();
  EXPECT_EQ(video_stream_config.rtp.local_ssrc,
            flexfec_stream_config.local_ssrc);
  EXPECT_EQ(video_stream_config.rtp.rtcp_mode, flexfec_stream_config.rtcp_mode);
  EXPECT_EQ(video_stream_config.rtcp_send_transport,
            flexfec_stream_config.rtcp_send_transport);
  // TODO(brandtr): Update this EXPECT when we set |transport_cc| in a
  // spec-compliant way.
  EXPECT_EQ(video_stream_config.rtp.transport_cc,
            flexfec_stream_config.transport_cc);
  EXPECT_EQ(video_stream_config.rtp.rtcp_mode, flexfec_stream_config.rtcp_mode);
  EXPECT_EQ(video_stream_config.rtp.extensions,
            flexfec_stream_config.rtp_header_extensions);
}

// We should not send FlexFEC, even if we advertise it, unless the right
// field trial is set.
// TODO(brandtr): Remove when FlexFEC is enabled by default.
TEST_F(WebRtcVideoChannelFlexfecRecvTest,
       SetSendCodecsWithoutSsrcWithFecDoesNotEnableFec) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("flexfec-03"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(-1, config.rtp.flexfec.payload_type);
  EXPECT_EQ(0, config.rtp.flexfec.ssrc);
  EXPECT_TRUE(config.rtp.flexfec.protected_media_ssrcs.empty());
}

TEST_F(WebRtcVideoChannelFlexfecRecvTest,
       SetSendCodecsWithSsrcWithFecDoesNotEnableFec) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("flexfec-03"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  FakeVideoSendStream* stream = AddSendStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(-1, config.rtp.flexfec.payload_type);
  EXPECT_EQ(0, config.rtp.flexfec.ssrc);
  EXPECT_TRUE(config.rtp.flexfec.protected_media_ssrcs.empty());
}

TEST_F(WebRtcVideoChannelTest,
       SetSendCodecRejectsRtxWithoutAssociatedPayloadType) {
  const int kUnusedPayloadType = 127;
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kUnusedPayloadType));

  cricket::VideoSendParameters parameters;
  cricket::VideoCodec rtx_codec(kUnusedPayloadType, "rtx");
  parameters.codecs.push_back(rtx_codec);
  EXPECT_FALSE(channel_->SetSendParameters(parameters))
      << "RTX codec without associated payload type should be rejected.";
}

TEST_F(WebRtcVideoChannelTest,
       SetSendCodecRejectsRtxWithoutMatchingVideoCodec) {
  const int kUnusedPayloadType1 = 126;
  const int kUnusedPayloadType2 = 127;
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kUnusedPayloadType1));
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kUnusedPayloadType2));
  {
    cricket::VideoCodec rtx_codec = cricket::VideoCodec::CreateRtxCodec(
        kUnusedPayloadType1, GetEngineCodec("VP8").id);
    cricket::VideoSendParameters parameters;
    parameters.codecs.push_back(GetEngineCodec("VP8"));
    parameters.codecs.push_back(rtx_codec);
    ASSERT_TRUE(channel_->SetSendParameters(parameters));
  }
  {
    cricket::VideoCodec rtx_codec = cricket::VideoCodec::CreateRtxCodec(
        kUnusedPayloadType1, kUnusedPayloadType2);
    cricket::VideoSendParameters parameters;
    parameters.codecs.push_back(GetEngineCodec("VP8"));
    parameters.codecs.push_back(rtx_codec);
    EXPECT_FALSE(channel_->SetSendParameters(parameters))
        << "RTX without matching video codec should be rejected.";
  }
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsWithChangedRtxPayloadType) {
  const int kUnusedPayloadType1 = 126;
  const int kUnusedPayloadType2 = 127;
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kUnusedPayloadType1));
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kUnusedPayloadType2));

  // SSRCs for RTX.
  cricket::StreamParams params =
      cricket::StreamParams::CreateLegacy(kSsrcs1[0]);
  params.AddFidSsrc(kSsrcs1[0], kRtxSsrcs1[0]);
  AddSendStream(params);

  // Original payload type for RTX.
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  cricket::VideoCodec rtx_codec(kUnusedPayloadType1, "rtx");
  rtx_codec.SetParam("apt", GetEngineCodec("VP8").id);
  parameters.codecs.push_back(rtx_codec);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  ASSERT_EQ(1U, fake_call_->GetVideoSendStreams().size());
  const webrtc::VideoSendStream::Config& config_before =
      fake_call_->GetVideoSendStreams()[0]->GetConfig();
  EXPECT_EQ(kUnusedPayloadType1, config_before.rtp.rtx.payload_type);
  ASSERT_EQ(1U, config_before.rtp.rtx.ssrcs.size());
  EXPECT_EQ(kRtxSsrcs1[0], config_before.rtp.rtx.ssrcs[0]);

  // Change payload type for RTX.
  parameters.codecs[1].id = kUnusedPayloadType2;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  ASSERT_EQ(1U, fake_call_->GetVideoSendStreams().size());
  const webrtc::VideoSendStream::Config& config_after =
      fake_call_->GetVideoSendStreams()[0]->GetConfig();
  EXPECT_EQ(kUnusedPayloadType2, config_after.rtp.rtx.payload_type);
  ASSERT_EQ(1U, config_after.rtp.rtx.ssrcs.size());
  EXPECT_EQ(kRtxSsrcs1[0], config_after.rtp.rtx.ssrcs[0]);
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsWithoutFecDisablesFec) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("ulpfec"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(GetEngineCodec("ulpfec").id, config.rtp.ulpfec.ulpfec_payload_type);

  parameters.codecs.pop_back();
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  stream = fake_call_->GetVideoSendStreams()[0];
  ASSERT_TRUE(stream != nullptr);
  config = stream->GetConfig().Copy();
  EXPECT_EQ(-1, config.rtp.ulpfec.ulpfec_payload_type)
      << "SetSendCodec without ULPFEC should disable current ULPFEC.";
}

TEST_F(WebRtcVideoChannelFlexfecSendRecvTest,
       SetSendCodecsWithoutFecDisablesFec) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("flexfec-03"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));

  FakeVideoSendStream* stream = AddSendStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));
  webrtc::VideoSendStream::Config config = stream->GetConfig().Copy();

  EXPECT_EQ(GetEngineCodec("flexfec-03").id, config.rtp.flexfec.payload_type);
  EXPECT_EQ(kFlexfecSsrc, config.rtp.flexfec.ssrc);
  ASSERT_EQ(1U, config.rtp.flexfec.protected_media_ssrcs.size());
  EXPECT_EQ(kSsrcs1[0], config.rtp.flexfec.protected_media_ssrcs[0]);

  parameters.codecs.pop_back();
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  stream = fake_call_->GetVideoSendStreams()[0];
  ASSERT_TRUE(stream != nullptr);
  config = stream->GetConfig().Copy();
  EXPECT_EQ(-1, config.rtp.flexfec.payload_type)
      << "SetSendCodec without FlexFEC should disable current FlexFEC.";
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsChangesExistingStreams) {
  cricket::VideoSendParameters parameters;
  cricket::VideoCodec codec(100, "VP8");
  codec.SetParam(kCodecParamMaxQuantization, kDefaultQpMax);
  parameters.codecs.push_back(codec);

  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  channel_->SetSend(true);

  FakeVideoSendStream* stream = AddSendStream();
  cricket::FakeVideoCapturer capturer;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, &capturer));

  std::vector<webrtc::VideoStream> streams = stream->GetVideoStreams();
  EXPECT_EQ(kDefaultQpMax, streams[0].max_qp);

  parameters.codecs.clear();
  codec.SetParam(kCodecParamMaxQuantization, kDefaultQpMax + 1);
  parameters.codecs.push_back(codec);
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  streams = fake_call_->GetVideoSendStreams()[0]->GetVideoStreams();
  EXPECT_EQ(kDefaultQpMax + 1, streams[0].max_qp);
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsWithBitrates) {
  SetSendCodecsShouldWorkForBitrates("100", 100000, "150", 150000, "200",
                                     200000);
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsWithHighMaxBitrate) {
  SetSendCodecsShouldWorkForBitrates("", 0, "", -1, "10000", 10000000);
  std::vector<webrtc::VideoStream> streams = AddSendStream()->GetVideoStreams();
  ASSERT_EQ(1u, streams.size());
  EXPECT_EQ(10000000, streams[0].max_bitrate_bps);
}

TEST_F(WebRtcVideoChannelTest,
       SetSendCodecsWithoutBitratesUsesCorrectDefaults) {
  SetSendCodecsShouldWorkForBitrates(
      "", 0, "", -1, "", -1);
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsCapsMinAndStartBitrate) {
  SetSendCodecsShouldWorkForBitrates("-1", 0, "-100", -1, "", -1);
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsRejectsMaxLessThanMinBitrate) {
  send_parameters_.codecs[0].params[kCodecParamMinBitrate] = "300";
  send_parameters_.codecs[0].params[kCodecParamMaxBitrate] = "200";
  EXPECT_FALSE(channel_->SetSendParameters(send_parameters_));
}

// Test that when both the codec-specific bitrate params and max_bandwidth_bps
// are present in the same send parameters, the settings are combined correctly.
TEST_F(WebRtcVideoChannelTest, SetSendCodecsWithBitratesAndMaxSendBandwidth) {
  send_parameters_.codecs[0].params[kCodecParamMinBitrate] = "100";
  send_parameters_.codecs[0].params[kCodecParamStartBitrate] = "200";
  send_parameters_.codecs[0].params[kCodecParamMaxBitrate] = "300";
  send_parameters_.max_bandwidth_bps = 400000;
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_EQ(100000, fake_call_->GetConfig().bitrate_config.min_bitrate_bps);
  EXPECT_EQ(200000, fake_call_->GetConfig().bitrate_config.start_bitrate_bps);
  // We expect max_bandwidth_bps to take priority, if set.
  EXPECT_EQ(400000, fake_call_->GetConfig().bitrate_config.max_bitrate_bps);

  // Decrease max_bandwidth_bps.
  send_parameters_.max_bandwidth_bps = 350000;
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_EQ(100000, fake_call_->GetConfig().bitrate_config.min_bitrate_bps);
  // Since the codec isn't changing, start_bitrate_bps should be -1.
  EXPECT_EQ(-1, fake_call_->GetConfig().bitrate_config.start_bitrate_bps);
  EXPECT_EQ(350000, fake_call_->GetConfig().bitrate_config.max_bitrate_bps);

  // Now try again with the values flipped around.
  send_parameters_.codecs[0].params[kCodecParamMaxBitrate] = "400";
  send_parameters_.max_bandwidth_bps = 300000;
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_EQ(100000, fake_call_->GetConfig().bitrate_config.min_bitrate_bps);
  EXPECT_EQ(200000, fake_call_->GetConfig().bitrate_config.start_bitrate_bps);
  EXPECT_EQ(300000, fake_call_->GetConfig().bitrate_config.max_bitrate_bps);

  // If we change the codec max, max_bandwidth_bps should still apply.
  send_parameters_.codecs[0].params[kCodecParamMaxBitrate] = "350";
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_EQ(100000, fake_call_->GetConfig().bitrate_config.min_bitrate_bps);
  EXPECT_EQ(200000, fake_call_->GetConfig().bitrate_config.start_bitrate_bps);
  EXPECT_EQ(300000, fake_call_->GetConfig().bitrate_config.max_bitrate_bps);
}

TEST_F(WebRtcVideoChannelTest,
       SetMaxSendBandwidthShouldPreserveOtherBitrates) {
  SetSendCodecsShouldWorkForBitrates("100", 100000, "150", 150000, "200",
                                     200000);
  send_parameters_.max_bandwidth_bps = 300000;
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_EQ(100000, fake_call_->GetConfig().bitrate_config.min_bitrate_bps)
      << "Setting max bitrate should keep previous min bitrate.";
  EXPECT_EQ(-1, fake_call_->GetConfig().bitrate_config.start_bitrate_bps)
      << "Setting max bitrate should not reset start bitrate.";
  EXPECT_EQ(300000, fake_call_->GetConfig().bitrate_config.max_bitrate_bps);
}

TEST_F(WebRtcVideoChannelTest, SetMaxSendBandwidthShouldBeRemovable) {
  send_parameters_.max_bandwidth_bps = 300000;
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_EQ(300000, fake_call_->GetConfig().bitrate_config.max_bitrate_bps);
  // -1 means to disable max bitrate (set infinite).
  send_parameters_.max_bandwidth_bps = -1;
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_EQ(-1, fake_call_->GetConfig().bitrate_config.max_bitrate_bps)
      << "Setting zero max bitrate did not reset start bitrate.";
}

TEST_F(WebRtcVideoChannelTest, SetMaxSendBandwidthAndAddSendStream) {
  send_parameters_.max_bandwidth_bps = 99999;
  FakeVideoSendStream* stream = AddSendStream();
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_EQ(send_parameters_.max_bandwidth_bps,
            fake_call_->GetConfig().bitrate_config.max_bitrate_bps);
  ASSERT_EQ(1u, stream->GetVideoStreams().size());
  EXPECT_EQ(send_parameters_.max_bandwidth_bps,
            stream->GetVideoStreams()[0].max_bitrate_bps);

  send_parameters_.max_bandwidth_bps = 77777;
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_EQ(send_parameters_.max_bandwidth_bps,
            fake_call_->GetConfig().bitrate_config.max_bitrate_bps);
  EXPECT_EQ(send_parameters_.max_bandwidth_bps,
            stream->GetVideoStreams()[0].max_bitrate_bps);
}

TEST_F(WebRtcVideoChannelTest, SetMaxSendBitrateCanIncreaseSenderBitrate) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  channel_->SetSend(true);

  FakeVideoSendStream* stream = AddSendStream();

  cricket::FakeVideoCapturer capturer;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));

  std::vector<webrtc::VideoStream> streams = stream->GetVideoStreams();
  int initial_max_bitrate_bps = streams[0].max_bitrate_bps;
  EXPECT_GT(initial_max_bitrate_bps, 0);

  parameters.max_bandwidth_bps = initial_max_bitrate_bps * 2;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  // Insert a frame to update the encoder config.
  EXPECT_TRUE(capturer.CaptureFrame());
  streams = stream->GetVideoStreams();
  EXPECT_EQ(initial_max_bitrate_bps * 2, streams[0].max_bitrate_bps);
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest,
       SetMaxSendBitrateCanIncreaseSimulcastSenderBitrate) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetSendParameters(parameters));
  channel_->SetSend(true);

  FakeVideoSendStream* stream = AddSendStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs3)));

  // Send a frame to make sure this scales up to >1 stream (simulcast).
  cricket::FakeVideoCapturer capturer;
  EXPECT_TRUE(channel_->SetVideoSend(kSsrcs3[0], true, nullptr, &capturer));
  EXPECT_EQ(cricket::CS_RUNNING,
            capturer.Start(capturer.GetSupportedFormats()->front()));
  EXPECT_TRUE(capturer.CaptureFrame());

  std::vector<webrtc::VideoStream> streams = stream->GetVideoStreams();
  ASSERT_GT(streams.size(), 1u)
      << "Without simulcast this test doesn't make sense.";
  int initial_max_bitrate_bps = GetTotalMaxBitrateBps(streams);
  EXPECT_GT(initial_max_bitrate_bps, 0);

  parameters.max_bandwidth_bps = initial_max_bitrate_bps * 2;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  // Insert a frame to update the encoder config.
  EXPECT_TRUE(capturer.CaptureFrame());
  streams = stream->GetVideoStreams();
  int increased_max_bitrate_bps = GetTotalMaxBitrateBps(streams);
  EXPECT_EQ(initial_max_bitrate_bps * 2, increased_max_bitrate_bps);

  EXPECT_TRUE(channel_->SetVideoSend(kSsrcs3[0], true, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsWithMaxQuantization) {
  static const char* kMaxQuantization = "21";
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs[0].params[kCodecParamMaxQuantization] = kMaxQuantization;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(static_cast<unsigned int>(atoi(kMaxQuantization)),
            AddSendStream()->GetVideoStreams().back().max_qp);

  VideoCodec codec;
  EXPECT_TRUE(channel_->GetSendCodec(&codec));
  EXPECT_EQ(kMaxQuantization, codec.params[kCodecParamMaxQuantization]);
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsRejectBadPayloadTypes) {
  // TODO(pbos): Should we only allow the dynamic range?
  static const int kIncorrectPayloads[] = {-2, -1, 128, 129};
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  for (size_t i = 0; i < arraysize(kIncorrectPayloads); ++i) {
    parameters.codecs[0].id = kIncorrectPayloads[i];
    EXPECT_FALSE(channel_->SetSendParameters(parameters))
        << "Bad payload type '" << kIncorrectPayloads[i] << "' accepted.";
  }
}

TEST_F(WebRtcVideoChannelTest, SetSendCodecsAcceptAllValidPayloadTypes) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  for (int payload_type = 96; payload_type <= 127; ++payload_type) {
    parameters.codecs[0].id = payload_type;
    EXPECT_TRUE(channel_->SetSendParameters(parameters))
        << "Payload type '" << payload_type << "' rejected.";
  }
}

// Test that setting the a different set of codecs but with an identical front
// codec doesn't result in the stream being recreated.
// This may happen when a subsequent negotiation includes fewer codecs, as a
// result of one of the codecs being rejected.
TEST_F(WebRtcVideoChannelTest,
       SetSendCodecsIdenticalFirstCodecDoesntRecreateStream) {
  cricket::VideoSendParameters parameters1;
  parameters1.codecs.push_back(GetEngineCodec("VP8"));
  parameters1.codecs.push_back(GetEngineCodec("VP9"));
  EXPECT_TRUE(channel_->SetSendParameters(parameters1));

  AddSendStream();
  EXPECT_EQ(1, fake_call_->GetNumCreatedSendStreams());

  cricket::VideoSendParameters parameters2;
  parameters2.codecs.push_back(GetEngineCodec("VP8"));
  EXPECT_TRUE(channel_->SetSendParameters(parameters2));
  EXPECT_EQ(1, fake_call_->GetNumCreatedSendStreams());
}

TEST_F(WebRtcVideoChannelTest, SetRecvCodecsWithOnlyVp8) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
}

// Test that we set our inbound RTX codecs properly.
TEST_F(WebRtcVideoChannelTest, SetRecvCodecsWithRtx) {
  const int kUnusedPayloadType1 = 126;
  const int kUnusedPayloadType2 = 127;
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kUnusedPayloadType1));
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kUnusedPayloadType2));

  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  cricket::VideoCodec rtx_codec(kUnusedPayloadType1, "rtx");
  parameters.codecs.push_back(rtx_codec);
  EXPECT_FALSE(channel_->SetRecvParameters(parameters))
      << "RTX codec without associated payload should be rejected.";

  parameters.codecs[1].SetParam("apt", kUnusedPayloadType2);
  EXPECT_FALSE(channel_->SetRecvParameters(parameters))
      << "RTX codec with invalid associated payload type should be rejected.";

  parameters.codecs[1].SetParam("apt", GetEngineCodec("VP8").id);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  cricket::VideoCodec rtx_codec2(kUnusedPayloadType2, "rtx");
  rtx_codec2.SetParam("apt", rtx_codec.id);
  parameters.codecs.push_back(rtx_codec2);

  EXPECT_FALSE(channel_->SetRecvParameters(parameters)) <<
      "RTX codec with another RTX as associated payload type should be "
      "rejected.";
}

TEST_F(WebRtcVideoChannelTest, SetRecvCodecsWithChangedRtxPayloadType) {
  const int kUnusedPayloadType1 = 126;
  const int kUnusedPayloadType2 = 127;
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kUnusedPayloadType1));
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kUnusedPayloadType2));

  // SSRCs for RTX.
  cricket::StreamParams params =
      cricket::StreamParams::CreateLegacy(kSsrcs1[0]);
  params.AddFidSsrc(kSsrcs1[0], kRtxSsrcs1[0]);
  AddRecvStream(params);

  // Original payload type for RTX.
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  cricket::VideoCodec rtx_codec(kUnusedPayloadType1, "rtx");
  rtx_codec.SetParam("apt", GetEngineCodec("VP8").id);
  parameters.codecs.push_back(rtx_codec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  ASSERT_EQ(1U, fake_call_->GetVideoReceiveStreams().size());
  const webrtc::VideoReceiveStream::Config& config_before =
      fake_call_->GetVideoReceiveStreams()[0]->GetConfig();
  EXPECT_EQ(1U, config_before.rtp.rtx_associated_payload_types.size());
  const int* payload_type_before = FindKeyByValue(
      config_before.rtp.rtx_associated_payload_types, GetEngineCodec("VP8").id);
  ASSERT_NE(payload_type_before, nullptr);
  EXPECT_EQ(kUnusedPayloadType1, *payload_type_before);
  EXPECT_EQ(kRtxSsrcs1[0], config_before.rtp.rtx_ssrc);

  // Change payload type for RTX.
  parameters.codecs[1].id = kUnusedPayloadType2;
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  ASSERT_EQ(1U, fake_call_->GetVideoReceiveStreams().size());
  const webrtc::VideoReceiveStream::Config& config_after =
      fake_call_->GetVideoReceiveStreams()[0]->GetConfig();
  EXPECT_EQ(1U, config_after.rtp.rtx_associated_payload_types.size());
  const int* payload_type_after = FindKeyByValue(
      config_after.rtp.rtx_associated_payload_types, GetEngineCodec("VP8").id);
  ASSERT_NE(payload_type_after, nullptr);
  EXPECT_EQ(kUnusedPayloadType2, *payload_type_after);
  EXPECT_EQ(kRtxSsrcs1[0], config_after.rtp.rtx_ssrc);
}

TEST_F(WebRtcVideoChannelTest, SetRecvCodecsDifferentPayloadType) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs[0].id = 99;
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
}

TEST_F(WebRtcVideoChannelTest, SetRecvCodecsAcceptDefaultCodecs) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs = engine_.codecs();
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  FakeVideoReceiveStream* stream = AddRecvStream();
  const webrtc::VideoReceiveStream::Config& config = stream->GetConfig();
  EXPECT_EQ(engine_.codecs()[0].name, config.decoders[0].payload_name);
  EXPECT_EQ(engine_.codecs()[0].id, config.decoders[0].payload_type);
}

TEST_F(WebRtcVideoChannelTest, SetRecvCodecsRejectUnsupportedCodec) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(VideoCodec(101, "WTF3"));
  EXPECT_FALSE(channel_->SetRecvParameters(parameters));
}

TEST_F(WebRtcVideoChannelTest, SetRecvCodecsAcceptsMultipleVideoCodecs) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
}

TEST_F(WebRtcVideoChannelTest, SetRecvCodecsWithoutFecDisablesFec) {
  cricket::VideoSendParameters send_parameters;
  send_parameters.codecs.push_back(GetEngineCodec("VP8"));
  send_parameters.codecs.push_back(GetEngineCodec("red"));
  send_parameters.codecs.push_back(GetEngineCodec("ulpfec"));
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters));

  FakeVideoReceiveStream* stream = AddRecvStream();

  EXPECT_EQ(GetEngineCodec("ulpfec").id,
            stream->GetConfig().rtp.ulpfec_payload_type);

  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  ASSERT_TRUE(stream != nullptr);
  EXPECT_EQ(-1, stream->GetConfig().rtp.ulpfec_payload_type)
      << "SetSendCodec without ULPFEC should disable current ULPFEC.";
}

TEST_F(WebRtcVideoChannelFlexfecRecvTest, SetRecvParamsWithoutFecDisablesFec) {
  AddRecvStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));
  const std::vector<FakeFlexfecReceiveStream*>& streams =
      fake_call_->GetFlexfecReceiveStreams();

  ASSERT_EQ(1U, streams.size());
  const FakeFlexfecReceiveStream* stream = streams.front();
  EXPECT_EQ(GetEngineCodec("flexfec-03").id, stream->GetConfig().payload_type);
  EXPECT_EQ(kFlexfecSsrc, stream->GetConfig().remote_ssrc);
  ASSERT_EQ(1U, stream->GetConfig().protected_media_ssrcs.size());
  EXPECT_EQ(kSsrcs1[0], stream->GetConfig().protected_media_ssrcs[0]);

  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));
  EXPECT_TRUE(streams.empty())
      << "SetSendCodec without FlexFEC should disable current FlexFEC.";
}

TEST_F(WebRtcVideoChannelTest, SetSendParamsWithFecEnablesFec) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  EXPECT_EQ(GetEngineCodec("ulpfec").id,
            stream->GetConfig().rtp.ulpfec_payload_type);

  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(GetEngineCodec("VP8"));
  recv_parameters.codecs.push_back(GetEngineCodec("red"));
  recv_parameters.codecs.push_back(GetEngineCodec("ulpfec"));
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  ASSERT_TRUE(stream != nullptr);
  EXPECT_EQ(GetEngineCodec("ulpfec").id,
            stream->GetConfig().rtp.ulpfec_payload_type)
      << "ULPFEC should be enabled on the receive stream.";

  cricket::VideoSendParameters send_parameters;
  send_parameters.codecs.push_back(GetEngineCodec("VP8"));
  send_parameters.codecs.push_back(GetEngineCodec("red"));
  send_parameters.codecs.push_back(GetEngineCodec("ulpfec"));
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters));
  stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(GetEngineCodec("ulpfec").id,
            stream->GetConfig().rtp.ulpfec_payload_type)
      << "ULPFEC should be enabled on the receive stream.";
}

TEST_F(WebRtcVideoChannelFlexfecSendRecvTest,
       SetSendRecvParamsWithFecEnablesFec) {
  AddRecvStream(
      CreatePrimaryWithFecFrStreamParams("cname", kSsrcs1[0], kFlexfecSsrc));
  const std::vector<FakeFlexfecReceiveStream*>& streams =
      fake_call_->GetFlexfecReceiveStreams();

  cricket::VideoRecvParameters recv_parameters;
  recv_parameters.codecs.push_back(GetEngineCodec("VP8"));
  recv_parameters.codecs.push_back(GetEngineCodec("flexfec-03"));
  ASSERT_TRUE(channel_->SetRecvParameters(recv_parameters));
  ASSERT_EQ(1U, streams.size());
  const FakeFlexfecReceiveStream* stream_with_recv_params = streams.front();
  EXPECT_EQ(GetEngineCodec("flexfec-03").id,
            stream_with_recv_params->GetConfig().payload_type);
  EXPECT_EQ(kFlexfecSsrc, stream_with_recv_params->GetConfig().remote_ssrc);
  EXPECT_EQ(1U,
            stream_with_recv_params->GetConfig().protected_media_ssrcs.size());
  EXPECT_EQ(kSsrcs1[0],
            stream_with_recv_params->GetConfig().protected_media_ssrcs[0]);

  cricket::VideoSendParameters send_parameters;
  send_parameters.codecs.push_back(GetEngineCodec("VP8"));
  send_parameters.codecs.push_back(GetEngineCodec("flexfec-03"));
  ASSERT_TRUE(channel_->SetSendParameters(send_parameters));
  ASSERT_EQ(1U, streams.size());
  const FakeFlexfecReceiveStream* stream_with_send_params = streams.front();
  EXPECT_EQ(GetEngineCodec("flexfec-03").id,
            stream_with_send_params->GetConfig().payload_type);
  EXPECT_EQ(kFlexfecSsrc, stream_with_send_params->GetConfig().remote_ssrc);
  EXPECT_EQ(1U,
            stream_with_send_params->GetConfig().protected_media_ssrcs.size());
  EXPECT_EQ(kSsrcs1[0],
            stream_with_send_params->GetConfig().protected_media_ssrcs[0]);
}

TEST_F(WebRtcVideoChannelTest, SetRecvCodecsRejectDuplicateFecPayloads) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("red"));
  parameters.codecs[1].id = parameters.codecs[0].id;
  EXPECT_FALSE(channel_->SetRecvParameters(parameters));
}

TEST_F(WebRtcVideoChannelFlexfecRecvTest,
       SetRecvCodecsRejectDuplicateFecPayloads) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("flexfec-03"));
  parameters.codecs[1].id = parameters.codecs[0].id;
  EXPECT_FALSE(channel_->SetRecvParameters(parameters));
}

TEST_F(WebRtcVideoChannelTest, SetRecvCodecsRejectDuplicateCodecPayloads) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  parameters.codecs[1].id = parameters.codecs[0].id;
  EXPECT_FALSE(channel_->SetRecvParameters(parameters));
}

TEST_F(WebRtcVideoChannelTest,
       SetRecvCodecsAcceptSameCodecOnMultiplePayloadTypes) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs[1].id += 1;
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
}

// Test that setting the same codecs but with a different order
// doesn't result in the stream being recreated.
TEST_F(WebRtcVideoChannelTest,
       SetRecvCodecsDifferentOrderDoesntRecreateStream) {
  cricket::VideoRecvParameters parameters1;
  parameters1.codecs.push_back(GetEngineCodec("VP8"));
  parameters1.codecs.push_back(GetEngineCodec("red"));
  EXPECT_TRUE(channel_->SetRecvParameters(parameters1));

  AddRecvStream(cricket::StreamParams::CreateLegacy(123));
  EXPECT_EQ(1, fake_call_->GetNumCreatedReceiveStreams());

  cricket::VideoRecvParameters parameters2;
  parameters2.codecs.push_back(GetEngineCodec("red"));
  parameters2.codecs.push_back(GetEngineCodec("VP8"));
  EXPECT_TRUE(channel_->SetRecvParameters(parameters2));
  EXPECT_EQ(1, fake_call_->GetNumCreatedReceiveStreams());
}

TEST_F(WebRtcVideoChannelTest, SendStreamNotSendingByDefault) {
  EXPECT_FALSE(AddSendStream()->IsSending());
}

TEST_F(WebRtcVideoChannelTest, ReceiveStreamReceivingByDefault) {
  EXPECT_TRUE(AddRecvStream()->IsReceiving());
}

TEST_F(WebRtcVideoChannelTest, SetSend) {
  FakeVideoSendStream* stream = AddSendStream();
  EXPECT_FALSE(stream->IsSending());

  // false->true
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(stream->IsSending());
  // true->true
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(stream->IsSending());
  // true->false
  EXPECT_TRUE(channel_->SetSend(false));
  EXPECT_FALSE(stream->IsSending());
  // false->false
  EXPECT_TRUE(channel_->SetSend(false));
  EXPECT_FALSE(stream->IsSending());

  EXPECT_TRUE(channel_->SetSend(true));
  FakeVideoSendStream* new_stream = AddSendStream();
  EXPECT_TRUE(new_stream->IsSending())
      << "Send stream created after SetSend(true) not sending initially.";
}

// This test verifies DSCP settings are properly applied on video media channel.
TEST_F(WebRtcVideoChannelTest, TestSetDscpOptions) {
  std::unique_ptr<cricket::FakeNetworkInterface> network_interface(
      new cricket::FakeNetworkInterface);
  MediaConfig config;
  std::unique_ptr<VideoMediaChannel> channel;

  channel.reset(engine_.CreateChannel(call_.get(), config, VideoOptions()));
  channel->SetInterface(network_interface.get());
  // Default value when DSCP is disabled should be DSCP_DEFAULT.
  EXPECT_EQ(rtc::DSCP_DEFAULT, network_interface->dscp());

  config.enable_dscp = true;
  channel.reset(engine_.CreateChannel(call_.get(), config, VideoOptions()));
  channel->SetInterface(network_interface.get());
  EXPECT_EQ(rtc::DSCP_AF41, network_interface->dscp());

  // Verify that setting the option to false resets the
  // DiffServCodePoint.
  config.enable_dscp = false;
  channel.reset(engine_.CreateChannel(call_.get(), config, VideoOptions()));
  channel->SetInterface(network_interface.get());
  EXPECT_EQ(rtc::DSCP_DEFAULT, network_interface->dscp());
}

// This test verifies that the RTCP reduced size mode is properly applied to
// send video streams.
TEST_F(WebRtcVideoChannelTest, TestSetSendRtcpReducedSize) {
  // Create stream, expecting that default mode is "compound".
  FakeVideoSendStream* stream1 = AddSendStream();
  EXPECT_EQ(webrtc::RtcpMode::kCompound, stream1->GetConfig().rtp.rtcp_mode);

  // Now enable reduced size mode.
  send_parameters_.rtcp.reduced_size = true;
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  stream1 = fake_call_->GetVideoSendStreams()[0];
  EXPECT_EQ(webrtc::RtcpMode::kReducedSize, stream1->GetConfig().rtp.rtcp_mode);

  // Create a new stream and ensure it picks up the reduced size mode.
  FakeVideoSendStream* stream2 = AddSendStream();
  EXPECT_EQ(webrtc::RtcpMode::kReducedSize, stream2->GetConfig().rtp.rtcp_mode);
}

// This test verifies that the RTCP reduced size mode is properly applied to
// receive video streams.
TEST_F(WebRtcVideoChannelTest, TestSetRecvRtcpReducedSize) {
  // Create stream, expecting that default mode is "compound".
  FakeVideoReceiveStream* stream1 = AddRecvStream();
  EXPECT_EQ(webrtc::RtcpMode::kCompound, stream1->GetConfig().rtp.rtcp_mode);

  // Now enable reduced size mode.
  // TODO(deadbeef): Once "recv_parameters" becomes "receiver_parameters",
  // the reduced_size flag should come from that.
  send_parameters_.rtcp.reduced_size = true;
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  stream1 = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(webrtc::RtcpMode::kReducedSize, stream1->GetConfig().rtp.rtcp_mode);

  // Create a new stream and ensure it picks up the reduced size mode.
  FakeVideoReceiveStream* stream2 = AddRecvStream();
  EXPECT_EQ(webrtc::RtcpMode::kReducedSize, stream2->GetConfig().rtp.rtcp_mode);
}

TEST_F(WebRtcVideoChannelTest, OnReadyToSendSignalsNetworkState) {
  EXPECT_EQ(webrtc::kNetworkUp,
            fake_call_->GetNetworkState(webrtc::MediaType::VIDEO));
  EXPECT_EQ(webrtc::kNetworkUp,
            fake_call_->GetNetworkState(webrtc::MediaType::AUDIO));

  channel_->OnReadyToSend(false);
  EXPECT_EQ(webrtc::kNetworkDown,
            fake_call_->GetNetworkState(webrtc::MediaType::VIDEO));
  EXPECT_EQ(webrtc::kNetworkUp,
            fake_call_->GetNetworkState(webrtc::MediaType::AUDIO));

  channel_->OnReadyToSend(true);
  EXPECT_EQ(webrtc::kNetworkUp,
            fake_call_->GetNetworkState(webrtc::MediaType::VIDEO));
  EXPECT_EQ(webrtc::kNetworkUp,
            fake_call_->GetNetworkState(webrtc::MediaType::AUDIO));
}

TEST_F(WebRtcVideoChannelTest, GetStatsReportsSentCodecName) {
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  AddSendStream();

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ("VP8", info.senders[0].codec_name);
}

TEST_F(WebRtcVideoChannelTest, GetStatsReportsEncoderImplementationName) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.encoder_implementation_name = "encoder_implementation_name";
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.encoder_implementation_name,
            info.senders[0].encoder_implementation_name);
}

TEST_F(WebRtcVideoChannelTest, GetStatsReportsCpuOveruseMetrics) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.avg_encode_time_ms = 13;
  stats.encode_usage_percent = 42;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.avg_encode_time_ms, info.senders[0].avg_encode_ms);
  EXPECT_EQ(stats.encode_usage_percent, info.senders[0].encode_usage_percent);
}

TEST_F(WebRtcVideoChannelTest, GetStatsReportsFramesEncoded) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.frames_encoded = 13;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.frames_encoded, info.senders[0].frames_encoded);
}

TEST_F(WebRtcVideoChannelTest, GetStatsReportsQpSum) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.qp_sum = 13;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.qp_sum, info.senders[0].qp_sum);
}

TEST_F(WebRtcVideoChannelTest, GetStatsReportsUpperResolution) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.substreams[17].width = 123;
  stats.substreams[17].height = 40;
  stats.substreams[42].width = 80;
  stats.substreams[42].height = 31;
  stats.substreams[11].width = 20;
  stats.substreams[11].height = 90;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1u, info.senders.size());
  EXPECT_EQ(123, info.senders[0].send_frame_width);
  EXPECT_EQ(90, info.senders[0].send_frame_height);
}

TEST_F(WebRtcVideoChannelTest, GetStatsReportsPreferredBitrate) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.preferred_media_bitrate_bps = 5;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1u, info.senders.size());
  EXPECT_EQ(5, info.senders[0].preferred_bitrate);
}

TEST_F(WebRtcVideoChannelTest, GetStatsReportsCpuAdaptationStats) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.number_of_cpu_adapt_changes = 2;
  stats.cpu_limited_resolution = true;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(WebRtcVideoChannel::ADAPTREASON_CPU, info.senders[0].adapt_reason);
  EXPECT_EQ(stats.number_of_cpu_adapt_changes, info.senders[0].adapt_changes);
}

TEST_F(WebRtcVideoChannelTest, GetStatsReportsAdaptationAndBandwidthStats) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.number_of_cpu_adapt_changes = 2;
  stats.cpu_limited_resolution = true;
  stats.bw_limited_resolution = true;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(WebRtcVideoChannel::ADAPTREASON_CPU |
                WebRtcVideoChannel::ADAPTREASON_BANDWIDTH,
            info.senders[0].adapt_reason);
  EXPECT_EQ(stats.number_of_cpu_adapt_changes, info.senders[0].adapt_changes);
}

TEST_F(WebRtcVideoChannelTest,
       GetStatsTranslatesBandwidthLimitedResolutionCorrectly) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.bw_limited_resolution = true;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  EXPECT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(1U, info.senders.size());
  EXPECT_EQ(WebRtcVideoChannel::ADAPTREASON_BANDWIDTH,
            info.senders[0].adapt_reason);
}

TEST_F(WebRtcVideoChannelTest,
       GetStatsTranslatesSendRtcpPacketTypesCorrectly) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.substreams[17].rtcp_packet_type_counts.fir_packets = 2;
  stats.substreams[17].rtcp_packet_type_counts.nack_packets = 3;
  stats.substreams[17].rtcp_packet_type_counts.pli_packets = 4;

  stats.substreams[42].rtcp_packet_type_counts.fir_packets = 5;
  stats.substreams[42].rtcp_packet_type_counts.nack_packets = 7;
  stats.substreams[42].rtcp_packet_type_counts.pli_packets = 9;

  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(7, info.senders[0].firs_rcvd);
  EXPECT_EQ(10, info.senders[0].nacks_rcvd);
  EXPECT_EQ(13, info.senders[0].plis_rcvd);
}

TEST_F(WebRtcVideoChannelTest,
       GetStatsTranslatesReceiveRtcpPacketTypesCorrectly) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  webrtc::VideoReceiveStream::Stats stats;
  stats.rtcp_packet_type_counts.fir_packets = 2;
  stats.rtcp_packet_type_counts.nack_packets = 3;
  stats.rtcp_packet_type_counts.pli_packets = 4;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.rtcp_packet_type_counts.fir_packets,
            info.receivers[0].firs_sent);
  EXPECT_EQ(stats.rtcp_packet_type_counts.nack_packets,
            info.receivers[0].nacks_sent);
  EXPECT_EQ(stats.rtcp_packet_type_counts.pli_packets,
            info.receivers[0].plis_sent);
}

TEST_F(WebRtcVideoChannelTest, GetStatsTranslatesDecodeStatsCorrectly) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  webrtc::VideoReceiveStream::Stats stats;
  stats.decoder_implementation_name = "decoder_implementation_name";
  stats.decode_ms = 2;
  stats.max_decode_ms = 3;
  stats.current_delay_ms = 4;
  stats.target_delay_ms = 5;
  stats.jitter_buffer_ms = 6;
  stats.min_playout_delay_ms = 7;
  stats.render_delay_ms = 8;
  stats.width = 9;
  stats.height = 10;
  stats.frame_counts.key_frames = 11;
  stats.frame_counts.delta_frames = 12;
  stats.frames_rendered = 13;
  stats.frames_decoded = 14;
  stats.qp_sum = 15;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.decoder_implementation_name,
            info.receivers[0].decoder_implementation_name);
  EXPECT_EQ(stats.decode_ms, info.receivers[0].decode_ms);
  EXPECT_EQ(stats.max_decode_ms, info.receivers[0].max_decode_ms);
  EXPECT_EQ(stats.current_delay_ms, info.receivers[0].current_delay_ms);
  EXPECT_EQ(stats.target_delay_ms, info.receivers[0].target_delay_ms);
  EXPECT_EQ(stats.jitter_buffer_ms, info.receivers[0].jitter_buffer_ms);
  EXPECT_EQ(stats.min_playout_delay_ms, info.receivers[0].min_playout_delay_ms);
  EXPECT_EQ(stats.render_delay_ms, info.receivers[0].render_delay_ms);
  EXPECT_EQ(stats.width, info.receivers[0].frame_width);
  EXPECT_EQ(stats.height, info.receivers[0].frame_height);
  EXPECT_EQ(stats.frame_counts.key_frames + stats.frame_counts.delta_frames,
            info.receivers[0].frames_received);
  EXPECT_EQ(stats.frames_rendered, info.receivers[0].frames_rendered);
  EXPECT_EQ(stats.frames_decoded, info.receivers[0].frames_decoded);
  EXPECT_EQ(stats.qp_sum, info.receivers[0].qp_sum);
}

TEST_F(WebRtcVideoChannelTest, GetStatsTranslatesReceivePacketStatsCorrectly) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  webrtc::VideoReceiveStream::Stats stats;
  stats.rtp_stats.transmitted.payload_bytes = 2;
  stats.rtp_stats.transmitted.header_bytes = 3;
  stats.rtp_stats.transmitted.padding_bytes = 4;
  stats.rtp_stats.transmitted.packets = 5;
  stats.rtcp_stats.packets_lost = 6;
  stats.rtcp_stats.fraction_lost = 7;
  stream->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_EQ(stats.rtp_stats.transmitted.payload_bytes +
                stats.rtp_stats.transmitted.header_bytes +
                stats.rtp_stats.transmitted.padding_bytes,
            info.receivers[0].bytes_rcvd);
  EXPECT_EQ(stats.rtp_stats.transmitted.packets,
            info.receivers[0].packets_rcvd);
  EXPECT_EQ(stats.rtcp_stats.packets_lost, info.receivers[0].packets_lost);
  EXPECT_EQ(static_cast<float>(stats.rtcp_stats.fraction_lost) / (1 << 8),
            info.receivers[0].fraction_lost);
}

TEST_F(WebRtcVideoChannelTest, TranslatesCallStatsCorrectly) {
  AddSendStream();
  AddSendStream();
  webrtc::Call::Stats stats;
  stats.rtt_ms = 123;
  fake_call_->SetStats(stats);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(2u, info.senders.size());
  EXPECT_EQ(stats.rtt_ms, info.senders[0].rtt_ms);
  EXPECT_EQ(stats.rtt_ms, info.senders[1].rtt_ms);
}

TEST_F(WebRtcVideoChannelTest, TranslatesSenderBitrateStatsCorrectly) {
  FakeVideoSendStream* stream = AddSendStream();
  webrtc::VideoSendStream::Stats stats;
  stats.target_media_bitrate_bps = 156;
  stats.media_bitrate_bps = 123;
  stats.substreams[17].total_bitrate_bps = 1;
  stats.substreams[17].retransmit_bitrate_bps = 2;
  stats.substreams[42].total_bitrate_bps = 3;
  stats.substreams[42].retransmit_bitrate_bps = 4;
  stream->SetStats(stats);

  FakeVideoSendStream* stream2 = AddSendStream();
  webrtc::VideoSendStream::Stats stats2;
  stats2.target_media_bitrate_bps = 200;
  stats2.media_bitrate_bps = 321;
  stats2.substreams[13].total_bitrate_bps = 5;
  stats2.substreams[13].retransmit_bitrate_bps = 6;
  stats2.substreams[21].total_bitrate_bps = 7;
  stats2.substreams[21].retransmit_bitrate_bps = 8;
  stream2->SetStats(stats2);

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));
  ASSERT_EQ(2u, info.senders.size());
  BandwidthEstimationInfo bwe_info;
  channel_->FillBitrateInfo(&bwe_info);
  // Assuming stream and stream2 corresponds to senders[0] and [1] respectively
  // is OK as std::maps are sorted and AddSendStream() gives increasing SSRCs.
  EXPECT_EQ(stats.media_bitrate_bps, info.senders[0].nominal_bitrate);
  EXPECT_EQ(stats2.media_bitrate_bps, info.senders[1].nominal_bitrate);
  EXPECT_EQ(stats.target_media_bitrate_bps + stats2.target_media_bitrate_bps,
            bwe_info.target_enc_bitrate);
  EXPECT_EQ(stats.media_bitrate_bps + stats2.media_bitrate_bps,
            bwe_info.actual_enc_bitrate);
  EXPECT_EQ(1 + 3 + 5 + 7, bwe_info.transmit_bitrate)
      << "Bandwidth stats should take all streams into account.";
  EXPECT_EQ(2 + 4 + 6 + 8, bwe_info.retransmit_bitrate)
      << "Bandwidth stats should take all streams into account.";
}

TEST_F(WebRtcVideoChannelTest, DefaultReceiveStreamReconfiguresToUseRtx) {
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  const std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32_t> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);

  ASSERT_EQ(0u, fake_call_->GetVideoReceiveStreams().size());
  const size_t kDataLength = 12;
  uint8_t data[kDataLength];
  memset(data, 0, sizeof(data));
  rtc::SetBE32(&data[8], ssrcs[0]);
  rtc::CopyOnWriteBuffer packet(data, kDataLength);
  rtc::PacketTime packet_time;
  channel_->OnPacketReceived(&packet, packet_time);

  ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size())
      << "No default receive stream created.";
  FakeVideoReceiveStream* recv_stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(0u, recv_stream->GetConfig().rtp.rtx_ssrc)
      << "Default receive stream should not have configured RTX";

  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs)));
  ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size())
      << "AddRecvStream should have reconfigured, not added a new receiver.";
  recv_stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_FALSE(
      recv_stream->GetConfig().rtp.rtx_associated_payload_types.empty());
  EXPECT_TRUE(VerifyRtxReceiveAssociations(recv_stream->GetConfig()))
      << "RTX should be mapped for all decoders/payload types.";
  EXPECT_TRUE(HasRtxReceiveAssociation(recv_stream->GetConfig(),
                                          GetEngineCodec("red").id))
      << "RTX should be mapped also for the RED payload type";
  EXPECT_EQ(rtx_ssrcs[0], recv_stream->GetConfig().rtp.rtx_ssrc);
}

TEST_F(WebRtcVideoChannelTest, RejectsAddingStreamsWithMissingSsrcsForRtx) {
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  const std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32_t> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);

  StreamParams sp =
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs);
  sp.ssrcs = ssrcs;  // Without RTXs, this is the important part.

  EXPECT_FALSE(channel_->AddSendStream(sp));
  EXPECT_FALSE(channel_->AddRecvStream(sp));
}

TEST_F(WebRtcVideoChannelTest, RejectsAddingStreamsWithOverlappingRtxSsrcs) {
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  const std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs1);
  const std::vector<uint32_t> rtx_ssrcs = MAKE_VECTOR(kRtxSsrcs1);

  StreamParams sp =
      cricket::CreateSimWithRtxStreamParams("cname", ssrcs, rtx_ssrcs);

  EXPECT_TRUE(channel_->AddSendStream(sp));
  EXPECT_TRUE(channel_->AddRecvStream(sp));

  // The RTX SSRC is already used in previous streams, using it should fail.
  sp = cricket::StreamParams::CreateLegacy(rtx_ssrcs[0]);
  EXPECT_FALSE(channel_->AddSendStream(sp));
  EXPECT_FALSE(channel_->AddRecvStream(sp));

  // After removing the original stream this should be fine to add (makes sure
  // that RTX ssrcs are not forever taken).
  EXPECT_TRUE(channel_->RemoveSendStream(ssrcs[0]));
  EXPECT_TRUE(channel_->RemoveRecvStream(ssrcs[0]));
  EXPECT_TRUE(channel_->AddSendStream(sp));
  EXPECT_TRUE(channel_->AddRecvStream(sp));
}

TEST_F(WebRtcVideoChannelTest,
       RejectsAddingStreamsWithOverlappingSimulcastSsrcs) {
  static const uint32_t kFirstStreamSsrcs[] = {1, 2, 3};
  static const uint32_t kOverlappingStreamSsrcs[] = {4, 3, 5};
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  StreamParams sp =
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kFirstStreamSsrcs));

  EXPECT_TRUE(channel_->AddSendStream(sp));
  EXPECT_TRUE(channel_->AddRecvStream(sp));

  // One of the SSRCs is already used in previous streams, using it should fail.
  sp = cricket::CreateSimStreamParams("cname",
                                      MAKE_VECTOR(kOverlappingStreamSsrcs));
  EXPECT_FALSE(channel_->AddSendStream(sp));
  EXPECT_FALSE(channel_->AddRecvStream(sp));

  // After removing the original stream this should be fine to add (makes sure
  // that RTX ssrcs are not forever taken).
  EXPECT_TRUE(channel_->RemoveSendStream(kFirstStreamSsrcs[0]));
  EXPECT_TRUE(channel_->RemoveRecvStream(kFirstStreamSsrcs[0]));
  EXPECT_TRUE(channel_->AddSendStream(sp));
  EXPECT_TRUE(channel_->AddRecvStream(sp));
}

TEST_F(WebRtcVideoChannelTest, ReportsSsrcGroupsInStats) {
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  static const uint32_t kSenderSsrcs[] = {4, 7, 10};
  static const uint32_t kSenderRtxSsrcs[] = {5, 8, 11};

  StreamParams sender_sp = cricket::CreateSimWithRtxStreamParams(
      "cname", MAKE_VECTOR(kSenderSsrcs), MAKE_VECTOR(kSenderRtxSsrcs));

  EXPECT_TRUE(channel_->AddSendStream(sender_sp));

  static const uint32_t kReceiverSsrcs[] = {3};
  static const uint32_t kReceiverRtxSsrcs[] = {2};

  StreamParams receiver_sp = cricket::CreateSimWithRtxStreamParams(
      "cname", MAKE_VECTOR(kReceiverSsrcs), MAKE_VECTOR(kReceiverRtxSsrcs));
  EXPECT_TRUE(channel_->AddRecvStream(receiver_sp));

  cricket::VideoMediaInfo info;
  ASSERT_TRUE(channel_->GetStats(&info));

  ASSERT_EQ(1u, info.senders.size());
  ASSERT_EQ(1u, info.receivers.size());

  EXPECT_NE(sender_sp.ssrc_groups, receiver_sp.ssrc_groups);
  EXPECT_EQ(sender_sp.ssrc_groups, info.senders[0].ssrc_groups);
  EXPECT_EQ(receiver_sp.ssrc_groups, info.receivers[0].ssrc_groups);
}

TEST_F(WebRtcVideoChannelTest, MapsReceivedPayloadTypeToCodecName) {
  FakeVideoReceiveStream* stream = AddRecvStream();
  webrtc::VideoReceiveStream::Stats stats;
  cricket::VideoMediaInfo info;

  // Report no codec name before receiving.
  stream->SetStats(stats);
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_STREQ("", info.receivers[0].codec_name.c_str());

  // Report VP8 if we're receiving it.
  stats.current_payload_type = GetEngineCodec("VP8").id;
  stream->SetStats(stats);
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_STREQ(kVp8CodecName, info.receivers[0].codec_name.c_str());

  // Report no codec name for unknown playload types.
  stats.current_payload_type = 3;
  stream->SetStats(stats);
  ASSERT_TRUE(channel_->GetStats(&info));
  EXPECT_STREQ("", info.receivers[0].codec_name.c_str());
}

void WebRtcVideoChannelTest::TestReceiveUnsignaledSsrcPacket(
    uint8_t payload_type,
    bool expect_created_receive_stream) {
  // kRedRtxPayloadType must currently be unused.
  EXPECT_FALSE(FindCodecById(engine_.codecs(), kRedRtxPayloadType));

  // Add a RED RTX codec.
  VideoCodec red_rtx_codec =
      VideoCodec::CreateRtxCodec(kRedRtxPayloadType, GetEngineCodec("red").id);
  recv_parameters_.codecs.push_back(red_rtx_codec);
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));

  ASSERT_EQ(0u, fake_call_->GetVideoReceiveStreams().size());
  const size_t kDataLength = 12;
  uint8_t data[kDataLength];
  memset(data, 0, sizeof(data));

  rtc::Set8(data, 1, payload_type);
  rtc::SetBE32(&data[8], kIncomingUnsignalledSsrc);
  rtc::CopyOnWriteBuffer packet(data, kDataLength);
  rtc::PacketTime packet_time;
  channel_->OnPacketReceived(&packet, packet_time);

  if (expect_created_receive_stream) {
    EXPECT_EQ(1u, fake_call_->GetVideoReceiveStreams().size())
        << "Should have created a receive stream for payload type: "
        << payload_type;
  } else {
    EXPECT_EQ(0u, fake_call_->GetVideoReceiveStreams().size())
        << "Shouldn't have created a receive stream for payload type: "
        << payload_type;
  }
}

TEST_F(WebRtcVideoChannelTest, Vp8PacketCreatesUnsignalledStream) {
  TestReceiveUnsignaledSsrcPacket(GetEngineCodec("VP8").id,
                                  true /* expect_created_receive_stream */);
}

TEST_F(WebRtcVideoChannelTest, Vp9PacketCreatesUnsignalledStream) {
  TestReceiveUnsignaledSsrcPacket(GetEngineCodec("VP9").id,
                                  true /* expect_created_receive_stream */);
}

TEST_F(WebRtcVideoChannelTest, RtxPacketDoesntCreateUnsignalledStream) {
  const cricket::VideoCodec vp8 = GetEngineCodec("VP8");
  const int rtx_vp8_payload_type = default_apt_rtx_types_[vp8.id];
  TestReceiveUnsignaledSsrcPacket(rtx_vp8_payload_type,
                                  false /* expect_created_receive_stream */);
}

TEST_F(WebRtcVideoChannelTest, UlpfecPacketDoesntCreateUnsignalledStream) {
  TestReceiveUnsignaledSsrcPacket(GetEngineCodec("ulpfec").id,
                                  false /* expect_created_receive_stream */);
}

TEST_F(WebRtcVideoChannelFlexfecRecvTest,
       FlexfecPacketDoesntCreateUnsignalledStream) {
  TestReceiveUnsignaledSsrcPacket(GetEngineCodec("flexfec-03").id,
                                  false /* expect_created_receive_stream */);
}

TEST_F(WebRtcVideoChannelTest, RedRtxPacketDoesntCreateUnsignalledStream) {
  TestReceiveUnsignaledSsrcPacket(kRedRtxPayloadType,
                                  false /* expect_created_receive_stream */);
}

// Test that receiving any unsignalled SSRC works even if it changes.
// The first unsignalled SSRC received will create a default receive stream.
// Any different unsignalled SSRC received will replace the default.
TEST_F(WebRtcVideoChannelTest, ReceiveDifferentUnsignaledSsrc) {
  // Allow receiving VP8, VP9, H264 (if enabled).
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP9"));

#if defined(WEBRTC_USE_H264)
  cricket::VideoCodec H264codec(126, "H264");
  parameters.codecs.push_back(H264codec);
#endif

  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  // No receive streams yet.
  ASSERT_EQ(0u, fake_call_->GetVideoReceiveStreams().size());
  cricket::FakeVideoRenderer renderer;
  EXPECT_TRUE(channel_->SetSink(kDefaultRecvSsrc, &renderer));

  // Receive VP8 packet on first SSRC.
  uint8_t data[kMinRtpPacketLen];
  cricket::RtpHeader rtpHeader;
  rtpHeader.payload_type = GetEngineCodec("VP8").id;
  rtpHeader.seq_num = rtpHeader.timestamp = 0;
  rtpHeader.ssrc = kIncomingUnsignalledSsrc+1;
  cricket::SetRtpHeader(data, sizeof(data), rtpHeader);
  rtc::CopyOnWriteBuffer packet(data, sizeof(data));
  rtc::PacketTime packet_time;
  channel_->OnPacketReceived(&packet, packet_time);
  // VP8 packet should create default receive stream.
  ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
  FakeVideoReceiveStream* recv_stream =
    fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(rtpHeader.ssrc, recv_stream->GetConfig().rtp.remote_ssrc);
  // Verify that the receive stream sinks to a renderer.
  webrtc::VideoFrame video_frame(CreateBlackFrameBuffer(4, 4), 100, 0,
                                 webrtc::kVideoRotation_0);
  recv_stream->InjectFrame(video_frame);
  EXPECT_EQ(1, renderer.num_rendered_frames());

  // Receive VP9 packet on second SSRC.
  rtpHeader.payload_type = GetEngineCodec("VP9").id;
  rtpHeader.ssrc = kIncomingUnsignalledSsrc+2;
  cricket::SetRtpHeader(data, sizeof(data), rtpHeader);
  rtc::CopyOnWriteBuffer packet2(data, sizeof(data));
  channel_->OnPacketReceived(&packet2, packet_time);
  // VP9 packet should replace the default receive SSRC.
  ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
  recv_stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(rtpHeader.ssrc, recv_stream->GetConfig().rtp.remote_ssrc);
  // Verify that the receive stream sinks to a renderer.
  webrtc::VideoFrame video_frame2(CreateBlackFrameBuffer(4, 4), 200, 0,
                                  webrtc::kVideoRotation_0);
  recv_stream->InjectFrame(video_frame2);
  EXPECT_EQ(2, renderer.num_rendered_frames());

#if defined(WEBRTC_USE_H264)
  // Receive H264 packet on third SSRC.
  rtpHeader.payload_type = 126;
  rtpHeader.ssrc = kIncomingUnsignalledSsrc+3;
  cricket::SetRtpHeader(data, sizeof(data), rtpHeader);
  rtc::CopyOnWriteBuffer packet3(data, sizeof(data));
  channel_->OnPacketReceived(&packet3, packet_time);
  // H264 packet should replace the default receive SSRC.
  ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
  recv_stream = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(rtpHeader.ssrc, recv_stream->GetConfig().rtp.remote_ssrc);
  // Verify that the receive stream sinks to a renderer.
  webrtc::VideoFrame video_frame3(CreateBlackFrameBuffer(4, 4), 300, 0,
                                  webrtc::kVideoRotation_0);
  recv_stream->InjectFrame(video_frame3);
  EXPECT_EQ(3, renderer.num_rendered_frames());
#endif
}

// This test verifies that when a new default stream is created for a new
// unsignaled SSRC, the new stream does not overwrite any old stream that had
// been the default receive stream before being properly signaled.
TEST_F(WebRtcVideoChannelTest,
       NewUnsignaledStreamDoesNotDestroyPreviouslyUnsignaledStream) {
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  ASSERT_TRUE(channel_->SetRecvParameters(parameters));

  // No streams signaled and no packets received, so we should not have any
  // stream objects created yet.
  EXPECT_EQ(0u, fake_call_->GetVideoReceiveStreams().size());

  // Receive packet on an unsignaled SSRC.
  uint8_t data[kMinRtpPacketLen];
  cricket::RtpHeader rtp_header;
  rtp_header.payload_type = GetEngineCodec("VP8").id;
  rtp_header.seq_num = rtp_header.timestamp = 0;
  rtp_header.ssrc = kSsrcs3[0];
  cricket::SetRtpHeader(data, sizeof(data), rtp_header);
  rtc::CopyOnWriteBuffer packet(data, sizeof(data));
  rtc::PacketTime packet_time;
  channel_->OnPacketReceived(&packet, packet_time);
  // Default receive stream should be created.
  ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
  FakeVideoReceiveStream* recv_stream0 =
      fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(kSsrcs3[0], recv_stream0->GetConfig().rtp.remote_ssrc);

  // Signal the SSRC.
  EXPECT_TRUE(
      channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(kSsrcs3[0])));
  ASSERT_EQ(1u, fake_call_->GetVideoReceiveStreams().size());
  recv_stream0 = fake_call_->GetVideoReceiveStreams()[0];
  EXPECT_EQ(kSsrcs3[0], recv_stream0->GetConfig().rtp.remote_ssrc);

  // Receive packet on a different unsignaled SSRC.
  rtp_header.ssrc = kSsrcs3[1];
  cricket::SetRtpHeader(data, sizeof(data), rtp_header);
  packet.SetData(data, sizeof(data));
  channel_->OnPacketReceived(&packet, packet_time);
  // New default receive stream should be created, but old stream should remain.
  ASSERT_EQ(2u, fake_call_->GetVideoReceiveStreams().size());
  EXPECT_EQ(recv_stream0, fake_call_->GetVideoReceiveStreams()[0]);
  FakeVideoReceiveStream* recv_stream1 =
      fake_call_->GetVideoReceiveStreams()[1];
  EXPECT_EQ(kSsrcs3[1], recv_stream1->GetConfig().rtp.remote_ssrc);
}

TEST_F(WebRtcVideoChannelTest, CanSentMaxBitrateForExistingStream) {
  AddSendStream();

  cricket::FakeVideoCapturer capturer;
  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, &capturer));
  cricket::VideoFormat capture_format_hd =
      capturer.GetSupportedFormats()->front();
  EXPECT_EQ(1280, capture_format_hd.width);
  EXPECT_EQ(720, capture_format_hd.height);
  EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(capture_format_hd));
  EXPECT_TRUE(channel_->SetSend(true));
  capturer.CaptureFrame();

  int default_encoder_bitrate = GetMaxEncoderBitrate();
  EXPECT_GT(default_encoder_bitrate, 1000);

  // TODO(skvlad): Resolve the inconsistency between the interpretation
  // of the global bitrate limit for audio and video:
  // - Audio: max_bandwidth_bps = 0 - fail the operation,
  //          max_bandwidth_bps = -1 - remove the bandwidth limit
  // - Video: max_bandwidth_bps = 0 - remove the bandwidth limit,
  //          max_bandwidth_bps = -1 - remove the bandwidth limit

  SetAndExpectMaxBitrate(1000, 0, 1000);
  SetAndExpectMaxBitrate(1000, 800, 800);
  SetAndExpectMaxBitrate(600, 800, 600);
  SetAndExpectMaxBitrate(0, 800, 800);
  SetAndExpectMaxBitrate(0, 0, default_encoder_bitrate);

  EXPECT_TRUE(channel_->SetVideoSend(last_ssrc_, true, nullptr, nullptr));
}

TEST_F(WebRtcVideoChannelTest, CannotSetMaxBitrateForNonexistentStream) {
  webrtc::RtpParameters nonexistent_parameters =
      channel_->GetRtpSendParameters(last_ssrc_);
  EXPECT_EQ(0, nonexistent_parameters.encodings.size());

  nonexistent_parameters.encodings.push_back(webrtc::RtpEncodingParameters());
  EXPECT_FALSE(
      channel_->SetRtpSendParameters(last_ssrc_, nonexistent_parameters));
}

TEST_F(WebRtcVideoChannelTest,
       CannotSetRtpSendParametersWithIncorrectNumberOfEncodings) {
  // This test verifies that setting RtpParameters succeeds only if
  // the structure contains exactly one encoding.
  // TODO(skvlad): Update this test when we start supporting setting parameters
  // for each encoding individually.

  AddSendStream();
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  // Two or more encodings should result in failure.
  parameters.encodings.push_back(webrtc::RtpEncodingParameters());
  EXPECT_FALSE(channel_->SetRtpSendParameters(last_ssrc_, parameters));
  // Zero encodings should also fail.
  parameters.encodings.clear();
  EXPECT_FALSE(channel_->SetRtpSendParameters(last_ssrc_, parameters));
}

// Changing the SSRC through RtpParameters is not allowed.
TEST_F(WebRtcVideoChannelTest, CannotSetSsrcInRtpSendParameters) {
  AddSendStream();
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  parameters.encodings[0].ssrc = 0xdeadbeef;
  EXPECT_FALSE(channel_->SetRtpSendParameters(last_ssrc_, parameters));
}

// Test that a stream will not be sending if its encoding is made inactive
// through SetRtpSendParameters.
// TODO(deadbeef): Update this test when we start supporting setting parameters
// for each encoding individually.
TEST_F(WebRtcVideoChannelTest, SetRtpSendParametersEncodingsActive) {
  FakeVideoSendStream* stream = AddSendStream();
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(stream->IsSending());

  // Get current parameters and change "active" to false.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  ASSERT_EQ(1u, parameters.encodings.size());
  ASSERT_TRUE(parameters.encodings[0].active);
  parameters.encodings[0].active = false;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters));
  EXPECT_FALSE(stream->IsSending());

  // Now change it back to active and verify we resume sending.
  parameters.encodings[0].active = true;
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters));
  EXPECT_TRUE(stream->IsSending());
}

// Test that if a stream is reconfigured (due to a codec change or other
// change) while its encoding is still inactive, it doesn't start sending.
TEST_F(WebRtcVideoChannelTest,
       InactiveStreamDoesntStartSendingWhenReconfigured) {
  // Set an initial codec list, which will be modified later.
  cricket::VideoSendParameters parameters1;
  parameters1.codecs.push_back(GetEngineCodec("VP8"));
  parameters1.codecs.push_back(GetEngineCodec("VP9"));
  EXPECT_TRUE(channel_->SetSendParameters(parameters1));

  FakeVideoSendStream* stream = AddSendStream();
  EXPECT_TRUE(channel_->SetSend(true));
  EXPECT_TRUE(stream->IsSending());

  // Get current parameters and change "active" to false.
  webrtc::RtpParameters parameters = channel_->GetRtpSendParameters(last_ssrc_);
  ASSERT_EQ(1u, parameters.encodings.size());
  ASSERT_TRUE(parameters.encodings[0].active);
  parameters.encodings[0].active = false;
  EXPECT_EQ(1u, GetFakeSendStreams().size());
  EXPECT_EQ(1, fake_call_->GetNumCreatedSendStreams());
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, parameters));
  EXPECT_FALSE(stream->IsSending());

  // Reorder the codec list, causing the stream to be reconfigured.
  cricket::VideoSendParameters parameters2;
  parameters2.codecs.push_back(GetEngineCodec("VP9"));
  parameters2.codecs.push_back(GetEngineCodec("VP8"));
  EXPECT_TRUE(channel_->SetSendParameters(parameters2));
  auto new_streams = GetFakeSendStreams();
  // Assert that a new underlying stream was created due to the codec change.
  // Otherwise, this test isn't testing what it set out to test.
  EXPECT_EQ(1u, GetFakeSendStreams().size());
  EXPECT_EQ(2, fake_call_->GetNumCreatedSendStreams());

  // Verify that we still are not sending anything, due to the inactive
  // encoding.
  EXPECT_FALSE(new_streams[0]->IsSending());
}

// Test that GetRtpSendParameters returns the currently configured codecs.
TEST_F(WebRtcVideoChannelTest, GetRtpSendParametersCodecs) {
  AddSendStream();
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  webrtc::RtpParameters rtp_parameters =
      channel_->GetRtpSendParameters(last_ssrc_);
  ASSERT_EQ(2u, rtp_parameters.codecs.size());
  EXPECT_EQ(GetEngineCodec("VP8").ToCodecParameters(),
            rtp_parameters.codecs[0]);
  EXPECT_EQ(GetEngineCodec("VP9").ToCodecParameters(),
            rtp_parameters.codecs[1]);
}

// Test that RtpParameters for send stream has one encoding and it has
// the correct SSRC.
TEST_F(WebRtcVideoChannelTest, GetRtpSendParametersSsrc) {
  AddSendStream();

  webrtc::RtpParameters rtp_parameters =
      channel_->GetRtpSendParameters(last_ssrc_);
  ASSERT_EQ(1u, rtp_parameters.encodings.size());
  EXPECT_EQ(last_ssrc_, rtp_parameters.encodings[0].ssrc);
}

// Test that if we set/get parameters multiple times, we get the same results.
TEST_F(WebRtcVideoChannelTest, SetAndGetRtpSendParameters) {
  AddSendStream();
  cricket::VideoSendParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  webrtc::RtpParameters initial_params =
      channel_->GetRtpSendParameters(last_ssrc_);

  // We should be able to set the params we just got.
  EXPECT_TRUE(channel_->SetRtpSendParameters(last_ssrc_, initial_params));

  // ... And this shouldn't change the params returned by GetRtpSendParameters.
  EXPECT_EQ(initial_params, channel_->GetRtpSendParameters(last_ssrc_));
}

// Test that GetRtpReceiveParameters returns the currently configured codecs.
TEST_F(WebRtcVideoChannelTest, GetRtpReceiveParametersCodecs) {
  AddRecvStream();
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  webrtc::RtpParameters rtp_parameters =
      channel_->GetRtpReceiveParameters(last_ssrc_);
  ASSERT_EQ(2u, rtp_parameters.codecs.size());
  EXPECT_EQ(GetEngineCodec("VP8").ToCodecParameters(),
            rtp_parameters.codecs[0]);
  EXPECT_EQ(GetEngineCodec("VP9").ToCodecParameters(),
            rtp_parameters.codecs[1]);
}

#if defined(WEBRTC_USE_H264)
TEST_F(WebRtcVideoChannelTest, GetRtpReceiveFmtpSprop) {
#else
TEST_F(WebRtcVideoChannelTest, DISABLED_GetRtpReceiveFmtpSprop) {
#endif
  cricket::VideoRecvParameters parameters;
  cricket::VideoCodec kH264sprop1(101, "H264");
  kH264sprop1.SetParam(kH264FmtpSpropParameterSets, "uvw");
  parameters.codecs.push_back(kH264sprop1);
  cricket::VideoCodec kH264sprop2(102, "H264");
  kH264sprop2.SetParam(kH264FmtpSpropParameterSets, "xyz");
  parameters.codecs.push_back(kH264sprop2);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  FakeVideoReceiveStream* recv_stream = AddRecvStream();
  const webrtc::VideoReceiveStream::Config& cfg = recv_stream->GetConfig();
  webrtc::RtpParameters rtp_parameters =
      channel_->GetRtpReceiveParameters(last_ssrc_);
  ASSERT_EQ(2u, rtp_parameters.codecs.size());
  EXPECT_EQ(kH264sprop1.ToCodecParameters(), rtp_parameters.codecs[0]);
  ASSERT_EQ(2u, cfg.decoders.size());
  EXPECT_EQ(101, cfg.decoders[0].payload_type);
  EXPECT_EQ("H264", cfg.decoders[0].payload_name);
  const auto it0 =
      cfg.decoders[0].codec_params.find(kH264FmtpSpropParameterSets);
  ASSERT_TRUE(it0 != cfg.decoders[0].codec_params.end());
  EXPECT_EQ("uvw", it0->second);

  EXPECT_EQ(102, cfg.decoders[1].payload_type);
  EXPECT_EQ("H264", cfg.decoders[1].payload_name);
  const auto it1 =
      cfg.decoders[1].codec_params.find(kH264FmtpSpropParameterSets);
  ASSERT_TRUE(it1 != cfg.decoders[1].codec_params.end());
  EXPECT_EQ("xyz", it1->second);
}

// Test that RtpParameters for receive stream has one encoding and it has
// the correct SSRC.
TEST_F(WebRtcVideoChannelTest, GetRtpReceiveParametersSsrc) {
  AddRecvStream();

  webrtc::RtpParameters rtp_parameters =
      channel_->GetRtpReceiveParameters(last_ssrc_);
  ASSERT_EQ(1u, rtp_parameters.encodings.size());
  EXPECT_EQ(last_ssrc_, rtp_parameters.encodings[0].ssrc);
}

// Test that if we set/get parameters multiple times, we get the same results.
TEST_F(WebRtcVideoChannelTest, SetAndGetRtpReceiveParameters) {
  AddRecvStream();
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  webrtc::RtpParameters initial_params =
      channel_->GetRtpReceiveParameters(last_ssrc_);

  // We should be able to set the params we just got.
  EXPECT_TRUE(channel_->SetRtpReceiveParameters(last_ssrc_, initial_params));

  // ... And this shouldn't change the params returned by
  // GetRtpReceiveParameters.
  EXPECT_EQ(initial_params, channel_->GetRtpReceiveParameters(last_ssrc_));
}

// Test that GetRtpReceiveParameters returns parameters correctly when SSRCs
// aren't signaled. It should always return an empty "RtpEncodingParameters",
// even after a packet is received and the unsignaled SSRC is known.
TEST_F(WebRtcVideoChannelTest, GetRtpReceiveParametersWithUnsignaledSsrc) {
  // Call necessary methods to configure receiving a default stream as
  // soon as it arrives.
  cricket::VideoRecvParameters parameters;
  parameters.codecs.push_back(GetEngineCodec("VP8"));
  parameters.codecs.push_back(GetEngineCodec("VP9"));
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  // Call GetRtpReceiveParameters before configured to receive an unsignaled
  // stream. Should return nothing.
  EXPECT_EQ(webrtc::RtpParameters(), channel_->GetRtpReceiveParameters(0));

  // Set a sink for an unsignaled stream.
  cricket::FakeVideoRenderer renderer;
  // Value of "0" means "unsignaled stream".
  EXPECT_TRUE(channel_->SetSink(0, &renderer));

  // Call GetRtpReceiveParameters before the SSRC is known. Value of "0"
  // in this method means "unsignaled stream".
  webrtc::RtpParameters rtp_parameters = channel_->GetRtpReceiveParameters(0);
  ASSERT_EQ(1u, rtp_parameters.encodings.size());
  EXPECT_FALSE(rtp_parameters.encodings[0].ssrc);

  // Receive VP8 packet.
  uint8_t data[kMinRtpPacketLen];
  cricket::RtpHeader rtpHeader;
  rtpHeader.payload_type = GetEngineCodec("VP8").id;
  rtpHeader.seq_num = rtpHeader.timestamp = 0;
  rtpHeader.ssrc = kIncomingUnsignalledSsrc;
  cricket::SetRtpHeader(data, sizeof(data), rtpHeader);
  rtc::CopyOnWriteBuffer packet(data, sizeof(data));
  rtc::PacketTime packet_time;
  channel_->OnPacketReceived(&packet, packet_time);

  // The |ssrc| member should still be unset.
  rtp_parameters = channel_->GetRtpReceiveParameters(0);
  ASSERT_EQ(1u, rtp_parameters.encodings.size());
  EXPECT_FALSE(rtp_parameters.encodings[0].ssrc);
}

void WebRtcVideoChannelTest::TestReceiverLocalSsrcConfiguration(
    bool receiver_first) {
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  const uint32_t kSenderSsrc = 0xC0FFEE;
  const uint32_t kSecondSenderSsrc = 0xBADCAFE;
  const uint32_t kReceiverSsrc = 0x4711;
  const uint32_t kExpectedDefaultReceiverSsrc = 1;

  if (receiver_first) {
    AddRecvStream(StreamParams::CreateLegacy(kReceiverSsrc));
    std::vector<FakeVideoReceiveStream*> receive_streams =
        fake_call_->GetVideoReceiveStreams();
    ASSERT_EQ(1u, receive_streams.size());
    // Default local SSRC when we have no sender.
    EXPECT_EQ(kExpectedDefaultReceiverSsrc,
              receive_streams[0]->GetConfig().rtp.local_ssrc);
  }
  AddSendStream(StreamParams::CreateLegacy(kSenderSsrc));
  if (!receiver_first)
    AddRecvStream(StreamParams::CreateLegacy(kReceiverSsrc));
  std::vector<FakeVideoReceiveStream*> receive_streams =
      fake_call_->GetVideoReceiveStreams();
  ASSERT_EQ(1u, receive_streams.size());
  EXPECT_EQ(kSenderSsrc, receive_streams[0]->GetConfig().rtp.local_ssrc);

  // Removing first sender should fall back to another (in this case the second)
  // local send stream's SSRC.
  AddSendStream(StreamParams::CreateLegacy(kSecondSenderSsrc));
  ASSERT_TRUE(channel_->RemoveSendStream(kSenderSsrc));
  receive_streams =
      fake_call_->GetVideoReceiveStreams();
  ASSERT_EQ(1u, receive_streams.size());
  EXPECT_EQ(kSecondSenderSsrc, receive_streams[0]->GetConfig().rtp.local_ssrc);

  // Removing the last sender should fall back to default local SSRC.
  ASSERT_TRUE(channel_->RemoveSendStream(kSecondSenderSsrc));
  receive_streams =
      fake_call_->GetVideoReceiveStreams();
  ASSERT_EQ(1u, receive_streams.size());
  EXPECT_EQ(kExpectedDefaultReceiverSsrc,
            receive_streams[0]->GetConfig().rtp.local_ssrc);
}

TEST_F(WebRtcVideoChannelTest, ConfiguresLocalSsrc) {
  TestReceiverLocalSsrcConfiguration(false);
}

TEST_F(WebRtcVideoChannelTest, ConfiguresLocalSsrcOnExistingReceivers) {
  TestReceiverLocalSsrcConfiguration(true);
}

class WebRtcVideoChannelSimulcastTest : public testing::Test {
 public:
  WebRtcVideoChannelSimulcastTest()
      : fake_call_(webrtc::Call::Config(&event_log_)),
        encoder_factory_(new cricket::FakeWebRtcVideoEncoderFactory),
        decoder_factory_(new cricket::FakeWebRtcVideoDecoderFactory),
        engine_(std::unique_ptr<cricket::WebRtcVideoEncoderFactory>(
                    encoder_factory_),
                std::unique_ptr<cricket::WebRtcVideoDecoderFactory>(
                    decoder_factory_)),
        last_ssrc_(0) {}

  void SetUp() override {
    channel_.reset(
        engine_.CreateChannel(&fake_call_, GetMediaConfig(), VideoOptions()));
    channel_->OnReadyToSend(true);
    last_ssrc_ = 123;
  }

 protected:
  void VerifySimulcastSettings(const VideoCodec& codec,
                               int capture_width,
                               int capture_height,
                               size_t num_configured_streams,
                               size_t expected_num_streams,
                               bool screenshare,
                               bool conference_mode) {
    cricket::VideoSendParameters parameters;
    parameters.codecs.push_back(codec);
    parameters.conference_mode = conference_mode;
    ASSERT_TRUE(channel_->SetSendParameters(parameters));

    std::vector<uint32_t> ssrcs = MAKE_VECTOR(kSsrcs3);
    RTC_DCHECK(num_configured_streams <= ssrcs.size());
    ssrcs.resize(num_configured_streams);

    AddSendStream(CreateSimStreamParams("cname", ssrcs));
    // Send a full-size frame to trigger a stream reconfiguration to use all
    // expected simulcast layers.
    cricket::FakeVideoCapturer capturer;
    VideoOptions options;
    if (screenshare)
      options.is_screencast = screenshare;
    EXPECT_TRUE(
        channel_->SetVideoSend(ssrcs.front(), true, &options, &capturer));
    // Fetch the latest stream since SetVideoSend() may recreate it if the
    // screen content setting is changed.
    FakeVideoSendStream* stream = fake_call_.GetVideoSendStreams().front();
    EXPECT_EQ(cricket::CS_RUNNING, capturer.Start(cricket::VideoFormat(
                                       capture_width, capture_height,
                                       cricket::VideoFormat::FpsToInterval(30),
                                       cricket::FOURCC_I420)));
    channel_->SetSend(true);
    EXPECT_TRUE(capturer.CaptureFrame());

    std::vector<webrtc::VideoStream> video_streams = stream->GetVideoStreams();
    ASSERT_EQ(expected_num_streams, video_streams.size());

    std::vector<webrtc::VideoStream> expected_streams;
    if (conference_mode) {
      expected_streams = GetSimulcastConfig(
          num_configured_streams, capture_width, capture_height, 0,
          kDefaultQpMax, kDefaultVideoMaxFramerate, screenshare);
      if (screenshare) {
        for (const webrtc::VideoStream& stream : expected_streams) {
          // Never scale screen content.
          EXPECT_EQ(stream.width, capture_width);
          EXPECT_EQ(stream.height, capture_height);
        }
      }
    } else {
      webrtc::VideoStream stream;
      stream.width = capture_width;
      stream.height = capture_height;
      stream.max_framerate = kDefaultVideoMaxFramerate;
      stream.min_bitrate_bps = cricket::kMinVideoBitrateBps;
      int max_bitrate_kbps;
      if (capture_width * capture_height <= 320 * 240) {
        max_bitrate_kbps = 600;
      } else if (capture_width * capture_height <= 640 * 480) {
        max_bitrate_kbps = 1700;
      } else if (capture_width * capture_height <= 960 * 540) {
        max_bitrate_kbps = 2000;
      } else {
        max_bitrate_kbps = 2500;
      }
      stream.target_bitrate_bps = stream.max_bitrate_bps =
          max_bitrate_kbps * 1000;
      stream.max_qp = kDefaultQpMax;
      expected_streams.push_back(stream);
    }

    ASSERT_EQ(expected_streams.size(), video_streams.size());

    size_t num_streams = video_streams.size();
    int total_max_bitrate_bps = 0;
    for (size_t i = 0; i < num_streams; ++i) {
      EXPECT_EQ(expected_streams[i].width, video_streams[i].width);
      EXPECT_EQ(expected_streams[i].height, video_streams[i].height);

      EXPECT_GT(video_streams[i].max_framerate, 0);
      EXPECT_EQ(expected_streams[i].max_framerate,
                video_streams[i].max_framerate);

      EXPECT_GT(video_streams[i].min_bitrate_bps, 0);
      EXPECT_EQ(expected_streams[i].min_bitrate_bps,
                video_streams[i].min_bitrate_bps);

      EXPECT_GT(video_streams[i].target_bitrate_bps, 0);
      EXPECT_EQ(expected_streams[i].target_bitrate_bps,
                video_streams[i].target_bitrate_bps);

      EXPECT_GT(video_streams[i].max_bitrate_bps, 0);
      EXPECT_EQ(expected_streams[i].max_bitrate_bps,
                video_streams[i].max_bitrate_bps);

      EXPECT_GT(video_streams[i].max_qp, 0);
      EXPECT_EQ(expected_streams[i].max_qp, video_streams[i].max_qp);

      EXPECT_EQ(!conference_mode,
                expected_streams[i].temporal_layer_thresholds_bps.empty());
      EXPECT_EQ(expected_streams[i].temporal_layer_thresholds_bps,
                video_streams[i].temporal_layer_thresholds_bps);

      if (i == num_streams - 1) {
        total_max_bitrate_bps += video_streams[i].max_bitrate_bps;
      } else {
        total_max_bitrate_bps += video_streams[i].target_bitrate_bps;
      }
    }

    EXPECT_TRUE(channel_->SetVideoSend(ssrcs.front(), true, nullptr, nullptr));
  }

  FakeVideoSendStream* AddSendStream() {
    return AddSendStream(StreamParams::CreateLegacy(last_ssrc_++));
  }

  FakeVideoSendStream* AddSendStream(const StreamParams& sp) {
    size_t num_streams =
        fake_call_.GetVideoSendStreams().size();
    EXPECT_TRUE(channel_->AddSendStream(sp));
    std::vector<FakeVideoSendStream*> streams =
        fake_call_.GetVideoSendStreams();
    EXPECT_EQ(num_streams + 1, streams.size());
    return streams[streams.size() - 1];
  }

  std::vector<FakeVideoSendStream*> GetFakeSendStreams() {
    return fake_call_.GetVideoSendStreams();
  }

  FakeVideoReceiveStream* AddRecvStream() {
    return AddRecvStream(StreamParams::CreateLegacy(last_ssrc_++));
  }

  FakeVideoReceiveStream* AddRecvStream(const StreamParams& sp) {
    size_t num_streams =
        fake_call_.GetVideoReceiveStreams().size();
    EXPECT_TRUE(channel_->AddRecvStream(sp));
    std::vector<FakeVideoReceiveStream*> streams =
        fake_call_.GetVideoReceiveStreams();
    EXPECT_EQ(num_streams + 1, streams.size());
    return streams[streams.size() - 1];
  }

  webrtc::RtcEventLogNullImpl event_log_;
  FakeCall fake_call_;
  cricket::FakeWebRtcVideoEncoderFactory* encoder_factory_;
  cricket::FakeWebRtcVideoDecoderFactory* decoder_factory_;
  WebRtcVideoEngine engine_;
  std::unique_ptr<VideoMediaChannel> channel_;
  uint32_t last_ssrc_;
};

TEST_F(WebRtcVideoChannelSimulcastTest, SetSendCodecsWith2SimulcastStreams) {
  VerifySimulcastSettings(cricket::VideoCodec("VP8"), 640, 360, 2, 2, false,
                          true);
}

TEST_F(WebRtcVideoChannelSimulcastTest, SetSendCodecsWith3SimulcastStreams) {
  VerifySimulcastSettings(cricket::VideoCodec("VP8"), 1280, 720, 3, 3, false,
                          true);
}

// Test that we normalize send codec format size in simulcast.
TEST_F(WebRtcVideoChannelSimulcastTest, SetSendCodecsWithOddSizeInSimulcast) {
  VerifySimulcastSettings(cricket::VideoCodec("VP8"), 541, 271, 2, 2, false,
                          true);
}

TEST_F(WebRtcVideoChannelSimulcastTest, SetSendCodecsForScreenshare) {
  VerifySimulcastSettings(cricket::VideoCodec("VP8"), 1280, 720, 3, 1, true,
                          false);
}

TEST_F(WebRtcVideoChannelSimulcastTest,
       SetSendCodecsForConferenceModeScreenshare) {
  VerifySimulcastSettings(cricket::VideoCodec("VP8"), 1280, 720, 3, 1, true,
                          true);
}

TEST_F(WebRtcVideoChannelSimulcastTest, SetSendCodecsForSimulcastScreenshare) {
  webrtc::test::ScopedFieldTrials override_field_trials_(
      "WebRTC-SimulcastScreenshare/Enabled/");
  VerifySimulcastSettings(cricket::VideoCodec("VP8"), 1280, 720, 3, 2, true,
                          true);
}

TEST_F(WebRtcVideoChannelSimulcastTest,
       NoSimulcastScreenshareWithoutConference) {
  webrtc::test::ScopedFieldTrials override_field_trials_(
      "WebRTC-SimulcastScreenshare/Enabled/");
  VerifySimulcastSettings(cricket::VideoCodec("VP8"), 1280, 720, 3, 1, true,
                          false);
}

}  // namespace cricket
