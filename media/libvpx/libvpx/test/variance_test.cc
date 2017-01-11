/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdlib>
#include <new>

#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./vpx_config.h"
#include "vpx/vpx_codec.h"
#include "vpx/vpx_integer.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx_ports/mem.h"
#if CONFIG_VP8_ENCODER
# include "./vp8_rtcd.h"
#endif  // CONFIG_VP8_ENCODER
#if CONFIG_VP9_ENCODER
# include "./vp9_rtcd.h"
# include "vp9/encoder/vp9_variance.h"
#endif  // CONFIG_VP9_ENCODER
#include "./vpx_dsp_rtcd.h"

namespace {

typedef unsigned int (*VarianceMxNFunc)(const uint8_t *a, int a_stride,
                                        const uint8_t *b, int b_stride,
                                        unsigned int *sse);
typedef unsigned int (*SubpixVarMxNFunc)(const uint8_t *a, int a_stride,
                                         int xoffset, int yoffset,
                                         const uint8_t *b, int b_stride,
                                         unsigned int *sse);
typedef unsigned int (*Get4x4SseFunc)(const uint8_t *a, int a_stride,
                                      const uint8_t *b, int b_stride);

using ::std::tr1::get;
using ::std::tr1::make_tuple;
using ::std::tr1::tuple;
using libvpx_test::ACMRandom;

// Truncate high bit depth results by downshifting (with rounding) by:
// 2 * (bit_depth - 8) for sse
// (bit_depth - 8) for se
static void RoundHighBitDepth(int bit_depth, int64_t *se, uint64_t *sse) {
  switch (bit_depth) {
    case VPX_BITS_12:
      *sse = (*sse + 128) >> 8;
      *se = (*se + 8) >> 4;
      break;
    case VPX_BITS_10:
      *sse = (*sse + 8) >> 4;
      *se = (*se + 2) >> 2;
      break;
    case VPX_BITS_8:
    default:
      break;
  }
}

static unsigned int mb_ss_ref(const int16_t *src) {
  unsigned int res = 0;
  for (int i = 0; i < 256; ++i) {
    res += src[i] * src[i];
  }
  return res;
}

static uint32_t variance_ref(const uint8_t *src, const uint8_t *ref,
                             int l2w, int l2h, int src_stride_coeff,
                             int ref_stride_coeff, uint32_t *sse_ptr,
                             bool use_high_bit_depth_,
                             vpx_bit_depth_t bit_depth) {
  int64_t se = 0;
  uint64_t sse = 0;
  const int w = 1 << l2w;
  const int h = 1 << l2h;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int diff;
      if (!use_high_bit_depth_) {
        diff = ref[w * y * ref_stride_coeff + x] -
               src[w * y * src_stride_coeff + x];
        se += diff;
        sse += diff * diff;
#if CONFIG_VP9_HIGHBITDEPTH
      } else {
        diff = CONVERT_TO_SHORTPTR(ref)[w * y * ref_stride_coeff + x] -
               CONVERT_TO_SHORTPTR(src)[w * y * src_stride_coeff + x];
        se += diff;
        sse += diff * diff;
#endif  // CONFIG_VP9_HIGHBITDEPTH
      }
    }
  }
  RoundHighBitDepth(bit_depth, &se, &sse);
  *sse_ptr = static_cast<uint32_t>(sse);
  return static_cast<uint32_t>(sse -
                               ((static_cast<int64_t>(se) * se) >>
                                (l2w + l2h)));
}

/* The subpel reference functions differ from the codec version in one aspect:
 * they calculate the bilinear factors directly instead of using a lookup table
 * and therefore upshift xoff and yoff by 1. Only every other calculated value
 * is used so the codec version shrinks the table to save space and maintain
 * compatibility with vp8.
 */
static uint32_t subpel_variance_ref(const uint8_t *ref, const uint8_t *src,
                                    int l2w, int l2h, int xoff, int yoff,
                                    uint32_t *sse_ptr,
                                    bool use_high_bit_depth_,
                                    vpx_bit_depth_t bit_depth) {
  int64_t se = 0;
  uint64_t sse = 0;
  const int w = 1 << l2w;
  const int h = 1 << l2h;

  xoff <<= 1;
  yoff <<= 1;

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      // Bilinear interpolation at a 16th pel step.
      if (!use_high_bit_depth_) {
        const int a1 = ref[(w + 1) * (y + 0) + x + 0];
        const int a2 = ref[(w + 1) * (y + 0) + x + 1];
        const int b1 = ref[(w + 1) * (y + 1) + x + 0];
        const int b2 = ref[(w + 1) * (y + 1) + x + 1];
        const int a = a1 + (((a2 - a1) * xoff + 8) >> 4);
        const int b = b1 + (((b2 - b1) * xoff + 8) >> 4);
        const int r = a + (((b - a) * yoff + 8) >> 4);
        const int diff = r - src[w * y + x];
        se += diff;
        sse += diff * diff;
#if CONFIG_VP9_HIGHBITDEPTH
      } else {
        uint16_t *ref16 = CONVERT_TO_SHORTPTR(ref);
        uint16_t *src16 = CONVERT_TO_SHORTPTR(src);
        const int a1 = ref16[(w + 1) * (y + 0) + x + 0];
        const int a2 = ref16[(w + 1) * (y + 0) + x + 1];
        const int b1 = ref16[(w + 1) * (y + 1) + x + 0];
        const int b2 = ref16[(w + 1) * (y + 1) + x + 1];
        const int a = a1 + (((a2 - a1) * xoff + 8) >> 4);
        const int b = b1 + (((b2 - b1) * xoff + 8) >> 4);
        const int r = a + (((b - a) * yoff + 8) >> 4);
        const int diff = r - src16[w * y + x];
        se += diff;
        sse += diff * diff;
#endif  // CONFIG_VP9_HIGHBITDEPTH
      }
    }
  }
  RoundHighBitDepth(bit_depth, &se, &sse);
  *sse_ptr = static_cast<uint32_t>(sse);
  return static_cast<uint32_t>(sse -
                               ((static_cast<int64_t>(se) * se) >>
                                (l2w + l2h)));
}

typedef unsigned int (*SumOfSquaresFunction)(const int16_t *src);

class SumOfSquaresTest : public ::testing::TestWithParam<SumOfSquaresFunction> {
 public:
  SumOfSquaresTest() : func_(GetParam()) {}

  virtual ~SumOfSquaresTest() {
    libvpx_test::ClearSystemState();
  }

 protected:
  void ConstTest();
  void RefTest();

  SumOfSquaresFunction func_;
  ACMRandom rnd_;
};

void SumOfSquaresTest::ConstTest() {
  int16_t mem[256];
  unsigned int res;
  for (int v = 0; v < 256; ++v) {
    for (int i = 0; i < 256; ++i) {
      mem[i] = v;
    }
    ASM_REGISTER_STATE_CHECK(res = func_(mem));
    EXPECT_EQ(256u * (v * v), res);
  }
}

void SumOfSquaresTest::RefTest() {
  int16_t mem[256];
  for (int i = 0; i < 100; ++i) {
    for (int j = 0; j < 256; ++j) {
      mem[j] = rnd_.Rand8() - rnd_.Rand8();
    }

    const unsigned int expected = mb_ss_ref(mem);
    unsigned int res;
    ASM_REGISTER_STATE_CHECK(res = func_(mem));
    EXPECT_EQ(expected, res);
  }
}

template<typename VarianceFunctionType>
class VarianceTest
    : public ::testing::TestWithParam<tuple<int, int,
                                            VarianceFunctionType, int> > {
 public:
  virtual void SetUp() {
    const tuple<int, int, VarianceFunctionType, int>& params = this->GetParam();
    log2width_  = get<0>(params);
    width_ = 1 << log2width_;
    log2height_ = get<1>(params);
    height_ = 1 << log2height_;
    variance_ = get<2>(params);
    if (get<3>(params)) {
      bit_depth_ = static_cast<vpx_bit_depth_t>(get<3>(params));
      use_high_bit_depth_ = true;
    } else {
      bit_depth_ = VPX_BITS_8;
      use_high_bit_depth_ = false;
    }
    mask_ = (1 << bit_depth_) - 1;

    rnd_.Reset(ACMRandom::DeterministicSeed());
    block_size_ = width_ * height_;
    if (!use_high_bit_depth_) {
      src_ = reinterpret_cast<uint8_t *>(vpx_memalign(16, block_size_ * 2));
      ref_ = new uint8_t[block_size_ * 2];
#if CONFIG_VP9_HIGHBITDEPTH
    } else {
      src_ = CONVERT_TO_BYTEPTR(reinterpret_cast<uint16_t *>(
          vpx_memalign(16, block_size_ * 2 * sizeof(uint16_t))));
      ref_ = CONVERT_TO_BYTEPTR(new uint16_t[block_size_ * 2]);
#endif  // CONFIG_VP9_HIGHBITDEPTH
    }
    ASSERT_TRUE(src_ != NULL);
    ASSERT_TRUE(ref_ != NULL);
  }

  virtual void TearDown() {
    if (!use_high_bit_depth_) {
      vpx_free(src_);
      delete[] ref_;
#if CONFIG_VP9_HIGHBITDEPTH
    } else {
      vpx_free(CONVERT_TO_SHORTPTR(src_));
      delete[] CONVERT_TO_SHORTPTR(ref_);
#endif  // CONFIG_VP9_HIGHBITDEPTH
    }
    libvpx_test::ClearSystemState();
  }

 protected:
  void ZeroTest();
  void RefTest();
  void RefStrideTest();
  void OneQuarterTest();

  ACMRandom rnd_;
  uint8_t *src_;
  uint8_t *ref_;
  int width_, log2width_;
  int height_, log2height_;
  vpx_bit_depth_t bit_depth_;
  int mask_;
  bool use_high_bit_depth_;
  int block_size_;
  VarianceFunctionType variance_;
};

template<typename VarianceFunctionType>
void VarianceTest<VarianceFunctionType>::ZeroTest() {
  for (int i = 0; i <= 255; ++i) {
    if (!use_high_bit_depth_) {
      memset(src_, i, block_size_);
#if CONFIG_VP9_HIGHBITDEPTH
    } else {
      vpx_memset16(CONVERT_TO_SHORTPTR(src_), i << (bit_depth_ - 8),
                   block_size_);
#endif  // CONFIG_VP9_HIGHBITDEPTH
    }
    for (int j = 0; j <= 255; ++j) {
      if (!use_high_bit_depth_) {
        memset(ref_, j, block_size_);
#if CONFIG_VP9_HIGHBITDEPTH
      } else {
        vpx_memset16(CONVERT_TO_SHORTPTR(ref_), j  << (bit_depth_ - 8),
                     block_size_);
#endif  // CONFIG_VP9_HIGHBITDEPTH
      }
      unsigned int sse;
      unsigned int var;
      ASM_REGISTER_STATE_CHECK(
          var = variance_(src_, width_, ref_, width_, &sse));
      EXPECT_EQ(0u, var) << "src values: " << i << " ref values: " << j;
    }
  }
}

template<typename VarianceFunctionType>
void VarianceTest<VarianceFunctionType>::RefTest() {
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < block_size_; j++) {
    if (!use_high_bit_depth_) {
      src_[j] = rnd_.Rand8();
      ref_[j] = rnd_.Rand8();
#if CONFIG_VP9_HIGHBITDEPTH
    } else {
      CONVERT_TO_SHORTPTR(src_)[j] = rnd_.Rand16() && mask_;
      CONVERT_TO_SHORTPTR(ref_)[j] = rnd_.Rand16() && mask_;
#endif  // CONFIG_VP9_HIGHBITDEPTH
    }
    }
    unsigned int sse1, sse2;
    unsigned int var1;
    const int stride_coeff = 1;
    ASM_REGISTER_STATE_CHECK(
        var1 = variance_(src_, width_, ref_, width_, &sse1));
    const unsigned int var2 = variance_ref(src_, ref_, log2width_,
                                           log2height_, stride_coeff,
                                           stride_coeff, &sse2,
                                           use_high_bit_depth_, bit_depth_);
    EXPECT_EQ(sse1, sse2);
    EXPECT_EQ(var1, var2);
  }
}

template<typename VarianceFunctionType>
void VarianceTest<VarianceFunctionType>::RefStrideTest() {
  for (int i = 0; i < 10; ++i) {
    int ref_stride_coeff = i % 2;
    int src_stride_coeff = (i >> 1) % 2;
    for (int j = 0; j < block_size_; j++) {
      int ref_ind = (j / width_) * ref_stride_coeff * width_ + j % width_;
      int src_ind = (j / width_) * src_stride_coeff * width_ + j % width_;
      if (!use_high_bit_depth_) {
        src_[src_ind] = rnd_.Rand8();
        ref_[ref_ind] = rnd_.Rand8();
#if CONFIG_VP9_HIGHBITDEPTH
      } else {
        CONVERT_TO_SHORTPTR(src_)[src_ind] = rnd_.Rand16() && mask_;
        CONVERT_TO_SHORTPTR(ref_)[ref_ind] = rnd_.Rand16() && mask_;
#endif  // CONFIG_VP9_HIGHBITDEPTH
      }
    }
    unsigned int sse1, sse2;
    unsigned int var1;

    ASM_REGISTER_STATE_CHECK(
        var1 = variance_(src_, width_ * src_stride_coeff,
                         ref_, width_ * ref_stride_coeff, &sse1));
    const unsigned int var2 = variance_ref(src_, ref_, log2width_,
                                           log2height_, src_stride_coeff,
                                           ref_stride_coeff, &sse2,
                                           use_high_bit_depth_, bit_depth_);
    EXPECT_EQ(sse1, sse2);
    EXPECT_EQ(var1, var2);
  }
}

template<typename VarianceFunctionType>
void VarianceTest<VarianceFunctionType>::OneQuarterTest() {
  const int half = block_size_ / 2;
  if (!use_high_bit_depth_) {
    memset(src_, 255, block_size_);
    memset(ref_, 255, half);
    memset(ref_ + half, 0, half);
#if CONFIG_VP9_HIGHBITDEPTH
  } else {
    vpx_memset16(CONVERT_TO_SHORTPTR(src_), 255 << (bit_depth_ - 8),
                 block_size_);
    vpx_memset16(CONVERT_TO_SHORTPTR(ref_), 255 << (bit_depth_ - 8), half);
    vpx_memset16(CONVERT_TO_SHORTPTR(ref_) + half, 0, half);
#endif  // CONFIG_VP9_HIGHBITDEPTH
  }
  unsigned int sse;
  unsigned int var;
  ASM_REGISTER_STATE_CHECK(var = variance_(src_, width_, ref_, width_, &sse));
  const unsigned int expected = block_size_ * 255 * 255 / 4;
  EXPECT_EQ(expected, var);
}

template<typename MseFunctionType>
class MseTest
    : public ::testing::TestWithParam<tuple<int, int, MseFunctionType> > {
 public:
  virtual void SetUp() {
    const tuple<int, int, MseFunctionType>& params = this->GetParam();
    log2width_  = get<0>(params);
    width_ = 1 << log2width_;
    log2height_ = get<1>(params);
    height_ = 1 << log2height_;
    mse_ = get<2>(params);

    rnd(ACMRandom::DeterministicSeed());
    block_size_ = width_ * height_;
    src_ = reinterpret_cast<uint8_t *>(vpx_memalign(16, block_size_));
    ref_ = new uint8_t[block_size_];
    ASSERT_TRUE(src_ != NULL);
    ASSERT_TRUE(ref_ != NULL);
  }

  virtual void TearDown() {
    vpx_free(src_);
    delete[] ref_;
    libvpx_test::ClearSystemState();
  }

 protected:
  void RefTest_mse();
  void RefTest_sse();
  void MaxTest_mse();
  void MaxTest_sse();

  ACMRandom rnd;
  uint8_t* src_;
  uint8_t* ref_;
  int width_, log2width_;
  int height_, log2height_;
  int block_size_;
  MseFunctionType mse_;
};

template<typename MseFunctionType>
void MseTest<MseFunctionType>::RefTest_mse() {
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < block_size_; j++) {
      src_[j] = rnd.Rand8();
      ref_[j] = rnd.Rand8();
    }
    unsigned int sse1, sse2;
    const int stride_coeff = 1;
    ASM_REGISTER_STATE_CHECK(mse_(src_, width_, ref_, width_, &sse1));
    variance_ref(src_, ref_, log2width_, log2height_, stride_coeff,
                 stride_coeff, &sse2, false, VPX_BITS_8);
    EXPECT_EQ(sse1, sse2);
  }
}

template<typename MseFunctionType>
void MseTest<MseFunctionType>::RefTest_sse() {
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < block_size_; j++) {
      src_[j] = rnd.Rand8();
      ref_[j] = rnd.Rand8();
    }
    unsigned int sse2;
    unsigned int var1;
    const int stride_coeff = 1;
    ASM_REGISTER_STATE_CHECK(var1 = mse_(src_, width_, ref_, width_));
    variance_ref(src_, ref_, log2width_, log2height_, stride_coeff,
                 stride_coeff, &sse2, false, VPX_BITS_8);
    EXPECT_EQ(var1, sse2);
  }
}

template<typename MseFunctionType>
void MseTest<MseFunctionType>::MaxTest_mse() {
  memset(src_, 255, block_size_);
  memset(ref_, 0, block_size_);
  unsigned int sse;
  ASM_REGISTER_STATE_CHECK(mse_(src_, width_, ref_, width_, &sse));
  const unsigned int expected = block_size_ * 255 * 255;
  EXPECT_EQ(expected, sse);
}

template<typename MseFunctionType>
void MseTest<MseFunctionType>::MaxTest_sse() {
  memset(src_, 255, block_size_);
  memset(ref_, 0, block_size_);
  unsigned int var;
  ASM_REGISTER_STATE_CHECK(var = mse_(src_, width_, ref_, width_));
  const unsigned int expected = block_size_ * 255 * 255;
  EXPECT_EQ(expected, var);
}

static uint32_t subpel_avg_variance_ref(const uint8_t *ref,
                                        const uint8_t *src,
                                        const uint8_t *second_pred,
                                        int l2w, int l2h,
                                        int xoff, int yoff,
                                        uint32_t *sse_ptr,
                                        bool use_high_bit_depth,
                                        vpx_bit_depth_t bit_depth) {
  int64_t se = 0;
  uint64_t sse = 0;
  const int w = 1 << l2w;
  const int h = 1 << l2h;

  xoff <<= 1;
  yoff <<= 1;

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      // bilinear interpolation at a 16th pel step
      if (!use_high_bit_depth) {
        const int a1 = ref[(w + 1) * (y + 0) + x + 0];
        const int a2 = ref[(w + 1) * (y + 0) + x + 1];
        const int b1 = ref[(w + 1) * (y + 1) + x + 0];
        const int b2 = ref[(w + 1) * (y + 1) + x + 1];
        const int a = a1 + (((a2 - a1) * xoff + 8) >> 4);
        const int b = b1 + (((b2 - b1) * xoff + 8) >> 4);
        const int r = a + (((b - a) * yoff + 8) >> 4);
        const int diff = ((r + second_pred[w * y + x] + 1) >> 1) - src[w * y + x];
        se += diff;
        sse += diff * diff;
#if CONFIG_VP9_HIGHBITDEPTH
      } else {
        uint16_t *ref16 = CONVERT_TO_SHORTPTR(ref);
        uint16_t *src16 = CONVERT_TO_SHORTPTR(src);
        uint16_t *sec16   = CONVERT_TO_SHORTPTR(second_pred);
        const int a1 = ref16[(w + 1) * (y + 0) + x + 0];
        const int a2 = ref16[(w + 1) * (y + 0) + x + 1];
        const int b1 = ref16[(w + 1) * (y + 1) + x + 0];
        const int b2 = ref16[(w + 1) * (y + 1) + x + 1];
        const int a = a1 + (((a2 - a1) * xoff + 8) >> 4);
        const int b = b1 + (((b2 - b1) * xoff + 8) >> 4);
        const int r = a + (((b - a) * yoff + 8) >> 4);
        const int diff = ((r + sec16[w * y + x] + 1) >> 1) - src16[w * y + x];
        se += diff;
        sse += diff * diff;
#endif  // CONFIG_VP9_HIGHBITDEPTH
      }
    }
  }
  RoundHighBitDepth(bit_depth, &se, &sse);
  *sse_ptr = static_cast<uint32_t>(sse);
  return static_cast<uint32_t>(sse -
                               ((static_cast<int64_t>(se) * se) >>
                                (l2w + l2h)));
}

template<typename SubpelVarianceFunctionType>
class SubpelVarianceTest
    : public ::testing::TestWithParam<tuple<int, int,
                                            SubpelVarianceFunctionType, int> > {
 public:
  virtual void SetUp() {
    const tuple<int, int, SubpelVarianceFunctionType, int>& params =
        this->GetParam();
    log2width_  = get<0>(params);
    width_ = 1 << log2width_;
    log2height_ = get<1>(params);
    height_ = 1 << log2height_;
    subpel_variance_ = get<2>(params);
    if (get<3>(params)) {
      bit_depth_ = (vpx_bit_depth_t) get<3>(params);
      use_high_bit_depth_ = true;
    } else {
      bit_depth_ = VPX_BITS_8;
      use_high_bit_depth_ = false;
    }
    mask_ = (1 << bit_depth_)-1;

    rnd_.Reset(ACMRandom::DeterministicSeed());
    block_size_ = width_ * height_;
    if (!use_high_bit_depth_) {
      src_ = reinterpret_cast<uint8_t *>(vpx_memalign(16, block_size_));
      sec_ = reinterpret_cast<uint8_t *>(vpx_memalign(16, block_size_));
      ref_ = new uint8_t[block_size_ + width_ + height_ + 1];
#if CONFIG_VP9_HIGHBITDEPTH
    } else {
      src_ = CONVERT_TO_BYTEPTR(
          reinterpret_cast<uint16_t *>(
              vpx_memalign(16, block_size_*sizeof(uint16_t))));
      sec_ = CONVERT_TO_BYTEPTR(
          reinterpret_cast<uint16_t *>(
              vpx_memalign(16, block_size_*sizeof(uint16_t))));
      ref_ = CONVERT_TO_BYTEPTR(
          new uint16_t[block_size_ + width_ + height_ + 1]);
#endif  // CONFIG_VP9_HIGHBITDEPTH
    }
    ASSERT_TRUE(src_ != NULL);
    ASSERT_TRUE(sec_ != NULL);
    ASSERT_TRUE(ref_ != NULL);
  }

  virtual void TearDown() {
    if (!use_high_bit_depth_) {
      vpx_free(src_);
      delete[] ref_;
      vpx_free(sec_);
#if CONFIG_VP9_HIGHBITDEPTH
    } else {
      vpx_free(CONVERT_TO_SHORTPTR(src_));
      delete[] CONVERT_TO_SHORTPTR(ref_);
      vpx_free(CONVERT_TO_SHORTPTR(sec_));
#endif  // CONFIG_VP9_HIGHBITDEPTH
    }
    libvpx_test::ClearSystemState();
  }

 protected:
  void RefTest();
  void ExtremeRefTest();

  ACMRandom rnd_;
  uint8_t *src_;
  uint8_t *ref_;
  uint8_t *sec_;
  bool use_high_bit_depth_;
  vpx_bit_depth_t bit_depth_;
  int width_, log2width_;
  int height_, log2height_;
  int block_size_,  mask_;
  SubpelVarianceFunctionType subpel_variance_;
};

template<typename SubpelVarianceFunctionType>
void SubpelVarianceTest<SubpelVarianceFunctionType>::RefTest() {
  for (int x = 0; x < 8; ++x) {
    for (int y = 0; y < 8; ++y) {
      if (!use_high_bit_depth_) {
        for (int j = 0; j < block_size_; j++) {
          src_[j] = rnd_.Rand8();
        }
        for (int j = 0; j < block_size_ + width_ + height_ + 1; j++) {
          ref_[j] = rnd_.Rand8();
        }
#if CONFIG_VP9_HIGHBITDEPTH
      } else {
        for (int j = 0; j < block_size_; j++) {
          CONVERT_TO_SHORTPTR(src_)[j] = rnd_.Rand16() & mask_;
        }
        for (int j = 0; j < block_size_ + width_ + height_ + 1; j++) {
          CONVERT_TO_SHORTPTR(ref_)[j] = rnd_.Rand16() & mask_;
        }
#endif  // CONFIG_VP9_HIGHBITDEPTH
      }
      unsigned int sse1, sse2;
      unsigned int var1;
      ASM_REGISTER_STATE_CHECK(var1 = subpel_variance_(ref_, width_ + 1, x, y,
                                                       src_, width_, &sse1));
      const unsigned int var2 = subpel_variance_ref(ref_, src_,
                                                    log2width_, log2height_,
                                                    x, y, &sse2,
                                                    use_high_bit_depth_,
                                                    bit_depth_);
      EXPECT_EQ(sse1, sse2) << "at position " << x << ", " << y;
      EXPECT_EQ(var1, var2) << "at position " << x << ", " << y;
    }
  }
}

template<typename SubpelVarianceFunctionType>
void SubpelVarianceTest<SubpelVarianceFunctionType>::ExtremeRefTest() {
  // Compare against reference.
  // Src: Set the first half of values to 0, the second half to the maximum.
  // Ref: Set the first half of values to the maximum, the second half to 0.
  for (int x = 0; x < 8; ++x) {
    for (int y = 0; y < 8; ++y) {
      const int half = block_size_ / 2;
      if (!use_high_bit_depth_) {
        memset(src_, 0, half);
        memset(src_ + half, 255, half);
        memset(ref_, 255, half);
        memset(ref_ + half, 0, half + width_ + height_ + 1);
#if CONFIG_VP9_HIGHBITDEPTH
      } else {
        vpx_memset16(CONVERT_TO_SHORTPTR(src_), mask_, half);
        vpx_memset16(CONVERT_TO_SHORTPTR(src_) + half, 0, half);
        vpx_memset16(CONVERT_TO_SHORTPTR(ref_), 0, half);
        vpx_memset16(CONVERT_TO_SHORTPTR(ref_) + half, mask_,
                     half + width_ + height_ + 1);
#endif  // CONFIG_VP9_HIGHBITDEPTH
      }
      unsigned int sse1, sse2;
      unsigned int var1;
      ASM_REGISTER_STATE_CHECK(
          var1 = subpel_variance_(ref_, width_ + 1, x, y, src_, width_, &sse1));
      const unsigned int var2 =
          subpel_variance_ref(ref_, src_, log2width_, log2height_,
                              x, y, &sse2, use_high_bit_depth_, bit_depth_);
      EXPECT_EQ(sse1, sse2) << "for xoffset " << x << " and yoffset " << y;
      EXPECT_EQ(var1, var2) << "for xoffset " << x << " and yoffset " << y;
    }
  }
}

#if CONFIG_VP9_ENCODER
template<>
void SubpelVarianceTest<vp9_subp_avg_variance_fn_t>::RefTest() {
  for (int x = 0; x < 8; ++x) {
    for (int y = 0; y < 8; ++y) {
      if (!use_high_bit_depth_) {
        for (int j = 0; j < block_size_; j++) {
          src_[j] = rnd_.Rand8();
          sec_[j] = rnd_.Rand8();
        }
        for (int j = 0; j < block_size_ + width_ + height_ + 1; j++) {
          ref_[j] = rnd_.Rand8();
        }
#if CONFIG_VP9_HIGHBITDEPTH
      } else {
        for (int j = 0; j < block_size_; j++) {
          CONVERT_TO_SHORTPTR(src_)[j] = rnd_.Rand16() & mask_;
          CONVERT_TO_SHORTPTR(sec_)[j] = rnd_.Rand16() & mask_;
        }
        for (int j = 0; j < block_size_ + width_ + height_ + 1; j++) {
          CONVERT_TO_SHORTPTR(ref_)[j] = rnd_.Rand16() & mask_;
        }
#endif  // CONFIG_VP9_HIGHBITDEPTH
      }
      unsigned int sse1, sse2;
      unsigned int var1;
      ASM_REGISTER_STATE_CHECK(
          var1 = subpel_variance_(ref_, width_ + 1, x, y,
                                  src_, width_, &sse1, sec_));
      const unsigned int var2 = subpel_avg_variance_ref(ref_, src_, sec_,
                                                        log2width_, log2height_,
                                                        x, y, &sse2,
                                                        use_high_bit_depth_,
                                                        bit_depth_);
      EXPECT_EQ(sse1, sse2) << "at position " << x << ", " << y;
      EXPECT_EQ(var1, var2) << "at position " << x << ", " << y;
    }
  }
}
#endif  // CONFIG_VP9_ENCODER

typedef MseTest<Get4x4SseFunc> VpxSseTest;
typedef MseTest<VarianceMxNFunc> VpxMseTest;
typedef VarianceTest<VarianceMxNFunc> VpxVarianceTest;

TEST_P(VpxSseTest, Ref_sse) { RefTest_sse(); }
TEST_P(VpxSseTest, Max_sse) { MaxTest_sse(); }
TEST_P(VpxMseTest, Ref_mse) { RefTest_mse(); }
TEST_P(VpxMseTest, Max_mse) { MaxTest_mse(); }
TEST_P(VpxVarianceTest, Zero) { ZeroTest(); }
TEST_P(VpxVarianceTest, Ref) { RefTest(); }
TEST_P(VpxVarianceTest, RefStride) { RefStrideTest(); }
TEST_P(VpxVarianceTest, OneQuarter) { OneQuarterTest(); }
TEST_P(SumOfSquaresTest, Const) { ConstTest(); }
TEST_P(SumOfSquaresTest, Ref) { RefTest(); }

INSTANTIATE_TEST_CASE_P(C, SumOfSquaresTest,
                        ::testing::Values(vpx_get_mb_ss_c));

const Get4x4SseFunc get4x4sse_cs_c = vpx_get4x4sse_cs_c;
INSTANTIATE_TEST_CASE_P(C, VpxSseTest,
                        ::testing::Values(make_tuple(2, 2, get4x4sse_cs_c)));

const VarianceMxNFunc mse16x16_c = vpx_mse16x16_c;
const VarianceMxNFunc mse16x8_c = vpx_mse16x8_c;
const VarianceMxNFunc mse8x16_c = vpx_mse8x16_c;
const VarianceMxNFunc mse8x8_c = vpx_mse8x8_c;
INSTANTIATE_TEST_CASE_P(C, VpxMseTest,
                        ::testing::Values(make_tuple(4, 4, mse16x16_c),
                                          make_tuple(4, 3, mse16x8_c),
                                          make_tuple(3, 4, mse8x16_c),
                                          make_tuple(3, 3, mse8x8_c)));

const VarianceMxNFunc variance64x64_c = vpx_variance64x64_c;
const VarianceMxNFunc variance64x32_c = vpx_variance64x32_c;
const VarianceMxNFunc variance32x64_c = vpx_variance32x64_c;
const VarianceMxNFunc variance32x32_c = vpx_variance32x32_c;
const VarianceMxNFunc variance32x16_c = vpx_variance32x16_c;
const VarianceMxNFunc variance16x32_c = vpx_variance16x32_c;
const VarianceMxNFunc variance16x16_c = vpx_variance16x16_c;
const VarianceMxNFunc variance16x8_c = vpx_variance16x8_c;
const VarianceMxNFunc variance8x16_c = vpx_variance8x16_c;
const VarianceMxNFunc variance8x8_c = vpx_variance8x8_c;
const VarianceMxNFunc variance8x4_c = vpx_variance8x4_c;
const VarianceMxNFunc variance4x8_c = vpx_variance4x8_c;
const VarianceMxNFunc variance4x4_c = vpx_variance4x4_c;

INSTANTIATE_TEST_CASE_P(
    C, VpxVarianceTest,
    ::testing::Values(make_tuple(6, 6, variance64x64_c, 0),
                      make_tuple(6, 5, variance64x32_c, 0),
                      make_tuple(5, 6, variance32x64_c, 0),
                      make_tuple(5, 5, variance32x32_c, 0),
                      make_tuple(5, 4, variance32x16_c, 0),
                      make_tuple(4, 5, variance16x32_c, 0),
                      make_tuple(4, 4, variance16x16_c, 0),
                      make_tuple(4, 3, variance16x8_c, 0),
                      make_tuple(3, 4, variance8x16_c, 0),
                      make_tuple(3, 3, variance8x8_c, 0),
                      make_tuple(3, 2, variance8x4_c, 0),
                      make_tuple(2, 3, variance4x8_c, 0),
                      make_tuple(2, 2, variance4x4_c, 0)));

#if CONFIG_VP9_HIGHBITDEPTH
typedef MseTest<VarianceMxNFunc> VpxHBDMseTest;
typedef VarianceTest<VarianceMxNFunc> VpxHBDVarianceTest;

TEST_P(VpxHBDMseTest, Ref_mse) { RefTest_mse(); }
TEST_P(VpxHBDMseTest, Max_mse) { MaxTest_mse(); }
TEST_P(VpxHBDVarianceTest, Zero) { ZeroTest(); }
TEST_P(VpxHBDVarianceTest, Ref) { RefTest(); }
TEST_P(VpxHBDVarianceTest, RefStride) { RefStrideTest(); }
TEST_P(VpxHBDVarianceTest, OneQuarter) { OneQuarterTest(); }

/* TODO(debargha): This test does not support the highbd version
const VarianceMxNFunc highbd_12_mse16x16_c = vpx_highbd_12_mse16x16_c;
const VarianceMxNFunc highbd_12_mse16x8_c = vpx_highbd_12_mse16x8_c;
const VarianceMxNFunc highbd_12_mse8x16_c = vpx_highbd_12_mse8x16_c;
const VarianceMxNFunc highbd_12_mse8x8_c = vpx_highbd_12_mse8x8_c;

const VarianceMxNFunc highbd_10_mse16x16_c = vpx_highbd_10_mse16x16_c;
const VarianceMxNFunc highbd_10_mse16x8_c = vpx_highbd_10_mse16x8_c;
const VarianceMxNFunc highbd_10_mse8x16_c = vpx_highbd_10_mse8x16_c;
const VarianceMxNFunc highbd_10_mse8x8_c = vpx_highbd_10_mse8x8_c;

const VarianceMxNFunc highbd_8_mse16x16_c = vpx_highbd_8_mse16x16_c;
const VarianceMxNFunc highbd_8_mse16x8_c = vpx_highbd_8_mse16x8_c;
const VarianceMxNFunc highbd_8_mse8x16_c = vpx_highbd_8_mse8x16_c;
const VarianceMxNFunc highbd_8_mse8x8_c = vpx_highbd_8_mse8x8_c;
INSTANTIATE_TEST_CASE_P(
    C, VpxHBDMseTest, ::testing::Values(make_tuple(4, 4, highbd_12_mse16x16_c),
                                        make_tuple(4, 4, highbd_12_mse16x8_c),
                                        make_tuple(4, 4, highbd_12_mse8x16_c),
                                        make_tuple(4, 4, highbd_12_mse8x8_c),
                                        make_tuple(4, 4, highbd_10_mse16x16_c),
                                        make_tuple(4, 4, highbd_10_mse16x8_c),
                                        make_tuple(4, 4, highbd_10_mse8x16_c),
                                        make_tuple(4, 4, highbd_10_mse8x8_c),
                                        make_tuple(4, 4, highbd_8_mse16x16_c),
                                        make_tuple(4, 4, highbd_8_mse16x8_c),
                                        make_tuple(4, 4, highbd_8_mse8x16_c),
                                        make_tuple(4, 4, highbd_8_mse8x8_c)));
*/

const VarianceMxNFunc highbd_12_variance64x64_c = vpx_highbd_12_variance64x64_c;
const VarianceMxNFunc highbd_12_variance64x32_c = vpx_highbd_12_variance64x32_c;
const VarianceMxNFunc highbd_12_variance32x64_c = vpx_highbd_12_variance32x64_c;
const VarianceMxNFunc highbd_12_variance32x32_c = vpx_highbd_12_variance32x32_c;
const VarianceMxNFunc highbd_12_variance32x16_c = vpx_highbd_12_variance32x16_c;
const VarianceMxNFunc highbd_12_variance16x32_c = vpx_highbd_12_variance16x32_c;
const VarianceMxNFunc highbd_12_variance16x16_c = vpx_highbd_12_variance16x16_c;
const VarianceMxNFunc highbd_12_variance16x8_c = vpx_highbd_12_variance16x8_c;
const VarianceMxNFunc highbd_12_variance8x16_c = vpx_highbd_12_variance8x16_c;
const VarianceMxNFunc highbd_12_variance8x8_c = vpx_highbd_12_variance8x8_c;
const VarianceMxNFunc highbd_12_variance8x4_c = vpx_highbd_12_variance8x4_c;
const VarianceMxNFunc highbd_12_variance4x8_c = vpx_highbd_12_variance4x8_c;
const VarianceMxNFunc highbd_12_variance4x4_c = vpx_highbd_12_variance4x4_c;

const VarianceMxNFunc highbd_10_variance64x64_c = vpx_highbd_10_variance64x64_c;
const VarianceMxNFunc highbd_10_variance64x32_c = vpx_highbd_10_variance64x32_c;
const VarianceMxNFunc highbd_10_variance32x64_c = vpx_highbd_10_variance32x64_c;
const VarianceMxNFunc highbd_10_variance32x32_c = vpx_highbd_10_variance32x32_c;
const VarianceMxNFunc highbd_10_variance32x16_c = vpx_highbd_10_variance32x16_c;
const VarianceMxNFunc highbd_10_variance16x32_c = vpx_highbd_10_variance16x32_c;
const VarianceMxNFunc highbd_10_variance16x16_c = vpx_highbd_10_variance16x16_c;
const VarianceMxNFunc highbd_10_variance16x8_c = vpx_highbd_10_variance16x8_c;
const VarianceMxNFunc highbd_10_variance8x16_c = vpx_highbd_10_variance8x16_c;
const VarianceMxNFunc highbd_10_variance8x8_c = vpx_highbd_10_variance8x8_c;
const VarianceMxNFunc highbd_10_variance8x4_c = vpx_highbd_10_variance8x4_c;
const VarianceMxNFunc highbd_10_variance4x8_c = vpx_highbd_10_variance4x8_c;
const VarianceMxNFunc highbd_10_variance4x4_c = vpx_highbd_10_variance4x4_c;

const VarianceMxNFunc highbd_8_variance64x64_c = vpx_highbd_8_variance64x64_c;
const VarianceMxNFunc highbd_8_variance64x32_c = vpx_highbd_8_variance64x32_c;
const VarianceMxNFunc highbd_8_variance32x64_c = vpx_highbd_8_variance32x64_c;
const VarianceMxNFunc highbd_8_variance32x32_c = vpx_highbd_8_variance32x32_c;
const VarianceMxNFunc highbd_8_variance32x16_c = vpx_highbd_8_variance32x16_c;
const VarianceMxNFunc highbd_8_variance16x32_c = vpx_highbd_8_variance16x32_c;
const VarianceMxNFunc highbd_8_variance16x16_c = vpx_highbd_8_variance16x16_c;
const VarianceMxNFunc highbd_8_variance16x8_c = vpx_highbd_8_variance16x8_c;
const VarianceMxNFunc highbd_8_variance8x16_c = vpx_highbd_8_variance8x16_c;
const VarianceMxNFunc highbd_8_variance8x8_c = vpx_highbd_8_variance8x8_c;
const VarianceMxNFunc highbd_8_variance8x4_c = vpx_highbd_8_variance8x4_c;
const VarianceMxNFunc highbd_8_variance4x8_c = vpx_highbd_8_variance4x8_c;
const VarianceMxNFunc highbd_8_variance4x4_c = vpx_highbd_8_variance4x4_c;
INSTANTIATE_TEST_CASE_P(
    C, VpxHBDVarianceTest,
    ::testing::Values(make_tuple(6, 6, highbd_12_variance64x64_c, 12),
                      make_tuple(6, 5, highbd_12_variance64x32_c, 12),
                      make_tuple(5, 6, highbd_12_variance32x64_c, 12),
                      make_tuple(5, 5, highbd_12_variance32x32_c, 12),
                      make_tuple(5, 4, highbd_12_variance32x16_c, 12),
                      make_tuple(4, 5, highbd_12_variance16x32_c, 12),
                      make_tuple(4, 4, highbd_12_variance16x16_c, 12),
                      make_tuple(4, 3, highbd_12_variance16x8_c, 12),
                      make_tuple(3, 4, highbd_12_variance8x16_c, 12),
                      make_tuple(3, 3, highbd_12_variance8x8_c, 12),
                      make_tuple(3, 2, highbd_12_variance8x4_c, 12),
                      make_tuple(2, 3, highbd_12_variance4x8_c, 12),
                      make_tuple(2, 2, highbd_12_variance4x4_c, 12),
                      make_tuple(6, 6, highbd_10_variance64x64_c, 10),
                      make_tuple(6, 5, highbd_10_variance64x32_c, 10),
                      make_tuple(5, 6, highbd_10_variance32x64_c, 10),
                      make_tuple(5, 5, highbd_10_variance32x32_c, 10),
                      make_tuple(5, 4, highbd_10_variance32x16_c, 10),
                      make_tuple(4, 5, highbd_10_variance16x32_c, 10),
                      make_tuple(4, 4, highbd_10_variance16x16_c, 10),
                      make_tuple(4, 3, highbd_10_variance16x8_c, 10),
                      make_tuple(3, 4, highbd_10_variance8x16_c, 10),
                      make_tuple(3, 3, highbd_10_variance8x8_c, 10),
                      make_tuple(3, 2, highbd_10_variance8x4_c, 10),
                      make_tuple(2, 3, highbd_10_variance4x8_c, 10),
                      make_tuple(2, 2, highbd_10_variance4x4_c, 10),
                      make_tuple(6, 6, highbd_8_variance64x64_c, 8),
                      make_tuple(6, 5, highbd_8_variance64x32_c, 8),
                      make_tuple(5, 6, highbd_8_variance32x64_c, 8),
                      make_tuple(5, 5, highbd_8_variance32x32_c, 8),
                      make_tuple(5, 4, highbd_8_variance32x16_c, 8),
                      make_tuple(4, 5, highbd_8_variance16x32_c, 8),
                      make_tuple(4, 4, highbd_8_variance16x16_c, 8),
                      make_tuple(4, 3, highbd_8_variance16x8_c, 8),
                      make_tuple(3, 4, highbd_8_variance8x16_c, 8),
                      make_tuple(3, 3, highbd_8_variance8x8_c, 8),
                      make_tuple(3, 2, highbd_8_variance8x4_c, 8),
                      make_tuple(2, 3, highbd_8_variance4x8_c, 8),
                      make_tuple(2, 2, highbd_8_variance4x4_c, 8)));
#endif  // CONFIG_VP9_HIGHBITDEPTH

#if HAVE_MMX
const VarianceMxNFunc mse16x16_mmx = vpx_mse16x16_mmx;
INSTANTIATE_TEST_CASE_P(MMX, VpxMseTest,
                        ::testing::Values(make_tuple(4, 4, mse16x16_mmx)));

INSTANTIATE_TEST_CASE_P(MMX, SumOfSquaresTest,
                        ::testing::Values(vpx_get_mb_ss_mmx));

const VarianceMxNFunc variance16x16_mmx = vpx_variance16x16_mmx;
const VarianceMxNFunc variance16x8_mmx = vpx_variance16x8_mmx;
const VarianceMxNFunc variance8x16_mmx = vpx_variance8x16_mmx;
const VarianceMxNFunc variance8x8_mmx = vpx_variance8x8_mmx;
const VarianceMxNFunc variance4x4_mmx = vpx_variance4x4_mmx;
INSTANTIATE_TEST_CASE_P(
    MMX, VpxVarianceTest,
    ::testing::Values(make_tuple(4, 4, variance16x16_mmx, 0),
                      make_tuple(4, 3, variance16x8_mmx, 0),
                      make_tuple(3, 4, variance8x16_mmx, 0),
                      make_tuple(3, 3, variance8x8_mmx, 0),
                      make_tuple(2, 2, variance4x4_mmx, 0)));
#endif  // HAVE_MMX

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(SSE2, SumOfSquaresTest,
                        ::testing::Values(vpx_get_mb_ss_sse2));

const VarianceMxNFunc mse16x16_sse2 = vpx_mse16x16_sse2;
const VarianceMxNFunc mse16x8_sse2 = vpx_mse16x8_sse2;
const VarianceMxNFunc mse8x16_sse2 = vpx_mse8x16_sse2;
const VarianceMxNFunc mse8x8_sse2 = vpx_mse8x8_sse2;
INSTANTIATE_TEST_CASE_P(SSE2, VpxMseTest,
                        ::testing::Values(make_tuple(4, 4, mse16x16_sse2),
                                          make_tuple(4, 3, mse16x8_sse2),
                                          make_tuple(3, 4, mse8x16_sse2),
                                          make_tuple(3, 3, mse8x8_sse2)));

const VarianceMxNFunc variance64x64_sse2 = vpx_variance64x64_sse2;
const VarianceMxNFunc variance64x32_sse2 = vpx_variance64x32_sse2;
const VarianceMxNFunc variance32x64_sse2 = vpx_variance32x64_sse2;
const VarianceMxNFunc variance32x32_sse2 = vpx_variance32x32_sse2;
const VarianceMxNFunc variance32x16_sse2 = vpx_variance32x16_sse2;
const VarianceMxNFunc variance16x32_sse2 = vpx_variance16x32_sse2;
const VarianceMxNFunc variance16x16_sse2 = vpx_variance16x16_sse2;
const VarianceMxNFunc variance16x8_sse2 = vpx_variance16x8_sse2;
const VarianceMxNFunc variance8x16_sse2 = vpx_variance8x16_sse2;
const VarianceMxNFunc variance8x8_sse2 = vpx_variance8x8_sse2;
const VarianceMxNFunc variance8x4_sse2 = vpx_variance8x4_sse2;
const VarianceMxNFunc variance4x8_sse2 = vpx_variance4x8_sse2;
const VarianceMxNFunc variance4x4_sse2 = vpx_variance4x4_sse2;
INSTANTIATE_TEST_CASE_P(
    SSE2, VpxVarianceTest,
    ::testing::Values(make_tuple(6, 6, variance64x64_sse2, 0),
                      make_tuple(6, 5, variance64x32_sse2, 0),
                      make_tuple(5, 6, variance32x64_sse2, 0),
                      make_tuple(5, 5, variance32x32_sse2, 0),
                      make_tuple(5, 4, variance32x16_sse2, 0),
                      make_tuple(4, 5, variance16x32_sse2, 0),
                      make_tuple(4, 4, variance16x16_sse2, 0),
                      make_tuple(4, 3, variance16x8_sse2, 0),
                      make_tuple(3, 4, variance8x16_sse2, 0),
                      make_tuple(3, 3, variance8x8_sse2, 0),
                      make_tuple(3, 2, variance8x4_sse2, 0),
                      make_tuple(2, 3, variance4x8_sse2, 0),
                      make_tuple(2, 2, variance4x4_sse2, 0)));
#if CONFIG_VP9_HIGHBITDEPTH
/* TODO(debargha): This test does not support the highbd version
const VarianceMxNFunc highbd_12_mse16x16_sse2 = vpx_highbd_12_mse16x16_sse2;
const VarianceMxNFunc highbd_12_mse16x8_sse2 = vpx_highbd_12_mse16x8_sse2;
const VarianceMxNFunc highbd_12_mse8x16_sse2 = vpx_highbd_12_mse8x16_sse2;
const VarianceMxNFunc highbd_12_mse8x8_sse2 = vpx_highbd_12_mse8x8_sse2;

const VarianceMxNFunc highbd_10_mse16x16_sse2 = vpx_highbd_10_mse16x16_sse2;
const VarianceMxNFunc highbd_10_mse16x8_sse2 = vpx_highbd_10_mse16x8_sse2;
const VarianceMxNFunc highbd_10_mse8x16_sse2 = vpx_highbd_10_mse8x16_sse2;
const VarianceMxNFunc highbd_10_mse8x8_sse2 = vpx_highbd_10_mse8x8_sse2;

const VarianceMxNFunc highbd_8_mse16x16_sse2 = vpx_highbd_8_mse16x16_sse2;
const VarianceMxNFunc highbd_8_mse16x8_sse2 = vpx_highbd_8_mse16x8_sse2;
const VarianceMxNFunc highbd_8_mse8x16_sse2 = vpx_highbd_8_mse8x16_sse2;
const VarianceMxNFunc highbd_8_mse8x8_sse2 = vpx_highbd_8_mse8x8_sse2;
INSTANTIATE_TEST_CASE_P(
    SSE2, VpxHBDMseTest, ::testing::Values(make_tuple(4, 4, highbd_12_mse16x16_sse2),
                                           make_tuple(4, 3, highbd_12_mse16x8_sse2),
                                           make_tuple(3, 4, highbd_12_mse8x16_sse2),
                                           make_tuple(3, 3, highbd_12_mse8x8_sse2),
                                           make_tuple(4, 4, highbd_10_mse16x16_sse2),
                                           make_tuple(4, 3, highbd_10_mse16x8_sse2),
                                           make_tuple(3, 4, highbd_10_mse8x16_sse2),
                                           make_tuple(3, 3, highbd_10_mse8x8_sse2),
                                           make_tuple(4, 4, highbd_8_mse16x16_sse2),
                                           make_tuple(4, 3, highbd_8_mse16x8_sse2),
                                           make_tuple(3, 4, highbd_8_mse8x16_sse2),
                                           make_tuple(3, 3, highbd_8_mse8x8_sse2)));
*/

const VarianceMxNFunc highbd_12_variance64x64_sse2 =
    vpx_highbd_12_variance64x64_sse2;
const VarianceMxNFunc highbd_12_variance64x32_sse2 =
    vpx_highbd_12_variance64x32_sse2;
const VarianceMxNFunc highbd_12_variance32x64_sse2 =
    vpx_highbd_12_variance32x64_sse2;
const VarianceMxNFunc highbd_12_variance32x32_sse2 =
    vpx_highbd_12_variance32x32_sse2;
const VarianceMxNFunc highbd_12_variance32x16_sse2 =
    vpx_highbd_12_variance32x16_sse2;
const VarianceMxNFunc highbd_12_variance16x32_sse2 =
    vpx_highbd_12_variance16x32_sse2;
const VarianceMxNFunc highbd_12_variance16x16_sse2 =
    vpx_highbd_12_variance16x16_sse2;
const VarianceMxNFunc highbd_12_variance16x8_sse2 =
    vpx_highbd_12_variance16x8_sse2;
const VarianceMxNFunc highbd_12_variance8x16_sse2 =
    vpx_highbd_12_variance8x16_sse2;
const VarianceMxNFunc highbd_12_variance8x8_sse2 =
    vpx_highbd_12_variance8x8_sse2;
const VarianceMxNFunc highbd_10_variance64x64_sse2 =
    vpx_highbd_10_variance64x64_sse2;
const VarianceMxNFunc highbd_10_variance64x32_sse2 =
    vpx_highbd_10_variance64x32_sse2;
const VarianceMxNFunc highbd_10_variance32x64_sse2 =
    vpx_highbd_10_variance32x64_sse2;
const VarianceMxNFunc highbd_10_variance32x32_sse2 =
    vpx_highbd_10_variance32x32_sse2;
const VarianceMxNFunc highbd_10_variance32x16_sse2 =
    vpx_highbd_10_variance32x16_sse2;
const VarianceMxNFunc highbd_10_variance16x32_sse2 =
    vpx_highbd_10_variance16x32_sse2;
const VarianceMxNFunc highbd_10_variance16x16_sse2 =
    vpx_highbd_10_variance16x16_sse2;
const VarianceMxNFunc highbd_10_variance16x8_sse2 =
    vpx_highbd_10_variance16x8_sse2;
const VarianceMxNFunc highbd_10_variance8x16_sse2 =
    vpx_highbd_10_variance8x16_sse2;
const VarianceMxNFunc highbd_10_variance8x8_sse2 =
    vpx_highbd_10_variance8x8_sse2;
const VarianceMxNFunc highbd_8_variance64x64_sse2 =
    vpx_highbd_8_variance64x64_sse2;
const VarianceMxNFunc highbd_8_variance64x32_sse2 =
    vpx_highbd_8_variance64x32_sse2;
const VarianceMxNFunc highbd_8_variance32x64_sse2 =
    vpx_highbd_8_variance32x64_sse2;
const VarianceMxNFunc highbd_8_variance32x32_sse2 =
    vpx_highbd_8_variance32x32_sse2;
const VarianceMxNFunc highbd_8_variance32x16_sse2 =
    vpx_highbd_8_variance32x16_sse2;
const VarianceMxNFunc highbd_8_variance16x32_sse2 =
    vpx_highbd_8_variance16x32_sse2;
const VarianceMxNFunc highbd_8_variance16x16_sse2 =
    vpx_highbd_8_variance16x16_sse2;
const VarianceMxNFunc highbd_8_variance16x8_sse2 =
    vpx_highbd_8_variance16x8_sse2;
const VarianceMxNFunc highbd_8_variance8x16_sse2 =
    vpx_highbd_8_variance8x16_sse2;
const VarianceMxNFunc highbd_8_variance8x8_sse2 =
    vpx_highbd_8_variance8x8_sse2;

INSTANTIATE_TEST_CASE_P(
    SSE2, VpxHBDVarianceTest,
    ::testing::Values(make_tuple(6, 6, highbd_12_variance64x64_sse2, 12),
                      make_tuple(6, 5, highbd_12_variance64x32_sse2, 12),
                      make_tuple(5, 6, highbd_12_variance32x64_sse2, 12),
                      make_tuple(5, 5, highbd_12_variance32x32_sse2, 12),
                      make_tuple(5, 4, highbd_12_variance32x16_sse2, 12),
                      make_tuple(4, 5, highbd_12_variance16x32_sse2, 12),
                      make_tuple(4, 4, highbd_12_variance16x16_sse2, 12),
                      make_tuple(4, 3, highbd_12_variance16x8_sse2, 12),
                      make_tuple(3, 4, highbd_12_variance8x16_sse2, 12),
                      make_tuple(3, 3, highbd_12_variance8x8_sse2, 12),
                      make_tuple(6, 6, highbd_10_variance64x64_sse2, 10),
                      make_tuple(6, 5, highbd_10_variance64x32_sse2, 10),
                      make_tuple(5, 6, highbd_10_variance32x64_sse2, 10),
                      make_tuple(5, 5, highbd_10_variance32x32_sse2, 10),
                      make_tuple(5, 4, highbd_10_variance32x16_sse2, 10),
                      make_tuple(4, 5, highbd_10_variance16x32_sse2, 10),
                      make_tuple(4, 4, highbd_10_variance16x16_sse2, 10),
                      make_tuple(4, 3, highbd_10_variance16x8_sse2, 10),
                      make_tuple(3, 4, highbd_10_variance8x16_sse2, 10),
                      make_tuple(3, 3, highbd_10_variance8x8_sse2, 10),
                      make_tuple(6, 6, highbd_8_variance64x64_sse2, 8),
                      make_tuple(6, 5, highbd_8_variance64x32_sse2, 8),
                      make_tuple(5, 6, highbd_8_variance32x64_sse2, 8),
                      make_tuple(5, 5, highbd_8_variance32x32_sse2, 8),
                      make_tuple(5, 4, highbd_8_variance32x16_sse2, 8),
                      make_tuple(4, 5, highbd_8_variance16x32_sse2, 8),
                      make_tuple(4, 4, highbd_8_variance16x16_sse2, 8),
                      make_tuple(4, 3, highbd_8_variance16x8_sse2, 8),
                      make_tuple(3, 4, highbd_8_variance8x16_sse2, 8),
                      make_tuple(3, 3, highbd_8_variance8x8_sse2, 8)));
#endif  // CONFIG_VP9_HIGHBITDEPTH
#endif  // HAVE_SSE2

#if CONFIG_VP8
typedef SubpelVarianceTest<SubpixVarMxNFunc> VP8SubpelVarianceTest;

TEST_P(VP8SubpelVarianceTest, Ref) { RefTest(); }
TEST_P(VP8SubpelVarianceTest, ExtremeRef) { ExtremeRefTest(); }
#endif  // CONFIG_VP8

#if CONFIG_VP9_ENCODER
typedef SubpelVarianceTest<SubpixVarMxNFunc> VP9SubpelVarianceTest;
typedef SubpelVarianceTest<vp9_subp_avg_variance_fn_t> VP9SubpelAvgVarianceTest;

TEST_P(VP9SubpelVarianceTest, Ref) { RefTest(); }
TEST_P(VP9SubpelVarianceTest, ExtremeRef) { ExtremeRefTest(); }
TEST_P(VP9SubpelAvgVarianceTest, Ref) { RefTest(); }

#if CONFIG_VP9_HIGHBITDEPTH
typedef SubpelVarianceTest<SubpixVarMxNFunc> VP9SubpelVarianceHighTest;
typedef SubpelVarianceTest<vp9_subp_avg_variance_fn_t>
    VP9SubpelAvgVarianceHighTest;

TEST_P(VP9SubpelVarianceHighTest, Ref) { RefTest(); }
TEST_P(VP9SubpelVarianceHighTest, ExtremeRef) { ExtremeRefTest(); }
TEST_P(VP9SubpelAvgVarianceHighTest, Ref) { RefTest(); }
#endif  // CONFIG_VP9_HIGHBITDEPTH

const SubpixVarMxNFunc subpel_variance4x4_c = vp9_sub_pixel_variance4x4_c;
const SubpixVarMxNFunc subpel_variance4x8_c = vp9_sub_pixel_variance4x8_c;
const SubpixVarMxNFunc subpel_variance8x4_c = vp9_sub_pixel_variance8x4_c;
const SubpixVarMxNFunc subpel_variance8x8_c = vp9_sub_pixel_variance8x8_c;
const SubpixVarMxNFunc subpel_variance8x16_c = vp9_sub_pixel_variance8x16_c;
const SubpixVarMxNFunc subpel_variance16x8_c = vp9_sub_pixel_variance16x8_c;
const SubpixVarMxNFunc subpel_variance16x16_c = vp9_sub_pixel_variance16x16_c;
const SubpixVarMxNFunc subpel_variance16x32_c = vp9_sub_pixel_variance16x32_c;
const SubpixVarMxNFunc subpel_variance32x16_c = vp9_sub_pixel_variance32x16_c;
const SubpixVarMxNFunc subpel_variance32x32_c = vp9_sub_pixel_variance32x32_c;
const SubpixVarMxNFunc subpel_variance32x64_c = vp9_sub_pixel_variance32x64_c;
const SubpixVarMxNFunc subpel_variance64x32_c = vp9_sub_pixel_variance64x32_c;
const SubpixVarMxNFunc subpel_variance64x64_c = vp9_sub_pixel_variance64x64_c;
INSTANTIATE_TEST_CASE_P(
    C, VP9SubpelVarianceTest,
    ::testing::Values(make_tuple(2, 2, subpel_variance4x4_c, 0),
                      make_tuple(2, 3, subpel_variance4x8_c, 0),
                      make_tuple(3, 2, subpel_variance8x4_c, 0),
                      make_tuple(3, 3, subpel_variance8x8_c, 0),
                      make_tuple(3, 4, subpel_variance8x16_c, 0),
                      make_tuple(4, 3, subpel_variance16x8_c, 0),
                      make_tuple(4, 4, subpel_variance16x16_c, 0),
                      make_tuple(4, 5, subpel_variance16x32_c, 0),
                      make_tuple(5, 4, subpel_variance32x16_c, 0),
                      make_tuple(5, 5, subpel_variance32x32_c, 0),
                      make_tuple(5, 6, subpel_variance32x64_c, 0),
                      make_tuple(6, 5, subpel_variance64x32_c, 0),
                      make_tuple(6, 6, subpel_variance64x64_c, 0)));

#if CONFIG_VP8
const SubpixVarMxNFunc vp8_subpel_variance16x16_c =
    vp8_sub_pixel_variance16x16_c;
const SubpixVarMxNFunc vp8_subpel_variance16x8_c = vp8_sub_pixel_variance16x8_c;
const SubpixVarMxNFunc vp8_subpel_variance8x16_c = vp8_sub_pixel_variance8x16_c;
const SubpixVarMxNFunc vp8_subpel_variance8x8_c = vp8_sub_pixel_variance8x8_c;
const SubpixVarMxNFunc vp8_subpel_variance4x4_c = vp8_sub_pixel_variance4x4_c;
INSTANTIATE_TEST_CASE_P(
    C, VP8SubpelVarianceTest,
    ::testing::Values(make_tuple(2, 2, vp8_subpel_variance4x4_c, 0),
                      make_tuple(3, 3, vp8_subpel_variance8x8_c, 0),
                      make_tuple(3, 4, vp8_subpel_variance8x16_c, 0),
                      make_tuple(4, 3, vp8_subpel_variance16x8_c, 0),
                      make_tuple(4, 4, vp8_subpel_variance16x16_c, 0)));
#endif  // CONFIG_VP8

const vp9_subp_avg_variance_fn_t subpel_avg_variance4x4_c =
    vp9_sub_pixel_avg_variance4x4_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance4x8_c =
    vp9_sub_pixel_avg_variance4x8_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance8x4_c =
    vp9_sub_pixel_avg_variance8x4_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance8x8_c =
    vp9_sub_pixel_avg_variance8x8_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance8x16_c =
    vp9_sub_pixel_avg_variance8x16_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance16x8_c =
    vp9_sub_pixel_avg_variance16x8_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance16x16_c =
    vp9_sub_pixel_avg_variance16x16_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance16x32_c =
    vp9_sub_pixel_avg_variance16x32_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance32x16_c =
    vp9_sub_pixel_avg_variance32x16_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance32x32_c =
    vp9_sub_pixel_avg_variance32x32_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance32x64_c =
    vp9_sub_pixel_avg_variance32x64_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance64x32_c =
    vp9_sub_pixel_avg_variance64x32_c;
const vp9_subp_avg_variance_fn_t subpel_avg_variance64x64_c =
    vp9_sub_pixel_avg_variance64x64_c;
INSTANTIATE_TEST_CASE_P(
    C, VP9SubpelAvgVarianceTest,
    ::testing::Values(make_tuple(2, 2, subpel_avg_variance4x4_c, 0),
                      make_tuple(2, 3, subpel_avg_variance4x8_c, 0),
                      make_tuple(3, 2, subpel_avg_variance8x4_c, 0),
                      make_tuple(3, 3, subpel_avg_variance8x8_c, 0),
                      make_tuple(3, 4, subpel_avg_variance8x16_c, 0),
                      make_tuple(4, 3, subpel_avg_variance16x8_c, 0),
                      make_tuple(4, 4, subpel_avg_variance16x16_c, 0),
                      make_tuple(4, 5, subpel_avg_variance16x32_c, 0),
                      make_tuple(5, 4, subpel_avg_variance32x16_c, 0),
                      make_tuple(5, 5, subpel_avg_variance32x32_c, 0),
                      make_tuple(5, 6, subpel_avg_variance32x64_c, 0),
                      make_tuple(6, 5, subpel_avg_variance64x32_c, 0),
                      make_tuple(6, 6, subpel_avg_variance64x64_c, 0)));
#if CONFIG_VP9_HIGHBITDEPTH
const SubpixVarMxNFunc highbd_10_subpel_variance4x4_c =
    vp9_highbd_10_sub_pixel_variance4x4_c;
const SubpixVarMxNFunc highbd_10_subpel_variance4x8_c =
    vp9_highbd_10_sub_pixel_variance4x8_c;
const SubpixVarMxNFunc highbd_10_subpel_variance8x4_c =
    vp9_highbd_10_sub_pixel_variance8x4_c;
const SubpixVarMxNFunc highbd_10_subpel_variance8x8_c =
    vp9_highbd_10_sub_pixel_variance8x8_c;
const SubpixVarMxNFunc highbd_10_subpel_variance8x16_c =
    vp9_highbd_10_sub_pixel_variance8x16_c;
const SubpixVarMxNFunc highbd_10_subpel_variance16x8_c =
    vp9_highbd_10_sub_pixel_variance16x8_c;
const SubpixVarMxNFunc highbd_10_subpel_variance16x16_c =
    vp9_highbd_10_sub_pixel_variance16x16_c;
const SubpixVarMxNFunc highbd_10_subpel_variance16x32_c =
    vp9_highbd_10_sub_pixel_variance16x32_c;
const SubpixVarMxNFunc highbd_10_subpel_variance32x16_c =
    vp9_highbd_10_sub_pixel_variance32x16_c;
const SubpixVarMxNFunc highbd_10_subpel_variance32x32_c =
    vp9_highbd_10_sub_pixel_variance32x32_c;
const SubpixVarMxNFunc highbd_10_subpel_variance32x64_c =
    vp9_highbd_10_sub_pixel_variance32x64_c;
const SubpixVarMxNFunc highbd_10_subpel_variance64x32_c =
    vp9_highbd_10_sub_pixel_variance64x32_c;
const SubpixVarMxNFunc highbd_10_subpel_variance64x64_c =
    vp9_highbd_10_sub_pixel_variance64x64_c;
const SubpixVarMxNFunc highbd_12_subpel_variance4x4_c =
    vp9_highbd_12_sub_pixel_variance4x4_c;
const SubpixVarMxNFunc highbd_12_subpel_variance4x8_c =
    vp9_highbd_12_sub_pixel_variance4x8_c;
const SubpixVarMxNFunc highbd_12_subpel_variance8x4_c =
    vp9_highbd_12_sub_pixel_variance8x4_c;
const SubpixVarMxNFunc highbd_12_subpel_variance8x8_c =
    vp9_highbd_12_sub_pixel_variance8x8_c;
const SubpixVarMxNFunc highbd_12_subpel_variance8x16_c =
    vp9_highbd_12_sub_pixel_variance8x16_c;
const SubpixVarMxNFunc highbd_12_subpel_variance16x8_c =
    vp9_highbd_12_sub_pixel_variance16x8_c;
const SubpixVarMxNFunc highbd_12_subpel_variance16x16_c =
    vp9_highbd_12_sub_pixel_variance16x16_c;
const SubpixVarMxNFunc highbd_12_subpel_variance16x32_c =
    vp9_highbd_12_sub_pixel_variance16x32_c;
const SubpixVarMxNFunc highbd_12_subpel_variance32x16_c =
    vp9_highbd_12_sub_pixel_variance32x16_c;
const SubpixVarMxNFunc highbd_12_subpel_variance32x32_c =
    vp9_highbd_12_sub_pixel_variance32x32_c;
const SubpixVarMxNFunc highbd_12_subpel_variance32x64_c =
    vp9_highbd_12_sub_pixel_variance32x64_c;
const SubpixVarMxNFunc highbd_12_subpel_variance64x32_c =
    vp9_highbd_12_sub_pixel_variance64x32_c;
const SubpixVarMxNFunc highbd_12_subpel_variance64x64_c =
    vp9_highbd_12_sub_pixel_variance64x64_c;
const SubpixVarMxNFunc highbd_subpel_variance4x4_c =
    vp9_highbd_sub_pixel_variance4x4_c;
const SubpixVarMxNFunc highbd_subpel_variance4x8_c =
    vp9_highbd_sub_pixel_variance4x8_c;
const SubpixVarMxNFunc highbd_subpel_variance8x4_c =
    vp9_highbd_sub_pixel_variance8x4_c;
const SubpixVarMxNFunc highbd_subpel_variance8x8_c =
    vp9_highbd_sub_pixel_variance8x8_c;
const SubpixVarMxNFunc highbd_subpel_variance8x16_c =
    vp9_highbd_sub_pixel_variance8x16_c;
const SubpixVarMxNFunc highbd_subpel_variance16x8_c =
    vp9_highbd_sub_pixel_variance16x8_c;
const SubpixVarMxNFunc highbd_subpel_variance16x16_c =
    vp9_highbd_sub_pixel_variance16x16_c;
const SubpixVarMxNFunc highbd_subpel_variance16x32_c =
    vp9_highbd_sub_pixel_variance16x32_c;
const SubpixVarMxNFunc highbd_subpel_variance32x16_c =
    vp9_highbd_sub_pixel_variance32x16_c;
const SubpixVarMxNFunc highbd_subpel_variance32x32_c =
    vp9_highbd_sub_pixel_variance32x32_c;
const SubpixVarMxNFunc highbd_subpel_variance32x64_c =
    vp9_highbd_sub_pixel_variance32x64_c;
const SubpixVarMxNFunc highbd_subpel_variance64x32_c =
    vp9_highbd_sub_pixel_variance64x32_c;
const SubpixVarMxNFunc highbd_subpel_variance64x64_c =
    vp9_highbd_sub_pixel_variance64x64_c;
INSTANTIATE_TEST_CASE_P(
    C, VP9SubpelVarianceHighTest,
    ::testing::Values(make_tuple(2, 2, highbd_10_subpel_variance4x4_c, 10),
                      make_tuple(2, 3, highbd_10_subpel_variance4x8_c, 10),
                      make_tuple(3, 2, highbd_10_subpel_variance8x4_c, 10),
                      make_tuple(3, 3, highbd_10_subpel_variance8x8_c, 10),
                      make_tuple(3, 4, highbd_10_subpel_variance8x16_c, 10),
                      make_tuple(4, 3, highbd_10_subpel_variance16x8_c, 10),
                      make_tuple(4, 4, highbd_10_subpel_variance16x16_c, 10),
                      make_tuple(4, 5, highbd_10_subpel_variance16x32_c, 10),
                      make_tuple(5, 4, highbd_10_subpel_variance32x16_c, 10),
                      make_tuple(5, 5, highbd_10_subpel_variance32x32_c, 10),
                      make_tuple(5, 6, highbd_10_subpel_variance32x64_c, 10),
                      make_tuple(6, 5, highbd_10_subpel_variance64x32_c, 10),
                      make_tuple(6, 6, highbd_10_subpel_variance64x64_c, 10),
                      make_tuple(2, 2, highbd_12_subpel_variance4x4_c, 12),
                      make_tuple(2, 3, highbd_12_subpel_variance4x8_c, 12),
                      make_tuple(3, 2, highbd_12_subpel_variance8x4_c, 12),
                      make_tuple(3, 3, highbd_12_subpel_variance8x8_c, 12),
                      make_tuple(3, 4, highbd_12_subpel_variance8x16_c, 12),
                      make_tuple(4, 3, highbd_12_subpel_variance16x8_c, 12),
                      make_tuple(4, 4, highbd_12_subpel_variance16x16_c, 12),
                      make_tuple(4, 5, highbd_12_subpel_variance16x32_c, 12),
                      make_tuple(5, 4, highbd_12_subpel_variance32x16_c, 12),
                      make_tuple(5, 5, highbd_12_subpel_variance32x32_c, 12),
                      make_tuple(5, 6, highbd_12_subpel_variance32x64_c, 12),
                      make_tuple(6, 5, highbd_12_subpel_variance64x32_c, 12),
                      make_tuple(6, 6, highbd_12_subpel_variance64x64_c, 12),
                      make_tuple(2, 2, highbd_subpel_variance4x4_c, 8),
                      make_tuple(2, 3, highbd_subpel_variance4x8_c, 8),
                      make_tuple(3, 2, highbd_subpel_variance8x4_c, 8),
                      make_tuple(3, 3, highbd_subpel_variance8x8_c, 8),
                      make_tuple(3, 4, highbd_subpel_variance8x16_c, 8),
                      make_tuple(4, 3, highbd_subpel_variance16x8_c, 8),
                      make_tuple(4, 4, highbd_subpel_variance16x16_c, 8),
                      make_tuple(4, 5, highbd_subpel_variance16x32_c, 8),
                      make_tuple(5, 4, highbd_subpel_variance32x16_c, 8),
                      make_tuple(5, 5, highbd_subpel_variance32x32_c, 8),
                      make_tuple(5, 6, highbd_subpel_variance32x64_c, 8),
                      make_tuple(6, 5, highbd_subpel_variance64x32_c, 8),
                      make_tuple(6, 6, highbd_subpel_variance64x64_c, 8)));
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance4x4_c =
    vp9_highbd_10_sub_pixel_avg_variance4x4_c;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance4x8_c =
    vp9_highbd_10_sub_pixel_avg_variance4x8_c;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance8x4_c =
    vp9_highbd_10_sub_pixel_avg_variance8x4_c;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance8x8_c =
    vp9_highbd_10_sub_pixel_avg_variance8x8_c;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance8x16_c =
    vp9_highbd_10_sub_pixel_avg_variance8x16_c;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance16x8_c =
    vp9_highbd_10_sub_pixel_avg_variance16x8_c;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance16x16_c =
    vp9_highbd_10_sub_pixel_avg_variance16x16_c;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance16x32_c =
    vp9_highbd_10_sub_pixel_avg_variance16x32_c;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance32x16_c =
    vp9_highbd_10_sub_pixel_avg_variance32x16_c;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance32x32_c =
    vp9_highbd_10_sub_pixel_avg_variance32x32_c;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance32x64_c =
    vp9_highbd_10_sub_pixel_avg_variance32x64_c;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance64x32_c =
    vp9_highbd_10_sub_pixel_avg_variance64x32_c;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance64x64_c =
    vp9_highbd_10_sub_pixel_avg_variance64x64_c;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance4x4_c =
    vp9_highbd_12_sub_pixel_avg_variance4x4_c;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance4x8_c =
    vp9_highbd_12_sub_pixel_avg_variance4x8_c;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance8x4_c =
    vp9_highbd_12_sub_pixel_avg_variance8x4_c;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance8x8_c =
    vp9_highbd_12_sub_pixel_avg_variance8x8_c;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance8x16_c =
    vp9_highbd_12_sub_pixel_avg_variance8x16_c;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance16x8_c =
    vp9_highbd_12_sub_pixel_avg_variance16x8_c;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance16x16_c =
    vp9_highbd_12_sub_pixel_avg_variance16x16_c;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance16x32_c =
    vp9_highbd_12_sub_pixel_avg_variance16x32_c;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance32x16_c =
    vp9_highbd_12_sub_pixel_avg_variance32x16_c;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance32x32_c =
    vp9_highbd_12_sub_pixel_avg_variance32x32_c;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance32x64_c =
    vp9_highbd_12_sub_pixel_avg_variance32x64_c;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance64x32_c =
    vp9_highbd_12_sub_pixel_avg_variance64x32_c;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance64x64_c =
    vp9_highbd_12_sub_pixel_avg_variance64x64_c;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance4x4_c =
    vp9_highbd_sub_pixel_avg_variance4x4_c;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance4x8_c =
    vp9_highbd_sub_pixel_avg_variance4x8_c;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance8x4_c =
    vp9_highbd_sub_pixel_avg_variance8x4_c;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance8x8_c =
    vp9_highbd_sub_pixel_avg_variance8x8_c;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance8x16_c =
    vp9_highbd_sub_pixel_avg_variance8x16_c;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance16x8_c =
    vp9_highbd_sub_pixel_avg_variance16x8_c;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance16x16_c =
    vp9_highbd_sub_pixel_avg_variance16x16_c;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance16x32_c =
    vp9_highbd_sub_pixel_avg_variance16x32_c;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance32x16_c =
    vp9_highbd_sub_pixel_avg_variance32x16_c;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance32x32_c =
    vp9_highbd_sub_pixel_avg_variance32x32_c;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance32x64_c =
    vp9_highbd_sub_pixel_avg_variance32x64_c;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance64x32_c =
    vp9_highbd_sub_pixel_avg_variance64x32_c;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance64x64_c =
    vp9_highbd_sub_pixel_avg_variance64x64_c;
INSTANTIATE_TEST_CASE_P(
    C, VP9SubpelAvgVarianceHighTest,
    ::testing::Values(
        make_tuple(2, 2, highbd_10_subpel_avg_variance4x4_c, 10),
        make_tuple(2, 3, highbd_10_subpel_avg_variance4x8_c, 10),
        make_tuple(3, 2, highbd_10_subpel_avg_variance8x4_c, 10),
        make_tuple(3, 3, highbd_10_subpel_avg_variance8x8_c, 10),
        make_tuple(3, 4, highbd_10_subpel_avg_variance8x16_c, 10),
        make_tuple(4, 3, highbd_10_subpel_avg_variance16x8_c, 10),
        make_tuple(4, 4, highbd_10_subpel_avg_variance16x16_c, 10),
        make_tuple(4, 5, highbd_10_subpel_avg_variance16x32_c, 10),
        make_tuple(5, 4, highbd_10_subpel_avg_variance32x16_c, 10),
        make_tuple(5, 5, highbd_10_subpel_avg_variance32x32_c, 10),
        make_tuple(5, 6, highbd_10_subpel_avg_variance32x64_c, 10),
        make_tuple(6, 5, highbd_10_subpel_avg_variance64x32_c, 10),
        make_tuple(6, 6, highbd_10_subpel_avg_variance64x64_c, 10),
        make_tuple(2, 2, highbd_12_subpel_avg_variance4x4_c, 12),
        make_tuple(2, 3, highbd_12_subpel_avg_variance4x8_c, 12),
        make_tuple(3, 2, highbd_12_subpel_avg_variance8x4_c, 12),
        make_tuple(3, 3, highbd_12_subpel_avg_variance8x8_c, 12),
        make_tuple(3, 4, highbd_12_subpel_avg_variance8x16_c, 12),
        make_tuple(4, 3, highbd_12_subpel_avg_variance16x8_c, 12),
        make_tuple(4, 4, highbd_12_subpel_avg_variance16x16_c, 12),
        make_tuple(4, 5, highbd_12_subpel_avg_variance16x32_c, 12),
        make_tuple(5, 4, highbd_12_subpel_avg_variance32x16_c, 12),
        make_tuple(5, 5, highbd_12_subpel_avg_variance32x32_c, 12),
        make_tuple(5, 6, highbd_12_subpel_avg_variance32x64_c, 12),
        make_tuple(6, 5, highbd_12_subpel_avg_variance64x32_c, 12),
        make_tuple(6, 6, highbd_12_subpel_avg_variance64x64_c, 12),
        make_tuple(2, 2, highbd_subpel_avg_variance4x4_c, 8),
        make_tuple(2, 3, highbd_subpel_avg_variance4x8_c, 8),
        make_tuple(3, 2, highbd_subpel_avg_variance8x4_c, 8),
        make_tuple(3, 3, highbd_subpel_avg_variance8x8_c, 8),
        make_tuple(3, 4, highbd_subpel_avg_variance8x16_c, 8),
        make_tuple(4, 3, highbd_subpel_avg_variance16x8_c, 8),
        make_tuple(4, 4, highbd_subpel_avg_variance16x16_c, 8),
        make_tuple(4, 5, highbd_subpel_avg_variance16x32_c, 8),
        make_tuple(5, 4, highbd_subpel_avg_variance32x16_c, 8),
        make_tuple(5, 5, highbd_subpel_avg_variance32x32_c, 8),
        make_tuple(5, 6, highbd_subpel_avg_variance32x64_c, 8),
        make_tuple(6, 5, highbd_subpel_avg_variance64x32_c, 8),
        make_tuple(6, 6, highbd_subpel_avg_variance64x64_c, 8)));
#endif  // CONFIG_VP9_HIGHBITDEPTH
#endif  // CONFIG_VP9_ENCODER

#if CONFIG_VP8
#if HAVE_MMX
const SubpixVarMxNFunc subpel_variance16x16_mmx =
    vp8_sub_pixel_variance16x16_mmx;
const SubpixVarMxNFunc subpel_variance16x8_mmx = vp8_sub_pixel_variance16x8_mmx;
const SubpixVarMxNFunc subpel_variance8x16_mmx = vp8_sub_pixel_variance8x16_mmx;
const SubpixVarMxNFunc subpel_variance8x8_mmx = vp8_sub_pixel_variance8x8_mmx;
const SubpixVarMxNFunc subpel_variance4x4_mmx = vp8_sub_pixel_variance4x4_mmx;
INSTANTIATE_TEST_CASE_P(
    MMX, VP8SubpelVarianceTest,
    ::testing::Values(make_tuple(4, 4, subpel_variance16x16_mmx, 0),
                      make_tuple(4, 3, subpel_variance16x8_mmx, 0),
                      make_tuple(3, 4, subpel_variance8x16_mmx, 0),
                      make_tuple(3, 3, subpel_variance8x8_mmx, 0),
                      make_tuple(2, 2, subpel_variance4x4_mmx, 0)));
#endif  // HAVE_MMX
#endif  // CONFIG_VP8

#if CONFIG_VP9_ENCODER
#if HAVE_SSE2
#if CONFIG_USE_X86INC
const SubpixVarMxNFunc subpel_variance4x4_sse = vp9_sub_pixel_variance4x4_sse;
const SubpixVarMxNFunc subpel_variance4x8_sse = vp9_sub_pixel_variance4x8_sse;
const SubpixVarMxNFunc subpel_variance8x4_sse2 = vp9_sub_pixel_variance8x4_sse2;
const SubpixVarMxNFunc subpel_variance8x8_sse2 = vp9_sub_pixel_variance8x8_sse2;
const SubpixVarMxNFunc subpel_variance8x16_sse2 =
    vp9_sub_pixel_variance8x16_sse2;
const SubpixVarMxNFunc subpel_variance16x8_sse2 =
    vp9_sub_pixel_variance16x8_sse2;
const SubpixVarMxNFunc subpel_variance16x16_sse2 =
    vp9_sub_pixel_variance16x16_sse2;
const SubpixVarMxNFunc subpel_variance16x32_sse2 =
    vp9_sub_pixel_variance16x32_sse2;
const SubpixVarMxNFunc subpel_variance32x16_sse2 =
    vp9_sub_pixel_variance32x16_sse2;
const SubpixVarMxNFunc subpel_variance32x32_sse2 =
    vp9_sub_pixel_variance32x32_sse2;
const SubpixVarMxNFunc subpel_variance32x64_sse2 =
    vp9_sub_pixel_variance32x64_sse2;
const SubpixVarMxNFunc subpel_variance64x32_sse2 =
    vp9_sub_pixel_variance64x32_sse2;
const SubpixVarMxNFunc subpel_variance64x64_sse2 =
    vp9_sub_pixel_variance64x64_sse2;
INSTANTIATE_TEST_CASE_P(
    SSE2, VP9SubpelVarianceTest,
    ::testing::Values(make_tuple(2, 2, subpel_variance4x4_sse, 0),
                      make_tuple(2, 3, subpel_variance4x8_sse, 0),
                      make_tuple(3, 2, subpel_variance8x4_sse2, 0),
                      make_tuple(3, 3, subpel_variance8x8_sse2, 0),
                      make_tuple(3, 4, subpel_variance8x16_sse2, 0),
                      make_tuple(4, 3, subpel_variance16x8_sse2, 0),
                      make_tuple(4, 4, subpel_variance16x16_sse2, 0),
                      make_tuple(4, 5, subpel_variance16x32_sse2, 0),
                      make_tuple(5, 4, subpel_variance32x16_sse2, 0),
                      make_tuple(5, 5, subpel_variance32x32_sse2, 0),
                      make_tuple(5, 6, subpel_variance32x64_sse2, 0),
                      make_tuple(6, 5, subpel_variance64x32_sse2, 0),
                      make_tuple(6, 6, subpel_variance64x64_sse2, 0)));
const vp9_subp_avg_variance_fn_t subpel_avg_variance4x4_sse =
    vp9_sub_pixel_avg_variance4x4_sse;
const vp9_subp_avg_variance_fn_t subpel_avg_variance4x8_sse =
    vp9_sub_pixel_avg_variance4x8_sse;
const vp9_subp_avg_variance_fn_t subpel_avg_variance8x4_sse2 =
    vp9_sub_pixel_avg_variance8x4_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance8x8_sse2 =
    vp9_sub_pixel_avg_variance8x8_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance8x16_sse2 =
    vp9_sub_pixel_avg_variance8x16_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance16x8_sse2 =
    vp9_sub_pixel_avg_variance16x8_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance16x16_sse2 =
    vp9_sub_pixel_avg_variance16x16_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance16x32_sse2 =
    vp9_sub_pixel_avg_variance16x32_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance32x16_sse2 =
    vp9_sub_pixel_avg_variance32x16_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance32x32_sse2 =
    vp9_sub_pixel_avg_variance32x32_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance32x64_sse2 =
    vp9_sub_pixel_avg_variance32x64_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance64x32_sse2 =
    vp9_sub_pixel_avg_variance64x32_sse2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance64x64_sse2 =
    vp9_sub_pixel_avg_variance64x64_sse2;
INSTANTIATE_TEST_CASE_P(
    SSE2, VP9SubpelAvgVarianceTest,
    ::testing::Values(make_tuple(2, 2, subpel_avg_variance4x4_sse, 0),
                      make_tuple(2, 3, subpel_avg_variance4x8_sse, 0),
                      make_tuple(3, 2, subpel_avg_variance8x4_sse2, 0),
                      make_tuple(3, 3, subpel_avg_variance8x8_sse2, 0),
                      make_tuple(3, 4, subpel_avg_variance8x16_sse2, 0),
                      make_tuple(4, 3, subpel_avg_variance16x8_sse2, 0),
                      make_tuple(4, 4, subpel_avg_variance16x16_sse2, 0),
                      make_tuple(4, 5, subpel_avg_variance16x32_sse2, 0),
                      make_tuple(5, 4, subpel_avg_variance32x16_sse2, 0),
                      make_tuple(5, 5, subpel_avg_variance32x32_sse2, 0),
                      make_tuple(5, 6, subpel_avg_variance32x64_sse2, 0),
                      make_tuple(6, 5, subpel_avg_variance64x32_sse2, 0),
                      make_tuple(6, 6, subpel_avg_variance64x64_sse2, 0)));
#if CONFIG_VP9_HIGHBITDEPTH
const SubpixVarMxNFunc highbd_subpel_variance8x4_sse2 =
    vp9_highbd_sub_pixel_variance8x4_sse2;
const SubpixVarMxNFunc highbd_subpel_variance8x8_sse2 =
    vp9_highbd_sub_pixel_variance8x8_sse2;
const SubpixVarMxNFunc highbd_subpel_variance8x16_sse2 =
    vp9_highbd_sub_pixel_variance8x16_sse2;
const SubpixVarMxNFunc highbd_subpel_variance16x8_sse2 =
    vp9_highbd_sub_pixel_variance16x8_sse2;
const SubpixVarMxNFunc highbd_subpel_variance16x16_sse2 =
    vp9_highbd_sub_pixel_variance16x16_sse2;
const SubpixVarMxNFunc highbd_subpel_variance16x32_sse2 =
    vp9_highbd_sub_pixel_variance16x32_sse2;
const SubpixVarMxNFunc highbd_subpel_variance32x16_sse2 =
    vp9_highbd_sub_pixel_variance32x16_sse2;
const SubpixVarMxNFunc highbd_subpel_variance32x32_sse2 =
    vp9_highbd_sub_pixel_variance32x32_sse2;
const SubpixVarMxNFunc highbd_subpel_variance32x64_sse2 =
    vp9_highbd_sub_pixel_variance32x64_sse2;
const SubpixVarMxNFunc highbd_subpel_variance64x32_sse2 =
    vp9_highbd_sub_pixel_variance64x32_sse2;
const SubpixVarMxNFunc highbd_subpel_variance64x64_sse2 =
    vp9_highbd_sub_pixel_variance64x64_sse2;
const SubpixVarMxNFunc highbd_10_subpel_variance8x4_sse2 =
    vp9_highbd_10_sub_pixel_variance8x4_sse2;
const SubpixVarMxNFunc highbd_10_subpel_variance8x8_sse2 =
    vp9_highbd_10_sub_pixel_variance8x8_sse2;
const SubpixVarMxNFunc highbd_10_subpel_variance8x16_sse2 =
    vp9_highbd_10_sub_pixel_variance8x16_sse2;
const SubpixVarMxNFunc highbd_10_subpel_variance16x8_sse2 =
    vp9_highbd_10_sub_pixel_variance16x8_sse2;
const SubpixVarMxNFunc highbd_10_subpel_variance16x16_sse2 =
    vp9_highbd_10_sub_pixel_variance16x16_sse2;
const SubpixVarMxNFunc highbd_10_subpel_variance16x32_sse2 =
    vp9_highbd_10_sub_pixel_variance16x32_sse2;
const SubpixVarMxNFunc highbd_10_subpel_variance32x16_sse2 =
    vp9_highbd_10_sub_pixel_variance32x16_sse2;
const SubpixVarMxNFunc highbd_10_subpel_variance32x32_sse2 =
    vp9_highbd_10_sub_pixel_variance32x32_sse2;
const SubpixVarMxNFunc highbd_10_subpel_variance32x64_sse2 =
    vp9_highbd_10_sub_pixel_variance32x64_sse2;
const SubpixVarMxNFunc highbd_10_subpel_variance64x32_sse2 =
    vp9_highbd_10_sub_pixel_variance64x32_sse2;
const SubpixVarMxNFunc highbd_10_subpel_variance64x64_sse2 =
    vp9_highbd_10_sub_pixel_variance64x64_sse2;
const SubpixVarMxNFunc highbd_12_subpel_variance8x4_sse2 =
    vp9_highbd_12_sub_pixel_variance8x4_sse2;
const SubpixVarMxNFunc highbd_12_subpel_variance8x8_sse2 =
    vp9_highbd_12_sub_pixel_variance8x8_sse2;
const SubpixVarMxNFunc highbd_12_subpel_variance8x16_sse2 =
    vp9_highbd_12_sub_pixel_variance8x16_sse2;
const SubpixVarMxNFunc highbd_12_subpel_variance16x8_sse2 =
    vp9_highbd_12_sub_pixel_variance16x8_sse2;
const SubpixVarMxNFunc highbd_12_subpel_variance16x16_sse2 =
    vp9_highbd_12_sub_pixel_variance16x16_sse2;
const SubpixVarMxNFunc highbd_12_subpel_variance16x32_sse2 =
    vp9_highbd_12_sub_pixel_variance16x32_sse2;
const SubpixVarMxNFunc highbd_12_subpel_variance32x16_sse2 =
    vp9_highbd_12_sub_pixel_variance32x16_sse2;
const SubpixVarMxNFunc highbd_12_subpel_variance32x32_sse2 =
    vp9_highbd_12_sub_pixel_variance32x32_sse2;
const SubpixVarMxNFunc highbd_12_subpel_variance32x64_sse2 =
    vp9_highbd_12_sub_pixel_variance32x64_sse2;
const SubpixVarMxNFunc highbd_12_subpel_variance64x32_sse2 =
    vp9_highbd_12_sub_pixel_variance64x32_sse2;
const SubpixVarMxNFunc highbd_12_subpel_variance64x64_sse2 =
    vp9_highbd_12_sub_pixel_variance64x64_sse2;
INSTANTIATE_TEST_CASE_P(
    SSE2, VP9SubpelVarianceHighTest,
    ::testing::Values(make_tuple(3, 2, highbd_10_subpel_variance8x4_sse2, 10),
                      make_tuple(3, 3, highbd_10_subpel_variance8x8_sse2, 10),
                      make_tuple(3, 4, highbd_10_subpel_variance8x16_sse2, 10),
                      make_tuple(4, 3, highbd_10_subpel_variance16x8_sse2, 10),
                      make_tuple(4, 4, highbd_10_subpel_variance16x16_sse2, 10),
                      make_tuple(4, 5, highbd_10_subpel_variance16x32_sse2, 10),
                      make_tuple(5, 4, highbd_10_subpel_variance32x16_sse2, 10),
                      make_tuple(5, 5, highbd_10_subpel_variance32x32_sse2, 10),
                      make_tuple(5, 6, highbd_10_subpel_variance32x64_sse2, 10),
                      make_tuple(6, 5, highbd_10_subpel_variance64x32_sse2, 10),
                      make_tuple(6, 6, highbd_10_subpel_variance64x64_sse2, 10),
                      make_tuple(3, 2, highbd_12_subpel_variance8x4_sse2, 12),
                      make_tuple(3, 3, highbd_12_subpel_variance8x8_sse2, 12),
                      make_tuple(3, 4, highbd_12_subpel_variance8x16_sse2, 12),
                      make_tuple(4, 3, highbd_12_subpel_variance16x8_sse2, 12),
                      make_tuple(4, 4, highbd_12_subpel_variance16x16_sse2, 12),
                      make_tuple(4, 5, highbd_12_subpel_variance16x32_sse2, 12),
                      make_tuple(5, 4, highbd_12_subpel_variance32x16_sse2, 12),
                      make_tuple(5, 5, highbd_12_subpel_variance32x32_sse2, 12),
                      make_tuple(5, 6, highbd_12_subpel_variance32x64_sse2, 12),
                      make_tuple(6, 5, highbd_12_subpel_variance64x32_sse2, 12),
                      make_tuple(6, 6, highbd_12_subpel_variance64x64_sse2, 12),
                      make_tuple(3, 2, highbd_subpel_variance8x4_sse2, 8),
                      make_tuple(3, 3, highbd_subpel_variance8x8_sse2, 8),
                      make_tuple(3, 4, highbd_subpel_variance8x16_sse2, 8),
                      make_tuple(4, 3, highbd_subpel_variance16x8_sse2, 8),
                      make_tuple(4, 4, highbd_subpel_variance16x16_sse2, 8),
                      make_tuple(4, 5, highbd_subpel_variance16x32_sse2, 8),
                      make_tuple(5, 4, highbd_subpel_variance32x16_sse2, 8),
                      make_tuple(5, 5, highbd_subpel_variance32x32_sse2, 8),
                      make_tuple(5, 6, highbd_subpel_variance32x64_sse2, 8),
                      make_tuple(6, 5, highbd_subpel_variance64x32_sse2, 8),
                      make_tuple(6, 6, highbd_subpel_variance64x64_sse2, 8)));
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance8x4_sse2 =
    vp9_highbd_sub_pixel_avg_variance8x4_sse2;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance8x8_sse2 =
    vp9_highbd_sub_pixel_avg_variance8x8_sse2;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance8x16_sse2 =
    vp9_highbd_sub_pixel_avg_variance8x16_sse2;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance16x8_sse2 =
    vp9_highbd_sub_pixel_avg_variance16x8_sse2;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance16x16_sse2 =
    vp9_highbd_sub_pixel_avg_variance16x16_sse2;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance16x32_sse2 =
    vp9_highbd_sub_pixel_avg_variance16x32_sse2;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance32x16_sse2 =
    vp9_highbd_sub_pixel_avg_variance32x16_sse2;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance32x32_sse2 =
    vp9_highbd_sub_pixel_avg_variance32x32_sse2;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance32x64_sse2 =
    vp9_highbd_sub_pixel_avg_variance32x64_sse2;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance64x32_sse2 =
    vp9_highbd_sub_pixel_avg_variance64x32_sse2;
const vp9_subp_avg_variance_fn_t highbd_subpel_avg_variance64x64_sse2 =
    vp9_highbd_sub_pixel_avg_variance64x64_sse2;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance8x4_sse2 =
    vp9_highbd_10_sub_pixel_avg_variance8x4_sse2;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance8x8_sse2 =
    vp9_highbd_10_sub_pixel_avg_variance8x8_sse2;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance8x16_sse2 =
    vp9_highbd_10_sub_pixel_avg_variance8x16_sse2;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance16x8_sse2 =
    vp9_highbd_10_sub_pixel_avg_variance16x8_sse2;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance16x16_sse2 =
    vp9_highbd_10_sub_pixel_avg_variance16x16_sse2;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance16x32_sse2 =
    vp9_highbd_10_sub_pixel_avg_variance16x32_sse2;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance32x16_sse2 =
    vp9_highbd_10_sub_pixel_avg_variance32x16_sse2;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance32x32_sse2 =
    vp9_highbd_10_sub_pixel_avg_variance32x32_sse2;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance32x64_sse2 =
    vp9_highbd_10_sub_pixel_avg_variance32x64_sse2;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance64x32_sse2 =
    vp9_highbd_10_sub_pixel_avg_variance64x32_sse2;
const vp9_subp_avg_variance_fn_t highbd_10_subpel_avg_variance64x64_sse2 =
    vp9_highbd_10_sub_pixel_avg_variance64x64_sse2;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance8x4_sse2 =
    vp9_highbd_12_sub_pixel_avg_variance8x4_sse2;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance8x8_sse2 =
    vp9_highbd_12_sub_pixel_avg_variance8x8_sse2;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance8x16_sse2 =
    vp9_highbd_12_sub_pixel_avg_variance8x16_sse2;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance16x8_sse2 =
    vp9_highbd_12_sub_pixel_avg_variance16x8_sse2;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance16x16_sse2 =
    vp9_highbd_12_sub_pixel_avg_variance16x16_sse2;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance16x32_sse2 =
    vp9_highbd_12_sub_pixel_avg_variance16x32_sse2;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance32x16_sse2 =
    vp9_highbd_12_sub_pixel_avg_variance32x16_sse2;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance32x32_sse2 =
    vp9_highbd_12_sub_pixel_avg_variance32x32_sse2;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance32x64_sse2 =
    vp9_highbd_12_sub_pixel_avg_variance32x64_sse2;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance64x32_sse2 =
    vp9_highbd_12_sub_pixel_avg_variance64x32_sse2;
const vp9_subp_avg_variance_fn_t highbd_12_subpel_avg_variance64x64_sse2 =
    vp9_highbd_12_sub_pixel_avg_variance64x64_sse2;
INSTANTIATE_TEST_CASE_P(
    SSE2, VP9SubpelAvgVarianceHighTest,
    ::testing::Values(
                  make_tuple(3, 2, highbd_10_subpel_avg_variance8x4_sse2, 10),
                  make_tuple(3, 3, highbd_10_subpel_avg_variance8x8_sse2, 10),
                  make_tuple(3, 4, highbd_10_subpel_avg_variance8x16_sse2, 10),
                  make_tuple(4, 3, highbd_10_subpel_avg_variance16x8_sse2, 10),
                  make_tuple(4, 4, highbd_10_subpel_avg_variance16x16_sse2, 10),
                  make_tuple(4, 5, highbd_10_subpel_avg_variance16x32_sse2, 10),
                  make_tuple(5, 4, highbd_10_subpel_avg_variance32x16_sse2, 10),
                  make_tuple(5, 5, highbd_10_subpel_avg_variance32x32_sse2, 10),
                  make_tuple(5, 6, highbd_10_subpel_avg_variance32x64_sse2, 10),
                  make_tuple(6, 5, highbd_10_subpel_avg_variance64x32_sse2, 10),
                  make_tuple(6, 6, highbd_10_subpel_avg_variance64x64_sse2, 10),
                  make_tuple(3, 2, highbd_12_subpel_avg_variance8x4_sse2, 12),
                  make_tuple(3, 3, highbd_12_subpel_avg_variance8x8_sse2, 12),
                  make_tuple(3, 4, highbd_12_subpel_avg_variance8x16_sse2, 12),
                  make_tuple(4, 3, highbd_12_subpel_avg_variance16x8_sse2, 12),
                  make_tuple(4, 4, highbd_12_subpel_avg_variance16x16_sse2, 12),
                  make_tuple(4, 5, highbd_12_subpel_avg_variance16x32_sse2, 12),
                  make_tuple(5, 4, highbd_12_subpel_avg_variance32x16_sse2, 12),
                  make_tuple(5, 5, highbd_12_subpel_avg_variance32x32_sse2, 12),
                  make_tuple(5, 6, highbd_12_subpel_avg_variance32x64_sse2, 12),
                  make_tuple(6, 5, highbd_12_subpel_avg_variance64x32_sse2, 12),
                  make_tuple(6, 6, highbd_12_subpel_avg_variance64x64_sse2, 12),
                  make_tuple(3, 2, highbd_subpel_avg_variance8x4_sse2, 8),
                  make_tuple(3, 3, highbd_subpel_avg_variance8x8_sse2, 8),
                  make_tuple(3, 4, highbd_subpel_avg_variance8x16_sse2, 8),
                  make_tuple(4, 3, highbd_subpel_avg_variance16x8_sse2, 8),
                  make_tuple(4, 4, highbd_subpel_avg_variance16x16_sse2, 8),
                  make_tuple(4, 5, highbd_subpel_avg_variance16x32_sse2, 8),
                  make_tuple(5, 4, highbd_subpel_avg_variance32x16_sse2, 8),
                  make_tuple(5, 5, highbd_subpel_avg_variance32x32_sse2, 8),
                  make_tuple(5, 6, highbd_subpel_avg_variance32x64_sse2, 8),
                  make_tuple(6, 5, highbd_subpel_avg_variance64x32_sse2, 8),
                  make_tuple(6, 6, highbd_subpel_avg_variance64x64_sse2, 8)));
#endif  // CONFIG_VP9_HIGHBITDEPTH
#endif  // CONFIG_USE_X86INC
#endif  // HAVE_SSE2
#endif  // CONFIG_VP9_ENCODER

#if CONFIG_VP8
#if HAVE_SSE2
const SubpixVarMxNFunc vp8_subpel_variance16x16_sse2 =
    vp8_sub_pixel_variance16x16_wmt;
const SubpixVarMxNFunc vp8_subpel_variance16x8_sse2 =
    vp8_sub_pixel_variance16x8_wmt;
const SubpixVarMxNFunc vp8_subpel_variance8x16_sse2 =
    vp8_sub_pixel_variance8x16_wmt;
const SubpixVarMxNFunc vp8_subpel_variance8x8_sse2 =
    vp8_sub_pixel_variance8x8_wmt;
const SubpixVarMxNFunc vp8_subpel_variance4x4_sse2 =
    vp8_sub_pixel_variance4x4_wmt;
INSTANTIATE_TEST_CASE_P(
    SSE2, VP8SubpelVarianceTest,
    ::testing::Values(make_tuple(2, 2, vp8_subpel_variance4x4_sse2, 0),
                      make_tuple(3, 3, vp8_subpel_variance8x8_sse2, 0),
                      make_tuple(3, 4, vp8_subpel_variance8x16_sse2, 0),
                      make_tuple(4, 3, vp8_subpel_variance16x8_sse2, 0),
                      make_tuple(4, 4, vp8_subpel_variance16x16_sse2, 0)));
#endif  // HAVE_SSE2
#endif  // CONFIG_VP8

#if CONFIG_VP9_ENCODER
#if HAVE_SSSE3
#if CONFIG_USE_X86INC
const SubpixVarMxNFunc subpel_variance4x4_ssse3 =
    vp9_sub_pixel_variance4x4_ssse3;
const SubpixVarMxNFunc subpel_variance4x8_ssse3 =
    vp9_sub_pixel_variance4x8_ssse3;
const SubpixVarMxNFunc subpel_variance8x4_ssse3 =
    vp9_sub_pixel_variance8x4_ssse3;
const SubpixVarMxNFunc subpel_variance8x8_ssse3 =
    vp9_sub_pixel_variance8x8_ssse3;
const SubpixVarMxNFunc subpel_variance8x16_ssse3 =
    vp9_sub_pixel_variance8x16_ssse3;
const SubpixVarMxNFunc subpel_variance16x8_ssse3 =
    vp9_sub_pixel_variance16x8_ssse3;
const SubpixVarMxNFunc subpel_variance16x16_ssse3 =
    vp9_sub_pixel_variance16x16_ssse3;
const SubpixVarMxNFunc subpel_variance16x32_ssse3 =
    vp9_sub_pixel_variance16x32_ssse3;
const SubpixVarMxNFunc subpel_variance32x16_ssse3 =
    vp9_sub_pixel_variance32x16_ssse3;
const SubpixVarMxNFunc subpel_variance32x32_ssse3 =
    vp9_sub_pixel_variance32x32_ssse3;
const SubpixVarMxNFunc subpel_variance32x64_ssse3 =
    vp9_sub_pixel_variance32x64_ssse3;
const SubpixVarMxNFunc subpel_variance64x32_ssse3 =
    vp9_sub_pixel_variance64x32_ssse3;
const SubpixVarMxNFunc subpel_variance64x64_ssse3 =
    vp9_sub_pixel_variance64x64_ssse3;
INSTANTIATE_TEST_CASE_P(
    SSSE3, VP9SubpelVarianceTest,
    ::testing::Values(make_tuple(2, 2, subpel_variance4x4_ssse3, 0),
                      make_tuple(2, 3, subpel_variance4x8_ssse3, 0),
                      make_tuple(3, 2, subpel_variance8x4_ssse3, 0),
                      make_tuple(3, 3, subpel_variance8x8_ssse3, 0),
                      make_tuple(3, 4, subpel_variance8x16_ssse3, 0),
                      make_tuple(4, 3, subpel_variance16x8_ssse3, 0),
                      make_tuple(4, 4, subpel_variance16x16_ssse3, 0),
                      make_tuple(4, 5, subpel_variance16x32_ssse3, 0),
                      make_tuple(5, 4, subpel_variance32x16_ssse3, 0),
                      make_tuple(5, 5, subpel_variance32x32_ssse3, 0),
                      make_tuple(5, 6, subpel_variance32x64_ssse3, 0),
                      make_tuple(6, 5, subpel_variance64x32_ssse3, 0),
                      make_tuple(6, 6, subpel_variance64x64_ssse3, 0)));
const vp9_subp_avg_variance_fn_t subpel_avg_variance4x4_ssse3 =
    vp9_sub_pixel_avg_variance4x4_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance4x8_ssse3 =
    vp9_sub_pixel_avg_variance4x8_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance8x4_ssse3 =
    vp9_sub_pixel_avg_variance8x4_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance8x8_ssse3 =
    vp9_sub_pixel_avg_variance8x8_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance8x16_ssse3 =
    vp9_sub_pixel_avg_variance8x16_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance16x8_ssse3 =
    vp9_sub_pixel_avg_variance16x8_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance16x16_ssse3 =
    vp9_sub_pixel_avg_variance16x16_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance16x32_ssse3 =
    vp9_sub_pixel_avg_variance16x32_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance32x16_ssse3 =
    vp9_sub_pixel_avg_variance32x16_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance32x32_ssse3 =
    vp9_sub_pixel_avg_variance32x32_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance32x64_ssse3 =
    vp9_sub_pixel_avg_variance32x64_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance64x32_ssse3 =
    vp9_sub_pixel_avg_variance64x32_ssse3;
const vp9_subp_avg_variance_fn_t subpel_avg_variance64x64_ssse3 =
    vp9_sub_pixel_avg_variance64x64_ssse3;
INSTANTIATE_TEST_CASE_P(
    SSSE3, VP9SubpelAvgVarianceTest,
    ::testing::Values(make_tuple(2, 2, subpel_avg_variance4x4_ssse3, 0),
                      make_tuple(2, 3, subpel_avg_variance4x8_ssse3, 0),
                      make_tuple(3, 2, subpel_avg_variance8x4_ssse3, 0),
                      make_tuple(3, 3, subpel_avg_variance8x8_ssse3, 0),
                      make_tuple(3, 4, subpel_avg_variance8x16_ssse3, 0),
                      make_tuple(4, 3, subpel_avg_variance16x8_ssse3, 0),
                      make_tuple(4, 4, subpel_avg_variance16x16_ssse3, 0),
                      make_tuple(4, 5, subpel_avg_variance16x32_ssse3, 0),
                      make_tuple(5, 4, subpel_avg_variance32x16_ssse3, 0),
                      make_tuple(5, 5, subpel_avg_variance32x32_ssse3, 0),
                      make_tuple(5, 6, subpel_avg_variance32x64_ssse3, 0),
                      make_tuple(6, 5, subpel_avg_variance64x32_ssse3, 0),
                      make_tuple(6, 6, subpel_avg_variance64x64_ssse3, 0)));
#endif  // CONFIG_USE_X86INC
#endif  // HAVE_SSSE3
#endif  // CONFIG_VP9_ENCODER

#if CONFIG_VP8
#if HAVE_SSSE3
const SubpixVarMxNFunc vp8_subpel_variance16x16_ssse3 =
    vp8_sub_pixel_variance16x16_ssse3;
const SubpixVarMxNFunc vp8_subpel_variance16x8_ssse3 =
    vp8_sub_pixel_variance16x8_ssse3;
INSTANTIATE_TEST_CASE_P(
    SSSE3, VP8SubpelVarianceTest,
    ::testing::Values(make_tuple(4, 3, vp8_subpel_variance16x8_ssse3, 0),
                      make_tuple(4, 4, vp8_subpel_variance16x16_ssse3, 0)));
#endif  // HAVE_SSSE3
#endif  // CONFIG_VP8

#if HAVE_AVX2
const VarianceMxNFunc mse16x16_avx2 = vpx_mse16x16_avx2;
INSTANTIATE_TEST_CASE_P(AVX2, VpxMseTest,
                        ::testing::Values(make_tuple(4, 4, mse16x16_avx2)));

const VarianceMxNFunc variance64x64_avx2 = vpx_variance64x64_avx2;
const VarianceMxNFunc variance64x32_avx2 = vpx_variance64x32_avx2;
const VarianceMxNFunc variance32x32_avx2 = vpx_variance32x32_avx2;
const VarianceMxNFunc variance32x16_avx2 = vpx_variance32x16_avx2;
const VarianceMxNFunc variance16x16_avx2 = vpx_variance16x16_avx2;
INSTANTIATE_TEST_CASE_P(
    AVX2, VpxVarianceTest,
    ::testing::Values(make_tuple(6, 6, variance64x64_avx2, 0),
                      make_tuple(6, 5, variance64x32_avx2, 0),
                      make_tuple(5, 5, variance32x32_avx2, 0),
                      make_tuple(5, 4, variance32x16_avx2, 0),
                      make_tuple(4, 4, variance16x16_avx2, 0)));

#if CONFIG_VP9_ENCODER
const SubpixVarMxNFunc subpel_variance32x32_avx2 =
    vp9_sub_pixel_variance32x32_avx2;
const SubpixVarMxNFunc subpel_variance64x64_avx2 =
    vp9_sub_pixel_variance64x64_avx2;
INSTANTIATE_TEST_CASE_P(
    AVX2, VP9SubpelVarianceTest,
    ::testing::Values(make_tuple(5, 5, subpel_variance32x32_avx2, 0),
                      make_tuple(6, 6, subpel_variance64x64_avx2, 0)));

const vp9_subp_avg_variance_fn_t subpel_avg_variance32x32_avx2 =
    vp9_sub_pixel_avg_variance32x32_avx2;
const vp9_subp_avg_variance_fn_t subpel_avg_variance64x64_avx2 =
    vp9_sub_pixel_avg_variance64x64_avx2;
INSTANTIATE_TEST_CASE_P(
    AVX2, VP9SubpelAvgVarianceTest,
    ::testing::Values(make_tuple(5, 5, subpel_avg_variance32x32_avx2, 0),
                      make_tuple(6, 6, subpel_avg_variance64x64_avx2, 0)));
#endif  // CONFIG_VP9_ENCODER
#endif  // HAVE_AVX2

#if CONFIG_VP8
#if HAVE_MEDIA
const SubpixVarMxNFunc subpel_variance16x16_media =
    vp8_sub_pixel_variance16x16_armv6;
const SubpixVarMxNFunc subpel_variance8x8_media =
    vp8_sub_pixel_variance8x8_armv6;
INSTANTIATE_TEST_CASE_P(
    MEDIA, VP8SubpelVarianceTest,
    ::testing::Values(make_tuple(3, 3, subpel_variance8x8_media, 0),
                      make_tuple(4, 4, subpel_variance16x16_media, 0)));
#endif  // HAVE_MEDIA
#endif  // CONFIG_VP8

#if HAVE_NEON
const Get4x4SseFunc get4x4sse_cs_neon = vpx_get4x4sse_cs_neon;
INSTANTIATE_TEST_CASE_P(NEON, VpxSseTest,
                        ::testing::Values(make_tuple(2, 2, get4x4sse_cs_neon)));

const VarianceMxNFunc mse16x16_neon = vpx_mse16x16_neon;
INSTANTIATE_TEST_CASE_P(NEON, VpxMseTest,
                        ::testing::Values(make_tuple(4, 4, mse16x16_neon)));

const VarianceMxNFunc variance64x64_neon = vpx_variance64x64_neon;
const VarianceMxNFunc variance64x32_neon = vpx_variance64x32_neon;
const VarianceMxNFunc variance32x64_neon = vpx_variance32x64_neon;
const VarianceMxNFunc variance32x32_neon = vpx_variance32x32_neon;
const VarianceMxNFunc variance16x16_neon = vpx_variance16x16_neon;
const VarianceMxNFunc variance16x8_neon = vpx_variance16x8_neon;
const VarianceMxNFunc variance8x16_neon = vpx_variance8x16_neon;
const VarianceMxNFunc variance8x8_neon = vpx_variance8x8_neon;
INSTANTIATE_TEST_CASE_P(
    NEON, VpxVarianceTest,
    ::testing::Values(make_tuple(6, 6, variance64x64_neon, 0),
                      make_tuple(6, 5, variance64x32_neon, 0),
                      make_tuple(5, 6, variance32x64_neon, 0),
                      make_tuple(5, 5, variance32x32_neon, 0),
                      make_tuple(4, 4, variance16x16_neon, 0),
                      make_tuple(4, 3, variance16x8_neon, 0),
                      make_tuple(3, 4, variance8x16_neon, 0),
                      make_tuple(3, 3, variance8x8_neon, 0)));

#if CONFIG_VP8
#if HAVE_NEON_ASM
const SubpixVarMxNFunc vp8_subpel_variance16x16_neon =
    vp8_sub_pixel_variance16x16_neon;
INSTANTIATE_TEST_CASE_P(
    NEON, VP8SubpelVarianceTest,
    ::testing::Values(make_tuple(4, 4, vp8_subpel_variance16x16_neon, 0)));
#endif  // HAVE_NEON_ASM
#endif  // CONFIG_VP8

#if CONFIG_VP9_ENCODER
const SubpixVarMxNFunc subpel_variance8x8_neon = vp9_sub_pixel_variance8x8_neon;
const SubpixVarMxNFunc subpel_variance16x16_neon =
    vp9_sub_pixel_variance16x16_neon;
const SubpixVarMxNFunc subpel_variance32x32_neon =
    vp9_sub_pixel_variance32x32_neon;
const SubpixVarMxNFunc subpel_variance64x64_neon =
    vp9_sub_pixel_variance64x64_neon;
INSTANTIATE_TEST_CASE_P(
    NEON, VP9SubpelVarianceTest,
    ::testing::Values(make_tuple(3, 3, subpel_variance8x8_neon, 0),
                      make_tuple(4, 4, subpel_variance16x16_neon, 0),
                      make_tuple(5, 5, subpel_variance32x32_neon, 0),
                      make_tuple(6, 6, subpel_variance64x64_neon, 0)));
#endif  // CONFIG_VP9_ENCODER
#endif  // HAVE_NEON

#if HAVE_MEDIA
const VarianceMxNFunc mse16x16_media = vpx_mse16x16_media;
INSTANTIATE_TEST_CASE_P(MEDIA, VpxMseTest,
                        ::testing::Values(make_tuple(4, 4, mse16x16_media)));

const VarianceMxNFunc variance16x16_media = vpx_variance16x16_media;
const VarianceMxNFunc variance8x8_media = vpx_variance8x8_media;
INSTANTIATE_TEST_CASE_P(
    MEDIA, VpxVarianceTest,
    ::testing::Values(make_tuple(4, 4, variance16x16_media, 0),
                      make_tuple(3, 3, variance8x8_media, 0)));
#endif  // HAVE_MEDIA
}  // namespace
