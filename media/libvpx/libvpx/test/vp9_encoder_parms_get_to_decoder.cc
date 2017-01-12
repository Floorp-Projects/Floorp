/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/y4m_video_source.h"
#include "test/yuv_video_source.h"
#include "test/util.h"
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "vp9/decoder/vp9_decoder.h"

typedef vpx_codec_stream_info_t vp9_stream_info_t;
struct vpx_codec_alg_priv {
  vpx_codec_priv_t        base;
  vpx_codec_dec_cfg_t     cfg;
  vp9_stream_info_t       si;
  struct VP9Decoder      *pbi;
  int                     postproc_cfg_set;
  vp8_postproc_cfg_t      postproc_cfg;
  vpx_decrypt_cb          decrypt_cb;
  void                   *decrypt_state;
  vpx_image_t             img;
  int                     img_avail;
  int                     flushed;
  int                     invert_tile_order;
  int                     frame_parallel_decode;

  // External frame buffer info to save for VP9 common.
  void *ext_priv;  // Private data associated with the external frame buffers.
  vpx_get_frame_buffer_cb_fn_t get_ext_fb_cb;
  vpx_release_frame_buffer_cb_fn_t release_ext_fb_cb;
};

static vpx_codec_alg_priv_t *get_alg_priv(vpx_codec_ctx_t *ctx) {
  return (vpx_codec_alg_priv_t *)ctx->priv;
}

namespace {

const unsigned int kFramerate = 50;
const int kCpuUsed = 2;

struct EncodePerfTestVideo {
  const char *name;
  uint32_t width;
  uint32_t height;
  uint32_t bitrate;
  int frames;
};

const EncodePerfTestVideo kVP9EncodePerfTestVectors[] = {
  {"niklas_1280_720_30.y4m", 1280, 720, 600, 10},
};

struct EncodeParameters {
  int32_t tile_rows;
  int32_t tile_cols;
  int32_t lossless;
  int32_t error_resilient;
  int32_t frame_parallel;
  vpx_color_space_t cs;
  // TODO(JBB): quantizers / bitrate
};

const EncodeParameters kVP9EncodeParameterSet[] = {
    {0, 0, 0, 1, 0, VPX_CS_BT_601},
    {0, 0, 0, 0, 0, VPX_CS_BT_709},
    {0, 0, 1, 0, 0, VPX_CS_BT_2020},
    {0, 2, 0, 0, 1, VPX_CS_UNKNOWN},
    // TODO(JBB): Test profiles (requires more work).
};

int is_extension_y4m(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if (!dot || dot == filename)
    return 0;
  else
    return !strcmp(dot, ".y4m");
}

class Vp9EncoderParmsGetToDecoder
    : public ::libvpx_test::EncoderTest,
      public ::libvpx_test::CodecTestWith2Params<EncodeParameters, \
                                                 EncodePerfTestVideo> {
 protected:
  Vp9EncoderParmsGetToDecoder()
      : EncoderTest(GET_PARAM(0)),
        encode_parms(GET_PARAM(1)) {
  }

  virtual ~Vp9EncoderParmsGetToDecoder() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(::libvpx_test::kTwoPassGood);
    cfg_.g_lag_in_frames = 25;
    cfg_.g_error_resilient = encode_parms.error_resilient;
    dec_cfg_.threads = 4;
    test_video_ = GET_PARAM(2);
    cfg_.rc_target_bitrate = test_video_.bitrate;
  }

  virtual void PreEncodeFrameHook(::libvpx_test::VideoSource *video,
                                  ::libvpx_test::Encoder *encoder) {
    if (video->frame() == 1) {
      encoder->Control(VP9E_SET_COLOR_SPACE, encode_parms.cs);
      encoder->Control(VP9E_SET_LOSSLESS, encode_parms.lossless);
      encoder->Control(VP9E_SET_FRAME_PARALLEL_DECODING,
                       encode_parms.frame_parallel);
      encoder->Control(VP9E_SET_TILE_ROWS, encode_parms.tile_rows);
      encoder->Control(VP9E_SET_TILE_COLUMNS, encode_parms.tile_cols);
      encoder->Control(VP8E_SET_CPUUSED, kCpuUsed);
      encoder->Control(VP8E_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(VP8E_SET_ARNR_MAXFRAMES, 7);
      encoder->Control(VP8E_SET_ARNR_STRENGTH, 5);
      encoder->Control(VP8E_SET_ARNR_TYPE, 3);
    }
  }

  virtual bool HandleDecodeResult(const vpx_codec_err_t res_dec,
                                  const libvpx_test::VideoSource& video,
                                  libvpx_test::Decoder *decoder) {
    vpx_codec_ctx_t* vp9_decoder = decoder->GetDecoder();
    vpx_codec_alg_priv_t* priv =
        (vpx_codec_alg_priv_t*) get_alg_priv(vp9_decoder);

    VP9Decoder* pbi = priv->pbi;
    VP9_COMMON* common = &pbi->common;

    if (encode_parms.lossless) {
      EXPECT_EQ(common->base_qindex, 0);
      EXPECT_EQ(common->y_dc_delta_q, 0);
      EXPECT_EQ(common->uv_dc_delta_q, 0);
      EXPECT_EQ(common->uv_ac_delta_q, 0);
      EXPECT_EQ(common->tx_mode, ONLY_4X4);
    }
    EXPECT_EQ(common->error_resilient_mode, encode_parms.error_resilient);
    if (encode_parms.error_resilient) {
      EXPECT_EQ(common->frame_parallel_decoding_mode, 1);
      EXPECT_EQ(common->use_prev_frame_mvs, 0);
    } else {
      EXPECT_EQ(common->frame_parallel_decoding_mode,
                encode_parms.frame_parallel);
    }
    EXPECT_EQ(common->color_space, encode_parms.cs);
    EXPECT_EQ(common->log2_tile_cols, encode_parms.tile_cols);
    EXPECT_EQ(common->log2_tile_rows, encode_parms.tile_rows);

    EXPECT_EQ(VPX_CODEC_OK, res_dec) << decoder->DecodeError();
    return VPX_CODEC_OK == res_dec;
  }

  EncodePerfTestVideo test_video_;

 private:
  EncodeParameters encode_parms;
};

// TODO(hkuang): This test conflicts with frame parallel decode. So disable it
// for now until fix.
TEST_P(Vp9EncoderParmsGetToDecoder, DISABLED_BitstreamParms) {
  init_flags_ = VPX_CODEC_USE_PSNR;

  libvpx_test::VideoSource *video;
  if (is_extension_y4m(test_video_.name)) {
    video = new libvpx_test::Y4mVideoSource(test_video_.name,
                                            0, test_video_.frames);
  } else {
    video = new libvpx_test::YUVVideoSource(test_video_.name,
                                            VPX_IMG_FMT_I420,
                                            test_video_.width,
                                            test_video_.height,
                                            kFramerate, 1, 0,
                                            test_video_.frames);
  }

  ASSERT_NO_FATAL_FAILURE(RunLoop(video));
  delete(video);
}

VP9_INSTANTIATE_TEST_CASE(
    Vp9EncoderParmsGetToDecoder,
    ::testing::ValuesIn(kVP9EncodeParameterSet),
    ::testing::ValuesIn(kVP9EncodePerfTestVectors));

}  // namespace
