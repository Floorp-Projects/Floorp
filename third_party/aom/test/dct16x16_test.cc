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
#include "aom_ports/msvc.h"  // for round()

using libaom_test::ACMRandom;

namespace {

const int kNumCoeffs = 256;
const double C1 = 0.995184726672197;
const double C2 = 0.98078528040323;
const double C3 = 0.956940335732209;
const double C4 = 0.923879532511287;
const double C5 = 0.881921264348355;
const double C6 = 0.831469612302545;
const double C7 = 0.773010453362737;
const double C8 = 0.707106781186548;
const double C9 = 0.634393284163646;
const double C10 = 0.555570233019602;
const double C11 = 0.471396736825998;
const double C12 = 0.38268343236509;
const double C13 = 0.290284677254462;
const double C14 = 0.195090322016128;
const double C15 = 0.098017140329561;

void butterfly_16x16_dct_1d(double input[16], double output[16]) {
  double step[16];
  double intermediate[16];
  double temp1, temp2;

  // step 1
  step[0] = input[0] + input[15];
  step[1] = input[1] + input[14];
  step[2] = input[2] + input[13];
  step[3] = input[3] + input[12];
  step[4] = input[4] + input[11];
  step[5] = input[5] + input[10];
  step[6] = input[6] + input[9];
  step[7] = input[7] + input[8];
  step[8] = input[7] - input[8];
  step[9] = input[6] - input[9];
  step[10] = input[5] - input[10];
  step[11] = input[4] - input[11];
  step[12] = input[3] - input[12];
  step[13] = input[2] - input[13];
  step[14] = input[1] - input[14];
  step[15] = input[0] - input[15];

  // step 2
  output[0] = step[0] + step[7];
  output[1] = step[1] + step[6];
  output[2] = step[2] + step[5];
  output[3] = step[3] + step[4];
  output[4] = step[3] - step[4];
  output[5] = step[2] - step[5];
  output[6] = step[1] - step[6];
  output[7] = step[0] - step[7];

  temp1 = step[8] * C7;
  temp2 = step[15] * C9;
  output[8] = temp1 + temp2;

  temp1 = step[9] * C11;
  temp2 = step[14] * C5;
  output[9] = temp1 - temp2;

  temp1 = step[10] * C3;
  temp2 = step[13] * C13;
  output[10] = temp1 + temp2;

  temp1 = step[11] * C15;
  temp2 = step[12] * C1;
  output[11] = temp1 - temp2;

  temp1 = step[11] * C1;
  temp2 = step[12] * C15;
  output[12] = temp2 + temp1;

  temp1 = step[10] * C13;
  temp2 = step[13] * C3;
  output[13] = temp2 - temp1;

  temp1 = step[9] * C5;
  temp2 = step[14] * C11;
  output[14] = temp2 + temp1;

  temp1 = step[8] * C9;
  temp2 = step[15] * C7;
  output[15] = temp2 - temp1;

  // step 3
  step[0] = output[0] + output[3];
  step[1] = output[1] + output[2];
  step[2] = output[1] - output[2];
  step[3] = output[0] - output[3];

  temp1 = output[4] * C14;
  temp2 = output[7] * C2;
  step[4] = temp1 + temp2;

  temp1 = output[5] * C10;
  temp2 = output[6] * C6;
  step[5] = temp1 + temp2;

  temp1 = output[5] * C6;
  temp2 = output[6] * C10;
  step[6] = temp2 - temp1;

  temp1 = output[4] * C2;
  temp2 = output[7] * C14;
  step[7] = temp2 - temp1;

  step[8] = output[8] + output[11];
  step[9] = output[9] + output[10];
  step[10] = output[9] - output[10];
  step[11] = output[8] - output[11];

  step[12] = output[12] + output[15];
  step[13] = output[13] + output[14];
  step[14] = output[13] - output[14];
  step[15] = output[12] - output[15];

  // step 4
  output[0] = (step[0] + step[1]);
  output[8] = (step[0] - step[1]);

  temp1 = step[2] * C12;
  temp2 = step[3] * C4;
  temp1 = temp1 + temp2;
  output[4] = 2 * (temp1 * C8);

  temp1 = step[2] * C4;
  temp2 = step[3] * C12;
  temp1 = temp2 - temp1;
  output[12] = 2 * (temp1 * C8);

  output[2] = 2 * ((step[4] + step[5]) * C8);
  output[14] = 2 * ((step[7] - step[6]) * C8);

  temp1 = step[4] - step[5];
  temp2 = step[6] + step[7];
  output[6] = (temp1 + temp2);
  output[10] = (temp1 - temp2);

  intermediate[8] = step[8] + step[14];
  intermediate[9] = step[9] + step[15];

  temp1 = intermediate[8] * C12;
  temp2 = intermediate[9] * C4;
  temp1 = temp1 - temp2;
  output[3] = 2 * (temp1 * C8);

  temp1 = intermediate[8] * C4;
  temp2 = intermediate[9] * C12;
  temp1 = temp2 + temp1;
  output[13] = 2 * (temp1 * C8);

  output[9] = 2 * ((step[10] + step[11]) * C8);

  intermediate[11] = step[10] - step[11];
  intermediate[12] = step[12] + step[13];
  intermediate[13] = step[12] - step[13];
  intermediate[14] = step[8] - step[14];
  intermediate[15] = step[9] - step[15];

  output[15] = (intermediate[11] + intermediate[12]);
  output[1] = -(intermediate[11] - intermediate[12]);

  output[7] = 2 * (intermediate[13] * C8);

  temp1 = intermediate[14] * C12;
  temp2 = intermediate[15] * C4;
  temp1 = temp1 - temp2;
  output[11] = -2 * (temp1 * C8);

  temp1 = intermediate[14] * C4;
  temp2 = intermediate[15] * C12;
  temp1 = temp2 + temp1;
  output[5] = 2 * (temp1 * C8);
}

void reference_16x16_dct_2d(int16_t input[256], double output[256]) {
  // First transform columns
  for (int i = 0; i < 16; ++i) {
    double temp_in[16], temp_out[16];
    for (int j = 0; j < 16; ++j) temp_in[j] = input[j * 16 + i];
    butterfly_16x16_dct_1d(temp_in, temp_out);
    for (int j = 0; j < 16; ++j) output[j * 16 + i] = temp_out[j];
  }
  // Then transform rows
  for (int i = 0; i < 16; ++i) {
    double temp_in[16], temp_out[16];
    for (int j = 0; j < 16; ++j) temp_in[j] = output[j + i * 16];
    butterfly_16x16_dct_1d(temp_in, temp_out);
    // Scale by some magic number
    for (int j = 0; j < 16; ++j) output[j + i * 16] = temp_out[j] / 2;
  }
}

typedef void (*FdctFunc)(const int16_t *in, tran_low_t *out, int stride);
typedef void (*IdctFunc)(const tran_low_t *in, uint8_t *out, int stride);
typedef void (*FhtFunc)(const int16_t *in, tran_low_t *out, int stride,
                        TxfmParam *txfm_param);
typedef void (*IhtFunc)(const tran_low_t *in, uint8_t *out, int stride,
                        const TxfmParam *txfm_param);

typedef std::tr1::tuple<FdctFunc, IdctFunc, TX_TYPE, aom_bit_depth_t>
    Dct16x16Param;
typedef std::tr1::tuple<FhtFunc, IhtFunc, TX_TYPE, aom_bit_depth_t>
    Ht16x16Param;
typedef std::tr1::tuple<IdctFunc, IdctFunc, TX_TYPE, aom_bit_depth_t>
    Idct16x16Param;

void fdct16x16_ref(const int16_t *in, tran_low_t *out, int stride,
                   TxfmParam * /*txfm_param*/) {
  aom_fdct16x16_c(in, out, stride);
}

void idct16x16_ref(const tran_low_t *in, uint8_t *dest, int stride,
                   const TxfmParam * /*txfm_param*/) {
  aom_idct16x16_256_add_c(in, dest, stride);
}

void fht16x16_ref(const int16_t *in, tran_low_t *out, int stride,
                  TxfmParam *txfm_param) {
  av1_fht16x16_c(in, out, stride, txfm_param);
}

void iht16x16_ref(const tran_low_t *in, uint8_t *dest, int stride,
                  const TxfmParam *txfm_param) {
  av1_iht16x16_256_add_c(in, dest, stride, txfm_param);
}

#if CONFIG_HIGHBITDEPTH
void fht16x16_10(const int16_t *in, tran_low_t *out, int stride,
                 TxfmParam *txfm_param) {
  av1_fwd_txfm2d_16x16_c(in, out, stride, txfm_param->tx_type, 10);
}

void fht16x16_12(const int16_t *in, tran_low_t *out, int stride,
                 TxfmParam *txfm_param) {
  av1_fwd_txfm2d_16x16_c(in, out, stride, txfm_param->tx_type, 12);
}

void iht16x16_10(const tran_low_t *in, uint8_t *out, int stride,
                 const TxfmParam *txfm_param) {
  av1_inv_txfm2d_add_16x16_c(in, CONVERT_TO_SHORTPTR(out), stride,
                             txfm_param->tx_type, 10);
}

void iht16x16_12(const tran_low_t *in, uint8_t *out, int stride,
                 const TxfmParam *txfm_param) {
  av1_inv_txfm2d_add_16x16_c(in, CONVERT_TO_SHORTPTR(out), stride,
                             txfm_param->tx_type, 12);
}
#endif  // CONFIG_HIGHBITDEPTH

class Trans16x16TestBase {
 public:
  virtual ~Trans16x16TestBase() {}

 protected:
  virtual void RunFwdTxfm(int16_t *in, tran_low_t *out, int stride) = 0;

  virtual void RunInvTxfm(tran_low_t *out, uint8_t *dst, int stride) = 0;

  void RunAccuracyCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    uint32_t max_error = 0;
    int64_t total_error = 0;
    const int count_test_block = 10000;
    for (int i = 0; i < count_test_block; ++i) {
      DECLARE_ALIGNED(16, int16_t, test_input_block[kNumCoeffs]);
      DECLARE_ALIGNED(16, tran_low_t, test_temp_block[kNumCoeffs]);
      DECLARE_ALIGNED(16, uint8_t, dst[kNumCoeffs]);
      DECLARE_ALIGNED(16, uint8_t, src[kNumCoeffs]);
#if CONFIG_HIGHBITDEPTH
      DECLARE_ALIGNED(16, uint16_t, dst16[kNumCoeffs]);
      DECLARE_ALIGNED(16, uint16_t, src16[kNumCoeffs]);
#endif

      // Initialize a test block with input range [-mask_, mask_].
      for (int j = 0; j < kNumCoeffs; ++j) {
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
      if (bit_depth_ == AOM_BITS_8) {
        ASM_REGISTER_STATE_CHECK(RunInvTxfm(test_temp_block, dst, pitch_));
#if CONFIG_HIGHBITDEPTH
      } else {
        ASM_REGISTER_STATE_CHECK(
            RunInvTxfm(test_temp_block, CONVERT_TO_BYTEPTR(dst16), pitch_));
#endif
      }

      for (int j = 0; j < kNumCoeffs; ++j) {
#if CONFIG_HIGHBITDEPTH
        const int32_t diff =
            bit_depth_ == AOM_BITS_8 ? dst[j] - src[j] : dst16[j] - src16[j];
#else
        const int32_t diff = dst[j] - src[j];
#endif
        const uint32_t error = diff * diff;
        if (max_error < error) max_error = error;
        total_error += error;
      }
    }

    EXPECT_GE(1u << 2 * (bit_depth_ - 8), max_error)
        << "Error: 16x16 FHT/IHT has an individual round trip error > 1";

    EXPECT_GE(count_test_block << 2 * (bit_depth_ - 8), total_error)
        << "Error: 16x16 FHT/IHT has average round trip error > 1 per block";
  }

  void RunCoeffCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 1000;
    DECLARE_ALIGNED(16, int16_t, input_block[kNumCoeffs]);
    DECLARE_ALIGNED(16, tran_low_t, output_ref_block[kNumCoeffs]);
    DECLARE_ALIGNED(16, tran_low_t, output_block[kNumCoeffs]);

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-mask_, mask_].
      for (int j = 0; j < kNumCoeffs; ++j)
        input_block[j] = (rnd.Rand16() & mask_) - (rnd.Rand16() & mask_);

      fwd_txfm_ref(input_block, output_ref_block, pitch_, &txfm_param_);
      ASM_REGISTER_STATE_CHECK(RunFwdTxfm(input_block, output_block, pitch_));

      // The minimum quant value is 4.
      for (int j = 0; j < kNumCoeffs; ++j)
        EXPECT_EQ(output_block[j], output_ref_block[j]);
    }
  }

  void RunMemCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 1000;
    DECLARE_ALIGNED(16, int16_t, input_extreme_block[kNumCoeffs]);
    DECLARE_ALIGNED(16, tran_low_t, output_ref_block[kNumCoeffs]);
    DECLARE_ALIGNED(16, tran_low_t, output_block[kNumCoeffs]);

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-mask_, mask_].
      for (int j = 0; j < kNumCoeffs; ++j) {
        input_extreme_block[j] = rnd.Rand8() % 2 ? mask_ : -mask_;
      }
      if (i == 0) {
        for (int j = 0; j < kNumCoeffs; ++j) input_extreme_block[j] = mask_;
      } else if (i == 1) {
        for (int j = 0; j < kNumCoeffs; ++j) input_extreme_block[j] = -mask_;
      }

      fwd_txfm_ref(input_extreme_block, output_ref_block, pitch_, &txfm_param_);
      ASM_REGISTER_STATE_CHECK(
          RunFwdTxfm(input_extreme_block, output_block, pitch_));

      // The minimum quant value is 4.
      for (int j = 0; j < kNumCoeffs; ++j) {
        EXPECT_EQ(output_block[j], output_ref_block[j]);
        EXPECT_GE(4 * DCT_MAX_VALUE << (bit_depth_ - 8), abs(output_block[j]))
            << "Error: 16x16 FDCT has coefficient larger than 4*DCT_MAX_VALUE";
      }
    }
  }

  void RunQuantCheck(int dc_thred, int ac_thred) {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 100000;
    DECLARE_ALIGNED(16, int16_t, input_extreme_block[kNumCoeffs]);
    DECLARE_ALIGNED(16, tran_low_t, output_ref_block[kNumCoeffs]);

    DECLARE_ALIGNED(16, uint8_t, dst[kNumCoeffs]);
    DECLARE_ALIGNED(16, uint8_t, ref[kNumCoeffs]);
#if CONFIG_HIGHBITDEPTH
    DECLARE_ALIGNED(16, uint16_t, dst16[kNumCoeffs]);
    DECLARE_ALIGNED(16, uint16_t, ref16[kNumCoeffs]);
#endif

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-mask_, mask_].
      for (int j = 0; j < kNumCoeffs; ++j) {
        input_extreme_block[j] = rnd.Rand8() % 2 ? mask_ : -mask_;
      }
      if (i == 0)
        for (int j = 0; j < kNumCoeffs; ++j) input_extreme_block[j] = mask_;
      if (i == 1)
        for (int j = 0; j < kNumCoeffs; ++j) input_extreme_block[j] = -mask_;

      fwd_txfm_ref(input_extreme_block, output_ref_block, pitch_, &txfm_param_);

      // clear reconstructed pixel buffers
      memset(dst, 0, kNumCoeffs * sizeof(uint8_t));
      memset(ref, 0, kNumCoeffs * sizeof(uint8_t));
#if CONFIG_HIGHBITDEPTH
      memset(dst16, 0, kNumCoeffs * sizeof(uint16_t));
      memset(ref16, 0, kNumCoeffs * sizeof(uint16_t));
#endif

      // quantization with maximum allowed step sizes
      output_ref_block[0] = (output_ref_block[0] / dc_thred) * dc_thred;
      for (int j = 1; j < kNumCoeffs; ++j)
        output_ref_block[j] = (output_ref_block[j] / ac_thred) * ac_thred;
      if (bit_depth_ == AOM_BITS_8) {
        inv_txfm_ref(output_ref_block, ref, pitch_, &txfm_param_);
        ASM_REGISTER_STATE_CHECK(RunInvTxfm(output_ref_block, dst, pitch_));
#if CONFIG_HIGHBITDEPTH
      } else {
        inv_txfm_ref(output_ref_block, CONVERT_TO_BYTEPTR(ref16), pitch_,
                     &txfm_param_);
        ASM_REGISTER_STATE_CHECK(
            RunInvTxfm(output_ref_block, CONVERT_TO_BYTEPTR(dst16), pitch_));
#endif
      }
      if (bit_depth_ == AOM_BITS_8) {
        for (int j = 0; j < kNumCoeffs; ++j) EXPECT_EQ(ref[j], dst[j]);
#if CONFIG_HIGHBITDEPTH
      } else {
        for (int j = 0; j < kNumCoeffs; ++j) EXPECT_EQ(ref16[j], dst16[j]);
#endif
      }
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
    DECLARE_ALIGNED(16, uint16_t, dst16[kNumCoeffs]);
    DECLARE_ALIGNED(16, uint16_t, src16[kNumCoeffs]);
#endif  // CONFIG_HIGHBITDEPTH

    for (int i = 0; i < count_test_block; ++i) {
      double out_r[kNumCoeffs];

      // Initialize a test block with input range [-255, 255].
      for (int j = 0; j < kNumCoeffs; ++j) {
        if (bit_depth_ == AOM_BITS_8) {
          src[j] = rnd.Rand8();
          dst[j] = rnd.Rand8();
          in[j] = src[j] - dst[j];
#if CONFIG_HIGHBITDEPTH
        } else {
          src16[j] = rnd.Rand16() & mask_;
          dst16[j] = rnd.Rand16() & mask_;
          in[j] = src16[j] - dst16[j];
#endif  // CONFIG_HIGHBITDEPTH
        }
      }

      reference_16x16_dct_2d(in, out_r);
      for (int j = 0; j < kNumCoeffs; ++j)
        coeff[j] = static_cast<tran_low_t>(round(out_r[j]));

      if (bit_depth_ == AOM_BITS_8) {
        ASM_REGISTER_STATE_CHECK(RunInvTxfm(coeff, dst, 16));
#if CONFIG_HIGHBITDEPTH
      } else {
        ASM_REGISTER_STATE_CHECK(
            RunInvTxfm(coeff, CONVERT_TO_BYTEPTR(dst16), 16));
#endif  // CONFIG_HIGHBITDEPTH
      }

      for (int j = 0; j < kNumCoeffs; ++j) {
#if CONFIG_HIGHBITDEPTH
        const int diff =
            bit_depth_ == AOM_BITS_8 ? dst[j] - src[j] : dst16[j] - src16[j];
#else
        const int diff = dst[j] - src[j];
#endif  // CONFIG_HIGHBITDEPTH
        const uint32_t error = diff * diff;
        EXPECT_GE(1u, error)
            << "Error: 16x16 IDCT has error " << error << " at index " << j;
      }
    }
  }

  void CompareInvReference(IdctFunc ref_txfm, int thresh) {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 10000;
    const int eob = 10;
    const int16_t *scan = av1_default_scan_orders[TX_16X16].scan;
    DECLARE_ALIGNED(16, tran_low_t, coeff[kNumCoeffs]);
    DECLARE_ALIGNED(16, uint8_t, dst[kNumCoeffs]);
    DECLARE_ALIGNED(16, uint8_t, ref[kNumCoeffs]);
#if CONFIG_HIGHBITDEPTH
    DECLARE_ALIGNED(16, uint16_t, dst16[kNumCoeffs]);
    DECLARE_ALIGNED(16, uint16_t, ref16[kNumCoeffs]);
#endif  // CONFIG_HIGHBITDEPTH

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
#endif  // CONFIG_HIGHBITDEPTH
        }
      }
      if (bit_depth_ == AOM_BITS_8) {
        ref_txfm(coeff, ref, pitch_);
        ASM_REGISTER_STATE_CHECK(RunInvTxfm(coeff, dst, pitch_));
      } else {
#if CONFIG_HIGHBITDEPTH
        ref_txfm(coeff, CONVERT_TO_BYTEPTR(ref16), pitch_);
        ASM_REGISTER_STATE_CHECK(
            RunInvTxfm(coeff, CONVERT_TO_BYTEPTR(dst16), pitch_));
#endif  // CONFIG_HIGHBITDEPTH
      }

      for (int j = 0; j < kNumCoeffs; ++j) {
#if CONFIG_HIGHBITDEPTH
        const int diff =
            bit_depth_ == AOM_BITS_8 ? dst[j] - ref[j] : dst16[j] - ref16[j];
#else
        const int diff = dst[j] - ref[j];
#endif  // CONFIG_HIGHBITDEPTH
        const uint32_t error = diff * diff;
        EXPECT_EQ(0u, error) << "Error: 16x16 IDCT Comparison has error "
                             << error << " at index " << j;
      }
    }
  }

  int pitch_;
  aom_bit_depth_t bit_depth_;
  int mask_;
  FhtFunc fwd_txfm_ref;
  IhtFunc inv_txfm_ref;
  TxfmParam txfm_param_;
};

class Trans16x16DCT : public Trans16x16TestBase,
                      public ::testing::TestWithParam<Dct16x16Param> {
 public:
  virtual ~Trans16x16DCT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    bit_depth_ = GET_PARAM(3);
    pitch_ = 16;
    fwd_txfm_ref = fdct16x16_ref;
    inv_txfm_ref = idct16x16_ref;
    mask_ = (1 << bit_depth_) - 1;
    inv_txfm_ref = idct16x16_ref;
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

TEST_P(Trans16x16DCT, AccuracyCheck) { RunAccuracyCheck(); }

TEST_P(Trans16x16DCT, CoeffCheck) { RunCoeffCheck(); }

TEST_P(Trans16x16DCT, MemCheck) { RunMemCheck(); }

TEST_P(Trans16x16DCT, QuantCheck) {
  // Use maximally allowed quantization step sizes for DC and AC
  // coefficients respectively.
  RunQuantCheck(1336, 1828);
}

TEST_P(Trans16x16DCT, InvAccuracyCheck) { RunInvAccuracyCheck(); }

class Trans16x16HT : public Trans16x16TestBase,
                     public ::testing::TestWithParam<Ht16x16Param> {
 public:
  virtual ~Trans16x16HT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    bit_depth_ = GET_PARAM(3);
    pitch_ = 16;
    mask_ = (1 << bit_depth_) - 1;
    txfm_param_.tx_type = GET_PARAM(2);
#if CONFIG_HIGHBITDEPTH
    switch (bit_depth_) {
      case AOM_BITS_10:
        fwd_txfm_ref = fht16x16_10;
        inv_txfm_ref = iht16x16_10;
        break;
      case AOM_BITS_12:
        fwd_txfm_ref = fht16x16_12;
        inv_txfm_ref = iht16x16_12;
        break;
      default:
        fwd_txfm_ref = fht16x16_ref;
        inv_txfm_ref = iht16x16_ref;
        break;
    }
#else
    fwd_txfm_ref = fht16x16_ref;
    inv_txfm_ref = iht16x16_ref;
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

TEST_P(Trans16x16HT, AccuracyCheck) { RunAccuracyCheck(); }

TEST_P(Trans16x16HT, CoeffCheck) { RunCoeffCheck(); }

TEST_P(Trans16x16HT, MemCheck) { RunMemCheck(); }

TEST_P(Trans16x16HT, QuantCheck) {
  // The encoder skips any non-DC intra prediction modes,
  // when the quantization step size goes beyond 988.
  RunQuantCheck(429, 729);
}

class InvTrans16x16DCT : public Trans16x16TestBase,
                         public ::testing::TestWithParam<Idct16x16Param> {
 public:
  virtual ~InvTrans16x16DCT() {}

  virtual void SetUp() {
    ref_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    thresh_ = GET_PARAM(2);
    bit_depth_ = GET_PARAM(3);
    pitch_ = 16;
    mask_ = (1 << bit_depth_) - 1;
  }
  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  void RunFwdTxfm(int16_t * /*in*/, tran_low_t * /*out*/, int /*stride*/) {}
  void RunInvTxfm(tran_low_t *out, uint8_t *dst, int stride) {
    inv_txfm_(out, dst, stride);
  }

  IdctFunc ref_txfm_;
  IdctFunc inv_txfm_;
  int thresh_;
};

TEST_P(InvTrans16x16DCT, CompareReference) {
  CompareInvReference(ref_txfm_, thresh_);
}

class PartialTrans16x16Test : public ::testing::TestWithParam<
                                  std::tr1::tuple<FdctFunc, aom_bit_depth_t> > {
 public:
  virtual ~PartialTrans16x16Test() {}
  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    bit_depth_ = GET_PARAM(1);
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  aom_bit_depth_t bit_depth_;
  FdctFunc fwd_txfm_;
};

TEST_P(PartialTrans16x16Test, Extremes) {
#if CONFIG_HIGHBITDEPTH
  const int16_t maxval =
      static_cast<int16_t>(clip_pixel_highbd(1 << 30, bit_depth_));
#else
  const int16_t maxval = 255;
#endif
  const int minval = -maxval;
  DECLARE_ALIGNED(16, int16_t, input[kNumCoeffs]);
  DECLARE_ALIGNED(16, tran_low_t, output[kNumCoeffs]);

  for (int i = 0; i < kNumCoeffs; ++i) input[i] = maxval;
  output[0] = 0;
  ASM_REGISTER_STATE_CHECK(fwd_txfm_(input, output, 16));
  EXPECT_EQ((maxval * kNumCoeffs) >> 1, output[0]);

  for (int i = 0; i < kNumCoeffs; ++i) input[i] = minval;
  output[0] = 0;
  ASM_REGISTER_STATE_CHECK(fwd_txfm_(input, output, 16));
  EXPECT_EQ((minval * kNumCoeffs) >> 1, output[0]);
}

TEST_P(PartialTrans16x16Test, Random) {
#if CONFIG_HIGHBITDEPTH
  const int16_t maxval =
      static_cast<int16_t>(clip_pixel_highbd(1 << 30, bit_depth_));
#else
  const int16_t maxval = 255;
#endif
  DECLARE_ALIGNED(16, int16_t, input[kNumCoeffs]);
  DECLARE_ALIGNED(16, tran_low_t, output[kNumCoeffs]);
  ACMRandom rnd(ACMRandom::DeterministicSeed());

  int sum = 0;
  for (int i = 0; i < kNumCoeffs; ++i) {
    const int val = (i & 1) ? -rnd(maxval + 1) : rnd(maxval + 1);
    input[i] = val;
    sum += val;
  }
  output[0] = 0;
  ASM_REGISTER_STATE_CHECK(fwd_txfm_(input, output, 16));
  EXPECT_EQ(sum >> 1, output[0]);
}

using std::tr1::make_tuple;

#if CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(C, Trans16x16DCT,
                        ::testing::Values(make_tuple(&aom_fdct16x16_c,
                                                     &aom_idct16x16_256_add_c,
                                                     DCT_DCT, AOM_BITS_8)));
#else
INSTANTIATE_TEST_CASE_P(C, Trans16x16DCT,
                        ::testing::Values(make_tuple(&aom_fdct16x16_c,
                                                     &aom_idct16x16_256_add_c,
                                                     DCT_DCT, AOM_BITS_8)));
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    C, Trans16x16HT,
    ::testing::Values(
        make_tuple(&fht16x16_10, &iht16x16_10, DCT_DCT, AOM_BITS_10),
        make_tuple(&fht16x16_10, &iht16x16_10, ADST_DCT, AOM_BITS_10),
        make_tuple(&fht16x16_10, &iht16x16_10, DCT_ADST, AOM_BITS_10),
        make_tuple(&fht16x16_10, &iht16x16_10, ADST_ADST, AOM_BITS_10),
        make_tuple(&fht16x16_12, &iht16x16_12, DCT_DCT, AOM_BITS_12),
        make_tuple(&fht16x16_12, &iht16x16_12, ADST_DCT, AOM_BITS_12),
        make_tuple(&fht16x16_12, &iht16x16_12, DCT_ADST, AOM_BITS_12),
        make_tuple(&fht16x16_12, &iht16x16_12, ADST_ADST, AOM_BITS_12),
        make_tuple(&av1_fht16x16_c, &av1_iht16x16_256_add_c, DCT_DCT,
                   AOM_BITS_8),
        make_tuple(&av1_fht16x16_c, &av1_iht16x16_256_add_c, ADST_DCT,
                   AOM_BITS_8),
        make_tuple(&av1_fht16x16_c, &av1_iht16x16_256_add_c, DCT_ADST,
                   AOM_BITS_8),
        make_tuple(&av1_fht16x16_c, &av1_iht16x16_256_add_c, ADST_ADST,
                   AOM_BITS_8)));
#else
INSTANTIATE_TEST_CASE_P(
    C, Trans16x16HT,
    ::testing::Values(make_tuple(&av1_fht16x16_c, &av1_iht16x16_256_add_c,
                                 DCT_DCT, AOM_BITS_8),
                      make_tuple(&av1_fht16x16_c, &av1_iht16x16_256_add_c,
                                 ADST_DCT, AOM_BITS_8),
                      make_tuple(&av1_fht16x16_c, &av1_iht16x16_256_add_c,
                                 DCT_ADST, AOM_BITS_8),
                      make_tuple(&av1_fht16x16_c, &av1_iht16x16_256_add_c,
                                 ADST_ADST, AOM_BITS_8)));
#endif  // CONFIG_HIGHBITDEPTH

#if HAVE_NEON_ASM && !CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    NEON, Trans16x16DCT,
    ::testing::Values(make_tuple(&aom_fdct16x16_c, &aom_idct16x16_256_add_neon,
                                 DCT_DCT, AOM_BITS_8)));
#endif

#if HAVE_SSE2 && !CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(SSE2, Trans16x16DCT,
                        ::testing::Values(make_tuple(
                            &aom_fdct16x16_sse2, &aom_idct16x16_256_add_sse2,
                            DCT_DCT, AOM_BITS_8)));
#if !CONFIG_DAALA_DCT16
INSTANTIATE_TEST_CASE_P(
    SSE2, Trans16x16HT,
    ::testing::Values(make_tuple(&av1_fht16x16_sse2, &av1_iht16x16_256_add_sse2,
                                 DCT_DCT, AOM_BITS_8),
                      make_tuple(&av1_fht16x16_sse2, &av1_iht16x16_256_add_sse2,
                                 ADST_DCT, AOM_BITS_8),
                      make_tuple(&av1_fht16x16_sse2, &av1_iht16x16_256_add_sse2,
                                 DCT_ADST, AOM_BITS_8),
                      make_tuple(&av1_fht16x16_sse2, &av1_iht16x16_256_add_sse2,
                                 ADST_ADST, AOM_BITS_8)));
#endif  // CONFIG_DAALA_DCT16
#endif  // HAVE_SSE2 && !CONFIG_HIGHBITDEPTH

#if HAVE_SSE2 && CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(SSE2, Trans16x16DCT,
                        ::testing::Values(make_tuple(&aom_fdct16x16_sse2,
                                                     &aom_idct16x16_256_add_c,
                                                     DCT_DCT, AOM_BITS_8)));
#if !CONFIG_DAALA_DCT16
INSTANTIATE_TEST_CASE_P(
    SSE2, Trans16x16HT,
    ::testing::Values(make_tuple(&av1_fht16x16_sse2, &av1_iht16x16_256_add_c,
                                 DCT_DCT, AOM_BITS_8),
                      make_tuple(&av1_fht16x16_sse2, &av1_iht16x16_256_add_c,
                                 ADST_DCT, AOM_BITS_8),
                      make_tuple(&av1_fht16x16_sse2, &av1_iht16x16_256_add_c,
                                 DCT_ADST, AOM_BITS_8),
                      make_tuple(&av1_fht16x16_sse2, &av1_iht16x16_256_add_c,
                                 ADST_ADST, AOM_BITS_8)));
#endif
#endif  // HAVE_SSE2 && CONFIG_HIGHBITDEPTH

#if HAVE_MSA && !CONFIG_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(MSA, Trans16x16DCT,
                        ::testing::Values(make_tuple(&aom_fdct16x16_msa,
                                                     &aom_idct16x16_256_add_msa,
                                                     DCT_DCT, AOM_BITS_8)));
#if !CONFIG_EXT_TX && !CONFIG_DAALA_DCT16
// TODO(yaowu): re-enable this after msa versions are updated to match C.
INSTANTIATE_TEST_CASE_P(
    DISABLED_MSA, Trans16x16HT,
    ::testing::Values(make_tuple(&av1_fht16x16_msa, &av1_iht16x16_256_add_msa,
                                 DCT_DCT, AOM_BITS_8),
                      make_tuple(&av1_fht16x16_msa, &av1_iht16x16_256_add_msa,
                                 ADST_DCT, AOM_BITS_8),
                      make_tuple(&av1_fht16x16_msa, &av1_iht16x16_256_add_msa,
                                 DCT_ADST, AOM_BITS_8),
                      make_tuple(&av1_fht16x16_msa, &av1_iht16x16_256_add_msa,
                                 ADST_ADST, AOM_BITS_8)));
#endif  // !CONFIG_EXT_TX && !CONFIG_DAALA_DCT16
#endif  // HAVE_MSA && !CONFIG_HIGHBITDEPTH
}  // namespace
