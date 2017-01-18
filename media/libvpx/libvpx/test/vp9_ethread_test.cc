/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>
#include <vector>
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/md5_helper.h"
#include "test/util.h"
#include "test/y4m_video_source.h"

namespace {
class VPxEncoderThreadTest
    : public ::libvpx_test::EncoderTest,
      public ::libvpx_test::CodecTestWith4Params<libvpx_test::TestMode, int,
                                                 int, int> {
 protected:
  VPxEncoderThreadTest()
      : EncoderTest(GET_PARAM(0)), encoder_initialized_(false),
        tiles_(GET_PARAM(3)), threads_(GET_PARAM(4)),
        encoding_mode_(GET_PARAM(1)), set_cpu_used_(GET_PARAM(2)) {
    init_flags_ = VPX_CODEC_USE_PSNR;
    md5_.clear();
  }
  virtual ~VPxEncoderThreadTest() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(encoding_mode_);

    if (encoding_mode_ != ::libvpx_test::kRealTime) {
      cfg_.g_lag_in_frames = 3;
      cfg_.rc_end_usage = VPX_VBR;
      cfg_.rc_2pass_vbr_minsection_pct = 5;
      cfg_.rc_2pass_vbr_maxsection_pct = 2000;
    } else {
      cfg_.g_lag_in_frames = 0;
      cfg_.rc_end_usage = VPX_CBR;
      cfg_.g_error_resilient = 1;
    }
    cfg_.rc_max_quantizer = 56;
    cfg_.rc_min_quantizer = 0;
  }

  virtual void BeginPassHook(unsigned int /*pass*/) {
    encoder_initialized_ = false;
  }

  virtual void PreEncodeFrameHook(::libvpx_test::VideoSource * /*video*/,
                                  ::libvpx_test::Encoder *encoder) {
    if (!encoder_initialized_) {
      // Encode 4 column tiles.
      encoder->Control(VP9E_SET_TILE_COLUMNS, tiles_);
      encoder->Control(VP8E_SET_CPUUSED, set_cpu_used_);
      if (encoding_mode_ != ::libvpx_test::kRealTime) {
        encoder->Control(VP8E_SET_ENABLEAUTOALTREF, 1);
        encoder->Control(VP8E_SET_ARNR_MAXFRAMES, 7);
        encoder->Control(VP8E_SET_ARNR_STRENGTH, 5);
        encoder->Control(VP8E_SET_ARNR_TYPE, 3);
        encoder->Control(VP9E_SET_FRAME_PARALLEL_DECODING, 0);
      } else {
        encoder->Control(VP8E_SET_ENABLEAUTOALTREF, 0);
        encoder->Control(VP9E_SET_AQ_MODE, 3);
      }
      encoder_initialized_ = true;
    }
  }

  virtual void DecompressedFrameHook(const vpx_image_t &img,
                                     vpx_codec_pts_t /*pts*/) {
    ::libvpx_test::MD5 md5_res;
    md5_res.Add(&img);
    md5_.push_back(md5_res.Get());
  }

  virtual bool HandleDecodeResult(const vpx_codec_err_t res,
                                  const libvpx_test::VideoSource & /*video*/,
                                  libvpx_test::Decoder * /*decoder*/) {
    if (res != VPX_CODEC_OK) {
      EXPECT_EQ(VPX_CODEC_OK, res);
      return false;
    }

    return true;
  }

  bool encoder_initialized_;
  int tiles_;
  int threads_;
  ::libvpx_test::TestMode encoding_mode_;
  int set_cpu_used_;
  std::vector<std::string> md5_;
};

TEST_P(VPxEncoderThreadTest, EncoderResultTest) {
  std::vector<std::string> single_thr_md5, multi_thr_md5;

  ::libvpx_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 15, 20);

  cfg_.rc_target_bitrate = 1000;

  // Encode using single thread.
  cfg_.g_threads = 1;
  init_flags_ = VPX_CODEC_USE_PSNR;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  single_thr_md5 = md5_;
  md5_.clear();

  // Encode using multiple threads.
  cfg_.g_threads = threads_;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  multi_thr_md5 = md5_;
  md5_.clear();

  // Compare to check if two vectors are equal.
  ASSERT_EQ(single_thr_md5, multi_thr_md5);
}

// Split this into two instantiations so that we can distinguish
// between very slow runs ( ie cpu_speed 0 ) vs ones that can be
// run nightly by adding Large to the title.
INSTANTIATE_TEST_CASE_P(
    VP9, VPxEncoderThreadTest,
    ::testing::Combine(
        ::testing::Values(
            static_cast<const libvpx_test::CodecFactory *>(&libvpx_test::kVP9)),
        ::testing::Values(::libvpx_test::kTwoPassGood,
                          ::libvpx_test::kOnePassGood,
                          ::libvpx_test::kRealTime),
        ::testing::Range(2, 9),    // cpu_used
        ::testing::Range(0, 3),    // tile_columns
        ::testing::Range(2, 5)));  // threads

INSTANTIATE_TEST_CASE_P(
    VP9Large, VPxEncoderThreadTest,
    ::testing::Combine(
        ::testing::Values(
            static_cast<const libvpx_test::CodecFactory *>(&libvpx_test::kVP9)),
        ::testing::Values(::libvpx_test::kTwoPassGood,
                          ::libvpx_test::kOnePassGood,
                          ::libvpx_test::kRealTime),
        ::testing::Range(0, 2),    // cpu_used
        ::testing::Range(0, 3),    // tile_columns
        ::testing::Range(2, 5)));  // threads

}  // namespace
