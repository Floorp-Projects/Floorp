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
#include <string.h>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./av1_rtcd.h"
#include "./aom_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "av1/common/entropy.h"
#include "av1/common/scan.h"
#include "aom/aom_codec.h"
#include "aom/aom_integer.h"
#include "aom_ports/mem.h"

using libaom_test::ACMRandom;

namespace {

const int kNumCoeffs = 64;
const double kPi = 3.141592653589793238462643383279502884;

const int kSignBiasMaxDiff255 = 1500;
const int kSignBiasMaxDiff15 = 10000;

typedef void (*FdctFunc)(const int16_t *in, tran_low_t *out, int stride);
typedef void (*IdctFunc)(const tran_low_t *in, uint8_t *out, int stride);
typedef void (*FhtFunc)(const int16_t *in, tran_low_t *out, int stride,
                        TxfmParam *txfm_param);
typedef void (*IhtFunc)(const tran_low_t *in, uint8_t *out, int stride,
                        const TxfmParam *txfm_param);

typedef std::tr1::tuple<FdctFunc, IdctFunc, TX_TYPE, aom_bit_depth_t>
    Dct8x8Param;
typedef std::tr1::tuple<FhtFunc, IhtFunc, TX_TYPE, aom_bit_depth_t> Ht8x8Param;
typedef std::tr1::tuple<IdctFunc, IdctFunc, int, aom_bit_depth_t> Idct8x8Param;

void reference_8x8_dct_1d(const double in[8], double out[8]) {
  const double kInvSqrt2 = 0.707106781186547524400844362104;
  for (int k = 0; k < 8; k++) {
    out[k] = 0.0;
    for (int n = 0; n < 8; n++)
      out[k] += in[n] * cos(kPi * (2 * n + 1) * k / 16.0);
    if (k == 0) out[k] = out[k] * kInvSqrt2;
  }
}

void reference_8x8_dct_2d(const int16_t input[kNumCoeffs],
                          double output[kNumCoeffs]) {
  // First transform columns
  for (int i = 0; i < 8; ++i) {
    double temp_in[8], temp_out[8];
    for (int j = 0; j < 8; ++j) temp_in[j] = input[j * 8 + i];
    reference_8x8_dct_1d(temp_in, temp_out);
    for (int j = 0; j < 8; ++j) output[j * 8 + i] = temp_out[j];
  }
  // Then transform rows
  for (int i = 0; i < 8; ++i) {
    double temp_in[8], temp_out[8];
    for (int j = 0; j < 8; ++j) temp_in[j] = output[j + i * 8];
    reference_8x8_dct_1d(temp_in, temp_out);
    // Scale by some magic number
    for (int j = 0; j < 8; ++j) output[j + i * 8] = temp_out[j] * 2;
  }
}

void fdct8x8_ref(const int16_t *in, tran_low_t *out, int stride,
                 TxfmParam * /*txfm_param*/) {
  aom_fdct8x8_c(in, out, stride);
}

void fht8x8_ref(const int16_t *in, tran_low_t *out, int stride,
                TxfmParam *txfm_param) {
  av1_fht8x8_c(in, out, stride, txfm_param);
}

#if CONFIG_HIGHBITDEPTH
void fht8x8_10(const int16_t *in, tran_low_t *out, int stride,
               TxfmParam *txfm_param) {
  av1_fwd_txfm2d_8x8_c(in, out, stride, txfm_param->tx_type, 10);
}

void fht8x8_12(const int16_t *in, tran_low_t *out, int stride,
               TxfmParam *txfm_param) {
  av1_fwd_txfm2d_8x8_c(in, out, stride, txfm_param->tx_type, 12);
}

void iht8x8_10(const tran_low_t *in, uint8_t *out, int stride,
               const TxfmParam *txfm_param) {
  av1_inv_txfm2d_add_8x8_c(in, CONVERT_TO_SHORTPTR(out), stride,
                           txfm_param->tx_type, 10);
}

void iht8x8_12(const tran_low_t *in, uint8_t *out, int stride,
               const TxfmParam *txfm_param) {
  av1_inv_txfm2d_add_8x8_c(in, CONVERT_TO_SHORTPTR(out), stride,
                           txfm_param->tx_type, 12);
}

#endif  // CONFIG_HIGHBITDEPTH

class FwdTrans8x8TestBase {
 public:
  virtual ~FwdTrans8x8TestBase() {}

 protected:
  virtual void RunFwdTxfm(int16_t *in, tran_low_t *out, int stride) = 0;
  virtual void RunInvTxfm(tran_low_t *out, uint8_t *dst, int stride) = 0;

  void RunSignBiasCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    DECLARE_ALIGNED(16, int16_t, test_input_block[64]);
    DECLARE_ALIGNED(16, tran_low_t, test_output_block[64]);
    int count_sign_block[64][2];
    const int count_test_block = 100000;

    memset(count_sign_block, 0, sizeof(count_sign_block));

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-255, 255].
      for (int j = 0; j < 64; ++j)
        test_input_block[j] = ((rnd.Rand16() >> (16 - bit_depth_)) & mask_) -
                              ((rnd.Rand16() >> (16 - bit_depth_)) & mask_);
      ASM_REGISTER_STATE_CHECK(
          RunFwdTxfm(test_input_block, test_output_block, pitch_));

      for (int j = 0; j < 64; ++j) {
        if (test_output_block[j] < 0)
          ++count_sign_block[j][0];
        else if (test_output_block[j] > 0)
          ++count_sign_block[j][1];
      }
    }

    for (int j = 0; j < 64; ++j) {
      const int diff = abs(count_sign_block[j][0] - count_sign_block[j][1]);
      const int max_diff = kSignBiasMaxDiff255;
      EXPECT_LT(diff, max_diff << (bit_depth_ - 8))
          << "Error: 8x8 FDCT/FHT has a sign bias > "
          << 1. * max_diff / count_test_block * 100 << "%"
          << " for input range [-255, 255] at index " << j
          << " count0: " << count_sign_block[j][0]
          << " count1: " << count_sign_block[j][1] << " diff: " << diff;
    }

    memset(count_sign_block, 0, sizeof(count_sign_block));

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-mask_ / 16, mask_ / 16].
      for (int j = 0; j < 64; ++j)
        test_input_block[j] =
            ((rnd.Rand16() & mask_) >> 4) - ((rnd.Rand16() & mask_) >> 4);
      ASM_REGISTER_STATE_CHECK(
          RunFwdTxfm(test_input_block, test_output_block, pitch_));

      for (int j = 0; j < 64; ++j) {
        if (test_output_block[j] < 0)
          ++count_sign_block[j][0];
        else if (test_output_block[j] > 0)
          ++count_sign_block[j][1];
      }
    }

    for (int j = 0; j < 64; ++j) {
      const int diff = abs(count_sign_block[j][0] - count_sign_block[j][1]);
      const int max_diff = kSignBiasMaxDiff15;
      EXPECT_LT(diff, max_diff << (bit_depth_ - 8))
          << "Error: 8x8 FDCT/FHT has a sign bias > "
          << 1. * max_diff / count_test_block * 100 << "%"
          << " for input range [-15, 15] at index " << j
          << " count0: " << count_sign_block[j][0]
          << " count1: " << count_sign_block[j][1] << " diff: " << diff;
    }
  }

  void RunRoundTripErrorCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    int max_error = 0;
    int total_error = 0;
    const int count_test_block = 100000;
    DECLARE_ALIGNED(16, int16_t, test_input_block[64]);
    DECLARE_ALIGNED(16, tran_low_t, test_temp_block[64]);
    DECLARE_ALIGNED(16, uint8_t, dst[64]);
    DECLARE_ALIGNED(16, uint8_t, src[64]);
#if CONFIG_HIGHBITDEPTH
    DECLARE_ALIGNED(16, uint16_t, dst16[64]);
    DECLARE_ALIGNED(16, uint16_t, src16[64]);
#endif

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-mask_, mask_].
      for (int j = 0; j < 64; ++j) {
        if (bit_depth_ == AOM_BITS_8) {
          src[j] = rnd.Rand8();
          dst[j] = rnd.Rand8();
          test_input_block[j] = src[j] - dst[j];
#if CONFIG_HIGHBITDEPTH
        } else {
          src16[j] = rnd.Rand16() & mask_;
          dst16[j] = rnd.Rand16() & mask_;
          test_input_block[j] = src16[j] - dst16[j];
#endif
        }
      }

      ASM_REGISTER_STATE_CHECK(
          RunFwdTxfm(test_input_block, test_temp_block, pitch_));
      for (int j = 0; j < 64; ++j) {
        if (test_temp_block[j] > 0) {
          test_temp_block[j] += 2;
          test_temp_block[j] /= 4;
          test_temp_block[j] *= 4;
        } else {
          test_temp_block[j] -= 2;
          test_temp_block[j] /= 4;
          test_temp_block[j] *= 4;
        }
      }
      if (bit_depth_ == AOM_BITS_8) {
        ASM_REGISTER_STATE_CHECK(RunInvTxfm(test_temp_block, dst, pitch_));
#if CONFIG_HIGHBITDEPTH
      } else {
        ASM_REGISTER_STATE_CHECK(
            RunInvTxfm(test_temp_block, CONVERT_TO_BYTEPTR(dst16), pitch_));
#endif
      }

      for (int j = 0; j < 64; ++j) {
#if CONFIG_HIGHBITDEPTH
        const int diff =
            bit_depth_ == AOM_BITS_8 ? dst[j] - src[j] : dst16[j] - src16[j];
#else
        const int diff = dst[j] - src[j];
#endif
        const int error = diff * diff;
        if (max_error < error) max_error = error;
        total_error += error;
      }
    }

    EXPECT_GE(1 << 2 * (bit_depth_ - 8), max_error)
        << "Error: 8x8 FDCT/IDCT or FHT/IHT has an individual"
        << " roundtrip error > 1";

    EXPECT_GE((count_test_block << 2 * (bit_depth_ - 8)) / 5, total_error)
        << "Error: 8x8 FDCT/IDCT or FHT/IHT has average roundtrip "
        << "error > 1/5 per block";
  }

  void RunExtremalCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    int max_error = 0;
    int total_error = 0;
    int total_coeff_error = 0;
    const int count_test_block = 100000;
    DECLARE_ALIGNED(16, int16_t, test_input_block[64]);
    DECLARE_ALIGNED(16, tran_low_t, test_temp_block[64]);
    DECLARE_ALIGNED(16, tran_low_t, ref_temp_block[64]);
    DECLARE_ALIGNED(16, uint8_t, dst[64]);
    DECLARE_ALIGNED(16, uint8_t, src[64]);
#if CONFIG_HIGHBITDEPTH
    DECLARE_ALIGNED(16, uint16_t, dst16[64]);
    DECLARE_ALIGNED(16, uint16_t, src16[64]);
#endif

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-mask_, mask_].
      for (int j = 0; j < 64; ++j) {
        if (bit_depth_ == AOM_BITS_8) {
          if (i == 0) {
            src[j] = 255;
            dst[j] = 0;
          } else if (i == 1) {
            src[j] = 0;
            dst[j] = 255;
          } else {
            src[j] = rnd.Rand8() % 2 ? 255 : 0;
            dst[j] = rnd.Rand8() % 2 ? 255 : 0;
          }
          test_input_block[j] = src[j] - dst[j];
#if CONFIG_HIGHBITDEPTH
        } else {
          if (i == 0) {
            src16[j] = mask_;
            dst16[j] = 0;
          } else if (i == 1) {
            src16[j] = 0;
            dst16[j] = mask_;
          } else {
            src16[j] = rnd.Rand8() % 2 ? mask_ : 0;
            dst16[j] = rnd.Rand8() % 2 ? mask_ : 0;
          }
          test_input_block[j] = src16[j] - dst16[j];
#endif
        }
      }

      ASM_REGISTER_STATE_CHECK(
          RunFwdTxfm(test_input_block, test_temp_block, pitch_));
      ASM_REGISTER_STATE_CHECK(
          fwd_txfm_ref(test_input_block, ref_temp_block, pitch_, &txfm_param_));
      if (bit_depth_ == AOM_BITS_8) {
        ASM_REGISTER_STATE_CHECK(RunInvTxfm(test_temp_block, dst, pitch_));
#if CONFIG_HIGHBITDEPTH
      } else {
        ASM_REGISTER_STATE_CHECK(
            RunInvTxfm(test_temp_block, CONVERT_TO_BYTEPTR(dst16), pitch_));
#endif
      }

      for (int j = 0; j < 64; ++j) {
#if CONFIG_HIGHBITDEPTH
        const int diff =
            bit_depth_ == AOM_BITS_8 ? dst[j] - src[j] : dst16[j] - src16[j];
#else
        const int diff = dst[j] - src[j];
#endif
        const int error = diff * diff;
        if (max_error < error) max_error = error;
        total_error += error;

        const int coeff_diff = test_temp_block[j] - ref_temp_block[j];
        total_coeff_error += abs(coeff_diff);
      }

      EXPECT_GE(1 << 2 * (bit_depth_ - 8), max_error)
          << "Error: Extremal 8x8 FDCT/IDCT or FHT/IHT has"
          << "an individual roundtrip error > 1";

      EXPECT_GE((count_test_block << 2 * (bit_depth_ - 8)) / 5, total_error)
          << "Error: Extremal 8x8 FDCT/IDCT or FHT/IHT has average"
          << " roundtrip error > 1/5 per block";

      EXPECT_EQ(0, total_coeff_error)
          << "Error: Extremal 8x8 FDCT/FHT has"
          << "overflow issues in the intermediate steps > 1";
    }
  }

  void RunInvAccuracyCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 1000;
    DECLARE_ALIGNED(16, int16_t, in[kNumCoeffs]);
    DECLARE_ALIGNED(16, tran_low_t, coeff[kNumCoeffs]);
    DECLARE_ALIGNED(16, uint8_t, dst[kNumCoeffs]);
    DECLARE_ALIGNED(16, uint8_t, src[kNumCoeffs]);
#if CONFIG_HIGHBITDEPTH
    DECLARE_ALIGNED(16, uint16_t, src16[kNumCoeffs]);
    DECLARE_ALIGNED(16, uint16_t, dst16[kNumCoeffs]);
#endif

    for (int i = 0; i < count_test_block; ++i) {
      double out_r[kNumCoeffs];

      // Initialize a test block with input range [-255, 255].
      for (int j = 0; j < kNumCoeffs; ++j) {
        if (bit_depth_ == AOM_BITS_8) {
          src[j] = rnd.Rand8() % 2 ? 255 : 0;
          dst[j] = src[j] > 0 ? 0 : 255;
          in[j] = src[j] - dst[j];
#if CONFIG_HIGHBITDEPTH
        } else {
          src16[j] = rnd.Rand8() % 2 ? mask_ : 0;
          dst16[j] = src16[j] > 0 ? 0 : mask_;
          in[j] = src16[j] - dst16[j];
#endif
        }
      }

      reference_8x8_dct_2d(in, out_r);
      for (int j = 0; j < kNumCoeffs; ++j)
        coeff[j] = static_cast<tran_low_t>(round(out_r[j]));

      if (bit_depth_ == AOM_BITS_8) {
        ASM_REGISTER_STATE_CHECK(RunInvTxfm(coeff, dst, pitch_));
#if CONFIG_HIGHBITDEPTH
      } else {
        ASM_REGISTER_STATE_CHECK(
            RunInvTxfm(coeff, CONVERT_TO_BYTEPTR(dst16), pitch_));
#endif
      }

      for (int j = 0; j < kNumCoeffs; ++j) {
#if CONFIG_HIGHBITDEPTH
        const int diff =
            bit_depth_ == AOM_BITS_8 ? dst[j] - src[j] : dst16[j] - src16[j];
#else
        const int diff = dst[j] - src[j];
#endif
        const uint32_t error = diff * diff;
        EXPECT_GE(1u << 2 * (bit_depth_ - 8), error)
            << "Error: 8x8 IDCT has error " << error << " at index " << j;
      }
    }
  }

  void RunFwdAccuracyCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 1000;
    DECLARE_ALIGNED(16, int16_t, in[kNumCoeffs]);
    DECLARE_ALIGNED(16, tran_low_t, coeff_r[kNumCoeffs]);
    DECLARE_ALIGNED(16, tran_low_t, coeff[kNumCoeffs]);

    for (int i = 0; i < count_test_block; ++i) {
      double out_r[kNumCoeffs];

      // Initialize a test block with input range [-mask_, mask_].
      for (int j = 0; j < kNumCoeffs; ++j)
        in[j] = rnd.Rand8() % 2 == 0 ? mask_ : -mask_;

      RunFwdTxfm(in, coeff, pitch_);
      reference_8x8_dct_2d(in, out_r);
      for (int j = 0; j < kNumCoeffs; ++j)
        coeff_r[j] = static_cast<tran_low_t>(round(out_r[j]));

      for (int j = 0; j < kNumCoeffs; ++j) {
        const int32_t diff = coeff[j] - coeff_r[j];
        const uint32_t error = diff * diff;
        EXPECT_GE(9u << 2 * (bit_depth_ - 8), error)
            << "Error: 8x8 DCT has error " << error << " at index " << j;
      }
    }
  }

  void CompareInvReference(IdctFunc ref_txfm, int thresh) {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 10000;
    const int eob = 12;
    DECLARE_ALIGNED(16, tran_low_t, coeff[kNumCoeffs]);
    DECLARE_ALIGNED(16, uint8_t, dst[kNumCoeffs]);
    DECLARE_ALIGNED(16, uint8_t, ref[kNumCoeffs]);
#if CONFIG_HIGHBITDEPTH
    DECLARE_ALIGNED(16, uint16_t, dst16[kNumCoeffs]);
    DECLARE_ALIGNED(16, uint16_t, ref16[kNumCoeffs]);
#endif
    const int16_t *scan = av1_default_scan_orders[TX_8X8].scan;

    for (int i = 0; i < count_test_block; ++i) {
      for (int j = 0; j < kNumCoeffs; ++j) {
        if (j < eob) {
          // Random values less than the threshold, either positive or negative
          coeff[scan[j]] = rnd(thresh) * (1 - 2 * (i % 2));
        } else {
          coeff[scan[j]] = 0;
        }
        if (bit_depth_ == AOM_BITS_8) {
          dst[j] = 0;
          ref[j] = 0;
#if CONFIG_HIGHBITDEPTH
        } else {
          dst16[j] = 0;
          ref16[j] = 0;
#endif
        }
      }
      if (bit_depth_ == AOM_BITS_8) {
        ref_txfm(coeff, ref, pitch_);
        ASM_REGISTER_STATE_CHECK(RunInvTxfm(coeff, dst, pitch_));
#if CONFIG_HIGHBITDEPTH
      } else {
        ref_txfm(coeff, CONVERT_TO_BYTEPTR(ref16), pitch_);
        ASM_REGISTER_STATE_CHECK(
            RunInvTxfm(coeff, CONVERT_TO_BYTEPTR(dst16), pitch_));
#endif
      }

      for (int j = 0; j < kNumCoeffs; ++j) {
#if CONFIG_HIGHBITDEPTH
        const int diff =
            bit_depth_ == AOM_BITS_8 ? dst[j] - ref[j] : dst16[j] - ref16[j];
#else
        const int diff = dst[j] - ref[j];
#endif
        const uint32_t error = diff * diff;
        EXPECT_EQ(0u, error)
            << "Error: 8x8 IDCT has error " << error << " at index " << j;
      }
    }
  }
  int pitch_;
  FhtFunc fwd_txfm_ref;
  aom_bit_depth_t bit_depth_;
  int mask_;
  TxfmParam txfm_param_;
};

class FwdTrans8x8DCT : public FwdTrans8x8TestBase,
                       public ::testing::TestWithParam<Dct8x8Param> {
 public:
  virtual ~FwdTrans8x8DCT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    pitch_ = 8;
    fwd_txfm_ref = fdct8x8_ref;
    bit_depth_ = GET_PARAM(3);
    mask_ = (1 << bit_depth_) - 1;
    txfm_param_.tx_type = GET_PARAM(2);
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  void RunFwdTxfm(int16_t *in, tran_low_t *out, int stride) {
    fwd_txfm_(in, out, stride);
  }
  void RunInvTxfm(tran_low_t *out, uint8_t *dst, int stride) {
    inv_txfm_(out, dst, stride);
  }

  FdctFunc fwd_txfm_;
  IdctFunc inv_txfm_;
};

TEST_P(FwdTrans8x8DCT, SignBiasCheck) { RunSignBiasCheck(); }

TEST_P(FwdTrans8x8DCT, RoundTripErrorCheck) { RunRoundTripErrorCheck(); }

TEST_P(FwdTrans8x8DCT, ExtremalCheck) { RunExtremalCheck(); }

TEST_P(FwdTrans8x8DCT, FwdAccuracyCheck) { RunFwdAccuracyCheck(); }

TEST_P(FwdTrans8x8DCT, InvAccuracyCheck) { RunInvAccuracyCheck(); }

class FwdTrans8x8HT : public FwdTrans8x8TestBase,
                      public ::testing::TestWithParam<Ht8x8Param> {
 public:
  virtual ~FwdTrans8x8HT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    pitch_ = 8;
    fwd_txfm_ref = fht8x8_ref;
    bit_depth_ = GET_PARAM(3);
    mask_ = (1 << bit_depth_) - 1;
    txfm_param_.tx_type = GET_PARAM(2);
#if CONFIG_HIGHBITDEPTH
    switch (bit_depth_) {
      case AOM_BITS_10: fwd_txfm_ref = fht8x8_10; break;
      case AOM_BITS_12: fwd_txfm_ref = fht8x8_12; break;
      default: fwd_txfm_ref = fht8x8_ref; break;
    }
#endif
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  void RunFwdTxfm(int16_t *in, tran_low_t *out, int stride) {
    fwd_txfm_(in, out, stride, &txfm_param_);
  }
  void RunInvTxfm(tran_low_t *out, uint8_t *dst, int stride) {
    inv_txfm_(out, dst, stride, &txfm_param_);
  }

  FhtFunc fwd_txfm_;
  IhtFunc inv_txfm_;
};

TEST_P(FwdTrans8x8HT, SignBiasCheck) { RunSignBiasCheck(); }

TEST_P(FwdTrans8x8HT, RoundTripErrorCheck) { RunRoundTripErrorCheck(); }

TEST_P(FwdTrans8x8HT, ExtremalCheck) { RunExtremalCheck(); }

class InvTrans8x8DCT : public FwdTrans8x8TestBase,
                       public ::testing::TestWithParam<Idct8x8Param> {
 public:
  virtual ~InvTrans8x8DCT() {}

  virtual void SetUp() {
    ref_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    thresh_ = GET_PARAM(2);
    pitch_ = 8;
    bit_depth_ = GET_PARAM(3);
    mask_ = (1 << bit_depth_) - 1;
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  void RunInvTxfm(tran_low_t *out, uint8_t *dst, int stride) {
    inv_txfm_(out, dst, stride);
  }
  void RunFwdTxfm(int16_t * /*out*/, tran_low_t * /*dst*/, int /*stride*/) {}

  IdctFunc ref_txfm_;
  IdctFunc inv_txfm_;
  int thresh_;
};

TEST_P(InvTrans8x8DCT, CompareReference) {
  CompareInvReference(ref_txfm_, thresh_);
}

using std::tr1::make_tuple;

#if CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(C, FwdTrans8x8DCT,
                        ::testing::Values(make_tuple(&aom_fdct8x8_c,
                                                     &aom_idct8x8_64_add_c,
                                                     DCT_DCT, AOM_BITS_8)));
#else
INSTANTIATE_TEST_CASE_P(C, FwdTrans8x8DCT,
                        ::testing::Values(make_tuple(&aom_fdct8x8_c,
                                                     &aom_idct8x8_64_add_c,
                                                     DCT_DCT, AOM_BITS_8)));
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    C, FwdTrans8x8HT,
    ::testing::Values(
        make_tuple(&av1_fht8x8_c, &av1_iht8x8_64_add_c, DCT_DCT, AOM_BITS_8),
        make_tuple(&fht8x8_10, &iht8x8_10, DCT_DCT, AOM_BITS_10),
        make_tuple(&fht8x8_10, &iht8x8_10, ADST_DCT, AOM_BITS_10),
        make_tuple(&fht8x8_10, &iht8x8_10, DCT_ADST, AOM_BITS_10),
        make_tuple(&fht8x8_10, &iht8x8_10, ADST_ADST, AOM_BITS_10),
        make_tuple(&fht8x8_12, &iht8x8_12, DCT_DCT, AOM_BITS_12),
        make_tuple(&fht8x8_12, &iht8x8_12, ADST_DCT, AOM_BITS_12),
        make_tuple(&fht8x8_12, &iht8x8_12, DCT_ADST, AOM_BITS_12),
        make_tuple(&fht8x8_12, &iht8x8_12, ADST_ADST, AOM_BITS_12),
        make_tuple(&av1_fht8x8_c, &av1_iht8x8_64_add_c, ADST_DCT, AOM_BITS_8),
        make_tuple(&av1_fht8x8_c, &av1_iht8x8_64_add_c, DCT_ADST, AOM_BITS_8),
        make_tuple(&av1_fht8x8_c, &av1_iht8x8_64_add_c, ADST_ADST,
                   AOM_BITS_8)));
#else
INSTANTIATE_TEST_CASE_P(
    C, FwdTrans8x8HT,
    ::testing::Values(
        make_tuple(&av1_fht8x8_c, &av1_iht8x8_64_add_c, DCT_DCT, AOM_BITS_8),
        make_tuple(&av1_fht8x8_c, &av1_iht8x8_64_add_c, ADST_DCT, AOM_BITS_8),
        make_tuple(&av1_fht8x8_c, &av1_iht8x8_64_add_c, DCT_ADST, AOM_BITS_8),
        make_tuple(&av1_fht8x8_c, &av1_iht8x8_64_add_c, ADST_ADST,
                   AOM_BITS_8)));
#endif  // CONFIG_HIGHBITDEPTH

#if HAVE_NEON_ASM && !CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(NEON, FwdTrans8x8DCT,
                        ::testing::Values(make_tuple(&aom_fdct8x8_neon,
                                                     &aom_idct8x8_64_add_neon,
                                                     DCT_DCT, AOM_BITS_8)));
#endif  // HAVE_NEON_ASM && !CONFIG_HIGHBITDEPTH

#if HAVE_NEON && !CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    NEON, FwdTrans8x8HT,
    ::testing::Values(make_tuple(&av1_fht8x8_c, &av1_iht8x8_64_add_neon,
                                 DCT_DCT, AOM_BITS_8),
                      make_tuple(&av1_fht8x8_c, &av1_iht8x8_64_add_neon,
                                 ADST_DCT, AOM_BITS_8),
                      make_tuple(&av1_fht8x8_c, &av1_iht8x8_64_add_neon,
                                 DCT_ADST, AOM_BITS_8),
                      make_tuple(&av1_fht8x8_c, &av1_iht8x8_64_add_neon,
                                 ADST_ADST, AOM_BITS_8)));
#endif  // HAVE_NEON && !CONFIG_HIGHBITDEPTH

#if HAVE_SSE2 && !CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(SSE2, FwdTrans8x8DCT,
                        ::testing::Values(make_tuple(&aom_fdct8x8_sse2,
                                                     &aom_idct8x8_64_add_sse2,
                                                     DCT_DCT, AOM_BITS_8)));
#if !CONFIG_DAALA_DCT8
INSTANTIATE_TEST_CASE_P(
    SSE2, FwdTrans8x8HT,
    ::testing::Values(make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2,
                                 DCT_DCT, AOM_BITS_8),
                      make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2,
                                 ADST_DCT, AOM_BITS_8),
                      make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2,
                                 DCT_ADST, AOM_BITS_8),
                      make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_sse2,
                                 ADST_ADST, AOM_BITS_8)));
#endif  // !CONFIG_DAALA_DCT8
#endif  // HAVE_SSE2 && !CONFIG_HIGHBITDEPTH

#if HAVE_SSE2 && CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(SSE2, FwdTrans8x8DCT,
                        ::testing::Values(make_tuple(&aom_fdct8x8_sse2,
                                                     &aom_idct8x8_64_add_c,
                                                     DCT_DCT, AOM_BITS_8)));
#if !CONFIG_DAALA_DCT8
INSTANTIATE_TEST_CASE_P(
    SSE2, FwdTrans8x8HT,
    ::testing::Values(make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_c,
                                 DCT_DCT, AOM_BITS_8),
                      make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_c,
                                 ADST_DCT, AOM_BITS_8),
                      make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_c,
                                 DCT_ADST, AOM_BITS_8),
                      make_tuple(&av1_fht8x8_sse2, &av1_iht8x8_64_add_c,
                                 ADST_ADST, AOM_BITS_8)));
#endif  // !CONFIG_DAALA_DCT8
#endif  // HAVE_SSE2 && CONFIG_HIGHBITDEPTH

#if HAVE_SSSE3 && ARCH_X86_64
INSTANTIATE_TEST_CASE_P(SSSE3, FwdTrans8x8DCT,
                        ::testing::Values(make_tuple(&aom_fdct8x8_ssse3,
                                                     &aom_idct8x8_64_add_ssse3,
                                                     DCT_DCT, AOM_BITS_8)));
#endif

#if HAVE_MSA && !CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(MSA, FwdTrans8x8DCT,
                        ::testing::Values(make_tuple(&aom_fdct8x8_msa,
                                                     &aom_idct8x8_64_add_msa,
                                                     DCT_DCT, AOM_BITS_8)));
#if !CONFIG_EXT_TX && !CONFIG_DAALA_DCT8
INSTANTIATE_TEST_CASE_P(
    MSA, FwdTrans8x8HT,
    ::testing::Values(make_tuple(&av1_fht8x8_msa, &av1_iht8x8_64_add_msa,
                                 DCT_DCT, AOM_BITS_8),
                      make_tuple(&av1_fht8x8_msa, &av1_iht8x8_64_add_msa,
                                 ADST_DCT, AOM_BITS_8),
                      make_tuple(&av1_fht8x8_msa, &av1_iht8x8_64_add_msa,
                                 DCT_ADST, AOM_BITS_8),
                      make_tuple(&av1_fht8x8_msa, &av1_iht8x8_64_add_msa,
                                 ADST_ADST, AOM_BITS_8)));
#endif  // !CONFIG_EXT_TX && !CONFIG_DAALA_DCT8
#endif  // HAVE_MSA && !CONFIG_HIGHBITDEPTH
}  // namespace
