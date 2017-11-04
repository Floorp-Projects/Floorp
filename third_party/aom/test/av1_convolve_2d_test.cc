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
#include "test/av1_convolve_2d_test_util.h"

using std::tr1::tuple;
using std::tr1::make_tuple;
using libaom_test::ACMRandom;
using libaom_test::AV1Convolve2D::AV1Convolve2DTest;
#if CONFIG_HIGHBITDEPTH
using libaom_test::AV1HighbdConvolve2D::AV1HighbdConvolve2DTest;
#endif

namespace {

TEST_P(AV1Convolve2DTest, CheckOutput) { RunCheckOutput(GET_PARAM(2)); }

INSTANTIATE_TEST_CASE_P(
    SSE2, AV1Convolve2DTest,
    libaom_test::AV1Convolve2D::BuildParams(av1_convolve_2d_sse2));

#if CONFIG_HIGHBITDEPTH && HAVE_SSSE3
TEST_P(AV1HighbdConvolve2DTest, CheckOutput) { RunCheckOutput(GET_PARAM(3)); }

INSTANTIATE_TEST_CASE_P(SSSE3, AV1HighbdConvolve2DTest,
                        libaom_test::AV1HighbdConvolve2D::BuildParams(
                            av1_highbd_convolve_2d_ssse3));

#endif

}  // namespace
