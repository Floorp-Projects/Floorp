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

#include "./aom_config.h"
#include "./av1_rtcd.h"
#include "aom_ports/aom_timer.h"
#include "av1/common/cdef_block.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"

using libaom_test::ACMRandom;

namespace {

typedef std::tr1::tuple<cdef_direction_func, cdef_direction_func, int>
    dering_dir_param_t;

class CDEFDeringDirTest : public ::testing::TestWithParam<dering_dir_param_t> {
 public:
  virtual ~CDEFDeringDirTest() {}
  virtual void SetUp() {
    dering = GET_PARAM(0);
    ref_dering = GET_PARAM(1);
    bsize = GET_PARAM(2);
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  int bsize;
  cdef_direction_func dering;
  cdef_direction_func ref_dering;
};

typedef CDEFDeringDirTest CDEFDeringSpeedTest;

void test_dering(int bsize, int iterations, cdef_direction_func dering,
                 cdef_direction_func ref_dering) {
  const int size = 8;
  const int ysize = size + 2 * CDEF_VBORDER;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, s[ysize * CDEF_BSTRIDE]);
  DECLARE_ALIGNED(16, static uint16_t, d[size * size]);
  DECLARE_ALIGNED(16, static uint16_t, ref_d[size * size]);
  memset(ref_d, 0, sizeof(ref_d));
  memset(d, 0, sizeof(d));

  int error = 0, threshold = 0, dir;
  int boundary, damping, depth, bits, level, count,
      errdepth = 0, errthreshold = 0, errboundary = 0, errdamping = 0;
  unsigned int pos = 0;

  for (boundary = 0; boundary < 16; boundary++) {
    for (depth = 8; depth <= 12; depth += 2) {
      for (damping = 5 + depth - 8; damping < 7 + depth - 8; damping++) {
        for (count = 0; count < iterations; count++) {
          for (level = 0; level < (1 << depth) && !error;
               level += (1 + 4 * !!boundary) << (depth - 8)) {
            for (bits = 1; bits <= depth && !error; bits++) {
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
                for (threshold = 0; threshold < 64 << (depth - 8) && !error;
                     threshold += (1 + 4 * !!boundary) << (depth - 8)) {
                  ref_dering(ref_d, size,
                             s + CDEF_HBORDER + CDEF_VBORDER * CDEF_BSTRIDE,
                             threshold, dir, damping);
                  // If dering and ref_dering are the same, we're just testing
                  // speed
                  if (dering != ref_dering)
                    ASM_REGISTER_STATE_CHECK(dering(
                        d, size, s + CDEF_HBORDER + CDEF_VBORDER * CDEF_BSTRIDE,
                        threshold, dir, damping));
                  if (ref_dering != dering) {
                    for (pos = 0; pos < sizeof(d) / sizeof(*d) && !error;
                         pos++) {
                      error = ref_d[pos] != d[pos];
                      errdepth = depth;
                      errthreshold = threshold;
                      errboundary = boundary;
                      errdamping = damping;
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
  EXPECT_EQ(0, error) << "Error: CDEFDeringDirTest, SIMD and C mismatch."
                      << std::endl
                      << "First error at " << pos % size << "," << pos / size
                      << " (" << (int16_t)ref_d[pos] << " : " << (int16_t)d[pos]
                      << ") " << std::endl
                      << "threshold: " << errthreshold << std::endl
                      << "damping: " << errdamping << std::endl
                      << "depth: " << errdepth << std::endl
                      << "size: " << bsize << std::endl
                      << "boundary: " << errboundary << std::endl
                      << std::endl;
}

void test_dering_speed(int bsize, int iterations, cdef_direction_func dering,
                       cdef_direction_func ref_dering) {
  aom_usec_timer ref_timer;
  aom_usec_timer timer;

  aom_usec_timer_start(&ref_timer);
  test_dering(bsize, iterations, ref_dering, ref_dering);
  aom_usec_timer_mark(&ref_timer);
  int ref_elapsed_time = (int)aom_usec_timer_elapsed(&ref_timer);

  aom_usec_timer_start(&timer);
  test_dering(bsize, iterations, dering, dering);
  aom_usec_timer_mark(&timer);
  int elapsed_time = (int)aom_usec_timer_elapsed(&timer);

#if 0
  std::cout << "[          ] C time = " << ref_elapsed_time / 1000
            << " ms, SIMD time = " << elapsed_time / 1000 << " ms" << std::endl;
#endif

  EXPECT_GT(ref_elapsed_time, elapsed_time)
      << "Error: CDEFDeringSpeedTest, SIMD slower than C." << std::endl
      << "C time: " << ref_elapsed_time << " us" << std::endl
      << "SIMD time: " << elapsed_time << " us" << std::endl;
}

typedef int (*find_dir_t)(const uint16_t *img, int stride, int32_t *var,
                          int coeff_shift);

typedef std::tr1::tuple<find_dir_t, find_dir_t> find_dir_param_t;

class CDEFDeringFindDirTest
    : public ::testing::TestWithParam<find_dir_param_t> {
 public:
  virtual ~CDEFDeringFindDirTest() {}
  virtual void SetUp() {
    finddir = GET_PARAM(0);
    ref_finddir = GET_PARAM(1);
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  find_dir_t finddir;
  find_dir_t ref_finddir;
};

typedef CDEFDeringFindDirTest CDEFDeringFindDirSpeedTest;

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

  EXPECT_EQ(0, error) << "Error: CDEFDeringFindDirTest, SIMD and C mismatch."
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

#if 0
  std::cout << "[          ] C time = " << ref_elapsed_time / 1000
            << " ms, SIMD time = " << elapsed_time / 1000 << " ms" << std::endl;
#endif

  EXPECT_GT(ref_elapsed_time, elapsed_time)
      << "Error: CDEFDeringFindDirSpeedTest, SIMD slower than C." << std::endl
      << "C time: " << ref_elapsed_time << " us" << std::endl
      << "SIMD time: " << elapsed_time << " us" << std::endl;
}

TEST_P(CDEFDeringDirTest, TestSIMDNoMismatch) {
  test_dering(bsize, 1, dering, ref_dering);
}

TEST_P(CDEFDeringSpeedTest, DISABLED_TestSpeed) {
  test_dering_speed(bsize, 4, dering, ref_dering);
}

TEST_P(CDEFDeringFindDirTest, TestSIMDNoMismatch) {
  test_finddir(finddir, ref_finddir);
}

TEST_P(CDEFDeringFindDirSpeedTest, DISABLED_TestSpeed) {
  test_finddir_speed(finddir, ref_finddir);
}

using std::tr1::make_tuple;

// VS compiling for 32 bit targets does not support vector types in
// structs as arguments, which makes the v256 type of the intrinsics
// hard to support, so optimizations for this target are disabled.
#if defined(_WIN64) || !defined(_MSC_VER) || defined(__clang__)
#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(SSE2, CDEFDeringDirTest,
                        ::testing::Values(make_tuple(&cdef_direction_4x4_sse2,
                                                     &cdef_direction_4x4_c, 4),
                                          make_tuple(&cdef_direction_8x8_sse2,
                                                     &cdef_direction_8x8_c,
                                                     8)));
INSTANTIATE_TEST_CASE_P(SSE2, CDEFDeringFindDirTest,
                        ::testing::Values(make_tuple(&cdef_find_dir_sse2,
                                                     &cdef_find_dir_c)));
#endif
#if HAVE_SSSE3
INSTANTIATE_TEST_CASE_P(SSSE3, CDEFDeringDirTest,
                        ::testing::Values(make_tuple(&cdef_direction_4x4_ssse3,
                                                     &cdef_direction_4x4_c, 4),
                                          make_tuple(&cdef_direction_8x8_ssse3,
                                                     &cdef_direction_8x8_c,
                                                     8)));
INSTANTIATE_TEST_CASE_P(SSSE3, CDEFDeringFindDirTest,
                        ::testing::Values(make_tuple(&cdef_find_dir_ssse3,
                                                     &cdef_find_dir_c)));
#endif

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(SSE4_1, CDEFDeringDirTest,
                        ::testing::Values(make_tuple(&cdef_direction_4x4_sse4_1,
                                                     &cdef_direction_4x4_c, 4),
                                          make_tuple(&cdef_direction_8x8_sse4_1,
                                                     &cdef_direction_8x8_c,
                                                     8)));
INSTANTIATE_TEST_CASE_P(SSE4_1, CDEFDeringFindDirTest,
                        ::testing::Values(make_tuple(&cdef_find_dir_sse4_1,
                                                     &cdef_find_dir_c)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(NEON, CDEFDeringDirTest,
                        ::testing::Values(make_tuple(&cdef_direction_4x4_neon,
                                                     &cdef_direction_4x4_c, 4),
                                          make_tuple(&cdef_direction_8x8_neon,
                                                     &cdef_direction_8x8_c,
                                                     8)));
INSTANTIATE_TEST_CASE_P(NEON, CDEFDeringFindDirTest,
                        ::testing::Values(make_tuple(&cdef_find_dir_neon,
                                                     &cdef_find_dir_c)));
#endif

// Test speed for all supported architectures
#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(SSE2, CDEFDeringSpeedTest,
                        ::testing::Values(make_tuple(&cdef_direction_4x4_sse2,
                                                     &cdef_direction_4x4_c, 4),
                                          make_tuple(&cdef_direction_8x8_sse2,
                                                     &cdef_direction_8x8_c,
                                                     8)));
INSTANTIATE_TEST_CASE_P(SSE2, CDEFDeringFindDirSpeedTest,
                        ::testing::Values(make_tuple(&cdef_find_dir_sse2,
                                                     &cdef_find_dir_c)));
#endif

#if HAVE_SSSE3
INSTANTIATE_TEST_CASE_P(SSSE3, CDEFDeringSpeedTest,
                        ::testing::Values(make_tuple(&cdef_direction_4x4_ssse3,
                                                     &cdef_direction_4x4_c, 4),
                                          make_tuple(&cdef_direction_8x8_ssse3,
                                                     &cdef_direction_8x8_c,
                                                     8)));
INSTANTIATE_TEST_CASE_P(SSSE3, CDEFDeringFindDirSpeedTest,
                        ::testing::Values(make_tuple(&cdef_find_dir_ssse3,
                                                     &cdef_find_dir_c)));
#endif

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(SSE4_1, CDEFDeringSpeedTest,
                        ::testing::Values(make_tuple(&cdef_direction_4x4_sse4_1,
                                                     &cdef_direction_4x4_c, 4),
                                          make_tuple(&cdef_direction_8x8_sse4_1,
                                                     &cdef_direction_8x8_c,
                                                     8)));
INSTANTIATE_TEST_CASE_P(SSE4_1, CDEFDeringFindDirSpeedTest,
                        ::testing::Values(make_tuple(&cdef_find_dir_sse4_1,
                                                     &cdef_find_dir_c)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(NEON, CDEFDeringSpeedTest,
                        ::testing::Values(make_tuple(&cdef_direction_4x4_neon,
                                                     &cdef_direction_4x4_c, 4),
                                          make_tuple(&cdef_direction_8x8_neon,
                                                     &cdef_direction_8x8_c,
                                                     8)));
INSTANTIATE_TEST_CASE_P(NEON, CDEFDeringFindDirSpeedTest,
                        ::testing::Values(make_tuple(&cdef_find_dir_neon,
                                                     &cdef_find_dir_c)));
#endif

#endif  // defined(_WIN64) || !defined(_MSC_VER)
}  // namespace
