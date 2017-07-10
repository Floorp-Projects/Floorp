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
#include "aom_dsp/ans.h"
#include "av1/av1_dx_iface.c"

// A note on ANS_MAX_SYMBOLS == 0:
// Fused gtest doesn't work with EXPECT_FATAL_FAILURE [1]. Just run with a
// single iteration and don't try to check the window size if we are unwindowed.
// [1] https://github.com/google/googletest/issues/356

namespace {

const char kTestVideoName[] = "niklas_1280_720_30.y4m";
const int kTestVideoFrames = 10;

class AnsCodecTest : public ::libaom_test::CodecTestWithParam<int>,
                     public ::libaom_test::EncoderTest {
 protected:
  AnsCodecTest()
      : EncoderTest(GET_PARAM(0)), ans_window_size_log2_(GET_PARAM(1)) {}

  virtual ~AnsCodecTest() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(::libaom_test::kOnePassGood);
    cfg_.g_lag_in_frames = 25;
    cfg_.rc_end_usage = AOM_CQ;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 1) {
#if ANS_MAX_SYMBOLS
      encoder->Control(AV1E_SET_ANS_WINDOW_SIZE_LOG2, ans_window_size_log2_);
#endif
      // Try to push a high symbol count through the codec
      encoder->Control(AOME_SET_CQ_LEVEL, 8);
      encoder->Control(AOME_SET_CPUUSED, 2);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
      encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
      encoder->Control(AV1E_SET_TILE_COLUMNS, 0);
      encoder->Control(AV1E_SET_TILE_ROWS, 0);
    }
  }

  virtual bool HandleDecodeResult(const aom_codec_err_t res_dec,
                                  libaom_test::Decoder *decoder) {
    aom_codec_ctx_t *const av1_decoder = decoder->GetDecoder();
#if ANS_MAX_SYMBOLS
    aom_codec_alg_priv_t *const priv =
        reinterpret_cast<aom_codec_alg_priv_t *>(av1_decoder->priv);
    FrameWorkerData *const worker_data =
        reinterpret_cast<FrameWorkerData *>(priv->frame_workers[0].data1);
    AV1_COMMON *const common = &worker_data->pbi->common;

    EXPECT_EQ(ans_window_size_log2_, common->ans_window_size_log2);
#endif

    EXPECT_EQ(AOM_CODEC_OK, res_dec) << decoder->DecodeError();
    return AOM_CODEC_OK == res_dec;
  }

 private:
  int ans_window_size_log2_;
};

TEST_P(AnsCodecTest, BitstreamParms) {
  testing::internal::scoped_ptr<libaom_test::VideoSource> video(
      new libaom_test::Y4mVideoSource(kTestVideoName, 0, kTestVideoFrames));
  ASSERT_TRUE(video.get() != NULL);

  ASSERT_NO_FATAL_FAILURE(RunLoop(video.get()));
}

#if ANS_MAX_SYMBOLS
AV1_INSTANTIATE_TEST_CASE(AnsCodecTest, ::testing::Range(8, 24));
#else
AV1_INSTANTIATE_TEST_CASE(AnsCodecTest, ::testing::Range(0, 1));
#endif
}  // namespace
