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

#include "test/comp_avg_pred_test.h"

using ::testing::make_tuple;
using ::testing::tuple;
using libaom_test::ACMRandom;
using libaom_test::AV1JNTCOMPAVG::AV1HighBDJNTCOMPAVGTest;
using libaom_test::AV1JNTCOMPAVG::AV1HighBDJNTCOMPAVGUPSAMPLEDTest;
using libaom_test::AV1JNTCOMPAVG::AV1JNTCOMPAVGTest;
using libaom_test::AV1JNTCOMPAVG::AV1JNTCOMPAVGUPSAMPLEDTest;

namespace {

TEST_P(AV1JNTCOMPAVGTest, DISABLED_Speed) { RunSpeedTest(GET_PARAM(0)); }

TEST_P(AV1JNTCOMPAVGTest, CheckOutput) { RunCheckOutput(GET_PARAM(0)); }

#if HAVE_SSSE3
INSTANTIATE_TEST_CASE_P(
    SSSE3, AV1JNTCOMPAVGTest,
    libaom_test::AV1JNTCOMPAVG::BuildParams(aom_jnt_comp_avg_pred_ssse3));
#endif

TEST_P(AV1JNTCOMPAVGUPSAMPLEDTest, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(0));
}

TEST_P(AV1JNTCOMPAVGUPSAMPLEDTest, CheckOutput) {
  RunCheckOutput(GET_PARAM(0));
}

#if HAVE_SSSE3
INSTANTIATE_TEST_CASE_P(SSSE3, AV1JNTCOMPAVGUPSAMPLEDTest,
                        libaom_test::AV1JNTCOMPAVG::BuildParams(
                            aom_jnt_comp_avg_upsampled_pred_ssse3));
#endif

TEST_P(AV1HighBDJNTCOMPAVGTest, DISABLED_Speed) { RunSpeedTest(GET_PARAM(1)); }

TEST_P(AV1HighBDJNTCOMPAVGTest, CheckOutput) { RunCheckOutput(GET_PARAM(1)); }

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, AV1HighBDJNTCOMPAVGTest,
    libaom_test::AV1JNTCOMPAVG::BuildParams(aom_highbd_jnt_comp_avg_pred_sse2));
#endif

TEST_P(AV1HighBDJNTCOMPAVGUPSAMPLEDTest, DISABLED_Speed) {
  RunSpeedTest(GET_PARAM(1));
}

TEST_P(AV1HighBDJNTCOMPAVGUPSAMPLEDTest, CheckOutput) {
  RunCheckOutput(GET_PARAM(1));
}

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(SSE2, AV1HighBDJNTCOMPAVGUPSAMPLEDTest,
                        libaom_test::AV1JNTCOMPAVG::BuildParams(
                            aom_highbd_jnt_comp_avg_upsampled_pred_sse2));
#endif

}  // namespace
