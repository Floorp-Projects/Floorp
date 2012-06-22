/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "gtest/gtest.h"
#include "modules/video_coding/codecs/interface/video_codec_interface.h"
#include "modules/video_coding/codecs/test/packet_manipulator.h"
#include "modules/video_coding/codecs/test/videoprocessor.h"
#include "modules/video_coding/codecs/vp8/main/interface/vp8.h"
#include "modules/video_coding/main/interface/video_coding.h"
#include "testsupport/fileutils.h"
#include "testsupport/frame_reader.h"
#include "testsupport/frame_writer.h"
#include "testsupport/metrics/video_metrics.h"
#include "testsupport/packet_reader.h"
#include "typedefs.h"

namespace webrtc {

const int kNbrFrames = 61;  // foreman_cif_short.yuv
const int kCIFWidth = 352;
const int kCIFHeight = 288;
const int kBitRateKbps = 500;

// Integration test for video processor. Encodes+decodes a small clip and
// writes it to the output directory. After completion, PSNR and SSIM
// measurements are performed on the original and the processed clip to verify
// the quality is acceptable.
// The limits for the PSNR and SSIM values must be set quite low, since we have
// no control over the random function used for packet loss in this test.
class VideoProcessorIntegrationTest: public testing::Test {
 protected:
  VideoEncoder* encoder_;
  VideoDecoder* decoder_;
  webrtc::test::FrameReader* frame_reader_;
  webrtc::test::FrameWriter* frame_writer_;
  webrtc::test::PacketReader packet_reader_;
  webrtc::test::PacketManipulator* packet_manipulator_;
  webrtc::test::Stats stats_;
  webrtc::test::TestConfig config_;
  VideoCodec codec_settings_;
  webrtc::test::VideoProcessor* processor_;

  VideoProcessorIntegrationTest() {}
  virtual ~VideoProcessorIntegrationTest() {}

  void SetUp() {
    encoder_ = VP8Encoder::Create();
    decoder_ = VP8Decoder::Create();

    // Setup the TestConfig struct for processing of a clip in CIF resolution.
    config_.input_filename =
        webrtc::test::ResourcePath("foreman_cif_short", "yuv");
    config_.output_filename = webrtc::test::OutputPath() +
        "foreman_cif_short_video_codecs_test_framework_integrationtests.yuv";
    config_.frame_length_in_bytes = 3 * kCIFWidth * kCIFHeight / 2;
    config_.verbose = false;
    // Only allow encoder/decoder to use single core, for predictability.
    config_.use_single_core = true;

    // Get a codec configuration struct and configure it.
    VideoCodingModule::Codec(kVideoCodecVP8, &codec_settings_);
    config_.codec_settings = &codec_settings_;
    config_.codec_settings->startBitrate = kBitRateKbps;
    config_.codec_settings->width = kCIFWidth;
    config_.codec_settings->height = kCIFHeight;

    frame_reader_ =
        new webrtc::test::FrameReaderImpl(config_.input_filename,
                                          config_.frame_length_in_bytes);
    frame_writer_ =
        new webrtc::test::FrameWriterImpl(config_.output_filename,
                                          config_.frame_length_in_bytes);
    ASSERT_TRUE(frame_reader_->Init());
    ASSERT_TRUE(frame_writer_->Init());

    packet_manipulator_ = new webrtc::test::PacketManipulatorImpl(
        &packet_reader_, config_.networking_config, config_.verbose);
    processor_ = new webrtc::test::VideoProcessorImpl(encoder_, decoder_,
                                                      frame_reader_,
                                                      frame_writer_,
                                                      packet_manipulator_,
                                                      config_, &stats_);
    ASSERT_TRUE(processor_->Init());
  }

  void TearDown() {
    delete processor_;
    delete packet_manipulator_;
    delete frame_writer_;
    delete frame_reader_;
    delete decoder_;
    delete encoder_;
  }

  // Processes all frames in the clip and verifies the result.
  void ProcessFramesAndVerify(double minimum_avg_psnr,
                              double minimum_min_psnr,
                              double minimum_avg_ssim,
                              double minimum_min_ssim) {
    int frame_number = 0;
    while (processor_->ProcessFrame(frame_number)) {
      frame_number++;
    }
    EXPECT_EQ(kNbrFrames, frame_number);
    EXPECT_EQ(kNbrFrames, static_cast<int>(stats_.stats_.size()));

    // Release encoder and decoder to make sure they have finished processing:
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Release());
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Release());
    // Close the files before we start using them for SSIM/PSNR calculations.
    frame_reader_->Close();
    frame_writer_->Close();

    webrtc::test::QualityMetricsResult psnr_result, ssim_result;
    EXPECT_EQ(0, webrtc::test::I420MetricsFromFiles(
        config_.input_filename.c_str(),
        config_.output_filename.c_str(),
        config_.codec_settings->width,
        config_.codec_settings->height,
        &psnr_result,
        &ssim_result));
    printf("PSNR avg: %f, min: %f    SSIM avg: %f, min: %f\n",
           psnr_result.average, psnr_result.min,
           ssim_result.average, ssim_result.min);
    EXPECT_GT(psnr_result.average, minimum_avg_psnr);
    EXPECT_GT(psnr_result.min, minimum_min_psnr);
    EXPECT_GT(ssim_result.average, minimum_avg_ssim);
    EXPECT_GT(ssim_result.min, minimum_min_ssim);
  }
};

// Run with no packet loss. Quality should be very high.
TEST_F(VideoProcessorIntegrationTest, ProcessZeroPacketLoss) {
  config_.networking_config.packet_loss_probability = 0;
  double minimum_avg_psnr = 36;
  double minimum_min_psnr = 34;
  double minimum_avg_ssim = 0.9;
  double minimum_min_ssim = 0.9;
  ProcessFramesAndVerify(minimum_avg_psnr, minimum_min_psnr,
                         minimum_avg_ssim, minimum_min_ssim);
}

// Run with 5% packet loss. Quality should be a bit lower.
TEST_F(VideoProcessorIntegrationTest, Process5PercentPacketLoss) {
  config_.networking_config.packet_loss_probability = 0.05;
  double minimum_avg_psnr = 21;
  double minimum_min_psnr = 16;
  double minimum_avg_ssim = 0.6;
  double minimum_min_ssim = 0.4;
  ProcessFramesAndVerify(minimum_avg_psnr, minimum_min_psnr,
                         minimum_avg_ssim, minimum_min_ssim);
}

// Run with 10% packet loss. Quality should be even lower.
TEST_F(VideoProcessorIntegrationTest, Process10PercentPacketLoss) {
  config_.networking_config.packet_loss_probability = 0.10;
  double minimum_avg_psnr = 19;
  double minimum_min_psnr = 16;
  double minimum_avg_ssim = 0.5;
  double minimum_min_ssim = 0.35;
  ProcessFramesAndVerify(minimum_avg_psnr, minimum_min_psnr,
                         minimum_avg_ssim, minimum_min_ssim);
}

}  // namespace webrtc
