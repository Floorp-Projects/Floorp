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

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"
#include "test/y4m_video_source.h"

namespace {

const int kMaxPsnr = 100;

class LosslessTestLarge
    : public ::libaom_test::CodecTestWithParam<libaom_test::TestMode>,
      public ::libaom_test::EncoderTest {
 protected:
  LosslessTestLarge()
      : EncoderTest(GET_PARAM(0)), psnr_(kMaxPsnr), nframes_(0),
        encoding_mode_(GET_PARAM(1)) {}

  virtual ~LosslessTestLarge() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(encoding_mode_);
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 1) {
      // Only call Control if quantizer > 0 to verify that using quantizer
      // alone will activate lossless
      if (cfg_.rc_max_quantizer > 0 || cfg_.rc_min_quantizer > 0) {
        encoder->Control(AV1E_SET_LOSSLESS, 1);
      }
    }
  }

  virtual void BeginPassHook(unsigned int /*pass*/) {
    psnr_ = kMaxPsnr;
    nframes_ = 0;
  }

  virtual void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) {
    if (pkt->data.psnr.psnr[0] < psnr_) psnr_ = pkt->data.psnr.psnr[0];
  }

  double GetMinPsnr() const { return psnr_; }

 private:
  double psnr_;
  unsigned int nframes_;
  libaom_test::TestMode encoding_mode_;
};

TEST_P(LosslessTestLarge, TestLossLessEncoding) {
  const aom_rational timebase = { 33333333, 1000000000 };
  cfg_.g_timebase = timebase;
  cfg_.rc_target_bitrate = 2000;
  cfg_.g_lag_in_frames = 25;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 0;

  init_flags_ = AOM_CODEC_USE_PSNR;

  // intentionally changed the dimension for better testing coverage
  libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                     timebase.den, timebase.num, 0, 5);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  const double psnr_lossless = GetMinPsnr();
  EXPECT_GE(psnr_lossless, kMaxPsnr);
}

TEST_P(LosslessTestLarge, TestLossLessEncoding444) {
  libaom_test::Y4mVideoSource video("rush_hour_444.y4m", 0, 5);

  cfg_.g_profile = 1;
  cfg_.g_timebase = video.timebase();
  cfg_.rc_target_bitrate = 2000;
  cfg_.g_lag_in_frames = 25;
  cfg_.rc_min_quantizer = 0;
  cfg_.rc_max_quantizer = 0;

  init_flags_ = AOM_CODEC_USE_PSNR;

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  const double psnr_lossless = GetMinPsnr();
  EXPECT_GE(psnr_lossless, kMaxPsnr);
}

TEST_P(LosslessTestLarge, TestLossLessEncodingCtrl) {
  const aom_rational timebase = { 33333333, 1000000000 };
  cfg_.g_timebase = timebase;
  cfg_.rc_target_bitrate = 2000;
  cfg_.g_lag_in_frames = 25;
  // Intentionally set Q > 0, to make sure control can be used to activate
  // lossless
  cfg_.rc_min_quantizer = 10;
  cfg_.rc_max_quantizer = 20;

  init_flags_ = AOM_CODEC_USE_PSNR;

  libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                     timebase.den, timebase.num, 0, 5);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  const double psnr_lossless = GetMinPsnr();
  EXPECT_GE(psnr_lossless, kMaxPsnr);
}

AV1_INSTANTIATE_TEST_CASE(LosslessTestLarge,
                          ::testing::Values(::libaom_test::kOnePassGood,
                                            ::libaom_test::kTwoPassGood));
}  // namespace
