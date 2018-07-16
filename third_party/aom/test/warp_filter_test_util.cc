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
#include "aom_ports/aom_timer.h"
#include "test/warp_filter_test_util.h"

using ::testing::make_tuple;
using ::testing::tuple;

namespace libaom_test {

int32_t random_warped_param(libaom_test::ACMRandom *rnd, int bits) {
  // 1 in 8 chance of generating zero (arbitrarily chosen)
  if (((rnd->Rand8()) & 7) == 0) return 0;
  // Otherwise, enerate uniform values in the range
  // [-(1 << bits), 1] U [1, 1<<bits]
  int32_t v = 1 + (rnd->Rand16() & ((1 << bits) - 1));
  if ((rnd->Rand8()) & 1) return -v;
  return v;
}

void generate_warped_model(libaom_test::ACMRandom *rnd, int32_t *mat,
                           int16_t *alpha, int16_t *beta, int16_t *gamma,
                           int16_t *delta) {
  while (1) {
    mat[0] = random_warped_param(rnd, WARPEDMODEL_PREC_BITS + 6);
    mat[1] = random_warped_param(rnd, WARPEDMODEL_PREC_BITS + 6);
    mat[2] = (random_warped_param(rnd, WARPEDMODEL_PREC_BITS - 3)) +
             (1 << WARPEDMODEL_PREC_BITS);
    mat[3] = random_warped_param(rnd, WARPEDMODEL_PREC_BITS - 3);
    // 50/50 chance of generating ROTZOOM vs. AFFINE models
    if (rnd->Rand8() & 1) {
      // AFFINE
      mat[4] = random_warped_param(rnd, WARPEDMODEL_PREC_BITS - 3);
      mat[5] = (random_warped_param(rnd, WARPEDMODEL_PREC_BITS - 3)) +
               (1 << WARPEDMODEL_PREC_BITS);
    } else {
      mat[4] = -mat[3];
      mat[5] = mat[2];
    }

    // Calculate the derived parameters and check that they are suitable
    // for the warp filter.
    assert(mat[2] != 0);

    *alpha = clamp(mat[2] - (1 << WARPEDMODEL_PREC_BITS), INT16_MIN, INT16_MAX);
    *beta = clamp(mat[3], INT16_MIN, INT16_MAX);
    *gamma = clamp(((int64_t)mat[4] * (1 << WARPEDMODEL_PREC_BITS)) / mat[2],
                   INT16_MIN, INT16_MAX);
    *delta =
        clamp(mat[5] - (((int64_t)mat[3] * mat[4] + (mat[2] / 2)) / mat[2]) -
                  (1 << WARPEDMODEL_PREC_BITS),
              INT16_MIN, INT16_MAX);

    if ((4 * abs(*alpha) + 7 * abs(*beta) >= (1 << WARPEDMODEL_PREC_BITS)) ||
        (4 * abs(*gamma) + 4 * abs(*delta) >= (1 << WARPEDMODEL_PREC_BITS)))
      continue;

    *alpha = ROUND_POWER_OF_TWO_SIGNED(*alpha, WARP_PARAM_REDUCE_BITS) *
             (1 << WARP_PARAM_REDUCE_BITS);
    *beta = ROUND_POWER_OF_TWO_SIGNED(*beta, WARP_PARAM_REDUCE_BITS) *
            (1 << WARP_PARAM_REDUCE_BITS);
    *gamma = ROUND_POWER_OF_TWO_SIGNED(*gamma, WARP_PARAM_REDUCE_BITS) *
             (1 << WARP_PARAM_REDUCE_BITS);
    *delta = ROUND_POWER_OF_TWO_SIGNED(*delta, WARP_PARAM_REDUCE_BITS) *
             (1 << WARP_PARAM_REDUCE_BITS);

    // We have a valid model, so finish
    return;
  }
}

namespace AV1WarpFilter {
::testing::internal::ParamGenerator<WarpTestParam> BuildParams(
    warp_affine_func filter) {
  const WarpTestParam params[] = {
    make_tuple(4, 4, 50000, filter),  make_tuple(8, 8, 50000, filter),
    make_tuple(64, 64, 1000, filter), make_tuple(4, 16, 20000, filter),
    make_tuple(32, 8, 10000, filter),
  };
  return ::testing::ValuesIn(params);
}

AV1WarpFilterTest::~AV1WarpFilterTest() {}
void AV1WarpFilterTest::SetUp() { rnd_.Reset(ACMRandom::DeterministicSeed()); }

void AV1WarpFilterTest::TearDown() { libaom_test::ClearSystemState(); }

void AV1WarpFilterTest::RunSpeedTest(warp_affine_func test_impl) {
  const int w = 128, h = 128;
  const int border = 16;
  const int stride = w + 2 * border;
  const int out_w = GET_PARAM(0), out_h = GET_PARAM(1);
  int sub_x, sub_y;
  const int bd = 8;

  uint8_t *input_ = new uint8_t[h * stride];
  uint8_t *input = input_ + border;

  // The warp functions always write rows with widths that are multiples of 8.
  // So to avoid a buffer overflow, we may need to pad rows to a multiple of 8.
  int output_n = ((out_w + 7) & ~7) * out_h;
  uint8_t *output = new uint8_t[output_n];
  int32_t mat[8];
  int16_t alpha, beta, gamma, delta;
  ConvolveParams conv_params = get_conv_params(0, 0, 0, bd);
  CONV_BUF_TYPE *dsta = new CONV_BUF_TYPE[output_n];

  generate_warped_model(&rnd_, mat, &alpha, &beta, &gamma, &delta);

  for (int r = 0; r < h; ++r)
    for (int c = 0; c < w; ++c) input[r * stride + c] = rnd_.Rand8();
  for (int r = 0; r < h; ++r) {
    memset(input + r * stride - border, input[r * stride], border);
    memset(input + r * stride + w, input[r * stride + (w - 1)], border);
  }

  sub_x = 0;
  sub_y = 0;
  int do_average = 0;

  conv_params = get_conv_params_no_round(0, do_average, 0, dsta, out_w, 1, bd);
  conv_params.use_jnt_comp_avg = 0;

  const int num_loops = 1000000000 / (out_w + out_h);
  aom_usec_timer timer;
  aom_usec_timer_start(&timer);
  for (int i = 0; i < num_loops; ++i)
    test_impl(mat, input, w, h, stride, output, 32, 32, out_w, out_h, out_w,
              sub_x, sub_y, &conv_params, alpha, beta, gamma, delta);

  aom_usec_timer_mark(&timer);
  const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
  printf("warp %3dx%-3d: %7.2f ns\n", out_w, out_h,
         1000.0 * elapsed_time / num_loops);

  delete[] input_;
  delete[] output;
  delete[] dsta;
}

void AV1WarpFilterTest::RunCheckOutput(warp_affine_func test_impl) {
  const int w = 128, h = 128;
  const int border = 16;
  const int stride = w + 2 * border;
  const int out_w = GET_PARAM(0), out_h = GET_PARAM(1);
  const int num_iters = GET_PARAM(2);
  int i, j, sub_x, sub_y;
  const int bd = 8;

  // The warp functions always write rows with widths that are multiples of 8.
  // So to avoid a buffer overflow, we may need to pad rows to a multiple of 8.
  int output_n = ((out_w + 7) & ~7) * out_h;
  uint8_t *input_ = new uint8_t[h * stride];
  uint8_t *input = input_ + border;
  uint8_t *output = new uint8_t[output_n];
  uint8_t *output2 = new uint8_t[output_n];
  int32_t mat[8];
  int16_t alpha, beta, gamma, delta;
  ConvolveParams conv_params = get_conv_params(0, 0, 0, bd);
  CONV_BUF_TYPE *dsta = new CONV_BUF_TYPE[output_n];
  CONV_BUF_TYPE *dstb = new CONV_BUF_TYPE[output_n];
  for (int i = 0; i < output_n; ++i) output[i] = output2[i] = rnd_.Rand8();

  for (i = 0; i < num_iters; ++i) {
    // Generate an input block and extend its borders horizontally
    for (int r = 0; r < h; ++r)
      for (int c = 0; c < w; ++c) input[r * stride + c] = rnd_.Rand8();
    for (int r = 0; r < h; ++r) {
      memset(input + r * stride - border, input[r * stride], border);
      memset(input + r * stride + w, input[r * stride + (w - 1)], border);
    }
    const int use_no_round = rnd_.Rand8() & 1;
    for (sub_x = 0; sub_x < 2; ++sub_x)
      for (sub_y = 0; sub_y < 2; ++sub_y) {
        generate_warped_model(&rnd_, mat, &alpha, &beta, &gamma, &delta);
        for (int ii = 0; ii < 2; ++ii) {
          for (int jj = 0; jj < 5; ++jj) {
            for (int do_average = 0; do_average <= 1; ++do_average) {
              if (use_no_round) {
                conv_params = get_conv_params_no_round(0, do_average, 0, dsta,
                                                       out_w, 1, bd);
              } else {
                conv_params = get_conv_params(0, 0, 0, bd);
              }
              if (jj >= 4) {
                conv_params.use_jnt_comp_avg = 0;
              } else {
                conv_params.use_jnt_comp_avg = 1;
                conv_params.fwd_offset = quant_dist_lookup_table[ii][jj][0];
                conv_params.bck_offset = quant_dist_lookup_table[ii][jj][1];
              }
              av1_warp_affine_c(mat, input, w, h, stride, output, 32, 32, out_w,
                                out_h, out_w, sub_x, sub_y, &conv_params, alpha,
                                beta, gamma, delta);
              if (use_no_round) {
                conv_params = get_conv_params_no_round(0, do_average, 0, dstb,
                                                       out_w, 1, bd);
              }
              if (jj >= 4) {
                conv_params.use_jnt_comp_avg = 0;
              } else {
                conv_params.use_jnt_comp_avg = 1;
                conv_params.fwd_offset = quant_dist_lookup_table[ii][jj][0];
                conv_params.bck_offset = quant_dist_lookup_table[ii][jj][1];
              }
              test_impl(mat, input, w, h, stride, output2, 32, 32, out_w, out_h,
                        out_w, sub_x, sub_y, &conv_params, alpha, beta, gamma,
                        delta);
              if (use_no_round) {
                for (j = 0; j < out_w * out_h; ++j)
                  ASSERT_EQ(dsta[j], dstb[j])
                      << "Pixel mismatch at index " << j << " = ("
                      << (j % out_w) << ", " << (j / out_w) << ") on iteration "
                      << i;
                for (j = 0; j < out_w * out_h; ++j)
                  ASSERT_EQ(output[j], output2[j])
                      << "Pixel mismatch at index " << j << " = ("
                      << (j % out_w) << ", " << (j / out_w) << ") on iteration "
                      << i;
              } else {
                for (j = 0; j < out_w * out_h; ++j)
                  ASSERT_EQ(output[j], output2[j])
                      << "Pixel mismatch at index " << j << " = ("
                      << (j % out_w) << ", " << (j / out_w) << ") on iteration "
                      << i;
              }
            }
          }
        }
      }
  }
  delete[] input_;
  delete[] output;
  delete[] output2;
  delete[] dsta;
  delete[] dstb;
}
}  // namespace AV1WarpFilter

namespace AV1HighbdWarpFilter {
::testing::internal::ParamGenerator<HighbdWarpTestParam> BuildParams(
    highbd_warp_affine_func filter) {
  const HighbdWarpTestParam params[] = {
    make_tuple(4, 4, 100, 8, filter),    make_tuple(8, 8, 100, 8, filter),
    make_tuple(64, 64, 100, 8, filter),  make_tuple(4, 16, 100, 8, filter),
    make_tuple(32, 8, 100, 8, filter),   make_tuple(4, 4, 100, 10, filter),
    make_tuple(8, 8, 100, 10, filter),   make_tuple(64, 64, 100, 10, filter),
    make_tuple(4, 16, 100, 10, filter),  make_tuple(32, 8, 100, 10, filter),
    make_tuple(4, 4, 100, 12, filter),   make_tuple(8, 8, 100, 12, filter),
    make_tuple(64, 64, 100, 12, filter), make_tuple(4, 16, 100, 12, filter),
    make_tuple(32, 8, 100, 12, filter),
  };
  return ::testing::ValuesIn(params);
}

AV1HighbdWarpFilterTest::~AV1HighbdWarpFilterTest() {}
void AV1HighbdWarpFilterTest::SetUp() {
  rnd_.Reset(ACMRandom::DeterministicSeed());
}

void AV1HighbdWarpFilterTest::TearDown() { libaom_test::ClearSystemState(); }

void AV1HighbdWarpFilterTest::RunSpeedTest(highbd_warp_affine_func test_impl) {
  const int w = 128, h = 128;
  const int border = 16;
  const int stride = w + 2 * border;
  const int out_w = GET_PARAM(0), out_h = GET_PARAM(1);
  const int bd = GET_PARAM(3);
  const int mask = (1 << bd) - 1;
  int sub_x, sub_y;

  // The warp functions always write rows with widths that are multiples of 8.
  // So to avoid a buffer overflow, we may need to pad rows to a multiple of 8.
  int output_n = ((out_w + 7) & ~7) * out_h;
  uint16_t *input_ = new uint16_t[h * stride];
  uint16_t *input = input_ + border;
  uint16_t *output = new uint16_t[output_n];
  int32_t mat[8];
  int16_t alpha, beta, gamma, delta;
  ConvolveParams conv_params = get_conv_params(0, 0, 0, bd);
  CONV_BUF_TYPE *dsta = new CONV_BUF_TYPE[output_n];

  generate_warped_model(&rnd_, mat, &alpha, &beta, &gamma, &delta);
  // Generate an input block and extend its borders horizontally
  for (int r = 0; r < h; ++r)
    for (int c = 0; c < w; ++c) input[r * stride + c] = rnd_.Rand16() & mask;
  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < border; ++c) {
      input[r * stride - border + c] = input[r * stride];
      input[r * stride + w + c] = input[r * stride + (w - 1)];
    }
  }

  sub_x = 0;
  sub_y = 0;
  int do_average = 0;
  conv_params.use_jnt_comp_avg = 0;
  conv_params = get_conv_params_no_round(0, do_average, 0, dsta, out_w, 1, bd);

  const int num_loops = 1000000000 / (out_w + out_h);
  aom_usec_timer timer;
  aom_usec_timer_start(&timer);

  for (int i = 0; i < num_loops; ++i)
    test_impl(mat, input, w, h, stride, output, 32, 32, out_w, out_h, out_w,
              sub_x, sub_y, bd, &conv_params, alpha, beta, gamma, delta);

  aom_usec_timer_mark(&timer);
  const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
  printf("highbd warp %3dx%-3d: %7.2f ns\n", out_w, out_h,
         1000.0 * elapsed_time / num_loops);

  delete[] input_;
  delete[] output;
  delete[] dsta;
}

void AV1HighbdWarpFilterTest::RunCheckOutput(
    highbd_warp_affine_func test_impl) {
  const int w = 128, h = 128;
  const int border = 16;
  const int stride = w + 2 * border;
  const int out_w = GET_PARAM(0), out_h = GET_PARAM(1);
  const int num_iters = GET_PARAM(2);
  const int bd = GET_PARAM(3);
  const int mask = (1 << bd) - 1;
  int i, j, sub_x, sub_y;

  // The warp functions always write rows with widths that are multiples of 8.
  // So to avoid a buffer overflow, we may need to pad rows to a multiple of 8.
  int output_n = ((out_w + 7) & ~7) * out_h;
  uint16_t *input_ = new uint16_t[h * stride];
  uint16_t *input = input_ + border;
  uint16_t *output = new uint16_t[output_n];
  uint16_t *output2 = new uint16_t[output_n];
  int32_t mat[8];
  int16_t alpha, beta, gamma, delta;
  ConvolveParams conv_params = get_conv_params(0, 0, 0, bd);
  CONV_BUF_TYPE *dsta = new CONV_BUF_TYPE[output_n];
  CONV_BUF_TYPE *dstb = new CONV_BUF_TYPE[output_n];
  for (int i = 0; i < output_n; ++i) output[i] = output2[i] = rnd_.Rand16();

  for (i = 0; i < num_iters; ++i) {
    // Generate an input block and extend its borders horizontally
    for (int r = 0; r < h; ++r)
      for (int c = 0; c < w; ++c) input[r * stride + c] = rnd_.Rand16() & mask;
    for (int r = 0; r < h; ++r) {
      for (int c = 0; c < border; ++c) {
        input[r * stride - border + c] = input[r * stride];
        input[r * stride + w + c] = input[r * stride + (w - 1)];
      }
    }
    const int use_no_round = rnd_.Rand8() & 1;
    for (sub_x = 0; sub_x < 2; ++sub_x)
      for (sub_y = 0; sub_y < 2; ++sub_y) {
        generate_warped_model(&rnd_, mat, &alpha, &beta, &gamma, &delta);
        for (int ii = 0; ii < 2; ++ii) {
          for (int jj = 0; jj < 5; ++jj) {
            for (int do_average = 0; do_average <= 1; ++do_average) {
              if (use_no_round) {
                conv_params = get_conv_params_no_round(0, do_average, 0, dsta,
                                                       out_w, 1, bd);
              } else {
                conv_params = get_conv_params(0, 0, 0, bd);
              }
              if (jj >= 4) {
                conv_params.use_jnt_comp_avg = 0;
              } else {
                conv_params.use_jnt_comp_avg = 1;
                conv_params.fwd_offset = quant_dist_lookup_table[ii][jj][0];
                conv_params.bck_offset = quant_dist_lookup_table[ii][jj][1];
              }

              av1_highbd_warp_affine_c(mat, input, w, h, stride, output, 32, 32,
                                       out_w, out_h, out_w, sub_x, sub_y, bd,
                                       &conv_params, alpha, beta, gamma, delta);
              if (use_no_round) {
                // TODO(angiebird): Change this to test_impl once we have SIMD
                // implementation
                conv_params = get_conv_params_no_round(0, do_average, 0, dstb,
                                                       out_w, 1, bd);
              }
              if (jj >= 4) {
                conv_params.use_jnt_comp_avg = 0;
              } else {
                conv_params.use_jnt_comp_avg = 1;
                conv_params.fwd_offset = quant_dist_lookup_table[ii][jj][0];
                conv_params.bck_offset = quant_dist_lookup_table[ii][jj][1];
              }
              test_impl(mat, input, w, h, stride, output2, 32, 32, out_w, out_h,
                        out_w, sub_x, sub_y, bd, &conv_params, alpha, beta,
                        gamma, delta);

              if (use_no_round) {
                for (j = 0; j < out_w * out_h; ++j)
                  ASSERT_EQ(dsta[j], dstb[j])
                      << "Pixel mismatch at index " << j << " = ("
                      << (j % out_w) << ", " << (j / out_w) << ") on iteration "
                      << i;
                for (j = 0; j < out_w * out_h; ++j)
                  ASSERT_EQ(output[j], output2[j])
                      << "Pixel mismatch at index " << j << " = ("
                      << (j % out_w) << ", " << (j / out_w) << ") on iteration "
                      << i;
              } else {
                for (j = 0; j < out_w * out_h; ++j)
                  ASSERT_EQ(output[j], output2[j])
                      << "Pixel mismatch at index " << j << " = ("
                      << (j % out_w) << ", " << (j / out_w) << ") on iteration "
                      << i;
              }
            }
          }
        }
      }
  }

  delete[] input_;
  delete[] output;
  delete[] output2;
  delete[] dsta;
  delete[] dstb;
}
}  // namespace AV1HighbdWarpFilter
}  // namespace libaom_test
