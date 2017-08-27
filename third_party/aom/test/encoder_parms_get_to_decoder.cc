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

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "av1/av1_dx_iface.c"

namespace {

const int kCpuUsed = 2;

struct EncodePerfTestVideo {
  const char *name;
  uint32_t width;
  uint32_t height;
  uint32_t bitrate;
  int frames;
};

const EncodePerfTestVideo kAV1EncodePerfTestVectors[] = {
  { "niklas_1280_720_30.y4m", 1280, 720, 600, 10 },
};

struct EncodeParameters {
  int32_t tile_rows;
  int32_t tile_cols;
  int32_t lossless;
  int32_t error_resilient;
  int32_t frame_parallel;
  aom_color_range_t color_range;
  aom_color_space_t cs;
#if CONFIG_COLORSPACE_HEADERS
  aom_transfer_function_t tf;
  aom_chroma_sample_position_t csp;
#endif
  int render_size[2];
  // TODO(JBB): quantizers / bitrate
};

const EncodeParameters kAV1EncodeParameterSet[] = {
  { 0, 0, 0, 1, 0, AOM_CR_STUDIO_RANGE, AOM_CS_BT_601, { 0, 0 } },
  { 0, 0, 0, 0, 0, AOM_CR_FULL_RANGE, AOM_CS_BT_709, { 0, 0 } },
#if CONFIG_COLORSPACE_HEADERS
  { 0, 0, 1, 0, 0, AOM_CR_FULL_RANGE, AOM_CS_BT_2020_NCL, { 0, 0 } },
#else
  { 0, 0, 1, 0, 0, AOM_CR_FULL_RANGE, AOM_CS_BT_2020, { 0, 0 } },
#endif
  { 0, 2, 0, 0, 1, AOM_CR_STUDIO_RANGE, AOM_CS_UNKNOWN, { 640, 480 } },
  // TODO(JBB): Test profiles (requires more work).
};

class AvxEncoderParmsGetToDecoder
    : public ::libaom_test::CodecTestWith2Params<EncodeParameters,
                                                 EncodePerfTestVideo>,
      public ::libaom_test::EncoderTest,
{
 protected:
  AvxEncoderParmsGetToDecoder()
      : EncoderTest(GET_PARAM(0)), encode_parms(GET_PARAM(1)) {}

  virtual ~AvxEncoderParmsGetToDecoder() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(::libaom_test::kTwoPassGood);
    cfg_.g_lag_in_frames = 25;
    cfg_.g_error_resilient = encode_parms.error_resilient;
    dec_cfg_.threads = 4;
    test_video_ = GET_PARAM(2);
    cfg_.rc_target_bitrate = test_video_.bitrate;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 1) {
      encoder->Control(AV1E_SET_COLOR_SPACE, encode_parms.cs);
#if CONFIG_COLORSPACE_HEADERS
      encoder->Control(AV1E_SET_TRANSFER_FUNCTION, encode_parms.tf);
      encoder->Control(AV1E_SET_CHROMA_SAMPLE_POSITION, encode_parms.csp);
#endif
      encoder->Control(AV1E_SET_COLOR_RANGE, encode_parms.color_range);
      encoder->Control(AV1E_SET_LOSSLESS, encode_parms.lossless);
      encoder->Control(AV1E_SET_FRAME_PARALLEL_DECODING,
                       encode_parms.frame_parallel);
      encoder->Control(AV1E_SET_TILE_ROWS, encode_parms.tile_rows);
      encoder->Control(AV1E_SET_TILE_COLUMNS, encode_parms.tile_cols);
      encoder->Control(AOME_SET_CPUUSED, kCpuUsed);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
      encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
      if (encode_parms.render_size[0] > 0 && encode_parms.render_size[1] > 0)
        encoder->Control(AV1E_SET_RENDER_SIZE, encode_parms.render_size);
    }
  }

  virtual bool HandleDecodeResult(const aom_codec_err_t res_dec,
                                  libaom_test::Decoder *decoder) {
    aom_codec_ctx_t *const av1_decoder = decoder->GetDecoder();
    aom_codec_alg_priv_t *const priv =
        reinterpret_cast<aom_codec_alg_priv_t *>(av1_decoder->priv);
    FrameWorkerData *const worker_data =
        reinterpret_cast<FrameWorkerData *>(priv->frame_workers[0].data1);
    AV1_COMMON *const common = &worker_data->pbi->common;

    if (encode_parms.lossless) {
      EXPECT_EQ(0, common->base_qindex);
      EXPECT_EQ(0, common->y_dc_delta_q);
      EXPECT_EQ(0, common->uv_dc_delta_q);
      EXPECT_EQ(0, common->uv_ac_delta_q);
      EXPECT_EQ(ONLY_4X4, common->tx_mode);
    }
    EXPECT_EQ(encode_parms.error_resilient, common->error_resilient_mode);
    if (encode_parms.error_resilient) {
      EXPECT_EQ(0, common->use_prev_frame_mvs);
    }
    EXPECT_EQ(encode_parms.color_range, common->color_range);
    EXPECT_EQ(encode_parms.cs, common->color_space);
#if CONFIG_COLORSPACE_HEADERS
    EXPECT_EQ(encode_parms.tf, common->transfer_function);
    EXPECT_EQ(encode_parms.csp, common->chroma_sample_position);
#endif
    if (encode_parms.render_size[0] > 0 && encode_parms.render_size[1] > 0) {
      EXPECT_EQ(encode_parms.render_size[0], common->render_width);
      EXPECT_EQ(encode_parms.render_size[1], common->render_height);
    }
    EXPECT_EQ(encode_parms.tile_cols, common->log2_tile_cols);
    EXPECT_EQ(encode_parms.tile_rows, common->log2_tile_rows);

    EXPECT_EQ(AOM_CODEC_OK, res_dec) << decoder->DecodeError();
    return AOM_CODEC_OK == res_dec;
  }

  EncodePerfTestVideo test_video_;

 private:
  EncodeParameters encode_parms;
};

TEST_P(AvxEncoderParmsGetToDecoder, BitstreamParms) {
  init_flags_ = AOM_CODEC_USE_PSNR;

  testing::internal::scoped_ptr<libaom_test::VideoSource> video(
      new libaom_test::Y4mVideoSource(test_video_.name, 0, test_video_.frames));
  ASSERT_TRUE(video.get() != NULL);

  ASSERT_NO_FATAL_FAILURE(RunLoop(video.get()));
}

AV1_INSTANTIATE_TEST_CASE(AvxEncoderParmsGetToDecoder,
                          ::testing::ValuesIn(kAV1EncodeParameterSet),
                          ::testing::ValuesIn(kAV1EncodePerfTestVectors));
}  // namespace
