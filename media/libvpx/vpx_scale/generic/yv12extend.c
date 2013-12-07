/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include "./vpx_config.h"
#include "vpx/vpx_integer.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx_scale/yv12config.h"

static void extend_plane(uint8_t *const src, int src_stride,
                         int width, int height,
                         int extend_top, int extend_left,
                         int extend_bottom, int extend_right) {
  int i;
  const int linesize = extend_left + extend_right + width;

  /* copy the left and right most columns out */
  uint8_t *src_ptr1 = src;
  uint8_t *src_ptr2 = src + width - 1;
  uint8_t *dst_ptr1 = src - extend_left;
  uint8_t *dst_ptr2 = src + width;

  for (i = 0; i < height; ++i) {
    vpx_memset(dst_ptr1, src_ptr1[0], extend_left);
    vpx_memset(dst_ptr2, src_ptr2[0], extend_right);
    src_ptr1 += src_stride;
    src_ptr2 += src_stride;
    dst_ptr1 += src_stride;
    dst_ptr2 += src_stride;
  }

  /* Now copy the top and bottom lines into each line of the respective
   * borders
   */
  src_ptr1 = src - extend_left;
  src_ptr2 = src + src_stride * (height - 1) - extend_left;
  dst_ptr1 = src + src_stride * -extend_top - extend_left;
  dst_ptr2 = src + src_stride * height - extend_left;

  for (i = 0; i < extend_top; ++i) {
    vpx_memcpy(dst_ptr1, src_ptr1, linesize);
    dst_ptr1 += src_stride;
  }

  for (i = 0; i < extend_bottom; ++i) {
    vpx_memcpy(dst_ptr2, src_ptr2, linesize);
    dst_ptr2 += src_stride;
  }
}

void vp8_yv12_extend_frame_borders_c(YV12_BUFFER_CONFIG *ybf) {
  assert(ybf->y_height - ybf->y_crop_height < 16);
  assert(ybf->y_width - ybf->y_crop_width < 16);
  assert(ybf->y_height - ybf->y_crop_height >= 0);
  assert(ybf->y_width - ybf->y_crop_width >= 0);

  extend_plane(ybf->y_buffer, ybf->y_stride,
               ybf->y_crop_width, ybf->y_crop_height,
               ybf->border, ybf->border,
               ybf->border + ybf->y_height - ybf->y_crop_height,
               ybf->border + ybf->y_width - ybf->y_crop_width);

  extend_plane(ybf->u_buffer, ybf->uv_stride,
               (ybf->y_crop_width + 1) / 2, (ybf->y_crop_height + 1) / 2,
               ybf->border / 2, ybf->border / 2,
               (ybf->border + ybf->y_height - ybf->y_crop_height + 1) / 2,
               (ybf->border + ybf->y_width - ybf->y_crop_width + 1) / 2);

  extend_plane(ybf->v_buffer, ybf->uv_stride,
               (ybf->y_crop_width + 1) / 2, (ybf->y_crop_height + 1) / 2,
               ybf->border / 2, ybf->border / 2,
               (ybf->border + ybf->y_height - ybf->y_crop_height + 1) / 2,
               (ybf->border + ybf->y_width - ybf->y_crop_width + 1) / 2);
}

#if CONFIG_VP9
static void extend_frame(YV12_BUFFER_CONFIG *const ybf,
                         int subsampling_x, int subsampling_y,
                         int ext_size) {
  const int c_w = ybf->uv_crop_width;
  const int c_h = ybf->uv_crop_height;
  const int c_ext_size = ext_size >> 1;
  const int c_et = c_ext_size;
  const int c_el = c_ext_size;
  const int c_eb = c_ext_size + ybf->uv_height - ybf->uv_crop_height;
  const int c_er = c_ext_size + ybf->uv_width - ybf->uv_crop_width;

  assert(ybf->y_height - ybf->y_crop_height < 16);
  assert(ybf->y_width - ybf->y_crop_width < 16);
  assert(ybf->y_height - ybf->y_crop_height >= 0);
  assert(ybf->y_width - ybf->y_crop_width >= 0);

  extend_plane(ybf->y_buffer, ybf->y_stride,
               ybf->y_crop_width, ybf->y_crop_height,
               ext_size, ext_size,
               ext_size + ybf->y_height - ybf->y_crop_height,
               ext_size + ybf->y_width - ybf->y_crop_width);

  extend_plane(ybf->u_buffer, ybf->uv_stride,
               c_w, c_h, c_et, c_el, c_eb, c_er);

  extend_plane(ybf->v_buffer, ybf->uv_stride,
               c_w, c_h, c_et, c_el, c_eb, c_er);
}

void vp9_extend_frame_borders_c(YV12_BUFFER_CONFIG *ybf,
                                int subsampling_x, int subsampling_y) {
  extend_frame(ybf, subsampling_x, subsampling_y, ybf->border);
}

void vp9_extend_frame_inner_borders_c(YV12_BUFFER_CONFIG *ybf,
                                      int subsampling_x, int subsampling_y) {
  const int inner_bw = (ybf->border > VP9INNERBORDERINPIXELS) ?
                       VP9INNERBORDERINPIXELS : ybf->border;
  extend_frame(ybf, subsampling_x, subsampling_y, inner_bw);
}
#endif  // CONFIG_VP9

// Copies the source image into the destination image and updates the
// destination's UMV borders.
// Note: The frames are assumed to be identical in size.
void vp8_yv12_copy_frame_c(const YV12_BUFFER_CONFIG *src_ybc,
                           YV12_BUFFER_CONFIG *dst_ybc) {
  int row;
  const uint8_t *src = src_ybc->y_buffer;
  uint8_t *dst = dst_ybc->y_buffer;

#if 0
  /* These assertions are valid in the codec, but the libvpx-tester uses
   * this code slightly differently.
   */
  assert(src_ybc->y_width == dst_ybc->y_width);
  assert(src_ybc->y_height == dst_ybc->y_height);
#endif

  for (row = 0; row < src_ybc->y_height; ++row) {
    vpx_memcpy(dst, src, src_ybc->y_width);
    src += src_ybc->y_stride;
    dst += dst_ybc->y_stride;
  }

  src = src_ybc->u_buffer;
  dst = dst_ybc->u_buffer;

  for (row = 0; row < src_ybc->uv_height; ++row) {
    vpx_memcpy(dst, src, src_ybc->uv_width);
    src += src_ybc->uv_stride;
    dst += dst_ybc->uv_stride;
  }

  src = src_ybc->v_buffer;
  dst = dst_ybc->v_buffer;

  for (row = 0; row < src_ybc->uv_height; ++row) {
    vpx_memcpy(dst, src, src_ybc->uv_width);
    src += src_ybc->uv_stride;
    dst += dst_ybc->uv_stride;
  }

  vp8_yv12_extend_frame_borders_c(dst_ybc);
}

void vpx_yv12_copy_y_c(const YV12_BUFFER_CONFIG *src_ybc,
                       YV12_BUFFER_CONFIG *dst_ybc) {
  int row;
  const uint8_t *src = src_ybc->y_buffer;
  uint8_t *dst = dst_ybc->y_buffer;

  for (row = 0; row < src_ybc->y_height; ++row) {
    vpx_memcpy(dst, src, src_ybc->y_width);
    src += src_ybc->y_stride;
    dst += dst_ybc->y_stride;
  }
}
