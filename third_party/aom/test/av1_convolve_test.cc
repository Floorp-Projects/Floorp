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

#include <algorithm>
#include <vector>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./av1_rtcd.h"
#include "./aom_dsp_rtcd.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/mem.h"
#include "av1/common/filter.h"
#include "av1/common/convolve.h"
#include "test/acm_random.h"
#include "test/util.h"

using libaom_test::ACMRandom;

namespace {
using std::tr1::tuple;
static void filter_block1d_horiz_c(const uint8_t *src_ptr, int src_stride,
                                   const int16_t *filter, int tap,
                                   uint8_t *dst_ptr, int dst_stride, int w,
                                   int h) {
  src_ptr -= tap / 2 - 1;
  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      int sum = 0;
      for (int i = 0; i < tap; ++i) {
        sum += src_ptr[c + i] * filter[i];
      }
      dst_ptr[c] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
    }
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  }
}

static void filter_block1d_vert_c(const uint8_t *src_ptr, int src_stride,
                                  const int16_t *filter, int tap,
                                  uint8_t *dst_ptr, int dst_stride, int w,
                                  int h) {
  src_ptr -= (tap / 2 - 1) * src_stride;
  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      int sum = 0;
      for (int i = 0; i < tap; ++i) {
        sum += src_ptr[c + i * src_stride] * filter[i];
      }
      dst_ptr[c] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
    }
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  }
}

static int match(const uint8_t *out, int out_stride, const uint8_t *ref_out,
                 int ref_out_stride, int w, int h) {
  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      if (out[r * out_stride + c] != ref_out[r * ref_out_stride + c]) return 0;
    }
  }
  return 1;
}

typedef void (*ConvolveFunc)(const uint8_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h,
                             const InterpFilterParams filter_params,
                             const int subpel_q4, int step_q4,
                             ConvolveParams *conv_params);

struct ConvolveFunctions {
  ConvolveFunctions(ConvolveFunc hf, ConvolveFunc vf) : hf_(hf), vf_(vf) {}
  ConvolveFunc hf_;
  ConvolveFunc vf_;
};

typedef tuple<ConvolveFunctions *, InterpFilter /*filter_x*/,
              InterpFilter /*filter_y*/>
    ConvolveParam;

class Av1ConvolveTest : public ::testing::TestWithParam<ConvolveParam> {
 public:
  virtual void SetUp() {
    rnd_(ACMRandom::DeterministicSeed());
    cfs_ = GET_PARAM(0);
    interp_filter_ls_[0] = GET_PARAM(2);
    interp_filter_ls_[2] = interp_filter_ls_[0];
    interp_filter_ls_[1] = GET_PARAM(1);
    interp_filter_ls_[3] = interp_filter_ls_[1];
  }
  virtual void TearDown() {
    while (buf_ls_.size() > 0) {
      uint8_t *buf = buf_ls_.back();
      aom_free(buf);
      buf_ls_.pop_back();
    }
  }
  virtual uint8_t *add_input(int w, int h, int *stride) {
    uint8_t *buf =
        reinterpret_cast<uint8_t *>(aom_memalign(kDataAlignment, kBufferSize));
    buf_ls_.push_back(buf);
    *stride = w + MAX_FILTER_TAP - 1;
    int offset = MAX_FILTER_TAP / 2 - 1;
    for (int r = 0; r < h + MAX_FILTER_TAP - 1; ++r) {
      for (int c = 0; c < w + MAX_FILTER_TAP - 1; ++c) {
        buf[r * (*stride) + c] = rnd_.Rand8();
      }
    }
    return buf + offset * (*stride) + offset;
  }
  virtual uint8_t *add_output(int w, int /*h*/, int *stride) {
    uint8_t *buf =
        reinterpret_cast<uint8_t *>(aom_memalign(kDataAlignment, kBufferSize));
    buf_ls_.push_back(buf);
    *stride = w;
    return buf;
  }
  virtual void random_init_buf(uint8_t *buf, int w, int h, int stride) {
    for (int r = 0; r < h; ++r) {
      for (int c = 0; c < w; ++c) {
        buf[r * stride + c] = rnd_.Rand8();
      }
    }
  }

 protected:
  static const int kDataAlignment = 16;
  static const int kOuterBlockSize = MAX_SB_SIZE + MAX_FILTER_TAP - 1;
  static const int kBufferSize = kOuterBlockSize * kOuterBlockSize;
  std::vector<uint8_t *> buf_ls_;
  InterpFilter interp_filter_ls_[4];
  ConvolveFunctions *cfs_;
  ACMRandom rnd_;
};

int bsize_ls[] = { 1, 2, 4, 8, 16, 32, 64, 3, 7, 15, 31, 63 };
int bsize_num = NELEMENTS(bsize_ls);

TEST_P(Av1ConvolveTest, av1_convolve_vert) {
  const int y_step_q4 = 16;
  ConvolveParams conv_params = get_conv_params(0, 0, 0);

  int in_stride, out_stride, ref_out_stride, avg_out_stride, ref_avg_out_stride;
  uint8_t *in = add_input(MAX_SB_SIZE, MAX_SB_SIZE, &in_stride);
  uint8_t *out = add_output(MAX_SB_SIZE, MAX_SB_SIZE, &out_stride);
  uint8_t *ref_out = add_output(MAX_SB_SIZE, MAX_SB_SIZE, &ref_out_stride);
  uint8_t *avg_out = add_output(MAX_SB_SIZE, MAX_SB_SIZE, &avg_out_stride);
  uint8_t *ref_avg_out =
      add_output(MAX_SB_SIZE, MAX_SB_SIZE, &ref_avg_out_stride);
  for (int hb_idx = 0; hb_idx < bsize_num; ++hb_idx) {
    for (int vb_idx = 0; vb_idx < bsize_num; ++vb_idx) {
      int w = bsize_ls[hb_idx];
      int h = bsize_ls[vb_idx];
      for (int subpel_y_q4 = 0; subpel_y_q4 < SUBPEL_SHIFTS; ++subpel_y_q4) {
        InterpFilter filter_y = interp_filter_ls_[0];
        InterpFilterParams param_vert = av1_get_interp_filter_params(filter_y);
        const int16_t *filter_vert =
            av1_get_interp_filter_subpel_kernel(param_vert, subpel_y_q4);

        filter_block1d_vert_c(in, in_stride, filter_vert, param_vert.taps,
                              ref_out, ref_out_stride, w, h);

        conv_params.ref = 0;
        conv_params.do_average = 0;
        cfs_->vf_(in, in_stride, out, out_stride, w, h, param_vert, subpel_y_q4,
                  y_step_q4, &conv_params);
        EXPECT_EQ(match(out, out_stride, ref_out, ref_out_stride, w, h), 1)
            << " hb_idx " << hb_idx << " vb_idx " << vb_idx << " filter_y "
            << filter_y << " subpel_y_q4 " << subpel_y_q4;

        random_init_buf(avg_out, w, h, avg_out_stride);
        for (int r = 0; r < h; ++r) {
          for (int c = 0; c < w; ++c) {
            ref_avg_out[r * ref_avg_out_stride + c] = ROUND_POWER_OF_TWO(
                avg_out[r * avg_out_stride + c] + out[r * out_stride + c], 1);
          }
        }
        conv_params.ref = 1;
        conv_params.do_average = 1;
        cfs_->vf_(in, in_stride, avg_out, avg_out_stride, w, h, param_vert,
                  subpel_y_q4, y_step_q4, &conv_params);
        EXPECT_EQ(match(avg_out, avg_out_stride, ref_avg_out,
                        ref_avg_out_stride, w, h),
                  1)
            << " hb_idx " << hb_idx << " vb_idx " << vb_idx << " filter_y "
            << filter_y << " subpel_y_q4 " << subpel_y_q4;
      }
    }
  }
};

TEST_P(Av1ConvolveTest, av1_convolve_horiz) {
  const int x_step_q4 = 16;
  ConvolveParams conv_params = get_conv_params(0, 0, 0);

  int in_stride, out_stride, ref_out_stride, avg_out_stride, ref_avg_out_stride;
  uint8_t *in = add_input(MAX_SB_SIZE, MAX_SB_SIZE, &in_stride);
  uint8_t *out = add_output(MAX_SB_SIZE, MAX_SB_SIZE, &out_stride);
  uint8_t *ref_out = add_output(MAX_SB_SIZE, MAX_SB_SIZE, &ref_out_stride);
  uint8_t *avg_out = add_output(MAX_SB_SIZE, MAX_SB_SIZE, &avg_out_stride);
  uint8_t *ref_avg_out =
      add_output(MAX_SB_SIZE, MAX_SB_SIZE, &ref_avg_out_stride);
  for (int hb_idx = 0; hb_idx < bsize_num; ++hb_idx) {
    for (int vb_idx = 0; vb_idx < bsize_num; ++vb_idx) {
      int w = bsize_ls[hb_idx];
      int h = bsize_ls[vb_idx];
      for (int subpel_x_q4 = 0; subpel_x_q4 < SUBPEL_SHIFTS; ++subpel_x_q4) {
        InterpFilter filter_x = interp_filter_ls_[1];
        InterpFilterParams param_horiz = av1_get_interp_filter_params(filter_x);
        const int16_t *filter_horiz =
            av1_get_interp_filter_subpel_kernel(param_horiz, subpel_x_q4);

        filter_block1d_horiz_c(in, in_stride, filter_horiz, param_horiz.taps,
                               ref_out, ref_out_stride, w, h);

        conv_params.ref = 0;
        conv_params.do_average = 0;
        cfs_->hf_(in, in_stride, out, out_stride, w, h, param_horiz,
                  subpel_x_q4, x_step_q4, &conv_params);
        EXPECT_EQ(match(out, out_stride, ref_out, ref_out_stride, w, h), 1)
            << " hb_idx " << hb_idx << " vb_idx " << vb_idx << " filter_x "
            << filter_x << " subpel_x_q4 " << subpel_x_q4;

        random_init_buf(avg_out, w, h, avg_out_stride);
        for (int r = 0; r < h; ++r) {
          for (int c = 0; c < w; ++c) {
            ref_avg_out[r * ref_avg_out_stride + c] = ROUND_POWER_OF_TWO(
                avg_out[r * avg_out_stride + c] + out[r * out_stride + c], 1);
          }
        }
        conv_params.ref = 1;
        conv_params.do_average = 1;
        cfs_->hf_(in, in_stride, avg_out, avg_out_stride, w, h, param_horiz,
                  subpel_x_q4, x_step_q4, &conv_params);
        EXPECT_EQ(match(avg_out, avg_out_stride, ref_avg_out,
                        ref_avg_out_stride, w, h),
                  1)
            << "hb_idx " << hb_idx << "vb_idx" << vb_idx << " filter_x "
            << filter_x << "subpel_x_q4 " << subpel_x_q4;
      }
    }
  }
};

ConvolveFunctions convolve_functions_c(av1_convolve_horiz_c,
                                       av1_convolve_vert_c);

InterpFilter filter_ls[] = { EIGHTTAP_REGULAR, EIGHTTAP_SMOOTH,
                             MULTITAP_SHARP };

INSTANTIATE_TEST_CASE_P(
    C, Av1ConvolveTest,
    ::testing::Combine(::testing::Values(&convolve_functions_c),
                       ::testing::ValuesIn(filter_ls),
                       ::testing::ValuesIn(filter_ls)));

#if CONFIG_HIGHBITDEPTH
#ifndef __clang_analyzer__
TEST(AV1ConvolveTest, av1_highbd_convolve) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  InterpFilters interp_filters = av1_broadcast_interp_filter(EIGHTTAP_REGULAR);
  InterpFilterParams filter_params =
      av1_get_interp_filter_params(EIGHTTAP_REGULAR);
  int filter_size = filter_params.taps;
  int filter_center = filter_size / 2 - 1;
  uint16_t src[12 * 12];
  int src_stride = filter_size;
  uint16_t dst[1] = { 0 };
  int dst_stride = 1;
  int x_step_q4 = 16;
  int y_step_q4 = 16;
  int avg = 0;
  int bd = 10;
  int w = 1;
  int h = 1;

  int subpel_x_q4;
  int subpel_y_q4;

  for (int i = 0; i < filter_size * filter_size; i++) {
    src[i] = rnd.Rand16() % (1 << bd);
  }

  for (subpel_x_q4 = 0; subpel_x_q4 < SUBPEL_SHIFTS; subpel_x_q4++) {
    for (subpel_y_q4 = 0; subpel_y_q4 < SUBPEL_SHIFTS; subpel_y_q4++) {
      av1_highbd_convolve(
          CONVERT_TO_BYTEPTR(src + src_stride * filter_center + filter_center),
          src_stride, CONVERT_TO_BYTEPTR(dst), dst_stride, w, h, interp_filters,
          subpel_x_q4, x_step_q4, subpel_y_q4, y_step_q4, avg, bd);

      const int16_t *x_filter =
          av1_get_interp_filter_subpel_kernel(filter_params, subpel_x_q4);
      const int16_t *y_filter =
          av1_get_interp_filter_subpel_kernel(filter_params, subpel_y_q4);

      int temp[12];
      int dst_ref = 0;
      for (int r = 0; r < filter_size; r++) {
        temp[r] = 0;
        for (int c = 0; c < filter_size; c++) {
          temp[r] += x_filter[c] * src[r * filter_size + c];
        }
        temp[r] =
            clip_pixel_highbd(ROUND_POWER_OF_TWO(temp[r], FILTER_BITS), bd);
        dst_ref += temp[r] * y_filter[r];
      }
      dst_ref = clip_pixel_highbd(ROUND_POWER_OF_TWO(dst_ref, FILTER_BITS), bd);
      EXPECT_EQ(dst[0], dst_ref);
    }
  }
}
#endif

TEST(AV1ConvolveTest, av1_highbd_convolve_avg) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  InterpFilters interp_filters = av1_broadcast_interp_filter(EIGHTTAP_REGULAR);
  InterpFilterParams filter_params =
      av1_get_interp_filter_params(EIGHTTAP_REGULAR);
  int filter_size = filter_params.taps;
  int filter_center = filter_size / 2 - 1;
  uint16_t src0[12 * 12];
  uint16_t src1[12 * 12];
  int src_stride = filter_size;
  uint16_t dst0[1] = { 0 };
  uint16_t dst1[1] = { 0 };
  uint16_t dst[1] = { 0 };
  int dst_stride = 1;
  int x_step_q4 = 16;
  int y_step_q4 = 16;
  int avg = 0;
  int bd = 10;

  int w = 1;
  int h = 1;

  int subpel_x_q4;
  int subpel_y_q4;

  for (int i = 0; i < filter_size * filter_size; i++) {
    src0[i] = rnd.Rand16() % (1 << bd);
    src1[i] = rnd.Rand16() % (1 << bd);
  }

  for (subpel_x_q4 = 0; subpel_x_q4 < SUBPEL_SHIFTS; subpel_x_q4++) {
    for (subpel_y_q4 = 0; subpel_y_q4 < SUBPEL_SHIFTS; subpel_y_q4++) {
      int offset = filter_size * filter_center + filter_center;

      avg = 0;
      av1_highbd_convolve(CONVERT_TO_BYTEPTR(src0 + offset), src_stride,
                          CONVERT_TO_BYTEPTR(dst0), dst_stride, w, h,
                          interp_filters, subpel_x_q4, x_step_q4, subpel_y_q4,
                          y_step_q4, avg, bd);
      avg = 0;
      av1_highbd_convolve(CONVERT_TO_BYTEPTR(src1 + offset), src_stride,
                          CONVERT_TO_BYTEPTR(dst1), dst_stride, w, h,
                          interp_filters, subpel_x_q4, x_step_q4, subpel_y_q4,
                          y_step_q4, avg, bd);

      avg = 0;
      av1_highbd_convolve(CONVERT_TO_BYTEPTR(src0 + offset), src_stride,
                          CONVERT_TO_BYTEPTR(dst), dst_stride, w, h,
                          interp_filters, subpel_x_q4, x_step_q4, subpel_y_q4,
                          y_step_q4, avg, bd);
      avg = 1;
      av1_highbd_convolve(CONVERT_TO_BYTEPTR(src1 + offset), src_stride,
                          CONVERT_TO_BYTEPTR(dst), dst_stride, w, h,
                          interp_filters, subpel_x_q4, x_step_q4, subpel_y_q4,
                          y_step_q4, avg, bd);

      EXPECT_EQ(dst[0], ROUND_POWER_OF_TWO(dst0[0] + dst1[0], 1));
    }
  }
}
#endif  // CONFIG_HIGHBITDEPTH

#define CONVOLVE_SPEED_TEST 0
#if CONVOLVE_SPEED_TEST
#define highbd_convolve_speed(func, block_size, frame_size)                  \
  TEST(AV1ConvolveTest, func##_speed_##block_size##_##frame_size) {          \
    ACMRandom rnd(ACMRandom::DeterministicSeed());                           \
    InterpFilter interp_filter = EIGHTTAP;                                   \
    InterpFilterParams filter_params =                                       \
        av1_get_interp_filter_params(interp_filter);                         \
    int filter_size = filter_params.tap;                                     \
    int filter_center = filter_size / 2 - 1;                                 \
    DECLARE_ALIGNED(16, uint16_t,                                            \
                    src[(frame_size + 7) * (frame_size + 7)]) = { 0 };       \
    int src_stride = frame_size + 7;                                         \
    DECLARE_ALIGNED(16, uint16_t, dst[frame_size * frame_size]) = { 0 };     \
    int dst_stride = frame_size;                                             \
    int x_step_q4 = 16;                                                      \
    int y_step_q4 = 16;                                                      \
    int subpel_x_q4 = 8;                                                     \
    int subpel_y_q4 = 6;                                                     \
    int bd = 10;                                                             \
                                                                             \
    int w = block_size;                                                      \
    int h = block_size;                                                      \
                                                                             \
    const int16_t *filter_x =                                                \
        av1_get_interp_filter_kernel(filter_params, subpel_x_q4);            \
    const int16_t *filter_y =                                                \
        av1_get_interp_filter_kernel(filter_params, subpel_y_q4);            \
                                                                             \
    for (int i = 0; i < src_stride * src_stride; i++) {                      \
      src[i] = rnd.Rand16() % (1 << bd);                                     \
    }                                                                        \
                                                                             \
    int offset = filter_center * src_stride + filter_center;                 \
    int row_offset = 0;                                                      \
    int col_offset = 0;                                                      \
    for (int i = 0; i < 100000; i++) {                                       \
      int src_total_offset = offset + col_offset * src_stride + row_offset;  \
      int dst_total_offset = col_offset * dst_stride + row_offset;           \
      func(CONVERT_TO_BYTEPTR(src + src_total_offset), src_stride,           \
           CONVERT_TO_BYTEPTR(dst + dst_total_offset), dst_stride, filter_x, \
           x_step_q4, filter_y, y_step_q4, w, h, bd);                        \
      if (offset + w + w < frame_size) {                                     \
        row_offset += w;                                                     \
      } else {                                                               \
        row_offset = 0;                                                      \
        col_offset += h;                                                     \
      }                                                                      \
      if (col_offset + h >= frame_size) {                                    \
        col_offset = 0;                                                      \
      }                                                                      \
    }                                                                        \
  }

#define lowbd_convolve_speed(func, block_size, frame_size)                  \
  TEST(AV1ConvolveTest, func##_speed_l_##block_size##_##frame_size) {       \
    ACMRandom rnd(ACMRandom::DeterministicSeed());                          \
    InterpFilter interp_filter = EIGHTTAP;                                  \
    InterpFilterParams filter_params =                                      \
        av1_get_interp_filter_params(interp_filter);                        \
    int filter_size = filter_params.tap;                                    \
    int filter_center = filter_size / 2 - 1;                                \
    DECLARE_ALIGNED(16, uint8_t, src[(frame_size + 7) * (frame_size + 7)]); \
    int src_stride = frame_size + 7;                                        \
    DECLARE_ALIGNED(16, uint8_t, dst[frame_size * frame_size]);             \
    int dst_stride = frame_size;                                            \
    int x_step_q4 = 16;                                                     \
    int y_step_q4 = 16;                                                     \
    int subpel_x_q4 = 8;                                                    \
    int subpel_y_q4 = 6;                                                    \
    int bd = 8;                                                             \
                                                                            \
    int w = block_size;                                                     \
    int h = block_size;                                                     \
                                                                            \
    const int16_t *filter_x =                                               \
        av1_get_interp_filter_kernel(filter_params, subpel_x_q4);           \
    const int16_t *filter_y =                                               \
        av1_get_interp_filter_kernel(filter_params, subpel_y_q4);           \
                                                                            \
    for (int i = 0; i < src_stride * src_stride; i++) {                     \
      src[i] = rnd.Rand16() % (1 << bd);                                    \
    }                                                                       \
                                                                            \
    int offset = filter_center * src_stride + filter_center;                \
    int row_offset = 0;                                                     \
    int col_offset = 0;                                                     \
    for (int i = 0; i < 100000; i++) {                                      \
      func(src + offset, src_stride, dst, dst_stride, filter_x, x_step_q4,  \
           filter_y, y_step_q4, w, h);                                      \
      if (offset + w + w < frame_size) {                                    \
        row_offset += w;                                                    \
      } else {                                                              \
        row_offset = 0;                                                     \
        col_offset += h;                                                    \
      }                                                                     \
      if (col_offset + h >= frame_size) {                                   \
        col_offset = 0;                                                     \
      }                                                                     \
    }                                                                       \
  }

// This experiment shows that when frame size is 64x64
// aom_highbd_convolve8_sse2 and aom_convolve8_sse2's speed are similar.
// However when frame size becomes 1024x1024
// aom_highbd_convolve8_sse2 is around 50% slower than aom_convolve8_sse2
// we think the bottleneck is from memory IO
highbd_convolve_speed(aom_highbd_convolve8_sse2, 8, 64);
highbd_convolve_speed(aom_highbd_convolve8_sse2, 16, 64);
highbd_convolve_speed(aom_highbd_convolve8_sse2, 32, 64);
highbd_convolve_speed(aom_highbd_convolve8_sse2, 64, 64);

lowbd_convolve_speed(aom_convolve8_sse2, 8, 64);
lowbd_convolve_speed(aom_convolve8_sse2, 16, 64);
lowbd_convolve_speed(aom_convolve8_sse2, 32, 64);
lowbd_convolve_speed(aom_convolve8_sse2, 64, 64);

highbd_convolve_speed(aom_highbd_convolve8_sse2, 8, 1024);
highbd_convolve_speed(aom_highbd_convolve8_sse2, 16, 1024);
highbd_convolve_speed(aom_highbd_convolve8_sse2, 32, 1024);
highbd_convolve_speed(aom_highbd_convolve8_sse2, 64, 1024);

lowbd_convolve_speed(aom_convolve8_sse2, 8, 1024);
lowbd_convolve_speed(aom_convolve8_sse2, 16, 1024);
lowbd_convolve_speed(aom_convolve8_sse2, 32, 1024);
lowbd_convolve_speed(aom_convolve8_sse2, 64, 1024);
#endif  // CONVOLVE_SPEED_TEST
}  // namespace
