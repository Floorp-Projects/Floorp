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

#include <assert.h>
#include <math.h>
#include <string.h>

#include "config/aom_scale_rtcd.h"

#include "aom/aom_integer.h"
#include "av1/common/cdef.h"
#include "av1/common/cdef_block.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/reconinter.h"

int sb_all_skip(const AV1_COMMON *const cm, int mi_row, int mi_col) {
  int maxc, maxr;
  int skip = 1;
  maxc = cm->mi_cols - mi_col;
  maxr = cm->mi_rows - mi_row;

  maxr = AOMMIN(maxr, MI_SIZE_64X64);
  maxc = AOMMIN(maxc, MI_SIZE_64X64);

  for (int r = 0; r < maxr; r++) {
    for (int c = 0; c < maxc; c++) {
      skip =
          skip &&
          cm->mi_grid_visible[(mi_row + r) * cm->mi_stride + mi_col + c]->skip;
    }
  }
  return skip;
}

static int is_8x8_block_skip(MB_MODE_INFO **grid, int mi_row, int mi_col,
                             int mi_stride) {
  int is_skip = 1;
  for (int r = 0; r < mi_size_high[BLOCK_8X8]; ++r)
    for (int c = 0; c < mi_size_wide[BLOCK_8X8]; ++c)
      is_skip &= grid[(mi_row + r) * mi_stride + (mi_col + c)]->skip;

  return is_skip;
}

int sb_compute_cdef_list(const AV1_COMMON *const cm, int mi_row, int mi_col,
                         cdef_list *dlist, BLOCK_SIZE bs) {
  MB_MODE_INFO **grid = cm->mi_grid_visible;
  int maxc = cm->mi_cols - mi_col;
  int maxr = cm->mi_rows - mi_row;

  if (bs == BLOCK_128X128 || bs == BLOCK_128X64)
    maxc = AOMMIN(maxc, MI_SIZE_128X128);
  else
    maxc = AOMMIN(maxc, MI_SIZE_64X64);
  if (bs == BLOCK_128X128 || bs == BLOCK_64X128)
    maxr = AOMMIN(maxr, MI_SIZE_128X128);
  else
    maxr = AOMMIN(maxr, MI_SIZE_64X64);

  const int r_step = mi_size_high[BLOCK_8X8];
  const int c_step = mi_size_wide[BLOCK_8X8];
  const int r_shift = (r_step == 2);
  const int c_shift = (c_step == 2);

  assert(r_step == 1 || r_step == 2);
  assert(c_step == 1 || c_step == 2);

  int count = 0;

  for (int r = 0; r < maxr; r += r_step) {
    for (int c = 0; c < maxc; c += c_step) {
      if (!is_8x8_block_skip(grid, mi_row + r, mi_col + c, cm->mi_stride)) {
        dlist[count].by = r >> r_shift;
        dlist[count].bx = c >> c_shift;
        dlist[count].skip = 0;
        count++;
      }
    }
  }
  return count;
}

void copy_rect8_8bit_to_16bit_c(uint16_t *dst, int dstride, const uint8_t *src,
                                int sstride, int v, int h) {
  for (int i = 0; i < v; i++) {
    for (int j = 0; j < h; j++) {
      dst[i * dstride + j] = src[i * sstride + j];
    }
  }
}

void copy_rect8_16bit_to_16bit_c(uint16_t *dst, int dstride,
                                 const uint16_t *src, int sstride, int v,
                                 int h) {
  for (int i = 0; i < v; i++) {
    for (int j = 0; j < h; j++) {
      dst[i * dstride + j] = src[i * sstride + j];
    }
  }
}

static void copy_sb8_16(AOM_UNUSED AV1_COMMON *cm, uint16_t *dst, int dstride,
                        const uint8_t *src, int src_voffset, int src_hoffset,
                        int sstride, int vsize, int hsize) {
  if (cm->seq_params.use_highbitdepth) {
    const uint16_t *base =
        &CONVERT_TO_SHORTPTR(src)[src_voffset * sstride + src_hoffset];
    copy_rect8_16bit_to_16bit(dst, dstride, base, sstride, vsize, hsize);
  } else {
    const uint8_t *base = &src[src_voffset * sstride + src_hoffset];
    copy_rect8_8bit_to_16bit(dst, dstride, base, sstride, vsize, hsize);
  }
}

static INLINE void fill_rect(uint16_t *dst, int dstride, int v, int h,
                             uint16_t x) {
  for (int i = 0; i < v; i++) {
    for (int j = 0; j < h; j++) {
      dst[i * dstride + j] = x;
    }
  }
}

static INLINE void copy_rect(uint16_t *dst, int dstride, const uint16_t *src,
                             int sstride, int v, int h) {
  for (int i = 0; i < v; i++) {
    for (int j = 0; j < h; j++) {
      dst[i * dstride + j] = src[i * sstride + j];
    }
  }
}

void av1_cdef_frame(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                    MACROBLOCKD *xd) {
  const int num_planes = av1_num_planes(cm);
  DECLARE_ALIGNED(16, uint16_t, src[CDEF_INBUF_SIZE]);
  uint16_t *linebuf[3];
  uint16_t *colbuf[3];
  cdef_list dlist[MI_SIZE_64X64 * MI_SIZE_64X64];
  unsigned char *row_cdef, *prev_row_cdef, *curr_row_cdef;
  int cdef_count;
  int dir[CDEF_NBLOCKS][CDEF_NBLOCKS] = { { 0 } };
  int var[CDEF_NBLOCKS][CDEF_NBLOCKS] = { { 0 } };
  int mi_wide_l2[3];
  int mi_high_l2[3];
  int xdec[3];
  int ydec[3];
  int coeff_shift = AOMMAX(cm->seq_params.bit_depth - 8, 0);
  const int nvfb = (cm->mi_rows + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;
  const int nhfb = (cm->mi_cols + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;
  av1_setup_dst_planes(xd->plane, cm->seq_params.sb_size, frame, 0, 0, 0,
                       num_planes);
  row_cdef = aom_malloc(sizeof(*row_cdef) * (nhfb + 2) * 2);
  memset(row_cdef, 1, sizeof(*row_cdef) * (nhfb + 2) * 2);
  prev_row_cdef = row_cdef + 1;
  curr_row_cdef = prev_row_cdef + nhfb + 2;
  for (int pli = 0; pli < num_planes; pli++) {
    xdec[pli] = xd->plane[pli].subsampling_x;
    ydec[pli] = xd->plane[pli].subsampling_y;
    mi_wide_l2[pli] = MI_SIZE_LOG2 - xd->plane[pli].subsampling_x;
    mi_high_l2[pli] = MI_SIZE_LOG2 - xd->plane[pli].subsampling_y;
  }
  const int stride = (cm->mi_cols << MI_SIZE_LOG2) + 2 * CDEF_HBORDER;
  for (int pli = 0; pli < num_planes; pli++) {
    linebuf[pli] = aom_malloc(sizeof(*linebuf) * CDEF_VBORDER * stride);
    colbuf[pli] =
        aom_malloc(sizeof(*colbuf) *
                   ((CDEF_BLOCKSIZE << mi_high_l2[pli]) + 2 * CDEF_VBORDER) *
                   CDEF_HBORDER);
  }
  for (int fbr = 0; fbr < nvfb; fbr++) {
    for (int pli = 0; pli < num_planes; pli++) {
      const int block_height =
          (MI_SIZE_64X64 << mi_high_l2[pli]) + 2 * CDEF_VBORDER;
      fill_rect(colbuf[pli], CDEF_HBORDER, block_height, CDEF_HBORDER,
                CDEF_VERY_LARGE);
    }
    int cdef_left = 1;
    for (int fbc = 0; fbc < nhfb; fbc++) {
      int level, sec_strength;
      int uv_level, uv_sec_strength;
      int nhb, nvb;
      int cstart = 0;
      curr_row_cdef[fbc] = 0;
      if (cm->mi_grid_visible[MI_SIZE_64X64 * fbr * cm->mi_stride +
                              MI_SIZE_64X64 * fbc] == NULL ||
          cm->mi_grid_visible[MI_SIZE_64X64 * fbr * cm->mi_stride +
                              MI_SIZE_64X64 * fbc]
                  ->cdef_strength == -1) {
        cdef_left = 0;
        continue;
      }
      if (!cdef_left) cstart = -CDEF_HBORDER;
      nhb = AOMMIN(MI_SIZE_64X64, cm->mi_cols - MI_SIZE_64X64 * fbc);
      nvb = AOMMIN(MI_SIZE_64X64, cm->mi_rows - MI_SIZE_64X64 * fbr);
      int frame_top, frame_left, frame_bottom, frame_right;

      int mi_row = MI_SIZE_64X64 * fbr;
      int mi_col = MI_SIZE_64X64 * fbc;
      // for the current filter block, it's top left corner mi structure (mi_tl)
      // is first accessed to check whether the top and left boundaries are
      // frame boundaries. Then bottom-left and top-right mi structures are
      // accessed to check whether the bottom and right boundaries
      // (respectively) are frame boundaries.
      //
      // Note that we can't just check the bottom-right mi structure - eg. if
      // we're at the right-hand edge of the frame but not the bottom, then
      // the bottom-right mi is NULL but the bottom-left is not.
      frame_top = (mi_row == 0) ? 1 : 0;
      frame_left = (mi_col == 0) ? 1 : 0;

      if (fbr != nvfb - 1)
        frame_bottom = (mi_row + MI_SIZE_64X64 == cm->mi_rows) ? 1 : 0;
      else
        frame_bottom = 1;

      if (fbc != nhfb - 1)
        frame_right = (mi_col + MI_SIZE_64X64 == cm->mi_cols) ? 1 : 0;
      else
        frame_right = 1;

      const int mbmi_cdef_strength =
          cm->mi_grid_visible[MI_SIZE_64X64 * fbr * cm->mi_stride +
                              MI_SIZE_64X64 * fbc]
              ->cdef_strength;
      level = cm->cdef_strengths[mbmi_cdef_strength] / CDEF_SEC_STRENGTHS;
      sec_strength =
          cm->cdef_strengths[mbmi_cdef_strength] % CDEF_SEC_STRENGTHS;
      sec_strength += sec_strength == 3;
      uv_level = cm->cdef_uv_strengths[mbmi_cdef_strength] / CDEF_SEC_STRENGTHS;
      uv_sec_strength =
          cm->cdef_uv_strengths[mbmi_cdef_strength] % CDEF_SEC_STRENGTHS;
      uv_sec_strength += uv_sec_strength == 3;
      if ((level == 0 && sec_strength == 0 && uv_level == 0 &&
           uv_sec_strength == 0) ||
          (cdef_count = sb_compute_cdef_list(cm, fbr * MI_SIZE_64X64,
                                             fbc * MI_SIZE_64X64, dlist,
                                             BLOCK_64X64)) == 0) {
        cdef_left = 0;
        continue;
      }

      curr_row_cdef[fbc] = 1;
      for (int pli = 0; pli < num_planes; pli++) {
        int coffset;
        int rend, cend;
        int pri_damping = cm->cdef_pri_damping;
        int sec_damping = cm->cdef_sec_damping;
        int hsize = nhb << mi_wide_l2[pli];
        int vsize = nvb << mi_high_l2[pli];

        if (pli) {
          level = uv_level;
          sec_strength = uv_sec_strength;
        }

        if (fbc == nhfb - 1)
          cend = hsize;
        else
          cend = hsize + CDEF_HBORDER;

        if (fbr == nvfb - 1)
          rend = vsize;
        else
          rend = vsize + CDEF_VBORDER;

        coffset = fbc * MI_SIZE_64X64 << mi_wide_l2[pli];
        if (fbc == nhfb - 1) {
          /* On the last superblock column, fill in the right border with
             CDEF_VERY_LARGE to avoid filtering with the outside. */
          fill_rect(&src[cend + CDEF_HBORDER], CDEF_BSTRIDE,
                    rend + CDEF_VBORDER, hsize + CDEF_HBORDER - cend,
                    CDEF_VERY_LARGE);
        }
        if (fbr == nvfb - 1) {
          /* On the last superblock row, fill in the bottom border with
             CDEF_VERY_LARGE to avoid filtering with the outside. */
          fill_rect(&src[(rend + CDEF_VBORDER) * CDEF_BSTRIDE], CDEF_BSTRIDE,
                    CDEF_VBORDER, hsize + 2 * CDEF_HBORDER, CDEF_VERY_LARGE);
        }
        /* Copy in the pixels we need from the current superblock for
           deringing.*/
        copy_sb8_16(cm,
                    &src[CDEF_VBORDER * CDEF_BSTRIDE + CDEF_HBORDER + cstart],
                    CDEF_BSTRIDE, xd->plane[pli].dst.buf,
                    (MI_SIZE_64X64 << mi_high_l2[pli]) * fbr, coffset + cstart,
                    xd->plane[pli].dst.stride, rend, cend - cstart);
        if (!prev_row_cdef[fbc]) {
          copy_sb8_16(cm, &src[CDEF_HBORDER], CDEF_BSTRIDE,
                      xd->plane[pli].dst.buf,
                      (MI_SIZE_64X64 << mi_high_l2[pli]) * fbr - CDEF_VBORDER,
                      coffset, xd->plane[pli].dst.stride, CDEF_VBORDER, hsize);
        } else if (fbr > 0) {
          copy_rect(&src[CDEF_HBORDER], CDEF_BSTRIDE, &linebuf[pli][coffset],
                    stride, CDEF_VBORDER, hsize);
        } else {
          fill_rect(&src[CDEF_HBORDER], CDEF_BSTRIDE, CDEF_VBORDER, hsize,
                    CDEF_VERY_LARGE);
        }
        if (!prev_row_cdef[fbc - 1]) {
          copy_sb8_16(cm, src, CDEF_BSTRIDE, xd->plane[pli].dst.buf,
                      (MI_SIZE_64X64 << mi_high_l2[pli]) * fbr - CDEF_VBORDER,
                      coffset - CDEF_HBORDER, xd->plane[pli].dst.stride,
                      CDEF_VBORDER, CDEF_HBORDER);
        } else if (fbr > 0 && fbc > 0) {
          copy_rect(src, CDEF_BSTRIDE, &linebuf[pli][coffset - CDEF_HBORDER],
                    stride, CDEF_VBORDER, CDEF_HBORDER);
        } else {
          fill_rect(src, CDEF_BSTRIDE, CDEF_VBORDER, CDEF_HBORDER,
                    CDEF_VERY_LARGE);
        }
        if (!prev_row_cdef[fbc + 1]) {
          copy_sb8_16(cm, &src[CDEF_HBORDER + (nhb << mi_wide_l2[pli])],
                      CDEF_BSTRIDE, xd->plane[pli].dst.buf,
                      (MI_SIZE_64X64 << mi_high_l2[pli]) * fbr - CDEF_VBORDER,
                      coffset + hsize, xd->plane[pli].dst.stride, CDEF_VBORDER,
                      CDEF_HBORDER);
        } else if (fbr > 0 && fbc < nhfb - 1) {
          copy_rect(&src[hsize + CDEF_HBORDER], CDEF_BSTRIDE,
                    &linebuf[pli][coffset + hsize], stride, CDEF_VBORDER,
                    CDEF_HBORDER);
        } else {
          fill_rect(&src[hsize + CDEF_HBORDER], CDEF_BSTRIDE, CDEF_VBORDER,
                    CDEF_HBORDER, CDEF_VERY_LARGE);
        }
        if (cdef_left) {
          /* If we deringed the superblock on the left then we need to copy in
             saved pixels. */
          copy_rect(src, CDEF_BSTRIDE, colbuf[pli], CDEF_HBORDER,
                    rend + CDEF_VBORDER, CDEF_HBORDER);
        }
        /* Saving pixels in case we need to dering the superblock on the
            right. */
        copy_rect(colbuf[pli], CDEF_HBORDER, src + hsize, CDEF_BSTRIDE,
                  rend + CDEF_VBORDER, CDEF_HBORDER);
        copy_sb8_16(
            cm, &linebuf[pli][coffset], stride, xd->plane[pli].dst.buf,
            (MI_SIZE_64X64 << mi_high_l2[pli]) * (fbr + 1) - CDEF_VBORDER,
            coffset, xd->plane[pli].dst.stride, CDEF_VBORDER, hsize);

        if (frame_top) {
          fill_rect(src, CDEF_BSTRIDE, CDEF_VBORDER, hsize + 2 * CDEF_HBORDER,
                    CDEF_VERY_LARGE);
        }
        if (frame_left) {
          fill_rect(src, CDEF_BSTRIDE, vsize + 2 * CDEF_VBORDER, CDEF_HBORDER,
                    CDEF_VERY_LARGE);
        }
        if (frame_bottom) {
          fill_rect(&src[(vsize + CDEF_VBORDER) * CDEF_BSTRIDE], CDEF_BSTRIDE,
                    CDEF_VBORDER, hsize + 2 * CDEF_HBORDER, CDEF_VERY_LARGE);
        }
        if (frame_right) {
          fill_rect(&src[hsize + CDEF_HBORDER], CDEF_BSTRIDE,
                    vsize + 2 * CDEF_VBORDER, CDEF_HBORDER, CDEF_VERY_LARGE);
        }

        if (cm->seq_params.use_highbitdepth) {
          cdef_filter_fb(
              NULL,
              &CONVERT_TO_SHORTPTR(
                  xd->plane[pli]
                      .dst.buf)[xd->plane[pli].dst.stride *
                                    (MI_SIZE_64X64 * fbr << mi_high_l2[pli]) +
                                (fbc * MI_SIZE_64X64 << mi_wide_l2[pli])],
              xd->plane[pli].dst.stride,
              &src[CDEF_VBORDER * CDEF_BSTRIDE + CDEF_HBORDER], xdec[pli],
              ydec[pli], dir, NULL, var, pli, dlist, cdef_count, level,
              sec_strength, pri_damping, sec_damping, coeff_shift);
        } else {
          cdef_filter_fb(
              &xd->plane[pli]
                   .dst.buf[xd->plane[pli].dst.stride *
                                (MI_SIZE_64X64 * fbr << mi_high_l2[pli]) +
                            (fbc * MI_SIZE_64X64 << mi_wide_l2[pli])],
              NULL, xd->plane[pli].dst.stride,
              &src[CDEF_VBORDER * CDEF_BSTRIDE + CDEF_HBORDER], xdec[pli],
              ydec[pli], dir, NULL, var, pli, dlist, cdef_count, level,
              sec_strength, pri_damping, sec_damping, coeff_shift);
        }
      }
      cdef_left = 1;
    }
    {
      unsigned char *tmp = prev_row_cdef;
      prev_row_cdef = curr_row_cdef;
      curr_row_cdef = tmp;
    }
  }
  aom_free(row_cdef);
  for (int pli = 0; pli < num_planes; pli++) {
    aom_free(linebuf[pli]);
    aom_free(colbuf[pli]);
  }
}
