/*
 *  Copyright (c) 2016 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./vpx_dsp_rtcd.h"

#include "test/acm_random.h"
#include "test/register_state_check.h"

namespace {

using ::libvpx_test::ACMRandom;

typedef void (*HadamardFunc)(const int16_t *a, int a_stride, int16_t *b);

void hadamard_loop(const int16_t *a, int a_stride, int16_t *out) {
  int16_t b[8];
  for (int i = 0; i < 8; i += 2) {
    b[i + 0] = a[i * a_stride] + a[(i + 1) * a_stride];
    b[i + 1] = a[i * a_stride] - a[(i + 1) * a_stride];
  }
  int16_t c[8];
  for (int i = 0; i < 8; i += 4) {
    c[i + 0] = b[i + 0] + b[i + 2];
    c[i + 1] = b[i + 1] + b[i + 3];
    c[i + 2] = b[i + 0] - b[i + 2];
    c[i + 3] = b[i + 1] - b[i + 3];
  }
  out[0] = c[0] + c[4];
  out[7] = c[1] + c[5];
  out[3] = c[2] + c[6];
  out[4] = c[3] + c[7];
  out[2] = c[0] - c[4];
  out[6] = c[1] - c[5];
  out[1] = c[2] - c[6];
  out[5] = c[3] - c[7];
}

void reference_hadamard8x8(const int16_t *a, int a_stride, int16_t *b) {
  int16_t buf[64];
  for (int i = 0; i < 8; ++i) {
    hadamard_loop(a + i, a_stride, buf + i * 8);
  }

  for (int i = 0; i < 8; ++i) {
    hadamard_loop(buf + i, 8, b + i * 8);
  }
}

void reference_hadamard16x16(const int16_t *a, int a_stride, int16_t *b) {
  /* The source is a 16x16 block. The destination is rearranged to 8x32.
   * Input is 9 bit. */
  reference_hadamard8x8(a + 0 + 0 * a_stride, a_stride, b + 0);
  reference_hadamard8x8(a + 8 + 0 * a_stride, a_stride, b + 64);
  reference_hadamard8x8(a + 0 + 8 * a_stride, a_stride, b + 128);
  reference_hadamard8x8(a + 8 + 8 * a_stride, a_stride, b + 192);

  /* Overlay the 8x8 blocks and combine. */
  for (int i = 0; i < 64; ++i) {
    /* 8x8 steps the range up to 15 bits. */
    const int16_t a0 = b[0];
    const int16_t a1 = b[64];
    const int16_t a2 = b[128];
    const int16_t a3 = b[192];

    /* Prevent the result from escaping int16_t. */
    const int16_t b0 = (a0 + a1) >> 1;
    const int16_t b1 = (a0 - a1) >> 1;
    const int16_t b2 = (a2 + a3) >> 1;
    const int16_t b3 = (a2 - a3) >> 1;

    /* Store a 16 bit value. */
    b[0] = b0 + b2;
    b[64] = b1 + b3;
    b[128] = b0 - b2;
    b[192] = b1 - b3;

    ++b;
  }
}

class HadamardTestBase : public ::testing::TestWithParam<HadamardFunc> {
 public:
  virtual void SetUp() {
    h_func_ = GetParam();
    rnd_.Reset(ACMRandom::DeterministicSeed());
  }

 protected:
  HadamardFunc h_func_;
  ACMRandom rnd_;
};

class Hadamard8x8Test : public HadamardTestBase {};

TEST_P(Hadamard8x8Test, CompareReferenceRandom) {
  DECLARE_ALIGNED(16, int16_t, a[64]);
  DECLARE_ALIGNED(16, int16_t, b[64]);
  int16_t b_ref[64];
  for (int i = 0; i < 64; ++i) {
    a[i] = rnd_.Rand9Signed();
  }
  memset(b, 0, sizeof(b));
  memset(b_ref, 0, sizeof(b_ref));

  reference_hadamard8x8(a, 8, b_ref);
  ASM_REGISTER_STATE_CHECK(h_func_(a, 8, b));

  // The order of the output is not important. Sort before checking.
  std::sort(b, b + 64);
  std::sort(b_ref, b_ref + 64);
  EXPECT_EQ(0, memcmp(b, b_ref, sizeof(b)));
}

TEST_P(Hadamard8x8Test, VaryStride) {
  DECLARE_ALIGNED(16, int16_t, a[64 * 8]);
  DECLARE_ALIGNED(16, int16_t, b[64]);
  int16_t b_ref[64];
  for (int i = 0; i < 64 * 8; ++i) {
    a[i] = rnd_.Rand9Signed();
  }

  for (int i = 8; i < 64; i += 8) {
    memset(b, 0, sizeof(b));
    memset(b_ref, 0, sizeof(b_ref));

    reference_hadamard8x8(a, i, b_ref);
    ASM_REGISTER_STATE_CHECK(h_func_(a, i, b));

    // The order of the output is not important. Sort before checking.
    std::sort(b, b + 64);
    std::sort(b_ref, b_ref + 64);
    EXPECT_EQ(0, memcmp(b, b_ref, sizeof(b)));
  }
}

INSTANTIATE_TEST_CASE_P(C, Hadamard8x8Test,
                        ::testing::Values(&vpx_hadamard_8x8_c));

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(SSE2, Hadamard8x8Test,
                        ::testing::Values(&vpx_hadamard_8x8_sse2));
#endif  // HAVE_SSE2

#if HAVE_SSSE3 && ARCH_X86_64
INSTANTIATE_TEST_CASE_P(SSSE3, Hadamard8x8Test,
                        ::testing::Values(&vpx_hadamard_8x8_ssse3));
#endif  // HAVE_SSSE3 && ARCH_X86_64

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(NEON, Hadamard8x8Test,
                        ::testing::Values(&vpx_hadamard_8x8_neon));
#endif  // HAVE_NEON

class Hadamard16x16Test : public HadamardTestBase {};

TEST_P(Hadamard16x16Test, CompareReferenceRandom) {
  DECLARE_ALIGNED(16, int16_t, a[16 * 16]);
  DECLARE_ALIGNED(16, int16_t, b[16 * 16]);
  int16_t b_ref[16 * 16];
  for (int i = 0; i < 16 * 16; ++i) {
    a[i] = rnd_.Rand9Signed();
  }
  memset(b, 0, sizeof(b));
  memset(b_ref, 0, sizeof(b_ref));

  reference_hadamard16x16(a, 16, b_ref);
  ASM_REGISTER_STATE_CHECK(h_func_(a, 16, b));

  // The order of the output is not important. Sort before checking.
  std::sort(b, b + 16 * 16);
  std::sort(b_ref, b_ref + 16 * 16);
  EXPECT_EQ(0, memcmp(b, b_ref, sizeof(b)));
}

TEST_P(Hadamard16x16Test, VaryStride) {
  DECLARE_ALIGNED(16, int16_t, a[16 * 16 * 8]);
  DECLARE_ALIGNED(16, int16_t, b[16 * 16]);
  int16_t b_ref[16 * 16];
  for (int i = 0; i < 16 * 16 * 8; ++i) {
    a[i] = rnd_.Rand9Signed();
  }

  for (int i = 8; i < 64; i += 8) {
    memset(b, 0, sizeof(b));
    memset(b_ref, 0, sizeof(b_ref));

    reference_hadamard16x16(a, i, b_ref);
    ASM_REGISTER_STATE_CHECK(h_func_(a, i, b));

    // The order of the output is not important. Sort before checking.
    std::sort(b, b + 16 * 16);
    std::sort(b_ref, b_ref + 16 * 16);
    EXPECT_EQ(0, memcmp(b, b_ref, sizeof(b)));
  }
}

INSTANTIATE_TEST_CASE_P(C, Hadamard16x16Test,
                        ::testing::Values(&vpx_hadamard_16x16_c));

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(SSE2, Hadamard16x16Test,
                        ::testing::Values(&vpx_hadamard_16x16_sse2));
#endif  // HAVE_SSE2

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(NEON, Hadamard16x16Test,
                        ::testing::Values(&vpx_hadamard_16x16_neon));
#endif  // HAVE_NEON
}  // namespace
