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

#include "test/hiprec_convolve_test_util.h"

#include "av1/common/restoration.h"

using std::tr1::tuple;
using std::tr1::make_tuple;

namespace libaom_test {

// Generate a random pair of filter kernels, using the ranges
// of possible values from the loop-restoration experiment
static void generate_kernels(ACMRandom *rnd, InterpKernel hkernel,
                             InterpKernel vkernel) {
  hkernel[0] = hkernel[6] =
      WIENER_FILT_TAP0_MINV +
      rnd->PseudoUniform(WIENER_FILT_TAP0_MAXV + 1 - WIENER_FILT_TAP0_MINV);
  hkernel[1] = hkernel[5] =
      WIENER_FILT_TAP1_MINV +
      rnd->PseudoUniform(WIENER_FILT_TAP1_MAXV + 1 - WIENER_FILT_TAP1_MINV);
  hkernel[2] = hkernel[4] =
      WIENER_FILT_TAP2_MINV +
      rnd->PseudoUniform(WIENER_FILT_TAP2_MAXV + 1 - WIENER_FILT_TAP2_MINV);
  hkernel[3] = -(hkernel[0] + hkernel[1] + hkernel[2]);
  hkernel[7] = 0;

  vkernel[0] = vkernel[6] =
      WIENER_FILT_TAP0_MINV +
      rnd->PseudoUniform(WIENER_FILT_TAP0_MAXV + 1 - WIENER_FILT_TAP0_MINV);
  vkernel[1] = vkernel[5] =
      WIENER_FILT_TAP1_MINV +
      rnd->PseudoUniform(WIENER_FILT_TAP1_MAXV + 1 - WIENER_FILT_TAP1_MINV);
  vkernel[2] = vkernel[4] =
      WIENER_FILT_TAP2_MINV +
      rnd->PseudoUniform(WIENER_FILT_TAP2_MAXV + 1 - WIENER_FILT_TAP2_MINV);
  vkernel[3] = -(vkernel[0] + vkernel[1] + vkernel[2]);
  vkernel[7] = 0;
}

namespace AV1HiprecConvolve {

::testing::internal::ParamGenerator<HiprecConvolveParam> BuildParams(
    hiprec_convolve_func filter) {
  const HiprecConvolveParam params[] = {
    make_tuple(8, 8, 50000, filter), make_tuple(64, 64, 1000, filter),
    make_tuple(32, 8, 10000, filter),
  };
  return ::testing::ValuesIn(params);
}

AV1HiprecConvolveTest::~AV1HiprecConvolveTest() {}
void AV1HiprecConvolveTest::SetUp() {
  rnd_.Reset(ACMRandom::DeterministicSeed());
}

void AV1HiprecConvolveTest::TearDown() { libaom_test::ClearSystemState(); }

void AV1HiprecConvolveTest::RunCheckOutput(hiprec_convolve_func test_impl) {
  const int w = 128, h = 128;
  const int out_w = GET_PARAM(0), out_h = GET_PARAM(1);
  const int num_iters = GET_PARAM(2);
  int i, j;

  uint8_t *input_ = new uint8_t[h * w];
  uint8_t *input = input_;

  // The convolve functions always write rows with widths that are multiples of
  // 8.
  // So to avoid a buffer overflow, we may need to pad rows to a multiple of 8.
  int output_n = ((out_w + 7) & ~7) * out_h;
  uint8_t *output = new uint8_t[output_n];
  uint8_t *output2 = new uint8_t[output_n];

  // Generate random filter kernels
  DECLARE_ALIGNED(16, InterpKernel, hkernel);
  DECLARE_ALIGNED(16, InterpKernel, vkernel);

  generate_kernels(&rnd_, hkernel, vkernel);

  for (i = 0; i < h; ++i)
    for (j = 0; j < w; ++j) input[i * w + j] = rnd_.Rand8();

  for (i = 0; i < num_iters; ++i) {
    // Choose random locations within the source block
    int offset_r = 3 + rnd_.PseudoUniform(h - out_h - 7);
    int offset_c = 3 + rnd_.PseudoUniform(w - out_w - 7);
    aom_convolve8_add_src_hip_c(input + offset_r * w + offset_c, w, output,
                                out_w, hkernel, 16, vkernel, 16, out_w, out_h);
    test_impl(input + offset_r * w + offset_c, w, output2, out_w, hkernel, 16,
              vkernel, 16, out_w, out_h);

    for (j = 0; j < out_w * out_h; ++j)
      ASSERT_EQ(output[j], output2[j])
          << "Pixel mismatch at index " << j << " = (" << (j % out_w) << ", "
          << (j / out_w) << ") on iteration " << i;
  }
  delete[] input_;
  delete[] output;
  delete[] output2;
}
}  // namespace AV1HiprecConvolve

#if CONFIG_HIGHBITDEPTH
namespace AV1HighbdHiprecConvolve {

::testing::internal::ParamGenerator<HighbdHiprecConvolveParam> BuildParams(
    highbd_hiprec_convolve_func filter) {
  const HighbdHiprecConvolveParam params[] = {
    make_tuple(8, 8, 50000, 8, filter),   make_tuple(64, 64, 1000, 8, filter),
    make_tuple(32, 8, 10000, 8, filter),  make_tuple(8, 8, 50000, 10, filter),
    make_tuple(64, 64, 1000, 10, filter), make_tuple(32, 8, 10000, 10, filter),
    make_tuple(8, 8, 50000, 12, filter),  make_tuple(64, 64, 1000, 12, filter),
    make_tuple(32, 8, 10000, 12, filter),
  };
  return ::testing::ValuesIn(params);
}

AV1HighbdHiprecConvolveTest::~AV1HighbdHiprecConvolveTest() {}
void AV1HighbdHiprecConvolveTest::SetUp() {
  rnd_.Reset(ACMRandom::DeterministicSeed());
}

void AV1HighbdHiprecConvolveTest::TearDown() {
  libaom_test::ClearSystemState();
}

void AV1HighbdHiprecConvolveTest::RunCheckOutput(
    highbd_hiprec_convolve_func test_impl) {
  const int w = 128, h = 128;
  const int out_w = GET_PARAM(0), out_h = GET_PARAM(1);
  const int num_iters = GET_PARAM(2);
  const int bd = GET_PARAM(3);
  int i, j;

  uint16_t *input = new uint16_t[h * w];

  // The convolve functions always write rows with widths that are multiples of
  // 8.
  // So to avoid a buffer overflow, we may need to pad rows to a multiple of 8.
  int output_n = ((out_w + 7) & ~7) * out_h;
  uint16_t *output = new uint16_t[output_n];
  uint16_t *output2 = new uint16_t[output_n];

  // Generate random filter kernels
  DECLARE_ALIGNED(16, InterpKernel, hkernel);
  DECLARE_ALIGNED(16, InterpKernel, vkernel);

  generate_kernels(&rnd_, hkernel, vkernel);

  for (i = 0; i < h; ++i)
    for (j = 0; j < w; ++j) input[i * w + j] = rnd_.Rand16() & ((1 << bd) - 1);

  uint8_t *input_ptr = CONVERT_TO_BYTEPTR(input);
  uint8_t *output_ptr = CONVERT_TO_BYTEPTR(output);
  uint8_t *output2_ptr = CONVERT_TO_BYTEPTR(output2);

  for (i = 0; i < num_iters; ++i) {
    // Choose random locations within the source block
    int offset_r = 3 + rnd_.PseudoUniform(h - out_h - 7);
    int offset_c = 3 + rnd_.PseudoUniform(w - out_w - 7);
    aom_highbd_convolve8_add_src_hip_c(input_ptr + offset_r * w + offset_c, w,
                                       output_ptr, out_w, hkernel, 16, vkernel,
                                       16, out_w, out_h, bd);
    test_impl(input_ptr + offset_r * w + offset_c, w, output2_ptr, out_w,
              hkernel, 16, vkernel, 16, out_w, out_h, bd);

    for (j = 0; j < out_w * out_h; ++j)
      ASSERT_EQ(output[j], output2[j])
          << "Pixel mismatch at index " << j << " = (" << (j % out_w) << ", "
          << (j / out_w) << ") on iteration " << i;
  }
  delete[] input;
  delete[] output;
  delete[] output2;
}
}  // namespace AV1HighbdHiprecConvolve
#endif  // CONFIG_HIGHBITDEPTH
}  // namespace libaom_test
