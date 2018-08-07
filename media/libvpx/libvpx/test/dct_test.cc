/*
 *  Copyright (c) 2017 The WebM project authors. All Rights Reserved.
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

#include "./vp9_rtcd.h"
#include "./vpx_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/buffer.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "vp9/common/vp9_entropy.h"
#include "vpx/vpx_codec.h"
#include "vpx/vpx_integer.h"
#include "vpx_ports/mem.h"

using libvpx_test::ACMRandom;
using libvpx_test::Buffer;
using std::tr1::tuple;
using std::tr1::make_tuple;

namespace {
typedef void (*FdctFunc)(const int16_t *in, tran_low_t *out, int stride);
typedef void (*IdctFunc)(const tran_low_t *in, uint8_t *out, int stride);
typedef void (*FhtFunc)(const int16_t *in, tran_low_t *out, int stride,
                        int tx_type);
typedef void (*FhtFuncRef)(const Buffer<int16_t> &in, Buffer<tran_low_t> *out,
                           int size, int tx_type);
typedef void (*IhtFunc)(const tran_low_t *in, uint8_t *out, int stride,
                        int tx_type);

/* forward transform, inverse transform, size, transform type, bit depth */
typedef tuple<FdctFunc, IdctFunc, int, int, vpx_bit_depth_t> DctParam;
typedef tuple<FhtFunc, IhtFunc, int, int, vpx_bit_depth_t> HtParam;

void fdct_ref(const Buffer<int16_t> &in, Buffer<tran_low_t> *out, int size,
              int /*tx_type*/) {
  const int16_t *i = in.TopLeftPixel();
  const int i_stride = in.stride();
  tran_low_t *o = out->TopLeftPixel();
  if (size == 4) {
    vpx_fdct4x4_c(i, o, i_stride);
  } else if (size == 8) {
    vpx_fdct8x8_c(i, o, i_stride);
  } else if (size == 16) {
    vpx_fdct16x16_c(i, o, i_stride);
  } else if (size == 32) {
    vpx_fdct32x32_c(i, o, i_stride);
  }
}

void fht_ref(const Buffer<int16_t> &in, Buffer<tran_low_t> *out, int size,
             int tx_type) {
  const int16_t *i = in.TopLeftPixel();
  const int i_stride = in.stride();
  tran_low_t *o = out->TopLeftPixel();
  if (size == 4) {
    vp9_fht4x4_c(i, o, i_stride, tx_type);
  } else if (size == 8) {
    vp9_fht8x8_c(i, o, i_stride, tx_type);
  } else if (size == 16) {
    vp9_fht16x16_c(i, o, i_stride, tx_type);
  }
}

void fwht_ref(const Buffer<int16_t> &in, Buffer<tran_low_t> *out, int size,
              int /*tx_type*/) {
  ASSERT_EQ(size, 4);
  vp9_fwht4x4_c(in.TopLeftPixel(), out->TopLeftPixel(), in.stride());
}

#if CONFIG_VP9_HIGHBITDEPTH
#define idctNxN(n, coeffs, bitdepth)                                       \
  void idct##n##x##n##_##bitdepth(const tran_low_t *in, uint8_t *out,      \
                                  int stride) {                            \
    vpx_highbd_idct##n##x##n##_##coeffs##_add_c(in, CAST_TO_SHORTPTR(out), \
                                                stride, bitdepth);         \
  }

idctNxN(4, 16, 10);
idctNxN(4, 16, 12);
idctNxN(8, 64, 10);
idctNxN(8, 64, 12);
idctNxN(16, 256, 10);
idctNxN(16, 256, 12);
idctNxN(32, 1024, 10);
idctNxN(32, 1024, 12);

#define ihtNxN(n, coeffs, bitdepth)                                        \
  void iht##n##x##n##_##bitdepth(const tran_low_t *in, uint8_t *out,       \
                                 int stride, int tx_type) {                \
    vp9_highbd_iht##n##x##n##_##coeffs##_add_c(in, CAST_TO_SHORTPTR(out),  \
                                               stride, tx_type, bitdepth); \
  }

ihtNxN(4, 16, 10);
ihtNxN(4, 16, 12);
ihtNxN(8, 64, 10);
ihtNxN(8, 64, 12);
ihtNxN(16, 256, 10);
// ihtNxN(16, 256, 12);

void iwht4x4_10(const tran_low_t *in, uint8_t *out, int stride) {
  vpx_highbd_iwht4x4_16_add_c(in, CAST_TO_SHORTPTR(out), stride, 10);
}

void iwht4x4_12(const tran_low_t *in, uint8_t *out, int stride) {
  vpx_highbd_iwht4x4_16_add_c(in, CAST_TO_SHORTPTR(out), stride, 12);
}
#endif  // CONFIG_VP9_HIGHBITDEPTH

class TransTestBase {
 public:
  virtual void TearDown() { libvpx_test::ClearSystemState(); }

 protected:
  virtual void RunFwdTxfm(const Buffer<int16_t> &in,
                          Buffer<tran_low_t> *out) = 0;

  virtual void RunInvTxfm(const Buffer<tran_low_t> &in, uint8_t *out) = 0;

  void RunAccuracyCheck(int limit) {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    Buffer<int16_t> test_input_block =
        Buffer<int16_t>(size_, size_, 8, size_ == 4 ? 0 : 16);
    ASSERT_TRUE(test_input_block.Init());
    Buffer<tran_low_t> test_temp_block =
        Buffer<tran_low_t>(size_, size_, 0, 16);
    ASSERT_TRUE(test_temp_block.Init());
    Buffer<uint8_t> dst = Buffer<uint8_t>(size_, size_, 0, 16);
    ASSERT_TRUE(dst.Init());
    Buffer<uint8_t> src = Buffer<uint8_t>(size_, size_, 0, 16);
    ASSERT_TRUE(src.Init());
#if CONFIG_VP9_HIGHBITDEPTH
    Buffer<uint16_t> dst16 = Buffer<uint16_t>(size_, size_, 0, 16);
    ASSERT_TRUE(dst16.Init());
    Buffer<uint16_t> src16 = Buffer<uint16_t>(size_, size_, 0, 16);
    ASSERT_TRUE(src16.Init());
#endif  // CONFIG_VP9_HIGHBITDEPTH
    uint32_t max_error = 0;
    int64_t total_error = 0;
    const int count_test_block = 10000;
    for (int i = 0; i < count_test_block; ++i) {
      if (bit_depth_ == 8) {
        src.Set(&rnd, &ACMRandom::Rand8);
        dst.Set(&rnd, &ACMRandom::Rand8);
        // Initialize a test block with input range [-255, 255].
        for (int h = 0; h < size_; ++h) {
          for (int w = 0; w < size_; ++w) {
            test_input_block.TopLeftPixel()[h * test_input_block.stride() + w] =
                src.TopLeftPixel()[h * src.stride() + w] -
                dst.TopLeftPixel()[h * dst.stride() + w];
          }
        }
#if CONFIG_VP9_HIGHBITDEPTH
      } else {
        src16.Set(&rnd, 0, max_pixel_value_);
        dst16.Set(&rnd, 0, max_pixel_value_);
        for (int h = 0; h < size_; ++h) {
          for (int w = 0; w < size_; ++w) {
            test_input_block.TopLeftPixel()[h * test_input_block.stride() + w] =
                src16.TopLeftPixel()[h * src16.stride() + w] -
                dst16.TopLeftPixel()[h * dst16.stride() + w];
          }
        }
#endif  // CONFIG_VP9_HIGHBITDEPTH
      }

      ASM_REGISTER_STATE_CHECK(RunFwdTxfm(test_input_block, &test_temp_block));
      if (bit_depth_ == VPX_BITS_8) {
        ASM_REGISTER_STATE_CHECK(
            RunInvTxfm(test_temp_block, dst.TopLeftPixel()));
#if CONFIG_VP9_HIGHBITDEPTH
      } else {
        ASM_REGISTER_STATE_CHECK(
            RunInvTxfm(test_temp_block, CAST_TO_BYTEPTR(dst16.TopLeftPixel())));
#endif  // CONFIG_VP9_HIGHBITDEPTH
      }

      for (int h = 0; h < size_; ++h) {
        for (int w = 0; w < size_; ++w) {
          int diff;
#if CONFIG_VP9_HIGHBITDEPTH
          if (bit_depth_ != 8) {
            diff = dst16.TopLeftPixel()[h * dst16.stride() + w] -
                   src16.TopLeftPixel()[h * src16.stride() + w];
          } else {
#endif  // CONFIG_VP9_HIGHBITDEPTH
            diff = dst.TopLeftPixel()[h * dst.stride() + w] -
                   src.TopLeftPixel()[h * src.stride() + w];
#if CONFIG_VP9_HIGHBITDEPTH
          }
#endif  // CONFIG_VP9_HIGHBITDEPTH
          const uint32_t error = diff * diff;
          if (max_error < error) max_error = error;
          total_error += error;
        }
      }
    }

    EXPECT_GE(static_cast<uint32_t>(limit), max_error)
        << "Error: 4x4 FHT/IHT has an individual round trip error > " << limit;

    EXPECT_GE(count_test_block * limit, total_error)
        << "Error: 4x4 FHT/IHT has average round trip error > " << limit
        << " per block";
  }

  void RunCoeffCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 5000;
    Buffer<int16_t> input_block =
        Buffer<int16_t>(size_, size_, 8, size_ == 4 ? 0 : 16);
    ASSERT_TRUE(input_block.Init());
    Buffer<tran_low_t> output_ref_block = Buffer<tran_low_t>(size_, size_, 0);
    ASSERT_TRUE(output_ref_block.Init());
    Buffer<tran_low_t> output_block = Buffer<tran_low_t>(size_, size_, 0, 16);
    ASSERT_TRUE(output_block.Init());

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-max_pixel_value_,
      // max_pixel_value_].
      input_block.Set(&rnd, -max_pixel_value_, max_pixel_value_);

      fwd_txfm_ref(input_block, &output_ref_block, size_, tx_type_);
      ASM_REGISTER_STATE_CHECK(RunFwdTxfm(input_block, &output_block));

      // The minimum quant value is 4.
      EXPECT_TRUE(output_block.CheckValues(output_ref_block));
      if (::testing::Test::HasFailure()) {
        printf("Size: %d Transform type: %d\n", size_, tx_type_);
        output_block.PrintDifference(output_ref_block);
        return;
      }
    }
  }

  void RunMemCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 5000;
    Buffer<int16_t> input_extreme_block =
        Buffer<int16_t>(size_, size_, 8, size_ == 4 ? 0 : 16);
    ASSERT_TRUE(input_extreme_block.Init());
    Buffer<tran_low_t> output_ref_block = Buffer<tran_low_t>(size_, size_, 0);
    ASSERT_TRUE(output_ref_block.Init());
    Buffer<tran_low_t> output_block = Buffer<tran_low_t>(size_, size_, 0, 16);
    ASSERT_TRUE(output_block.Init());

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with -max_pixel_value_ or max_pixel_value_.
      if (i == 0) {
        input_extreme_block.Set(max_pixel_value_);
      } else if (i == 1) {
        input_extreme_block.Set(-max_pixel_value_);
      } else {
        for (int h = 0; h < size_; ++h) {
          for (int w = 0; w < size_; ++w) {
            input_extreme_block
                .TopLeftPixel()[h * input_extreme_block.stride() + w] =
                rnd.Rand8() % 2 ? max_pixel_value_ : -max_pixel_value_;
          }
        }
      }

      fwd_txfm_ref(input_extreme_block, &output_ref_block, size_, tx_type_);
      ASM_REGISTER_STATE_CHECK(RunFwdTxfm(input_extreme_block, &output_block));

      // The minimum quant value is 4.
      EXPECT_TRUE(output_block.CheckValues(output_ref_block));
      for (int h = 0; h < size_; ++h) {
        for (int w = 0; w < size_; ++w) {
          EXPECT_GE(
              4 * DCT_MAX_VALUE << (bit_depth_ - 8),
              abs(output_block.TopLeftPixel()[h * output_block.stride() + w]))
              << "Error: 4x4 FDCT has coefficient larger than "
                 "4*DCT_MAX_VALUE"
              << " at " << w << "," << h;
          if (::testing::Test::HasFailure()) {
            printf("Size: %d Transform type: %d\n", size_, tx_type_);
            output_block.DumpBuffer();
            return;
          }
        }
      }
    }
  }

  void RunInvAccuracyCheck(int limit) {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 1000;
    Buffer<int16_t> in = Buffer<int16_t>(size_, size_, 4);
    ASSERT_TRUE(in.Init());
    Buffer<tran_low_t> coeff = Buffer<tran_low_t>(size_, size_, 0, 16);
    ASSERT_TRUE(coeff.Init());
    Buffer<uint8_t> dst = Buffer<uint8_t>(size_, size_, 0, 16);
    ASSERT_TRUE(dst.Init());
    Buffer<uint8_t> src = Buffer<uint8_t>(size_, size_, 0);
    ASSERT_TRUE(src.Init());
    Buffer<uint16_t> dst16 = Buffer<uint16_t>(size_, size_, 0, 16);
    ASSERT_TRUE(dst16.Init());
    Buffer<uint16_t> src16 = Buffer<uint16_t>(size_, size_, 0);
    ASSERT_TRUE(src16.Init());

    for (int i = 0; i < count_test_block; ++i) {
      // Initialize a test block with input range [-max_pixel_value_,
      // max_pixel_value_].
      if (bit_depth_ == VPX_BITS_8) {
        src.Set(&rnd, &ACMRandom::Rand8);
        dst.Set(&rnd, &ACMRandom::Rand8);
        for (int h = 0; h < size_; ++h) {
          for (int w = 0; w < size_; ++w) {
            in.TopLeftPixel()[h * in.stride() + w] =
                src.TopLeftPixel()[h * src.stride() + w] -
                dst.TopLeftPixel()[h * dst.stride() + w];
          }
        }
#if CONFIG_VP9_HIGHBITDEPTH
      } else {
        src16.Set(&rnd, 0, max_pixel_value_);
        dst16.Set(&rnd, 0, max_pixel_value_);
        for (int h = 0; h < size_; ++h) {
          for (int w = 0; w < size_; ++w) {
            in.TopLeftPixel()[h * in.stride() + w] =
                src16.TopLeftPixel()[h * src16.stride() + w] -
                dst16.TopLeftPixel()[h * dst16.stride() + w];
          }
        }
#endif  // CONFIG_VP9_HIGHBITDEPTH
      }

      fwd_txfm_ref(in, &coeff, size_, tx_type_);

      if (bit_depth_ == VPX_BITS_8) {
        ASM_REGISTER_STATE_CHECK(RunInvTxfm(coeff, dst.TopLeftPixel()));
#if CONFIG_VP9_HIGHBITDEPTH
      } else {
        ASM_REGISTER_STATE_CHECK(
            RunInvTxfm(coeff, CAST_TO_BYTEPTR(dst16.TopLeftPixel())));
#endif  // CONFIG_VP9_HIGHBITDEPTH
      }

      for (int h = 0; h < size_; ++h) {
        for (int w = 0; w < size_; ++w) {
          int diff;
#if CONFIG_VP9_HIGHBITDEPTH
          if (bit_depth_ != 8) {
            diff = dst16.TopLeftPixel()[h * dst16.stride() + w] -
                   src16.TopLeftPixel()[h * src16.stride() + w];
          } else {
#endif  // CONFIG_VP9_HIGHBITDEPTH
            diff = dst.TopLeftPixel()[h * dst.stride() + w] -
                   src.TopLeftPixel()[h * src.stride() + w];
#if CONFIG_VP9_HIGHBITDEPTH
          }
#endif  // CONFIG_VP9_HIGHBITDEPTH
          const uint32_t error = diff * diff;
          EXPECT_GE(static_cast<uint32_t>(limit), error)
              << "Error: " << size_ << "x" << size_ << " IDCT has error "
              << error << " at " << w << "," << h;
        }
      }
    }
  }

  FhtFuncRef fwd_txfm_ref;
  vpx_bit_depth_t bit_depth_;
  int tx_type_;
  int max_pixel_value_;
  int size_;
};

class TransDCT : public TransTestBase,
                 public ::testing::TestWithParam<DctParam> {
 public:
  TransDCT() {
    fwd_txfm_ref = fdct_ref;
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    size_ = GET_PARAM(2);
    tx_type_ = GET_PARAM(3);
    bit_depth_ = GET_PARAM(4);
    max_pixel_value_ = (1 << bit_depth_) - 1;
  }

 protected:
  void RunFwdTxfm(const Buffer<int16_t> &in, Buffer<tran_low_t> *out) {
    fwd_txfm_(in.TopLeftPixel(), out->TopLeftPixel(), in.stride());
  }

  void RunInvTxfm(const Buffer<tran_low_t> &in, uint8_t *out) {
    inv_txfm_(in.TopLeftPixel(), out, in.stride());
  }

  FdctFunc fwd_txfm_;
  IdctFunc inv_txfm_;
};

TEST_P(TransDCT, AccuracyCheck) { RunAccuracyCheck(1); }

TEST_P(TransDCT, CoeffCheck) { RunCoeffCheck(); }

TEST_P(TransDCT, MemCheck) { RunMemCheck(); }

TEST_P(TransDCT, InvAccuracyCheck) { RunInvAccuracyCheck(1); }

#if CONFIG_VP9_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    C, TransDCT,
    ::testing::Values(
        make_tuple(&vpx_highbd_fdct32x32_c, &idct32x32_10, 32, 0, VPX_BITS_10),
        make_tuple(&vpx_highbd_fdct32x32_c, &idct32x32_12, 32, 0, VPX_BITS_10),
        make_tuple(&vpx_fdct32x32_c, &vpx_idct32x32_1024_add_c, 32, 0,
                   VPX_BITS_8),
        make_tuple(&vpx_highbd_fdct16x16_c, &idct16x16_10, 16, 0, VPX_BITS_10),
        make_tuple(&vpx_highbd_fdct16x16_c, &idct16x16_12, 16, 0, VPX_BITS_10),
        make_tuple(&vpx_fdct16x16_c, &vpx_idct16x16_256_add_c, 16, 0,
                   VPX_BITS_8),
        make_tuple(&vpx_highbd_fdct8x8_c, &idct8x8_10, 8, 0, VPX_BITS_10),
        make_tuple(&vpx_highbd_fdct8x8_c, &idct8x8_12, 8, 0, VPX_BITS_10),
        make_tuple(&vpx_fdct8x8_c, &vpx_idct8x8_64_add_c, 8, 0, VPX_BITS_8),
        make_tuple(&vpx_highbd_fdct4x4_c, &idct4x4_10, 4, 0, VPX_BITS_10),
        make_tuple(&vpx_highbd_fdct4x4_c, &idct4x4_12, 4, 0, VPX_BITS_12),
        make_tuple(&vpx_fdct4x4_c, &vpx_idct4x4_16_add_c, 4, 0, VPX_BITS_8)));
#else
INSTANTIATE_TEST_CASE_P(
    C, TransDCT,
    ::testing::Values(
        make_tuple(&vpx_fdct32x32_c, &vpx_idct32x32_1024_add_c, 32, 0,
                   VPX_BITS_8),
        make_tuple(&vpx_fdct16x16_c, &vpx_idct16x16_256_add_c, 16, 0,
                   VPX_BITS_8),
        make_tuple(&vpx_fdct8x8_c, &vpx_idct8x8_64_add_c, 8, 0, VPX_BITS_8),
        make_tuple(&vpx_fdct4x4_c, &vpx_idct4x4_16_add_c, 4, 0, VPX_BITS_8)));
#endif  // CONFIG_VP9_HIGHBITDEPTH

#if HAVE_SSE2
#if !CONFIG_EMULATE_HARDWARE
#if CONFIG_VP9_HIGHBITDEPTH
/* TODO:(johannkoenig) Determine why these fail AccuracyCheck
   make_tuple(&vpx_highbd_fdct32x32_sse2, &idct32x32_12, 32, 0, VPX_BITS_12),
   make_tuple(&vpx_highbd_fdct16x16_sse2, &idct16x16_12, 16, 0, VPX_BITS_12),
*/
INSTANTIATE_TEST_CASE_P(
    SSE2, TransDCT,
    ::testing::Values(
        make_tuple(&vpx_highbd_fdct32x32_sse2, &idct32x32_10, 32, 0,
                   VPX_BITS_10),
        make_tuple(&vpx_fdct32x32_sse2, &vpx_idct32x32_1024_add_sse2, 32, 0,
                   VPX_BITS_8),
        make_tuple(&vpx_highbd_fdct16x16_sse2, &idct16x16_10, 16, 0,
                   VPX_BITS_10),
        make_tuple(&vpx_fdct16x16_sse2, &vpx_idct16x16_256_add_sse2, 16, 0,
                   VPX_BITS_8),
        make_tuple(&vpx_highbd_fdct8x8_sse2, &idct8x8_10, 8, 0, VPX_BITS_10),
        make_tuple(&vpx_highbd_fdct8x8_sse2, &idct8x8_12, 8, 0, VPX_BITS_12),
        make_tuple(&vpx_fdct8x8_sse2, &vpx_idct8x8_64_add_sse2, 8, 0,
                   VPX_BITS_8),
        make_tuple(&vpx_highbd_fdct4x4_sse2, &idct4x4_10, 4, 0, VPX_BITS_10),
        make_tuple(&vpx_highbd_fdct4x4_sse2, &idct4x4_12, 4, 0, VPX_BITS_12),
        make_tuple(&vpx_fdct4x4_sse2, &vpx_idct4x4_16_add_sse2, 4, 0,
                   VPX_BITS_8)));
#else
INSTANTIATE_TEST_CASE_P(
    SSE2, TransDCT,
    ::testing::Values(make_tuple(&vpx_fdct32x32_sse2,
                                 &vpx_idct32x32_1024_add_sse2, 32, 0,
                                 VPX_BITS_8),
                      make_tuple(&vpx_fdct16x16_sse2,
                                 &vpx_idct16x16_256_add_sse2, 16, 0,
                                 VPX_BITS_8),
                      make_tuple(&vpx_fdct8x8_sse2, &vpx_idct8x8_64_add_sse2, 8,
                                 0, VPX_BITS_8),
                      make_tuple(&vpx_fdct4x4_sse2, &vpx_idct4x4_16_add_sse2, 4,
                                 0, VPX_BITS_8)));
#endif  // CONFIG_VP9_HIGHBITDEPTH
#endif  // !CONFIG_EMULATE_HARDWARE
#endif  // HAVE_SSE2

#if !CONFIG_VP9_HIGHBITDEPTH
#if HAVE_SSSE3 && !CONFIG_EMULATE_HARDWARE
#if !ARCH_X86_64
// TODO(johannkoenig): high bit depth fdct8x8.
INSTANTIATE_TEST_CASE_P(
    SSSE3, TransDCT,
    ::testing::Values(make_tuple(&vpx_fdct32x32_c, &vpx_idct32x32_1024_add_sse2,
                                 32, 0, VPX_BITS_8),
                      make_tuple(&vpx_fdct8x8_c, &vpx_idct8x8_64_add_sse2, 8, 0,
                                 VPX_BITS_8)));
#else
// vpx_fdct8x8_ssse3 is only available in 64 bit builds.
INSTANTIATE_TEST_CASE_P(
    SSSE3, TransDCT,
    ::testing::Values(make_tuple(&vpx_fdct32x32_c, &vpx_idct32x32_1024_add_sse2,
                                 32, 0, VPX_BITS_8),
                      make_tuple(&vpx_fdct8x8_ssse3, &vpx_idct8x8_64_add_sse2,
                                 8, 0, VPX_BITS_8)));
#endif  // !ARCH_X86_64
#endif  // HAVE_SSSE3 && !CONFIG_EMULATE_HARDWARE
#endif  // !CONFIG_VP9_HIGHBITDEPTH

#if !CONFIG_VP9_HIGHBITDEPTH && HAVE_AVX2 && !CONFIG_EMULATE_HARDWARE
// TODO(johannkoenig): high bit depth fdct32x32.
INSTANTIATE_TEST_CASE_P(
    AVX2, TransDCT, ::testing::Values(make_tuple(&vpx_fdct32x32_avx2,
                                                 &vpx_idct32x32_1024_add_sse2,
                                                 32, 0, VPX_BITS_8)));

#endif  // !CONFIG_VP9_HIGHBITDEPTH && HAVE_AVX2 && !CONFIG_EMULATE_HARDWARE

#if HAVE_NEON
#if !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_CASE_P(
    NEON, TransDCT,
    ::testing::Values(make_tuple(&vpx_fdct32x32_neon,
                                 &vpx_idct32x32_1024_add_neon, 32, 0,
                                 VPX_BITS_8),
                      make_tuple(&vpx_fdct16x16_neon,
                                 &vpx_idct16x16_256_add_neon, 16, 0,
                                 VPX_BITS_8),
                      make_tuple(&vpx_fdct8x8_neon, &vpx_idct8x8_64_add_neon, 8,
                                 0, VPX_BITS_8),
                      make_tuple(&vpx_fdct4x4_neon, &vpx_idct4x4_16_add_neon, 4,
                                 0, VPX_BITS_8)));
#endif  // !CONFIG_EMULATE_HARDWARE
#endif  // HAVE_NEON

#if HAVE_MSA
#if !CONFIG_VP9_HIGHBITDEPTH
#if !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_CASE_P(
    MSA, TransDCT,
    ::testing::Values(
        make_tuple(&vpx_fdct32x32_msa, &vpx_idct32x32_1024_add_msa, 32, 0,
                   VPX_BITS_8),
        make_tuple(&vpx_fdct16x16_msa, &vpx_idct16x16_256_add_msa, 16, 0,
                   VPX_BITS_8),
        make_tuple(&vpx_fdct8x8_msa, &vpx_idct8x8_64_add_msa, 8, 0, VPX_BITS_8),
        make_tuple(&vpx_fdct4x4_msa, &vpx_idct4x4_16_add_msa, 4, 0,
                   VPX_BITS_8)));
#endif  // !CONFIG_EMULATE_HARDWARE
#endif  // !CONFIG_VP9_HIGHBITDEPTH
#endif  // HAVE_MSA

#if HAVE_VSX && !CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE
INSTANTIATE_TEST_CASE_P(VSX, TransDCT,
                        ::testing::Values(make_tuple(&vpx_fdct4x4_c,
                                                     &vpx_idct4x4_16_add_vsx, 4,
                                                     0, VPX_BITS_8)));
#endif  // HAVE_VSX && !CONFIG_VP9_HIGHBITDEPTH && !CONFIG_EMULATE_HARDWARE

class TransHT : public TransTestBase, public ::testing::TestWithParam<HtParam> {
 public:
  TransHT() {
    fwd_txfm_ref = fht_ref;
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    size_ = GET_PARAM(2);
    tx_type_ = GET_PARAM(3);
    bit_depth_ = GET_PARAM(4);
    max_pixel_value_ = (1 << bit_depth_) - 1;
  }

 protected:
  void RunFwdTxfm(const Buffer<int16_t> &in, Buffer<tran_low_t> *out) {
    fwd_txfm_(in.TopLeftPixel(), out->TopLeftPixel(), in.stride(), tx_type_);
  }

  void RunInvTxfm(const Buffer<tran_low_t> &in, uint8_t *out) {
    inv_txfm_(in.TopLeftPixel(), out, in.stride(), tx_type_);
  }

  FhtFunc fwd_txfm_;
  IhtFunc inv_txfm_;
};

TEST_P(TransHT, AccuracyCheck) { RunAccuracyCheck(1); }

TEST_P(TransHT, CoeffCheck) { RunCoeffCheck(); }

TEST_P(TransHT, MemCheck) { RunMemCheck(); }

TEST_P(TransHT, InvAccuracyCheck) { RunInvAccuracyCheck(1); }

/* TODO:(johannkoenig) Determine why these fail AccuracyCheck
   make_tuple(&vp9_highbd_fht16x16_c, &iht16x16_12, 16, 0, VPX_BITS_12),
   make_tuple(&vp9_highbd_fht16x16_c, &iht16x16_12, 16, 1, VPX_BITS_12),
   make_tuple(&vp9_highbd_fht16x16_c, &iht16x16_12, 16, 2, VPX_BITS_12),
   make_tuple(&vp9_highbd_fht16x16_c, &iht16x16_12, 16, 3, VPX_BITS_12),
  */
#if CONFIG_VP9_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    C, TransHT,
    ::testing::Values(
        make_tuple(&vp9_highbd_fht16x16_c, &iht16x16_10, 16, 0, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht16x16_c, &iht16x16_10, 16, 1, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht16x16_c, &iht16x16_10, 16, 2, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht16x16_c, &iht16x16_10, 16, 3, VPX_BITS_10),
        make_tuple(&vp9_fht16x16_c, &vp9_iht16x16_256_add_c, 16, 0, VPX_BITS_8),
        make_tuple(&vp9_fht16x16_c, &vp9_iht16x16_256_add_c, 16, 1, VPX_BITS_8),
        make_tuple(&vp9_fht16x16_c, &vp9_iht16x16_256_add_c, 16, 2, VPX_BITS_8),
        make_tuple(&vp9_fht16x16_c, &vp9_iht16x16_256_add_c, 16, 3, VPX_BITS_8),
        make_tuple(&vp9_highbd_fht8x8_c, &iht8x8_10, 8, 0, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht8x8_c, &iht8x8_10, 8, 1, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht8x8_c, &iht8x8_10, 8, 2, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht8x8_c, &iht8x8_10, 8, 3, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht8x8_c, &iht8x8_12, 8, 0, VPX_BITS_12),
        make_tuple(&vp9_highbd_fht8x8_c, &iht8x8_12, 8, 1, VPX_BITS_12),
        make_tuple(&vp9_highbd_fht8x8_c, &iht8x8_12, 8, 2, VPX_BITS_12),
        make_tuple(&vp9_highbd_fht8x8_c, &iht8x8_12, 8, 3, VPX_BITS_12),
        make_tuple(&vp9_fht8x8_c, &vp9_iht8x8_64_add_c, 8, 0, VPX_BITS_8),
        make_tuple(&vp9_fht8x8_c, &vp9_iht8x8_64_add_c, 8, 1, VPX_BITS_8),
        make_tuple(&vp9_fht8x8_c, &vp9_iht8x8_64_add_c, 8, 2, VPX_BITS_8),
        make_tuple(&vp9_fht8x8_c, &vp9_iht8x8_64_add_c, 8, 3, VPX_BITS_8),
        make_tuple(&vp9_highbd_fht4x4_c, &iht4x4_10, 4, 0, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht4x4_c, &iht4x4_10, 4, 1, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht4x4_c, &iht4x4_10, 4, 2, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht4x4_c, &iht4x4_10, 4, 3, VPX_BITS_10),
        make_tuple(&vp9_highbd_fht4x4_c, &iht4x4_12, 4, 0, VPX_BITS_12),
        make_tuple(&vp9_highbd_fht4x4_c, &iht4x4_12, 4, 1, VPX_BITS_12),
        make_tuple(&vp9_highbd_fht4x4_c, &iht4x4_12, 4, 2, VPX_BITS_12),
        make_tuple(&vp9_highbd_fht4x4_c, &iht4x4_12, 4, 3, VPX_BITS_12),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 4, 0, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 4, 1, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 4, 2, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 4, 3, VPX_BITS_8)));
#else
INSTANTIATE_TEST_CASE_P(
    C, TransHT,
    ::testing::Values(
        make_tuple(&vp9_fht16x16_c, &vp9_iht16x16_256_add_c, 16, 0, VPX_BITS_8),
        make_tuple(&vp9_fht16x16_c, &vp9_iht16x16_256_add_c, 16, 1, VPX_BITS_8),
        make_tuple(&vp9_fht16x16_c, &vp9_iht16x16_256_add_c, 16, 2, VPX_BITS_8),
        make_tuple(&vp9_fht16x16_c, &vp9_iht16x16_256_add_c, 16, 3, VPX_BITS_8),

        make_tuple(&vp9_fht8x8_c, &vp9_iht8x8_64_add_c, 8, 0, VPX_BITS_8),
        make_tuple(&vp9_fht8x8_c, &vp9_iht8x8_64_add_c, 8, 1, VPX_BITS_8),
        make_tuple(&vp9_fht8x8_c, &vp9_iht8x8_64_add_c, 8, 2, VPX_BITS_8),
        make_tuple(&vp9_fht8x8_c, &vp9_iht8x8_64_add_c, 8, 3, VPX_BITS_8),

        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 4, 0, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 4, 1, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 4, 2, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_c, &vp9_iht4x4_16_add_c, 4, 3, VPX_BITS_8)));
#endif  // CONFIG_VP9_HIGHBITDEPTH

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, TransHT,
    ::testing::Values(
        make_tuple(&vp9_fht16x16_sse2, &vp9_iht16x16_256_add_sse2, 16, 0,
                   VPX_BITS_8),
        make_tuple(&vp9_fht16x16_sse2, &vp9_iht16x16_256_add_sse2, 16, 1,
                   VPX_BITS_8),
        make_tuple(&vp9_fht16x16_sse2, &vp9_iht16x16_256_add_sse2, 16, 2,
                   VPX_BITS_8),
        make_tuple(&vp9_fht16x16_sse2, &vp9_iht16x16_256_add_sse2, 16, 3,
                   VPX_BITS_8),

        make_tuple(&vp9_fht8x8_sse2, &vp9_iht8x8_64_add_sse2, 8, 0, VPX_BITS_8),
        make_tuple(&vp9_fht8x8_sse2, &vp9_iht8x8_64_add_sse2, 8, 1, VPX_BITS_8),
        make_tuple(&vp9_fht8x8_sse2, &vp9_iht8x8_64_add_sse2, 8, 2, VPX_BITS_8),
        make_tuple(&vp9_fht8x8_sse2, &vp9_iht8x8_64_add_sse2, 8, 3, VPX_BITS_8),

        make_tuple(&vp9_fht4x4_sse2, &vp9_iht4x4_16_add_sse2, 4, 0, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_sse2, &vp9_iht4x4_16_add_sse2, 4, 1, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_sse2, &vp9_iht4x4_16_add_sse2, 4, 2, VPX_BITS_8),
        make_tuple(&vp9_fht4x4_sse2, &vp9_iht4x4_16_add_sse2, 4, 3,
                   VPX_BITS_8)));
#endif  // HAVE_SSE2

class TransWHT : public TransTestBase,
                 public ::testing::TestWithParam<DctParam> {
 public:
  TransWHT() {
    fwd_txfm_ref = fwht_ref;
    fwd_txfm_ = GET_PARAM(0);
    inv_txfm_ = GET_PARAM(1);
    size_ = GET_PARAM(2);
    tx_type_ = GET_PARAM(3);
    bit_depth_ = GET_PARAM(4);
    max_pixel_value_ = (1 << bit_depth_) - 1;
  }

 protected:
  void RunFwdTxfm(const Buffer<int16_t> &in, Buffer<tran_low_t> *out) {
    fwd_txfm_(in.TopLeftPixel(), out->TopLeftPixel(), in.stride());
  }

  void RunInvTxfm(const Buffer<tran_low_t> &in, uint8_t *out) {
    inv_txfm_(in.TopLeftPixel(), out, in.stride());
  }

  FdctFunc fwd_txfm_;
  IdctFunc inv_txfm_;
};

TEST_P(TransWHT, AccuracyCheck) { RunAccuracyCheck(0); }

TEST_P(TransWHT, CoeffCheck) { RunCoeffCheck(); }

TEST_P(TransWHT, MemCheck) { RunMemCheck(); }

TEST_P(TransWHT, InvAccuracyCheck) { RunInvAccuracyCheck(0); }

#if CONFIG_VP9_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(
    C, TransWHT,
    ::testing::Values(
        make_tuple(&vp9_highbd_fwht4x4_c, &iwht4x4_10, 4, 0, VPX_BITS_10),
        make_tuple(&vp9_highbd_fwht4x4_c, &iwht4x4_12, 4, 0, VPX_BITS_12),
        make_tuple(&vp9_fwht4x4_c, &vpx_iwht4x4_16_add_c, 4, 0, VPX_BITS_8)));
#else
INSTANTIATE_TEST_CASE_P(C, TransWHT,
                        ::testing::Values(make_tuple(&vp9_fwht4x4_c,
                                                     &vpx_iwht4x4_16_add_c, 4,
                                                     0, VPX_BITS_8)));
#endif  // CONFIG_VP9_HIGHBITDEPTH

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(SSE2, TransWHT,
                        ::testing::Values(make_tuple(&vp9_fwht4x4_sse2,
                                                     &vpx_iwht4x4_16_add_sse2,
                                                     4, 0, VPX_BITS_8)));
#endif  // HAVE_SSE2
}  // namespace
