/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "./vpx_config.h"
#include "vpx_scale/yv12config.h"
#include "vp9/common/vp9_postproc.h"
#include "vp9/common/vp9_textblit.h"
#include "vpx_scale/vpx_scale.h"
#include "vp9/common/vp9_systemdependent.h"
#include "./vp9_rtcd.h"
#include "./vpx_scale_rtcd.h"

#define RGB_TO_YUV(t)                                            \
  ( (0.257*(float)(t >> 16))  + (0.504*(float)(t >> 8 & 0xff)) + \
    (0.098*(float)(t & 0xff)) + 16),                             \
  (-(0.148*(float)(t >> 16))  - (0.291*(float)(t >> 8 & 0xff)) + \
    (0.439*(float)(t & 0xff)) + 128),                            \
  ( (0.439*(float)(t >> 16))  - (0.368*(float)(t >> 8 & 0xff)) - \
    (0.071*(float)(t & 0xff)) + 128)

/* global constants */
#if 0 && CONFIG_POSTPROC_VISUALIZER
static const unsigned char MB_PREDICTION_MODE_colors[MB_MODE_COUNT][3] = {
  { RGB_TO_YUV(0x98FB98) },   /* PaleGreen */
  { RGB_TO_YUV(0x00FF00) },   /* Green */
  { RGB_TO_YUV(0xADFF2F) },   /* GreenYellow */
  { RGB_TO_YUV(0x8F0000) },   /* Dark Red */
  { RGB_TO_YUV(0x008F8F) },   /* Dark Cyan */
  { RGB_TO_YUV(0x008F8F) },   /* Dark Cyan */
  { RGB_TO_YUV(0x008F8F) },   /* Dark Cyan */
  { RGB_TO_YUV(0x8F0000) },   /* Dark Red */
  { RGB_TO_YUV(0x8F0000) },   /* Dark Red */
  { RGB_TO_YUV(0x228B22) },   /* ForestGreen */
  { RGB_TO_YUV(0x006400) },   /* DarkGreen */
  { RGB_TO_YUV(0x98F5FF) },   /* Cadet Blue */
  { RGB_TO_YUV(0x6CA6CD) },   /* Sky Blue */
  { RGB_TO_YUV(0x00008B) },   /* Dark blue */
  { RGB_TO_YUV(0x551A8B) },   /* Purple */
  { RGB_TO_YUV(0xFF0000) }    /* Red */
  { RGB_TO_YUV(0xCC33FF) },   /* Magenta */
};

static const unsigned char B_PREDICTION_MODE_colors[INTRA_MODES][3] = {
  { RGB_TO_YUV(0x6633ff) },   /* Purple */
  { RGB_TO_YUV(0xcc33ff) },   /* Magenta */
  { RGB_TO_YUV(0xff33cc) },   /* Pink */
  { RGB_TO_YUV(0xff3366) },   /* Coral */
  { RGB_TO_YUV(0x3366ff) },   /* Blue */
  { RGB_TO_YUV(0xed00f5) },   /* Dark Blue */
  { RGB_TO_YUV(0x2e00b8) },   /* Dark Purple */
  { RGB_TO_YUV(0xff6633) },   /* Orange */
  { RGB_TO_YUV(0x33ccff) },   /* Light Blue */
  { RGB_TO_YUV(0x8ab800) },   /* Green */
  { RGB_TO_YUV(0xffcc33) },   /* Light Orange */
  { RGB_TO_YUV(0x33ffcc) },   /* Aqua */
  { RGB_TO_YUV(0x66ff33) },   /* Light Green */
  { RGB_TO_YUV(0xccff33) },   /* Yellow */
};

static const unsigned char MV_REFERENCE_FRAME_colors[MAX_REF_FRAMES][3] = {
  { RGB_TO_YUV(0x00ff00) },   /* Blue */
  { RGB_TO_YUV(0x0000ff) },   /* Green */
  { RGB_TO_YUV(0xffff00) },   /* Yellow */
  { RGB_TO_YUV(0xff0000) },   /* Red */
};
#endif

static const short kernel5[] = {
  1, 1, 4, 1, 1
};

const short vp9_rv[] = {
  8, 5, 2, 2, 8, 12, 4, 9, 8, 3,
  0, 3, 9, 0, 0, 0, 8, 3, 14, 4,
  10, 1, 11, 14, 1, 14, 9, 6, 12, 11,
  8, 6, 10, 0, 0, 8, 9, 0, 3, 14,
  8, 11, 13, 4, 2, 9, 0, 3, 9, 6,
  1, 2, 3, 14, 13, 1, 8, 2, 9, 7,
  3, 3, 1, 13, 13, 6, 6, 5, 2, 7,
  11, 9, 11, 8, 7, 3, 2, 0, 13, 13,
  14, 4, 12, 5, 12, 10, 8, 10, 13, 10,
  4, 14, 4, 10, 0, 8, 11, 1, 13, 7,
  7, 14, 6, 14, 13, 2, 13, 5, 4, 4,
  0, 10, 0, 5, 13, 2, 12, 7, 11, 13,
  8, 0, 4, 10, 7, 2, 7, 2, 2, 5,
  3, 4, 7, 3, 3, 14, 14, 5, 9, 13,
  3, 14, 3, 6, 3, 0, 11, 8, 13, 1,
  13, 1, 12, 0, 10, 9, 7, 6, 2, 8,
  5, 2, 13, 7, 1, 13, 14, 7, 6, 7,
  9, 6, 10, 11, 7, 8, 7, 5, 14, 8,
  4, 4, 0, 8, 7, 10, 0, 8, 14, 11,
  3, 12, 5, 7, 14, 3, 14, 5, 2, 6,
  11, 12, 12, 8, 0, 11, 13, 1, 2, 0,
  5, 10, 14, 7, 8, 0, 4, 11, 0, 8,
  0, 3, 10, 5, 8, 0, 11, 6, 7, 8,
  10, 7, 13, 9, 2, 5, 1, 5, 10, 2,
  4, 3, 5, 6, 10, 8, 9, 4, 11, 14,
  0, 10, 0, 5, 13, 2, 12, 7, 11, 13,
  8, 0, 4, 10, 7, 2, 7, 2, 2, 5,
  3, 4, 7, 3, 3, 14, 14, 5, 9, 13,
  3, 14, 3, 6, 3, 0, 11, 8, 13, 1,
  13, 1, 12, 0, 10, 9, 7, 6, 2, 8,
  5, 2, 13, 7, 1, 13, 14, 7, 6, 7,
  9, 6, 10, 11, 7, 8, 7, 5, 14, 8,
  4, 4, 0, 8, 7, 10, 0, 8, 14, 11,
  3, 12, 5, 7, 14, 3, 14, 5, 2, 6,
  11, 12, 12, 8, 0, 11, 13, 1, 2, 0,
  5, 10, 14, 7, 8, 0, 4, 11, 0, 8,
  0, 3, 10, 5, 8, 0, 11, 6, 7, 8,
  10, 7, 13, 9, 2, 5, 1, 5, 10, 2,
  4, 3, 5, 6, 10, 8, 9, 4, 11, 14,
  3, 8, 3, 7, 8, 5, 11, 4, 12, 3,
  11, 9, 14, 8, 14, 13, 4, 3, 1, 2,
  14, 6, 5, 4, 4, 11, 4, 6, 2, 1,
  5, 8, 8, 12, 13, 5, 14, 10, 12, 13,
  0, 9, 5, 5, 11, 10, 13, 9, 10, 13,
};


/****************************************************************************
 */
void vp9_post_proc_down_and_across_c(const uint8_t *src_ptr,
                                     uint8_t *dst_ptr,
                                     int src_pixels_per_line,
                                     int dst_pixels_per_line,
                                     int rows,
                                     int cols,
                                     int flimit) {
  uint8_t const *p_src;
  uint8_t *p_dst;
  int row;
  int col;
  int i;
  int v;
  int pitch = src_pixels_per_line;
  uint8_t d[8];
  (void)dst_pixels_per_line;

  for (row = 0; row < rows; row++) {
    /* post_proc_down for one row */
    p_src = src_ptr;
    p_dst = dst_ptr;

    for (col = 0; col < cols; col++) {
      int kernel = 4;
      int v = p_src[col];

      for (i = -2; i <= 2; i++) {
        if (abs(v - p_src[col + i * pitch]) > flimit)
          goto down_skip_convolve;

        kernel += kernel5[2 + i] * p_src[col + i * pitch];
      }

      v = (kernel >> 3);
    down_skip_convolve:
      p_dst[col] = v;
    }

    /* now post_proc_across */
    p_src = dst_ptr;
    p_dst = dst_ptr;

    for (i = 0; i < 8; i++)
      d[i] = p_src[i];

    for (col = 0; col < cols; col++) {
      int kernel = 4;
      v = p_src[col];

      d[col & 7] = v;

      for (i = -2; i <= 2; i++) {
        if (abs(v - p_src[col + i]) > flimit)
          goto across_skip_convolve;

        kernel += kernel5[2 + i] * p_src[col + i];
      }

      d[col & 7] = (kernel >> 3);
    across_skip_convolve:

      if (col >= 2)
        p_dst[col - 2] = d[(col - 2) & 7];
    }

    /* handle the last two pixels */
    p_dst[col - 2] = d[(col - 2) & 7];
    p_dst[col - 1] = d[(col - 1) & 7];


    /* next row */
    src_ptr += pitch;
    dst_ptr += pitch;
  }
}

static int q2mbl(int x) {
  if (x < 20) x = 20;

  x = 50 + (x - 50) * 10 / 8;
  return x * x / 3;
}

void vp9_mbpost_proc_across_ip_c(uint8_t *src, int pitch,
                                 int rows, int cols, int flimit) {
  int r, c, i;

  uint8_t *s = src;
  uint8_t d[16];


  for (r = 0; r < rows; r++) {
    int sumsq = 0;
    int sum   = 0;

    for (i = -8; i <= 6; i++) {
      sumsq += s[i] * s[i];
      sum   += s[i];
      d[i + 8] = 0;
    }

    for (c = 0; c < cols + 8; c++) {
      int x = s[c + 7] - s[c - 8];
      int y = s[c + 7] + s[c - 8];

      sum  += x;
      sumsq += x * y;

      d[c & 15] = s[c];

      if (sumsq * 15 - sum * sum < flimit) {
        d[c & 15] = (8 + sum + s[c]) >> 4;
      }

      s[c - 8] = d[(c - 8) & 15];
    }

    s += pitch;
  }
}

void vp9_mbpost_proc_down_c(uint8_t *dst, int pitch,
                            int rows, int cols, int flimit) {
  int r, c, i;
  const short *rv3 = &vp9_rv[63 & rand()]; // NOLINT

  for (c = 0; c < cols; c++) {
    uint8_t *s = &dst[c];
    int sumsq = 0;
    int sum   = 0;
    uint8_t d[16];
    const short *rv2 = rv3 + ((c * 17) & 127);

    for (i = -8; i <= 6; i++) {
      sumsq += s[i * pitch] * s[i * pitch];
      sum   += s[i * pitch];
    }

    for (r = 0; r < rows + 8; r++) {
      sumsq += s[7 * pitch] * s[ 7 * pitch] - s[-8 * pitch] * s[-8 * pitch];
      sum  += s[7 * pitch] - s[-8 * pitch];
      d[r & 15] = s[0];

      if (sumsq * 15 - sum * sum < flimit) {
        d[r & 15] = (rv2[r & 127] + sum + s[0]) >> 4;
      }

      s[-8 * pitch] = d[(r - 8) & 15];
      s += pitch;
    }
  }
}

static void deblock_and_de_macro_block(YV12_BUFFER_CONFIG   *source,
                                       YV12_BUFFER_CONFIG   *post,
                                       int                   q,
                                       int                   low_var_thresh,
                                       int                   flag) {
  double level = 6.0e-05 * q * q * q - .0067 * q * q + .306 * q + .0065;
  int ppl = (int)(level + .5);
  (void) low_var_thresh;
  (void) flag;

  vp9_post_proc_down_and_across(source->y_buffer, post->y_buffer,
                                source->y_stride, post->y_stride,
                                source->y_height, source->y_width, ppl);

  vp9_mbpost_proc_across_ip(post->y_buffer, post->y_stride, post->y_height,
                            post->y_width, q2mbl(q));

  vp9_mbpost_proc_down(post->y_buffer, post->y_stride, post->y_height,
                       post->y_width, q2mbl(q));

  vp9_post_proc_down_and_across(source->u_buffer, post->u_buffer,
                                source->uv_stride, post->uv_stride,
                                source->uv_height, source->uv_width, ppl);
  vp9_post_proc_down_and_across(source->v_buffer, post->v_buffer,
                                source->uv_stride, post->uv_stride,
                                source->uv_height, source->uv_width, ppl);
}

void vp9_deblock(const YV12_BUFFER_CONFIG *src, YV12_BUFFER_CONFIG *dst,
                 int q) {
  const int ppl = (int)(6.0e-05 * q * q * q - 0.0067 * q * q + 0.306 * q
                        + 0.0065 + 0.5);
  int i;

  const uint8_t *const srcs[4] = {src->y_buffer, src->u_buffer, src->v_buffer,
                                  src->alpha_buffer};
  const int src_strides[4] = {src->y_stride, src->uv_stride, src->uv_stride,
                              src->alpha_stride};
  const int src_widths[4] = {src->y_width, src->uv_width, src->uv_width,
                             src->alpha_width};
  const int src_heights[4] = {src->y_height, src->uv_height, src->uv_height,
                              src->alpha_height};

  uint8_t *const dsts[4] = {dst->y_buffer, dst->u_buffer, dst->v_buffer,
                            dst->alpha_buffer};
  const int dst_strides[4] = {dst->y_stride, dst->uv_stride, dst->uv_stride,
                              dst->alpha_stride};

  for (i = 0; i < MAX_MB_PLANE; ++i)
    vp9_post_proc_down_and_across(srcs[i], dsts[i],
                                  src_strides[i], dst_strides[i],
                                  src_heights[i], src_widths[i], ppl);
}

void vp9_denoise(const YV12_BUFFER_CONFIG *src, YV12_BUFFER_CONFIG *dst,
                 int q) {
  const int ppl = (int)(6.0e-05 * q * q * q - 0.0067 * q * q + 0.306 * q
                        + 0.0065 + 0.5);
  int i;

  const uint8_t *const srcs[4] = {src->y_buffer, src->u_buffer, src->v_buffer,
                                  src->alpha_buffer};
  const int src_strides[4] = {src->y_stride, src->uv_stride, src->uv_stride,
                              src->alpha_stride};
  const int src_widths[4] = {src->y_width, src->uv_width, src->uv_width,
                             src->alpha_width};
  const int src_heights[4] = {src->y_height, src->uv_height, src->uv_height,
                              src->alpha_height};

  uint8_t *const dsts[4] = {dst->y_buffer, dst->u_buffer, dst->v_buffer,
                            dst->alpha_buffer};
  const int dst_strides[4] = {dst->y_stride, dst->uv_stride, dst->uv_stride,
                              dst->alpha_stride};

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    const int src_stride = src_strides[i];
    const uint8_t *const src = srcs[i] + 2 * src_stride + 2;
    const int src_width = src_widths[i] - 4;
    const int src_height = src_heights[i] - 4;

    const int dst_stride = dst_strides[i];
    uint8_t *const dst = dsts[i] + 2 * dst_stride + 2;

    vp9_post_proc_down_and_across(src, dst, src_stride, dst_stride,
                                  src_height, src_width, ppl);
  }
}

double vp9_gaussian(double sigma, double mu, double x) {
  return 1 / (sigma * sqrt(2.0 * 3.14159265)) *
         (exp(-(x - mu) * (x - mu) / (2 * sigma * sigma)));
}

static void fillrd(struct postproc_state *state, int q, int a) {
  char char_dist[300];

  double sigma;
  int ai = a, qi = q, i;

  vp9_clear_system_state();

  sigma = ai + .5 + .6 * (63 - qi) / 63.0;

  /* set up a lookup table of 256 entries that matches
   * a gaussian distribution with sigma determined by q.
   */
  {
    double i;
    int next, j;

    next = 0;

    for (i = -32; i < 32; i++) {
      int a = (int)(.5 + 256 * vp9_gaussian(sigma, 0, i));

      if (a) {
        for (j = 0; j < a; j++) {
          char_dist[next + j] = (char) i;
        }

        next = next + j;
      }
    }

    for (; next < 256; next++)
      char_dist[next] = 0;
  }

  for (i = 0; i < 3072; i++) {
    state->noise[i] = char_dist[rand() & 0xff];  // NOLINT
  }

  for (i = 0; i < 16; i++) {
    state->blackclamp[i] = -char_dist[0];
    state->whiteclamp[i] = -char_dist[0];
    state->bothclamp[i] = -2 * char_dist[0];
  }

  state->last_q = q;
  state->last_noise = a;
}

/****************************************************************************
 *
 *  ROUTINE       : plane_add_noise_c
 *
 *  INPUTS        : unsigned char *Start  starting address of buffer to
 *                                        add gaussian noise to
 *                  unsigned int width    width of plane
 *                  unsigned int height   height of plane
 *                  int  pitch    distance between subsequent lines of frame
 *                  int  q        quantizer used to determine amount of noise
 *                                  to add
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void.
 *
 *  FUNCTION      : adds gaussian noise to a plane of pixels
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
void vp9_plane_add_noise_c(uint8_t *start, char *noise,
                           char blackclamp[16],
                           char whiteclamp[16],
                           char bothclamp[16],
                           unsigned int width, unsigned int height, int pitch) {
  unsigned int i, j;

  for (i = 0; i < height; i++) {
    uint8_t *pos = start + i * pitch;
    char  *ref = (char *)(noise + (rand() & 0xff));  // NOLINT

    for (j = 0; j < width; j++) {
      if (pos[j] < blackclamp[0])
        pos[j] = blackclamp[0];

      if (pos[j] > 255 + whiteclamp[0])
        pos[j] = 255 + whiteclamp[0];

      pos[j] += ref[j];
    }
  }
}

/* Blend the macro block with a solid colored square.  Leave the
 * edges unblended to give distinction to macro blocks in areas
 * filled with the same color block.
 */
void vp9_blend_mb_inner_c(uint8_t *y, uint8_t *u, uint8_t *v,
                          int y1, int u1, int v1, int alpha, int stride) {
  int i, j;
  int y1_const = y1 * ((1 << 16) - alpha);
  int u1_const = u1 * ((1 << 16) - alpha);
  int v1_const = v1 * ((1 << 16) - alpha);

  y += 2 * stride + 2;
  for (i = 0; i < 12; i++) {
    for (j = 0; j < 12; j++) {
      y[j] = (y[j] * alpha + y1_const) >> 16;
    }
    y += stride;
  }

  stride >>= 1;

  u += stride + 1;
  v += stride + 1;

  for (i = 0; i < 6; i++) {
    for (j = 0; j < 6; j++) {
      u[j] = (u[j] * alpha + u1_const) >> 16;
      v[j] = (v[j] * alpha + v1_const) >> 16;
    }
    u += stride;
    v += stride;
  }
}

/* Blend only the edge of the macro block.  Leave center
 * unblended to allow for other visualizations to be layered.
 */
void vp9_blend_mb_outer_c(uint8_t *y, uint8_t *u, uint8_t *v,
                          int y1, int u1, int v1, int alpha, int stride) {
  int i, j;
  int y1_const = y1 * ((1 << 16) - alpha);
  int u1_const = u1 * ((1 << 16) - alpha);
  int v1_const = v1 * ((1 << 16) - alpha);

  for (i = 0; i < 2; i++) {
    for (j = 0; j < 16; j++) {
      y[j] = (y[j] * alpha + y1_const) >> 16;
    }
    y += stride;
  }

  for (i = 0; i < 12; i++) {
    y[0]  = (y[0] * alpha  + y1_const) >> 16;
    y[1]  = (y[1] * alpha  + y1_const) >> 16;
    y[14] = (y[14] * alpha + y1_const) >> 16;
    y[15] = (y[15] * alpha + y1_const) >> 16;
    y += stride;
  }

  for (i = 0; i < 2; i++) {
    for (j = 0; j < 16; j++) {
      y[j] = (y[j] * alpha + y1_const) >> 16;
    }
    y += stride;
  }

  stride >>= 1;

  for (j = 0; j < 8; j++) {
    u[j] = (u[j] * alpha + u1_const) >> 16;
    v[j] = (v[j] * alpha + v1_const) >> 16;
  }
  u += stride;
  v += stride;

  for (i = 0; i < 6; i++) {
    u[0] = (u[0] * alpha + u1_const) >> 16;
    v[0] = (v[0] * alpha + v1_const) >> 16;

    u[7] = (u[7] * alpha + u1_const) >> 16;
    v[7] = (v[7] * alpha + v1_const) >> 16;

    u += stride;
    v += stride;
  }

  for (j = 0; j < 8; j++) {
    u[j] = (u[j] * alpha + u1_const) >> 16;
    v[j] = (v[j] * alpha + v1_const) >> 16;
  }
}

void vp9_blend_b_c(uint8_t *y, uint8_t *u, uint8_t *v,
                   int y1, int u1, int v1, int alpha, int stride) {
  int i, j;
  int y1_const = y1 * ((1 << 16) - alpha);
  int u1_const = u1 * ((1 << 16) - alpha);
  int v1_const = v1 * ((1 << 16) - alpha);

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      y[j] = (y[j] * alpha + y1_const) >> 16;
    }
    y += stride;
  }

  stride >>= 1;

  for (i = 0; i < 2; i++) {
    for (j = 0; j < 2; j++) {
      u[j] = (u[j] * alpha + u1_const) >> 16;
      v[j] = (v[j] * alpha + v1_const) >> 16;
    }
    u += stride;
    v += stride;
  }
}

static void constrain_line(int x0, int *x1, int y0, int *y1,
                           int width, int height) {
  int dx;
  int dy;

  if (*x1 > width) {
    dx = *x1 - x0;
    dy = *y1 - y0;

    *x1 = width;
    if (dx)
      *y1 = ((width - x0) * dy) / dx + y0;
  }
  if (*x1 < 0) {
    dx = *x1 - x0;
    dy = *y1 - y0;

    *x1 = 0;
    if (dx)
      *y1 = ((0 - x0) * dy) / dx + y0;
  }
  if (*y1 > height) {
    dx = *x1 - x0;
    dy = *y1 - y0;

    *y1 = height;
    if (dy)
      *x1 = ((height - y0) * dx) / dy + x0;
  }
  if (*y1 < 0) {
    dx = *x1 - x0;
    dy = *y1 - y0;

    *y1 = 0;
    if (dy)
      *x1 = ((0 - y0) * dx) / dy + x0;
  }
}

int vp9_post_proc_frame(struct VP9Common *cm,
                        YV12_BUFFER_CONFIG *dest, vp9_ppflags_t *ppflags) {
  int q = cm->lf.filter_level * 10 / 6;
  int flags = ppflags->post_proc_flag;
  int deblock_level = ppflags->deblocking_level;
  int noise_level = ppflags->noise_level;

  if (!cm->frame_to_show)
    return -1;

  if (q > 63)
    q = 63;

  if (!flags) {
    *dest = *cm->frame_to_show;
    return 0;
  }

#if ARCH_X86||ARCH_X86_64
  vpx_reset_mmx_state();
#endif

  if (flags & VP9D_DEMACROBLOCK) {
    deblock_and_de_macro_block(cm->frame_to_show, &cm->post_proc_buffer,
                               q + (deblock_level - 5) * 10, 1, 0);
  } else if (flags & VP9D_DEBLOCK) {
    vp9_deblock(cm->frame_to_show, &cm->post_proc_buffer, q);
  } else {
    vp8_yv12_copy_frame(cm->frame_to_show, &cm->post_proc_buffer);
  }

  if (flags & VP9D_ADDNOISE) {
    if (cm->postproc_state.last_q != q
        || cm->postproc_state.last_noise != noise_level) {
      fillrd(&cm->postproc_state, 63 - q, noise_level);
    }

    vp9_plane_add_noise(cm->post_proc_buffer.y_buffer,
                        cm->postproc_state.noise,
                        cm->postproc_state.blackclamp,
                        cm->postproc_state.whiteclamp,
                        cm->postproc_state.bothclamp,
                        cm->post_proc_buffer.y_width,
                        cm->post_proc_buffer.y_height,
                        cm->post_proc_buffer.y_stride);
  }

#if 0 && CONFIG_POSTPROC_VISUALIZER
  if (flags & VP9D_DEBUG_TXT_FRAME_INFO) {
    char message[512];
    snprintf(message, sizeof(message) -1,
             "F%1dG%1dQ%3dF%3dP%d_s%dx%d",
             (cm->frame_type == KEY_FRAME),
             cm->refresh_golden_frame,
             cm->base_qindex,
             cm->filter_level,
             flags,
             cm->mb_cols, cm->mb_rows);
    vp9_blit_text(message, cm->post_proc_buffer.y_buffer,
                  cm->post_proc_buffer.y_stride);
  }

  if (flags & VP9D_DEBUG_TXT_MBLK_MODES) {
    int i, j;
    uint8_t *y_ptr;
    YV12_BUFFER_CONFIG *post = &cm->post_proc_buffer;
    int mb_rows = post->y_height >> 4;
    int mb_cols = post->y_width  >> 4;
    int mb_index = 0;
    MODE_INFO *mi = cm->mi;

    y_ptr = post->y_buffer + 4 * post->y_stride + 4;

    /* vp9_filter each macro block */
    for (i = 0; i < mb_rows; i++) {
      for (j = 0; j < mb_cols; j++) {
        char zz[4];

        snprintf(zz, sizeof(zz) - 1, "%c", mi[mb_index].mbmi.mode + 'a');

        vp9_blit_text(zz, y_ptr, post->y_stride);
        mb_index++;
        y_ptr += 16;
      }

      mb_index++; /* border */
      y_ptr += post->y_stride  * 16 - post->y_width;
    }
  }

  if (flags & VP9D_DEBUG_TXT_DC_DIFF) {
    int i, j;
    uint8_t *y_ptr;
    YV12_BUFFER_CONFIG *post = &cm->post_proc_buffer;
    int mb_rows = post->y_height >> 4;
    int mb_cols = post->y_width  >> 4;
    int mb_index = 0;
    MODE_INFO *mi = cm->mi;

    y_ptr = post->y_buffer + 4 * post->y_stride + 4;

    /* vp9_filter each macro block */
    for (i = 0; i < mb_rows; i++) {
      for (j = 0; j < mb_cols; j++) {
        char zz[4];
        int dc_diff = !(mi[mb_index].mbmi.mode != I4X4_PRED &&
                        mi[mb_index].mbmi.mode != SPLITMV &&
                        mi[mb_index].mbmi.skip_coeff);

        if (cm->frame_type == KEY_FRAME)
          snprintf(zz, sizeof(zz) - 1, "a");
        else
          snprintf(zz, sizeof(zz) - 1, "%c", dc_diff + '0');

        vp9_blit_text(zz, y_ptr, post->y_stride);
        mb_index++;
        y_ptr += 16;
      }

      mb_index++; /* border */
      y_ptr += post->y_stride  * 16 - post->y_width;
    }
  }

  if (flags & VP9D_DEBUG_TXT_RATE_INFO) {
    char message[512];
    snprintf(message, sizeof(message),
             "Bitrate: %10.2f framerate: %10.2f ",
             cm->bitrate, cm->framerate);
    vp9_blit_text(message, cm->post_proc_buffer.y_buffer,
                  cm->post_proc_buffer.y_stride);
  }

  /* Draw motion vectors */
  if ((flags & VP9D_DEBUG_DRAW_MV) && ppflags->display_mv_flag) {
    YV12_BUFFER_CONFIG *post = &cm->post_proc_buffer;
    int width  = post->y_width;
    int height = post->y_height;
    uint8_t *y_buffer = cm->post_proc_buffer.y_buffer;
    int y_stride = cm->post_proc_buffer.y_stride;
    MODE_INFO *mi = cm->mi;
    int x0, y0;

    for (y0 = 0; y0 < height; y0 += 16) {
      for (x0 = 0; x0 < width; x0 += 16) {
        int x1, y1;

        if (!(ppflags->display_mv_flag & (1 << mi->mbmi.mode))) {
          mi++;
          continue;
        }

        if (mi->mbmi.mode == SPLITMV) {
          switch (mi->mbmi.partitioning) {
            case PARTITIONING_16X8 : {  /* mv_top_bottom */
              union b_mode_info *bmi = &mi->bmi[0];
              MV *mv = &bmi->mv.as_mv;

              x1 = x0 + 8 + (mv->col >> 3);
              y1 = y0 + 4 + (mv->row >> 3);

              constrain_line(x0 + 8, &x1, y0 + 4, &y1, width, height);
              vp9_blit_line(x0 + 8,  x1, y0 + 4,  y1, y_buffer, y_stride);

              bmi = &mi->bmi[8];

              x1 = x0 + 8 + (mv->col >> 3);
              y1 = y0 + 12 + (mv->row >> 3);

              constrain_line(x0 + 8, &x1, y0 + 12, &y1, width, height);
              vp9_blit_line(x0 + 8,  x1, y0 + 12,  y1, y_buffer, y_stride);

              break;
            }
            case PARTITIONING_8X16 : {  /* mv_left_right */
              union b_mode_info *bmi = &mi->bmi[0];
              MV *mv = &bmi->mv.as_mv;

              x1 = x0 + 4 + (mv->col >> 3);
              y1 = y0 + 8 + (mv->row >> 3);

              constrain_line(x0 + 4, &x1, y0 + 8, &y1, width, height);
              vp9_blit_line(x0 + 4,  x1, y0 + 8,  y1, y_buffer, y_stride);

              bmi = &mi->bmi[2];

              x1 = x0 + 12 + (mv->col >> 3);
              y1 = y0 + 8 + (mv->row >> 3);

              constrain_line(x0 + 12, &x1, y0 + 8, &y1, width, height);
              vp9_blit_line(x0 + 12,  x1, y0 + 8,  y1, y_buffer, y_stride);

              break;
            }
            case PARTITIONING_8X8 : {  /* mv_quarters   */
              union b_mode_info *bmi = &mi->bmi[0];
              MV *mv = &bmi->mv.as_mv;

              x1 = x0 + 4 + (mv->col >> 3);
              y1 = y0 + 4 + (mv->row >> 3);

              constrain_line(x0 + 4, &x1, y0 + 4, &y1, width, height);
              vp9_blit_line(x0 + 4,  x1, y0 + 4,  y1, y_buffer, y_stride);

              bmi = &mi->bmi[2];

              x1 = x0 + 12 + (mv->col >> 3);
              y1 = y0 + 4 + (mv->row >> 3);

              constrain_line(x0 + 12, &x1, y0 + 4, &y1, width, height);
              vp9_blit_line(x0 + 12,  x1, y0 + 4,  y1, y_buffer, y_stride);

              bmi = &mi->bmi[8];

              x1 = x0 + 4 + (mv->col >> 3);
              y1 = y0 + 12 + (mv->row >> 3);

              constrain_line(x0 + 4, &x1, y0 + 12, &y1, width, height);
              vp9_blit_line(x0 + 4,  x1, y0 + 12,  y1, y_buffer, y_stride);

              bmi = &mi->bmi[10];

              x1 = x0 + 12 + (mv->col >> 3);
              y1 = y0 + 12 + (mv->row >> 3);

              constrain_line(x0 + 12, &x1, y0 + 12, &y1, width, height);
              vp9_blit_line(x0 + 12,  x1, y0 + 12,  y1, y_buffer, y_stride);
              break;
            }
            case PARTITIONING_4X4:
            default : {
              union b_mode_info *bmi = mi->bmi;
              int bx0, by0;

              for (by0 = y0; by0 < (y0 + 16); by0 += 4) {
                for (bx0 = x0; bx0 < (x0 + 16); bx0 += 4) {
                  MV *mv = &bmi->mv.as_mv;

                  x1 = bx0 + 2 + (mv->col >> 3);
                  y1 = by0 + 2 + (mv->row >> 3);

                  constrain_line(bx0 + 2, &x1, by0 + 2, &y1, width, height);
                  vp9_blit_line(bx0 + 2,  x1, by0 + 2,  y1, y_buffer, y_stride);

                  bmi++;
                }
              }
            }
          }
        } else if (is_inter_mode(mi->mbmi.mode)) {
          MV *mv = &mi->mbmi.mv.as_mv;
          const int lx0 = x0 + 8;
          const int ly0 = y0 + 8;

          x1 = lx0 + (mv->col >> 3);
          y1 = ly0 + (mv->row >> 3);

          if (x1 != lx0 && y1 != ly0) {
            constrain_line(lx0, &x1, ly0 - 1, &y1, width, height);
            vp9_blit_line(lx0,  x1, ly0 - 1,  y1, y_buffer, y_stride);

            constrain_line(lx0, &x1, ly0 + 1, &y1, width, height);
            vp9_blit_line(lx0,  x1, ly0 + 1,  y1, y_buffer, y_stride);
          } else {
            vp9_blit_line(lx0,  x1, ly0,  y1, y_buffer, y_stride);
          }
        }

        mi++;
      }
      mi++;
    }
  }

  /* Color in block modes */
  if ((flags & VP9D_DEBUG_CLR_BLK_MODES)
      && (ppflags->display_mb_modes_flag || ppflags->display_b_modes_flag)) {
    int y, x;
    YV12_BUFFER_CONFIG *post = &cm->post_proc_buffer;
    int width  = post->y_width;
    int height = post->y_height;
    uint8_t *y_ptr = cm->post_proc_buffer.y_buffer;
    uint8_t *u_ptr = cm->post_proc_buffer.u_buffer;
    uint8_t *v_ptr = cm->post_proc_buffer.v_buffer;
    int y_stride = cm->post_proc_buffer.y_stride;
    MODE_INFO *mi = cm->mi;

    for (y = 0; y < height; y += 16) {
      for (x = 0; x < width; x += 16) {
        int Y = 0, U = 0, V = 0;

        if (mi->mbmi.mode == I4X4_PRED &&
            ((ppflags->display_mb_modes_flag & I4X4_PRED) ||
             ppflags->display_b_modes_flag)) {
          int by, bx;
          uint8_t *yl, *ul, *vl;
          union b_mode_info *bmi = mi->bmi;

          yl = y_ptr + x;
          ul = u_ptr + (x >> 1);
          vl = v_ptr + (x >> 1);

          for (by = 0; by < 16; by += 4) {
            for (bx = 0; bx < 16; bx += 4) {
              if ((ppflags->display_b_modes_flag & (1 << mi->mbmi.mode))
                  || (ppflags->display_mb_modes_flag & I4X4_PRED)) {
                Y = B_PREDICTION_MODE_colors[bmi->as_mode][0];
                U = B_PREDICTION_MODE_colors[bmi->as_mode][1];
                V = B_PREDICTION_MODE_colors[bmi->as_mode][2];

                vp9_blend_b(yl + bx, ul + (bx >> 1), vl + (bx >> 1), Y, U, V,
                    0xc000, y_stride);
              }
              bmi++;
            }

            yl += y_stride * 4;
            ul += y_stride * 1;
            vl += y_stride * 1;
          }
        } else if (ppflags->display_mb_modes_flag & (1 << mi->mbmi.mode)) {
          Y = MB_PREDICTION_MODE_colors[mi->mbmi.mode][0];
          U = MB_PREDICTION_MODE_colors[mi->mbmi.mode][1];
          V = MB_PREDICTION_MODE_colors[mi->mbmi.mode][2];

          vp9_blend_mb_inner(y_ptr + x, u_ptr + (x >> 1), v_ptr + (x >> 1),
                             Y, U, V, 0xc000, y_stride);
        }

        mi++;
      }
      y_ptr += y_stride * 16;
      u_ptr += y_stride * 4;
      v_ptr += y_stride * 4;

      mi++;
    }
  }

  /* Color in frame reference blocks */
  if ((flags & VP9D_DEBUG_CLR_FRM_REF_BLKS) &&
      ppflags->display_ref_frame_flag) {
    int y, x;
    YV12_BUFFER_CONFIG *post = &cm->post_proc_buffer;
    int width  = post->y_width;
    int height = post->y_height;
    uint8_t *y_ptr = cm->post_proc_buffer.y_buffer;
    uint8_t *u_ptr = cm->post_proc_buffer.u_buffer;
    uint8_t *v_ptr = cm->post_proc_buffer.v_buffer;
    int y_stride = cm->post_proc_buffer.y_stride;
    MODE_INFO *mi = cm->mi;

    for (y = 0; y < height; y += 16) {
      for (x = 0; x < width; x += 16) {
        int Y = 0, U = 0, V = 0;

        if (ppflags->display_ref_frame_flag & (1 << mi->mbmi.ref_frame)) {
          Y = MV_REFERENCE_FRAME_colors[mi->mbmi.ref_frame][0];
          U = MV_REFERENCE_FRAME_colors[mi->mbmi.ref_frame][1];
          V = MV_REFERENCE_FRAME_colors[mi->mbmi.ref_frame][2];

          vp9_blend_mb_outer(y_ptr + x, u_ptr + (x >> 1), v_ptr + (x >> 1),
                             Y, U, V, 0xc000, y_stride);
        }

        mi++;
      }
      y_ptr += y_stride * 16;
      u_ptr += y_stride * 4;
      v_ptr += y_stride * 4;

      mi++;
    }
  }
#endif

  *dest = cm->post_proc_buffer;

  /* handle problem with extending borders */
  dest->y_width = cm->width;
  dest->y_height = cm->height;
  dest->uv_width = dest->y_width >> cm->subsampling_x;
  dest->uv_height = dest->y_height >> cm->subsampling_y;

  return 0;
}
