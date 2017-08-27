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
#include "test/warp_filter_test_util.h"

using std::tr1::tuple;
using std::tr1::make_tuple;
using libaom_test::ACMRandom;
using libaom_test::AV1WarpFilter::AV1WarpFilterTest;
#if CONFIG_HIGHBITDEPTH
using libaom_test::AV1HighbdWarpFilter::AV1HighbdWarpFilterTest;
#endif

namespace {

TEST_P(AV1WarpFilterTest, CheckOutput) { RunCheckOutput(GET_PARAM(3)); }

INSTANTIATE_TEST_CASE_P(
    SSE2, AV1WarpFilterTest,
    libaom_test::AV1WarpFilter::BuildParams(av1_warp_affine_sse2));

#if HAVE_SSSE3
INSTANTIATE_TEST_CASE_P(
    SSSE3, AV1WarpFilterTest,
    libaom_test::AV1WarpFilter::BuildParams(av1_warp_affine_ssse3));
#endif

#if CONFIG_HIGHBITDEPTH && HAVE_SSSE3
TEST_P(AV1HighbdWarpFilterTest, CheckOutput) {
  RunCheckOutput(av1_highbd_warp_affine_ssse3);
}

INSTANTIATE_TEST_CASE_P(SSSE3, AV1HighbdWarpFilterTest,
                        libaom_test::AV1HighbdWarpFilter::GetDefaultParams());
#endif

}  // namespace
