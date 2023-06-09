/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_codec.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/test/create_video_codec_tester.h"
#include "api/test/videocodec_test_stats.h"
#include "api/units/data_rate.h"
#include "api/units/frequency.h"
#include "api/video/i420_buffer.h"
#include "api/video/resolution.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "media/base/media_constants.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/svc/scalability_mode_util.h"
#if defined(WEBRTC_ANDROID)
#include "modules/video_coding/codecs/test/android_codec_factory_helper.h"
#endif
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {

namespace {
using ::testing::Combine;
using ::testing::Values;
using PacingMode = VideoCodecTester::PacingSettings::PacingMode;

struct VideoInfo {
  std::string name;
  Resolution resolution;
};

struct CodecInfo {
  std::string type;
  std::string encoder;
  std::string decoder;
};

struct LayerId {
  int spatial_idx;
  int temporal_idx;

  bool operator==(const LayerId& o) const {
    return spatial_idx == o.spatial_idx && temporal_idx == o.temporal_idx;
  }

  bool operator<(const LayerId& o) const {
    if (spatial_idx < o.spatial_idx)
      return true;
    if (temporal_idx < o.temporal_idx)
      return true;
    return false;
  }
};

struct EncodingSettings {
  ScalabilityMode scalability_mode;
  // Spatial layer resolution.
  std::map<int, Resolution> resolution;
  // Top temporal layer frame rate.
  Frequency framerate;
  // Bitrate of spatial and temporal layers.
  std::map<LayerId, DataRate> bitrate;
};

struct EncodingTestSettings {
  std::string name;
  int num_frames = 1;
  std::map<int, EncodingSettings> frame_settings;
};

struct DecodingTestSettings {
  std::string name;
};

struct EncodeDecodeTestParams {
  CodecInfo codec;
  VideoInfo video;
  VideoCodecTester::EncoderSettings encoder_settings;
  VideoCodecTester::DecoderSettings decoder_settings;
  EncodingTestSettings encoding_settings;
  DecodingTestSettings decoding_settings;
  struct Expectations {
    double min_apsnr_y;
  } test_expectations;
};

const EncodingSettings kQvga64Kbps30Fps = {
    .scalability_mode = ScalabilityMode::kL1T1,
    .resolution = {{0, {.width = 320, .height = 180}}},
    .framerate = Frequency::Hertz(30),
    .bitrate = {
        {{.spatial_idx = 0, .temporal_idx = 0}, DataRate::KilobitsPerSec(64)}}};

const EncodingTestSettings kConstantRateQvga64Kbps30Fps = {
    .name = "ConstantRateQvga64Kbps30Fps",
    .num_frames = 300,
    .frame_settings = {{/*frame_num=*/0, kQvga64Kbps30Fps}}};

const VideoInfo kFourPeople_1280x720_30 = {
    .name = "FourPeople_1280x720_30",
    .resolution = {.width = 1280, .height = 720}};

class TestRawVideoSource : public VideoCodecTester::RawVideoSource {
 public:
  static constexpr Frequency k90kHz = Frequency::Hertz(90000);

  TestRawVideoSource(std::unique_ptr<FrameReader> frame_reader,
                     const EncodingTestSettings& test_settings)
      : frame_reader_(std::move(frame_reader)),
        test_settings_(test_settings),
        frame_num_(0),
        timestamp_rtp_(0) {
    // Ensure settings for the first frame are provided.
    RTC_CHECK_GT(test_settings_.frame_settings.size(), 0u);
    RTC_CHECK_EQ(test_settings_.frame_settings.begin()->first, 0);
  }

  // Pulls next frame. Frame RTP timestamp is set accordingly to
  // `EncodingSettings::framerate`.
  absl::optional<VideoFrame> PullFrame() override {
    if (frame_num_ >= test_settings_.num_frames) {
      // End of stream.
      return absl::nullopt;
    }

    EncodingSettings frame_settings =
        std::prev(test_settings_.frame_settings.upper_bound(frame_num_))
            ->second;

    int pulled_frame;
    auto buffer = frame_reader_->PullFrame(
        &pulled_frame, frame_settings.resolution.rbegin()->second,
        {.num = 30, .den = static_cast<int>(frame_settings.framerate.hertz())});
    RTC_CHECK(buffer) << "Cannot pull frame " << frame_num_;

    auto frame = VideoFrame::Builder()
                     .set_video_frame_buffer(buffer)
                     .set_timestamp_rtp(timestamp_rtp_)
                     .build();

    pulled_frames_[timestamp_rtp_] = pulled_frame;
    timestamp_rtp_ += k90kHz / frame_settings.framerate;
    ++frame_num_;

    return frame;
  }

  // Reads frame specified by `timestamp_rtp`, scales it to `resolution` and
  // returns. Frame with the given `timestamp_rtp` is expected to be pulled
  // before.
  VideoFrame GetFrame(uint32_t timestamp_rtp, Resolution resolution) override {
    RTC_CHECK(pulled_frames_.find(timestamp_rtp) != pulled_frames_.end())
        << "Frame with RTP timestamp " << timestamp_rtp
        << " was not pulled before";
    auto buffer =
        frame_reader_->ReadFrame(pulled_frames_[timestamp_rtp], resolution);
    return VideoFrame::Builder()
        .set_video_frame_buffer(buffer)
        .set_timestamp_rtp(timestamp_rtp)
        .build();
  }

 protected:
  std::unique_ptr<FrameReader> frame_reader_;
  const EncodingTestSettings& test_settings_;
  int frame_num_;
  uint32_t timestamp_rtp_;
  std::map<uint32_t, int> pulled_frames_;
};

class TestEncoder : public VideoCodecTester::Encoder,
                    public EncodedImageCallback {
 public:
  TestEncoder(std::unique_ptr<VideoEncoder> encoder,
              const CodecInfo& codec_info,
              const std::map<int, EncodingSettings>& frame_settings)
      : encoder_(std::move(encoder)),
        codec_info_(codec_info),
        frame_settings_(frame_settings),
        frame_num_(0) {
    // Ensure settings for the first frame is provided.
    RTC_CHECK_GT(frame_settings_.size(), 0u);
    RTC_CHECK_EQ(frame_settings_.begin()->first, 0);

    encoder_->RegisterEncodeCompleteCallback(this);
  }

  void Encode(const VideoFrame& frame, EncodeCallback callback) override {
    callbacks_[frame.timestamp()] = std::move(callback);

    if (auto fs = frame_settings_.find(frame_num_);
        fs != frame_settings_.end()) {
      if (fs == frame_settings_.begin() ||
          ConfigChanged(fs->second, std::prev(fs)->second)) {
        Configure(fs->second);
      }
      if (fs == frame_settings_.begin() ||
          RateChanged(fs->second, std::prev(fs)->second)) {
        SetRates(fs->second);
      }
    }

    encoder_->Encode(frame, nullptr);
    ++frame_num_;
  }

  void Flush() override {
    // TODO(webrtc:14852): For codecs which buffer frames we need a to
    // flush them to get last frames. Add such functionality to VideoEncoder
    // API. On Android it will map directly to `MediaCodec.flush()`.
    encoder_->Release();
  }

 protected:
  Result OnEncodedImage(const EncodedImage& encoded_image,
                        const CodecSpecificInfo* codec_specific_info) override {
    auto cb = callbacks_.find(encoded_image.Timestamp());
    RTC_CHECK(cb != callbacks_.end());
    cb->second(encoded_image);

    callbacks_.erase(callbacks_.begin(), cb);
    return Result(Result::Error::OK);
  }

  void Configure(const EncodingSettings& es) {
    VideoCodec vc;
    const Resolution& resolution = es.resolution.rbegin()->second;
    vc.width = resolution.width;
    vc.height = resolution.height;
    const DataRate& bitrate = es.bitrate.rbegin()->second;
    vc.startBitrate = bitrate.kbps();
    vc.maxBitrate = bitrate.kbps();
    vc.minBitrate = 0;
    vc.maxFramerate = static_cast<uint32_t>(es.framerate.hertz());
    vc.active = true;
    vc.qpMax = 0;
    vc.numberOfSimulcastStreams = 0;
    vc.mode = webrtc::VideoCodecMode::kRealtimeVideo;
    vc.SetFrameDropEnabled(true);

    vc.codecType = PayloadStringToCodecType(codec_info_.type);
    if (vc.codecType == kVideoCodecVP8) {
      *(vc.VP8()) = VideoEncoder::GetDefaultVp8Settings();
    } else if (vc.codecType == kVideoCodecVP9) {
      *(vc.VP9()) = VideoEncoder::GetDefaultVp9Settings();
    } else if (vc.codecType == kVideoCodecH264) {
      *(vc.H264()) = VideoEncoder::GetDefaultH264Settings();
    }

    VideoEncoder::Settings ves(
        VideoEncoder::Capabilities(/*loss_notification=*/false),
        /*number_of_cores=*/1,
        /*max_payload_size=*/1440);

    int result = encoder_->InitEncode(&vc, ves);
    RTC_CHECK_EQ(result, WEBRTC_VIDEO_CODEC_OK);
  }

  void SetRates(const EncodingSettings& es) {
    VideoEncoder::RateControlParameters rc;
    int num_spatial_layers =
        ScalabilityModeToNumSpatialLayers(es.scalability_mode);
    int num_temporal_layers =
        ScalabilityModeToNumSpatialLayers(es.scalability_mode);
    for (int sidx = 0; sidx < num_spatial_layers; ++sidx) {
      for (int tidx = 0; tidx < num_temporal_layers; ++tidx) {
        LayerId layer_id = {.spatial_idx = sidx, .temporal_idx = tidx};
        RTC_CHECK(es.bitrate.find(layer_id) != es.bitrate.end())
            << "Bitrate for layer S=" << sidx << " T=" << tidx << " is not set";
        rc.bitrate.SetBitrate(sidx, tidx, es.bitrate.at(layer_id).bps());
      }
    }

    rc.framerate_fps = es.framerate.millihertz() / 1000.0;
    encoder_->SetRates(rc);
  }

  bool ConfigChanged(const EncodingSettings& es,
                     const EncodingSettings& prev_es) const {
    return es.scalability_mode != prev_es.scalability_mode ||
           es.resolution != prev_es.resolution;
  }

  bool RateChanged(const EncodingSettings& es,
                   const EncodingSettings& prev_es) const {
    return es.bitrate != prev_es.bitrate || es.framerate != prev_es.framerate;
  }

  std::unique_ptr<VideoEncoder> encoder_;
  const CodecInfo& codec_info_;
  const std::map<int, EncodingSettings>& frame_settings_;
  int frame_num_;
  std::map<uint32_t, EncodeCallback> callbacks_;
};

class TestDecoder : public VideoCodecTester::Decoder,
                    public DecodedImageCallback {
 public:
  TestDecoder(std::unique_ptr<VideoDecoder> decoder,
              const CodecInfo& codec_info)
      : decoder_(std::move(decoder)), codec_info_(codec_info), frame_num_(0) {
    decoder_->RegisterDecodeCompleteCallback(this);
  }

  void Decode(const EncodedImage& frame, DecodeCallback callback) override {
    callbacks_[frame.Timestamp()] = std::move(callback);

    if (frame_num_ == 0) {
      Configure();
    }

    decoder_->Decode(frame, /*missing_frames=*/false,
                     /*render_time_ms=*/0);
    ++frame_num_;
  }

  void Flush() override {
    // TODO(webrtc:14852): For codecs which buffer frames we need a to
    // flush them to get last frames. Add such functionality to VideoDecoder
    // API. On Android it will map directly to `MediaCodec.flush()`.
    decoder_->Release();
  }

 protected:
  void Configure() {
    VideoDecoder::Settings ds;
    ds.set_codec_type(PayloadStringToCodecType(codec_info_.type));
    ds.set_number_of_cores(1);
    ds.set_max_render_resolution({1280, 720});

    bool result = decoder_->Configure(ds);
    RTC_CHECK(result);
  }

  int Decoded(VideoFrame& decoded_frame) override {
    auto cb = callbacks_.find(decoded_frame.timestamp());
    RTC_CHECK(cb != callbacks_.end());
    cb->second(decoded_frame);

    callbacks_.erase(callbacks_.begin(), cb);
    return WEBRTC_VIDEO_CODEC_OK;
  }

  std::unique_ptr<VideoDecoder> decoder_;
  const CodecInfo& codec_info_;
  int frame_num_;
  std::map<uint32_t, DecodeCallback> callbacks_;
};

std::unique_ptr<VideoCodecTester::Encoder> CreateEncoder(
    const CodecInfo& codec_info,
    const std::map<int, EncodingSettings>& frame_settings) {
  std::unique_ptr<VideoEncoderFactory> factory;
  if (codec_info.encoder == "libvpx" || codec_info.encoder == "libaom" ||
      codec_info.encoder == "openh264") {
    factory = CreateBuiltinVideoEncoderFactory();
  } else if (codec_info.encoder == "mediacodec") {
#if defined(WEBRTC_ANDROID)
    InitializeAndroidObjects();
    factory = CreateAndroidEncoderFactory();
#endif
  }

  RTC_CHECK(factory);
  auto encoder = factory->CreateVideoEncoder(SdpVideoFormat(codec_info.type));
  return std::make_unique<TestEncoder>(std::move(encoder), codec_info,
                                       frame_settings);
}

std::unique_ptr<VideoCodecTester::Decoder> CreateDecoder(
    const CodecInfo& codec_info) {
  std::unique_ptr<VideoDecoderFactory> factory;
  if (codec_info.decoder == "libvpx" || codec_info.decoder == "dav1d" ||
      codec_info.decoder == "ffmpeg") {
    factory = CreateBuiltinVideoDecoderFactory();
  } else if (codec_info.decoder == "mediacodec") {
#if defined(WEBRTC_ANDROID)
    InitializeAndroidObjects();
    factory = CreateAndroidDecoderFactory();
#endif
  }

  RTC_CHECK(factory);
  auto decoder = factory->CreateVideoDecoder(SdpVideoFormat(codec_info.type));
  return std::make_unique<TestDecoder>(std::move(decoder), codec_info);
}

}  // namespace

class EncodeDecodeTest
    : public ::testing::TestWithParam<EncodeDecodeTestParams> {
 public:
  EncodeDecodeTest() : test_params_(GetParam()) {}

  void SetUp() override {
    std::unique_ptr<FrameReader> frame_reader =
        CreateYuvFrameReader(ResourcePath(test_params_.video.name, "yuv"),
                             test_params_.video.resolution,
                             YuvFrameReaderImpl::RepeatMode::kPingPong);
    video_source_ = std::make_unique<TestRawVideoSource>(
        std::move(frame_reader), test_params_.encoding_settings);

    encoder_ = CreateEncoder(test_params_.codec,
                             test_params_.encoding_settings.frame_settings);
    decoder_ = CreateDecoder(test_params_.codec);

    tester_ = CreateVideoCodecTester();
  }

  static std::string TestParametersToStr(
      const ::testing::TestParamInfo<EncodeDecodeTest::ParamType>& info) {
    return std::string(info.param.encoding_settings.name +
                       info.param.codec.type + info.param.codec.encoder +
                       info.param.codec.decoder + info.param.video.name);
  }

 protected:
  EncodeDecodeTestParams test_params_;
  std::unique_ptr<TestRawVideoSource> video_source_;
  std::unique_ptr<VideoCodecTester::Encoder> encoder_;
  std::unique_ptr<VideoCodecTester::Decoder> decoder_;
  std::unique_ptr<VideoCodecTester> tester_;
};

TEST_P(EncodeDecodeTest, DISABLED_TestEncodeDecode) {
  std::unique_ptr<VideoCodecStats> stats = tester_->RunEncodeDecodeTest(
      video_source_.get(), encoder_.get(), decoder_.get(),
      test_params_.encoder_settings, test_params_.decoder_settings);

  const auto& frame_settings = test_params_.encoding_settings.frame_settings;
  for (auto fs = frame_settings.begin(); fs != frame_settings.end(); ++fs) {
    int first_frame = fs->first;
    int last_frame = std::next(fs) != frame_settings.end()
                         ? std::next(fs)->first - 1
                         : test_params_.encoding_settings.num_frames - 1;
    VideoCodecStats::Filter slicer = {.first_frame = first_frame,
                                      .last_frame = last_frame};
    std::vector<VideoCodecStats::Frame> frames = stats->Slice(slicer);
    VideoCodecStats::Stream stream = stats->Aggregate(frames);
    EXPECT_GE(stream.psnr.y.GetAverage(),
              test_params_.test_expectations.min_apsnr_y);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ConstantRate,
    EncodeDecodeTest,
    ::testing::ValuesIn({
      EncodeDecodeTestParams({
          .codec = {.type = "VP8", .encoder = "libvpx", .decoder = "libvpx"},
          .video = kFourPeople_1280x720_30,
          .encoder_settings = {.pacing = {.mode = PacingMode::kNoPacing}},
          .decoder_settings = {.pacing = {.mode = PacingMode::kNoPacing}},
          .encoding_settings = kConstantRateQvga64Kbps30Fps,
          .test_expectations = {.min_apsnr_y = 30.0},
      })
#if defined(WEBRTC_ANDROID)
          ,
          EncodeDecodeTestParams({
              .codec = {.type = "VP8",
                        .encoder = "mediacodec",
                        .decoder = "mediacodec"},
              .video = kFourPeople_1280x720_30,
              .encoder_settings = {.pacing = {.mode = PacingMode::kRealTime}},
              .decoder_settings = {.pacing = {.mode = PacingMode::kRealTime}},
              .encoding_settings = kConstantRateQvga64Kbps30Fps,
              .test_expectations = {.min_apsnr_y = 30.0},
          })
#endif
    }),
    EncodeDecodeTest::TestParametersToStr);
}  // namespace test

}  // namespace webrtc
