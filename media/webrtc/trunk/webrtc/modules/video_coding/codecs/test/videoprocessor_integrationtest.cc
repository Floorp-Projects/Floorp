/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>

#include "gtest/gtest.h"

#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/modules/video_coding/codecs/test/packet_manipulator.h"
#include "webrtc/modules/video_coding/codecs/test/videoprocessor.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8_common_types.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/frame_reader.h"
#include "webrtc/test/testsupport/frame_writer.h"
#include "webrtc/test/testsupport/metrics/video_metrics.h"
#include "webrtc/test/testsupport/packet_reader.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Maximum number of rate updates (i.e., calls to encoder to change bitrate
// and/or frame rate) for the current tests.
const int kMaxNumRateUpdates = 3;

const int kPercTargetvsActualMismatch = 20;
const int kBaseKeyFrameInterval = 3000;

// Codec and network settings.
struct CodecConfigPars {
  float packet_loss;
  int num_temporal_layers;
  int key_frame_interval;
  bool error_concealment_on;
  bool denoising_on;
  bool frame_dropper_on;
  bool spatial_resize_on;
};

// Quality metrics.
struct QualityMetrics {
  double minimum_avg_psnr;
  double minimum_min_psnr;
  double minimum_avg_ssim;
  double minimum_min_ssim;
};

// The sequence of bitrate and frame rate changes for the encoder, the frame
// number where the changes are made, and the total number of frames for the
// test.
struct RateProfile {
  int target_bit_rate[kMaxNumRateUpdates];
  int input_frame_rate[kMaxNumRateUpdates];
  int frame_index_rate_update[kMaxNumRateUpdates + 1];
  int num_frames;
};

// Metrics for the rate control. The rate mismatch metrics are defined as
// percentages.|max_time_hit_target| is defined as number of frames, after a
// rate update is made to the encoder, for the encoder to reach within
// |kPercTargetvsActualMismatch| of new target rate. The metrics are defined for
// each rate update sequence.
struct RateControlMetrics {
  int max_num_dropped_frames;
  int max_key_frame_size_mismatch;
  int max_delta_frame_size_mismatch;
  int max_encoding_rate_mismatch;
  int max_time_hit_target;
  int num_spatial_resizes;
};


// Sequence used is foreman (CIF): may be better to use VGA for resize test.
const int kCIFWidth = 352;
const int kCIFHeight = 288;
const int kNbrFramesShort = 100;  // Some tests are run for shorter sequence.
const int kNbrFramesLong = 299;

// Parameters from VP8 wrapper, which control target size of key frames.
const float kInitialBufferSize = 0.5f;
const float kOptimalBufferSize = 0.6f;
const float kScaleKeyFrameSize = 0.5f;

// Integration test for video processor. Encodes+decodes a clip and
// writes it to the output directory. After completion, quality metrics
// (PSNR and SSIM) and rate control metrics are computed to verify that the
// quality and encoder response is acceptable. The rate control tests allow us
// to verify the behavior for changing bitrate, changing frame rate, frame
// dropping/spatial resize, and temporal layers. The limits for the rate
// control metrics are set to be fairly conservative, so failure should only
// happen when some significant regression or breakdown occurs.
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

  // Quantities defined/updated for every encoder rate update.
  // Some quantities defined per temporal layer (at most 3 layers in this test).
  int num_frames_per_update_[3];
  float sum_frame_size_mismatch_[3];
  float sum_encoded_frame_size_[3];
  float encoding_bitrate_[3];
  float per_frame_bandwidth_[3];
  float bit_rate_layer_[3];
  float frame_rate_layer_[3];
  int num_frames_total_;
  float sum_encoded_frame_size_total_;
  float encoding_bitrate_total_;
  float perc_encoding_rate_mismatch_;
  int num_frames_to_hit_target_;
  bool encoding_rate_within_target_;
  int bit_rate_;
  int frame_rate_;
  int layer_;
  float target_size_key_frame_initial_;
  float target_size_key_frame_;
  float sum_key_frame_size_mismatch_;
  int num_key_frames_;
  float start_bitrate_;

  // Codec and network settings.
  float packet_loss_;
  int num_temporal_layers_;
  int key_frame_interval_;
  bool error_concealment_on_;
  bool denoising_on_;
  bool frame_dropper_on_;
  bool spatial_resize_on_;


  VideoProcessorIntegrationTest() {}
  virtual ~VideoProcessorIntegrationTest() {}

  void SetUpCodecConfig() {
    encoder_ = VP8Encoder::Create();
    decoder_ = VP8Decoder::Create();

    // CIF is currently used for all tests below.
    // Setup the TestConfig struct for processing of a clip in CIF resolution.
    config_.input_filename =
        webrtc::test::ResourcePath("foreman_cif", "yuv");
    config_.output_filename = webrtc::test::OutputPath() +
          "foreman_cif_short_video_codecs_test_framework_integrationtests.yuv";
    config_.frame_length_in_bytes = CalcBufferSize(kI420,
                                                   kCIFWidth, kCIFHeight);
    config_.verbose = false;
    // Only allow encoder/decoder to use single core, for predictability.
    config_.use_single_core = true;
    // Key frame interval and packet loss are set for each test.
    config_.keyframe_interval = key_frame_interval_;
    config_.networking_config.packet_loss_probability = packet_loss_;

    // Get a codec configuration struct and configure it.
    VideoCodingModule::Codec(kVideoCodecVP8, &codec_settings_);
    config_.codec_settings = &codec_settings_;
    config_.codec_settings->startBitrate = start_bitrate_;
    config_.codec_settings->width = kCIFWidth;
    config_.codec_settings->height = kCIFHeight;
    // These features may be set depending on the test.
    config_.codec_settings->codecSpecific.VP8.errorConcealmentOn =
        error_concealment_on_;
    config_.codec_settings->codecSpecific.VP8.denoisingOn =
        denoising_on_;
    config_.codec_settings->codecSpecific.VP8.numberOfTemporalLayers =
        num_temporal_layers_;
    config_.codec_settings->codecSpecific.VP8.frameDroppingOn =
        frame_dropper_on_;
    config_.codec_settings->codecSpecific.VP8.automaticResizeOn =
        spatial_resize_on_;
    config_.codec_settings->codecSpecific.VP8.keyFrameInterval =
        kBaseKeyFrameInterval;

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

  // Reset quantities after each encoder update, update the target
  // per-frame bandwidth.
  void ResetRateControlMetrics(int num_frames) {
    for (int i = 0; i < num_temporal_layers_; i++) {
      num_frames_per_update_[i] = 0;
      sum_frame_size_mismatch_[i] = 0.0f;
      sum_encoded_frame_size_[i] = 0.0f;
      encoding_bitrate_[i] = 0.0f;
      // Update layer per-frame-bandwidth.
      per_frame_bandwidth_[i] = static_cast<float>(bit_rate_layer_[i]) /
             static_cast<float>(frame_rate_layer_[i]);
    }
    // Set maximum size of key frames, following setting in the VP8 wrapper.
    float max_key_size = kScaleKeyFrameSize * kOptimalBufferSize * frame_rate_;
    // We don't know exact target size of the key frames (except for first one),
    // but the minimum in libvpx is ~|3 * per_frame_bandwidth| and maximum is
    // set by |max_key_size_  * per_frame_bandwidth|. Take middle point/average
    // as reference for mismatch. Note key frames always correspond to base
    // layer frame in this test.
    target_size_key_frame_ = 0.5 * (3 + max_key_size) * per_frame_bandwidth_[0];
    num_frames_total_ = 0;
    sum_encoded_frame_size_total_ = 0.0f;
    encoding_bitrate_total_ = 0.0f;
    perc_encoding_rate_mismatch_ = 0.0f;
    num_frames_to_hit_target_ = num_frames;
    encoding_rate_within_target_ = false;
    sum_key_frame_size_mismatch_ = 0.0;
    num_key_frames_ = 0;
  }

  // For every encoded frame, update the rate control metrics.
  void UpdateRateControlMetrics(int frame_num, VideoFrameType frame_type) {
    int encoded_frame_size = processor_->EncodedFrameSize();
    float encoded_size_kbits = encoded_frame_size * 8.0f / 1000.0f;
    // Update layer data.
    // Update rate mismatch relative to per-frame bandwidth for delta frames.
    if (frame_type == kDeltaFrame) {
      // TODO(marpan): Should we count dropped (zero size) frames in mismatch?
      sum_frame_size_mismatch_[layer_] += fabs(encoded_size_kbits -
                                               per_frame_bandwidth_[layer_]) /
                                               per_frame_bandwidth_[layer_];
    } else {
      float target_size = (frame_num == 1) ? target_size_key_frame_initial_ :
          target_size_key_frame_;
      sum_key_frame_size_mismatch_ += fabs(encoded_size_kbits - target_size) /
          target_size;
      num_key_frames_ += 1;
    }
    sum_encoded_frame_size_[layer_] += encoded_size_kbits;
    // Encoding bitrate per layer: from the start of the update/run to the
    // current frame.
    encoding_bitrate_[layer_] = sum_encoded_frame_size_[layer_] *
        frame_rate_layer_[layer_] /
        num_frames_per_update_[layer_];
    // Total encoding rate: from the start of the update/run to current frame.
    sum_encoded_frame_size_total_ += encoded_size_kbits;
    encoding_bitrate_total_ = sum_encoded_frame_size_total_ * frame_rate_ /
        num_frames_total_;
    perc_encoding_rate_mismatch_ =  100 * fabs(encoding_bitrate_total_ -
                                               bit_rate_) / bit_rate_;
    if (perc_encoding_rate_mismatch_ < kPercTargetvsActualMismatch &&
        !encoding_rate_within_target_) {
      num_frames_to_hit_target_ = num_frames_total_;
      encoding_rate_within_target_ = true;
    }
  }

  // Verify expected behavior of rate control and print out data.
  void VerifyRateControl(int update_index,
                         int max_key_frame_size_mismatch,
                         int max_delta_frame_size_mismatch,
                         int max_encoding_rate_mismatch,
                         int max_time_hit_target,
                         int max_num_dropped_frames,
                         int num_spatial_resizes) {
    int num_dropped_frames = processor_->NumberDroppedFrames();
    int num_resize_actions = processor_->NumberSpatialResizes();
    printf("For update #: %d,\n "
        " Target Bitrate: %d,\n"
        " Encoding bitrate: %f,\n"
        " Frame rate: %d \n",
        update_index, bit_rate_, encoding_bitrate_total_, frame_rate_);
    printf(" Number of frames to approach target rate = %d, \n"
           " Number of dropped frames = %d, \n"
           " Number of spatial resizes = %d, \n",
           num_frames_to_hit_target_, num_dropped_frames, num_resize_actions);
    EXPECT_LE(perc_encoding_rate_mismatch_, max_encoding_rate_mismatch);
    if (num_key_frames_ > 0) {
      int perc_key_frame_size_mismatch = 100 * sum_key_frame_size_mismatch_ /
              num_key_frames_;
      printf(" Number of Key frames: %d \n"
             " Key frame rate mismatch: %d \n",
             num_key_frames_, perc_key_frame_size_mismatch);
      EXPECT_LE(perc_key_frame_size_mismatch, max_key_frame_size_mismatch);
    }
    printf("\n");
    printf("Rates statistics for Layer data \n");
    for (int i = 0; i < num_temporal_layers_ ; i++) {
      printf("Layer #%d \n", i);
      int perc_frame_size_mismatch = 100 * sum_frame_size_mismatch_[i] /
        num_frames_per_update_[i];
      int perc_encoding_rate_mismatch = 100 * fabs(encoding_bitrate_[i] -
                                                   bit_rate_layer_[i]) /
                                                   bit_rate_layer_[i];
      printf(" Target Layer Bit rate: %f \n"
          " Layer frame rate: %f, \n"
          " Layer per frame bandwidth: %f, \n"
          " Layer Encoding bit rate: %f, \n"
          " Layer Percent frame size mismatch: %d,  \n"
          " Layer Percent encoding rate mismatch = %d, \n"
          " Number of frame processed per layer = %d \n",
          bit_rate_layer_[i], frame_rate_layer_[i], per_frame_bandwidth_[i],
          encoding_bitrate_[i], perc_frame_size_mismatch,
          perc_encoding_rate_mismatch, num_frames_per_update_[i]);
      EXPECT_LE(perc_frame_size_mismatch, max_delta_frame_size_mismatch);
      EXPECT_LE(perc_encoding_rate_mismatch, max_encoding_rate_mismatch);
    }
    printf("\n");
    EXPECT_LE(num_frames_to_hit_target_, max_time_hit_target);
    EXPECT_LE(num_dropped_frames, max_num_dropped_frames);
    EXPECT_EQ(num_resize_actions, num_spatial_resizes);
  }

  // Layer index corresponding to frame number, for up to 3 layers.
  void LayerIndexForFrame(int frame_number) {
    if (num_temporal_layers_ == 1) {
      layer_ = 0;
    } else if (num_temporal_layers_ == 2) {
        // layer 0:  0     2     4 ...
        // layer 1:     1     3
        if (frame_number % 2 == 0) {
          layer_ = 0;
        } else {
          layer_ = 1;
        }
    } else if (num_temporal_layers_ == 3) {
      // layer 0:  0            4            8 ...
      // layer 1:        2            6
      // layer 2:     1      3     5      7
      if (frame_number % 4 == 0) {
        layer_ = 0;
      } else if ((frame_number + 2) % 4 == 0) {
        layer_ = 1;
      } else if ((frame_number + 1) % 2 == 0) {
        layer_ = 2;
      }
    } else {
      assert(false);  // Only up to 3 layers.
    }
  }

  // Set the bitrate and frame rate per layer, for up to 3 layers.
  void SetLayerRates() {
    assert(num_temporal_layers_<= 3);
    for (int i = 0; i < num_temporal_layers_; i++) {
      float bit_rate_ratio =
          kVp8LayerRateAlloction[num_temporal_layers_ - 1][i];
      if (i > 0) {
        float bit_rate_delta_ratio = kVp8LayerRateAlloction
            [num_temporal_layers_ - 1][i] -
            kVp8LayerRateAlloction[num_temporal_layers_ - 1][i - 1];
        bit_rate_layer_[i] = bit_rate_ * bit_rate_delta_ratio;
      } else {
        bit_rate_layer_[i] = bit_rate_ * bit_rate_ratio;
      }
      frame_rate_layer_[i] = frame_rate_ / static_cast<float>(
          1 << (num_temporal_layers_ - 1));
    }
    if (num_temporal_layers_ == 3) {
      frame_rate_layer_[2] = frame_rate_ / 2.0f;
    }
  }

  VideoFrameType FrameType(int frame_number) {
    if (frame_number == 0 || ((frame_number) % key_frame_interval_ == 0 &&
        key_frame_interval_ > 0)) {
      return kKeyFrame;
    } else {
      return kDeltaFrame;
    }
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
  void ProcessFramesAndVerify(QualityMetrics quality_metrics,
                              RateProfile rate_profile,
                              CodecConfigPars process,
                              RateControlMetrics* rc_metrics) {
    // Codec/config settings.
    start_bitrate_ = rate_profile.target_bit_rate[0];
    packet_loss_ = process.packet_loss;
    key_frame_interval_ = process.key_frame_interval;
    num_temporal_layers_ = process.num_temporal_layers;
    error_concealment_on_ = process.error_concealment_on;
    denoising_on_ = process.denoising_on;
    frame_dropper_on_ = process.frame_dropper_on;
    spatial_resize_on_ = process.spatial_resize_on;
    SetUpCodecConfig();
    // Update the layers and the codec with the initial rates.
    bit_rate_ =  rate_profile.target_bit_rate[0];
    frame_rate_ = rate_profile.input_frame_rate[0];
    SetLayerRates();
    // Set the initial target size for key frame.
    target_size_key_frame_initial_ = 0.5 * kInitialBufferSize *
        bit_rate_layer_[0];
    processor_->SetRates(bit_rate_, frame_rate_);
    // Process each frame, up to |num_frames|.
    int num_frames = rate_profile.num_frames;
    int update_index = 0;
    ResetRateControlMetrics(
        rate_profile.frame_index_rate_update[update_index + 1]);
    int frame_number = 0;
    VideoFrameType frame_type = kDeltaFrame;
    while (processor_->ProcessFrame(frame_number) &&
        frame_number < num_frames) {
      // Get the layer index for the frame |frame_number|.
      LayerIndexForFrame(frame_number);
      frame_type = FrameType(frame_number);
      // Counter for whole sequence run.
      ++frame_number;
      // Counters for each rate update.
      ++num_frames_per_update_[layer_];
      ++num_frames_total_;
      UpdateRateControlMetrics(frame_number, frame_type);
      // If we hit another/next update, verify stats for current state and
      // update layers and codec with new rates.
      if (frame_number ==
          rate_profile.frame_index_rate_update[update_index + 1]) {
        VerifyRateControl(
            update_index,
            rc_metrics[update_index].max_key_frame_size_mismatch,
            rc_metrics[update_index].max_delta_frame_size_mismatch,
            rc_metrics[update_index].max_encoding_rate_mismatch,
            rc_metrics[update_index].max_time_hit_target,
            rc_metrics[update_index].max_num_dropped_frames,
            rc_metrics[update_index].num_spatial_resizes);
        // Update layer rates and the codec with new rates.
        ++update_index;
        bit_rate_ =  rate_profile.target_bit_rate[update_index];
        frame_rate_ = rate_profile.input_frame_rate[update_index];
        SetLayerRates();
        ResetRateControlMetrics(rate_profile.
                                frame_index_rate_update[update_index + 1]);
        processor_->SetRates(bit_rate_, frame_rate_);
      }
    }
    VerifyRateControl(
        update_index,
        rc_metrics[update_index].max_key_frame_size_mismatch,
        rc_metrics[update_index].max_delta_frame_size_mismatch,
        rc_metrics[update_index].max_encoding_rate_mismatch,
        rc_metrics[update_index].max_time_hit_target,
        rc_metrics[update_index].max_num_dropped_frames,
        rc_metrics[update_index].num_spatial_resizes);
    EXPECT_EQ(num_frames, frame_number);
    EXPECT_EQ(num_frames + 1, static_cast<int>(stats_.stats_.size()));

    // Release encoder and decoder to make sure they have finished processing:
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Release());
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Release());
    // Close the files before we start using them for SSIM/PSNR calculations.
    frame_reader_->Close();
    frame_writer_->Close();

    // TODO(marpan): should compute these quality metrics per SetRates update.
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
    stats_.PrintSummary();
    EXPECT_GT(psnr_result.average, quality_metrics.minimum_avg_psnr);
    EXPECT_GT(psnr_result.min, quality_metrics.minimum_min_psnr);
    EXPECT_GT(ssim_result.average, quality_metrics.minimum_avg_ssim);
    EXPECT_GT(ssim_result.min, quality_metrics.minimum_min_ssim);
  }
};

void SetRateProfilePars(RateProfile* rate_profile,
                        int update_index,
                        int bit_rate,
                        int frame_rate,
                        int frame_index_rate_update) {
  rate_profile->target_bit_rate[update_index] = bit_rate;
  rate_profile->input_frame_rate[update_index] = frame_rate;
  rate_profile->frame_index_rate_update[update_index] = frame_index_rate_update;
}

void SetCodecParameters(CodecConfigPars* process_settings,
                        float packet_loss,
                        int key_frame_interval,
                        int num_temporal_layers,
                        bool error_concealment_on,
                        bool denoising_on,
                        bool frame_dropper_on,
                        bool spatial_resize_on) {
  process_settings->packet_loss = packet_loss;
  process_settings->key_frame_interval =  key_frame_interval;
  process_settings->num_temporal_layers = num_temporal_layers,
  process_settings->error_concealment_on = error_concealment_on;
  process_settings->denoising_on = denoising_on;
  process_settings->frame_dropper_on = frame_dropper_on;
  process_settings->spatial_resize_on = spatial_resize_on;
}

void SetQualityMetrics(QualityMetrics* quality_metrics,
                       double minimum_avg_psnr,
                       double minimum_min_psnr,
                       double minimum_avg_ssim,
                       double minimum_min_ssim) {
  quality_metrics->minimum_avg_psnr = minimum_avg_psnr;
  quality_metrics->minimum_min_psnr = minimum_min_psnr;
  quality_metrics->minimum_avg_ssim = minimum_avg_ssim;
  quality_metrics->minimum_min_ssim = minimum_min_ssim;
}

void SetRateControlMetrics(RateControlMetrics* rc_metrics,
                           int update_index,
                           int max_num_dropped_frames,
                           int max_key_frame_size_mismatch,
                           int max_delta_frame_size_mismatch,
                           int max_encoding_rate_mismatch,
                           int max_time_hit_target,
                           int num_spatial_resizes) {
  rc_metrics[update_index].max_num_dropped_frames = max_num_dropped_frames;
  rc_metrics[update_index].max_key_frame_size_mismatch =
      max_key_frame_size_mismatch;
  rc_metrics[update_index].max_delta_frame_size_mismatch =
      max_delta_frame_size_mismatch;
  rc_metrics[update_index].max_encoding_rate_mismatch =
      max_encoding_rate_mismatch;
  rc_metrics[update_index].max_time_hit_target = max_time_hit_target;
  rc_metrics[update_index].num_spatial_resizes = num_spatial_resizes;
}

// Run with no packet loss and fixed bitrate. Quality should be very high.
// One key frame (first frame only) in sequence. Setting |key_frame_interval|
// to -1 below means no periodic key frames in test.
TEST_F(VideoProcessorIntegrationTest, ProcessZeroPacketLoss) {
  // Bitrate and frame rate profile.
  RateProfile rate_profile;
  SetRateProfilePars(&rate_profile, 0, 500, 30, 0);
  rate_profile.frame_index_rate_update[1] = kNbrFramesShort + 1;
  rate_profile.num_frames = kNbrFramesShort;
  // Codec/network settings.
  CodecConfigPars process_settings;
  SetCodecParameters(&process_settings, 0.0f, -1, 1, false, true, true, false);
  // Metrics for expected quality.
  QualityMetrics quality_metrics;
  SetQualityMetrics(&quality_metrics, 36.95, 33.0, 0.90, 0.90);
  // Metrics for rate control.
  RateControlMetrics rc_metrics[1];
  SetRateControlMetrics(rc_metrics, 0, 0, 40, 20, 10, 15, 0);
  ProcessFramesAndVerify(quality_metrics,
                         rate_profile,
                         process_settings,
                         rc_metrics);
}

// Run with 5% packet loss and fixed bitrate. Quality should be a bit lower.
// One key frame (first frame only) in sequence.
TEST_F(VideoProcessorIntegrationTest, Process5PercentPacketLoss) {
  // Bitrate and frame rate profile.
  RateProfile rate_profile;
  SetRateProfilePars(&rate_profile, 0, 500, 30, 0);
  rate_profile.frame_index_rate_update[1] = kNbrFramesShort + 1;
  rate_profile.num_frames = kNbrFramesShort;
  // Codec/network settings.
  CodecConfigPars process_settings;
  SetCodecParameters(&process_settings, 0.05f, -1, 1, false, true, true, false);
  // Metrics for expected quality.
  QualityMetrics quality_metrics;
  SetQualityMetrics(&quality_metrics, 20.0, 16.0, 0.60, 0.40);
  // Metrics for rate control.
  RateControlMetrics rc_metrics[1];
  SetRateControlMetrics(rc_metrics, 0, 0, 40, 20, 10, 15, 0);
  ProcessFramesAndVerify(quality_metrics,
                         rate_profile,
                         process_settings,
                         rc_metrics);
}

// Run with 10% packet loss and fixed bitrate. Quality should be even lower.
// One key frame (first frame only) in sequence.
TEST_F(VideoProcessorIntegrationTest, Process10PercentPacketLoss) {
  // Bitrate and frame rate profile.
  RateProfile rate_profile;
  SetRateProfilePars(&rate_profile, 0, 500, 30, 0);
  rate_profile.frame_index_rate_update[1] = kNbrFramesShort + 1;
  rate_profile.num_frames = kNbrFramesShort;
  // Codec/network settings.
  CodecConfigPars process_settings;
  SetCodecParameters(&process_settings, 0.1f, -1, 1, false, true, true, false);
  // Metrics for expected quality.
  QualityMetrics quality_metrics;
  SetQualityMetrics(&quality_metrics, 19.0, 16.0, 0.50, 0.35);
  // Metrics for rate control.
  RateControlMetrics rc_metrics[1];
  SetRateControlMetrics(rc_metrics, 0, 0, 40, 20, 10, 15, 0);
  ProcessFramesAndVerify(quality_metrics,
                         rate_profile,
                         process_settings,
                         rc_metrics);
}

// Run with no packet loss, with varying bitrate (3 rate updates):
// low to high to medium. Check that quality and encoder response to the new
// target rate/per-frame bandwidth (for each rate update) is within limits.
// One key frame (first frame only) in sequence.
TEST_F(VideoProcessorIntegrationTest, ProcessNoLossChangeBitRate) {
  // Bitrate and frame rate profile.
  RateProfile rate_profile;
  SetRateProfilePars(&rate_profile, 0, 200, 30, 0);
  SetRateProfilePars(&rate_profile, 1, 800, 30, 100);
  SetRateProfilePars(&rate_profile, 2, 500, 30, 200);
  rate_profile.frame_index_rate_update[3] = kNbrFramesLong + 1;
  rate_profile.num_frames = kNbrFramesLong;
  // Codec/network settings.
  CodecConfigPars process_settings;
  SetCodecParameters(&process_settings, 0.0f, -1, 1, false, true, true, false);
  // Metrics for expected quality.
  QualityMetrics quality_metrics;
  SetQualityMetrics(&quality_metrics, 34.0, 32.0, 0.85, 0.80);
  // Metrics for rate control.
  RateControlMetrics rc_metrics[3];
  SetRateControlMetrics(rc_metrics, 0, 0, 45, 20, 10, 15, 0);
  SetRateControlMetrics(rc_metrics, 1, 0, 0, 25, 20, 10, 0);
  SetRateControlMetrics(rc_metrics, 2, 0, 0, 25, 15, 10, 0);
  ProcessFramesAndVerify(quality_metrics,
                         rate_profile,
                         process_settings,
                         rc_metrics);
}

// Run with no packet loss, with an update (decrease) in frame rate.
// Lower frame rate means higher per-frame-bandwidth, so easier to encode.
// At the bitrate in this test, this means better rate control after the
// update(s) to lower frame rate. So expect less frame drops, and max values
// for the rate control metrics can be lower. One key frame (first frame only).
// Note: quality after update should be higher but we currently compute quality
// metrics avergaed over whole sequence run.
TEST_F(VideoProcessorIntegrationTest, ProcessNoLossChangeFrameRateFrameDrop) {
  config_.networking_config.packet_loss_probability = 0;
  // Bitrate and frame rate profile.
  RateProfile rate_profile;
  SetRateProfilePars(&rate_profile, 0, 80, 24, 0);
  SetRateProfilePars(&rate_profile, 1, 80, 15, 100);
  SetRateProfilePars(&rate_profile, 2, 80, 10, 200);
  rate_profile.frame_index_rate_update[3] = kNbrFramesLong + 1;
  rate_profile.num_frames = kNbrFramesLong;
  // Codec/network settings.
  CodecConfigPars process_settings;
  SetCodecParameters(&process_settings, 0.0f, -1, 1, false, true, true, false);
  // Metrics for expected quality.
  QualityMetrics quality_metrics;
  SetQualityMetrics(&quality_metrics, 31.0, 22.0, 0.80, 0.65);
  // Metrics for rate control.
  RateControlMetrics rc_metrics[3];
  SetRateControlMetrics(rc_metrics, 0, 40, 20, 75, 15, 60, 0);
  SetRateControlMetrics(rc_metrics, 1, 10, 0, 25, 10, 35, 0);
  SetRateControlMetrics(rc_metrics, 2, 0, 0, 20, 10, 15, 0);
  ProcessFramesAndVerify(quality_metrics,
                         rate_profile,
                         process_settings,
                         rc_metrics);
}

// Run with no packet loss, at low bitrate, then increase rate somewhat.
// Key frame is thrown in every 120 frames. Can expect some frame drops after
// key frame, even at high rate. The internal spatial resizer is on, so expect
// spatial resize down at first key frame, and back up at second key frame.
// Error_concealment is off in this test since there is a memory leak with
// resizing and error concealment.
TEST_F(VideoProcessorIntegrationTest, ProcessNoLossSpatialResizeFrameDrop) {
  config_.networking_config.packet_loss_probability = 0;
  // Bitrate and frame rate profile.
  RateProfile rate_profile;
  SetRateProfilePars(&rate_profile, 0, 100, 30, 0);
  SetRateProfilePars(&rate_profile, 1, 200, 30, 120);
  SetRateProfilePars(&rate_profile, 2, 200, 30, 240);
  rate_profile.frame_index_rate_update[3] = kNbrFramesLong + 1;
  rate_profile.num_frames = kNbrFramesLong;
  // Codec/network settings.
  CodecConfigPars process_settings;
  SetCodecParameters(&process_settings, 0.0f, 120, 1, false, true, true, true);
  // Metrics for expected quality.: lower quality on average from up-sampling
  // the down-sampled portion of the run, in case resizer is on.
  QualityMetrics quality_metrics;
  SetQualityMetrics(&quality_metrics, 29.0, 20.0, 0.75, 0.60);
  // Metrics for rate control.
  RateControlMetrics rc_metrics[3];
  SetRateControlMetrics(rc_metrics, 0, 45, 30, 75, 20, 70, 0);
  SetRateControlMetrics(rc_metrics, 1, 20, 35, 30, 20, 15, 1);
  SetRateControlMetrics(rc_metrics, 2, 0, 30, 30, 15, 25, 1);
  ProcessFramesAndVerify(quality_metrics,
                         rate_profile,
                         process_settings,
                         rc_metrics);
}

// Run with no packet loss, with 3 temporal layers, with a rate update in the
// middle of the sequence. The max values for the frame size mismatch and
// encoding rate mismatch are applied to each layer.
// No dropped frames in this test, and internal spatial resizer is off.
// One key frame (first frame only) in sequence, so no spatial resizing.
TEST_F(VideoProcessorIntegrationTest, ProcessNoLossTemporalLayers) {
  config_.networking_config.packet_loss_probability = 0;
  // Bitrate and frame rate profile.
  RateProfile rate_profile;
  SetRateProfilePars(&rate_profile, 0, 200, 30, 0);
  SetRateProfilePars(&rate_profile, 1, 400, 30, 150);
  rate_profile.frame_index_rate_update[2] = kNbrFramesLong + 1;
  rate_profile.num_frames = kNbrFramesLong;
  // Codec/network settings.
  CodecConfigPars process_settings;
  SetCodecParameters(&process_settings, 0.0f, -1, 3, false, true, true, false);
  // Metrics for expected quality.
  QualityMetrics quality_metrics;
  SetQualityMetrics(&quality_metrics, 32.5, 30.0, 0.85, 0.80);
  // Metrics for rate control.
  RateControlMetrics rc_metrics[2];
  SetRateControlMetrics(rc_metrics, 0, 0, 20, 30, 10, 10, 0);
  SetRateControlMetrics(rc_metrics, 1, 0, 0, 30, 15, 10, 0);
  ProcessFramesAndVerify(quality_metrics,
                         rate_profile,
                         process_settings,
                         rc_metrics);
}
}  // namespace webrtc
