/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/video_codec_tester.h"

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "api/test/mock_video_decoder.h"
#include "api/test/mock_video_decoder_factory.h"
#include "api/test/mock_video_encoder.h"
#include "api/test/mock_video_encoder_factory.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/svc/scalability_mode_util.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

namespace webrtc {
namespace test {

namespace {
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SizeIs;

using VideoCodecStats = VideoCodecTester::VideoCodecStats;
using VideoSourceSettings = VideoCodecTester::VideoSourceSettings;
using CodedVideoSource = VideoCodecTester::CodedVideoSource;
using EncodingSettings = VideoCodecTester::EncodingSettings;
using LayerSettings = EncodingSettings::LayerSettings;
using LayerId = VideoCodecTester::LayerId;
using DecoderSettings = VideoCodecTester::DecoderSettings;
using EncoderSettings = VideoCodecTester::EncoderSettings;
using PacingSettings = VideoCodecTester::PacingSettings;
using PacingMode = PacingSettings::PacingMode;
using Filter = VideoCodecStats::Filter;
using Frame = VideoCodecTester::VideoCodecStats::Frame;
using Stream = VideoCodecTester::VideoCodecStats::Stream;

constexpr int kWidth = 2;
constexpr int kHeight = 2;
const DataRate kTargetLayerBitrate = DataRate::BytesPerSec(100);
const Frequency kTargetFramerate = Frequency::Hertz(30);
constexpr Frequency k90kHz = Frequency::Hertz(90000);

rtc::scoped_refptr<I420Buffer> CreateYuvBuffer(uint8_t y = 0,
                                               uint8_t u = 0,
                                               uint8_t v = 0) {
  rtc::scoped_refptr<I420Buffer> buffer(I420Buffer::Create(2, 2));

  libyuv::I420Rect(buffer->MutableDataY(), buffer->StrideY(),
                   buffer->MutableDataU(), buffer->StrideU(),
                   buffer->MutableDataV(), buffer->StrideV(), 0, 0,
                   buffer->width(), buffer->height(), y, u, v);
  return buffer;
}

std::string CreateYuvFile(int width, int height, int num_frames) {
  std::string path = webrtc::test::TempFilename(webrtc::test::OutputPath(),
                                                "video_codec_tester_unittest");
  FILE* file = fopen(path.c_str(), "wb");
  for (int frame_num = 0; frame_num < num_frames; ++frame_num) {
    uint8_t y = (frame_num + 0) & 255;
    uint8_t u = (frame_num + 1) & 255;
    uint8_t v = (frame_num + 2) & 255;
    rtc::scoped_refptr<I420Buffer> buffer = CreateYuvBuffer(y, u, v);
    fwrite(buffer->DataY(), 1, width * height, file);
    int chroma_size_bytes = (width + 1) / 2 * (height + 1) / 2;
    fwrite(buffer->DataU(), 1, chroma_size_bytes, file);
    fwrite(buffer->DataV(), 1, chroma_size_bytes, file);
  }
  fclose(file);
  return path;
}

std::unique_ptr<VideoCodecStats> RunTest(std::vector<std::vector<Frame>> frames,
                                         ScalabilityMode scalability_mode) {
  int num_frames = static_cast<int>(frames.size());
  std::string source_yuv_path = CreateYuvFile(kWidth, kHeight, num_frames);
  VideoSourceSettings source_settings{
      .file_path = source_yuv_path,
      .resolution = {.width = kWidth, .height = kHeight},
      .framerate = kTargetFramerate};

  int num_encoded_frames = 0;
  EncodedImageCallback* encoded_frame_callback;
  NiceMock<MockVideoEncoderFactory> encoder_factory;
  ON_CALL(encoder_factory, CreateVideoEncoder)
      .WillByDefault([&](const SdpVideoFormat&) {
        auto encoder = std::make_unique<NiceMock<MockVideoEncoder>>();
        ON_CALL(*encoder, RegisterEncodeCompleteCallback)
            .WillByDefault([&](EncodedImageCallback* callback) {
              encoded_frame_callback = callback;
              return WEBRTC_VIDEO_CODEC_OK;
            });
        ON_CALL(*encoder, Encode)
            .WillByDefault([&](const VideoFrame& input_frame,
                               const std::vector<VideoFrameType>*) {
              for (const Frame& frame : frames[num_encoded_frames]) {
                EncodedImage encoded_frame;
                encoded_frame._encodedWidth = frame.width;
                encoded_frame._encodedHeight = frame.height;
                encoded_frame.SetFrameType(
                    frame.keyframe ? VideoFrameType::kVideoFrameKey
                                   : VideoFrameType::kVideoFrameDelta);
                encoded_frame.SetRtpTimestamp(input_frame.timestamp());
                encoded_frame.SetSpatialIndex(frame.layer_id.spatial_idx);
                encoded_frame.SetTemporalIndex(frame.layer_id.temporal_idx);
                encoded_frame.SetEncodedData(
                    EncodedImageBuffer::Create(frame.frame_size.bytes()));
                encoded_frame_callback->OnEncodedImage(
                    encoded_frame,
                    /*codec_specific_info=*/nullptr);
              }
              ++num_encoded_frames;
              return WEBRTC_VIDEO_CODEC_OK;
            });
        return encoder;
      });

  int num_decoded_frames = 0;
  DecodedImageCallback* decode_callback;
  NiceMock<MockVideoDecoderFactory> decoder_factory;
  ON_CALL(decoder_factory, CreateVideoDecoder)
      .WillByDefault([&](const SdpVideoFormat&) {
        auto decoder = std::make_unique<NiceMock<MockVideoDecoder>>();
        ON_CALL(*decoder, RegisterDecodeCompleteCallback)
            .WillByDefault([&](DecodedImageCallback* callback) {
              decode_callback = callback;
              return WEBRTC_VIDEO_CODEC_OK;
            });
        ON_CALL(*decoder, Decode(_, _))
            .WillByDefault([&](const EncodedImage& encoded_frame, int64_t) {
              // Make values to be different from source YUV generated in
              // `CreateYuvFile`.
              uint8_t y = ((num_decoded_frames + 1) * 2) & 255;
              uint8_t u = ((num_decoded_frames + 2) * 2) & 255;
              uint8_t v = ((num_decoded_frames + 3) * 2) & 255;
              rtc::scoped_refptr<I420Buffer> frame_buffer =
                  CreateYuvBuffer(y, u, v);
              VideoFrame decoded_frame =
                  VideoFrame::Builder()
                      .set_video_frame_buffer(frame_buffer)
                      .set_timestamp_rtp(encoded_frame.RtpTimestamp())
                      .build();
              decode_callback->Decoded(decoded_frame);
              ++num_decoded_frames;
              return WEBRTC_VIDEO_CODEC_OK;
            });
        return decoder;
      });

  int num_spatial_layers = ScalabilityModeToNumSpatialLayers(scalability_mode);
  int num_temporal_layers =
      ScalabilityModeToNumTemporalLayers(scalability_mode);

  std::map<uint32_t, EncodingSettings> encoding_settings;
  for (int frame_num = 0; frame_num < num_frames; ++frame_num) {
    std::map<LayerId, LayerSettings> layers_settings;
    for (int sidx = 0; sidx < num_spatial_layers; ++sidx) {
      for (int tidx = 0; tidx < num_temporal_layers; ++tidx) {
        layers_settings.emplace(
            LayerId{.spatial_idx = sidx, .temporal_idx = tidx},
            LayerSettings{.resolution = {.width = kWidth, .height = kHeight},
                          .framerate = kTargetFramerate /
                                       (1 << (num_temporal_layers - 1 - tidx)),
                          .bitrate = kTargetLayerBitrate});
      }
    }
    encoding_settings.emplace(
        frames[frame_num][0].timestamp_rtp,
        EncodingSettings{.scalability_mode = scalability_mode,
                         .layers_settings = layers_settings});
  }

  EncoderSettings encoder_settings;
  DecoderSettings decoder_settings;
  std::unique_ptr<VideoCodecStats> stats =
      VideoCodecTester::RunEncodeDecodeTest(
          source_settings, &encoder_factory, &decoder_factory, encoder_settings,
          decoder_settings, encoding_settings);
  remove(source_yuv_path.c_str());
  return stats;
}

EncodedImage CreateEncodedImage(uint32_t timestamp_rtp) {
  EncodedImage encoded_image;
  encoded_image.SetRtpTimestamp(timestamp_rtp);
  return encoded_image;
}

class MockCodedVideoSource : public CodedVideoSource {
 public:
  MockCodedVideoSource(int num_frames, Frequency framerate)
      : num_frames_(num_frames), frame_num_(0), framerate_(framerate) {}

  absl::optional<EncodedImage> PullFrame() override {
    if (frame_num_ >= num_frames_) {
      return absl::nullopt;
    }
    uint32_t timestamp_rtp = frame_num_ * k90kHz / framerate_;
    ++frame_num_;
    return CreateEncodedImage(timestamp_rtp);
  }

 private:
  int num_frames_;
  int frame_num_;
  Frequency framerate_;
};

}  // namespace

TEST(VideoCodecTester, Slice) {
  std::unique_ptr<VideoCodecStats> stats = RunTest(
      {{{.timestamp_rtp = 0, .layer_id = {.spatial_idx = 0, .temporal_idx = 0}},
        {.timestamp_rtp = 0,
         .layer_id = {.spatial_idx = 1, .temporal_idx = 0}}},
       {{.timestamp_rtp = 1,
         .layer_id = {.spatial_idx = 0, .temporal_idx = 1}}}},
      ScalabilityMode::kL2T2);
  std::vector<Frame> slice = stats->Slice(Filter{}, /*merge=*/false);
  EXPECT_THAT(slice, ElementsAre(Field(&Frame::timestamp_rtp, 0),
                                 Field(&Frame::timestamp_rtp, 0),
                                 Field(&Frame::timestamp_rtp, 1)));

  slice = stats->Slice({.min_timestamp_rtp = 1}, /*merge=*/false);
  EXPECT_THAT(slice, ElementsAre(Field(&Frame::timestamp_rtp, 1)));

  slice = stats->Slice({.max_timestamp_rtp = 0}, /*merge=*/false);
  EXPECT_THAT(slice, ElementsAre(Field(&Frame::timestamp_rtp, 0),
                                 Field(&Frame::timestamp_rtp, 0)));

  slice = stats->Slice({.layer_id = {{.spatial_idx = 0, .temporal_idx = 0}}},
                       /*merge=*/false);
  EXPECT_THAT(slice, ElementsAre(Field(&Frame::timestamp_rtp, 0)));

  slice = stats->Slice({.layer_id = {{.spatial_idx = 0, .temporal_idx = 1}}},
                       /*merge=*/false);
  EXPECT_THAT(slice, ElementsAre(Field(&Frame::timestamp_rtp, 0),
                                 Field(&Frame::timestamp_rtp, 1)));
}

TEST(VideoCodecTester, Merge) {
  std::unique_ptr<VideoCodecStats> stats =
      RunTest({{{.timestamp_rtp = 0,
                 .layer_id = {.spatial_idx = 0, .temporal_idx = 0},
                 .frame_size = DataSize::Bytes(1),
                 .keyframe = true},
                {.timestamp_rtp = 0,
                 .layer_id = {.spatial_idx = 1, .temporal_idx = 0},
                 .frame_size = DataSize::Bytes(2)}},
               {{.timestamp_rtp = 1,
                 .layer_id = {.spatial_idx = 0, .temporal_idx = 1},
                 .frame_size = DataSize::Bytes(4)},
                {.timestamp_rtp = 1,
                 .layer_id = {.spatial_idx = 1, .temporal_idx = 1},
                 .frame_size = DataSize::Bytes(8)}}},
              ScalabilityMode::kL2T2_KEY);

  std::vector<Frame> slice = stats->Slice(Filter{}, /*merge=*/true);
  EXPECT_THAT(
      slice,
      ElementsAre(
          AllOf(Field(&Frame::timestamp_rtp, 0), Field(&Frame::keyframe, true),
                Field(&Frame::frame_size, DataSize::Bytes(3))),
          AllOf(Field(&Frame::timestamp_rtp, 1), Field(&Frame::keyframe, false),
                Field(&Frame::frame_size, DataSize::Bytes(12)))));
}

struct AggregationTestParameters {
  Filter filter;
  double expected_keyframe_sum;
  double expected_encoded_bitrate_kbps;
  double expected_encoded_framerate_fps;
  double expected_bitrate_mismatch_pct;
  double expected_framerate_mismatch_pct;
};

class VideoCodecTesterTestAggregation
    : public ::testing::TestWithParam<AggregationTestParameters> {};

TEST_P(VideoCodecTesterTestAggregation, Aggregate) {
  AggregationTestParameters test_params = GetParam();
  std::unique_ptr<VideoCodecStats> stats =
      RunTest({{// L0T0
                {.timestamp_rtp = 0,
                 .layer_id = {.spatial_idx = 0, .temporal_idx = 0},
                 .frame_size = DataSize::Bytes(1),
                 .keyframe = true},
                // L1T0
                {.timestamp_rtp = 0,
                 .layer_id = {.spatial_idx = 1, .temporal_idx = 0},
                 .frame_size = DataSize::Bytes(2)}},
               // Emulate frame drop (frame_size = 0).
               {{.timestamp_rtp = 3000,
                 .layer_id = {.spatial_idx = 0, .temporal_idx = 0},
                 .frame_size = DataSize::Zero()}},
               {// L0T1
                {.timestamp_rtp = 87000,
                 .layer_id = {.spatial_idx = 0, .temporal_idx = 1},
                 .frame_size = DataSize::Bytes(4)},
                // L1T1
                {.timestamp_rtp = 87000,
                 .layer_id = {.spatial_idx = 1, .temporal_idx = 1},
                 .frame_size = DataSize::Bytes(8)}}},
              ScalabilityMode::kL2T2_KEY);

  Stream stream = stats->Aggregate(test_params.filter);
  EXPECT_EQ(stream.keyframe.GetSum(), test_params.expected_keyframe_sum);
  EXPECT_EQ(stream.encoded_bitrate_kbps.GetAverage(),
            test_params.expected_encoded_bitrate_kbps);
  EXPECT_EQ(stream.encoded_framerate_fps.GetAverage(),
            test_params.expected_encoded_framerate_fps);
  EXPECT_EQ(stream.bitrate_mismatch_pct.GetAverage(),
            test_params.expected_bitrate_mismatch_pct);
  EXPECT_EQ(stream.framerate_mismatch_pct.GetAverage(),
            test_params.expected_framerate_mismatch_pct);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    VideoCodecTesterTestAggregation,
    ::testing::Values(
        // No filtering.
        AggregationTestParameters{
            .filter = {},
            .expected_keyframe_sum = 1,
            .expected_encoded_bitrate_kbps =
                DataRate::BytesPerSec(15).kbps<double>(),
            .expected_encoded_framerate_fps = 2,
            .expected_bitrate_mismatch_pct =
                100 * (15.0 / (kTargetLayerBitrate.bytes_per_sec() * 4) - 1),
            .expected_framerate_mismatch_pct =
                100 * (2.0 / kTargetFramerate.hertz() - 1)},
        // L0T0
        AggregationTestParameters{
            .filter = {.layer_id = {{.spatial_idx = 0, .temporal_idx = 0}}},
            .expected_keyframe_sum = 1,
            .expected_encoded_bitrate_kbps =
                DataRate::BytesPerSec(1).kbps<double>(),
            .expected_encoded_framerate_fps = 1,
            .expected_bitrate_mismatch_pct =
                100 * (1.0 / kTargetLayerBitrate.bytes_per_sec() - 1),
            .expected_framerate_mismatch_pct =
                100 * (1.0 / (kTargetFramerate.hertz() / 2) - 1)},
        // L0T1
        AggregationTestParameters{
            .filter = {.layer_id = {{.spatial_idx = 0, .temporal_idx = 1}}},
            .expected_keyframe_sum = 1,
            .expected_encoded_bitrate_kbps =
                DataRate::BytesPerSec(5).kbps<double>(),
            .expected_encoded_framerate_fps = 2,
            .expected_bitrate_mismatch_pct =
                100 * (5.0 / (kTargetLayerBitrate.bytes_per_sec() * 2) - 1),
            .expected_framerate_mismatch_pct =
                100 * (2.0 / kTargetFramerate.hertz() - 1)},
        // L1T0
        AggregationTestParameters{
            .filter = {.layer_id = {{.spatial_idx = 1, .temporal_idx = 0}}},
            .expected_keyframe_sum = 1,
            .expected_encoded_bitrate_kbps =
                DataRate::BytesPerSec(3).kbps<double>(),
            .expected_encoded_framerate_fps = 1,
            .expected_bitrate_mismatch_pct =
                100 * (3.0 / kTargetLayerBitrate.bytes_per_sec() - 1),
            .expected_framerate_mismatch_pct =
                100 * (1.0 / (kTargetFramerate.hertz() / 2) - 1)},
        // L1T1
        AggregationTestParameters{
            .filter = {.layer_id = {{.spatial_idx = 1, .temporal_idx = 1}}},
            .expected_keyframe_sum = 1,
            .expected_encoded_bitrate_kbps =
                DataRate::BytesPerSec(11).kbps<double>(),
            .expected_encoded_framerate_fps = 2,
            .expected_bitrate_mismatch_pct =
                100 * (11.0 / (kTargetLayerBitrate.bytes_per_sec() * 2) - 1),
            .expected_framerate_mismatch_pct =
                100 * (2.0 / kTargetFramerate.hertz() - 1)}));

TEST(VideoCodecTester, Psnr) {
  std::unique_ptr<VideoCodecStats> stats =
      RunTest({{{.timestamp_rtp = 0, .frame_size = DataSize::Bytes(1)}},
               {{.timestamp_rtp = 3000, .frame_size = DataSize::Bytes(1)}}},
              ScalabilityMode::kL1T1);

  std::vector<Frame> slice = stats->Slice(Filter{}, /*merge=*/false);
  ASSERT_THAT(slice, SizeIs(2));
  ASSERT_TRUE(slice[0].psnr.has_value());
  ASSERT_TRUE(slice[1].psnr.has_value());
  EXPECT_NEAR(slice[0].psnr->y, 42, 1);
  EXPECT_NEAR(slice[0].psnr->u, 38, 1);
  EXPECT_NEAR(slice[0].psnr->v, 36, 1);
  EXPECT_NEAR(slice[1].psnr->y, 38, 1);
  EXPECT_NEAR(slice[1].psnr->u, 36, 1);
  EXPECT_NEAR(slice[1].psnr->v, 34, 1);
}

class VideoCodecTesterTestPacing
    : public ::testing::TestWithParam<std::tuple<PacingSettings, int>> {
 public:
  const int kSourceWidth = 2;
  const int kSourceHeight = 2;
  const int kNumFrames = 3;
  const int kTargetLayerBitrateKbps = 128;
  const Frequency kTargetFramerate = Frequency::Hertz(10);

  void SetUp() override {
    source_yuv_file_path_ = webrtc::test::TempFilename(
        webrtc::test::OutputPath(), "video_codec_tester_impl_unittest");
    FILE* file = fopen(source_yuv_file_path_.c_str(), "wb");
    for (int i = 0; i < 3 * kSourceWidth * kSourceHeight / 2; ++i) {
      fwrite("x", 1, 1, file);
    }
    fclose(file);
  }

 protected:
  std::string source_yuv_file_path_;
};

TEST_P(VideoCodecTesterTestPacing, PaceEncode) {
  auto [pacing_settings, expected_delta_ms] = GetParam();
  VideoSourceSettings video_source{
      .file_path = source_yuv_file_path_,
      .resolution = {.width = kSourceWidth, .height = kSourceHeight},
      .framerate = kTargetFramerate};

  NiceMock<MockVideoEncoderFactory> encoder_factory;
  ON_CALL(encoder_factory, CreateVideoEncoder(_))
      .WillByDefault([](const SdpVideoFormat&) {
        return std::make_unique<NiceMock<MockVideoEncoder>>();
      });

  std::map<uint32_t, EncodingSettings> encoding_settings =
      VideoCodecTester::CreateEncodingSettings(
          "VP8", "L1T1", kSourceWidth, kSourceHeight, {kTargetLayerBitrateKbps},
          kTargetFramerate.hertz(), kNumFrames);

  EncoderSettings encoder_settings;
  encoder_settings.pacing_settings = pacing_settings;
  std::vector<Frame> frames =
      VideoCodecTester::RunEncodeTest(video_source, &encoder_factory,
                                      encoder_settings, encoding_settings)
          ->Slice(/*filter=*/{}, /*merge=*/false);
  ASSERT_THAT(frames, SizeIs(kNumFrames));
  EXPECT_NEAR((frames[1].encode_start - frames[0].encode_start).ms(),
              expected_delta_ms, 10);
  EXPECT_NEAR((frames[2].encode_start - frames[1].encode_start).ms(),
              expected_delta_ms, 10);
}

TEST_P(VideoCodecTesterTestPacing, PaceDecode) {
  auto [pacing_settings, expected_delta_ms] = GetParam();
  MockCodedVideoSource video_source(kNumFrames, kTargetFramerate);

  NiceMock<MockVideoDecoderFactory> decoder_factory;
  ON_CALL(decoder_factory, CreateVideoDecoder(_))
      .WillByDefault([](const SdpVideoFormat&) {
        return std::make_unique<NiceMock<MockVideoDecoder>>();
      });

  DecoderSettings decoder_settings;
  decoder_settings.pacing_settings = pacing_settings;
  std::vector<Frame> frames =
      VideoCodecTester::RunDecodeTest(&video_source, &decoder_factory,
                                      decoder_settings, SdpVideoFormat("VP8"))
          ->Slice(/*filter=*/{}, /*merge=*/false);
  ASSERT_THAT(frames, SizeIs(kNumFrames));
  EXPECT_NEAR((frames[1].decode_start - frames[0].decode_start).ms(),
              expected_delta_ms, 10);
  EXPECT_NEAR((frames[2].decode_start - frames[1].decode_start).ms(),
              expected_delta_ms, 10);
}

INSTANTIATE_TEST_SUITE_P(
    DISABLED_All,
    VideoCodecTesterTestPacing,
    ::testing::Values(
        // No pacing.
        std::make_tuple(PacingSettings{.mode = PacingMode::kNoPacing},
                        /*expected_delta_ms=*/0),
        // Real-time pacing.
        std::make_tuple(PacingSettings{.mode = PacingMode::kRealTime},
                        /*expected_delta_ms=*/100),
        // Pace with specified constant rate.
        std::make_tuple(PacingSettings{.mode = PacingMode::kConstantRate,
                                       .constant_rate = Frequency::Hertz(20)},
                        /*expected_delta_ms=*/50)));
}  // namespace test
}  // namespace webrtc
