/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vpx_mem/vpx_mem.h"

#include "vp9/common/vp9_common.h"
#include "vp9/encoder/vp9_extend.h"

static void copy_and_extend_plane(const uint8_t *src, int src_pitch,
                                  uint8_t *dst, int dst_pitch,
                                  int w, int h,
                                  int extend_top, int extend_left,
                                  int extend_bottom, int extend_right) {
  int i, linesize;

  // copy the left and right most columns out
  const uint8_t *src_ptr1 = src;
  const uint8_t *src_ptr2 = src + w - 1;
  uint8_t *dst_ptr1 = dst - extend_left;
  uint8_t *dst_ptr2 = dst + w;

  for (i = 0; i < h; i++) {
    vpx_memset(dst_ptr1, src_ptr1[0], extend_left);
    vpx_memcpy(dst_ptr1 + extend_left, src_ptr1, w);
    vpx_memset(dst_ptr2, src_ptr2[0], extend_right);
    src_ptr1 += src_pitch;
    src_ptr2 += src_pitch;
    dst_ptr1 += dst_pitch;
    dst_ptr2 += dst_pitch;
  }

  // Now copy the top and bottom lines into each line of the respective
  // borders
  src_ptr1 = dst - extend_left;
  src_ptr2 = dst + dst_pitch * (h - 1) - extend_left;
  dst_ptr1 = dst + dst_pitch * (-extend_top) - extend_left;
  dst_ptr2 = dst + dst_pitch * (h) - extend_left;
  linesize = extend_left + extend_right + w;

  for (i = 0; i < extend_top; i++) {
    vpx_memcpy(dst_ptr1, src_ptr1, linesize);
    dst_ptr1 += dst_pitch;
  }

  for (i = 0; i < extend_bottom; i++) {
    vpx_memcpy(dst_ptr2, src_ptr2, linesize);
    dst_ptr2 += dst_pitch;
  }
}

void vp9_copy_and_extend_frame(const YV12_BUFFER_CONFIG *src,
                               YV12_BUFFER_CONFIG *dst) {
  // Extend src frame in buffer
  // Altref filtering assumes 16 pixel extension
  const int et_y = 16;
  const int el_y = 16;
  // Motion estimation may use src block variance with the block size up
  // to 64x64, so the right and bottom need to be extended to 64 multiple
  // or up to 16, whichever is greater.
  const int eb_y = MAX(ALIGN_POWER_OF_TWO(src->y_width, 6) - src->y_width,
                       16);
  const int er_y = MAX(ALIGN_POWER_OF_TWO(src->y_height, 6) - src->y_height,
                       16);
  const int uv_width_subsampling = (src->uv_width != src->y_width);
  const int uv_height_subsampling = (src->uv_height != src->y_height);
  const int et_uv = et_y >> uv_height_subsampling;
  const int el_uv = el_y >> uv_width_subsampling;
  const int eb_uv = eb_y >> uv_height_subsampling;
  const int er_uv = er_y >> uv_width_subsampling;

#if CONFIG_ALPHA
  const int et_a = dst->border >> (dst->alpha_height != dst->y_height);
  const int el_a = dst->border >> (dst->alpha_width != dst->y_width);
  const int eb_a = et_a + dst->alpha_height - src->alpha_height;
  const int er_a = el_a + dst->alpha_width - src->alpha_width;

  copy_and_extend_plane(src->alpha_buffer, src->alpha_stride,
                        dst->alpha_buffer, dst->alpha_stride,
                        src->alpha_width, src->alpha_height,
                        et_a, el_a, eb_a, er_a);
#endif

  copy_and_extend_plane(src->y_buffer, src->y_stride,
                        dst->y_buffer, dst->y_stride,
                        src->y_width, src->y_height,
                        et_y, el_y, eb_y, er_y);

  copy_and_extend_plane(src->u_buffer, src->uv_stride,
                        dst->u_buffer, dst->uv_stride,
                        src->uv_width, src->uv_height,
                        et_uv, el_uv, eb_uv, er_uv);

  copy_and_extend_plane(src->v_buffer, src->uv_stride,
                        dst->v_buffer, dst->uv_stride,
                        src->uv_width, src->uv_height,
                        et_uv, el_uv, eb_uv, er_uv);
}

void vp9_copy_and_extend_frame_with_rect(const YV12_BUFFER_CONFIG *src,
                                         YV12_BUFFER_CONFIG *dst,
                                         int srcy, int srcx,
                                         int srch, int srcw) {
  // If the side is not touching the bounder then don't extend.
  const int et_y = srcy ? 0 : dst->border;
  const int el_y = srcx ? 0 : dst->border;
  const int eb_y = srcy + srch != src->y_height ? 0 :
                      dst->border + dst->y_height - src->y_height;
  const int er_y = srcx + srcw != src->y_width ? 0 :
                      dst->border + dst->y_width - src->y_width;
  const int src_y_offset = srcy * src->y_stride + srcx;
  const int dst_y_offset = srcy * dst->y_stride + srcx;

  const int et_uv = ROUND_POWER_OF_TWO(et_y, 1);
  const int el_uv = ROUND_POWER_OF_TWO(el_y, 1);
  const int eb_uv = ROUND_POWER_OF_TWO(eb_y, 1);
  const int er_uv = ROUND_POWER_OF_TWO(er_y, 1);
  const int src_uv_offset = ((srcy * src->uv_stride) >> 1) + (srcx >> 1);
  const int dst_uv_offset = ((srcy * dst->uv_stride) >> 1) + (srcx >> 1);
  const int srch_uv = ROUND_POWER_OF_TWO(srch, 1);
  const int srcw_uv = ROUND_POWER_OF_TWO(srcw, 1);

  copy_and_extend_plane(src->y_buffer + src_y_offset, src->y_stride,
                        dst->y_buffer + dst_y_offset, dst->y_stride,
                        srcw, srch,
                        et_y, el_y, eb_y, er_y);

  copy_and_extend_plane(src->u_buffer + src_uv_offset, src->uv_stride,
                        dst->u_buffer + dst_uv_offset, dst->uv_stride,
                        srcw_uv, srch_uv,
                        et_uv, el_uv, eb_uv, er_uv);

  copy_and_extend_plane(src->v_buffer + src_uv_offset, src->uv_stride,
                        dst->v_buffer + dst_uv_offset, dst->uv_stride,
                        srcw_uv, srch_uv,
                        et_uv, el_uv, eb_uv, er_uv);
}
