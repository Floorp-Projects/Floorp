/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <vector>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "test/function_equivalence_test.h"
#include "test/register_state_check.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "av1/encoder/pickrst.h"

#define MAX_WIENER_BLOCK 384
#define MAX_DATA_BLOCK (MAX_WIENER_BLOCK + WIENER_WIN)
using libaom_test::FunctionEquivalenceTest;

namespace {

static void compute_stats_win_opt_c(int wiener_win, const uint8_t *dgd,
                                    const uint8_t *src, int h_start, int h_end,
                                    int v_start, int v_end, int dgd_stride,
                                    int src_stride, double *M, double *H) {
  ASSERT_TRUE(wiener_win == WIENER_WIN || wiener_win == WIENER_WIN_CHROMA);
  int i, j, k, l, m, n;
  const int pixel_count = (h_end - h_start) * (v_end - v_start);
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin = (wiener_win >> 1);
  const double avg =
      find_average(dgd, h_start, h_end, v_start, v_end, dgd_stride);

  std::vector<std::vector<int64_t> > M_int(wiener_win,
                                           std::vector<int64_t>(wiener_win, 0));
  std::vector<std::vector<int64_t> > H_int(
      wiener_win * wiener_win, std::vector<int64_t>(wiener_win * 8, 0));
  std::vector<std::vector<int32_t> > sumY(wiener_win,
                                          std::vector<int32_t>(wiener_win, 0));
  int32_t sumX = 0;
  const uint8_t *dgd_win = dgd - wiener_halfwin * dgd_stride - wiener_halfwin;

  for (i = v_start; i < v_end; i++) {
    for (j = h_start; j < h_end; j += 2) {
      const uint8_t X1 = src[i * src_stride + j];
      const uint8_t X2 = src[i * src_stride + j + 1];
      sumX += X1 + X2;

      const uint8_t *dgd_ij = dgd_win + i * dgd_stride + j;
      for (k = 0; k < wiener_win; k++) {
        for (l = 0; l < wiener_win; l++) {
          const uint8_t *dgd_ijkl = dgd_ij + k * dgd_stride + l;
          int64_t *H_int_temp = &H_int[(l * wiener_win + k)][0];
          const uint8_t D1 = dgd_ijkl[0];
          const uint8_t D2 = dgd_ijkl[1];
          sumY[k][l] += D1 + D2;
          M_int[l][k] += D1 * X1 + D2 * X2;
          for (m = 0; m < wiener_win; m++) {
            for (n = 0; n < wiener_win; n++) {
              H_int_temp[m * 8 + n] += D1 * dgd_ij[n + dgd_stride * m] +
                                       D2 * dgd_ij[n + dgd_stride * m + 1];
            }
          }
        }
      }
    }
  }

  const double avg_square_sum = avg * avg * pixel_count;
  for (k = 0; k < wiener_win; k++) {
    for (l = 0; l < wiener_win; l++) {
      M[l * wiener_win + k] =
          M_int[l][k] + avg_square_sum - avg * (sumX + sumY[k][l]);
      for (m = 0; m < wiener_win; m++) {
        for (n = 0; n < wiener_win; n++) {
          H[(l * wiener_win + k) * wiener_win2 + m * wiener_win + n] =
              H_int[(l * wiener_win + k)][n * 8 + m] + avg_square_sum -
              avg * (sumY[k][l] + sumY[n][m]);
        }
      }
    }
  }
}

void compute_stats_opt_c(int wiener_win, const uint8_t *dgd, const uint8_t *src,
                         int h_start, int h_end, int v_start, int v_end,
                         int dgd_stride, int src_stride, double *M, double *H) {
  if (wiener_win == WIENER_WIN || wiener_win == WIENER_WIN_CHROMA) {
    compute_stats_win_opt_c(wiener_win, dgd, src, h_start, h_end, v_start,
                            v_end, dgd_stride, src_stride, M, H);
  } else {
    av1_compute_stats_c(wiener_win, dgd, src, h_start, h_end, v_start, v_end,
                        dgd_stride, src_stride, M, H);
  }
}

static const int kIterations = 100;
static const double min_error = (double)(0.01);
typedef void (*compute_stats_Func)(int wiener_win, const uint8_t *dgd,
                                   const uint8_t *src, int h_start, int h_end,
                                   int v_start, int v_end, int dgd_stride,
                                   int src_stride, double *M, double *H);

typedef libaom_test::FuncParam<compute_stats_Func> TestFuncs;

////////////////////////////////////////////////////////////////////////////////
// 8 bit
////////////////////////////////////////////////////////////////////////////////

typedef ::testing::tuple<const compute_stats_Func> WienerTestParam;

class WienerTest : public ::testing::TestWithParam<WienerTestParam> {
 public:
  virtual void SetUp() { target_func_ = GET_PARAM(0); }
  void runWienerTest(const int32_t wiener_win, int32_t run_times);
  void runWienerTest_ExtremeValues(const int32_t wiener_win);

 private:
  compute_stats_Func target_func_;
  ACMRandom rng_;
};

void WienerTest::runWienerTest(const int32_t wiener_win, int32_t run_times) {
  const int32_t wiener_halfwin = wiener_win >> 1;
  const int32_t wiener_win2 = wiener_win * wiener_win;
  DECLARE_ALIGNED(32, uint8_t, dgd_buf[MAX_DATA_BLOCK * MAX_DATA_BLOCK]);
  DECLARE_ALIGNED(32, uint8_t, src_buf[MAX_DATA_BLOCK * MAX_DATA_BLOCK]);
  DECLARE_ALIGNED(32, double, M_ref[WIENER_WIN2]);
  DECLARE_ALIGNED(32, double, H_ref[WIENER_WIN2 * WIENER_WIN2]);
  DECLARE_ALIGNED(32, double, M_test[WIENER_WIN2]);
  DECLARE_ALIGNED(32, double, H_test[WIENER_WIN2 * WIENER_WIN2]);
  const int h_start = ((rng_.Rand16() % (MAX_WIENER_BLOCK / 2)) & (~7));
  int h_end =
      run_times != 1 ? 256 : ((rng_.Rand16() % MAX_WIENER_BLOCK) & (~7)) + 8;
  const int v_start = ((rng_.Rand16() % (MAX_WIENER_BLOCK / 2)) & (~7));
  int v_end =
      run_times != 1 ? 256 : ((rng_.Rand16() % MAX_WIENER_BLOCK) & (~7)) + 8;
  const int dgd_stride = h_end;
  const int src_stride = MAX_DATA_BLOCK;
  const int iters = run_times == 1 ? kIterations : 2;
  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_buf[i] = rng_.Rand8();
      src_buf[i] = rng_.Rand8();
    }
    uint8_t *dgd = dgd_buf + wiener_halfwin * MAX_DATA_BLOCK + wiener_halfwin;
    uint8_t *src = src_buf;

    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      av1_compute_stats_c(wiener_win, dgd, src, h_start, h_end, v_start, v_end,
                          dgd_stride, src_stride, M_ref, H_ref);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      target_func_(wiener_win, dgd, src, h_start, h_end, v_start, v_end,
                   dgd_stride, src_stride, M_test, H_test);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    if (run_times > 10) {
      printf("win %d %3dx%-3d:%7.2f/%7.2fns", wiener_win, h_end, v_end, time1,
             time2);
      printf("(%3.2f)\n", time1 / time2);
    }
    int failed = 0;
    for (int i = 0; i < wiener_win2; ++i) {
      if (fabs(M_ref[i] - M_test[i]) > min_error) {
        failed = 1;
        printf("win %d M iter %d [%4d] ref %6.0f test %6.0f \n", wiener_win,
               iter, i, M_ref[i], M_test[i]);
        break;
      }
    }
    // ASSERT_EQ(failed, 0);
    for (int i = 0; i < wiener_win2 * wiener_win2; ++i) {
      if (fabs(H_ref[i] - H_test[i]) > min_error) {
        failed = 1;
        printf("win %d H iter %d [%4d] ref %6.0f test %6.0f \n", wiener_win,
               iter, i, H_ref[i], H_test[i]);
        break;
      }
    }
    ASSERT_EQ(failed, 0);
  }
}

void WienerTest::runWienerTest_ExtremeValues(const int32_t wiener_win) {
  const int32_t wiener_halfwin = wiener_win >> 1;
  const int32_t wiener_win2 = wiener_win * wiener_win;
  DECLARE_ALIGNED(32, uint8_t, dgd_buf[MAX_DATA_BLOCK * MAX_DATA_BLOCK]);
  DECLARE_ALIGNED(32, uint8_t, src_buf[MAX_DATA_BLOCK * MAX_DATA_BLOCK]);
  DECLARE_ALIGNED(32, double, M_ref[WIENER_WIN2]);
  DECLARE_ALIGNED(32, double, H_ref[WIENER_WIN2 * WIENER_WIN2]);
  DECLARE_ALIGNED(32, double, M_test[WIENER_WIN2]);
  DECLARE_ALIGNED(32, double, H_test[WIENER_WIN2 * WIENER_WIN2]);
  const int h_start = 16;
  const int h_end = MAX_WIENER_BLOCK;
  const int v_start = 16;
  const int v_end = MAX_WIENER_BLOCK;
  const int dgd_stride = h_end;
  const int src_stride = MAX_DATA_BLOCK;
  const int iters = 1;
  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_buf[i] = 255;
      src_buf[i] = 255;
    }
    uint8_t *dgd = dgd_buf + wiener_halfwin * MAX_DATA_BLOCK + wiener_halfwin;
    uint8_t *src = src_buf;

    av1_compute_stats_c(wiener_win, dgd, src, h_start, h_end, v_start, v_end,
                        dgd_stride, src_stride, M_ref, H_ref);

    target_func_(wiener_win, dgd, src, h_start, h_end, v_start, v_end,
                 dgd_stride, src_stride, M_test, H_test);

    int failed = 0;
    for (int i = 0; i < wiener_win2; ++i) {
      if (fabs(M_ref[i] - M_test[i]) > min_error) {
        failed = 1;
        printf("win %d M iter %d [%4d] ref %6.0f test %6.0f \n", wiener_win,
               iter, i, M_ref[i], M_test[i]);
        break;
      }
    }
    // ASSERT_EQ(failed, 0);
    for (int i = 0; i < wiener_win2 * wiener_win2; ++i) {
      if (fabs(H_ref[i] - H_test[i]) > min_error) {
        failed = 1;
        printf("win %d H iter %d [%4d] ref %6.0f test %6.0f \n", wiener_win,
               iter, i, H_ref[i], H_test[i]);
        break;
      }
    }
    ASSERT_EQ(failed, 0);
  }
}

TEST_P(WienerTest, RandomValues) {
  runWienerTest(WIENER_WIN, 1);
  runWienerTest(WIENER_WIN_CHROMA, 1);
}

TEST_P(WienerTest, ExtremeValues) {
  runWienerTest_ExtremeValues(WIENER_WIN);
  runWienerTest_ExtremeValues(WIENER_WIN_CHROMA);
}

TEST_P(WienerTest, DISABLED_Speed) {
  runWienerTest(WIENER_WIN, 200);
  runWienerTest(WIENER_WIN_CHROMA, 200);
}

INSTANTIATE_TEST_CASE_P(C, WienerTest, ::testing::Values(compute_stats_opt_c));

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(SSE4_1, WienerTest,
                        ::testing::Values(av1_compute_stats_sse4_1));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2

INSTANTIATE_TEST_CASE_P(AVX2, WienerTest,
                        ::testing::Values(av1_compute_stats_avx2));
#endif  // HAVE_AVX2

}  // namespace
