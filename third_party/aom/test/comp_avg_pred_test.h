/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef TEST_COMP_AVG_PRED_TEST_H_
#define TEST_COMP_AVG_PRED_TEST_H_

#include "config/aom_dsp_rtcd.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/acm_random.h"
#include "test/util.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "av1/common/common_data.h"
#include "aom_ports/aom_timer.h"

namespace libaom_test {
const int kMaxSize = 128 + 32;  // padding

namespace AV1JNTCOMPAVG {

typedef void (*jntcompavg_func)(uint8_t *comp_pred, const uint8_t *pred,
                                int width, int height, const uint8_t *ref,
                                int ref_stride,
                                const JNT_COMP_PARAMS *jcp_param);

typedef void (*jntcompavgupsampled_func)(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
    int ref_stride, const JNT_COMP_PARAMS *jcp_param);

typedef void (*highbdjntcompavg_func)(uint16_t *comp_pred, const uint8_t *pred8,
                                      int width, int height,
                                      const uint8_t *ref8, int ref_stride,
                                      const JNT_COMP_PARAMS *jcp_param);

typedef void (*highbdjntcompavgupsampled_func)(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint16_t *comp_pred, const uint8_t *pred8, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8,
    int ref_stride, int bd, const JNT_COMP_PARAMS *jcp_param);

typedef ::testing::tuple<jntcompavg_func, BLOCK_SIZE> JNTCOMPAVGParam;

typedef ::testing::tuple<jntcompavgupsampled_func, BLOCK_SIZE>
    JNTCOMPAVGUPSAMPLEDParam;

typedef ::testing::tuple<int, highbdjntcompavg_func, BLOCK_SIZE>
    HighbdJNTCOMPAVGParam;

typedef ::testing::tuple<int, highbdjntcompavgupsampled_func, BLOCK_SIZE>
    HighbdJNTCOMPAVGUPSAMPLEDParam;

::testing::internal::ParamGenerator<JNTCOMPAVGParam> BuildParams(
    jntcompavg_func filter) {
  return ::testing::Combine(::testing::Values(filter),
                            ::testing::Range(BLOCK_4X4, BLOCK_SIZES_ALL));
}

::testing::internal::ParamGenerator<JNTCOMPAVGUPSAMPLEDParam> BuildParams(
    jntcompavgupsampled_func filter) {
  return ::testing::Combine(::testing::Values(filter),
                            ::testing::Range(BLOCK_4X4, BLOCK_SIZES_ALL));
}

::testing::internal::ParamGenerator<HighbdJNTCOMPAVGParam> BuildParams(
    highbdjntcompavg_func filter) {
  return ::testing::Combine(::testing::Range(8, 13, 2),
                            ::testing::Values(filter),
                            ::testing::Range(BLOCK_4X4, BLOCK_SIZES_ALL));
}

::testing::internal::ParamGenerator<HighbdJNTCOMPAVGUPSAMPLEDParam> BuildParams(
    highbdjntcompavgupsampled_func filter) {
  return ::testing::Combine(::testing::Range(8, 13, 2),
                            ::testing::Values(filter),
                            ::testing::Range(BLOCK_4X4, BLOCK_SIZES_ALL));
}

class AV1JNTCOMPAVGTest : public ::testing::TestWithParam<JNTCOMPAVGParam> {
 public:
  ~AV1JNTCOMPAVGTest() {}
  void SetUp() { rnd_.Reset(ACMRandom::DeterministicSeed()); }
  void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  void RunCheckOutput(jntcompavg_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(1);

    uint8_t pred8[kMaxSize * kMaxSize];
    uint8_t ref8[kMaxSize * kMaxSize];
    uint8_t output[kMaxSize * kMaxSize];
    uint8_t output2[kMaxSize * kMaxSize];

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand8();
        ref8[i * w + j] = rnd_.Rand8();
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    JNT_COMP_PARAMS jnt_comp_params;
    jnt_comp_params.use_jnt_comp_avg = 1;

    for (int ii = 0; ii < 2; ii++) {
      for (int jj = 0; jj < 4; jj++) {
        jnt_comp_params.fwd_offset = quant_dist_lookup_table[ii][jj][0];
        jnt_comp_params.bck_offset = quant_dist_lookup_table[ii][jj][1];

        const int offset_r = 3 + rnd_.PseudoUniform(h - in_h - 7);
        const int offset_c = 3 + rnd_.PseudoUniform(w - in_w - 7);
        aom_jnt_comp_avg_pred_c(output, pred8 + offset_r * w + offset_c, in_w,
                                in_h, ref8 + offset_r * w + offset_c, in_w,
                                &jnt_comp_params);
        test_impl(output2, pred8 + offset_r * w + offset_c, in_w, in_h,
                  ref8 + offset_r * w + offset_c, in_w, &jnt_comp_params);

        for (int i = 0; i < in_h; ++i) {
          for (int j = 0; j < in_w; ++j) {
            int idx = i * in_w + j;
            ASSERT_EQ(output[idx], output2[idx])
                << "Mismatch at unit tests for AV1JNTCOMPAVGTest\n"
                << in_w << "x" << in_h << " Pixel mismatch at index " << idx
                << " = (" << i << ", " << j << ")";
          }
        }
      }
    }
  }
  void RunSpeedTest(jntcompavg_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(1);

    uint8_t pred8[kMaxSize * kMaxSize];
    uint8_t ref8[kMaxSize * kMaxSize];
    uint8_t output[kMaxSize * kMaxSize];
    uint8_t output2[kMaxSize * kMaxSize];

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand8();
        ref8[i * w + j] = rnd_.Rand8();
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    JNT_COMP_PARAMS jnt_comp_params;
    jnt_comp_params.use_jnt_comp_avg = 1;

    jnt_comp_params.fwd_offset = quant_dist_lookup_table[0][0][0];
    jnt_comp_params.bck_offset = quant_dist_lookup_table[0][0][1];

    const int num_loops = 1000000000 / (in_w + in_h);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);

    for (int i = 0; i < num_loops; ++i)
      aom_jnt_comp_avg_pred_c(output, pred8, in_w, in_h, ref8, in_w,
                              &jnt_comp_params);

    aom_usec_timer_mark(&timer);
    const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
    printf("jntcompavg c_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time / num_loops);

    aom_usec_timer timer1;
    aom_usec_timer_start(&timer1);

    for (int i = 0; i < num_loops; ++i)
      test_impl(output2, pred8, in_w, in_h, ref8, in_w, &jnt_comp_params);

    aom_usec_timer_mark(&timer1);
    const int elapsed_time1 = static_cast<int>(aom_usec_timer_elapsed(&timer1));
    printf("jntcompavg test_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time1 / num_loops);
  }

  libaom_test::ACMRandom rnd_;
};  // class AV1JNTCOMPAVGTest

class AV1JNTCOMPAVGUPSAMPLEDTest
    : public ::testing::TestWithParam<JNTCOMPAVGUPSAMPLEDParam> {
 public:
  ~AV1JNTCOMPAVGUPSAMPLEDTest() {}
  void SetUp() { rnd_.Reset(ACMRandom::DeterministicSeed()); }
  void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  void RunCheckOutput(jntcompavgupsampled_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(1);

    uint8_t pred8[kMaxSize * kMaxSize];
    uint8_t ref8[kMaxSize * kMaxSize];
    DECLARE_ALIGNED(16, uint8_t, output[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(16, uint8_t, output2[MAX_SB_SQUARE]);

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand8();
        ref8[i * w + j] = rnd_.Rand8();
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    JNT_COMP_PARAMS jnt_comp_params;
    jnt_comp_params.use_jnt_comp_avg = 1;
    int sub_x_q3, sub_y_q3;
    for (sub_x_q3 = 0; sub_x_q3 < 8; ++sub_x_q3) {
      for (sub_y_q3 = 0; sub_y_q3 < 8; ++sub_y_q3) {
        for (int ii = 0; ii < 2; ii++) {
          for (int jj = 0; jj < 4; jj++) {
            jnt_comp_params.fwd_offset = quant_dist_lookup_table[ii][jj][0];
            jnt_comp_params.bck_offset = quant_dist_lookup_table[ii][jj][1];

            const int offset_r = 3 + rnd_.PseudoUniform(h - in_h - 7);
            const int offset_c = 3 + rnd_.PseudoUniform(w - in_w - 7);

            aom_jnt_comp_avg_upsampled_pred_c(
                NULL, NULL, 0, 0, NULL, output, pred8 + offset_r * w + offset_c,
                in_w, in_h, sub_x_q3, sub_y_q3, ref8 + offset_r * w + offset_c,
                in_w, &jnt_comp_params);
            test_impl(NULL, NULL, 0, 0, NULL, output2,
                      pred8 + offset_r * w + offset_c, in_w, in_h, sub_x_q3,
                      sub_y_q3, ref8 + offset_r * w + offset_c, in_w,
                      &jnt_comp_params);

            for (int i = 0; i < in_h; ++i) {
              for (int j = 0; j < in_w; ++j) {
                int idx = i * in_w + j;
                ASSERT_EQ(output[idx], output2[idx])
                    << "Mismatch at unit tests for AV1JNTCOMPAVGUPSAMPLEDTest\n"
                    << in_w << "x" << in_h << " Pixel mismatch at index " << idx
                    << " = (" << i << ", " << j << "), sub pixel offset = ("
                    << sub_y_q3 << ", " << sub_x_q3 << ")";
              }
            }
          }
        }
      }
    }
  }
  void RunSpeedTest(jntcompavgupsampled_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(1);

    uint8_t pred8[kMaxSize * kMaxSize];
    uint8_t ref8[kMaxSize * kMaxSize];
    DECLARE_ALIGNED(16, uint8_t, output[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(16, uint8_t, output2[MAX_SB_SQUARE]);

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand8();
        ref8[i * w + j] = rnd_.Rand8();
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    JNT_COMP_PARAMS jnt_comp_params;
    jnt_comp_params.use_jnt_comp_avg = 1;

    jnt_comp_params.fwd_offset = quant_dist_lookup_table[0][0][0];
    jnt_comp_params.bck_offset = quant_dist_lookup_table[0][0][1];

    int sub_x_q3 = 0;
    int sub_y_q3 = 0;

    const int num_loops = 1000000000 / (in_w + in_h);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);

    for (int i = 0; i < num_loops; ++i)
      aom_jnt_comp_avg_upsampled_pred_c(NULL, NULL, 0, 0, NULL, output, pred8,
                                        in_w, in_h, sub_x_q3, sub_y_q3, ref8,
                                        in_w, &jnt_comp_params);

    aom_usec_timer_mark(&timer);
    const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
    printf("jntcompavgupsampled c_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time / num_loops);

    aom_usec_timer timer1;
    aom_usec_timer_start(&timer1);

    for (int i = 0; i < num_loops; ++i)
      test_impl(NULL, NULL, 0, 0, NULL, output2, pred8, in_w, in_h, sub_x_q3,
                sub_y_q3, ref8, in_w, &jnt_comp_params);

    aom_usec_timer_mark(&timer1);
    const int elapsed_time1 = static_cast<int>(aom_usec_timer_elapsed(&timer1));
    printf("jntcompavgupsampled test_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time1 / num_loops);
  }

  libaom_test::ACMRandom rnd_;
};  // class AV1JNTCOMPAVGUPSAMPLEDTest

class AV1HighBDJNTCOMPAVGTest
    : public ::testing::TestWithParam<HighbdJNTCOMPAVGParam> {
 public:
  ~AV1HighBDJNTCOMPAVGTest() {}
  void SetUp() { rnd_.Reset(ACMRandom::DeterministicSeed()); }

  void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  void RunCheckOutput(highbdjntcompavg_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(2);
    const int bd = GET_PARAM(0);
    uint16_t pred8[kMaxSize * kMaxSize];
    uint16_t ref8[kMaxSize * kMaxSize];
    uint16_t output[kMaxSize * kMaxSize];
    uint16_t output2[kMaxSize * kMaxSize];

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
        ref8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    JNT_COMP_PARAMS jnt_comp_params;
    jnt_comp_params.use_jnt_comp_avg = 1;

    for (int ii = 0; ii < 2; ii++) {
      for (int jj = 0; jj < 4; jj++) {
        jnt_comp_params.fwd_offset = quant_dist_lookup_table[ii][jj][0];
        jnt_comp_params.bck_offset = quant_dist_lookup_table[ii][jj][1];

        const int offset_r = 3 + rnd_.PseudoUniform(h - in_h - 7);
        const int offset_c = 3 + rnd_.PseudoUniform(w - in_w - 7);
        aom_highbd_jnt_comp_avg_pred_c(
            output, CONVERT_TO_BYTEPTR(pred8) + offset_r * w + offset_c, in_w,
            in_h, CONVERT_TO_BYTEPTR(ref8) + offset_r * w + offset_c, in_w,
            &jnt_comp_params);
        test_impl(output2, CONVERT_TO_BYTEPTR(pred8) + offset_r * w + offset_c,
                  in_w, in_h,
                  CONVERT_TO_BYTEPTR(ref8) + offset_r * w + offset_c, in_w,
                  &jnt_comp_params);

        for (int i = 0; i < in_h; ++i) {
          for (int j = 0; j < in_w; ++j) {
            int idx = i * in_w + j;
            ASSERT_EQ(output[idx], output2[idx])
                << "Mismatch at unit tests for AV1HighBDJNTCOMPAVGTest\n"
                << in_w << "x" << in_h << " Pixel mismatch at index " << idx
                << " = (" << i << ", " << j << ")";
          }
        }
      }
    }
  }
  void RunSpeedTest(highbdjntcompavg_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(2);
    const int bd = GET_PARAM(0);
    uint16_t pred8[kMaxSize * kMaxSize];
    uint16_t ref8[kMaxSize * kMaxSize];
    uint16_t output[kMaxSize * kMaxSize];
    uint16_t output2[kMaxSize * kMaxSize];

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
        ref8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    JNT_COMP_PARAMS jnt_comp_params;
    jnt_comp_params.use_jnt_comp_avg = 1;

    jnt_comp_params.fwd_offset = quant_dist_lookup_table[0][0][0];
    jnt_comp_params.bck_offset = quant_dist_lookup_table[0][0][1];

    const int num_loops = 1000000000 / (in_w + in_h);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);

    for (int i = 0; i < num_loops; ++i)
      aom_highbd_jnt_comp_avg_pred_c(output, CONVERT_TO_BYTEPTR(pred8), in_w,
                                     in_h, CONVERT_TO_BYTEPTR(ref8), in_w,
                                     &jnt_comp_params);

    aom_usec_timer_mark(&timer);
    const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
    printf("highbdjntcompavg c_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time / num_loops);

    aom_usec_timer timer1;
    aom_usec_timer_start(&timer1);

    for (int i = 0; i < num_loops; ++i)
      test_impl(output2, CONVERT_TO_BYTEPTR(pred8), in_w, in_h,
                CONVERT_TO_BYTEPTR(ref8), in_w, &jnt_comp_params);

    aom_usec_timer_mark(&timer1);
    const int elapsed_time1 = static_cast<int>(aom_usec_timer_elapsed(&timer1));
    printf("highbdjntcompavg test_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time1 / num_loops);
  }

  libaom_test::ACMRandom rnd_;
};  // class AV1HighBDJNTCOMPAVGTest

class AV1HighBDJNTCOMPAVGUPSAMPLEDTest
    : public ::testing::TestWithParam<HighbdJNTCOMPAVGUPSAMPLEDParam> {
 public:
  ~AV1HighBDJNTCOMPAVGUPSAMPLEDTest() {}
  void SetUp() { rnd_.Reset(ACMRandom::DeterministicSeed()); }
  void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  void RunCheckOutput(highbdjntcompavgupsampled_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(2);
    const int bd = GET_PARAM(0);
    uint16_t pred8[kMaxSize * kMaxSize];
    uint16_t ref8[kMaxSize * kMaxSize];
    uint16_t output[kMaxSize * kMaxSize];
    uint16_t output2[kMaxSize * kMaxSize];

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
        ref8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    JNT_COMP_PARAMS jnt_comp_params;
    jnt_comp_params.use_jnt_comp_avg = 1;
    int sub_x_q3, sub_y_q3;

    for (sub_x_q3 = 0; sub_x_q3 < 8; ++sub_x_q3) {
      for (sub_y_q3 = 0; sub_y_q3 < 8; ++sub_y_q3) {
        for (int ii = 0; ii < 2; ii++) {
          for (int jj = 0; jj < 4; jj++) {
            jnt_comp_params.fwd_offset = quant_dist_lookup_table[ii][jj][0];
            jnt_comp_params.bck_offset = quant_dist_lookup_table[ii][jj][1];

            const int offset_r = 3 + rnd_.PseudoUniform(h - in_h - 7);
            const int offset_c = 3 + rnd_.PseudoUniform(w - in_w - 7);

            aom_highbd_jnt_comp_avg_upsampled_pred_c(
                NULL, NULL, 0, 0, NULL, output,
                CONVERT_TO_BYTEPTR(pred8) + offset_r * w + offset_c, in_w, in_h,
                sub_x_q3, sub_y_q3,
                CONVERT_TO_BYTEPTR(ref8) + offset_r * w + offset_c, in_w, bd,
                &jnt_comp_params);
            test_impl(NULL, NULL, 0, 0, NULL, output2,
                      CONVERT_TO_BYTEPTR(pred8) + offset_r * w + offset_c, in_w,
                      in_h, sub_x_q3, sub_y_q3,
                      CONVERT_TO_BYTEPTR(ref8) + offset_r * w + offset_c, in_w,
                      bd, &jnt_comp_params);

            for (int i = 0; i < in_h; ++i) {
              for (int j = 0; j < in_w; ++j) {
                int idx = i * in_w + j;
                ASSERT_EQ(output[idx], output2[idx])
                    << "Mismatch at unit tests for "
                       "AV1HighBDJNTCOMPAVGUPSAMPLEDTest\n"
                    << in_w << "x" << in_h << " Pixel mismatch at index " << idx
                    << " = (" << i << ", " << j << "), sub pixel offset = ("
                    << sub_y_q3 << ", " << sub_x_q3 << ")";
              }
            }
          }
        }
      }
    }
  }
  void RunSpeedTest(highbdjntcompavgupsampled_func test_impl) {
    const int w = kMaxSize, h = kMaxSize;
    const int block_idx = GET_PARAM(2);
    const int bd = GET_PARAM(0);
    uint16_t pred8[kMaxSize * kMaxSize];
    uint16_t ref8[kMaxSize * kMaxSize];
    uint16_t output[kMaxSize * kMaxSize];
    uint16_t output2[kMaxSize * kMaxSize];

    for (int i = 0; i < h; ++i)
      for (int j = 0; j < w; ++j) {
        pred8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
        ref8[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);
      }
    const int in_w = block_size_wide[block_idx];
    const int in_h = block_size_high[block_idx];

    JNT_COMP_PARAMS jnt_comp_params;
    jnt_comp_params.use_jnt_comp_avg = 1;

    jnt_comp_params.fwd_offset = quant_dist_lookup_table[0][0][0];
    jnt_comp_params.bck_offset = quant_dist_lookup_table[0][0][1];
    int sub_x_q3 = 0;
    int sub_y_q3 = 0;
    const int num_loops = 1000000000 / (in_w + in_h);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);

    for (int i = 0; i < num_loops; ++i)
      aom_highbd_jnt_comp_avg_upsampled_pred_c(
          NULL, NULL, 0, 0, NULL, output, CONVERT_TO_BYTEPTR(pred8), in_w, in_h,
          sub_x_q3, sub_y_q3, CONVERT_TO_BYTEPTR(ref8), in_w, bd,
          &jnt_comp_params);

    aom_usec_timer_mark(&timer);
    const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
    printf("highbdjntcompavgupsampled c_code %3dx%-3d: %7.2f us\n", in_w, in_h,
           1000.0 * elapsed_time / num_loops);

    aom_usec_timer timer1;
    aom_usec_timer_start(&timer1);

    for (int i = 0; i < num_loops; ++i)
      test_impl(NULL, NULL, 0, 0, NULL, output2, CONVERT_TO_BYTEPTR(pred8),
                in_w, in_h, sub_x_q3, sub_y_q3, CONVERT_TO_BYTEPTR(ref8), in_w,
                bd, &jnt_comp_params);

    aom_usec_timer_mark(&timer1);
    const int elapsed_time1 = static_cast<int>(aom_usec_timer_elapsed(&timer1));
    printf("highbdjntcompavgupsampled test_code %3dx%-3d: %7.2f us\n", in_w,
           in_h, 1000.0 * elapsed_time1 / num_loops);
  }

  libaom_test::ACMRandom rnd_;
};  // class AV1HighBDJNTCOMPAVGUPSAMPLEDTest

}  // namespace AV1JNTCOMPAVG
}  // namespace libaom_test

#endif  // TEST_COMP_AVG_PRED_TEST_H_
