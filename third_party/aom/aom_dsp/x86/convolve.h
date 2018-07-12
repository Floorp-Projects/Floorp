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
#ifndef AOM_DSP_X86_CONVOLVE_H_
#define AOM_DSP_X86_CONVOLVE_H_

#include <assert.h>

#include "./aom_config.h"
#include "aom/aom_integer.h"
#include "aom_ports/mem.h"
#include "aom_dsp/aom_convolve.h"

typedef void filter8_1dfunction(const uint8_t *src_ptr, ptrdiff_t src_pitch,
                                uint8_t *output_ptr, ptrdiff_t out_pitch,
                                uint32_t output_height, const int16_t *filter);

#define FUN_CONV_1D(name, step_q4, filter, dir, src_start, avg, opt)         \
  void aom_convolve8_##name##_##opt(                                         \
      const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,                \
      ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4,          \
      const int16_t *filter_y, int y_step_q4, int w, int h) {                \
    (void)filter_x;                                                          \
    (void)x_step_q4;                                                         \
    (void)filter_y;                                                          \
    (void)y_step_q4;                                                         \
    assert((-128 <= filter[3]) && (filter[3] <= 127));                       \
    assert(step_q4 == 16);                                                   \
    if (filter[0] | filter[1] | filter[2]) {                                 \
      while (w >= 16) {                                                      \
        aom_filter_block1d16_##dir##8_##avg##opt(src_start, src_stride, dst, \
                                                 dst_stride, h, filter);     \
        src += 16;                                                           \
        dst += 16;                                                           \
        w -= 16;                                                             \
      }                                                                      \
      while (w >= 8) {                                                       \
        aom_filter_block1d8_##dir##8_##avg##opt(src_start, src_stride, dst,  \
                                                dst_stride, h, filter);      \
        src += 8;                                                            \
        dst += 8;                                                            \
        w -= 8;                                                              \
      }                                                                      \
      while (w >= 4) {                                                       \
        aom_filter_block1d4_##dir##8_##avg##opt(src_start, src_stride, dst,  \
                                                dst_stride, h, filter);      \
        src += 4;                                                            \
        dst += 4;                                                            \
        w -= 4;                                                              \
      }                                                                      \
    } else {                                                                 \
      while (w >= 16) {                                                      \
        aom_filter_block1d16_##dir##2_##avg##opt(src, src_stride, dst,       \
                                                 dst_stride, h, filter);     \
        src += 16;                                                           \
        dst += 16;                                                           \
        w -= 16;                                                             \
      }                                                                      \
      while (w >= 8) {                                                       \
        aom_filter_block1d8_##dir##2_##avg##opt(src, src_stride, dst,        \
                                                dst_stride, h, filter);      \
        src += 8;                                                            \
        dst += 8;                                                            \
        w -= 8;                                                              \
      }                                                                      \
      while (w >= 4) {                                                       \
        aom_filter_block1d4_##dir##2_##avg##opt(src, src_stride, dst,        \
                                                dst_stride, h, filter);      \
        src += 4;                                                            \
        dst += 4;                                                            \
        w -= 4;                                                              \
      }                                                                      \
    }                                                                        \
    if (w) {                                                                 \
      aom_convolve8_##name##_c(src, src_stride, dst, dst_stride, filter_x,   \
                               x_step_q4, filter_y, y_step_q4, w, h);        \
    }                                                                        \
  }

#define FUN_CONV_2D(avg, opt)                                                \
  void aom_convolve8_##avg##opt(                                             \
      const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,                \
      ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4,          \
      const int16_t *filter_y, int y_step_q4, int w, int h) {                \
    assert((-128 <= filter_x[3]) && (filter_x[3] <= 127));                   \
    assert((-128 <= filter_y[3]) && (filter_y[3] <= 127));                   \
    assert(w <= MAX_SB_SIZE);                                                \
    assert(h <= MAX_SB_SIZE);                                                \
    assert(x_step_q4 == 16);                                                 \
    assert(y_step_q4 == 16);                                                 \
    if (filter_x[0] || filter_x[1] || filter_x[2] || filter_y[0] ||          \
        filter_y[1] || filter_y[2]) {                                        \
      DECLARE_ALIGNED(16, uint8_t, fdata2[MAX_SB_SIZE * (MAX_SB_SIZE + 7)]); \
      aom_convolve8_horiz_##opt(src - 3 * src_stride, src_stride, fdata2,    \
                                MAX_SB_SIZE, filter_x, x_step_q4, filter_y,  \
                                y_step_q4, w, h + 7);                        \
      aom_convolve8_##avg##vert_##opt(fdata2 + 3 * MAX_SB_SIZE, MAX_SB_SIZE, \
                                      dst, dst_stride, filter_x, x_step_q4,  \
                                      filter_y, y_step_q4, w, h);            \
    } else {                                                                 \
      DECLARE_ALIGNED(16, uint8_t, fdata2[MAX_SB_SIZE * (MAX_SB_SIZE + 1)]); \
      aom_convolve8_horiz_##opt(src, src_stride, fdata2, MAX_SB_SIZE,        \
                                filter_x, x_step_q4, filter_y, y_step_q4, w, \
                                h + 1);                                      \
      aom_convolve8_##avg##vert_##opt(fdata2, MAX_SB_SIZE, dst, dst_stride,  \
                                      filter_x, x_step_q4, filter_y,         \
                                      y_step_q4, w, h);                      \
    }                                                                        \
  }

#if CONFIG_LOOP_RESTORATION
// convolve_add_src is only used by the Wiener filter, which will never
// end up calling the bilinear functions (it uses a symmetric filter, so
// the possible numbers of taps are 1,3,5,7)
#define FUN_CONV_1D_NO_BILINEAR(name, step_q4, filter, dir, src_start, avg, \
                                opt)                                        \
  void aom_convolve8_##name##_##opt(                                        \
      const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,               \
      ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4,         \
      const int16_t *filter_y, int y_step_q4, int w, int h) {               \
    (void)filter_x;                                                         \
    (void)x_step_q4;                                                        \
    (void)filter_y;                                                         \
    (void)y_step_q4;                                                        \
    assert((-128 <= filter[3]) && (filter[3] <= 127));                      \
    assert(step_q4 == 16);                                                  \
    while (w >= 16) {                                                       \
      aom_filter_block1d16_##dir##8_##avg##opt(src_start, src_stride, dst,  \
                                               dst_stride, h, filter);      \
      src += 16;                                                            \
      dst += 16;                                                            \
      w -= 16;                                                              \
    }                                                                       \
    while (w >= 8) {                                                        \
      aom_filter_block1d8_##dir##8_##avg##opt(src_start, src_stride, dst,   \
                                              dst_stride, h, filter);       \
      src += 8;                                                             \
      dst += 8;                                                             \
      w -= 8;                                                               \
    }                                                                       \
    while (w >= 4) {                                                        \
      aom_filter_block1d4_##dir##8_##avg##opt(src_start, src_stride, dst,   \
                                              dst_stride, h, filter);       \
      src += 4;                                                             \
      dst += 4;                                                             \
      w -= 4;                                                               \
    }                                                                       \
    if (w) {                                                                \
      aom_convolve8_##name##_c(src, src_stride, dst, dst_stride, filter_x,  \
                               x_step_q4, filter_y, y_step_q4, w, h);       \
    }                                                                       \
  }

#define FUN_CONV_2D_NO_BILINEAR(type, htype, opt)                           \
  void aom_convolve8_##type##opt(                                           \
      const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,               \
      ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4,         \
      const int16_t *filter_y, int y_step_q4, int w, int h) {               \
    DECLARE_ALIGNED(16, uint8_t, fdata2[MAX_SB_SIZE * (MAX_SB_SIZE + 7)]);  \
    assert((-128 <= filter_x[3]) && (filter_x[3] <= 127));                  \
    assert((-128 <= filter_y[3]) && (filter_y[3] <= 127));                  \
    assert(w <= MAX_SB_SIZE);                                               \
    assert(h <= MAX_SB_SIZE);                                               \
    assert(x_step_q4 == 16);                                                \
    assert(y_step_q4 == 16);                                                \
    aom_convolve8_##htype##horiz_##opt(                                     \
        src - 3 * src_stride, src_stride, fdata2, MAX_SB_SIZE, filter_x,    \
        x_step_q4, filter_y, y_step_q4, w, h + 7);                          \
    aom_convolve8_##type##vert_##opt(fdata2 + 3 * MAX_SB_SIZE, MAX_SB_SIZE, \
                                     dst, dst_stride, filter_x, x_step_q4,  \
                                     filter_y, y_step_q4, w, h);            \
  }
#endif

#if CONFIG_HIGHBITDEPTH
typedef void highbd_filter8_1dfunction(const uint16_t *src_ptr,
                                       const ptrdiff_t src_pitch,
                                       uint16_t *output_ptr,
                                       ptrdiff_t out_pitch,
                                       unsigned int output_height,
                                       const int16_t *filter, int bd);

#define HIGH_FUN_CONV_1D(name, step_q4, filter, dir, src_start, avg, opt)  \
  void aom_highbd_convolve8_##name##_##opt(                                \
      const uint8_t *src8, ptrdiff_t src_stride, uint8_t *dst8,            \
      ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4,        \
      const int16_t *filter_y, int y_step_q4, int w, int h, int bd) {      \
    uint16_t *src = CONVERT_TO_SHORTPTR(src8);                             \
    uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);                             \
    if (step_q4 == 16 && filter[3] != 128) {                               \
      if (filter[0] | filter[1] | filter[2]) {                             \
        while (w >= 16) {                                                  \
          aom_highbd_filter_block1d16_##dir##8_##avg##opt(                 \
              src_start, src_stride, dst, dst_stride, h, filter, bd);      \
          src += 16;                                                       \
          dst += 16;                                                       \
          w -= 16;                                                         \
        }                                                                  \
        while (w >= 8) {                                                   \
          aom_highbd_filter_block1d8_##dir##8_##avg##opt(                  \
              src_start, src_stride, dst, dst_stride, h, filter, bd);      \
          src += 8;                                                        \
          dst += 8;                                                        \
          w -= 8;                                                          \
        }                                                                  \
        while (w >= 4) {                                                   \
          aom_highbd_filter_block1d4_##dir##8_##avg##opt(                  \
              src_start, src_stride, dst, dst_stride, h, filter, bd);      \
          src += 4;                                                        \
          dst += 4;                                                        \
          w -= 4;                                                          \
        }                                                                  \
      } else {                                                             \
        while (w >= 16) {                                                  \
          aom_highbd_filter_block1d16_##dir##2_##avg##opt(                 \
              src, src_stride, dst, dst_stride, h, filter, bd);            \
          src += 16;                                                       \
          dst += 16;                                                       \
          w -= 16;                                                         \
        }                                                                  \
        while (w >= 8) {                                                   \
          aom_highbd_filter_block1d8_##dir##2_##avg##opt(                  \
              src, src_stride, dst, dst_stride, h, filter, bd);            \
          src += 8;                                                        \
          dst += 8;                                                        \
          w -= 8;                                                          \
        }                                                                  \
        while (w >= 4) {                                                   \
          aom_highbd_filter_block1d4_##dir##2_##avg##opt(                  \
              src, src_stride, dst, dst_stride, h, filter, bd);            \
          src += 4;                                                        \
          dst += 4;                                                        \
          w -= 4;                                                          \
        }                                                                  \
      }                                                                    \
    }                                                                      \
    if (w) {                                                               \
      aom_highbd_convolve8_##name##_c(                                     \
          CONVERT_TO_BYTEPTR(src), src_stride, CONVERT_TO_BYTEPTR(dst),    \
          dst_stride, filter_x, x_step_q4, filter_y, y_step_q4, w, h, bd); \
    }                                                                      \
  }

#define HIGH_FUN_CONV_2D(avg, opt)                                            \
  void aom_highbd_convolve8_##avg##opt(                                       \
      const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,                 \
      ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4,           \
      const int16_t *filter_y, int y_step_q4, int w, int h, int bd) {         \
    assert(w <= MAX_SB_SIZE);                                                 \
    assert(h <= MAX_SB_SIZE);                                                 \
    if (x_step_q4 == 16 && y_step_q4 == 16) {                                 \
      if (filter_x[0] || filter_x[1] || filter_x[2] || filter_x[3] == 128 ||  \
          filter_y[0] || filter_y[1] || filter_y[2] || filter_y[3] == 128) {  \
        DECLARE_ALIGNED(16, uint16_t,                                         \
                        fdata2[MAX_SB_SIZE * (MAX_SB_SIZE + 7)]);             \
        aom_highbd_convolve8_horiz_##opt(src - 3 * src_stride, src_stride,    \
                                         CONVERT_TO_BYTEPTR(fdata2),          \
                                         MAX_SB_SIZE, filter_x, x_step_q4,    \
                                         filter_y, y_step_q4, w, h + 7, bd);  \
        aom_highbd_convolve8_##avg##vert_##opt(                               \
            CONVERT_TO_BYTEPTR(fdata2) + 3 * MAX_SB_SIZE, MAX_SB_SIZE, dst,   \
            dst_stride, filter_x, x_step_q4, filter_y, y_step_q4, w, h, bd);  \
      } else {                                                                \
        DECLARE_ALIGNED(16, uint16_t,                                         \
                        fdata2[MAX_SB_SIZE * (MAX_SB_SIZE + 1)]);             \
        aom_highbd_convolve8_horiz_##opt(                                     \
            src, src_stride, CONVERT_TO_BYTEPTR(fdata2), MAX_SB_SIZE,         \
            filter_x, x_step_q4, filter_y, y_step_q4, w, h + 1, bd);          \
        aom_highbd_convolve8_##avg##vert_##opt(                               \
            CONVERT_TO_BYTEPTR(fdata2), MAX_SB_SIZE, dst, dst_stride,         \
            filter_x, x_step_q4, filter_y, y_step_q4, w, h, bd);              \
      }                                                                       \
    } else {                                                                  \
      aom_highbd_convolve8_##avg##c(src, src_stride, dst, dst_stride,         \
                                    filter_x, x_step_q4, filter_y, y_step_q4, \
                                    w, h, bd);                                \
    }                                                                         \
  }
#endif  // CONFIG_HIGHBITDEPTH

#endif  // AOM_DSP_X86_CONVOLVE_H_
