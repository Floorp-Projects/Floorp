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
#include <string>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_ports/aom_timer.h"
#include "av1/common/cdef_block.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"

using libaom_test::ACMRandom;

namespace {

typedef ::testing::tuple<cdef_filter_block_func, cdef_filter_block_func,
                         BLOCK_SIZE, int, int>
    cdef_dir_param_t;

class CDEFBlockTest : public ::testing::TestWithParam<cdef_dir_param_t> {
 public:
  virtual ~CDEFBlockTest() {}
  virtual void SetUp() {
    cdef = GET_PARAM(0);
    ref_cdef = GET_PARAM(1);
    bsize = GET_PARAM(2);
    boundary = GET_PARAM(3);
    depth = GET_PARAM(4);
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  int bsize;
  int boundary;
  int depth;
  cdef_filter_block_func cdef;
  cdef_filter_block_func ref_cdef;
};

typedef CDEFBlockTest CDEFSpeedTest;

void test_cdef(int bsize, int iterations, cdef_filter_block_func cdef,
               cdef_filter_block_func ref_cdef, int boundary, int depth) {
  const int size = 8;
  const int ysize = size + 2 * CDEF_VBORDER;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, s[ysize * CDEF_BSTRIDE]);
  DECLARE_ALIGNED(16, static uint16_t, d[size * size]);
  DECLARE_ALIGNED(16, static uint16_t, ref_d[size * size]);
  memset(ref_d, 0, sizeof(ref_d));
  memset(d, 0, sizeof(d));

  int error = 0, pristrength = 0, secstrength, dir;
  int pridamping, secdamping, bits, level, count,
      errdepth = 0, errpristrength = 0, errsecstrength = 0, errboundary = 0,
      errpridamping = 0, errsecdamping = 0;
  unsigned int pos = 0;

  const unsigned int max_pos = size * size >> static_cast<int>(depth == 8);
  for (pridamping = 3 + depth - 8; pridamping < 7 - 3 * !!boundary + depth - 8;
       pridamping++) {
    for (secdamping = 3 + depth - 8;
         secdamping < 7 - 3 * !!boundary + depth - 8; secdamping++) {
      for (count = 0; count < iterations; count++) {
        for (level = 0; level < (1 << depth) && !error;
             level += (2 + 6 * !!boundary) << (depth - 8)) {
          for (bits = 1; bits <= depth && !error; bits += 1 + 3 * !!boundary) {
            for (unsigned int i = 0; i < sizeof(s) / sizeof(*s); i++)
              s[i] = clamp((rnd.Rand16() & ((1 << bits) - 1)) + level, 0,
                           (1 << depth) - 1);
            if (boundary) {
              if (boundary & 1) {  // Left
                for (int i = 0; i < ysize; i++)
                  for (int j = 0; j < CDEF_HBORDER; j++)
                    s[i * CDEF_BSTRIDE + j] = CDEF_VERY_LARGE;
              }
              if (boundary & 2) {  // Right
                for (int i = 0; i < ysize; i++)
                  for (int j = CDEF_HBORDER + size; j < CDEF_BSTRIDE; j++)
                    s[i * CDEF_BSTRIDE + j] = CDEF_VERY_LARGE;
              }
              if (boundary & 4) {  // Above
                for (int i = 0; i < CDEF_VBORDER; i++)
                  for (int j = 0; j < CDEF_BSTRIDE; j++)
                    s[i * CDEF_BSTRIDE + j] = CDEF_VERY_LARGE;
              }
              if (boundary & 8) {  // Below
                for (int i = CDEF_VBORDER + size; i < ysize; i++)
                  for (int j = 0; j < CDEF_BSTRIDE; j++)
                    s[i * CDEF_BSTRIDE + j] = CDEF_VERY_LARGE;
              }
            }
            for (dir = 0; dir < 8; dir++) {
              for (pristrength = 0; pristrength <= 19 << (depth - 8) && !error;
                   pristrength += (1 + 4 * !!boundary) << (depth - 8)) {
                if (pristrength == 16) pristrength = 19;
                for (secstrength = 0; secstrength <= 4 << (depth - 8) && !error;
                     secstrength += 1 << (depth - 8)) {
                  if (secstrength == 3 << (depth - 8)) continue;
                  ref_cdef(depth == 8 ? (uint8_t *)ref_d : 0, ref_d, size,
                           s + CDEF_HBORDER + CDEF_VBORDER * CDEF_BSTRIDE,
                           pristrength, secstrength, dir, pridamping,
                           secdamping, bsize, (1 << depth) - 1, depth - 8);
                  // If cdef and ref_cdef are the same, we're just testing
                  // speed
                  if (cdef != ref_cdef)
                    ASM_REGISTER_STATE_CHECK(
                        cdef(depth == 8 ? (uint8_t *)d : 0, d, size,
                             s + CDEF_HBORDER + CDEF_VBORDER * CDEF_BSTRIDE,
                             pristrength, secstrength, dir, pridamping,
                             secdamping, bsize, (1 << depth) - 1, depth - 8));
                  if (ref_cdef != cdef) {
                    for (pos = 0; pos < max_pos && !error; pos++) {
                      error = ref_d[pos] != d[pos];
                      errdepth = depth;
                      errpristrength = pristrength;
                      errsecstrength = secstrength;
                      errboundary = boundary;
                      errpridamping = pridamping;
                      errsecdamping = secdamping;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  pos--;
  EXPECT_EQ(0, error) << "Error: CDEFBlockTest, SIMD and C mismatch."
                      << std::endl
                      << "First error at " << pos % size << "," << pos / size
                      << " (" << (int16_t)ref_d[pos] << " : " << (int16_t)d[pos]
                      << ") " << std::endl
                      << "pristrength: " << errpristrength << std::endl
                      << "pridamping: " << errpridamping << std::endl
                      << "secstrength: " << errsecstrength << std::endl
                      << "secdamping: " << errsecdamping << std::endl
                      << "depth: " << errdepth << std::endl
                      << "size: " << bsize << std::endl
                      << "boundary: " << errboundary << std::endl
                      << std::endl;
}

void test_cdef_speed(int bsize, int iterations, cdef_filter_block_func cdef,
                     cdef_filter_block_func ref_cdef, int boundary, int depth) {
  aom_usec_timer ref_timer;
  aom_usec_timer timer;

  aom_usec_timer_start(&ref_timer);
  test_cdef(bsize, iterations, ref_cdef, ref_cdef, boundary, depth);
  aom_usec_timer_mark(&ref_timer);
  int ref_elapsed_time = (int)aom_usec_timer_elapsed(&ref_timer);

  aom_usec_timer_start(&timer);
  test_cdef(bsize, iterations, cdef, cdef, boundary, depth);
  aom_usec_timer_mark(&timer);
  int elapsed_time = (int)aom_usec_timer_elapsed(&timer);

  EXPECT_GT(ref_elapsed_time, elapsed_time)
      << "Error: CDEFSpeedTest, SIMD slower than C." << std::endl
      << "C time: " << ref_elapsed_time << " us" << std::endl
      << "SIMD time: " << elapsed_time << " us" << std::endl;
}

typedef int (*find_dir_t)(const uint16_t *img, int stride, int32_t *var,
                          int coeff_shift);

typedef ::testing::tuple<find_dir_t, find_dir_t> find_dir_param_t;

class CDEFFindDirTest : public ::testing::TestWithParam<find_dir_param_t> {
 public:
  virtual ~CDEFFindDirTest() {}
  virtual void SetUp() {
    finddir = GET_PARAM(0);
    ref_finddir = GET_PARAM(1);
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  find_dir_t finddir;
  find_dir_t ref_finddir;
};

typedef CDEFFindDirTest CDEFFindDirSpeedTest;

void test_finddir(int (*finddir)(const uint16_t *img, int stride, int32_t *var,
                                 int coeff_shift),
                  int (*ref_finddir)(const uint16_t *img, int stride,
                                     int32_t *var, int coeff_shift)) {
  const int size = 8;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, s[size * size]);

  int error = 0;
  int depth, bits, level, count, errdepth = 0;
  int ref_res = 0, res = 0;
  int32_t ref_var = 0, var = 0;

  for (depth = 8; depth <= 12 && !error; depth += 2) {
    for (count = 0; count < 512 && !error; count++) {
      for (level = 0; level < (1 << depth) && !error;
           level += 1 << (depth - 8)) {
        for (bits = 1; bits <= depth && !error; bits++) {
          for (unsigned int i = 0; i < sizeof(s) / sizeof(*s); i++)
            s[i] = clamp((rnd.Rand16() & ((1 << bits) - 1)) + level, 0,
                         (1 << depth) - 1);
          for (int c = 0; c < 1 + 9 * (finddir == ref_finddir); c++)
            ref_res = ref_finddir(s, size, &ref_var, depth - 8);
          if (finddir != ref_finddir)
            ASM_REGISTER_STATE_CHECK(res = finddir(s, size, &var, depth - 8));
          if (ref_finddir != finddir) {
            if (res != ref_res || var != ref_var) error = 1;
            errdepth = depth;
          }
        }
      }
    }
  }

  EXPECT_EQ(0, error) << "Error: CDEFFindDirTest, SIMD and C mismatch."
                      << std::endl
                      << "return: " << res << " : " << ref_res << std::endl
                      << "var: " << var << " : " << ref_var << std::endl
                      << "depth: " << errdepth << std::endl
                      << std::endl;
}

void test_finddir_speed(int (*finddir)(const uint16_t *img, int stride,
                                       int32_t *var, int coeff_shift),
                        int (*ref_finddir)(const uint16_t *img, int stride,
                                           int32_t *var, int coeff_shift)) {
  aom_usec_timer ref_timer;
  aom_usec_timer timer;

  aom_usec_timer_start(&ref_timer);
  test_finddir(ref_finddir, ref_finddir);
  aom_usec_timer_mark(&ref_timer);
  int ref_elapsed_time = (int)aom_usec_timer_elapsed(&ref_timer);

  aom_usec_timer_start(&timer);
  test_finddir(finddir, finddir);
  aom_usec_timer_mark(&timer);
  int elapsed_time = (int)aom_usec_timer_elapsed(&timer);

  EXPECT_GT(ref_elapsed_time, elapsed_time)
      << "Error: CDEFFindDirSpeedTest, SIMD slower than C." << std::endl
      << "C time: " << ref_elapsed_time << " us" << std::endl
      << "SIMD time: " << elapsed_time << " us" << std::endl;
}

TEST_P(CDEFBlockTest, TestSIMDNoMismatch) {
  test_cdef(bsize, 1, cdef, ref_cdef, boundary, depth);
}

TEST_P(CDEFSpeedTest, DISABLED_TestSpeed) {
  test_cdef_speed(bsize, 4, cdef, ref_cdef, boundary, depth);
}

TEST_P(CDEFFindDirTest, TestSIMDNoMismatch) {
  test_finddir(finddir, ref_finddir);
}

TEST_P(CDEFFindDirSpeedTest, DISABLED_TestSpeed) {
  test_finddir_speed(finddir, ref_finddir);
}

using ::testing::make_tuple;

// VS compiling for 32 bit targets does not support vector types in
// structs as arguments, which makes the v256 type of the intrinsics
// hard to support, so optimizations for this target are disabled.
#if defined(_WIN64) || !defined(_MSC_VER) || defined(__clang__)
#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, CDEFBlockTest,
    ::testing::Combine(::testing::Values(&cdef_filter_block_sse2),
                       ::testing::Values(&cdef_filter_block_c),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(8, 13, 2)));
INSTANTIATE_TEST_CASE_P(SSE2, CDEFFindDirTest,
                        ::testing::Values(make_tuple(&cdef_find_dir_sse2,
                                                     &cdef_find_dir_c)));
#endif
#if HAVE_SSSE3
INSTANTIATE_TEST_CASE_P(
    SSSE3, CDEFBlockTest,
    ::testing::Combine(::testing::Values(&cdef_filter_block_ssse3),
                       ::testing::Values(&cdef_filter_block_c),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(8, 13, 2)));
INSTANTIATE_TEST_CASE_P(SSSE3, CDEFFindDirTest,
                        ::testing::Values(make_tuple(&cdef_find_dir_ssse3,
                                                     &cdef_find_dir_c)));
#endif

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(
    SSE4_1, CDEFBlockTest,
    ::testing::Combine(::testing::Values(&cdef_filter_block_sse4_1),
                       ::testing::Values(&cdef_filter_block_c),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(8, 13, 2)));
INSTANTIATE_TEST_CASE_P(SSE4_1, CDEFFindDirTest,
                        ::testing::Values(make_tuple(&cdef_find_dir_sse4_1,
                                                     &cdef_find_dir_c)));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_CASE_P(
    AVX2, CDEFBlockTest,
    ::testing::Combine(::testing::Values(&cdef_filter_block_avx2),
                       ::testing::Values(&cdef_filter_block_c),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(8, 13, 2)));
INSTANTIATE_TEST_CASE_P(AVX2, CDEFFindDirTest,
                        ::testing::Values(make_tuple(&cdef_find_dir_avx2,
                                                     &cdef_find_dir_c)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, CDEFBlockTest,
    ::testing::Combine(::testing::Values(&cdef_filter_block_neon),
                       ::testing::Values(&cdef_filter_block_c),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(8, 13, 2)));
INSTANTIATE_TEST_CASE_P(NEON, CDEFFindDirTest,
                        ::testing::Values(make_tuple(&cdef_find_dir_neon,
                                                     &cdef_find_dir_c)));
#endif

// Test speed for all supported architectures
#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, CDEFSpeedTest,
    ::testing::Combine(::testing::Values(&cdef_filter_block_sse2),
                       ::testing::Values(&cdef_filter_block_c),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(8, 13, 2)));
INSTANTIATE_TEST_CASE_P(SSE2, CDEFFindDirSpeedTest,
                        ::testing::Values(make_tuple(&cdef_find_dir_sse2,
                                                     &cdef_find_dir_c)));
#endif

#if HAVE_SSSE3
INSTANTIATE_TEST_CASE_P(
    SSSE3, CDEFSpeedTest,
    ::testing::Combine(::testing::Values(&cdef_filter_block_ssse3),
                       ::testing::Values(&cdef_filter_block_c),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(8, 13, 2)));
INSTANTIATE_TEST_CASE_P(SSSE3, CDEFFindDirSpeedTest,
                        ::testing::Values(make_tuple(&cdef_find_dir_ssse3,
                                                     &cdef_find_dir_c)));
#endif

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(
    SSE4_1, CDEFSpeedTest,
    ::testing::Combine(::testing::Values(&cdef_filter_block_sse4_1),
                       ::testing::Values(&cdef_filter_block_c),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(8, 13, 2)));
INSTANTIATE_TEST_CASE_P(SSE4_1, CDEFFindDirSpeedTest,
                        ::testing::Values(make_tuple(&cdef_find_dir_sse4_1,
                                                     &cdef_find_dir_c)));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_CASE_P(
    AVX2, CDEFSpeedTest,
    ::testing::Combine(::testing::Values(&cdef_filter_block_avx2),
                       ::testing::Values(&cdef_filter_block_c),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(8, 13, 2)));
INSTANTIATE_TEST_CASE_P(AVX2, CDEFFindDirSpeedTest,
                        ::testing::Values(make_tuple(&cdef_find_dir_avx2,
                                                     &cdef_find_dir_c)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, CDEFSpeedTest,
    ::testing::Combine(::testing::Values(&cdef_filter_block_neon),
                       ::testing::Values(&cdef_filter_block_c),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(8, 13, 2)));
INSTANTIATE_TEST_CASE_P(NEON, CDEFFindDirSpeedTest,
                        ::testing::Values(make_tuple(&cdef_find_dir_neon,
                                                     &cdef_find_dir_c)));
#endif

#endif  // defined(_WIN64) || !defined(_MSC_VER)
}  // namespace
