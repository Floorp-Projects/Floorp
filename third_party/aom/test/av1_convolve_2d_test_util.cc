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

#include "test/av1_convolve_2d_test_util.h"

#include "av1/common/convolve.h"

using std::tr1::tuple;
using std::tr1::make_tuple;

namespace libaom_test {

namespace AV1Convolve2D {

::testing::internal::ParamGenerator<Convolve2DParam> BuildParams(
    convolve_2d_func filter) {
  const Convolve2DParam params[] = {
    make_tuple(4, 4, filter),   make_tuple(8, 8, filter),
    make_tuple(64, 64, filter), make_tuple(4, 16, filter),
    make_tuple(32, 8, filter),
  };
  return ::testing::ValuesIn(params);
}

AV1Convolve2DTest::~AV1Convolve2DTest() {}
void AV1Convolve2DTest::SetUp() { rnd_.Reset(ACMRandom::DeterministicSeed()); }

void AV1Convolve2DTest::TearDown() { libaom_test::ClearSystemState(); }

void AV1Convolve2DTest::RunCheckOutput(convolve_2d_func test_impl) {
  const int w = 128, h = 128;
  const int out_w = GET_PARAM(0), out_h = GET_PARAM(1);
  int i, j, k;

  uint8_t *input = new uint8_t[h * w];

  int output_n = out_h * MAX_SB_SIZE;
  CONV_BUF_TYPE *output = new CONV_BUF_TYPE[output_n];
  CONV_BUF_TYPE *output2 = new CONV_BUF_TYPE[output_n];

  for (i = 0; i < h; ++i)
    for (j = 0; j < w; ++j) input[i * w + j] = rnd_.Rand8();

  int hfilter, vfilter, subx, suby;
  for (hfilter = EIGHTTAP_REGULAR; hfilter < INTERP_FILTERS_ALL; ++hfilter) {
    for (vfilter = EIGHTTAP_REGULAR; vfilter < INTERP_FILTERS_ALL; ++vfilter) {
      InterpFilterParams filter_params_x =
          av1_get_interp_filter_params((InterpFilter)hfilter);
      InterpFilterParams filter_params_y =
          av1_get_interp_filter_params((InterpFilter)vfilter);
      const int do_average = rnd_.Rand8() & 1;
      ConvolveParams conv_params1 =
          get_conv_params_no_round(0, do_average, 0, output, MAX_SB_SIZE);
      ConvolveParams conv_params2 =
          get_conv_params_no_round(0, do_average, 0, output2, MAX_SB_SIZE);

      for (subx = 0; subx < 16; ++subx)
        for (suby = 0; suby < 16; ++suby) {
          // av1_convolve_2d is designed for accumulate two predicted blocks for
          // compound mode, so we set num_iter to two here.
          // A larger number may introduce overflow
          const int num_iters = 2;
          memset(output, 0, output_n * sizeof(*output));
          memset(output2, 0, output_n * sizeof(*output2));
          for (i = 0; i < num_iters; ++i) {
            // Choose random locations within the source block
            int offset_r = 3 + rnd_.PseudoUniform(h - out_h - 7);
            int offset_c = 3 + rnd_.PseudoUniform(w - out_w - 7);
            av1_convolve_2d_c(input + offset_r * w + offset_c, w, output,
                              MAX_SB_SIZE, out_w, out_h, &filter_params_x,
                              &filter_params_y, subx, suby, &conv_params1);
            test_impl(input + offset_r * w + offset_c, w, output2, MAX_SB_SIZE,
                      out_w, out_h, &filter_params_x, &filter_params_y, subx,
                      suby, &conv_params2);

            for (j = 0; j < out_h; ++j)
              for (k = 0; k < out_w; ++k) {
                int idx = j * MAX_SB_SIZE + k;
                ASSERT_EQ(output[idx], output2[idx])
                    << "Pixel mismatch at index " << idx << " = (" << j << ", "
                    << k << "), sub pixel offset = (" << suby << ", " << subx
                    << ")";
              }
          }
        }
    }
  }
  delete[] input;
  delete[] output;
  delete[] output2;
}
}  // namespace AV1Convolve2D

#if CONFIG_HIGHBITDEPTH
namespace AV1HighbdConvolve2D {

::testing::internal::ParamGenerator<HighbdConvolve2DParam> BuildParams(
    highbd_convolve_2d_func filter) {
  const HighbdConvolve2DParam params[] = {
    make_tuple(4, 4, 8, filter),    make_tuple(8, 8, 8, filter),
    make_tuple(64, 64, 8, filter),  make_tuple(4, 16, 8, filter),
    make_tuple(32, 8, 8, filter),   make_tuple(4, 4, 10, filter),
    make_tuple(8, 8, 10, filter),   make_tuple(64, 64, 10, filter),
    make_tuple(4, 16, 10, filter),  make_tuple(32, 8, 10, filter),
    make_tuple(4, 4, 12, filter),   make_tuple(8, 8, 12, filter),
    make_tuple(64, 64, 12, filter), make_tuple(4, 16, 12, filter),
    make_tuple(32, 8, 12, filter),
  };
  return ::testing::ValuesIn(params);
}

AV1HighbdConvolve2DTest::~AV1HighbdConvolve2DTest() {}
void AV1HighbdConvolve2DTest::SetUp() {
  rnd_.Reset(ACMRandom::DeterministicSeed());
}

void AV1HighbdConvolve2DTest::TearDown() { libaom_test::ClearSystemState(); }

void AV1HighbdConvolve2DTest::RunCheckOutput(
    highbd_convolve_2d_func test_impl) {
  const int w = 128, h = 128;
  const int out_w = GET_PARAM(0), out_h = GET_PARAM(1);
  const int bd = GET_PARAM(2);
  int i, j, k;

  uint16_t *input = new uint16_t[h * w];

  int output_n = out_h * MAX_SB_SIZE;
  CONV_BUF_TYPE *output = new CONV_BUF_TYPE[output_n];
  CONV_BUF_TYPE *output2 = new CONV_BUF_TYPE[output_n];

  for (i = 0; i < h; ++i)
    for (j = 0; j < w; ++j) input[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);

  int hfilter, vfilter, subx, suby;
  for (hfilter = EIGHTTAP_REGULAR; hfilter < INTERP_FILTERS_ALL; ++hfilter) {
    for (vfilter = EIGHTTAP_REGULAR; vfilter < INTERP_FILTERS_ALL; ++vfilter) {
      InterpFilterParams filter_params_x =
          av1_get_interp_filter_params((InterpFilter)hfilter);
      InterpFilterParams filter_params_y =
          av1_get_interp_filter_params((InterpFilter)vfilter);
      ConvolveParams conv_params1 =
          get_conv_params_no_round(0, 0, 0, output, MAX_SB_SIZE);
      ConvolveParams conv_params2 =
          get_conv_params_no_round(0, 0, 0, output2, MAX_SB_SIZE);

      for (subx = 0; subx < 16; ++subx)
        for (suby = 0; suby < 16; ++suby) {
          // av1_convolve_2d is designed for accumulate two predicted blocks for
          // compound mode, so we set num_iter to two here.
          // A larger number may introduce overflow
          const int num_iters = 2;
          memset(output, 0, output_n * sizeof(*output));
          memset(output2, 0, output_n * sizeof(*output2));
          for (i = 0; i < num_iters; ++i) {
            // Choose random locations within the source block
            int offset_r = 3 + rnd_.PseudoUniform(h - out_h - 7);
            int offset_c = 3 + rnd_.PseudoUniform(w - out_w - 7);
            av1_highbd_convolve_2d_c(input + offset_r * w + offset_c, w, output,
                                     MAX_SB_SIZE, out_w, out_h,
                                     &filter_params_x, &filter_params_y, subx,
                                     suby, &conv_params1, bd);
            test_impl(input + offset_r * w + offset_c, w, output2, MAX_SB_SIZE,
                      out_w, out_h, &filter_params_x, &filter_params_y, subx,
                      suby, &conv_params2, bd);

            for (j = 0; j < out_h; ++j)
              for (k = 0; k < out_w; ++k) {
                int idx = j * MAX_SB_SIZE + k;
                ASSERT_EQ(output[idx], output2[idx])
                    << "Pixel mismatch at index " << idx << " = (" << j << ", "
                    << k << "), sub pixel offset = (" << suby << ", " << subx
                    << ")";
              }
          }
        }
    }
  }
  delete[] input;
  delete[] output;
  delete[] output2;
}
}  // namespace AV1HighbdConvolve2D
#endif  // CONFIG_HIGHBITDEPTH
}  // namespace libaom_test
