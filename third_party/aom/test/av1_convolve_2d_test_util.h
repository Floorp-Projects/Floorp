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

#ifndef TEST_HIPREC_CONVOLVE_TEST_UTIL_H_
#define TEST_HIPREC_CONVOLVE_TEST_UTIL_H_

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/acm_random.h"
#include "test/util.h"
#include "./av1_rtcd.h"
#include "./aom_dsp_rtcd.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"

namespace libaom_test {

namespace AV1Convolve2D {

typedef void (*convolve_2d_func)(const uint8_t *src, int src_stride,
                                 CONV_BUF_TYPE *dst, int dst_stride, int w,
                                 int h, InterpFilterParams *filter_params_x,
                                 InterpFilterParams *filter_params_y,
                                 const int subpel_x_q4, const int subpel_y_q4,
                                 ConvolveParams *conv_params);

typedef std::tr1::tuple<int, int, convolve_2d_func> Convolve2DParam;

::testing::internal::ParamGenerator<Convolve2DParam> BuildParams(
    convolve_2d_func filter);

class AV1Convolve2DTest : public ::testing::TestWithParam<Convolve2DParam> {
 public:
  virtual ~AV1Convolve2DTest();
  virtual void SetUp();

  virtual void TearDown();

 protected:
  void RunCheckOutput(convolve_2d_func test_impl);

  libaom_test::ACMRandom rnd_;
};

}  // namespace AV1Convolve2D

#if CONFIG_HIGHBITDEPTH
namespace AV1HighbdConvolve2D {
typedef void (*highbd_convolve_2d_func)(
    const uint16_t *src, int src_stride, CONV_BUF_TYPE *dst, int dst_stride,
    int w, int h, InterpFilterParams *filter_params_x,
    InterpFilterParams *filter_params_y, const int subpel_x_q4,
    const int subpel_y_q4, ConvolveParams *conv_params, int bd);

typedef std::tr1::tuple<int, int, int, highbd_convolve_2d_func>
    HighbdConvolve2DParam;

::testing::internal::ParamGenerator<HighbdConvolve2DParam> BuildParams(
    highbd_convolve_2d_func filter);

class AV1HighbdConvolve2DTest
    : public ::testing::TestWithParam<HighbdConvolve2DParam> {
 public:
  virtual ~AV1HighbdConvolve2DTest();
  virtual void SetUp();

  virtual void TearDown();

 protected:
  void RunCheckOutput(highbd_convolve_2d_func test_impl);

  libaom_test::ACMRandom rnd_;
};

}  // namespace AV1HighbdConvolve2D
#endif  // CONFIG_HIGHBITDEPTH

}  // namespace libaom_test

#endif  // TEST_HIPREC_CONVOLVE_TEST_UTIL_H_
