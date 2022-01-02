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

#include <string>
#include <vector>
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/md5_helper.h"
#include "test/util.h"
#include "test/yuv_video_source.h"

namespace {
class AVxEncoderThreadTest
    : public ::libaom_test::CodecTestWith4Params<libaom_test::TestMode, int,
                                                 int, int>,
      public ::libaom_test::EncoderTest {
 protected:
  AVxEncoderThreadTest()
      : EncoderTest(GET_PARAM(0)), encoder_initialized_(false),
        encoding_mode_(GET_PARAM(1)), set_cpu_used_(GET_PARAM(2)),
        tile_cols_(GET_PARAM(3)), tile_rows_(GET_PARAM(4)) {
    init_flags_ = AOM_CODEC_USE_PSNR;
    aom_codec_dec_cfg_t cfg = aom_codec_dec_cfg_t();
    cfg.w = 1280;
    cfg.h = 720;
    cfg.allow_lowbitdepth = 1;
    decoder_ = codec_->CreateDecoder(cfg, 0);
    if (decoder_->IsAV1()) {
      decoder_->Control(AV1_SET_DECODE_TILE_ROW, -1);
      decoder_->Control(AV1_SET_DECODE_TILE_COL, -1);
    }

    size_enc_.clear();
    md5_dec_.clear();
    md5_enc_.clear();
  }
  virtual ~AVxEncoderThreadTest() { delete decoder_; }

  virtual void SetUp() {
    InitializeConfig();
    SetMode(encoding_mode_);

    if (encoding_mode_ != ::libaom_test::kRealTime) {
      cfg_.g_lag_in_frames = 5;
      cfg_.rc_end_usage = AOM_VBR;
      cfg_.rc_2pass_vbr_minsection_pct = 5;
      cfg_.rc_2pass_vbr_maxsection_pct = 2000;
    } else {
      cfg_.g_lag_in_frames = 0;
      cfg_.rc_end_usage = AOM_CBR;
      cfg_.g_error_resilient = 1;
    }
    cfg_.rc_max_quantizer = 56;
    cfg_.rc_min_quantizer = 0;
  }

  virtual void BeginPassHook(unsigned int /*pass*/) {
    encoder_initialized_ = false;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource * /*video*/,
                                  ::libaom_test::Encoder *encoder) {
    if (!encoder_initialized_) {
      SetTileSize(encoder);
      encoder->Control(AOME_SET_CPUUSED, set_cpu_used_);
      encoder->Control(AV1E_SET_ROW_MT, row_mt_);
      if (encoding_mode_ != ::libaom_test::kRealTime) {
        encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
        encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
        encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
        encoder->Control(AV1E_SET_FRAME_PARALLEL_DECODING, 0);
      } else {
        encoder->Control(AOME_SET_ENABLEAUTOALTREF, 0);
        encoder->Control(AV1E_SET_AQ_MODE, 3);
      }
      encoder_initialized_ = true;
    }
  }

  virtual void SetTileSize(libaom_test::Encoder *encoder) {
    encoder->Control(AV1E_SET_TILE_COLUMNS, tile_cols_);
    encoder->Control(AV1E_SET_TILE_ROWS, tile_rows_);
  }

  virtual void FramePktHook(const aom_codec_cx_pkt_t *pkt) {
    size_enc_.push_back(pkt->data.frame.sz);

    ::libaom_test::MD5 md5_enc;
    md5_enc.Add(reinterpret_cast<uint8_t *>(pkt->data.frame.buf),
                pkt->data.frame.sz);
    md5_enc_.push_back(md5_enc.Get());

    const aom_codec_err_t res = decoder_->DecodeFrame(
        reinterpret_cast<uint8_t *>(pkt->data.frame.buf), pkt->data.frame.sz);
    if (res != AOM_CODEC_OK) {
      abort_ = true;
      ASSERT_EQ(AOM_CODEC_OK, res);
    }
    const aom_image_t *img = decoder_->GetDxData().Next();

    if (img) {
      ::libaom_test::MD5 md5_res;
      md5_res.Add(img);
      md5_dec_.push_back(md5_res.Get());
    }
  }

  void DoTest() {
    ::libaom_test::YUVVideoSource video(
        "niklas_640_480_30.yuv", AOM_IMG_FMT_I420, 640, 480, 30, 1, 15, 21);
    cfg_.rc_target_bitrate = 1000;

    // Encode using single thread.
    row_mt_ = 0;
    cfg_.g_threads = 1;
    init_flags_ = AOM_CODEC_USE_PSNR;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    std::vector<size_t> single_thr_size_enc;
    std::vector<std::string> single_thr_md5_enc;
    std::vector<std::string> single_thr_md5_dec;
    single_thr_size_enc = size_enc_;
    single_thr_md5_enc = md5_enc_;
    single_thr_md5_dec = md5_dec_;
    size_enc_.clear();
    md5_enc_.clear();
    md5_dec_.clear();

    // Encode using multiple threads.
    cfg_.g_threads = 4;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    std::vector<size_t> multi_thr_size_enc;
    std::vector<std::string> multi_thr_md5_enc;
    std::vector<std::string> multi_thr_md5_dec;
    multi_thr_size_enc = size_enc_;
    multi_thr_md5_enc = md5_enc_;
    multi_thr_md5_dec = md5_dec_;
    size_enc_.clear();
    md5_enc_.clear();
    md5_dec_.clear();

    // Check that the vectors are equal.
    ASSERT_EQ(single_thr_size_enc, multi_thr_size_enc);
    ASSERT_EQ(single_thr_md5_enc, multi_thr_md5_enc);
    ASSERT_EQ(single_thr_md5_dec, multi_thr_md5_dec);

    // Encode using multiple threads row-mt enabled.
    row_mt_ = 1;
    cfg_.g_threads = 2;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    std::vector<size_t> multi_thr2_row_mt_size_enc;
    std::vector<std::string> multi_thr2_row_mt_md5_enc;
    std::vector<std::string> multi_thr2_row_mt_md5_dec;
    multi_thr2_row_mt_size_enc = size_enc_;
    multi_thr2_row_mt_md5_enc = md5_enc_;
    multi_thr2_row_mt_md5_dec = md5_dec_;
    size_enc_.clear();
    md5_enc_.clear();
    md5_dec_.clear();

    // Disable threads=3 test for now to reduce the time so that the nightly
    // test would not time out.
    // cfg_.g_threads = 3;
    // ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    // std::vector<size_t> multi_thr3_row_mt_size_enc;
    // std::vector<std::string> multi_thr3_row_mt_md5_enc;
    // std::vector<std::string> multi_thr3_row_mt_md5_dec;
    // multi_thr3_row_mt_size_enc = size_enc_;
    // multi_thr3_row_mt_md5_enc = md5_enc_;
    // multi_thr3_row_mt_md5_dec = md5_dec_;
    // size_enc_.clear();
    // md5_enc_.clear();
    // md5_dec_.clear();
    // Check that the vectors are equal.
    // ASSERT_EQ(multi_thr3_row_mt_size_enc, multi_thr2_row_mt_size_enc);
    // ASSERT_EQ(multi_thr3_row_mt_md5_enc, multi_thr2_row_mt_md5_enc);
    // ASSERT_EQ(multi_thr3_row_mt_md5_dec, multi_thr2_row_mt_md5_dec);

    cfg_.g_threads = 4;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    std::vector<size_t> multi_thr4_row_mt_size_enc;
    std::vector<std::string> multi_thr4_row_mt_md5_enc;
    std::vector<std::string> multi_thr4_row_mt_md5_dec;
    multi_thr4_row_mt_size_enc = size_enc_;
    multi_thr4_row_mt_md5_enc = md5_enc_;
    multi_thr4_row_mt_md5_dec = md5_dec_;
    size_enc_.clear();
    md5_enc_.clear();
    md5_dec_.clear();

    // Check that the vectors are equal.
    ASSERT_EQ(multi_thr4_row_mt_size_enc, multi_thr2_row_mt_size_enc);
    ASSERT_EQ(multi_thr4_row_mt_md5_enc, multi_thr2_row_mt_md5_enc);
    ASSERT_EQ(multi_thr4_row_mt_md5_dec, multi_thr2_row_mt_md5_dec);
  }

  bool encoder_initialized_;
  ::libaom_test::TestMode encoding_mode_;
  int set_cpu_used_;
  int tile_cols_;
  int tile_rows_;
  int row_mt_;
  ::libaom_test::Decoder *decoder_;
  std::vector<size_t> size_enc_;
  std::vector<std::string> md5_enc_;
  std::vector<std::string> md5_dec_;
};

TEST_P(AVxEncoderThreadTest, EncoderResultTest) {
  cfg_.large_scale_tile = 0;
  decoder_->Control(AV1_SET_TILE_MODE, 0);
  DoTest();
}

class AVxEncoderThreadTestLarge : public AVxEncoderThreadTest {};

TEST_P(AVxEncoderThreadTestLarge, EncoderResultTest) {
  cfg_.large_scale_tile = 0;
  decoder_->Control(AV1_SET_TILE_MODE, 0);
  DoTest();
}

// For AV1, only test speed 0 to 3.
// Here test cpu_used 2 and 3
AV1_INSTANTIATE_TEST_CASE(AVxEncoderThreadTest,
                          ::testing::Values(::libaom_test::kTwoPassGood),
                          ::testing::Range(2, 4), ::testing::Values(0, 2),
                          ::testing::Values(0, 1));

// Test cpu_used 0 and 1.
AV1_INSTANTIATE_TEST_CASE(AVxEncoderThreadTestLarge,
                          ::testing::Values(::libaom_test::kTwoPassGood,
                                            ::libaom_test::kOnePassGood),
                          ::testing::Range(0, 2), ::testing::Values(0, 1, 2, 6),
                          ::testing::Values(0, 1, 2, 6));

class AVxEncoderThreadLSTest : public AVxEncoderThreadTest {
  virtual void SetTileSize(libaom_test::Encoder *encoder) {
    encoder->Control(AV1E_SET_TILE_COLUMNS, tile_cols_);
    encoder->Control(AV1E_SET_TILE_ROWS, tile_rows_);
  }
};

TEST_P(AVxEncoderThreadLSTest, EncoderResultTest) {
  cfg_.large_scale_tile = 1;
  decoder_->Control(AV1_SET_TILE_MODE, 1);
  decoder_->Control(AV1D_EXT_TILE_DEBUG, 1);
  DoTest();
}

class AVxEncoderThreadLSTestLarge : public AVxEncoderThreadLSTest {};

TEST_P(AVxEncoderThreadLSTestLarge, EncoderResultTest) {
  cfg_.large_scale_tile = 1;
  decoder_->Control(AV1_SET_TILE_MODE, 1);
  decoder_->Control(AV1D_EXT_TILE_DEBUG, 1);
  DoTest();
}

AV1_INSTANTIATE_TEST_CASE(AVxEncoderThreadLSTestLarge,
                          ::testing::Values(::libaom_test::kTwoPassGood,
                                            ::libaom_test::kOnePassGood),
                          ::testing::Range(0, 4), ::testing::Values(0, 6),
                          ::testing::Values(0, 6));
}  // namespace
