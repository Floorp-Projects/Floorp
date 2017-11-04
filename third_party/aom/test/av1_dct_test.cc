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

#include <math.h>
#include <stdlib.h>
#include <new>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/acm_random.h"
#include "test/util.h"
#include "./aom_config.h"
#include "aom_ports/msvc.h"

#undef CONFIG_COEFFICIENT_RANGE_CHECKING
#define CONFIG_COEFFICIENT_RANGE_CHECKING 1
#define AV1_DCT_GTEST
#include "av1/encoder/dct.c"
#if CONFIG_DAALA_DCT4 || CONFIG_DAALA_DCT8 || CONFIG_DAALA_DCT16 || \
    CONFIG_DAALA_DCT32
#include "av1/common/daala_tx.c"
#endif

using libaom_test::ACMRandom;

namespace {
void reference_dct_1d(const double *in, double *out, int size) {
  const double kInvSqrt2 = 0.707106781186547524400844362104;
  for (int k = 0; k < size; ++k) {
    out[k] = 0;
    for (int n = 0; n < size; ++n) {
      out[k] += in[n] * cos(PI * (2 * n + 1) * k / (2 * size));
    }
    if (k == 0) out[k] = out[k] * kInvSqrt2;
  }
}

typedef void (*FdctFuncRef)(const double *in, double *out, int size);
typedef void (*IdctFuncRef)(const double *in, double *out, int size);
typedef void (*FdctFunc)(const tran_low_t *in, tran_low_t *out);
typedef void (*IdctFunc)(const tran_low_t *in, tran_low_t *out);

class TransTestBase {
 public:
  virtual ~TransTestBase() {}

 protected:
  void RunFwdAccuracyCheck() {
    tran_low_t *input = new tran_low_t[txfm_size_];
    tran_low_t *output = new tran_low_t[txfm_size_];
    double *ref_input = new double[txfm_size_];
    double *ref_output = new double[txfm_size_];

    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 5000;
    for (int ti = 0; ti < count_test_block; ++ti) {
      for (int ni = 0; ni < txfm_size_; ++ni) {
        input[ni] = rnd.Rand8() - rnd.Rand8();
        ref_input[ni] = static_cast<double>(input[ni]);
      }

      fwd_txfm_(input, output);
      fwd_txfm_ref_(ref_input, ref_output, txfm_size_);

      for (int ni = 0; ni < txfm_size_; ++ni) {
        EXPECT_LE(
            abs(output[ni] - static_cast<tran_low_t>(round(ref_output[ni]))),
            max_error_);
      }
    }

    delete[] input;
    delete[] output;
    delete[] ref_input;
    delete[] ref_output;
  }

  double max_error_;
  int txfm_size_;
  FdctFunc fwd_txfm_;
  FdctFuncRef fwd_txfm_ref_;
};

typedef std::tr1::tuple<FdctFunc, FdctFuncRef, int, int> FdctParam;
class AV1FwdTxfm : public TransTestBase,
                   public ::testing::TestWithParam<FdctParam> {
 public:
  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    fwd_txfm_ref_ = GET_PARAM(1);
    txfm_size_ = GET_PARAM(2);
    max_error_ = GET_PARAM(3);
  }
  virtual void TearDown() {}
};

TEST_P(AV1FwdTxfm, RunFwdAccuracyCheck) { RunFwdAccuracyCheck(); }

INSTANTIATE_TEST_CASE_P(
    C, AV1FwdTxfm,
    ::testing::Values(FdctParam(&fdct4, &reference_dct_1d, 4, 1),
                      FdctParam(&fdct8, &reference_dct_1d, 8, 1),
                      FdctParam(&fdct16, &reference_dct_1d, 16, 2),
                      FdctParam(&fdct32, &reference_dct_1d, 32, 3)));
}  // namespace
