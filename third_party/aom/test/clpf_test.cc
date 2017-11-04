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

typedef void (*clpf_block_t)(uint8_t *dst, const uint16_t *src, int dstride,
                             int sstride, int sizex, int sizey,
                             unsigned int strength, unsigned int bitdepth);

typedef std::tr1::tuple<clpf_block_t, clpf_block_t, int, int>
    clpf_block_param_t;

class CDEFClpfBlockTest : public ::testing::TestWithParam<clpf_block_param_t> {
 public:
  virtual ~CDEFClpfBlockTest() {}
  virtual void SetUp() {
    clpf = GET_PARAM(0);
    ref_clpf = GET_PARAM(1);
    sizex = GET_PARAM(2);
    sizey = GET_PARAM(3);
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  int sizex;
  int sizey;
  clpf_block_t clpf;
  clpf_block_t ref_clpf;
};

typedef CDEFClpfBlockTest CDEFClpfSpeedTest;

#if CONFIG_HIGHBITDEPTH
typedef void (*clpf_block_hbd_t)(uint16_t *dst, const uint16_t *src,
                                 int dstride, int sstride, int sizex, int sizey,
                                 unsigned int strength, unsigned int bitdepth);

typedef std::tr1::tuple<clpf_block_hbd_t, clpf_block_hbd_t, int, int>
    clpf_block_hbd_param_t;

class CDEFClpfBlockHbdTest
    : public ::testing::TestWithParam<clpf_block_hbd_param_t> {
 public:
  virtual ~CDEFClpfBlockHbdTest() {}
  virtual void SetUp() {
    clpf = GET_PARAM(0);
    ref_clpf = GET_PARAM(1);
    sizex = GET_PARAM(2);
    sizey = GET_PARAM(3);
  }

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  int sizex;
  int sizey;
  clpf_block_hbd_t clpf;
  clpf_block_hbd_t ref_clpf;
};

typedef CDEFClpfBlockHbdTest ClpfHbdSpeedTest;
#endif

template <typename pixel>
void test_clpf(int w, int h, unsigned int depth, unsigned int iterations,
               void (*clpf)(pixel *dst, const uint16_t *src, int dstride,
                            int sstride, int sizex, int sizey,
                            unsigned int strength, unsigned int bitdepth),
               void (*ref_clpf)(pixel *dst, const uint16_t *src, int dstride,
                                int sstride, int sizex, int sizey,
                                unsigned int strength, unsigned int bitdepth)) {
  const int size = 24;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, s[size * size]);
  DECLARE_ALIGNED(16, pixel, d[size * size]);
  DECLARE_ALIGNED(16, pixel, ref_d[size * size]);
  memset(ref_d, 0, size * size * sizeof(*ref_d));
  memset(d, 0, size * size * sizeof(*d));

  int error = 0, pos = 0, xpos = 8, ypos = 8;
  unsigned int strength = 0, bits, level, count, damp = 0, boundary = 0;

  assert(size >= w + 16 && size >= h + 16);
  assert(depth >= 8);

  // Test every combination of:
  // * Input with up to <depth> bits of noise
  // * Noise level around every value from 0 to (1<<depth)-1
  // * All strengths
  // * All dampings
  // * Boundaries
  // If clpf and ref_clpf are the same, we're just testing speed
  for (boundary = 0; boundary < 16; boundary++) {
    for (count = 0; count < iterations; count++) {
      for (level = 0; level < (1U << depth) && !error;
           level += (1 + 4 * !!boundary) << (depth - 8)) {
        for (bits = 1; bits <= depth && !error; bits++) {
          for (damp = 4 + depth - 8; damp < depth - 1 && !error; damp++) {
            for (int i = 0; i < size * size; i++)
              s[i] = clamp((rnd.Rand16() & ((1 << bits) - 1)) + level, 0,
                           (1 << depth) - 1);
            if (boundary) {
              if (boundary & 1) {  // Left
                for (int i = 0; i < size; i++)
                  for (int j = 0; j < xpos; j++)
                    s[i * size + j] = CDEF_VERY_LARGE;
              }
              if (boundary & 2) {  // Right
                for (int i = 0; i < size; i++)
                  for (int j = xpos + w; j < size; j++)
                    s[i * size + j] = CDEF_VERY_LARGE;
              }
              if (boundary & 4) {  // Above
                for (int i = 0; i < ypos; i++)
                  for (int j = 0; j < size; j++)
                    s[i * size + j] = CDEF_VERY_LARGE;
              }
              if (boundary & 8) {  // Below
                for (int i = ypos + h; i < size; i++)
                  for (int j = 0; j < size; j++)
                    s[i * size + j] = CDEF_VERY_LARGE;
              }
            }
            for (strength = depth - 8; strength < depth - 5 && !error;
                 strength += !error) {
              ref_clpf(ref_d + ypos * size + xpos, s + ypos * size + xpos, size,
                       size, w, h, 1 << strength, damp);
              if (clpf != ref_clpf)
                ASM_REGISTER_STATE_CHECK(clpf(d + ypos * size + xpos,
                                              s + ypos * size + xpos, size,
                                              size, w, h, 1 << strength, damp));
              if (ref_clpf != clpf) {
                for (pos = 0; pos < size * size && !error; pos++) {
                  error = ref_d[pos] != d[pos];
                }
              }
            }
          }
        }
      }
    }
  }

  pos--;
  EXPECT_EQ(0, error)
      << "Error: CDEFClpfBlockTest, SIMD and C mismatch." << std::endl
      << "First error at " << pos % size << "," << pos / size << " ("
      << (int16_t)ref_d[pos] << " != " << (int16_t)d[pos] << ") " << std::endl
      << "strength: " << (1 << strength) << std::endl
      << "damping: " << damp << std::endl
      << "depth: " << depth << std::endl
      << "boundary: " << boundary << std::endl
      << "w: " << w << std::endl
      << "h: " << h << std::endl
      << "A=" << (pos > 2 * size ? (int16_t)s[pos - 2 * size] : -1) << std::endl
      << "B=" << (pos > size ? (int16_t)s[pos - size] : -1) << std::endl
      << "C=" << (pos % size - 2 >= 0 ? (int16_t)s[pos - 2] : -1) << std::endl
      << "D=" << (pos % size - 1 >= 0 ? (int16_t)s[pos - 1] : -1) << std::endl
      << "X=" << (int16_t)s[pos] << std::endl
      << "E=" << (pos % size + 1 < size ? (int16_t)s[pos + 1] : -1) << std::endl
      << "F=" << (pos % size + 2 < size ? (int16_t)s[pos + 2] : -1) << std::endl
      << "G=" << (pos + size < size * size ? (int16_t)s[pos + size] : -1)
      << std::endl
      << "H="
      << (pos + 2 * size < size * size ? (int16_t)s[pos + 2 * size] : -1)
      << std::endl;
}

template <typename pixel>
void test_clpf_speed(int w, int h, unsigned int depth, unsigned int iterations,
                     void (*clpf)(pixel *dst, const uint16_t *src, int dstride,
                                  int sstride, int sizex, int sizey,
                                  unsigned int strength, unsigned int bitdepth),
                     void (*ref_clpf)(pixel *dst, const uint16_t *src,
                                      int dstride, int sstride, int sizex,
                                      int sizey, unsigned int strength,
                                      unsigned int bitdepth)) {
  aom_usec_timer ref_timer;
  aom_usec_timer timer;

  aom_usec_timer_start(&ref_timer);
  test_clpf(w, h, depth, iterations, ref_clpf, ref_clpf);
  aom_usec_timer_mark(&ref_timer);
  int ref_elapsed_time = (int)aom_usec_timer_elapsed(&ref_timer);

  aom_usec_timer_start(&timer);
  test_clpf(w, h, depth, iterations, clpf, clpf);
  aom_usec_timer_mark(&timer);
  int elapsed_time = (int)aom_usec_timer_elapsed(&timer);

#if 0
  std::cout << "[          ] C time = " << ref_elapsed_time / 1000
            << " ms, SIMD time = " << elapsed_time / 1000 << " ms" << std::endl;
#endif

  EXPECT_GT(ref_elapsed_time, elapsed_time)
      << "Error: CDEFClpfSpeedTest, SIMD slower than C." << std::endl
      << "C time: " << ref_elapsed_time << " us" << std::endl
      << "SIMD time: " << elapsed_time << " us" << std::endl;
}

TEST_P(CDEFClpfBlockTest, TestSIMDNoMismatch) {
  test_clpf(sizex, sizey, 8, 1, clpf, ref_clpf);
}

TEST_P(CDEFClpfSpeedTest, DISABLED_TestSpeed) {
  test_clpf_speed(sizex, sizey, 8, 16, clpf, ref_clpf);
}

#if CONFIG_HIGHBITDEPTH
TEST_P(CDEFClpfBlockHbdTest, TestSIMDNoMismatch) {
  test_clpf(sizex, sizey, 12, 1, clpf, ref_clpf);
}

TEST_P(ClpfHbdSpeedTest, DISABLED_TestSpeed) {
  test_clpf_speed(sizex, sizey, 12, 4, clpf, ref_clpf);
}
#endif

using std::tr1::make_tuple;

// VS compiling for 32 bit targets does not support vector types in
// structs as arguments, which makes the v256 type of the intrinsics
// hard to support, so optimizations for this target are disabled.
#if defined(_WIN64) || !defined(_MSC_VER) || defined(__clang__)
// Test all supported architectures and block sizes
#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, CDEFClpfBlockTest,
    ::testing::Values(
        make_tuple(&aom_clpf_block_sse2, &aom_clpf_block_c, 8, 8),
        make_tuple(&aom_clpf_block_sse2, &aom_clpf_block_c, 8, 4),
        make_tuple(&aom_clpf_block_sse2, &aom_clpf_block_c, 4, 8),
        make_tuple(&aom_clpf_block_sse2, &aom_clpf_block_c, 4, 4),
        make_tuple(&aom_clpf_hblock_sse2, &aom_clpf_hblock_c, 8, 8),
        make_tuple(&aom_clpf_hblock_sse2, &aom_clpf_hblock_c, 8, 4),
        make_tuple(&aom_clpf_hblock_sse2, &aom_clpf_hblock_c, 4, 8),
        make_tuple(&aom_clpf_hblock_sse2, &aom_clpf_hblock_c, 4, 4)));
#endif

#if HAVE_SSSE3
INSTANTIATE_TEST_CASE_P(
    SSSE3, CDEFClpfBlockTest,
    ::testing::Values(
        make_tuple(&aom_clpf_block_ssse3, &aom_clpf_block_c, 8, 8),
        make_tuple(&aom_clpf_block_ssse3, &aom_clpf_block_c, 8, 4),
        make_tuple(&aom_clpf_block_ssse3, &aom_clpf_block_c, 4, 8),
        make_tuple(&aom_clpf_block_ssse3, &aom_clpf_block_c, 4, 4),
        make_tuple(&aom_clpf_hblock_ssse3, &aom_clpf_hblock_c, 8, 8),
        make_tuple(&aom_clpf_hblock_ssse3, &aom_clpf_hblock_c, 8, 4),
        make_tuple(&aom_clpf_hblock_ssse3, &aom_clpf_hblock_c, 4, 8),
        make_tuple(&aom_clpf_hblock_ssse3, &aom_clpf_hblock_c, 4, 4)));
#endif

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(
    SSE4_1, CDEFClpfBlockTest,
    ::testing::Values(
        make_tuple(&aom_clpf_block_sse4_1, &aom_clpf_block_c, 8, 8),
        make_tuple(&aom_clpf_block_sse4_1, &aom_clpf_block_c, 8, 4),
        make_tuple(&aom_clpf_block_sse4_1, &aom_clpf_block_c, 4, 8),
        make_tuple(&aom_clpf_block_sse4_1, &aom_clpf_block_c, 4, 4),
        make_tuple(&aom_clpf_hblock_sse4_1, &aom_clpf_hblock_c, 8, 8),
        make_tuple(&aom_clpf_hblock_sse4_1, &aom_clpf_hblock_c, 8, 4),
        make_tuple(&aom_clpf_hblock_sse4_1, &aom_clpf_hblock_c, 4, 8),
        make_tuple(&aom_clpf_hblock_sse4_1, &aom_clpf_hblock_c, 4, 4)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, CDEFClpfBlockTest,
    ::testing::Values(
        make_tuple(&aom_clpf_block_neon, &aom_clpf_block_c, 8, 8),
        make_tuple(&aom_clpf_block_neon, &aom_clpf_block_c, 8, 4),
        make_tuple(&aom_clpf_block_neon, &aom_clpf_block_c, 4, 8),
        make_tuple(&aom_clpf_block_neon, &aom_clpf_block_c, 4, 4),
        make_tuple(&aom_clpf_hblock_neon, &aom_clpf_hblock_c, 8, 8),
        make_tuple(&aom_clpf_hblock_neon, &aom_clpf_hblock_c, 8, 4),
        make_tuple(&aom_clpf_hblock_neon, &aom_clpf_hblock_c, 4, 8),
        make_tuple(&aom_clpf_hblock_neon, &aom_clpf_hblock_c, 4, 4)));
#endif

#if CONFIG_HIGHBITDEPTH
#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, CDEFClpfBlockHbdTest,
    ::testing::Values(
        make_tuple(&aom_clpf_block_hbd_sse2, &aom_clpf_block_hbd_c, 8, 8),
        make_tuple(&aom_clpf_block_hbd_sse2, &aom_clpf_block_hbd_c, 8, 4),
        make_tuple(&aom_clpf_block_hbd_sse2, &aom_clpf_block_hbd_c, 4, 8),
        make_tuple(&aom_clpf_block_hbd_sse2, &aom_clpf_block_hbd_c, 4, 4),
        make_tuple(&aom_clpf_hblock_hbd_sse2, &aom_clpf_hblock_hbd_c, 8, 8),
        make_tuple(&aom_clpf_hblock_hbd_sse2, &aom_clpf_hblock_hbd_c, 8, 4),
        make_tuple(&aom_clpf_hblock_hbd_sse2, &aom_clpf_hblock_hbd_c, 4, 8),
        make_tuple(&aom_clpf_hblock_hbd_sse2, &aom_clpf_hblock_hbd_c, 4, 4)));
#endif

#if HAVE_SSSE3
INSTANTIATE_TEST_CASE_P(
    SSSE3, CDEFClpfBlockHbdTest,
    ::testing::Values(
        make_tuple(&aom_clpf_block_hbd_ssse3, &aom_clpf_block_hbd_c, 8, 8),
        make_tuple(&aom_clpf_block_hbd_ssse3, &aom_clpf_block_hbd_c, 8, 4),
        make_tuple(&aom_clpf_block_hbd_ssse3, &aom_clpf_block_hbd_c, 4, 8),
        make_tuple(&aom_clpf_block_hbd_ssse3, &aom_clpf_block_hbd_c, 4, 4),
        make_tuple(&aom_clpf_hblock_hbd_ssse3, &aom_clpf_hblock_hbd_c, 8, 8),
        make_tuple(&aom_clpf_hblock_hbd_ssse3, &aom_clpf_hblock_hbd_c, 8, 4),
        make_tuple(&aom_clpf_hblock_hbd_ssse3, &aom_clpf_hblock_hbd_c, 4, 8),
        make_tuple(&aom_clpf_hblock_hbd_ssse3, &aom_clpf_hblock_hbd_c, 4, 4)));
#endif

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(
    SSE4_1, CDEFClpfBlockHbdTest,
    ::testing::Values(
        make_tuple(&aom_clpf_block_hbd_sse4_1, &aom_clpf_block_hbd_c, 8, 8),
        make_tuple(&aom_clpf_block_hbd_sse4_1, &aom_clpf_block_hbd_c, 8, 4),
        make_tuple(&aom_clpf_block_hbd_sse4_1, &aom_clpf_block_hbd_c, 4, 8),
        make_tuple(&aom_clpf_block_hbd_sse4_1, &aom_clpf_block_hbd_c, 4, 4),
        make_tuple(&aom_clpf_hblock_hbd_sse4_1, &aom_clpf_hblock_hbd_c, 8, 8),
        make_tuple(&aom_clpf_hblock_hbd_sse4_1, &aom_clpf_hblock_hbd_c, 8, 4),
        make_tuple(&aom_clpf_hblock_hbd_sse4_1, &aom_clpf_hblock_hbd_c, 4, 8),
        make_tuple(&aom_clpf_hblock_hbd_sse4_1, &aom_clpf_hblock_hbd_c, 4, 4)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, CDEFClpfBlockHbdTest,
    ::testing::Values(
        make_tuple(&aom_clpf_block_hbd_neon, &aom_clpf_block_hbd_c, 8, 8),
        make_tuple(&aom_clpf_block_hbd_neon, &aom_clpf_block_hbd_c, 8, 4),
        make_tuple(&aom_clpf_block_hbd_neon, &aom_clpf_block_hbd_c, 4, 8),
        make_tuple(&aom_clpf_block_hbd_neon, &aom_clpf_block_hbd_c, 4, 4),
        make_tuple(&aom_clpf_hblock_hbd_neon, &aom_clpf_hblock_hbd_c, 8, 8),
        make_tuple(&aom_clpf_hblock_hbd_neon, &aom_clpf_hblock_hbd_c, 8, 4),
        make_tuple(&aom_clpf_hblock_hbd_neon, &aom_clpf_hblock_hbd_c, 4, 8),
        make_tuple(&aom_clpf_hblock_hbd_neon, &aom_clpf_hblock_hbd_c, 4, 4)));
#endif
#endif  // CONFIG_HIGHBITDEPTH

// Test speed for all supported architectures
#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, CDEFClpfSpeedTest,
    ::testing::Values(make_tuple(&aom_clpf_block_sse2, &aom_clpf_block_c, 8, 8),
                      make_tuple(&aom_clpf_hblock_sse2, &aom_clpf_hblock_c, 8,
                                 8)));
#endif

#if HAVE_SSSE3
INSTANTIATE_TEST_CASE_P(SSSE3, CDEFClpfSpeedTest,
                        ::testing::Values(make_tuple(&aom_clpf_block_ssse3,
                                                     &aom_clpf_block_c, 8, 8),
                                          make_tuple(&aom_clpf_hblock_ssse3,
                                                     &aom_clpf_hblock_c, 8,
                                                     8)));
#endif

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(SSE4_1, CDEFClpfSpeedTest,
                        ::testing::Values(make_tuple(&aom_clpf_block_sse4_1,
                                                     &aom_clpf_block_c, 8, 8),
                                          make_tuple(&aom_clpf_hblock_sse4_1,
                                                     &aom_clpf_hblock_c, 8,
                                                     8)));

#endif

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, CDEFClpfSpeedTest,
    ::testing::Values(make_tuple(&aom_clpf_block_neon, &aom_clpf_block_c, 8, 8),
                      make_tuple(&aom_clpf_hblock_neon, &aom_clpf_hblock_c, 8,
                                 8)));
#endif

#if CONFIG_HIGHBITDEPTH
#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, ClpfHbdSpeedTest,
    ::testing::Values(
        make_tuple(&aom_clpf_block_hbd_sse2, &aom_clpf_block_hbd_c, 8, 8),
        make_tuple(&aom_clpf_hblock_hbd_sse2, &aom_clpf_hblock_hbd_c, 8, 8)));
#endif

#if HAVE_SSSE3
INSTANTIATE_TEST_CASE_P(
    SSSE3, ClpfHbdSpeedTest,
    ::testing::Values(
        make_tuple(&aom_clpf_block_hbd_ssse3, &aom_clpf_block_hbd_c, 8, 8),
        make_tuple(&aom_clpf_hblock_hbd_ssse3, &aom_clpf_hblock_hbd_c, 8, 8)));
#endif

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(
    SSE4_1, ClpfHbdSpeedTest,
    ::testing::Values(
        make_tuple(&aom_clpf_block_hbd_sse4_1, &aom_clpf_block_hbd_c, 8, 8),
        make_tuple(&aom_clpf_hblock_hbd_sse4_1, &aom_clpf_hblock_hbd_c, 8, 8)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, ClpfHbdSpeedTest,
    ::testing::Values(
        make_tuple(&aom_clpf_block_hbd_neon, &aom_clpf_block_hbd_c, 8, 8),
        make_tuple(&aom_clpf_hblock_hbd_neon, &aom_clpf_hblock_hbd_c, 8, 8)));
#endif
#endif  // CONFIG_HIGHBITDEPTH
#endif  // defined(_WIN64) || !defined(_MSC_VER)

}  // namespace
