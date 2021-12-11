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

#include <cstdlib>
#include <new>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "aom/aom_codec.h"
#include "aom/aom_integer.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/mem.h"

namespace {

typedef unsigned int (*VarianceMxNFunc)(const uint8_t *a, int a_stride,
                                        const uint8_t *b, int b_stride,
                                        unsigned int *sse);
typedef unsigned int (*SubpixVarMxNFunc)(const uint8_t *a, int a_stride,
                                         int xoffset, int yoffset,
                                         const uint8_t *b, int b_stride,
                                         unsigned int *sse);
typedef unsigned int (*SubpixAvgVarMxNFunc)(const uint8_t *a, int a_stride,
                                            int xoffset, int yoffset,
                                            const uint8_t *b, int b_stride,
                                            uint32_t *sse,
                                            const uint8_t *second_pred);
typedef unsigned int (*Get4x4SseFunc)(const uint8_t *a, int a_stride,
                                      const uint8_t *b, int b_stride);
typedef unsigned int (*SumOfSquaresFunction)(const int16_t *src);
typedef unsigned int (*JntSubpixAvgVarMxNFunc)(
    const uint8_t *a, int a_stride, int xoffset, int yoffset, const uint8_t *b,
    int b_stride, uint32_t *sse, const uint8_t *second_pred,
    const JNT_COMP_PARAMS *jcp_param);
typedef uint32_t (*ObmcSubpelVarFunc)(const uint8_t *pre, int pre_stride,
                                      int xoffset, int yoffset,
                                      const int32_t *wsrc, const int32_t *mask,
                                      unsigned int *sse);

using libaom_test::ACMRandom;

// Truncate high bit depth results by downshifting (with rounding) by:
// 2 * (bit_depth - 8) for sse
// (bit_depth - 8) for se
static void RoundHighBitDepth(int bit_depth, int64_t *se, uint64_t *sse) {
  switch (bit_depth) {
    case AOM_BITS_12:
      *sse = (*sse + 128) >> 8;
      *se = (*se + 8) >> 4;
      break;
    case AOM_BITS_10:
      *sse = (*sse + 8) >> 4;
      *se = (*se + 2) >> 2;
      break;
    case AOM_BITS_8:
    default: break;
  }
}

static unsigned int mb_ss_ref(const int16_t *src) {
  unsigned int res = 0;
  for (int i = 0; i < 256; ++i) {
    res += src[i] * src[i];
  }
  return res;
}

/* Note:
 *  Our codebase calculates the "diff" value in the variance algorithm by
 *  (src - ref).
 */
static uint32_t variance_ref(const uint8_t *src, const uint8_t *ref, int l2w,
                             int l2h, int src_stride, int ref_stride,
                             uint32_t *sse_ptr, bool use_high_bit_depth_,
                             aom_bit_depth_t bit_depth) {
  int64_t se = 0;
  uint64_t sse = 0;
  const int w = 1 << l2w;
  const int h = 1 << l2h;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int diff;
      if (!use_high_bit_depth_) {
        diff = src[y * src_stride + x] - ref[y * ref_stride + x];
        se += diff;
        sse += diff * diff;
      } else {
        diff = CONVERT_TO_SHORTPTR(src)[y * src_stride + x] -
               CONVERT_TO_SHORTPTR(ref)[y * ref_stride + x];
        se += diff;
        sse += diff * diff;
      }
    }
  }
  RoundHighBitDepth(bit_depth, &se, &sse);
  *sse_ptr = static_cast<uint32_t>(sse);
  return static_cast<uint32_t>(sse - ((se * se) >> (l2w + l2h)));
}

/* The subpel reference functions differ from the codec version in one aspect:
 * they calculate the bilinear factors directly instead of using a lookup table
 * and therefore upshift xoff and yoff by 1. Only every other calculated value
 * is used so the codec version shrinks the table to save space and maintain
 * compatibility with vp8.
 */
static uint32_t subpel_variance_ref(const uint8_t *ref, const uint8_t *src,
                                    int l2w, int l2h, int xoff, int yoff,
                                    uint32_t *sse_ptr, bool use_high_bit_depth_,
                                    aom_bit_depth_t bit_depth) {
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
      }
    }
  }
  RoundHighBitDepth(bit_depth, &se, &sse);
  *sse_ptr = static_cast<uint32_t>(sse);
  return static_cast<uint32_t>(sse - ((se * se) >> (l2w + l2h)));
}

static uint32_t subpel_avg_variance_ref(const uint8_t *ref, const uint8_t *src,
                                        const uint8_t *second_pred, int l2w,
                                        int l2h, int xoff, int yoff,
                                        uint32_t *sse_ptr,
                                        bool use_high_bit_depth,
                                        aom_bit_depth_t bit_depth) {
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
        const int diff =
            ((r + second_pred[w * y + x] + 1) >> 1) - src[w * y + x];
        se += diff;
        sse += diff * diff;
      } else {
        const uint16_t *ref16 = CONVERT_TO_SHORTPTR(ref);
        const uint16_t *src16 = CONVERT_TO_SHORTPTR(src);
        const uint16_t *sec16 = CONVERT_TO_SHORTPTR(second_pred);
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
      }
    }
  }
  RoundHighBitDepth(bit_depth, &se, &sse);
  *sse_ptr = static_cast<uint32_t>(sse);
  return static_cast<uint32_t>(sse - ((se * se) >> (l2w + l2h)));
}

static uint32_t jnt_subpel_avg_variance_ref(
    const uint8_t *ref, const uint8_t *src, const uint8_t *second_pred, int l2w,
    int l2h, int xoff, int yoff, uint32_t *sse_ptr, bool use_high_bit_depth,
    aom_bit_depth_t bit_depth, JNT_COMP_PARAMS *jcp_param) {
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
        const int a1 = ref[(w + 0) * (y + 0) + x + 0];
        const int a2 = ref[(w + 0) * (y + 0) + x + 1];
        const int b1 = ref[(w + 0) * (y + 1) + x + 0];
        const int b2 = ref[(w + 0) * (y + 1) + x + 1];
        const int a = a1 + (((a2 - a1) * xoff + 8) >> 4);
        const int b = b1 + (((b2 - b1) * xoff + 8) >> 4);
        const int r = a + (((b - a) * yoff + 8) >> 4);
        const int avg = ROUND_POWER_OF_TWO(
            r * jcp_param->fwd_offset +
                second_pred[w * y + x] * jcp_param->bck_offset,
            DIST_PRECISION_BITS);
        const int diff = avg - src[w * y + x];

        se += diff;
        sse += diff * diff;
      } else {
        const uint16_t *ref16 = CONVERT_TO_SHORTPTR(ref);
        const uint16_t *src16 = CONVERT_TO_SHORTPTR(src);
        const uint16_t *sec16 = CONVERT_TO_SHORTPTR(second_pred);
        const int a1 = ref16[(w + 0) * (y + 0) + x + 0];
        const int a2 = ref16[(w + 0) * (y + 0) + x + 1];
        const int b1 = ref16[(w + 0) * (y + 1) + x + 0];
        const int b2 = ref16[(w + 0) * (y + 1) + x + 1];
        const int a = a1 + (((a2 - a1) * xoff + 8) >> 4);
        const int b = b1 + (((b2 - b1) * xoff + 8) >> 4);
        const int r = a + (((b - a) * yoff + 8) >> 4);
        const int avg =
            ROUND_POWER_OF_TWO(r * jcp_param->fwd_offset +
                                   sec16[w * y + x] * jcp_param->bck_offset,
                               DIST_PRECISION_BITS);
        const int diff = avg - src16[w * y + x];

        se += diff;
        sse += diff * diff;
      }
    }
  }
  RoundHighBitDepth(bit_depth, &se, &sse);
  *sse_ptr = static_cast<uint32_t>(sse);
  return static_cast<uint32_t>(sse - ((se * se) >> (l2w + l2h)));
}

static uint32_t obmc_subpel_variance_ref(const uint8_t *pre, int l2w, int l2h,
                                         int xoff, int yoff,
                                         const int32_t *wsrc,
                                         const int32_t *mask, uint32_t *sse_ptr,
                                         bool use_high_bit_depth_,
                                         aom_bit_depth_t bit_depth) {
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
        const int a1 = pre[(w + 1) * (y + 0) + x + 0];
        const int a2 = pre[(w + 1) * (y + 0) + x + 1];
        const int b1 = pre[(w + 1) * (y + 1) + x + 0];
        const int b2 = pre[(w + 1) * (y + 1) + x + 1];
        const int a = a1 + (((a2 - a1) * xoff + 8) >> 4);
        const int b = b1 + (((b2 - b1) * xoff + 8) >> 4);
        const int r = a + (((b - a) * yoff + 8) >> 4);
        const int diff = ROUND_POWER_OF_TWO_SIGNED(
            wsrc[w * y + x] - r * mask[w * y + x], 12);
        se += diff;
        sse += diff * diff;
      } else {
        uint16_t *pre16 = CONVERT_TO_SHORTPTR(pre);
        const int a1 = pre16[(w + 1) * (y + 0) + x + 0];
        const int a2 = pre16[(w + 1) * (y + 0) + x + 1];
        const int b1 = pre16[(w + 1) * (y + 1) + x + 0];
        const int b2 = pre16[(w + 1) * (y + 1) + x + 1];
        const int a = a1 + (((a2 - a1) * xoff + 8) >> 4);
        const int b = b1 + (((b2 - b1) * xoff + 8) >> 4);
        const int r = a + (((b - a) * yoff + 8) >> 4);
        const int diff = ROUND_POWER_OF_TWO_SIGNED(
            wsrc[w * y + x] - r * mask[w * y + x], 12);
        se += diff;
        sse += diff * diff;
      }
    }
  }
  RoundHighBitDepth(bit_depth, &se, &sse);
  *sse_ptr = static_cast<uint32_t>(sse);
  return static_cast<uint32_t>(sse - ((se * se) >> (l2w + l2h)));
}

////////////////////////////////////////////////////////////////////////////////

class SumOfSquaresTest : public ::testing::TestWithParam<SumOfSquaresFunction> {
 public:
  SumOfSquaresTest() : func_(GetParam()) {}

  virtual ~SumOfSquaresTest() { libaom_test::ClearSystemState(); }

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

////////////////////////////////////////////////////////////////////////////////
// Encapsulating struct to store the function to test along with
// some testing context.
// Can be used for MSE, SSE, Variance, etc.

template <typename Func>
struct TestParams {
  TestParams(int log2w = 0, int log2h = 0, Func function = NULL,
             int bit_depth_value = 0)
      : log2width(log2w), log2height(log2h), func(function) {
    use_high_bit_depth = (bit_depth_value > 0);
    if (use_high_bit_depth) {
      bit_depth = static_cast<aom_bit_depth_t>(bit_depth_value);
    } else {
      bit_depth = AOM_BITS_8;
    }
    width = 1 << log2width;
    height = 1 << log2height;
    block_size = width * height;
    mask = (1u << bit_depth) - 1;
  }

  int log2width, log2height;
  int width, height;
  int block_size;
  Func func;
  aom_bit_depth_t bit_depth;
  bool use_high_bit_depth;
  uint32_t mask;
};

template <typename Func>
std::ostream &operator<<(std::ostream &os, const TestParams<Func> &p) {
  return os << "width/height:" << p.width << "/" << p.height
            << " function:" << reinterpret_cast<const void *>(p.func)
            << " bit-depth:" << p.bit_depth;
}

// Main class for testing a function type
template <typename FunctionType>
class MainTestClass
    : public ::testing::TestWithParam<TestParams<FunctionType> > {
 public:
  virtual void SetUp() {
    params_ = this->GetParam();

    rnd_.Reset(ACMRandom::DeterministicSeed());
    const size_t unit =
        use_high_bit_depth() ? sizeof(uint16_t) : sizeof(uint8_t);
    src_ = reinterpret_cast<uint8_t *>(aom_memalign(16, block_size() * unit));
    ref_ = new uint8_t[block_size() * unit];
    ASSERT_TRUE(src_ != NULL);
    ASSERT_TRUE(ref_ != NULL);
    if (use_high_bit_depth()) {
      // TODO(skal): remove!
      src_ = CONVERT_TO_BYTEPTR(src_);
      ref_ = CONVERT_TO_BYTEPTR(ref_);
    }
  }

  virtual void TearDown() {
    if (use_high_bit_depth()) {
      // TODO(skal): remove!
      src_ = reinterpret_cast<uint8_t *>(CONVERT_TO_SHORTPTR(src_));
      ref_ = reinterpret_cast<uint8_t *>(CONVERT_TO_SHORTPTR(ref_));
    }

    aom_free(src_);
    delete[] ref_;
    src_ = NULL;
    ref_ = NULL;
    libaom_test::ClearSystemState();
  }

 protected:
  // We could sub-class MainTestClass into dedicated class for Variance
  // and MSE/SSE, but it involves a lot of 'this->xxx' dereferencing
  // to access top class fields xxx. That's cumbersome, so for now we'll just
  // implement the testing methods here:

  // Variance tests
  void ZeroTest();
  void RefTest();
  void RefStrideTest();
  void OneQuarterTest();
  void SpeedTest();

  // MSE/SSE tests
  void RefTestMse();
  void RefTestSse();
  void MaxTestMse();
  void MaxTestSse();

 protected:
  ACMRandom rnd_;
  uint8_t *src_;
  uint8_t *ref_;
  TestParams<FunctionType> params_;

  // some relay helpers
  bool use_high_bit_depth() const { return params_.use_high_bit_depth; }
  int byte_shift() const { return params_.bit_depth - 8; }
  int block_size() const { return params_.block_size; }
  int width() const { return params_.width; }
  int height() const { return params_.height; }
  uint32_t mask() const { return params_.mask; }
};

////////////////////////////////////////////////////////////////////////////////
// Tests related to variance.

template <typename VarianceFunctionType>
void MainTestClass<VarianceFunctionType>::ZeroTest() {
  for (int i = 0; i <= 255; ++i) {
    if (!use_high_bit_depth()) {
      memset(src_, i, block_size());
    } else {
      uint16_t *const src16 = CONVERT_TO_SHORTPTR(src_);
      for (int k = 0; k < block_size(); ++k) src16[k] = i << byte_shift();
    }
    for (int j = 0; j <= 255; ++j) {
      if (!use_high_bit_depth()) {
        memset(ref_, j, block_size());
      } else {
        uint16_t *const ref16 = CONVERT_TO_SHORTPTR(ref_);
        for (int k = 0; k < block_size(); ++k) ref16[k] = j << byte_shift();
      }
      unsigned int sse, var;
      ASM_REGISTER_STATE_CHECK(
          var = params_.func(src_, width(), ref_, width(), &sse));
      EXPECT_EQ(0u, var) << "src values: " << i << " ref values: " << j;
    }
  }
}

template <typename VarianceFunctionType>
void MainTestClass<VarianceFunctionType>::RefTest() {
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < block_size(); j++) {
      if (!use_high_bit_depth()) {
        src_[j] = rnd_.Rand8();
        ref_[j] = rnd_.Rand8();
      } else {
        CONVERT_TO_SHORTPTR(src_)[j] = rnd_.Rand16() & mask();
        CONVERT_TO_SHORTPTR(ref_)[j] = rnd_.Rand16() & mask();
      }
    }
    unsigned int sse1, sse2, var1, var2;
    const int stride = width();
    ASM_REGISTER_STATE_CHECK(
        var1 = params_.func(src_, stride, ref_, stride, &sse1));
    var2 =
        variance_ref(src_, ref_, params_.log2width, params_.log2height, stride,
                     stride, &sse2, use_high_bit_depth(), params_.bit_depth);
    EXPECT_EQ(sse1, sse2) << "Error at test index: " << i;
    EXPECT_EQ(var1, var2) << "Error at test index: " << i;
  }
}

template <typename VarianceFunctionType>
void MainTestClass<VarianceFunctionType>::RefStrideTest() {
  for (int i = 0; i < 10; ++i) {
    const int ref_stride = (i & 1) * width();
    const int src_stride = ((i >> 1) & 1) * width();
    for (int j = 0; j < block_size(); j++) {
      const int ref_ind = (j / width()) * ref_stride + j % width();
      const int src_ind = (j / width()) * src_stride + j % width();
      if (!use_high_bit_depth()) {
        src_[src_ind] = rnd_.Rand8();
        ref_[ref_ind] = rnd_.Rand8();
      } else {
        CONVERT_TO_SHORTPTR(src_)[src_ind] = rnd_.Rand16() & mask();
        CONVERT_TO_SHORTPTR(ref_)[ref_ind] = rnd_.Rand16() & mask();
      }
    }
    unsigned int sse1, sse2;
    unsigned int var1, var2;

    ASM_REGISTER_STATE_CHECK(
        var1 = params_.func(src_, src_stride, ref_, ref_stride, &sse1));
    var2 = variance_ref(src_, ref_, params_.log2width, params_.log2height,
                        src_stride, ref_stride, &sse2, use_high_bit_depth(),
                        params_.bit_depth);
    EXPECT_EQ(sse1, sse2) << "Error at test index: " << i;
    EXPECT_EQ(var1, var2) << "Error at test index: " << i;
  }
}

template <typename VarianceFunctionType>
void MainTestClass<VarianceFunctionType>::OneQuarterTest() {
  const int half = block_size() / 2;
  if (!use_high_bit_depth()) {
    memset(src_, 255, block_size());
    memset(ref_, 255, half);
    memset(ref_ + half, 0, half);
  } else {
    aom_memset16(CONVERT_TO_SHORTPTR(src_), 255 << byte_shift(), block_size());
    aom_memset16(CONVERT_TO_SHORTPTR(ref_), 255 << byte_shift(), half);
    aom_memset16(CONVERT_TO_SHORTPTR(ref_) + half, 0, half);
  }
  unsigned int sse, var, expected;
  ASM_REGISTER_STATE_CHECK(
      var = params_.func(src_, width(), ref_, width(), &sse));
  expected = block_size() * 255 * 255 / 4;
  EXPECT_EQ(expected, var);
}

template <typename VarianceFunctionType>
void MainTestClass<VarianceFunctionType>::SpeedTest() {
  for (int j = 0; j < block_size(); j++) {
    if (!use_high_bit_depth()) {
      src_[j] = rnd_.Rand8();
      ref_[j] = rnd_.Rand8();
    } else {
      CONVERT_TO_SHORTPTR(src_)[j] = rnd_.Rand16() & mask();
      CONVERT_TO_SHORTPTR(ref_)[j] = rnd_.Rand16() & mask();
    }
  }
  unsigned int sse;
  const int stride = width();
  int run_time = 1000000000 / block_size();
  aom_usec_timer timer;
  aom_usec_timer_start(&timer);
  for (int i = 0; i < run_time; ++i) {
    params_.func(src_, stride, ref_, stride, &sse);
  }

  aom_usec_timer_mark(&timer);
  const double elapsed_time =
      static_cast<double>(aom_usec_timer_elapsed(&timer));
  printf("Variance %dx%d : %7.2fns\n", width(), height(), elapsed_time);
}

////////////////////////////////////////////////////////////////////////////////
// Tests related to MSE / SSE.

template <typename FunctionType>
void MainTestClass<FunctionType>::RefTestMse() {
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < block_size(); ++j) {
      src_[j] = rnd_.Rand8();
      ref_[j] = rnd_.Rand8();
    }
    unsigned int sse1, sse2;
    const int stride = width();
    ASM_REGISTER_STATE_CHECK(params_.func(src_, stride, ref_, stride, &sse1));
    variance_ref(src_, ref_, params_.log2width, params_.log2height, stride,
                 stride, &sse2, false, AOM_BITS_8);
    EXPECT_EQ(sse1, sse2);
  }
}

template <typename FunctionType>
void MainTestClass<FunctionType>::RefTestSse() {
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < block_size(); ++j) {
      src_[j] = rnd_.Rand8();
      ref_[j] = rnd_.Rand8();
    }
    unsigned int sse2;
    unsigned int var1;
    const int stride = width();
    ASM_REGISTER_STATE_CHECK(var1 = params_.func(src_, stride, ref_, stride));
    variance_ref(src_, ref_, params_.log2width, params_.log2height, stride,
                 stride, &sse2, false, AOM_BITS_8);
    EXPECT_EQ(var1, sse2);
  }
}

template <typename FunctionType>
void MainTestClass<FunctionType>::MaxTestMse() {
  memset(src_, 255, block_size());
  memset(ref_, 0, block_size());
  unsigned int sse;
  ASM_REGISTER_STATE_CHECK(params_.func(src_, width(), ref_, width(), &sse));
  const unsigned int expected = block_size() * 255 * 255;
  EXPECT_EQ(expected, sse);
}

template <typename FunctionType>
void MainTestClass<FunctionType>::MaxTestSse() {
  memset(src_, 255, block_size());
  memset(ref_, 0, block_size());
  unsigned int var;
  ASM_REGISTER_STATE_CHECK(var = params_.func(src_, width(), ref_, width()));
  const unsigned int expected = block_size() * 255 * 255;
  EXPECT_EQ(expected, var);
}

////////////////////////////////////////////////////////////////////////////////

using ::testing::get;
using ::testing::make_tuple;
using ::testing::tuple;

template <typename FunctionType>
class SubpelVarianceTest
    : public ::testing::TestWithParam<TestParams<FunctionType> > {
 public:
  virtual void SetUp() {
    params_ = this->GetParam();

    rnd_.Reset(ACMRandom::DeterministicSeed());
    if (!use_high_bit_depth()) {
      src_ = reinterpret_cast<uint8_t *>(aom_memalign(32, block_size()));
      sec_ = reinterpret_cast<uint8_t *>(aom_memalign(32, block_size()));
      ref_ = reinterpret_cast<uint8_t *>(
          aom_memalign(32, block_size() + width() + height() + 1));
    } else {
      src_ = CONVERT_TO_BYTEPTR(reinterpret_cast<uint16_t *>(
          aom_memalign(32, block_size() * sizeof(uint16_t))));
      sec_ = CONVERT_TO_BYTEPTR(reinterpret_cast<uint16_t *>(
          aom_memalign(32, block_size() * sizeof(uint16_t))));
      ref_ = CONVERT_TO_BYTEPTR(aom_memalign(
          32, (block_size() + width() + height() + 1) * sizeof(uint16_t)));
    }
    ASSERT_TRUE(src_ != NULL);
    ASSERT_TRUE(sec_ != NULL);
    ASSERT_TRUE(ref_ != NULL);
  }

  virtual void TearDown() {
    if (!use_high_bit_depth()) {
      aom_free(src_);
      aom_free(ref_);
      aom_free(sec_);
    } else {
      aom_free(CONVERT_TO_SHORTPTR(src_));
      aom_free(CONVERT_TO_SHORTPTR(ref_));
      aom_free(CONVERT_TO_SHORTPTR(sec_));
    }
    libaom_test::ClearSystemState();
  }

 protected:
  void RefTest();
  void ExtremeRefTest();

  ACMRandom rnd_;
  uint8_t *src_;
  uint8_t *ref_;
  uint8_t *sec_;
  TestParams<FunctionType> params_;
  JNT_COMP_PARAMS jcp_param_;

  // some relay helpers
  bool use_high_bit_depth() const { return params_.use_high_bit_depth; }
  int byte_shift() const { return params_.bit_depth - 8; }
  int block_size() const { return params_.block_size; }
  int width() const { return params_.width; }
  int height() const { return params_.height; }
  uint32_t mask() const { return params_.mask; }
};

template <typename SubpelVarianceFunctionType>
void SubpelVarianceTest<SubpelVarianceFunctionType>::RefTest() {
  for (int x = 0; x < 8; ++x) {
    for (int y = 0; y < 8; ++y) {
      if (!use_high_bit_depth()) {
        for (int j = 0; j < block_size(); j++) {
          src_[j] = rnd_.Rand8();
        }
        for (int j = 0; j < block_size() + width() + height() + 1; j++) {
          ref_[j] = rnd_.Rand8();
        }
      } else {
        for (int j = 0; j < block_size(); j++) {
          CONVERT_TO_SHORTPTR(src_)[j] = rnd_.Rand16() & mask();
        }
        for (int j = 0; j < block_size() + width() + height() + 1; j++) {
          CONVERT_TO_SHORTPTR(ref_)[j] = rnd_.Rand16() & mask();
        }
      }
      unsigned int sse1, sse2;
      unsigned int var1;
      ASM_REGISTER_STATE_CHECK(
          var1 = params_.func(ref_, width() + 1, x, y, src_, width(), &sse1));
      const unsigned int var2 = subpel_variance_ref(
          ref_, src_, params_.log2width, params_.log2height, x, y, &sse2,
          use_high_bit_depth(), params_.bit_depth);
      EXPECT_EQ(sse1, sse2) << "at position " << x << ", " << y;
      EXPECT_EQ(var1, var2) << "at position " << x << ", " << y;
    }
  }
}

template <typename SubpelVarianceFunctionType>
void SubpelVarianceTest<SubpelVarianceFunctionType>::ExtremeRefTest() {
  // Compare against reference.
  // Src: Set the first half of values to 0, the second half to the maximum.
  // Ref: Set the first half of values to the maximum, the second half to 0.
  for (int x = 0; x < 8; ++x) {
    for (int y = 0; y < 8; ++y) {
      const int half = block_size() / 2;
      if (!use_high_bit_depth()) {
        memset(src_, 0, half);
        memset(src_ + half, 255, half);
        memset(ref_, 255, half);
        memset(ref_ + half, 0, half + width() + height() + 1);
      } else {
        aom_memset16(CONVERT_TO_SHORTPTR(src_), mask(), half);
        aom_memset16(CONVERT_TO_SHORTPTR(src_) + half, 0, half);
        aom_memset16(CONVERT_TO_SHORTPTR(ref_), 0, half);
        aom_memset16(CONVERT_TO_SHORTPTR(ref_) + half, mask(),
                     half + width() + height() + 1);
      }
      unsigned int sse1, sse2;
      unsigned int var1;
      ASM_REGISTER_STATE_CHECK(
          var1 = params_.func(ref_, width() + 1, x, y, src_, width(), &sse1));
      const unsigned int var2 = subpel_variance_ref(
          ref_, src_, params_.log2width, params_.log2height, x, y, &sse2,
          use_high_bit_depth(), params_.bit_depth);
      EXPECT_EQ(sse1, sse2) << "for xoffset " << x << " and yoffset " << y;
      EXPECT_EQ(var1, var2) << "for xoffset " << x << " and yoffset " << y;
    }
  }
}

template <>
void SubpelVarianceTest<SubpixAvgVarMxNFunc>::RefTest() {
  for (int x = 0; x < 8; ++x) {
    for (int y = 0; y < 8; ++y) {
      if (!use_high_bit_depth()) {
        for (int j = 0; j < block_size(); j++) {
          src_[j] = rnd_.Rand8();
          sec_[j] = rnd_.Rand8();
        }
        for (int j = 0; j < block_size() + width() + height() + 1; j++) {
          ref_[j] = rnd_.Rand8();
        }
      } else {
        for (int j = 0; j < block_size(); j++) {
          CONVERT_TO_SHORTPTR(src_)[j] = rnd_.Rand16() & mask();
          CONVERT_TO_SHORTPTR(sec_)[j] = rnd_.Rand16() & mask();
        }
        for (int j = 0; j < block_size() + width() + height() + 1; j++) {
          CONVERT_TO_SHORTPTR(ref_)[j] = rnd_.Rand16() & mask();
        }
      }
      uint32_t sse1, sse2;
      uint32_t var1, var2;
      ASM_REGISTER_STATE_CHECK(var1 = params_.func(ref_, width() + 1, x, y,
                                                   src_, width(), &sse1, sec_));
      var2 = subpel_avg_variance_ref(ref_, src_, sec_, params_.log2width,
                                     params_.log2height, x, y, &sse2,
                                     use_high_bit_depth(), params_.bit_depth);
      EXPECT_EQ(sse1, sse2) << "at position " << x << ", " << y;
      EXPECT_EQ(var1, var2) << "at position " << x << ", " << y;
    }
  }
}

template <>
void SubpelVarianceTest<JntSubpixAvgVarMxNFunc>::RefTest() {
  for (int x = 0; x < 8; ++x) {
    for (int y = 0; y < 8; ++y) {
      if (!use_high_bit_depth()) {
        for (int j = 0; j < block_size(); j++) {
          src_[j] = rnd_.Rand8();
          sec_[j] = rnd_.Rand8();
        }
        for (int j = 0; j < block_size() + width() + height() + 1; j++) {
          ref_[j] = rnd_.Rand8();
        }
      } else {
        for (int j = 0; j < block_size(); j++) {
          CONVERT_TO_SHORTPTR(src_)[j] = rnd_.Rand16() & mask();
          CONVERT_TO_SHORTPTR(sec_)[j] = rnd_.Rand16() & mask();
        }
        for (int j = 0; j < block_size() + width() + height() + 1; j++) {
          CONVERT_TO_SHORTPTR(ref_)[j] = rnd_.Rand16() & mask();
        }
      }
      for (int x0 = 0; x0 < 2; ++x0) {
        for (int y0 = 0; y0 < 4; ++y0) {
          uint32_t sse1, sse2;
          uint32_t var1, var2;
          jcp_param_.fwd_offset = quant_dist_lookup_table[x0][y0][0];
          jcp_param_.bck_offset = quant_dist_lookup_table[x0][y0][1];
          ASM_REGISTER_STATE_CHECK(var1 = params_.func(ref_, width() + 0, x, y,
                                                       src_, width(), &sse1,
                                                       sec_, &jcp_param_));
          var2 = jnt_subpel_avg_variance_ref(
              ref_, src_, sec_, params_.log2width, params_.log2height, x, y,
              &sse2, use_high_bit_depth(), params_.bit_depth, &jcp_param_);
          EXPECT_EQ(sse1, sse2) << "at position " << x << ", " << y;
          EXPECT_EQ(var1, var2) << "at position " << x << ", " << y;
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

static const int kMaskMax = 64;

typedef TestParams<ObmcSubpelVarFunc> ObmcSubpelVarianceParams;

template <typename FunctionType>
class ObmcVarianceTest
    : public ::testing::TestWithParam<TestParams<FunctionType> > {
 public:
  virtual void SetUp() {
    params_ = this->GetParam();

    rnd_.Reset(ACMRandom::DeterministicSeed());
    if (!use_high_bit_depth()) {
      pre_ = reinterpret_cast<uint8_t *>(
          aom_memalign(32, block_size() + width() + height() + 1));
    } else {
      pre_ = CONVERT_TO_BYTEPTR(reinterpret_cast<uint16_t *>(aom_memalign(
          32, block_size() + width() + height() + 1 * sizeof(uint16_t))));
    }
    wsrc_ = reinterpret_cast<int32_t *>(
        aom_memalign(32, block_size() * sizeof(uint32_t)));
    mask_ = reinterpret_cast<int32_t *>(
        aom_memalign(32, block_size() * sizeof(uint32_t)));
    ASSERT_TRUE(pre_ != NULL);
    ASSERT_TRUE(wsrc_ != NULL);
    ASSERT_TRUE(mask_ != NULL);
  }

  virtual void TearDown() {
    if (!use_high_bit_depth()) {
      aom_free(pre_);
    } else {
      aom_free(CONVERT_TO_SHORTPTR(pre_));
    }
    aom_free(wsrc_);
    aom_free(mask_);
    libaom_test::ClearSystemState();
  }

 protected:
  void RefTest();
  void ExtremeRefTest();
  void SpeedTest();

  ACMRandom rnd_;
  uint8_t *pre_;
  int32_t *wsrc_;
  int32_t *mask_;
  TestParams<FunctionType> params_;

  // some relay helpers
  bool use_high_bit_depth() const { return params_.use_high_bit_depth; }
  int byte_shift() const { return params_.bit_depth - 8; }
  int block_size() const { return params_.block_size; }
  int width() const { return params_.width; }
  int height() const { return params_.height; }
  uint32_t bd_mask() const { return params_.mask; }
};

template <>
void ObmcVarianceTest<ObmcSubpelVarFunc>::RefTest() {
  for (int x = 0; x < 8; ++x) {
    for (int y = 0; y < 8; ++y) {
      if (!use_high_bit_depth())
        for (int j = 0; j < block_size() + width() + height() + 1; j++)
          pre_[j] = rnd_.Rand8();
      else
        for (int j = 0; j < block_size() + width() + height() + 1; j++)
          CONVERT_TO_SHORTPTR(pre_)[j] = rnd_.Rand16() & bd_mask();
      for (int j = 0; j < block_size(); j++) {
        wsrc_[j] = (rnd_.Rand16() & bd_mask()) * rnd_(kMaskMax * kMaskMax + 1);
        mask_[j] = rnd_(kMaskMax * kMaskMax + 1);
      }

      uint32_t sse1, sse2;
      uint32_t var1, var2;
      ASM_REGISTER_STATE_CHECK(
          var1 = params_.func(pre_, width() + 1, x, y, wsrc_, mask_, &sse1));
      var2 = obmc_subpel_variance_ref(
          pre_, params_.log2width, params_.log2height, x, y, wsrc_, mask_,
          &sse2, use_high_bit_depth(), params_.bit_depth);
      EXPECT_EQ(sse1, sse2) << "for xoffset " << x << " and yoffset " << y;
      EXPECT_EQ(var1, var2) << "for xoffset " << x << " and yoffset " << y;
    }
  }
}

template <>
void ObmcVarianceTest<ObmcSubpelVarFunc>::ExtremeRefTest() {
  // Pre: Set the first half of values to the maximum, the second half to 0.
  // Mask: same as above
  // WSrc: Set the first half of values to 0, the second half to the maximum.
  for (int x = 0; x < 8; ++x) {
    for (int y = 0; y < 8; ++y) {
      const int half = block_size() / 2;
      if (!use_high_bit_depth()) {
        memset(pre_, 255, half);
        memset(pre_ + half, 0, half + width() + height() + 1);
      } else {
        aom_memset16(CONVERT_TO_SHORTPTR(pre_), bd_mask(), half);
        aom_memset16(CONVERT_TO_SHORTPTR(pre_) + half, 0, half);
      }
      for (int j = 0; j < half; j++) {
        wsrc_[j] = bd_mask() * kMaskMax * kMaskMax;
        mask_[j] = 0;
      }
      for (int j = half; j < block_size(); j++) {
        wsrc_[j] = 0;
        mask_[j] = kMaskMax * kMaskMax;
      }

      uint32_t sse1, sse2;
      uint32_t var1, var2;
      ASM_REGISTER_STATE_CHECK(
          var1 = params_.func(pre_, width() + 1, x, y, wsrc_, mask_, &sse1));
      var2 = obmc_subpel_variance_ref(
          pre_, params_.log2width, params_.log2height, x, y, wsrc_, mask_,
          &sse2, use_high_bit_depth(), params_.bit_depth);
      EXPECT_EQ(sse1, sse2) << "for xoffset " << x << " and yoffset " << y;
      EXPECT_EQ(var1, var2) << "for xoffset " << x << " and yoffset " << y;
    }
  }
}

template <>
void ObmcVarianceTest<ObmcSubpelVarFunc>::SpeedTest() {
  if (!use_high_bit_depth())
    for (int j = 0; j < block_size() + width() + height() + 1; j++)
      pre_[j] = rnd_.Rand8();
  else
    for (int j = 0; j < block_size() + width() + height() + 1; j++)
      CONVERT_TO_SHORTPTR(pre_)[j] = rnd_.Rand16() & bd_mask();
  for (int j = 0; j < block_size(); j++) {
    wsrc_[j] = (rnd_.Rand16() & bd_mask()) * rnd_(kMaskMax * kMaskMax + 1);
    mask_[j] = rnd_(kMaskMax * kMaskMax + 1);
  }
  unsigned int sse1;
  const int stride = width() + 1;
  int run_time = 1000000000 / block_size();
  aom_usec_timer timer;

  aom_usec_timer_start(&timer);
  for (int i = 0; i < run_time; ++i) {
    int x = rnd_(8);
    int y = rnd_(8);
    ASM_REGISTER_STATE_CHECK(
        params_.func(pre_, stride, x, y, wsrc_, mask_, &sse1));
  }
  aom_usec_timer_mark(&timer);

  const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
  printf("obmc_sub_pixel_variance_%dx%d_%d: %d us\n", width(), height(),
         params_.bit_depth, elapsed_time);
}

typedef MainTestClass<Get4x4SseFunc> AvxSseTest;
typedef MainTestClass<VarianceMxNFunc> AvxMseTest;
typedef MainTestClass<VarianceMxNFunc> AvxVarianceTest;
typedef SubpelVarianceTest<SubpixVarMxNFunc> AvxSubpelVarianceTest;
typedef SubpelVarianceTest<SubpixAvgVarMxNFunc> AvxSubpelAvgVarianceTest;
typedef SubpelVarianceTest<JntSubpixAvgVarMxNFunc> AvxJntSubpelAvgVarianceTest;
typedef ObmcVarianceTest<ObmcSubpelVarFunc> AvxObmcSubpelVarianceTest;

TEST_P(AvxSseTest, RefSse) { RefTestSse(); }
TEST_P(AvxSseTest, MaxSse) { MaxTestSse(); }
TEST_P(AvxMseTest, RefMse) { RefTestMse(); }
TEST_P(AvxMseTest, MaxMse) { MaxTestMse(); }
TEST_P(AvxVarianceTest, Zero) { ZeroTest(); }
TEST_P(AvxVarianceTest, Ref) { RefTest(); }
TEST_P(AvxVarianceTest, RefStride) { RefStrideTest(); }
TEST_P(AvxVarianceTest, OneQuarter) { OneQuarterTest(); }
TEST_P(AvxVarianceTest, DISABLED_Speed) { SpeedTest(); }
TEST_P(SumOfSquaresTest, Const) { ConstTest(); }
TEST_P(SumOfSquaresTest, Ref) { RefTest(); }
TEST_P(AvxSubpelVarianceTest, Ref) { RefTest(); }
TEST_P(AvxSubpelVarianceTest, ExtremeRef) { ExtremeRefTest(); }
TEST_P(AvxSubpelAvgVarianceTest, Ref) { RefTest(); }
TEST_P(AvxJntSubpelAvgVarianceTest, Ref) { RefTest(); }
TEST_P(AvxObmcSubpelVarianceTest, Ref) { RefTest(); }
TEST_P(AvxObmcSubpelVarianceTest, ExtremeRef) { ExtremeRefTest(); }
TEST_P(AvxObmcSubpelVarianceTest, DISABLED_Speed) { SpeedTest(); }

INSTANTIATE_TEST_CASE_P(C, SumOfSquaresTest,
                        ::testing::Values(aom_get_mb_ss_c));

typedef TestParams<Get4x4SseFunc> SseParams;
INSTANTIATE_TEST_CASE_P(C, AvxSseTest,
                        ::testing::Values(SseParams(2, 2,
                                                    &aom_get4x4sse_cs_c)));

typedef TestParams<VarianceMxNFunc> MseParams;
INSTANTIATE_TEST_CASE_P(C, AvxMseTest,
                        ::testing::Values(MseParams(4, 4, &aom_mse16x16_c),
                                          MseParams(4, 3, &aom_mse16x8_c),
                                          MseParams(3, 4, &aom_mse8x16_c),
                                          MseParams(3, 3, &aom_mse8x8_c)));

typedef TestParams<VarianceMxNFunc> VarianceParams;
INSTANTIATE_TEST_CASE_P(
    C, AvxVarianceTest,
    ::testing::Values(VarianceParams(7, 7, &aom_variance128x128_c),
                      VarianceParams(7, 6, &aom_variance128x64_c),
                      VarianceParams(6, 7, &aom_variance64x128_c),
                      VarianceParams(6, 6, &aom_variance64x64_c),
                      VarianceParams(6, 5, &aom_variance64x32_c),
                      VarianceParams(5, 6, &aom_variance32x64_c),
                      VarianceParams(5, 5, &aom_variance32x32_c),
                      VarianceParams(5, 4, &aom_variance32x16_c),
                      VarianceParams(4, 5, &aom_variance16x32_c),
                      VarianceParams(4, 4, &aom_variance16x16_c),
                      VarianceParams(4, 3, &aom_variance16x8_c),
                      VarianceParams(3, 4, &aom_variance8x16_c),
                      VarianceParams(3, 3, &aom_variance8x8_c),
                      VarianceParams(3, 2, &aom_variance8x4_c),
                      VarianceParams(2, 3, &aom_variance4x8_c),
                      VarianceParams(2, 2, &aom_variance4x4_c)));

typedef TestParams<SubpixVarMxNFunc> SubpelVarianceParams;
INSTANTIATE_TEST_CASE_P(
    C, AvxSubpelVarianceTest,
    ::testing::Values(
        SubpelVarianceParams(7, 7, &aom_sub_pixel_variance128x128_c, 0),
        SubpelVarianceParams(7, 6, &aom_sub_pixel_variance128x64_c, 0),
        SubpelVarianceParams(6, 7, &aom_sub_pixel_variance64x128_c, 0),
        SubpelVarianceParams(6, 6, &aom_sub_pixel_variance64x64_c, 0),
        SubpelVarianceParams(6, 5, &aom_sub_pixel_variance64x32_c, 0),
        SubpelVarianceParams(5, 6, &aom_sub_pixel_variance32x64_c, 0),
        SubpelVarianceParams(5, 5, &aom_sub_pixel_variance32x32_c, 0),
        SubpelVarianceParams(5, 4, &aom_sub_pixel_variance32x16_c, 0),
        SubpelVarianceParams(4, 5, &aom_sub_pixel_variance16x32_c, 0),
        SubpelVarianceParams(4, 4, &aom_sub_pixel_variance16x16_c, 0),
        SubpelVarianceParams(4, 3, &aom_sub_pixel_variance16x8_c, 0),
        SubpelVarianceParams(3, 4, &aom_sub_pixel_variance8x16_c, 0),
        SubpelVarianceParams(3, 3, &aom_sub_pixel_variance8x8_c, 0),
        SubpelVarianceParams(3, 2, &aom_sub_pixel_variance8x4_c, 0),
        SubpelVarianceParams(2, 3, &aom_sub_pixel_variance4x8_c, 0),
        SubpelVarianceParams(2, 2, &aom_sub_pixel_variance4x4_c, 0)));

typedef TestParams<SubpixAvgVarMxNFunc> SubpelAvgVarianceParams;
INSTANTIATE_TEST_CASE_P(
    C, AvxSubpelAvgVarianceTest,
    ::testing::Values(
        SubpelAvgVarianceParams(7, 7, &aom_sub_pixel_avg_variance128x128_c, 0),
        SubpelAvgVarianceParams(7, 6, &aom_sub_pixel_avg_variance128x64_c, 0),
        SubpelAvgVarianceParams(6, 7, &aom_sub_pixel_avg_variance64x128_c, 0),
        SubpelAvgVarianceParams(6, 6, &aom_sub_pixel_avg_variance64x64_c, 0),
        SubpelAvgVarianceParams(6, 5, &aom_sub_pixel_avg_variance64x32_c, 0),
        SubpelAvgVarianceParams(5, 6, &aom_sub_pixel_avg_variance32x64_c, 0),
        SubpelAvgVarianceParams(5, 5, &aom_sub_pixel_avg_variance32x32_c, 0),
        SubpelAvgVarianceParams(5, 4, &aom_sub_pixel_avg_variance32x16_c, 0),
        SubpelAvgVarianceParams(4, 5, &aom_sub_pixel_avg_variance16x32_c, 0),
        SubpelAvgVarianceParams(4, 4, &aom_sub_pixel_avg_variance16x16_c, 0),
        SubpelAvgVarianceParams(4, 3, &aom_sub_pixel_avg_variance16x8_c, 0),
        SubpelAvgVarianceParams(3, 4, &aom_sub_pixel_avg_variance8x16_c, 0),
        SubpelAvgVarianceParams(3, 3, &aom_sub_pixel_avg_variance8x8_c, 0),
        SubpelAvgVarianceParams(3, 2, &aom_sub_pixel_avg_variance8x4_c, 0),
        SubpelAvgVarianceParams(2, 3, &aom_sub_pixel_avg_variance4x8_c, 0),
        SubpelAvgVarianceParams(2, 2, &aom_sub_pixel_avg_variance4x4_c, 0)));

typedef TestParams<JntSubpixAvgVarMxNFunc> JntSubpelAvgVarianceParams;
INSTANTIATE_TEST_CASE_P(
    C, AvxJntSubpelAvgVarianceTest,
    ::testing::Values(
        JntSubpelAvgVarianceParams(6, 6, &aom_jnt_sub_pixel_avg_variance64x64_c,
                                   0),
        JntSubpelAvgVarianceParams(6, 5, &aom_jnt_sub_pixel_avg_variance64x32_c,
                                   0),
        JntSubpelAvgVarianceParams(5, 6, &aom_jnt_sub_pixel_avg_variance32x64_c,
                                   0),
        JntSubpelAvgVarianceParams(5, 5, &aom_jnt_sub_pixel_avg_variance32x32_c,
                                   0),
        JntSubpelAvgVarianceParams(5, 4, &aom_jnt_sub_pixel_avg_variance32x16_c,
                                   0),
        JntSubpelAvgVarianceParams(4, 5, &aom_jnt_sub_pixel_avg_variance16x32_c,
                                   0),
        JntSubpelAvgVarianceParams(4, 4, &aom_jnt_sub_pixel_avg_variance16x16_c,
                                   0),
        JntSubpelAvgVarianceParams(4, 3, &aom_jnt_sub_pixel_avg_variance16x8_c,
                                   0),
        JntSubpelAvgVarianceParams(3, 4, &aom_jnt_sub_pixel_avg_variance8x16_c,
                                   0),
        JntSubpelAvgVarianceParams(3, 3, &aom_jnt_sub_pixel_avg_variance8x8_c,
                                   0),
        JntSubpelAvgVarianceParams(3, 2, &aom_jnt_sub_pixel_avg_variance8x4_c,
                                   0),
        JntSubpelAvgVarianceParams(2, 3, &aom_jnt_sub_pixel_avg_variance4x8_c,
                                   0),
        JntSubpelAvgVarianceParams(2, 2, &aom_jnt_sub_pixel_avg_variance4x4_c,
                                   0)));

INSTANTIATE_TEST_CASE_P(
    C, AvxObmcSubpelVarianceTest,
    ::testing::Values(
        ObmcSubpelVarianceParams(7, 7, &aom_obmc_sub_pixel_variance128x128_c,
                                 0),
        ObmcSubpelVarianceParams(7, 6, &aom_obmc_sub_pixel_variance128x64_c, 0),
        ObmcSubpelVarianceParams(6, 7, &aom_obmc_sub_pixel_variance64x128_c, 0),
        ObmcSubpelVarianceParams(6, 6, &aom_obmc_sub_pixel_variance64x64_c, 0),
        ObmcSubpelVarianceParams(6, 5, &aom_obmc_sub_pixel_variance64x32_c, 0),
        ObmcSubpelVarianceParams(5, 6, &aom_obmc_sub_pixel_variance32x64_c, 0),
        ObmcSubpelVarianceParams(5, 5, &aom_obmc_sub_pixel_variance32x32_c, 0),
        ObmcSubpelVarianceParams(5, 4, &aom_obmc_sub_pixel_variance32x16_c, 0),
        ObmcSubpelVarianceParams(4, 5, &aom_obmc_sub_pixel_variance16x32_c, 0),
        ObmcSubpelVarianceParams(4, 4, &aom_obmc_sub_pixel_variance16x16_c, 0),
        ObmcSubpelVarianceParams(4, 3, &aom_obmc_sub_pixel_variance16x8_c, 0),
        ObmcSubpelVarianceParams(3, 4, &aom_obmc_sub_pixel_variance8x16_c, 0),
        ObmcSubpelVarianceParams(3, 3, &aom_obmc_sub_pixel_variance8x8_c, 0),
        ObmcSubpelVarianceParams(3, 2, &aom_obmc_sub_pixel_variance8x4_c, 0),
        ObmcSubpelVarianceParams(2, 3, &aom_obmc_sub_pixel_variance4x8_c, 0),
        ObmcSubpelVarianceParams(2, 2, &aom_obmc_sub_pixel_variance4x4_c, 0)));

typedef MainTestClass<VarianceMxNFunc> AvxHBDMseTest;
typedef MainTestClass<VarianceMxNFunc> AvxHBDVarianceTest;
typedef SubpelVarianceTest<SubpixVarMxNFunc> AvxHBDSubpelVarianceTest;
typedef SubpelVarianceTest<SubpixAvgVarMxNFunc> AvxHBDSubpelAvgVarianceTest;
typedef ObmcVarianceTest<ObmcSubpelVarFunc> AvxHBDObmcSubpelVarianceTest;

TEST_P(AvxHBDMseTest, RefMse) { RefTestMse(); }
TEST_P(AvxHBDMseTest, MaxMse) { MaxTestMse(); }
TEST_P(AvxHBDVarianceTest, Zero) { ZeroTest(); }
TEST_P(AvxHBDVarianceTest, Ref) { RefTest(); }
TEST_P(AvxHBDVarianceTest, RefStride) { RefStrideTest(); }
TEST_P(AvxHBDVarianceTest, OneQuarter) { OneQuarterTest(); }
TEST_P(AvxHBDVarianceTest, DISABLED_Speed) { SpeedTest(); }
TEST_P(AvxHBDSubpelVarianceTest, Ref) { RefTest(); }
TEST_P(AvxHBDSubpelVarianceTest, ExtremeRef) { ExtremeRefTest(); }
TEST_P(AvxHBDSubpelAvgVarianceTest, Ref) { RefTest(); }

/* TODO(debargha): This test does not support the highbd version
INSTANTIATE_TEST_CASE_P(
    C, AvxHBDMseTest,
    ::testing::Values(make_tuple(4, 4, &aom_highbd_12_mse16x16_c),
                      make_tuple(4, 4, &aom_highbd_12_mse16x8_c),
                      make_tuple(4, 4, &aom_highbd_12_mse8x16_c),
                      make_tuple(4, 4, &aom_highbd_12_mse8x8_c),
                      make_tuple(4, 4, &aom_highbd_10_mse16x16_c),
                      make_tuple(4, 4, &aom_highbd_10_mse16x8_c),
                      make_tuple(4, 4, &aom_highbd_10_mse8x16_c),
                      make_tuple(4, 4, &aom_highbd_10_mse8x8_c),
                      make_tuple(4, 4, &aom_highbd_8_mse16x16_c),
                      make_tuple(4, 4, &aom_highbd_8_mse16x8_c),
                      make_tuple(4, 4, &aom_highbd_8_mse8x16_c),
                      make_tuple(4, 4, &aom_highbd_8_mse8x8_c)));
*/

const VarianceParams kArrayHBDVariance_c[] = {
  VarianceParams(7, 7, &aom_highbd_12_variance128x128_c, 12),
  VarianceParams(7, 6, &aom_highbd_12_variance128x64_c, 12),
  VarianceParams(6, 7, &aom_highbd_12_variance64x128_c, 12),
  VarianceParams(6, 6, &aom_highbd_12_variance64x64_c, 12),
  VarianceParams(6, 5, &aom_highbd_12_variance64x32_c, 12),
  VarianceParams(5, 6, &aom_highbd_12_variance32x64_c, 12),
  VarianceParams(5, 5, &aom_highbd_12_variance32x32_c, 12),
  VarianceParams(5, 4, &aom_highbd_12_variance32x16_c, 12),
  VarianceParams(4, 5, &aom_highbd_12_variance16x32_c, 12),
  VarianceParams(4, 4, &aom_highbd_12_variance16x16_c, 12),
  VarianceParams(4, 3, &aom_highbd_12_variance16x8_c, 12),
  VarianceParams(3, 4, &aom_highbd_12_variance8x16_c, 12),
  VarianceParams(3, 3, &aom_highbd_12_variance8x8_c, 12),
  VarianceParams(3, 2, &aom_highbd_12_variance8x4_c, 12),
  VarianceParams(2, 3, &aom_highbd_12_variance4x8_c, 12),
  VarianceParams(2, 2, &aom_highbd_12_variance4x4_c, 12),
  VarianceParams(7, 7, &aom_highbd_10_variance128x128_c, 10),
  VarianceParams(7, 6, &aom_highbd_10_variance128x64_c, 10),
  VarianceParams(6, 7, &aom_highbd_10_variance64x128_c, 10),
  VarianceParams(6, 6, &aom_highbd_10_variance64x64_c, 10),
  VarianceParams(6, 5, &aom_highbd_10_variance64x32_c, 10),
  VarianceParams(5, 6, &aom_highbd_10_variance32x64_c, 10),
  VarianceParams(5, 5, &aom_highbd_10_variance32x32_c, 10),
  VarianceParams(5, 4, &aom_highbd_10_variance32x16_c, 10),
  VarianceParams(4, 5, &aom_highbd_10_variance16x32_c, 10),
  VarianceParams(4, 4, &aom_highbd_10_variance16x16_c, 10),
  VarianceParams(4, 3, &aom_highbd_10_variance16x8_c, 10),
  VarianceParams(3, 4, &aom_highbd_10_variance8x16_c, 10),
  VarianceParams(3, 3, &aom_highbd_10_variance8x8_c, 10),
  VarianceParams(3, 2, &aom_highbd_10_variance8x4_c, 10),
  VarianceParams(2, 3, &aom_highbd_10_variance4x8_c, 10),
  VarianceParams(2, 2, &aom_highbd_10_variance4x4_c, 10),
  VarianceParams(7, 7, &aom_highbd_8_variance128x128_c, 8),
  VarianceParams(7, 6, &aom_highbd_8_variance128x64_c, 8),
  VarianceParams(6, 7, &aom_highbd_8_variance64x128_c, 8),
  VarianceParams(6, 6, &aom_highbd_8_variance64x64_c, 8),
  VarianceParams(6, 5, &aom_highbd_8_variance64x32_c, 8),
  VarianceParams(5, 6, &aom_highbd_8_variance32x64_c, 8),
  VarianceParams(5, 5, &aom_highbd_8_variance32x32_c, 8),
  VarianceParams(5, 4, &aom_highbd_8_variance32x16_c, 8),
  VarianceParams(4, 5, &aom_highbd_8_variance16x32_c, 8),
  VarianceParams(4, 4, &aom_highbd_8_variance16x16_c, 8),
  VarianceParams(4, 3, &aom_highbd_8_variance16x8_c, 8),
  VarianceParams(3, 4, &aom_highbd_8_variance8x16_c, 8),
  VarianceParams(3, 3, &aom_highbd_8_variance8x8_c, 8),
  VarianceParams(3, 2, &aom_highbd_8_variance8x4_c, 8),
  VarianceParams(2, 3, &aom_highbd_8_variance4x8_c, 8),
  VarianceParams(2, 2, &aom_highbd_8_variance4x4_c, 8)
};
INSTANTIATE_TEST_CASE_P(C, AvxHBDVarianceTest,
                        ::testing::ValuesIn(kArrayHBDVariance_c));

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(
    SSE4_1, AvxHBDVarianceTest,
    ::testing::Values(
        VarianceParams(2, 2, &aom_highbd_8_variance4x4_sse4_1, 8),
        VarianceParams(2, 2, &aom_highbd_10_variance4x4_sse4_1, 10),
        VarianceParams(2, 2, &aom_highbd_12_variance4x4_sse4_1, 12)));
#endif  // HAVE_SSE4_1

const SubpelVarianceParams kArrayHBDSubpelVariance_c[] = {
  SubpelVarianceParams(7, 7, &aom_highbd_8_sub_pixel_variance128x128_c, 8),
  SubpelVarianceParams(7, 6, &aom_highbd_8_sub_pixel_variance128x64_c, 8),
  SubpelVarianceParams(6, 7, &aom_highbd_8_sub_pixel_variance64x128_c, 8),
  SubpelVarianceParams(6, 6, &aom_highbd_8_sub_pixel_variance64x64_c, 8),
  SubpelVarianceParams(6, 5, &aom_highbd_8_sub_pixel_variance64x32_c, 8),
  SubpelVarianceParams(5, 6, &aom_highbd_8_sub_pixel_variance32x64_c, 8),
  SubpelVarianceParams(5, 5, &aom_highbd_8_sub_pixel_variance32x32_c, 8),
  SubpelVarianceParams(5, 4, &aom_highbd_8_sub_pixel_variance32x16_c, 8),
  SubpelVarianceParams(4, 5, &aom_highbd_8_sub_pixel_variance16x32_c, 8),
  SubpelVarianceParams(4, 4, &aom_highbd_8_sub_pixel_variance16x16_c, 8),
  SubpelVarianceParams(4, 3, &aom_highbd_8_sub_pixel_variance16x8_c, 8),
  SubpelVarianceParams(3, 4, &aom_highbd_8_sub_pixel_variance8x16_c, 8),
  SubpelVarianceParams(3, 3, &aom_highbd_8_sub_pixel_variance8x8_c, 8),
  SubpelVarianceParams(3, 2, &aom_highbd_8_sub_pixel_variance8x4_c, 8),
  SubpelVarianceParams(2, 3, &aom_highbd_8_sub_pixel_variance4x8_c, 8),
  SubpelVarianceParams(2, 2, &aom_highbd_8_sub_pixel_variance4x4_c, 8),
  SubpelVarianceParams(7, 7, &aom_highbd_10_sub_pixel_variance128x128_c, 10),
  SubpelVarianceParams(7, 6, &aom_highbd_10_sub_pixel_variance128x64_c, 10),
  SubpelVarianceParams(6, 7, &aom_highbd_10_sub_pixel_variance64x128_c, 10),
  SubpelVarianceParams(6, 6, &aom_highbd_10_sub_pixel_variance64x64_c, 10),
  SubpelVarianceParams(6, 5, &aom_highbd_10_sub_pixel_variance64x32_c, 10),
  SubpelVarianceParams(5, 6, &aom_highbd_10_sub_pixel_variance32x64_c, 10),
  SubpelVarianceParams(5, 5, &aom_highbd_10_sub_pixel_variance32x32_c, 10),
  SubpelVarianceParams(5, 4, &aom_highbd_10_sub_pixel_variance32x16_c, 10),
  SubpelVarianceParams(4, 5, &aom_highbd_10_sub_pixel_variance16x32_c, 10),
  SubpelVarianceParams(4, 4, &aom_highbd_10_sub_pixel_variance16x16_c, 10),
  SubpelVarianceParams(4, 3, &aom_highbd_10_sub_pixel_variance16x8_c, 10),
  SubpelVarianceParams(3, 4, &aom_highbd_10_sub_pixel_variance8x16_c, 10),
  SubpelVarianceParams(3, 3, &aom_highbd_10_sub_pixel_variance8x8_c, 10),
  SubpelVarianceParams(3, 2, &aom_highbd_10_sub_pixel_variance8x4_c, 10),
  SubpelVarianceParams(2, 3, &aom_highbd_10_sub_pixel_variance4x8_c, 10),
  SubpelVarianceParams(2, 2, &aom_highbd_10_sub_pixel_variance4x4_c, 10),
  SubpelVarianceParams(7, 7, &aom_highbd_12_sub_pixel_variance128x128_c, 12),
  SubpelVarianceParams(7, 6, &aom_highbd_12_sub_pixel_variance128x64_c, 12),
  SubpelVarianceParams(6, 7, &aom_highbd_12_sub_pixel_variance64x128_c, 12),
  SubpelVarianceParams(6, 6, &aom_highbd_12_sub_pixel_variance64x64_c, 12),
  SubpelVarianceParams(6, 5, &aom_highbd_12_sub_pixel_variance64x32_c, 12),
  SubpelVarianceParams(5, 6, &aom_highbd_12_sub_pixel_variance32x64_c, 12),
  SubpelVarianceParams(5, 5, &aom_highbd_12_sub_pixel_variance32x32_c, 12),
  SubpelVarianceParams(5, 4, &aom_highbd_12_sub_pixel_variance32x16_c, 12),
  SubpelVarianceParams(4, 5, &aom_highbd_12_sub_pixel_variance16x32_c, 12),
  SubpelVarianceParams(4, 4, &aom_highbd_12_sub_pixel_variance16x16_c, 12),
  SubpelVarianceParams(4, 3, &aom_highbd_12_sub_pixel_variance16x8_c, 12),
  SubpelVarianceParams(3, 4, &aom_highbd_12_sub_pixel_variance8x16_c, 12),
  SubpelVarianceParams(3, 3, &aom_highbd_12_sub_pixel_variance8x8_c, 12),
  SubpelVarianceParams(3, 2, &aom_highbd_12_sub_pixel_variance8x4_c, 12),
  SubpelVarianceParams(2, 3, &aom_highbd_12_sub_pixel_variance4x8_c, 12),
  SubpelVarianceParams(2, 2, &aom_highbd_12_sub_pixel_variance4x4_c, 12),
};
INSTANTIATE_TEST_CASE_P(C, AvxHBDSubpelVarianceTest,
                        ::testing::ValuesIn(kArrayHBDSubpelVariance_c));

const SubpelAvgVarianceParams kArrayHBDSubpelAvgVariance_c[] = {
  SubpelAvgVarianceParams(7, 7, &aom_highbd_8_sub_pixel_avg_variance128x128_c,
                          8),
  SubpelAvgVarianceParams(7, 6, &aom_highbd_8_sub_pixel_avg_variance128x64_c,
                          8),
  SubpelAvgVarianceParams(6, 7, &aom_highbd_8_sub_pixel_avg_variance64x128_c,
                          8),
  SubpelAvgVarianceParams(6, 6, &aom_highbd_8_sub_pixel_avg_variance64x64_c, 8),
  SubpelAvgVarianceParams(6, 5, &aom_highbd_8_sub_pixel_avg_variance64x32_c, 8),
  SubpelAvgVarianceParams(5, 6, &aom_highbd_8_sub_pixel_avg_variance32x64_c, 8),
  SubpelAvgVarianceParams(5, 5, &aom_highbd_8_sub_pixel_avg_variance32x32_c, 8),
  SubpelAvgVarianceParams(5, 4, &aom_highbd_8_sub_pixel_avg_variance32x16_c, 8),
  SubpelAvgVarianceParams(4, 5, &aom_highbd_8_sub_pixel_avg_variance16x32_c, 8),
  SubpelAvgVarianceParams(4, 4, &aom_highbd_8_sub_pixel_avg_variance16x16_c, 8),
  SubpelAvgVarianceParams(4, 3, &aom_highbd_8_sub_pixel_avg_variance16x8_c, 8),
  SubpelAvgVarianceParams(3, 4, &aom_highbd_8_sub_pixel_avg_variance8x16_c, 8),
  SubpelAvgVarianceParams(3, 3, &aom_highbd_8_sub_pixel_avg_variance8x8_c, 8),
  SubpelAvgVarianceParams(3, 2, &aom_highbd_8_sub_pixel_avg_variance8x4_c, 8),
  SubpelAvgVarianceParams(2, 3, &aom_highbd_8_sub_pixel_avg_variance4x8_c, 8),
  SubpelAvgVarianceParams(2, 2, &aom_highbd_8_sub_pixel_avg_variance4x4_c, 8),
  SubpelAvgVarianceParams(7, 7, &aom_highbd_10_sub_pixel_avg_variance128x128_c,
                          10),
  SubpelAvgVarianceParams(7, 6, &aom_highbd_10_sub_pixel_avg_variance128x64_c,
                          10),
  SubpelAvgVarianceParams(6, 7, &aom_highbd_10_sub_pixel_avg_variance64x128_c,
                          10),
  SubpelAvgVarianceParams(6, 6, &aom_highbd_10_sub_pixel_avg_variance64x64_c,
                          10),
  SubpelAvgVarianceParams(6, 5, &aom_highbd_10_sub_pixel_avg_variance64x32_c,
                          10),
  SubpelAvgVarianceParams(5, 6, &aom_highbd_10_sub_pixel_avg_variance32x64_c,
                          10),
  SubpelAvgVarianceParams(5, 5, &aom_highbd_10_sub_pixel_avg_variance32x32_c,
                          10),
  SubpelAvgVarianceParams(5, 4, &aom_highbd_10_sub_pixel_avg_variance32x16_c,
                          10),
  SubpelAvgVarianceParams(4, 5, &aom_highbd_10_sub_pixel_avg_variance16x32_c,
                          10),
  SubpelAvgVarianceParams(4, 4, &aom_highbd_10_sub_pixel_avg_variance16x16_c,
                          10),
  SubpelAvgVarianceParams(4, 3, &aom_highbd_10_sub_pixel_avg_variance16x8_c,
                          10),
  SubpelAvgVarianceParams(3, 4, &aom_highbd_10_sub_pixel_avg_variance8x16_c,
                          10),
  SubpelAvgVarianceParams(3, 3, &aom_highbd_10_sub_pixel_avg_variance8x8_c, 10),
  SubpelAvgVarianceParams(3, 2, &aom_highbd_10_sub_pixel_avg_variance8x4_c, 10),
  SubpelAvgVarianceParams(2, 3, &aom_highbd_10_sub_pixel_avg_variance4x8_c, 10),
  SubpelAvgVarianceParams(2, 2, &aom_highbd_10_sub_pixel_avg_variance4x4_c, 10),
  SubpelAvgVarianceParams(7, 7, &aom_highbd_12_sub_pixel_avg_variance128x128_c,
                          12),
  SubpelAvgVarianceParams(7, 6, &aom_highbd_12_sub_pixel_avg_variance128x64_c,
                          12),
  SubpelAvgVarianceParams(6, 7, &aom_highbd_12_sub_pixel_avg_variance64x128_c,
                          12),
  SubpelAvgVarianceParams(6, 6, &aom_highbd_12_sub_pixel_avg_variance64x64_c,
                          12),
  SubpelAvgVarianceParams(6, 5, &aom_highbd_12_sub_pixel_avg_variance64x32_c,
                          12),
  SubpelAvgVarianceParams(5, 6, &aom_highbd_12_sub_pixel_avg_variance32x64_c,
                          12),
  SubpelAvgVarianceParams(5, 5, &aom_highbd_12_sub_pixel_avg_variance32x32_c,
                          12),
  SubpelAvgVarianceParams(5, 4, &aom_highbd_12_sub_pixel_avg_variance32x16_c,
                          12),
  SubpelAvgVarianceParams(4, 5, &aom_highbd_12_sub_pixel_avg_variance16x32_c,
                          12),
  SubpelAvgVarianceParams(4, 4, &aom_highbd_12_sub_pixel_avg_variance16x16_c,
                          12),
  SubpelAvgVarianceParams(4, 3, &aom_highbd_12_sub_pixel_avg_variance16x8_c,
                          12),
  SubpelAvgVarianceParams(3, 4, &aom_highbd_12_sub_pixel_avg_variance8x16_c,
                          12),
  SubpelAvgVarianceParams(3, 3, &aom_highbd_12_sub_pixel_avg_variance8x8_c, 12),
  SubpelAvgVarianceParams(3, 2, &aom_highbd_12_sub_pixel_avg_variance8x4_c, 12),
  SubpelAvgVarianceParams(2, 3, &aom_highbd_12_sub_pixel_avg_variance4x8_c, 12),
  SubpelAvgVarianceParams(2, 2, &aom_highbd_12_sub_pixel_avg_variance4x4_c, 12)
};
INSTANTIATE_TEST_CASE_P(C, AvxHBDSubpelAvgVarianceTest,
                        ::testing::ValuesIn(kArrayHBDSubpelAvgVariance_c));

const ObmcSubpelVarianceParams kArrayHBDObmcSubpelVariance_c[] = {
  ObmcSubpelVarianceParams(7, 7, &aom_highbd_obmc_sub_pixel_variance128x128_c,
                           8),
  ObmcSubpelVarianceParams(7, 6, &aom_highbd_obmc_sub_pixel_variance128x64_c,
                           8),
  ObmcSubpelVarianceParams(6, 7, &aom_highbd_obmc_sub_pixel_variance64x128_c,
                           8),
  ObmcSubpelVarianceParams(6, 6, &aom_highbd_obmc_sub_pixel_variance64x64_c, 8),
  ObmcSubpelVarianceParams(6, 5, &aom_highbd_obmc_sub_pixel_variance64x32_c, 8),
  ObmcSubpelVarianceParams(5, 6, &aom_highbd_obmc_sub_pixel_variance32x64_c, 8),
  ObmcSubpelVarianceParams(5, 5, &aom_highbd_obmc_sub_pixel_variance32x32_c, 8),
  ObmcSubpelVarianceParams(5, 4, &aom_highbd_obmc_sub_pixel_variance32x16_c, 8),
  ObmcSubpelVarianceParams(4, 5, &aom_highbd_obmc_sub_pixel_variance16x32_c, 8),
  ObmcSubpelVarianceParams(4, 4, &aom_highbd_obmc_sub_pixel_variance16x16_c, 8),
  ObmcSubpelVarianceParams(4, 3, &aom_highbd_obmc_sub_pixel_variance16x8_c, 8),
  ObmcSubpelVarianceParams(3, 4, &aom_highbd_obmc_sub_pixel_variance8x16_c, 8),
  ObmcSubpelVarianceParams(3, 3, &aom_highbd_obmc_sub_pixel_variance8x8_c, 8),
  ObmcSubpelVarianceParams(3, 2, &aom_highbd_obmc_sub_pixel_variance8x4_c, 8),
  ObmcSubpelVarianceParams(2, 3, &aom_highbd_obmc_sub_pixel_variance4x8_c, 8),
  ObmcSubpelVarianceParams(2, 2, &aom_highbd_obmc_sub_pixel_variance4x4_c, 8),
  ObmcSubpelVarianceParams(7, 7,
                           &aom_highbd_10_obmc_sub_pixel_variance128x128_c, 10),
  ObmcSubpelVarianceParams(7, 6, &aom_highbd_10_obmc_sub_pixel_variance128x64_c,
                           10),
  ObmcSubpelVarianceParams(6, 7, &aom_highbd_10_obmc_sub_pixel_variance64x128_c,
                           10),
  ObmcSubpelVarianceParams(6, 6, &aom_highbd_10_obmc_sub_pixel_variance64x64_c,
                           10),
  ObmcSubpelVarianceParams(6, 5, &aom_highbd_10_obmc_sub_pixel_variance64x32_c,
                           10),
  ObmcSubpelVarianceParams(5, 6, &aom_highbd_10_obmc_sub_pixel_variance32x64_c,
                           10),
  ObmcSubpelVarianceParams(5, 5, &aom_highbd_10_obmc_sub_pixel_variance32x32_c,
                           10),
  ObmcSubpelVarianceParams(5, 4, &aom_highbd_10_obmc_sub_pixel_variance32x16_c,
                           10),
  ObmcSubpelVarianceParams(4, 5, &aom_highbd_10_obmc_sub_pixel_variance16x32_c,
                           10),
  ObmcSubpelVarianceParams(4, 4, &aom_highbd_10_obmc_sub_pixel_variance16x16_c,
                           10),
  ObmcSubpelVarianceParams(4, 3, &aom_highbd_10_obmc_sub_pixel_variance16x8_c,
                           10),
  ObmcSubpelVarianceParams(3, 4, &aom_highbd_10_obmc_sub_pixel_variance8x16_c,
                           10),
  ObmcSubpelVarianceParams(3, 3, &aom_highbd_10_obmc_sub_pixel_variance8x8_c,
                           10),
  ObmcSubpelVarianceParams(3, 2, &aom_highbd_10_obmc_sub_pixel_variance8x4_c,
                           10),
  ObmcSubpelVarianceParams(2, 3, &aom_highbd_10_obmc_sub_pixel_variance4x8_c,
                           10),
  ObmcSubpelVarianceParams(2, 2, &aom_highbd_10_obmc_sub_pixel_variance4x4_c,
                           10),
  ObmcSubpelVarianceParams(7, 7,
                           &aom_highbd_12_obmc_sub_pixel_variance128x128_c, 12),
  ObmcSubpelVarianceParams(7, 6, &aom_highbd_12_obmc_sub_pixel_variance128x64_c,
                           12),
  ObmcSubpelVarianceParams(6, 7, &aom_highbd_12_obmc_sub_pixel_variance64x128_c,
                           12),
  ObmcSubpelVarianceParams(6, 6, &aom_highbd_12_obmc_sub_pixel_variance64x64_c,
                           12),
  ObmcSubpelVarianceParams(6, 5, &aom_highbd_12_obmc_sub_pixel_variance64x32_c,
                           12),
  ObmcSubpelVarianceParams(5, 6, &aom_highbd_12_obmc_sub_pixel_variance32x64_c,
                           12),
  ObmcSubpelVarianceParams(5, 5, &aom_highbd_12_obmc_sub_pixel_variance32x32_c,
                           12),
  ObmcSubpelVarianceParams(5, 4, &aom_highbd_12_obmc_sub_pixel_variance32x16_c,
                           12),
  ObmcSubpelVarianceParams(4, 5, &aom_highbd_12_obmc_sub_pixel_variance16x32_c,
                           12),
  ObmcSubpelVarianceParams(4, 4, &aom_highbd_12_obmc_sub_pixel_variance16x16_c,
                           12),
  ObmcSubpelVarianceParams(4, 3, &aom_highbd_12_obmc_sub_pixel_variance16x8_c,
                           12),
  ObmcSubpelVarianceParams(3, 4, &aom_highbd_12_obmc_sub_pixel_variance8x16_c,
                           12),
  ObmcSubpelVarianceParams(3, 3, &aom_highbd_12_obmc_sub_pixel_variance8x8_c,
                           12),
  ObmcSubpelVarianceParams(3, 2, &aom_highbd_12_obmc_sub_pixel_variance8x4_c,
                           12),
  ObmcSubpelVarianceParams(2, 3, &aom_highbd_12_obmc_sub_pixel_variance4x8_c,
                           12),
  ObmcSubpelVarianceParams(2, 2, &aom_highbd_12_obmc_sub_pixel_variance4x4_c,
                           12)
};
INSTANTIATE_TEST_CASE_P(C, AvxHBDObmcSubpelVarianceTest,
                        ::testing::ValuesIn(kArrayHBDObmcSubpelVariance_c));

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(SSE2, SumOfSquaresTest,
                        ::testing::Values(aom_get_mb_ss_sse2));

INSTANTIATE_TEST_CASE_P(SSE2, AvxMseTest,
                        ::testing::Values(MseParams(4, 4, &aom_mse16x16_sse2),
                                          MseParams(4, 3, &aom_mse16x8_sse2),
                                          MseParams(3, 4, &aom_mse8x16_sse2),
                                          MseParams(3, 3, &aom_mse8x8_sse2)));

INSTANTIATE_TEST_CASE_P(
    SSE2, AvxVarianceTest,
    ::testing::Values(VarianceParams(7, 7, &aom_variance128x128_sse2),
                      VarianceParams(7, 6, &aom_variance128x64_sse2),
                      VarianceParams(6, 7, &aom_variance64x128_sse2),
                      VarianceParams(6, 6, &aom_variance64x64_sse2),
                      VarianceParams(6, 5, &aom_variance64x32_sse2),
                      VarianceParams(6, 4, &aom_variance64x16_sse2),
                      VarianceParams(5, 6, &aom_variance32x64_sse2),
                      VarianceParams(5, 5, &aom_variance32x32_sse2),
                      VarianceParams(5, 4, &aom_variance32x16_sse2),
                      VarianceParams(5, 3, &aom_variance32x8_sse2),
                      VarianceParams(4, 6, &aom_variance16x64_sse2),
                      VarianceParams(4, 5, &aom_variance16x32_sse2),
                      VarianceParams(4, 4, &aom_variance16x16_sse2),
                      VarianceParams(4, 3, &aom_variance16x8_sse2),
                      VarianceParams(4, 2, &aom_variance16x4_sse2),
                      VarianceParams(3, 5, &aom_variance8x32_sse2),
                      VarianceParams(3, 4, &aom_variance8x16_sse2),
                      VarianceParams(3, 3, &aom_variance8x8_sse2),
                      VarianceParams(3, 2, &aom_variance8x4_sse2),
                      VarianceParams(2, 4, &aom_variance4x16_sse2),
                      VarianceParams(2, 3, &aom_variance4x8_sse2),
                      VarianceParams(2, 2, &aom_variance4x4_sse2)));

INSTANTIATE_TEST_CASE_P(
    SSE2, AvxSubpelVarianceTest,
    ::testing::Values(
        SubpelVarianceParams(7, 7, &aom_sub_pixel_variance128x128_sse2, 0),
        SubpelVarianceParams(7, 6, &aom_sub_pixel_variance128x64_sse2, 0),
        SubpelVarianceParams(6, 7, &aom_sub_pixel_variance64x128_sse2, 0),
        SubpelVarianceParams(6, 6, &aom_sub_pixel_variance64x64_sse2, 0),
        SubpelVarianceParams(6, 5, &aom_sub_pixel_variance64x32_sse2, 0),
        SubpelVarianceParams(5, 6, &aom_sub_pixel_variance32x64_sse2, 0),
        SubpelVarianceParams(5, 5, &aom_sub_pixel_variance32x32_sse2, 0),
        SubpelVarianceParams(5, 4, &aom_sub_pixel_variance32x16_sse2, 0),
        SubpelVarianceParams(4, 5, &aom_sub_pixel_variance16x32_sse2, 0),
        SubpelVarianceParams(4, 4, &aom_sub_pixel_variance16x16_sse2, 0),
        SubpelVarianceParams(4, 3, &aom_sub_pixel_variance16x8_sse2, 0),
        SubpelVarianceParams(3, 4, &aom_sub_pixel_variance8x16_sse2, 0),
        SubpelVarianceParams(3, 3, &aom_sub_pixel_variance8x8_sse2, 0),
        SubpelVarianceParams(3, 2, &aom_sub_pixel_variance8x4_sse2, 0),
        SubpelVarianceParams(2, 3, &aom_sub_pixel_variance4x8_sse2, 0),
        SubpelVarianceParams(2, 2, &aom_sub_pixel_variance4x4_sse2, 0)));

INSTANTIATE_TEST_CASE_P(
    SSE2, AvxSubpelAvgVarianceTest,
    ::testing::Values(
        SubpelAvgVarianceParams(7, 7, &aom_sub_pixel_avg_variance128x128_sse2,
                                0),
        SubpelAvgVarianceParams(7, 6, &aom_sub_pixel_avg_variance128x64_sse2,
                                0),
        SubpelAvgVarianceParams(6, 7, &aom_sub_pixel_avg_variance64x128_sse2,
                                0),
        SubpelAvgVarianceParams(6, 6, &aom_sub_pixel_avg_variance64x64_sse2, 0),
        SubpelAvgVarianceParams(6, 5, &aom_sub_pixel_avg_variance64x32_sse2, 0),
        SubpelAvgVarianceParams(5, 6, &aom_sub_pixel_avg_variance32x64_sse2, 0),
        SubpelAvgVarianceParams(5, 5, &aom_sub_pixel_avg_variance32x32_sse2, 0),
        SubpelAvgVarianceParams(5, 4, &aom_sub_pixel_avg_variance32x16_sse2, 0),
        SubpelAvgVarianceParams(4, 5, &aom_sub_pixel_avg_variance16x32_sse2, 0),
        SubpelAvgVarianceParams(4, 4, &aom_sub_pixel_avg_variance16x16_sse2, 0),
        SubpelAvgVarianceParams(4, 3, &aom_sub_pixel_avg_variance16x8_sse2, 0),
        SubpelAvgVarianceParams(3, 4, &aom_sub_pixel_avg_variance8x16_sse2, 0),
        SubpelAvgVarianceParams(3, 3, &aom_sub_pixel_avg_variance8x8_sse2, 0),
        SubpelAvgVarianceParams(3, 2, &aom_sub_pixel_avg_variance8x4_sse2, 0),
        SubpelAvgVarianceParams(2, 3, &aom_sub_pixel_avg_variance4x8_sse2, 0),
        SubpelAvgVarianceParams(2, 2, &aom_sub_pixel_avg_variance4x4_sse2, 0)));

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(
    SSE4_1, AvxSubpelVarianceTest,
    ::testing::Values(
        SubpelVarianceParams(2, 2, &aom_highbd_8_sub_pixel_variance4x4_sse4_1,
                             8),
        SubpelVarianceParams(2, 2, &aom_highbd_10_sub_pixel_variance4x4_sse4_1,
                             10),
        SubpelVarianceParams(2, 2, &aom_highbd_12_sub_pixel_variance4x4_sse4_1,
                             12)));

INSTANTIATE_TEST_CASE_P(
    SSE4_1, AvxSubpelAvgVarianceTest,
    ::testing::Values(
        SubpelAvgVarianceParams(2, 2,
                                &aom_highbd_8_sub_pixel_avg_variance4x4_sse4_1,
                                8),
        SubpelAvgVarianceParams(2, 2,
                                &aom_highbd_10_sub_pixel_avg_variance4x4_sse4_1,
                                10),
        SubpelAvgVarianceParams(2, 2,
                                &aom_highbd_12_sub_pixel_avg_variance4x4_sse4_1,
                                12)));
#endif  // HAVE_SSE4_1

/* TODO(debargha): This test does not support the highbd version
INSTANTIATE_TEST_CASE_P(
    SSE2, AvxHBDMseTest,
    ::testing::Values(MseParams(4, 4, &aom_highbd_12_mse16x16_sse2),
                      MseParams(4, 3, &aom_highbd_12_mse16x8_sse2),
                      MseParams(3, 4, &aom_highbd_12_mse8x16_sse2),
                      MseParams(3, 3, &aom_highbd_12_mse8x8_sse2),
                      MseParams(4, 4, &aom_highbd_10_mse16x16_sse2),
                      MseParams(4, 3, &aom_highbd_10_mse16x8_sse2),
                      MseParams(3, 4, &aom_highbd_10_mse8x16_sse2),
                      MseParams(3, 3, &aom_highbd_10_mse8x8_sse2),
                      MseParams(4, 4, &aom_highbd_8_mse16x16_sse2),
                      MseParams(4, 3, &aom_highbd_8_mse16x8_sse2),
                      MseParams(3, 4, &aom_highbd_8_mse8x16_sse2),
                      MseParams(3, 3, &aom_highbd_8_mse8x8_sse2)));
*/

const VarianceParams kArrayHBDVariance_sse2[] = {
  VarianceParams(7, 7, &aom_highbd_12_variance128x128_sse2, 12),
  VarianceParams(7, 6, &aom_highbd_12_variance128x64_sse2, 12),
  VarianceParams(6, 7, &aom_highbd_12_variance64x128_sse2, 12),
  VarianceParams(6, 6, &aom_highbd_12_variance64x64_sse2, 12),
  VarianceParams(6, 5, &aom_highbd_12_variance64x32_sse2, 12),
  VarianceParams(5, 6, &aom_highbd_12_variance32x64_sse2, 12),
  VarianceParams(5, 5, &aom_highbd_12_variance32x32_sse2, 12),
  VarianceParams(5, 4, &aom_highbd_12_variance32x16_sse2, 12),
  VarianceParams(4, 5, &aom_highbd_12_variance16x32_sse2, 12),
  VarianceParams(4, 4, &aom_highbd_12_variance16x16_sse2, 12),
  VarianceParams(4, 3, &aom_highbd_12_variance16x8_sse2, 12),
  VarianceParams(3, 4, &aom_highbd_12_variance8x16_sse2, 12),
  VarianceParams(3, 3, &aom_highbd_12_variance8x8_sse2, 12),
  VarianceParams(7, 7, &aom_highbd_10_variance128x128_sse2, 10),
  VarianceParams(7, 6, &aom_highbd_10_variance128x64_sse2, 10),
  VarianceParams(6, 7, &aom_highbd_10_variance64x128_sse2, 10),
  VarianceParams(6, 6, &aom_highbd_10_variance64x64_sse2, 10),
  VarianceParams(6, 5, &aom_highbd_10_variance64x32_sse2, 10),
  VarianceParams(5, 6, &aom_highbd_10_variance32x64_sse2, 10),
  VarianceParams(5, 5, &aom_highbd_10_variance32x32_sse2, 10),
  VarianceParams(5, 4, &aom_highbd_10_variance32x16_sse2, 10),
  VarianceParams(4, 5, &aom_highbd_10_variance16x32_sse2, 10),
  VarianceParams(4, 4, &aom_highbd_10_variance16x16_sse2, 10),
  VarianceParams(4, 3, &aom_highbd_10_variance16x8_sse2, 10),
  VarianceParams(3, 4, &aom_highbd_10_variance8x16_sse2, 10),
  VarianceParams(3, 3, &aom_highbd_10_variance8x8_sse2, 10),
  VarianceParams(7, 7, &aom_highbd_8_variance128x128_sse2, 8),
  VarianceParams(7, 6, &aom_highbd_8_variance128x64_sse2, 8),
  VarianceParams(6, 7, &aom_highbd_8_variance64x128_sse2, 8),
  VarianceParams(6, 6, &aom_highbd_8_variance64x64_sse2, 8),
  VarianceParams(6, 5, &aom_highbd_8_variance64x32_sse2, 8),
  VarianceParams(5, 6, &aom_highbd_8_variance32x64_sse2, 8),
  VarianceParams(5, 5, &aom_highbd_8_variance32x32_sse2, 8),
  VarianceParams(5, 4, &aom_highbd_8_variance32x16_sse2, 8),
  VarianceParams(4, 5, &aom_highbd_8_variance16x32_sse2, 8),
  VarianceParams(4, 4, &aom_highbd_8_variance16x16_sse2, 8),
  VarianceParams(4, 3, &aom_highbd_8_variance16x8_sse2, 8),
  VarianceParams(3, 4, &aom_highbd_8_variance8x16_sse2, 8),
  VarianceParams(3, 3, &aom_highbd_8_variance8x8_sse2, 8)
};
INSTANTIATE_TEST_CASE_P(SSE2, AvxHBDVarianceTest,
                        ::testing::ValuesIn(kArrayHBDVariance_sse2));

#if HAVE_AVX2

const VarianceParams kArrayHBDVariance_avx2[] = {
  VarianceParams(7, 7, &aom_highbd_10_variance128x128_avx2, 10),
  VarianceParams(7, 6, &aom_highbd_10_variance128x64_avx2, 10),
  VarianceParams(6, 7, &aom_highbd_10_variance64x128_avx2, 10),
  VarianceParams(6, 6, &aom_highbd_10_variance64x64_avx2, 10),
  VarianceParams(6, 5, &aom_highbd_10_variance64x32_avx2, 10),
  VarianceParams(5, 6, &aom_highbd_10_variance32x64_avx2, 10),
  VarianceParams(5, 5, &aom_highbd_10_variance32x32_avx2, 10),
  VarianceParams(5, 4, &aom_highbd_10_variance32x16_avx2, 10),
  VarianceParams(4, 5, &aom_highbd_10_variance16x32_avx2, 10),
  VarianceParams(4, 4, &aom_highbd_10_variance16x16_avx2, 10),
  VarianceParams(4, 3, &aom_highbd_10_variance16x8_avx2, 10),
  VarianceParams(3, 4, &aom_highbd_10_variance8x16_avx2, 10),
  VarianceParams(3, 3, &aom_highbd_10_variance8x8_avx2, 10)
};

INSTANTIATE_TEST_CASE_P(AVX2, AvxHBDVarianceTest,
                        ::testing::ValuesIn(kArrayHBDVariance_avx2));
#endif  // HAVE_AVX2

const SubpelVarianceParams kArrayHBDSubpelVariance_sse2[] = {
  SubpelVarianceParams(6, 6, &aom_highbd_12_sub_pixel_variance64x64_sse2, 12),
  SubpelVarianceParams(6, 5, &aom_highbd_12_sub_pixel_variance64x32_sse2, 12),
  SubpelVarianceParams(5, 6, &aom_highbd_12_sub_pixel_variance32x64_sse2, 12),
  SubpelVarianceParams(5, 5, &aom_highbd_12_sub_pixel_variance32x32_sse2, 12),
  SubpelVarianceParams(5, 4, &aom_highbd_12_sub_pixel_variance32x16_sse2, 12),
  SubpelVarianceParams(4, 5, &aom_highbd_12_sub_pixel_variance16x32_sse2, 12),
  SubpelVarianceParams(4, 4, &aom_highbd_12_sub_pixel_variance16x16_sse2, 12),
  SubpelVarianceParams(4, 3, &aom_highbd_12_sub_pixel_variance16x8_sse2, 12),
  SubpelVarianceParams(3, 4, &aom_highbd_12_sub_pixel_variance8x16_sse2, 12),
  SubpelVarianceParams(3, 3, &aom_highbd_12_sub_pixel_variance8x8_sse2, 12),
  SubpelVarianceParams(3, 2, &aom_highbd_12_sub_pixel_variance8x4_sse2, 12),
  SubpelVarianceParams(6, 6, &aom_highbd_10_sub_pixel_variance64x64_sse2, 10),
  SubpelVarianceParams(6, 5, &aom_highbd_10_sub_pixel_variance64x32_sse2, 10),
  SubpelVarianceParams(5, 6, &aom_highbd_10_sub_pixel_variance32x64_sse2, 10),
  SubpelVarianceParams(5, 5, &aom_highbd_10_sub_pixel_variance32x32_sse2, 10),
  SubpelVarianceParams(5, 4, &aom_highbd_10_sub_pixel_variance32x16_sse2, 10),
  SubpelVarianceParams(4, 5, &aom_highbd_10_sub_pixel_variance16x32_sse2, 10),
  SubpelVarianceParams(4, 4, &aom_highbd_10_sub_pixel_variance16x16_sse2, 10),
  SubpelVarianceParams(4, 3, &aom_highbd_10_sub_pixel_variance16x8_sse2, 10),
  SubpelVarianceParams(3, 4, &aom_highbd_10_sub_pixel_variance8x16_sse2, 10),
  SubpelVarianceParams(3, 3, &aom_highbd_10_sub_pixel_variance8x8_sse2, 10),
  SubpelVarianceParams(3, 2, &aom_highbd_10_sub_pixel_variance8x4_sse2, 10),
  SubpelVarianceParams(6, 6, &aom_highbd_8_sub_pixel_variance64x64_sse2, 8),
  SubpelVarianceParams(6, 5, &aom_highbd_8_sub_pixel_variance64x32_sse2, 8),
  SubpelVarianceParams(5, 6, &aom_highbd_8_sub_pixel_variance32x64_sse2, 8),
  SubpelVarianceParams(5, 5, &aom_highbd_8_sub_pixel_variance32x32_sse2, 8),
  SubpelVarianceParams(5, 4, &aom_highbd_8_sub_pixel_variance32x16_sse2, 8),
  SubpelVarianceParams(4, 5, &aom_highbd_8_sub_pixel_variance16x32_sse2, 8),
  SubpelVarianceParams(4, 4, &aom_highbd_8_sub_pixel_variance16x16_sse2, 8),
  SubpelVarianceParams(4, 3, &aom_highbd_8_sub_pixel_variance16x8_sse2, 8),
  SubpelVarianceParams(3, 4, &aom_highbd_8_sub_pixel_variance8x16_sse2, 8),
  SubpelVarianceParams(3, 3, &aom_highbd_8_sub_pixel_variance8x8_sse2, 8),
  SubpelVarianceParams(3, 2, &aom_highbd_8_sub_pixel_variance8x4_sse2, 8)
};

INSTANTIATE_TEST_CASE_P(SSE2, AvxHBDSubpelVarianceTest,
                        ::testing::ValuesIn(kArrayHBDSubpelVariance_sse2));

const SubpelAvgVarianceParams kArrayHBDSubpelAvgVariance_sse2[] = {
  SubpelAvgVarianceParams(6, 6, &aom_highbd_12_sub_pixel_avg_variance64x64_sse2,
                          12),
  SubpelAvgVarianceParams(6, 5, &aom_highbd_12_sub_pixel_avg_variance64x32_sse2,
                          12),
  SubpelAvgVarianceParams(5, 6, &aom_highbd_12_sub_pixel_avg_variance32x64_sse2,
                          12),
  SubpelAvgVarianceParams(5, 5, &aom_highbd_12_sub_pixel_avg_variance32x32_sse2,
                          12),
  SubpelAvgVarianceParams(5, 4, &aom_highbd_12_sub_pixel_avg_variance32x16_sse2,
                          12),
  SubpelAvgVarianceParams(4, 5, &aom_highbd_12_sub_pixel_avg_variance16x32_sse2,
                          12),
  SubpelAvgVarianceParams(4, 4, &aom_highbd_12_sub_pixel_avg_variance16x16_sse2,
                          12),
  SubpelAvgVarianceParams(4, 3, &aom_highbd_12_sub_pixel_avg_variance16x8_sse2,
                          12),
  SubpelAvgVarianceParams(3, 4, &aom_highbd_12_sub_pixel_avg_variance8x16_sse2,
                          12),
  SubpelAvgVarianceParams(3, 3, &aom_highbd_12_sub_pixel_avg_variance8x8_sse2,
                          12),
  SubpelAvgVarianceParams(3, 2, &aom_highbd_12_sub_pixel_avg_variance8x4_sse2,
                          12),
  SubpelAvgVarianceParams(6, 6, &aom_highbd_10_sub_pixel_avg_variance64x64_sse2,
                          10),
  SubpelAvgVarianceParams(6, 5, &aom_highbd_10_sub_pixel_avg_variance64x32_sse2,
                          10),
  SubpelAvgVarianceParams(5, 6, &aom_highbd_10_sub_pixel_avg_variance32x64_sse2,
                          10),
  SubpelAvgVarianceParams(5, 5, &aom_highbd_10_sub_pixel_avg_variance32x32_sse2,
                          10),
  SubpelAvgVarianceParams(5, 4, &aom_highbd_10_sub_pixel_avg_variance32x16_sse2,
                          10),
  SubpelAvgVarianceParams(4, 5, &aom_highbd_10_sub_pixel_avg_variance16x32_sse2,
                          10),
  SubpelAvgVarianceParams(4, 4, &aom_highbd_10_sub_pixel_avg_variance16x16_sse2,
                          10),
  SubpelAvgVarianceParams(4, 3, &aom_highbd_10_sub_pixel_avg_variance16x8_sse2,
                          10),
  SubpelAvgVarianceParams(3, 4, &aom_highbd_10_sub_pixel_avg_variance8x16_sse2,
                          10),
  SubpelAvgVarianceParams(3, 3, &aom_highbd_10_sub_pixel_avg_variance8x8_sse2,
                          10),
  SubpelAvgVarianceParams(3, 2, &aom_highbd_10_sub_pixel_avg_variance8x4_sse2,
                          10),
  SubpelAvgVarianceParams(6, 6, &aom_highbd_8_sub_pixel_avg_variance64x64_sse2,
                          8),
  SubpelAvgVarianceParams(6, 5, &aom_highbd_8_sub_pixel_avg_variance64x32_sse2,
                          8),
  SubpelAvgVarianceParams(5, 6, &aom_highbd_8_sub_pixel_avg_variance32x64_sse2,
                          8),
  SubpelAvgVarianceParams(5, 5, &aom_highbd_8_sub_pixel_avg_variance32x32_sse2,
                          8),
  SubpelAvgVarianceParams(5, 4, &aom_highbd_8_sub_pixel_avg_variance32x16_sse2,
                          8),
  SubpelAvgVarianceParams(4, 5, &aom_highbd_8_sub_pixel_avg_variance16x32_sse2,
                          8),
  SubpelAvgVarianceParams(4, 4, &aom_highbd_8_sub_pixel_avg_variance16x16_sse2,
                          8),
  SubpelAvgVarianceParams(4, 3, &aom_highbd_8_sub_pixel_avg_variance16x8_sse2,
                          8),
  SubpelAvgVarianceParams(3, 4, &aom_highbd_8_sub_pixel_avg_variance8x16_sse2,
                          8),
  SubpelAvgVarianceParams(3, 3, &aom_highbd_8_sub_pixel_avg_variance8x8_sse2,
                          8),
  SubpelAvgVarianceParams(3, 2, &aom_highbd_8_sub_pixel_avg_variance8x4_sse2, 8)
};

INSTANTIATE_TEST_CASE_P(SSE2, AvxHBDSubpelAvgVarianceTest,
                        ::testing::ValuesIn(kArrayHBDSubpelAvgVariance_sse2));
#endif  // HAVE_SSE2

#if HAVE_SSSE3
INSTANTIATE_TEST_CASE_P(
    SSSE3, AvxSubpelVarianceTest,
    ::testing::Values(
        SubpelVarianceParams(7, 7, &aom_sub_pixel_variance128x128_ssse3, 0),
        SubpelVarianceParams(7, 6, &aom_sub_pixel_variance128x64_ssse3, 0),
        SubpelVarianceParams(6, 7, &aom_sub_pixel_variance64x128_ssse3, 0),
        SubpelVarianceParams(6, 6, &aom_sub_pixel_variance64x64_ssse3, 0),
        SubpelVarianceParams(6, 5, &aom_sub_pixel_variance64x32_ssse3, 0),
        SubpelVarianceParams(5, 6, &aom_sub_pixel_variance32x64_ssse3, 0),
        SubpelVarianceParams(5, 5, &aom_sub_pixel_variance32x32_ssse3, 0),
        SubpelVarianceParams(5, 4, &aom_sub_pixel_variance32x16_ssse3, 0),
        SubpelVarianceParams(4, 5, &aom_sub_pixel_variance16x32_ssse3, 0),
        SubpelVarianceParams(4, 4, &aom_sub_pixel_variance16x16_ssse3, 0),
        SubpelVarianceParams(4, 3, &aom_sub_pixel_variance16x8_ssse3, 0),
        SubpelVarianceParams(3, 4, &aom_sub_pixel_variance8x16_ssse3, 0),
        SubpelVarianceParams(3, 3, &aom_sub_pixel_variance8x8_ssse3, 0),
        SubpelVarianceParams(3, 2, &aom_sub_pixel_variance8x4_ssse3, 0),
        SubpelVarianceParams(2, 3, &aom_sub_pixel_variance4x8_ssse3, 0),
        SubpelVarianceParams(2, 2, &aom_sub_pixel_variance4x4_ssse3, 0)));

INSTANTIATE_TEST_CASE_P(
    SSSE3, AvxSubpelAvgVarianceTest,
    ::testing::Values(
        SubpelAvgVarianceParams(7, 7, &aom_sub_pixel_avg_variance128x128_ssse3,
                                0),
        SubpelAvgVarianceParams(7, 6, &aom_sub_pixel_avg_variance128x64_ssse3,
                                0),
        SubpelAvgVarianceParams(6, 7, &aom_sub_pixel_avg_variance64x128_ssse3,
                                0),
        SubpelAvgVarianceParams(6, 6, &aom_sub_pixel_avg_variance64x64_ssse3,
                                0),
        SubpelAvgVarianceParams(6, 5, &aom_sub_pixel_avg_variance64x32_ssse3,
                                0),
        SubpelAvgVarianceParams(5, 6, &aom_sub_pixel_avg_variance32x64_ssse3,
                                0),
        SubpelAvgVarianceParams(5, 5, &aom_sub_pixel_avg_variance32x32_ssse3,
                                0),
        SubpelAvgVarianceParams(5, 4, &aom_sub_pixel_avg_variance32x16_ssse3,
                                0),
        SubpelAvgVarianceParams(4, 5, &aom_sub_pixel_avg_variance16x32_ssse3,
                                0),
        SubpelAvgVarianceParams(4, 4, &aom_sub_pixel_avg_variance16x16_ssse3,
                                0),
        SubpelAvgVarianceParams(4, 3, &aom_sub_pixel_avg_variance16x8_ssse3, 0),
        SubpelAvgVarianceParams(3, 4, &aom_sub_pixel_avg_variance8x16_ssse3, 0),
        SubpelAvgVarianceParams(3, 3, &aom_sub_pixel_avg_variance8x8_ssse3, 0),
        SubpelAvgVarianceParams(3, 2, &aom_sub_pixel_avg_variance8x4_ssse3, 0),
        SubpelAvgVarianceParams(2, 3, &aom_sub_pixel_avg_variance4x8_ssse3, 0),
        SubpelAvgVarianceParams(2, 2, &aom_sub_pixel_avg_variance4x4_ssse3,
                                0)));

INSTANTIATE_TEST_CASE_P(
    SSSE3, AvxJntSubpelAvgVarianceTest,
    ::testing::Values(
        JntSubpelAvgVarianceParams(6, 6,
                                   &aom_jnt_sub_pixel_avg_variance64x64_ssse3,
                                   0),
        JntSubpelAvgVarianceParams(6, 5,
                                   &aom_jnt_sub_pixel_avg_variance64x32_ssse3,
                                   0),
        JntSubpelAvgVarianceParams(5, 6,
                                   &aom_jnt_sub_pixel_avg_variance32x64_ssse3,
                                   0),
        JntSubpelAvgVarianceParams(5, 5,
                                   &aom_jnt_sub_pixel_avg_variance32x32_ssse3,
                                   0),
        JntSubpelAvgVarianceParams(5, 4,
                                   &aom_jnt_sub_pixel_avg_variance32x16_ssse3,
                                   0),
        JntSubpelAvgVarianceParams(4, 5,
                                   &aom_jnt_sub_pixel_avg_variance16x32_ssse3,
                                   0),
        JntSubpelAvgVarianceParams(4, 4,
                                   &aom_jnt_sub_pixel_avg_variance16x16_ssse3,
                                   0),
        JntSubpelAvgVarianceParams(4, 3,
                                   &aom_jnt_sub_pixel_avg_variance16x8_ssse3,
                                   0),
        JntSubpelAvgVarianceParams(3, 4,
                                   &aom_jnt_sub_pixel_avg_variance8x16_ssse3,
                                   0),
        JntSubpelAvgVarianceParams(3, 3,
                                   &aom_jnt_sub_pixel_avg_variance8x8_ssse3, 0),
        JntSubpelAvgVarianceParams(3, 2,
                                   &aom_jnt_sub_pixel_avg_variance8x4_ssse3, 0),
        JntSubpelAvgVarianceParams(2, 3,
                                   &aom_jnt_sub_pixel_avg_variance4x8_ssse3, 0),
        JntSubpelAvgVarianceParams(2, 2,
                                   &aom_jnt_sub_pixel_avg_variance4x4_ssse3,
                                   0)));
#endif  // HAVE_SSSE3

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(
    SSE4_1, AvxObmcSubpelVarianceTest,
    ::testing::Values(
        ObmcSubpelVarianceParams(7, 7,
                                 &aom_obmc_sub_pixel_variance128x128_sse4_1, 0),
        ObmcSubpelVarianceParams(7, 6,
                                 &aom_obmc_sub_pixel_variance128x64_sse4_1, 0),
        ObmcSubpelVarianceParams(6, 7,
                                 &aom_obmc_sub_pixel_variance64x128_sse4_1, 0),
        ObmcSubpelVarianceParams(6, 6, &aom_obmc_sub_pixel_variance64x64_sse4_1,
                                 0),
        ObmcSubpelVarianceParams(6, 5, &aom_obmc_sub_pixel_variance64x32_sse4_1,
                                 0),
        ObmcSubpelVarianceParams(5, 6, &aom_obmc_sub_pixel_variance32x64_sse4_1,
                                 0),
        ObmcSubpelVarianceParams(5, 5, &aom_obmc_sub_pixel_variance32x32_sse4_1,
                                 0),
        ObmcSubpelVarianceParams(5, 4, &aom_obmc_sub_pixel_variance32x16_sse4_1,
                                 0),
        ObmcSubpelVarianceParams(4, 5, &aom_obmc_sub_pixel_variance16x32_sse4_1,
                                 0),
        ObmcSubpelVarianceParams(4, 4, &aom_obmc_sub_pixel_variance16x16_sse4_1,
                                 0),
        ObmcSubpelVarianceParams(4, 3, &aom_obmc_sub_pixel_variance16x8_sse4_1,
                                 0),
        ObmcSubpelVarianceParams(3, 4, &aom_obmc_sub_pixel_variance8x16_sse4_1,
                                 0),
        ObmcSubpelVarianceParams(3, 3, &aom_obmc_sub_pixel_variance8x8_sse4_1,
                                 0),
        ObmcSubpelVarianceParams(3, 2, &aom_obmc_sub_pixel_variance8x4_sse4_1,
                                 0),
        ObmcSubpelVarianceParams(2, 3, &aom_obmc_sub_pixel_variance4x8_sse4_1,
                                 0),
        ObmcSubpelVarianceParams(2, 2, &aom_obmc_sub_pixel_variance4x4_sse4_1,
                                 0)));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2
INSTANTIATE_TEST_CASE_P(AVX2, AvxMseTest,
                        ::testing::Values(MseParams(4, 4, &aom_mse16x16_avx2)));

INSTANTIATE_TEST_CASE_P(
    AVX2, AvxVarianceTest,
    ::testing::Values(VarianceParams(7, 7, &aom_variance128x128_avx2),
                      VarianceParams(7, 6, &aom_variance128x64_avx2),
                      VarianceParams(6, 7, &aom_variance64x128_avx2),
                      VarianceParams(6, 6, &aom_variance64x64_avx2),
                      VarianceParams(6, 5, &aom_variance64x32_avx2),
                      VarianceParams(6, 4, &aom_variance64x16_avx2),
                      VarianceParams(5, 6, &aom_variance32x64_avx2),
                      VarianceParams(5, 5, &aom_variance32x32_avx2),
                      VarianceParams(5, 4, &aom_variance32x16_avx2),
                      VarianceParams(5, 3, &aom_variance32x8_avx2),
                      VarianceParams(4, 6, &aom_variance16x64_avx2),
                      VarianceParams(4, 5, &aom_variance16x32_avx2),
                      VarianceParams(4, 4, &aom_variance16x16_avx2),
                      VarianceParams(4, 3, &aom_variance16x8_avx2),
                      VarianceParams(4, 2, &aom_variance16x4_avx2)));

INSTANTIATE_TEST_CASE_P(
    AVX2, AvxSubpelVarianceTest,
    ::testing::Values(
        SubpelVarianceParams(7, 7, &aom_sub_pixel_variance128x128_avx2, 0),
        SubpelVarianceParams(7, 6, &aom_sub_pixel_variance128x64_avx2, 0),
        SubpelVarianceParams(6, 7, &aom_sub_pixel_variance64x128_avx2, 0),
        SubpelVarianceParams(6, 6, &aom_sub_pixel_variance64x64_avx2, 0),
        SubpelVarianceParams(6, 5, &aom_sub_pixel_variance64x32_avx2, 0),
        SubpelVarianceParams(5, 6, &aom_sub_pixel_variance32x64_avx2, 0),
        SubpelVarianceParams(5, 5, &aom_sub_pixel_variance32x32_avx2, 0),
        SubpelVarianceParams(5, 4, &aom_sub_pixel_variance32x16_avx2, 0)));

INSTANTIATE_TEST_CASE_P(
    AVX2, AvxSubpelAvgVarianceTest,
    ::testing::Values(
        SubpelAvgVarianceParams(7, 7, &aom_sub_pixel_avg_variance128x128_avx2,
                                0),
        SubpelAvgVarianceParams(7, 6, &aom_sub_pixel_avg_variance128x64_avx2,
                                0),
        SubpelAvgVarianceParams(6, 7, &aom_sub_pixel_avg_variance64x128_avx2,
                                0),
        SubpelAvgVarianceParams(6, 6, &aom_sub_pixel_avg_variance64x64_avx2, 0),
        SubpelAvgVarianceParams(6, 5, &aom_sub_pixel_avg_variance64x32_avx2, 0),
        SubpelAvgVarianceParams(5, 6, &aom_sub_pixel_avg_variance32x64_avx2, 0),
        SubpelAvgVarianceParams(5, 5, &aom_sub_pixel_avg_variance32x32_avx2, 0),
        SubpelAvgVarianceParams(5, 4, &aom_sub_pixel_avg_variance32x16_avx2,
                                0)));
#endif  // HAVE_AVX2

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(NEON, AvxSseTest,
                        ::testing::Values(SseParams(2, 2,
                                                    &aom_get4x4sse_cs_neon)));

INSTANTIATE_TEST_CASE_P(NEON, AvxMseTest,
                        ::testing::Values(MseParams(4, 4, &aom_mse16x16_neon)));

INSTANTIATE_TEST_CASE_P(
    NEON, AvxVarianceTest,
    ::testing::Values(VarianceParams(6, 6, &aom_variance64x64_neon),
                      VarianceParams(6, 5, &aom_variance64x32_neon),
                      VarianceParams(5, 6, &aom_variance32x64_neon),
                      VarianceParams(5, 5, &aom_variance32x32_neon),
                      VarianceParams(4, 4, &aom_variance16x16_neon),
                      VarianceParams(4, 3, &aom_variance16x8_neon),
                      VarianceParams(3, 4, &aom_variance8x16_neon),
                      VarianceParams(3, 3, &aom_variance8x8_neon)));

INSTANTIATE_TEST_CASE_P(
    NEON, AvxSubpelVarianceTest,
    ::testing::Values(
        SubpelVarianceParams(6, 6, &aom_sub_pixel_variance64x64_neon, 0),
        SubpelVarianceParams(5, 5, &aom_sub_pixel_variance32x32_neon, 0),
        SubpelVarianceParams(4, 4, &aom_sub_pixel_variance16x16_neon, 0),
        SubpelVarianceParams(3, 3, &aom_sub_pixel_variance8x8_neon, 0)));
#endif  // HAVE_NEON

#if HAVE_MSA
INSTANTIATE_TEST_CASE_P(MSA, SumOfSquaresTest,
                        ::testing::Values(aom_get_mb_ss_msa));

INSTANTIATE_TEST_CASE_P(MSA, AvxSseTest,
                        ::testing::Values(SseParams(2, 2,
                                                    &aom_get4x4sse_cs_msa)));

INSTANTIATE_TEST_CASE_P(MSA, AvxMseTest,
                        ::testing::Values(MseParams(4, 4, &aom_mse16x16_msa),
                                          MseParams(4, 3, &aom_mse16x8_msa),
                                          MseParams(3, 4, &aom_mse8x16_msa),
                                          MseParams(3, 3, &aom_mse8x8_msa)));

INSTANTIATE_TEST_CASE_P(
    MSA, AvxVarianceTest,
    ::testing::Values(VarianceParams(6, 6, &aom_variance64x64_msa),
                      VarianceParams(6, 5, &aom_variance64x32_msa),
                      VarianceParams(5, 6, &aom_variance32x64_msa),
                      VarianceParams(5, 5, &aom_variance32x32_msa),
                      VarianceParams(5, 4, &aom_variance32x16_msa),
                      VarianceParams(4, 5, &aom_variance16x32_msa),
                      VarianceParams(4, 4, &aom_variance16x16_msa),
                      VarianceParams(4, 3, &aom_variance16x8_msa),
                      VarianceParams(3, 4, &aom_variance8x16_msa),
                      VarianceParams(3, 3, &aom_variance8x8_msa),
                      VarianceParams(3, 2, &aom_variance8x4_msa),
                      VarianceParams(2, 3, &aom_variance4x8_msa),
                      VarianceParams(2, 2, &aom_variance4x4_msa)));

INSTANTIATE_TEST_CASE_P(
    MSA, AvxSubpelVarianceTest,
    ::testing::Values(
        SubpelVarianceParams(2, 2, &aom_sub_pixel_variance4x4_msa, 0),
        SubpelVarianceParams(2, 3, &aom_sub_pixel_variance4x8_msa, 0),
        SubpelVarianceParams(3, 2, &aom_sub_pixel_variance8x4_msa, 0),
        SubpelVarianceParams(3, 3, &aom_sub_pixel_variance8x8_msa, 0),
        SubpelVarianceParams(3, 4, &aom_sub_pixel_variance8x16_msa, 0),
        SubpelVarianceParams(4, 3, &aom_sub_pixel_variance16x8_msa, 0),
        SubpelVarianceParams(4, 4, &aom_sub_pixel_variance16x16_msa, 0),
        SubpelVarianceParams(4, 5, &aom_sub_pixel_variance16x32_msa, 0),
        SubpelVarianceParams(5, 4, &aom_sub_pixel_variance32x16_msa, 0),
        SubpelVarianceParams(5, 5, &aom_sub_pixel_variance32x32_msa, 0),
        SubpelVarianceParams(5, 6, &aom_sub_pixel_variance32x64_msa, 0),
        SubpelVarianceParams(6, 5, &aom_sub_pixel_variance64x32_msa, 0),
        SubpelVarianceParams(6, 6, &aom_sub_pixel_variance64x64_msa, 0)));

INSTANTIATE_TEST_CASE_P(
    MSA, AvxSubpelAvgVarianceTest,
    ::testing::Values(
        SubpelAvgVarianceParams(6, 6, &aom_sub_pixel_avg_variance64x64_msa, 0),
        SubpelAvgVarianceParams(6, 5, &aom_sub_pixel_avg_variance64x32_msa, 0),
        SubpelAvgVarianceParams(5, 6, &aom_sub_pixel_avg_variance32x64_msa, 0),
        SubpelAvgVarianceParams(5, 5, &aom_sub_pixel_avg_variance32x32_msa, 0),
        SubpelAvgVarianceParams(5, 4, &aom_sub_pixel_avg_variance32x16_msa, 0),
        SubpelAvgVarianceParams(4, 5, &aom_sub_pixel_avg_variance16x32_msa, 0),
        SubpelAvgVarianceParams(4, 4, &aom_sub_pixel_avg_variance16x16_msa, 0),
        SubpelAvgVarianceParams(4, 3, &aom_sub_pixel_avg_variance16x8_msa, 0),
        SubpelAvgVarianceParams(3, 4, &aom_sub_pixel_avg_variance8x16_msa, 0),
        SubpelAvgVarianceParams(3, 3, &aom_sub_pixel_avg_variance8x8_msa, 0),
        SubpelAvgVarianceParams(3, 2, &aom_sub_pixel_avg_variance8x4_msa, 0),
        SubpelAvgVarianceParams(2, 3, &aom_sub_pixel_avg_variance4x8_msa, 0),
        SubpelAvgVarianceParams(2, 2, &aom_sub_pixel_avg_variance4x4_msa, 0)));
#endif  // HAVE_MSA
}  // namespace
