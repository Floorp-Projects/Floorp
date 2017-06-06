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

#include "./aom_scale_rtcd.h"
#include "aom/aom_integer.h"
#include "av1/common/cdef.h"
#include "av1/common/od_dering.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/reconinter.h"

int sb_all_skip(const AV1_COMMON *const cm, int mi_row, int mi_col) {
  int r, c;
  int maxc, maxr;
  int skip = 1;
  maxc = cm->mi_cols - mi_col;
  maxr = cm->mi_rows - mi_row;

  maxr = AOMMIN(maxr, cm->mib_size);
  maxc = AOMMIN(maxc, cm->mib_size);

  for (r = 0; r < maxr; r++) {
    for (c = 0; c < maxc; c++) {
      skip = skip &&
             cm->mi_grid_visible[(mi_row + r) * cm->mi_stride + mi_col + c]
                 ->mbmi.skip;
    }
  }
  return skip;
}

static int is_8x8_block_skip(MODE_INFO **grid, int mi_row, int mi_col,
                             int mi_stride) {
  int is_skip = 1;
  for (int r = 0; r < mi_size_high[BLOCK_8X8]; ++r)
    for (int c = 0; c < mi_size_wide[BLOCK_8X8]; ++c)
      is_skip &= grid[(mi_row + r) * mi_stride + (mi_col + c)]->mbmi.skip;

  return is_skip;
}

int sb_compute_dering_list(const AV1_COMMON *const cm, int mi_row, int mi_col,
                           dering_list *dlist, int filter_skip) {
  int r, c;
  int maxc, maxr;
  MODE_INFO **grid;
  int count = 0;
  grid = cm->mi_grid_visible;
  maxc = cm->mi_cols - mi_col;
  maxr = cm->mi_rows - mi_row;

  maxr = AOMMIN(maxr, cm->mib_size);
  maxc = AOMMIN(maxc, cm->mib_size);

  const int r_step = mi_size_high[BLOCK_8X8];
  const int c_step = mi_size_wide[BLOCK_8X8];
  const int r_shift = (r_step == 2);
  const int c_shift = (c_step == 2);

  assert(r_step == 1 || r_step == 2);
  assert(c_step == 1 || c_step == 2);

  if (filter_skip) {
    for (r = 0; r < maxr; r += r_step) {
      for (c = 0; c < maxc; c += c_step) {
        dlist[count].by = r >> r_shift;
        dlist[count].bx = c >> c_shift;
        dlist[count].skip =
            is_8x8_block_skip(grid, mi_row + r, mi_col + c, cm->mi_stride);
        count++;
      }
    }
  } else {
    for (r = 0; r < maxr; r += r_step) {
      for (c = 0; c < maxc; c += c_step) {
        if (!is_8x8_block_skip(grid, mi_row + r, mi_col + c, cm->mi_stride)) {
          dlist[count].by = r >> r_shift;
          dlist[count].bx = c >> c_shift;
          dlist[count].skip = 0;
          count++;
        }
      }
    }
  }
  return count;
}

void copy_rect8_8bit_to_16bit_c(uint16_t *dst, int dstride, const uint8_t *src,
                                int sstride, int v, int h) {
  int i, j;
  for (i = 0; i < v; i++) {
    for (j = 0; j < h; j++) {
      dst[i * dstride + j] = src[i * sstride + j];
    }
  }
}

void copy_rect8_16bit_to_16bit_c(uint16_t *dst, int dstride,
                                 const uint16_t *src, int sstride, int v,
                                 int h) {
  int i, j;
  for (i = 0; i < v; i++) {
    for (j = 0; j < h; j++) {
      dst[i * dstride + j] = src[i * sstride + j];
    }
  }
}

void copy_sb8_16(UNUSED AV1_COMMON *cm, uint16_t *dst, int dstride,
                 const uint8_t *src, int src_voffset, int src_hoffset,
                 int sstride, int vsize, int hsize) {
#if CONFIG_HIGHBITDEPTH
  if (cm->use_highbitdepth) {
    const uint16_t *base =
        &CONVERT_TO_SHORTPTR(src)[src_voffset * sstride + src_hoffset];
    copy_rect8_16bit_to_16bit(dst, dstride, base, sstride, vsize, hsize);
  } else {
#endif
    const uint8_t *base = &src[src_voffset * sstride + src_hoffset];
    copy_rect8_8bit_to_16bit(dst, dstride, base, sstride, vsize, hsize);
#if CONFIG_HIGHBITDEPTH
  }
#endif
}

static INLINE void fill_rect(uint16_t *dst, int dstride, int v, int h,
                             uint16_t x) {
  int i, j;
  for (i = 0; i < v; i++) {
    for (j = 0; j < h; j++) {
      dst[i * dstride + j] = x;
    }
  }
}

static INLINE void copy_rect(uint16_t *dst, int dstride, const uint16_t *src,
                             int sstride, int v, int h) {
  int i, j;
  for (i = 0; i < v; i++) {
    for (j = 0; j < h; j++) {
      dst[i * dstride + j] = src[i * sstride + j];
    }
  }
}

void av1_cdef_frame(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                    MACROBLOCKD *xd) {
  int sbr, sbc;
  int nhsb, nvsb;
  uint16_t src[OD_DERING_INBUF_SIZE];
  uint16_t *linebuf[3];
  uint16_t *colbuf[3];
  dering_list dlist[MAX_MIB_SIZE * MAX_MIB_SIZE];
  unsigned char *row_dering, *prev_row_dering, *curr_row_dering;
  int dering_count;
  int dir[OD_DERING_NBLOCKS][OD_DERING_NBLOCKS] = { { 0 } };
  int var[OD_DERING_NBLOCKS][OD_DERING_NBLOCKS] = { { 0 } };
  int stride;
  int mi_wide_l2[3];
  int mi_high_l2[3];
  int xdec[3];
  int ydec[3];
  int pli;
  int dering_left;
  int coeff_shift = AOMMAX(cm->bit_depth - 8, 0);
  int nplanes = 3;
  int chroma_dering =
      xd->plane[1].subsampling_x == xd->plane[1].subsampling_y &&
      xd->plane[2].subsampling_x == xd->plane[2].subsampling_y;
  nvsb = (cm->mi_rows + MAX_MIB_SIZE - 1) / MAX_MIB_SIZE;
  nhsb = (cm->mi_cols + MAX_MIB_SIZE - 1) / MAX_MIB_SIZE;
  av1_setup_dst_planes(xd->plane, cm->sb_size, frame, 0, 0);
  row_dering = aom_malloc(sizeof(*row_dering) * (nhsb + 2) * 2);
  memset(row_dering, 1, sizeof(*row_dering) * (nhsb + 2) * 2);
  prev_row_dering = row_dering + 1;
  curr_row_dering = prev_row_dering + nhsb + 2;
  for (pli = 0; pli < nplanes; pli++) {
    xdec[pli] = xd->plane[pli].subsampling_x;
    ydec[pli] = xd->plane[pli].subsampling_y;
    mi_wide_l2[pli] = MI_SIZE_LOG2 - xd->plane[pli].subsampling_x;
    mi_high_l2[pli] = MI_SIZE_LOG2 - xd->plane[pli].subsampling_y;
  }
  stride = (cm->mi_cols << MI_SIZE_LOG2) + 2 * OD_FILT_HBORDER;
  for (pli = 0; pli < nplanes; pli++) {
    linebuf[pli] = aom_malloc(sizeof(*linebuf) * OD_FILT_VBORDER * stride);
    colbuf[pli] =
        aom_malloc(sizeof(*colbuf) *
                   ((MAX_SB_SIZE << mi_high_l2[pli]) + 2 * OD_FILT_VBORDER) *
                   OD_FILT_HBORDER);
  }
  for (sbr = 0; sbr < nvsb; sbr++) {
    for (pli = 0; pli < nplanes; pli++) {
      const int block_height =
          (MAX_MIB_SIZE << mi_high_l2[pli]) + 2 * OD_FILT_VBORDER;
      fill_rect(colbuf[pli], OD_FILT_HBORDER, block_height, OD_FILT_HBORDER,
                OD_DERING_VERY_LARGE);
    }
    dering_left = 1;
    for (sbc = 0; sbc < nhsb; sbc++) {
      int level, clpf_strength;
      int uv_level, uv_clpf_strength;
      int nhb, nvb;
      int cstart = 0;
      curr_row_dering[sbc] = 0;
      if (cm->mi_grid_visible[MAX_MIB_SIZE * sbr * cm->mi_stride +
                              MAX_MIB_SIZE * sbc] == NULL ||
          cm->mi_grid_visible[MAX_MIB_SIZE * sbr * cm->mi_stride +
                              MAX_MIB_SIZE * sbc]
                  ->mbmi.cdef_strength == -1) {
        dering_left = 0;
        continue;
      }
      if (!dering_left) cstart = -OD_FILT_HBORDER;
      nhb = AOMMIN(MAX_MIB_SIZE, cm->mi_cols - MAX_MIB_SIZE * sbc);
      nvb = AOMMIN(MAX_MIB_SIZE, cm->mi_rows - MAX_MIB_SIZE * sbr);
      int tile_top, tile_left, tile_bottom, tile_right;
      int mi_idx = MAX_MIB_SIZE * sbr * cm->mi_stride + MAX_MIB_SIZE * sbc;
      BOUNDARY_TYPE boundary_tl =
          cm->mi_grid_visible[MAX_MIB_SIZE * sbr * cm->mi_stride +
                              MAX_MIB_SIZE * sbc]
              ->mbmi.boundary_info;
      tile_top = boundary_tl & TILE_ABOVE_BOUNDARY;
      tile_left = boundary_tl & TILE_LEFT_BOUNDARY;
      /* Right and bottom information appear unreliable, so we use the top
         and left flags for the next superblocks. */
      if (sbr != nvsb - 1 &&
          cm->mi_grid_visible[mi_idx + MAX_MIB_SIZE * cm->mi_stride])
        tile_bottom = cm->mi_grid_visible[mi_idx + MAX_MIB_SIZE * cm->mi_stride]
                          ->mbmi.boundary_info &
                      TILE_ABOVE_BOUNDARY;
      else
        tile_bottom = 1;
      if (sbc != nhsb - 1 && cm->mi_grid_visible[mi_idx + MAX_MIB_SIZE])
        tile_right =
            cm->mi_grid_visible[mi_idx + MAX_MIB_SIZE]->mbmi.boundary_info &
            TILE_LEFT_BOUNDARY;
      else
        tile_right = 1;
      const int mbmi_cdef_strength =
          cm->mi_grid_visible[MAX_MIB_SIZE * sbr * cm->mi_stride +
                              MAX_MIB_SIZE * sbc]
              ->mbmi.cdef_strength;
      level = cm->cdef_strengths[mbmi_cdef_strength] / CLPF_STRENGTHS;
      clpf_strength = cm->cdef_strengths[mbmi_cdef_strength] % CLPF_STRENGTHS;
      clpf_strength += clpf_strength == 3;
      uv_level = cm->cdef_uv_strengths[mbmi_cdef_strength] / CLPF_STRENGTHS;
      uv_clpf_strength =
          cm->cdef_uv_strengths[mbmi_cdef_strength] % CLPF_STRENGTHS;
      uv_clpf_strength += uv_clpf_strength == 3;
      if ((level == 0 && clpf_strength == 0 && uv_level == 0 &&
           uv_clpf_strength == 0) ||
          (dering_count = sb_compute_dering_list(
               cm, sbr * MAX_MIB_SIZE, sbc * MAX_MIB_SIZE, dlist,
               get_filter_skip(level) || get_filter_skip(uv_level))) == 0) {
        dering_left = 0;
        continue;
      }

      curr_row_dering[sbc] = 1;
      for (pli = 0; pli < nplanes; pli++) {
        uint16_t dst[MAX_SB_SIZE * MAX_SB_SIZE];
        int coffset;
        int rend, cend;
        int clpf_damping = cm->cdef_clpf_damping;
        int dering_damping = cm->cdef_dering_damping;
        int hsize = nhb << mi_wide_l2[pli];
        int vsize = nvb << mi_high_l2[pli];

        if (pli) {
          if (chroma_dering)
            level = uv_level;
          else
            level = 0;
          clpf_strength = uv_clpf_strength;
        }

        if (sbc == nhsb - 1)
          cend = hsize;
        else
          cend = hsize + OD_FILT_HBORDER;

        if (sbr == nvsb - 1)
          rend = vsize;
        else
          rend = vsize + OD_FILT_VBORDER;

        coffset = sbc * MAX_MIB_SIZE << mi_wide_l2[pli];
        if (sbc == nhsb - 1) {
          /* On the last superblock column, fill in the right border with
             OD_DERING_VERY_LARGE to avoid filtering with the outside. */
          fill_rect(&src[cend + OD_FILT_HBORDER], OD_FILT_BSTRIDE,
                    rend + OD_FILT_VBORDER, hsize + OD_FILT_HBORDER - cend,
                    OD_DERING_VERY_LARGE);
        }
        if (sbr == nvsb - 1) {
          /* On the last superblock row, fill in the bottom border with
             OD_DERING_VERY_LARGE to avoid filtering with the outside. */
          fill_rect(&src[(rend + OD_FILT_VBORDER) * OD_FILT_BSTRIDE],
                    OD_FILT_BSTRIDE, OD_FILT_VBORDER,
                    hsize + 2 * OD_FILT_HBORDER, OD_DERING_VERY_LARGE);
        }
        /* Copy in the pixels we need from the current superblock for
           deringing.*/
        copy_sb8_16(
            cm,
            &src[OD_FILT_VBORDER * OD_FILT_BSTRIDE + OD_FILT_HBORDER + cstart],
            OD_FILT_BSTRIDE, xd->plane[pli].dst.buf,
            (MAX_MIB_SIZE << mi_high_l2[pli]) * sbr, coffset + cstart,
            xd->plane[pli].dst.stride, rend, cend - cstart);
        if (!prev_row_dering[sbc]) {
          copy_sb8_16(cm, &src[OD_FILT_HBORDER], OD_FILT_BSTRIDE,
                      xd->plane[pli].dst.buf,
                      (MAX_MIB_SIZE << mi_high_l2[pli]) * sbr - OD_FILT_VBORDER,
                      coffset, xd->plane[pli].dst.stride, OD_FILT_VBORDER,
                      hsize);
        } else if (sbr > 0) {
          copy_rect(&src[OD_FILT_HBORDER], OD_FILT_BSTRIDE,
                    &linebuf[pli][coffset], stride, OD_FILT_VBORDER, hsize);
        } else {
          fill_rect(&src[OD_FILT_HBORDER], OD_FILT_BSTRIDE, OD_FILT_VBORDER,
                    hsize, OD_DERING_VERY_LARGE);
        }
        if (!prev_row_dering[sbc - 1]) {
          copy_sb8_16(cm, src, OD_FILT_BSTRIDE, xd->plane[pli].dst.buf,
                      (MAX_MIB_SIZE << mi_high_l2[pli]) * sbr - OD_FILT_VBORDER,
                      coffset - OD_FILT_HBORDER, xd->plane[pli].dst.stride,
                      OD_FILT_VBORDER, OD_FILT_HBORDER);
        } else if (sbr > 0 && sbc > 0) {
          copy_rect(src, OD_FILT_BSTRIDE,
                    &linebuf[pli][coffset - OD_FILT_HBORDER], stride,
                    OD_FILT_VBORDER, OD_FILT_HBORDER);
        } else {
          fill_rect(src, OD_FILT_BSTRIDE, OD_FILT_VBORDER, OD_FILT_HBORDER,
                    OD_DERING_VERY_LARGE);
        }
        if (!prev_row_dering[sbc + 1]) {
          copy_sb8_16(cm, &src[OD_FILT_HBORDER + (nhb << mi_wide_l2[pli])],
                      OD_FILT_BSTRIDE, xd->plane[pli].dst.buf,
                      (MAX_MIB_SIZE << mi_high_l2[pli]) * sbr - OD_FILT_VBORDER,
                      coffset + hsize, xd->plane[pli].dst.stride,
                      OD_FILT_VBORDER, OD_FILT_HBORDER);
        } else if (sbr > 0 && sbc < nhsb - 1) {
          copy_rect(&src[hsize + OD_FILT_HBORDER], OD_FILT_BSTRIDE,
                    &linebuf[pli][coffset + hsize], stride, OD_FILT_VBORDER,
                    OD_FILT_HBORDER);
        } else {
          fill_rect(&src[hsize + OD_FILT_HBORDER], OD_FILT_BSTRIDE,
                    OD_FILT_VBORDER, OD_FILT_HBORDER, OD_DERING_VERY_LARGE);
        }
        if (dering_left) {
          /* If we deringed the superblock on the left then we need to copy in
             saved pixels. */
          copy_rect(src, OD_FILT_BSTRIDE, colbuf[pli], OD_FILT_HBORDER,
                    rend + OD_FILT_VBORDER, OD_FILT_HBORDER);
        }
        /* Saving pixels in case we need to dering the superblock on the
            right. */
        copy_rect(colbuf[pli], OD_FILT_HBORDER, src + hsize, OD_FILT_BSTRIDE,
                  rend + OD_FILT_VBORDER, OD_FILT_HBORDER);
        copy_sb8_16(
            cm, &linebuf[pli][coffset], stride, xd->plane[pli].dst.buf,
            (MAX_MIB_SIZE << mi_high_l2[pli]) * (sbr + 1) - OD_FILT_VBORDER,
            coffset, xd->plane[pli].dst.stride, OD_FILT_VBORDER, hsize);

        if (tile_top) {
          fill_rect(src, OD_FILT_BSTRIDE, OD_FILT_VBORDER,
                    hsize + 2 * OD_FILT_HBORDER, OD_DERING_VERY_LARGE);
        }
        if (tile_left) {
          fill_rect(src, OD_FILT_BSTRIDE, vsize + 2 * OD_FILT_VBORDER,
                    OD_FILT_HBORDER, OD_DERING_VERY_LARGE);
        }
        if (tile_bottom) {
          fill_rect(&src[(vsize + OD_FILT_VBORDER) * OD_FILT_BSTRIDE],
                    OD_FILT_BSTRIDE, OD_FILT_VBORDER,
                    hsize + 2 * OD_FILT_HBORDER, OD_DERING_VERY_LARGE);
        }
        if (tile_right) {
          fill_rect(&src[hsize + OD_FILT_HBORDER], OD_FILT_BSTRIDE,
                    vsize + 2 * OD_FILT_VBORDER, OD_FILT_HBORDER,
                    OD_DERING_VERY_LARGE);
        }
#if CONFIG_HIGHBITDEPTH
        if (cm->use_highbitdepth) {
          od_dering(
              (uint8_t *)&CONVERT_TO_SHORTPTR(
                  xd->plane[pli]
                      .dst.buf)[xd->plane[pli].dst.stride *
                                    (MAX_MIB_SIZE * sbr << mi_high_l2[pli]) +
                                (sbc * MAX_MIB_SIZE << mi_wide_l2[pli])],
              xd->plane[pli].dst.stride, dst,
              &src[OD_FILT_VBORDER * OD_FILT_BSTRIDE + OD_FILT_HBORDER],
              xdec[pli], ydec[pli], dir, NULL, var, pli, dlist, dering_count,
              level, clpf_strength, clpf_damping, dering_damping, coeff_shift,
              0, 1);
        } else {
#endif
          od_dering(&xd->plane[pli]
                         .dst.buf[xd->plane[pli].dst.stride *
                                      (MAX_MIB_SIZE * sbr << mi_high_l2[pli]) +
                                  (sbc * MAX_MIB_SIZE << mi_wide_l2[pli])],
                    xd->plane[pli].dst.stride, dst,
                    &src[OD_FILT_VBORDER * OD_FILT_BSTRIDE + OD_FILT_HBORDER],
                    xdec[pli], ydec[pli], dir, NULL, var, pli, dlist,
                    dering_count, level, clpf_strength, clpf_damping,
                    dering_damping, coeff_shift, 0, 0);

#if CONFIG_HIGHBITDEPTH
        }
#endif
      }
      dering_left = 1;
    }
    {
      unsigned char *tmp;
      tmp = prev_row_dering;
      prev_row_dering = curr_row_dering;
      curr_row_dering = tmp;
    }
  }
  aom_free(row_dering);
  for (pli = 0; pli < nplanes; pli++) {
    aom_free(linebuf[pli]);
    aom_free(colbuf[pli]);
  }
}
