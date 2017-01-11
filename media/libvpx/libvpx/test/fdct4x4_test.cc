/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"

#include "./vp9_rtcd.h"
#include "vp9/common/vp9_entropy.h"
#include "vpx/vpx_codec.h"
#include "vpx/vpx_integer.h"
#include "vpx_ports/mem.h"

using libvpx_test::ACMRandom;

namespace {
const int kNumCoeffs = 16;
typedef void (*FdctFunc)(const int16_t *in, tran_low_t *out, int stride);
typedef void (*IdctFunc)(const tran_low_t *in, uint8_t *out, int stride);
typedef void (*FhtFunc)(const int16_t *in, tran_low_t *out, int stride,
                        int tx_type);
typedef void (*IhtFunc)(const tran_low_t *in, uint8_t *out, int stride,
                        int tx_type);

typedef std::tr1::tuple<FdctFunc, IdctFunc, int, vpx_bit_depth_t> Dct4x4Param;
typedef std::tr1::tuple<FhtFunc, IhtFunc, int, vpx_bit_depth_t> Ht4x4Param;

void fdct4x4_ref(const int16_t *in, tran_low_t *out, int stride,
                 int tx_type) {
  vp9_fdct4x4_c(in, out, stride);
}

void fht4x4_ref(const int16_t *in, tran_low_t *out, int stride, int tx_type) {
  vp9_fht4x4_c(in, out, stride, tx_type);
}

void fwht4x4_ref(const int16_t *in, tran_low_t *out, int stride,
                 int tx_type) {
  vp9_fwht4x4_c(in, out, stride);
}

#if CONFIG_VP9_HIGHBITDEPTH
void idct4x4_10(const tran_low_t *in, uint8_t *out, int stride) {
  vp9_highbd_idct4x4_16_add_c(in, out, stride, 10);
}

void idct4x4_12(const tran_low_t *in, uint8_t *out, int stride) {
  vp9_highbd_idct4x4_16_add_c(in, out, stride, 12);
}

void iht4x4_10(const tran_low_t *in, uint8_t *out, int stride, int tx_type) {
  vp9_highbd_iht4x4_16_add_c(in, out, stride, tx_type, 10);
}

void iht4x4_12(const tran_low_t *in, uint8_t *out, int stride, int tx_type) {
  vp9_highbd_iht4x4_16_add_c(in, out, stride, tx_type, 12);
}

void iwht4x4_10(const tran_low_t *in, uint8_t *out, int stride) {
  vp9_highbd_iwht4x4_16_add_c(in, out, stride, 10);
}

void iwht4x4_12(const tran_low_t *in, uint8_t *out, int stride) {
  vp9_highbd_iwht4x4_16_add_c(in, out, stride, 12);
}

#if HAVE_SSE2
void idct4x4_10_sse2(const tran_low_t *in, uint8_t *out, int stride) {
  vp9_highbd_idct4x4_16_add_sse2(in, out, stride, 10);
}

void idct4x4_12_sse2(const tran_low_t *in, uint8_t *out, int stride) {
  vp9_highbd_idct4x4_16_add_sse2(in, out, stride, 12);
}
#endif  // HAVE_SSE2
#endif  // CONFIG_VP9_HIGHBITDEPTH

class Trans4x4TestBase {
 public:
  virtual ~Trans4x4TestBase() {}

 protected:
  virtual void RunFwdTxfm(const int16_t *in, tran_low_t *out, int stride) = 0;

  virtual void RunInvTxfm(const tran_low_t *out, uint8_t *dst, int stride) = 0;

  void RunAccuracyCheck(int limit) {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    uint32_t max_error = 0;
    int64_t total_error = 0;
    const int count_test_block = 10000;
    for (int i = 0; i < count_test_block; ++i) {
      DECLARE_ALIGNED(16, int16_t, test_input_block[kNumCoeffs]);
      DECLARE_ALIGNED(16, tran_low_t, test_temp_block[kNumCoeffs]);
      DECLARE_ALIGNED(16, uint8_t, dst[kNumCoeffs]);
      DECLARE_ALIGNED(16, uint8_t, src[kNumCoeffs]);
#if CONFIG_VP9_HIGHBITDEPTH
      DECLARE_ALIGNED(16, uint16_t, dst16[kNumCoeffs]);
      DECLARE_ALIGNED(16, uint16_t, src16[kNumCoeffs]);
#endif

      // Initialize a test block with input range [-255, 255].
      for (int j = 0; j < kNumCoeffs; ++j) {
        if (bit_depth_ == VPX_BITS_8) {
          src[j] = rnd.Rand8();
          dst[j] = rnd.Rand8();
          test_input_block[j] = src[j] - dst[j];
#if CONFIG_VP9_HIGHBITDEPTH
        } else {
          src16[j] = rnd.Rand16() & mask_;
          dst16[j] = rnd.Rand16() & mask_;
          test_input_block[j] = src16[j] - dst16[j];
#endif
        }
      }

      ASM_REGISTER_STATE_CHECK(RunFwdTxfm(test_input_block,
                                          test_temp_block, pitch_));
      if (bit_depth_ == VPX_BITS_8) {
        ASM_REGISTER_STATE_CHECK(RunInvTxfm(test_temp_block, dst, pitch_));
#if CONFIG_VP9_HIGHBITDEPTH
      } else {
        ASM_REGISTER_STATE_CHECK(RunInvTxfm(test_temp_block,
                                            CONVERT_TO_BYTEPTR(dst16), pitch_));
#endif
      }

      for (int j = 0; j < kNumCoeffs; ++j) {
#if CONFIG_VP9_HIGHBITDEPTH
        const uint32_t diff =
            bit_depth_ == VPX_BITS_8 ? dst[j] - src[j] : dst16[j] - src16[j];
#else
        ASSERT_EQ(VPX_BITS_8, bit_depth_);
        const uint32_t diff = dst[j] - src[j];
#endif
        const uint32_t error = diff * diff;
        if (max_error < error)
          max_error = error;
        total_error += error;
      }
    }

    EXPECT_GE(static_cast<uint32_t>(limit), max_error)
        << "Error: 4x4 FHT/IHT has an individual round trip error > "
        << limit;

    EXPECT_GE(count_test_block * limit, total_error)
        << "Error: 4x4 FHT/IHT has average round trip error > " << limit
        << " per block";
  }

  void RunCoeffCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 5000;
    DECLARE_ALIGNED(16, int16_t, input_block[kNumCoeffs]);
    DECLARE_ALIGNED(16, tran_low_t, output_ref_block[kNumCoeffs]);
    DECLARE_ALIGNED(16, tran_low_t, output_block[kNumCoeffs]);

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-mask_, mask_].
      for (int j = 0; j < kNumCoeffs; ++j)
        input_block[j] = (rnd.Rand16() & mask_) - (rnd.Rand16() & mask_);

      fwd_txfm_ref(input_block, output_ref_block, pitch_, tx_type_);
      ASM_REGISTER_STATE_CHECK(RunFwdTxfm(input_block, output_block, pitch_));

      // The minimum quant value is 4.
      for (int j = 0; j < kNumCoeffs; ++j)
        EXPECT_EQ(output_block[j], output_ref_block[j]);
    }
  }

  void RunMemCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 5000;
    DECLARE_ALIGNED(16, int16_t, input_extreme_block[kNumCoeffs]);
    DECLARE_ALIGNED(16, tran_low_t, output_ref_block[kNumCoeffs]);
    DECLARE_ALIGNED(16, tran_low_t, output_block[kNumCoeffs]);

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-mask_, mask_].
      for (int j = 0; j < kNumCoeffs; ++j) {
        input_extreme_block[j] = rnd.Rand8() % 2 ? mask_ : -mask_;
      }
      if (i == 0) {
        for (int j = 0; j < kNumCoeffs; ++j)
          input_extreme_block[j] = mask_;
      } else if (i == 1) {
        for (int j = 0; j < kNumCoeffs; ++j)
          input_extreme_block[j] = -mask_;
      }

      fwd_txfm_ref(input_extreme_block, output_ref_block, pitch_, tx_type_);
      ASM_REGISTER_STATE_CHECK(RunFwdTxfm(input_extreme_block,
                                          output_block, pitch_));

      // The minimum quant value is 4.
      for (int j = 0; j < kNumCoeffs; ++j) {
        EXPECT_EQ(output_block[j], output_ref_block[j]);
        EXPECT_GE(4 * DCT_MAX_VALUE << (bit_depth_ - 8), abs(output_block[j]))
            << "Error: 4x4 FDCT has coefficient larger than 4*DCT_MAX_VALUE";
      }
    }
  }

  void RunInvAccuracyCheck(int limit) {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 1000;
    DECLARE_ALIGNED(16, int16_t, in[kNumCoeffs]);
    DECLARE_ALIGNED(16, tran_low_t, coeff[kNumCoeffs]);
    DECLARE_ALIGNED(16, uint8_t, dst[kNumCoeffs]);
    DECLARE_ALIGNED(16, uint8_t, src[kNumCoeffs]);
#if CONFIG_VP9_HIGHBITDEPTH
    DECLARE_ALIGNED(16, uint16_t, dst16[kNumCoeffs]);
    DECLARE_ALIGNED(16, uint16_t, src16[kNumCoeffs]);
#endif

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-mask_, mask_].
      for (int j = 0; j < kNumCoeffs; ++j) {
        if (bit_depth_ == VPX_BITS_8) {
          src[j] = rnd.Rand8();
          dst[j] = rnd.Rand8();
          in[j] = src[j] - dst[j];
#if CONFIG_VP9_HIGHBITDEPTH
        } else {
          src16[j] = rnd.Rand16() & mask_;
          dst16[j] = rnd.Rand16() & mask_;
          in[j] = src16[j] - dst16[j];
#endif
        }
      }

      fwd_txfm_ref(in, coeff, pitch_, tx_type_);

      if (bit_depth_ == VPX_BITS_8) {
        ASM_REGISTER_STATE_CHECK(RunInvTxfm(coeff, dst, pitch_));
#if CONFIG_VP9_HIGHBITDEPTH
      } else {
        ASM_REGISTER_STATE_CHECK(RunInvTxfm(coeff, CONVERT_TO_BYTEPTR(dst16),
                                            pitch_));
#endif
      }

      for (int j = 0; j < kNumCoeffs; ++j) {
#if CONFIG_VP9_HIGHBITDEPTH
        const uint32_t diff =
            bit_depth_ == VPX_BITS_8 ? dst[j] - src[j] : dst16[j] - src16[j];
#else
        const uint32_t diff = dst[j] - src[j];
#endif
        const uint32_t error = diff * diff;
        EXPECT_GE(static_cast<uint32_t>(limit), error)
            << "Error: 4x4 IDCT has error " << error
            << " at index " << j;
      }
    }
  }

  int pitch_;
  int tx_type_;
  FhtFunc fwd_txfm_ref;
  vpx_bit_depth_t bit_depth_;
  int mask_;
};

class Trans4x4DCT
    : public Trans4x4TestBase,
      public ::testing::TestWithParam<Dct4x4Param> {
 public:
  virtual ~Trans4x4DCT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    tx_type_  = GET_PARAM(2);
    pitch_    = 4;
    fwd_txfm_ref = fdct4x4_ref;
    bit_depth_ = GET_PARAM(3);
    mask_ = (1 << bit_depth_) - 1;
  }
  virtual void TearDown() { libvpx_test::ClearSystemState(); }

 protected:
  void RunFwdTxfm(const int16_t *in, tran_low_t *out, int stride) {
    fwd_txfm_(in, out, stride);
  }
  void RunInvTxfm(const tran_low_t *out, uint8_t *dst, int stride) {
    inv_txfm_(out, dst, stride);
  }

  FdctFunc fwd_txfm_;
  IdctFunc inv_txfm_;
};

TEST_P(Trans4x4DCT, AccuracyCheck) {
  RunAccuracyCheck(1);
}

TEST_P(Trans4x4DCT, CoeffCheck) {
  RunCoeffCheck();
}

TEST_P(Trans4x4DCT, MemCheck) {
  RunMemCheck();
}

TEST_P(Trans4x4DCT, InvAccuracyCheck) {
  RunInvAccuracyCheck(1);
}

class Trans4x4HT
    : public Trans4x4TestBase,
      public ::testing::TestWithParam<Ht4x4Param> {
 public:
  virtual ~Trans4x4HT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    tx_type_  = GET_PARAM(2);
    pitch_    = 4;
    fwd_txfm_ref = fht4x4_ref;
    bit_depth_ = GET_PARAM(3);
    mask_ = (1 << bit_depth_) - 1;
  }
  virtual void TearDown() { libvpx_test::ClearSystemState(); }

 protected:
  void RunFwdTxfm(const int16_t *in, tran_low_t *out, int stride) {
    fwd_txfm_(in, out, stride, tx_type_);
  }

  void RunInvTxfm(const tran_low_t *out, uint8_t *dst, int stride) {
    inv_txfm_(out, dst, stride, tx_type_);
  }

  FhtFunc fwd_txfm_;
  IhtFunc inv_txfm_;
};

TEST_P(Trans4x4HT, AccuracyCheck) {
  RunAccuracyCheck(1);
}

TEST_P(Trans4x4HT, CoeffCheck) {
  RunCoeffCheck();
}

TEST_P(Trans4x4HT, MemCheck) {
  RunMemCheck();
}

TEST_P(Trans4x4HT, InvAccuracyCheck) {
  RunInvAccuracyCheck(1);
}

class Trans4x4WHT
    : public Trans4x4TestBase,
      public ::testing::TestWithParam<Dct4x4Param> {
 public:
  virtual ~Trans4x4WHT() {}

  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    tx_type_  = GET_PARAM(2);
    pitch_    = 4;
    fwd_txfm_ref = fwht4x4_ref;
    bit_depth_ = GET_PARAM(3);
    mask_ = (1 << bit_depth_) - 1;
  }
  virtual void TearDown() { libvpx_test::ClearSystemState(); }

 protected:
  void RunFwdTxfm(const int16_t *in, tran_low_t *out, int stride) {
    fwd_txfm_(in, out, stride);
  }
  void RunInvTxfm(const tran_low_t *out, uint8_t *dst, int stride) {
    inv_txfm_(out, dst, stride);
  }

  FdctFunc fwd_txfm_;
  IdctFunc inv_txfm_;
};

TEST_P(Trans4x4WHT, AccuracyCheck) {
  RunAccuracyCheck(0);
}

TEST_P(Trans4x4WHT, CoeffCheck) {
  RunCoeffCheck();
}

TEST_P(Trans4x4WHT, MemCheck) {
  RunMemCheck();
}

TEST_P(Trans4x4WHT, InvAccuracyCheck) {
  RunInvAccuracyCheck(0);
}
using std::tr1::make_tuple;

#if CONFIG_VP9_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    C, Trans4x4DCT,
    ::testing::Values(
        make_tuple(&vp9_highbd_fdct4x4_c, &idct4x4_10, 0, VPX_BITS_10),
        make_tuple(&vp9_highbd_fdct4x4_c, &idct4x4_12, 0, VPX_BITS_12),
        make_tuple(&vp9_fdct4x4_c, &vp9_idct4x4_16_add_c, 0, VPX_BITS_8)));
#else
INSTANTIATE_TEST_CASE_P(
    C, Trans4x4DCT,
    ::testing::Values(
        make_tuple(&vp9_fdct4x4_c, &vp9_idct4x4_16_add_c, 0, VPX_BITS_8)));
#endif  // CONFIG_VP9_HIGHBITDEPTH

#if CONFIG_VP9_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    C, Trans4x4HT,
    ::testing::Values(
        make_tuple(&vp9_highbd_fht4x4_c, &iht4x4_10, 0, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht4x4_c, &iht4x4_10, 1, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht4x4_c, &iht4x4_10, 2, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht4x4_c, &iht4x4_10, 3, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht4x4_c, &iht4x4_12, 0, VPX_BITS_12),
        make_tuple(&vp9_highbd_fht4x4_c, &iht4x4_12, 1, VPX_BITS_12),
        make_tuple(&vp9_highbd_fht4x4_c, &iht4x4_12, 2, VPX_BITS_12),
        make_tuple(&vp9_highbd_fht4x4_c, &iht4x4_12, 3, VPX_BITS_12),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 0, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 1, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 2, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 3, VPX_BITS_8)));
#else
INSTANTIATE_TEST_CASE_P(
    C, Trans4x4HT,
    ::testing::Values(
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 0, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 1, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 2, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 3, VPX_BITS_8)));
#endif  // CONFIG_VP9_HIGHBITDEPTH

#if CONFIG_VP9_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    C, Trans4x4WHT,
    ::testing::Values(
        make_tuple(&vp9_highbd_fwht4x4_c, &iwht4x4_10, 0, VPX_BITS_10),
        make_tuple(&vp9_highbd_fwht4x4_c, &iwht4x4_12, 0, VPX_BITS_12),
        make_tuple(&vp9_fwht4x4_c, &vp9_iwht4x4_16_add_c, 0, VPX_BITS_8)));
#else
INSTANTIATE_TEST_CASE_P(
    C, Trans4x4WHT,
    ::testing::Values(
        make_tuple(&vp9_fwht4x4_c, &vp9_iwht4x4_16_add_c, 0, VPX_BITS_8)));
#endif  // CONFIG_VP9_HIGHBITDEPTH

#if HAVE_NEON_ASM && !CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_CASE_P(
    NEON, Trans4x4DCT,
    ::testing::Values(
        make_tuple(&vp9_fdct4x4_c,
                   &vp9_idct4x4_16_add_neon, 0, VPX_BITS_8)));
#endif  // HAVE_NEON_ASM && !CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE

#if HAVE_NEON && !CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_CASE_P(
    NEON, Trans4x4HT,
    ::testing::Values(
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_neon, 0, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_neon, 1, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_neon, 2, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_neon, 3, VPX_BITS_8)));
#endif  // HAVE_NEON && !CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE

#if CONFIG_USE_X86INC && HAVE_MMX && !CONFIG_VP9_HIGHBITDEPTH && \
    !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_CASE_P(
    MMX, Trans4x4WHT,
    ::testing::Values(
        make_tuple(&vp9_fwht4x4_mmx, &vp9_iwht4x4_16_add_c, 0, VPX_BITS_8)));
#endif

#if HAVE_SSE2 && !CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_CASE_P(
    SSE2, Trans4x4DCT,
    ::testing::Values(
        make_tuple(&vp9_fdct4x4_sse2,
                   &vp9_idct4x4_16_add_sse2, 0, VPX_BITS_8)));
INSTANTIATE_TEST_CASE_P(
    SSE2, Trans4x4HT,
    ::testing::Values(
        make_tuple(&vp9_fht4x4_sse2, &vp9_iht4x4_16_add_sse2, 0, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_sse2, &vp9_iht4x4_16_add_sse2, 1, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_sse2, &vp9_iht4x4_16_add_sse2, 2, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_sse2, &vp9_iht4x4_16_add_sse2, 3, VPX_BITS_8)));
#endif  // HAVE_SSE2 && !CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE

#if HAVE_SSE2 && CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_CASE_P(
    SSE2, Trans4x4DCT,
    ::testing::Values(
        make_tuple(&vp9_highbd_fdct4x4_c,    &idct4x4_10_sse2, 0, VPX_BITS_10),
        make_tuple(&vp9_highbd_fdct4x4_sse2, &idct4x4_10_sse2, 0, VPX_BITS_10),
        make_tuple(&vp9_highbd_fdct4x4_c,    &idct4x4_12_sse2, 0, VPX_BITS_12),
        make_tuple(&vp9_highbd_fdct4x4_sse2, &idct4x4_12_sse2, 0, VPX_BITS_12),
        make_tuple(&vp9_fdct4x4_sse2,      &vp9_idct4x4_16_add_c, 0,
                   VPX_BITS_8)));

INSTANTIATE_TEST_CASE_P(
    SSE2, Trans4x4HT,
    ::testing::Values(
        make_tuple(&vp9_highbd_fht4x4_sse2, &iht4x4_10, 0, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht4x4_sse2, &iht4x4_10, 1, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht4x4_sse2, &iht4x4_10, 2, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht4x4_sse2, &iht4x4_10, 3, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht4x4_sse2, &iht4x4_12, 0, VPX_BITS_12),
        make_tuple(&vp9_highbd_fht4x4_sse2, &iht4x4_12, 1, VPX_BITS_12),
        make_tuple(&vp9_highbd_fht4x4_sse2, &iht4x4_12, 2, VPX_BITS_12),
        make_tuple(&vp9_highbd_fht4x4_sse2, &iht4x4_12, 3, VPX_BITS_12),
        make_tuple(&vp9_fht4x4_sse2, &vp9_iht4x4_16_add_c, 0, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_sse2, &vp9_iht4x4_16_add_c, 1, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_sse2, &vp9_iht4x4_16_add_c, 2, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_sse2, &vp9_iht4x4_16_add_c, 3, VPX_BITS_8)));
#endif  // HAVE_SSE2 && CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE

#if HAVE_MSA && !CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_CASE_P(
    MSA, Trans4x4DCT,
    ::testing::Values(
        make_tuple(&vp9_fdct4x4_c, &vp9_idct4x4_16_add_msa, 1, VPX_BITS_8)));
INSTANTIATE_TEST_CASE_P(
    MSA, Trans4x4HT,
    ::testing::Values(
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_msa, 0, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_msa, 1, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_msa, 2, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_msa, 3, VPX_BITS_8)));
#endif  // HAVE_MSA && !CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
}  // namespace
