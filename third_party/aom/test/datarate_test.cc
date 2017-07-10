/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
*/

#include "./aom_config.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "aom/aom_codec.h"

namespace {

class DatarateTestLarge
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, int>,
      public ::libaom_test::EncoderTest {
 public:
  DatarateTestLarge() : EncoderTest(GET_PARAM(0)) {}

 protected:
  virtual ~DatarateTestLarge() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
    set_cpu_used_ = GET_PARAM(2);
    ResetModel();
  }

  virtual void ResetModel() {
    last_pts_ = 0;
    bits_in_buffer_model_ = cfg_.rc_target_bitrate * cfg_.rc_buf_initial_sz;
    frame_number_ = 0;
    tot_frame_number_ = 0;
    first_drop_ = 0;
    num_drops_ = 0;
    // Denoiser is off by default.
    denoiser_on_ = 0;
    bits_total_ = 0;
    denoiser_offon_test_ = 0;
    denoiser_offon_period_ = -1;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) encoder->Control(AOME_SET_CPUUSED, set_cpu_used_);

    if (denoiser_offon_test_) {
      ASSERT_GT(denoiser_offon_period_, 0)
          << "denoiser_offon_period_ is not positive.";
      if ((video->frame() + 1) % denoiser_offon_period_ == 0) {
        // Flip denoiser_on_ periodically
        denoiser_on_ ^= 1;
      }
    }

    encoder->Control(AV1E_SET_NOISE_SENSITIVITY, denoiser_on_);

    const aom_rational_t tb = video->timebase();
    timebase_ = static_cast<double>(tb.num) / tb.den;
    duration_ = 0;
  }

  virtual void FramePktHook(const aom_codec_cx_pkt_t *pkt) {
    // Time since last timestamp = duration.
    aom_codec_pts_t duration = pkt->data.frame.pts - last_pts_;

    if (duration > 1) {
      // If first drop not set and we have a drop set it to this time.
      if (!first_drop_) first_drop_ = last_pts_ + 1;
      // Update the number of frame drops.
      num_drops_ += static_cast<int>(duration - 1);
      // Update counter for total number of frames (#frames input to encoder).
      // Needed for setting the proper layer_id below.
      tot_frame_number_ += static_cast<int>(duration - 1);
    }

    // Add to the buffer the bits we'd expect from a constant bitrate server.
    bits_in_buffer_model_ += static_cast<int64_t>(
        duration * timebase_ * cfg_.rc_target_bitrate * 1000);

    // Buffer should not go negative.
    ASSERT_GE(bits_in_buffer_model_, 0) << "Buffer Underrun at frame "
                                        << pkt->data.frame.pts;

    const size_t frame_size_in_bits = pkt->data.frame.sz * 8;

    // Update the total encoded bits.
    bits_total_ += frame_size_in_bits;

    // Update the most recent pts.
    last_pts_ = pkt->data.frame.pts;
    ++frame_number_;
    ++tot_frame_number_;
  }

  virtual void EndPassHook(void) {
    duration_ = (last_pts_ + 1) * timebase_;
    // Effective file datarate:
    effective_datarate_ = (bits_total_ / 1000.0) / duration_;
  }

  aom_codec_pts_t last_pts_;
  double timebase_;
  int frame_number_;      // Counter for number of non-dropped/encoded frames.
  int tot_frame_number_;  // Counter for total number of input frames.
  int64_t bits_total_;
  double duration_;
  double effective_datarate_;
  int set_cpu_used_;
  int64_t bits_in_buffer_model_;
  aom_codec_pts_t first_drop_;
  int num_drops_;
  int denoiser_on_;
  int denoiser_offon_test_;
  int denoiser_offon_period_;
};

// Check basic rate targeting for VBR mode.
TEST_P(DatarateTestLarge, BasicRateTargetingVBR) {
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.g_error_resilient = 0;
  cfg_.rc_end_usage = AOM_VBR;
  cfg_.g_lag_in_frames = 0;

  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);
  for (int i = 400; i <= 800; i += 400) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(effective_datarate_, cfg_.rc_target_bitrate * 0.75)
        << " The datarate for the file is lower than target by too much!";
    ASSERT_LE(effective_datarate_, cfg_.rc_target_bitrate * 1.25)
        << " The datarate for the file is greater than target by too much!";
  }
}

// Check basic rate targeting for CBR,
TEST_P(DatarateTestLarge, BasicRateTargeting) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = AOM_CBR;
  cfg_.g_lag_in_frames = 0;

  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 140);
  for (int i = 150; i < 800; i += 400) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(effective_datarate_, cfg_.rc_target_bitrate * 0.85)
        << " The datarate for the file is lower than target by too much!";
    ASSERT_LE(effective_datarate_, cfg_.rc_target_bitrate * 1.15)
        << " The datarate for the file is greater than target by too much!";
  }
}

// Check basic rate targeting for CBR.
TEST_P(DatarateTestLarge, BasicRateTargeting444) {
  ::libaom_test::Y4mVideoSource video("rush_hour_444.y4m", 0, 140);

  cfg_.g_profile = 1;
  cfg_.g_timebase = video.timebase();

  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_dropframe_thresh = 1;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 63;
  cfg_.rc_end_usage = AOM_CBR;

  for (int i = 250; i < 900; i += 400) {
    cfg_.rc_target_bitrate = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(static_cast<double>(cfg_.rc_target_bitrate),
              effective_datarate_ * 0.85)
        << " The datarate for the file exceeds the target by too much!";
    ASSERT_LE(static_cast<double>(cfg_.rc_target_bitrate),
              effective_datarate_ * 1.15)
        << " The datarate for the file missed the target!"
        << cfg_.rc_target_bitrate << " " << effective_datarate_;
  }
}

// Check that (1) the first dropped frame gets earlier and earlier
// as the drop frame threshold is increased, and (2) that the total number of
// frame drops does not decrease as we increase frame drop threshold.
// Use a lower qp-max to force some frame drops.
TEST_P(DatarateTestLarge, ChangingDropFrameThresh) {
  cfg_.rc_buf_initial_sz = 500;
  cfg_.rc_buf_optimal_sz = 500;
  cfg_.rc_buf_sz = 1000;
  cfg_.rc_undershoot_pct = 20;
  cfg_.rc_undershoot_pct = 20;
  cfg_.rc_dropframe_thresh = 10;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 50;
  cfg_.rc_end_usage = AOM_CBR;
  cfg_.rc_target_bitrate = 200;
  cfg_.g_lag_in_frames = 0;
  // TODO(marpan): Investigate datarate target failures with a smaller keyframe
  // interval (128).
  cfg_.kf_max_dist = 9999;

  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 100);

  const int kDropFrameThreshTestStep = 30;
  aom_codec_pts_t last_drop = 140;
  int last_num_drops = 0;
  for (int i = 40; i < 100; i += kDropFrameThreshTestStep) {
    cfg_.rc_dropframe_thresh = i;
    ResetModel();
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_GE(effective_datarate_, cfg_.rc_target_bitrate * 0.85)
        << " The datarate for the file is lower than target by too much!";
    ASSERT_LE(effective_datarate_, cfg_.rc_target_bitrate * 1.15)
        << " The datarate for the file is greater than target by too much!";
    ASSERT_LE(first_drop_, last_drop)
        << " The first dropped frame for drop_thresh " << i
        << " > first dropped frame for drop_thresh "
        << i - kDropFrameThreshTestStep;
    ASSERT_GE(num_drops_, last_num_drops * 0.85)
        << " The number of dropped frames for drop_thresh " << i
        << " < number of dropped frames for drop_thresh "
        << i - kDropFrameThreshTestStep;
    last_drop = first_drop_;
    last_num_drops = num_drops_;
  }
}

AV1_INSTANTIATE_TEST_CASE(DatarateTestLarge,
                          ::testing::Values(::libaom_test::kOnePassGood,
                                            ::libaom_test::kRealTime),
                          ::testing::Values(2, 5));
}  // namespace
