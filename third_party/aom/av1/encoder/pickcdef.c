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

#include <math.h>
#include <string.h>

#include "./aom_scale_rtcd.h"
#include "aom/aom_integer.h"
#include "av1/common/cdef.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/reconinter.h"
#include "av1/encoder/encoder.h"

#define TOTAL_STRENGTHS (DERING_STRENGTHS * CLPF_STRENGTHS)

/* Search for the best strength to add as an option, knowing we
   already selected nb_strengths options. */
static uint64_t search_one(int *lev, int nb_strengths,
                           uint64_t mse[][TOTAL_STRENGTHS], int sb_count) {
  uint64_t tot_mse[TOTAL_STRENGTHS];
  int i, j;
  uint64_t best_tot_mse = (uint64_t)1 << 63;
  int best_id = 0;
  memset(tot_mse, 0, sizeof(tot_mse));
  for (i = 0; i < sb_count; i++) {
    int gi;
    uint64_t best_mse = (uint64_t)1 << 63;
    /* Find best mse among already selected options. */
    for (gi = 0; gi < nb_strengths; gi++) {
      if (mse[i][lev[gi]] < best_mse) {
        best_mse = mse[i][lev[gi]];
      }
    }
    /* Find best mse when adding each possible new option. */
    for (j = 0; j < TOTAL_STRENGTHS; j++) {
      uint64_t best = best_mse;
      if (mse[i][j] < best) best = mse[i][j];
      tot_mse[j] += best;
    }
  }
  for (j = 0; j < TOTAL_STRENGTHS; j++) {
    if (tot_mse[j] < best_tot_mse) {
      best_tot_mse = tot_mse[j];
      best_id = j;
    }
  }
  lev[nb_strengths] = best_id;
  return best_tot_mse;
}

/* Search for the best luma+chroma strength to add as an option, knowing we
   already selected nb_strengths options. */
static uint64_t search_one_dual(int *lev0, int *lev1, int nb_strengths,
                                uint64_t (**mse)[TOTAL_STRENGTHS],
                                int sb_count) {
  uint64_t tot_mse[TOTAL_STRENGTHS][TOTAL_STRENGTHS];
  int i, j;
  uint64_t best_tot_mse = (uint64_t)1 << 63;
  int best_id0 = 0;
  int best_id1 = 0;
  memset(tot_mse, 0, sizeof(tot_mse));
  for (i = 0; i < sb_count; i++) {
    int gi;
    uint64_t best_mse = (uint64_t)1 << 63;
    /* Find best mse among already selected options. */
    for (gi = 0; gi < nb_strengths; gi++) {
      uint64_t curr = mse[0][i][lev0[gi]];
      curr += mse[1][i][lev1[gi]];
      if (curr < best_mse) {
        best_mse = curr;
      }
    }
    /* Find best mse when adding each possible new option. */
    for (j = 0; j < TOTAL_STRENGTHS; j++) {
      int k;
      for (k = 0; k < TOTAL_STRENGTHS; k++) {
        uint64_t best = best_mse;
        uint64_t curr = mse[0][i][j];
        curr += mse[1][i][k];
        if (curr < best) best = curr;
        tot_mse[j][k] += best;
      }
    }
  }
  for (j = 0; j < TOTAL_STRENGTHS; j++) {
    int k;
    for (k = 0; k < TOTAL_STRENGTHS; k++) {
      if (tot_mse[j][k] < best_tot_mse) {
        best_tot_mse = tot_mse[j][k];
        best_id0 = j;
        best_id1 = k;
      }
    }
  }
  lev0[nb_strengths] = best_id0;
  lev1[nb_strengths] = best_id1;
  return best_tot_mse;
}

/* Search for the set of strengths that minimizes mse. */
static uint64_t joint_strength_search(int *best_lev, int nb_strengths,
                                      uint64_t mse[][TOTAL_STRENGTHS],
                                      int sb_count) {
  uint64_t best_tot_mse;
  int i;
  best_tot_mse = (uint64_t)1 << 63;
  /* Greedy search: add one strength options at a time. */
  for (i = 0; i < nb_strengths; i++) {
    best_tot_mse = search_one(best_lev, i, mse, sb_count);
  }
  /* Trying to refine the greedy search by reconsidering each
     already-selected option. */
  for (i = 0; i < 4 * nb_strengths; i++) {
    int j;
    for (j = 0; j < nb_strengths - 1; j++) best_lev[j] = best_lev[j + 1];
    best_tot_mse = search_one(best_lev, nb_strengths - 1, mse, sb_count);
  }
  return best_tot_mse;
}

/* Search for the set of luma+chroma strengths that minimizes mse. */
static uint64_t joint_strength_search_dual(int *best_lev0, int *best_lev1,
                                           int nb_strengths,
                                           uint64_t (**mse)[TOTAL_STRENGTHS],
                                           int sb_count) {
  uint64_t best_tot_mse;
  int i;
  best_tot_mse = (uint64_t)1 << 63;
  /* Greedy search: add one strength options at a time. */
  for (i = 0; i < nb_strengths; i++) {
    best_tot_mse = search_one_dual(best_lev0, best_lev1, i, mse, sb_count);
  }
  /* Trying to refine the greedy search by reconsidering each
     already-selected option. */
  for (i = 0; i < 4 * nb_strengths; i++) {
    int j;
    for (j = 0; j < nb_strengths - 1; j++) {
      best_lev0[j] = best_lev0[j + 1];
      best_lev1[j] = best_lev1[j + 1];
    }
    best_tot_mse =
        search_one_dual(best_lev0, best_lev1, nb_strengths - 1, mse, sb_count);
  }
  return best_tot_mse;
}

/* FIXME: SSE-optimize this. */
static void copy_sb16_16(uint16_t *dst, int dstride, const uint16_t *src,
                         int src_voffset, int src_hoffset, int sstride,
                         int vsize, int hsize) {
  int r, c;
  const uint16_t *base = &src[src_voffset * sstride + src_hoffset];
  for (r = 0; r < vsize; r++) {
    for (c = 0; c < hsize; c++) {
      dst[r * dstride + c] = base[r * sstride + c];
    }
  }
}

static INLINE uint64_t dist_8x8_16bit(uint16_t *dst, int dstride, uint16_t *src,
                                      int sstride, int coeff_shift) {
  uint64_t svar = 0;
  uint64_t dvar = 0;
  uint64_t sum_s = 0;
  uint64_t sum_d = 0;
  uint64_t sum_s2 = 0;
  uint64_t sum_d2 = 0;
  uint64_t sum_sd = 0;
  int i, j;
  for (i = 0; i < 8; i++) {
    for (j = 0; j < 8; j++) {
      sum_s += src[i * sstride + j];
      sum_d += dst[i * dstride + j];
      sum_s2 += src[i * sstride + j] * src[i * sstride + j];
      sum_d2 += dst[i * dstride + j] * dst[i * dstride + j];
      sum_sd += src[i * sstride + j] * dst[i * dstride + j];
    }
  }
  /* Compute the variance -- the calculation cannot go negative. */
  svar = sum_s2 - ((sum_s * sum_s + 32) >> 6);
  dvar = sum_d2 - ((sum_d * sum_d + 32) >> 6);
  return (uint64_t)floor(
      .5 +
      (sum_d2 + sum_s2 - 2 * sum_sd) * .5 *
          (svar + dvar + (400 << 2 * coeff_shift)) /
          (sqrt((20000 << 4 * coeff_shift) + svar * (double)dvar)));
}

static INLINE uint64_t mse_8x8_16bit(uint16_t *dst, int dstride, uint16_t *src,
                                     int sstride) {
  uint64_t sum = 0;
  int i, j;
  for (i = 0; i < 8; i++) {
    for (j = 0; j < 8; j++) {
      int e = dst[i * dstride + j] - src[i * sstride + j];
      sum += e * e;
    }
  }
  return sum;
}

static INLINE uint64_t mse_4x4_16bit(uint16_t *dst, int dstride, uint16_t *src,
                                     int sstride) {
  uint64_t sum = 0;
  int i, j;
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      int e = dst[i * dstride + j] - src[i * sstride + j];
      sum += e * e;
    }
  }
  return sum;
}

/* Compute MSE only on the blocks we filtered. */
uint64_t compute_dering_dist(uint16_t *dst, int dstride, uint16_t *src,
                             dering_list *dlist, int dering_count,
                             BLOCK_SIZE bsize, int coeff_shift, int pli) {
  uint64_t sum = 0;
  int bi, bx, by;
  if (bsize == BLOCK_8X8) {
    for (bi = 0; bi < dering_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      if (pli == 0) {
        sum += dist_8x8_16bit(&dst[(by << 3) * dstride + (bx << 3)], dstride,
                              &src[bi << (3 + 3)], 8, coeff_shift);
      } else {
        sum += mse_8x8_16bit(&dst[(by << 3) * dstride + (bx << 3)], dstride,
                             &src[bi << (3 + 3)], 8);
      }
    }
  } else if (bsize == BLOCK_4X8) {
    for (bi = 0; bi < dering_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      sum += mse_4x4_16bit(&dst[(by << 3) * dstride + (bx << 2)], dstride,
                           &src[bi << (3 + 2)], 4);
      sum += mse_4x4_16bit(&dst[((by << 3) + 4) * dstride + (bx << 2)], dstride,
                           &src[(bi << (3 + 2)) + 4 * 4], 4);
    }
  } else if (bsize == BLOCK_8X4) {
    for (bi = 0; bi < dering_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      sum += mse_4x4_16bit(&dst[(by << 2) * dstride + (bx << 3)], dstride,
                           &src[bi << (2 + 3)], 8);
      sum += mse_4x4_16bit(&dst[(by << 2) * dstride + (bx << 3) + 4], dstride,
                           &src[(bi << (2 + 3)) + 4], 8);
    }
  } else {
    assert(bsize == BLOCK_4X4);
    for (bi = 0; bi < dering_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      sum += mse_4x4_16bit(&dst[(by << 2) * dstride + (bx << 2)], dstride,
                           &src[bi << (2 + 2)], 4);
    }
  }
  return sum >> 2 * coeff_shift;
}

void av1_cdef_search(YV12_BUFFER_CONFIG *frame, const YV12_BUFFER_CONFIG *ref,
                     AV1_COMMON *cm, MACROBLOCKD *xd) {
  int r, c;
  int sbr, sbc;
  uint16_t *src[3];
  uint16_t *ref_coeff[3];
  dering_list dlist[MAX_MIB_SIZE * MAX_MIB_SIZE];
  int dir[OD_DERING_NBLOCKS][OD_DERING_NBLOCKS] = { { 0 } };
  int var[OD_DERING_NBLOCKS][OD_DERING_NBLOCKS] = { { 0 } };
  int stride[3];
  int bsize[3];
  int mi_wide_l2[3];
  int mi_high_l2[3];
  int xdec[3];
  int ydec[3];
  int pli;
  int dering_count;
  int coeff_shift = AOMMAX(cm->bit_depth - 8, 0);
  uint64_t best_tot_mse = (uint64_t)1 << 63;
  uint64_t tot_mse;
  int sb_count;
  int nvsb = (cm->mi_rows + MAX_MIB_SIZE - 1) / MAX_MIB_SIZE;
  int nhsb = (cm->mi_cols + MAX_MIB_SIZE - 1) / MAX_MIB_SIZE;
  int *sb_index = aom_malloc(nvsb * nhsb * sizeof(*sb_index));
  int *selected_strength = aom_malloc(nvsb * nhsb * sizeof(*sb_index));
  uint64_t(*mse[2])[TOTAL_STRENGTHS];
  int clpf_damping = 3 + (cm->base_qindex >> 6);
  int dering_damping = 6;
  int i;
  int nb_strengths;
  int nb_strength_bits;
  int quantizer;
  double lambda;
  int nplanes = 3;
  DECLARE_ALIGNED(32, uint16_t, inbuf[OD_DERING_INBUF_SIZE]);
  uint16_t *in;
  DECLARE_ALIGNED(32, uint16_t, tmp_dst[MAX_SB_SQUARE]);
  int chroma_dering =
      xd->plane[1].subsampling_x == xd->plane[1].subsampling_y &&
      xd->plane[2].subsampling_x == xd->plane[2].subsampling_y;
  quantizer =
      av1_ac_quant(cm->base_qindex, 0, cm->bit_depth) >> (cm->bit_depth - 8);
  lambda = .12 * quantizer * quantizer / 256.;

  av1_setup_dst_planes(xd->plane, cm->sb_size, frame, 0, 0);
  mse[0] = aom_malloc(sizeof(**mse) * nvsb * nhsb);
  mse[1] = aom_malloc(sizeof(**mse) * nvsb * nhsb);
  for (pli = 0; pli < nplanes; pli++) {
    uint8_t *ref_buffer;
    int ref_stride;
    switch (pli) {
      case 0:
        ref_buffer = ref->y_buffer;
        ref_stride = ref->y_stride;
        break;
      case 1:
        ref_buffer = ref->u_buffer;
        ref_stride = ref->uv_stride;
        break;
      case 2:
        ref_buffer = ref->v_buffer;
        ref_stride = ref->uv_stride;
        break;
    }
    src[pli] = aom_memalign(
        32, sizeof(*src) * cm->mi_rows * cm->mi_cols * MI_SIZE * MI_SIZE);
    ref_coeff[pli] = aom_memalign(
        32, sizeof(*ref_coeff) * cm->mi_rows * cm->mi_cols * MI_SIZE * MI_SIZE);
    xdec[pli] = xd->plane[pli].subsampling_x;
    ydec[pli] = xd->plane[pli].subsampling_y;
    bsize[pli] = ydec[pli] ? (xdec[pli] ? BLOCK_4X4 : BLOCK_8X4)
                           : (xdec[pli] ? BLOCK_4X8 : BLOCK_8X8);
    stride[pli] = cm->mi_cols << MI_SIZE_LOG2;
    mi_wide_l2[pli] = MI_SIZE_LOG2 - xd->plane[pli].subsampling_x;
    mi_high_l2[pli] = MI_SIZE_LOG2 - xd->plane[pli].subsampling_y;

    const int frame_height =
        (cm->mi_rows * MI_SIZE) >> xd->plane[pli].subsampling_y;
    const int frame_width =
        (cm->mi_cols * MI_SIZE) >> xd->plane[pli].subsampling_x;

    for (r = 0; r < frame_height; ++r) {
      for (c = 0; c < frame_width; ++c) {
#if CONFIG_HIGHBITDEPTH
        if (cm->use_highbitdepth) {
          src[pli][r * stride[pli] + c] = CONVERT_TO_SHORTPTR(
              xd->plane[pli].dst.buf)[r * xd->plane[pli].dst.stride + c];
          ref_coeff[pli][r * stride[pli] + c] =
              CONVERT_TO_SHORTPTR(ref_buffer)[r * ref_stride + c];
        } else {
#endif
          src[pli][r * stride[pli] + c] =
              xd->plane[pli].dst.buf[r * xd->plane[pli].dst.stride + c];
          ref_coeff[pli][r * stride[pli] + c] = ref_buffer[r * ref_stride + c];
#if CONFIG_HIGHBITDEPTH
        }
#endif
      }
    }
  }
  in = inbuf + OD_FILT_VBORDER * OD_FILT_BSTRIDE + OD_FILT_HBORDER;
  sb_count = 0;
  for (sbr = 0; sbr < nvsb; ++sbr) {
    for (sbc = 0; sbc < nhsb; ++sbc) {
      int nvb, nhb;
      int gi;
      int dirinit = 0;
      nhb = AOMMIN(MAX_MIB_SIZE, cm->mi_cols - MAX_MIB_SIZE * sbc);
      nvb = AOMMIN(MAX_MIB_SIZE, cm->mi_rows - MAX_MIB_SIZE * sbr);
      cm->mi_grid_visible[MAX_MIB_SIZE * sbr * cm->mi_stride +
                          MAX_MIB_SIZE * sbc]
          ->mbmi.cdef_strength = -1;
      if (sb_all_skip(cm, sbr * MAX_MIB_SIZE, sbc * MAX_MIB_SIZE)) continue;
      dering_count = sb_compute_dering_list(cm, sbr * MAX_MIB_SIZE,
                                            sbc * MAX_MIB_SIZE, dlist, 1);
      for (pli = 0; pli < nplanes; pli++) {
        for (i = 0; i < OD_DERING_INBUF_SIZE; i++)
          inbuf[i] = OD_DERING_VERY_LARGE;
        for (gi = 0; gi < TOTAL_STRENGTHS; gi++) {
          int threshold;
          uint64_t curr_mse;
          int clpf_strength;
          threshold = gi / CLPF_STRENGTHS;
          if (pli > 0 && !chroma_dering) threshold = 0;
          /* We avoid filtering the pixels for which some of the pixels to
             average
             are outside the frame. We could change the filter instead, but it
             would add special cases for any future vectorization. */
          int yoff = OD_FILT_VBORDER * (sbr != 0);
          int xoff = OD_FILT_HBORDER * (sbc != 0);
          int ysize = (nvb << mi_high_l2[pli]) +
                      OD_FILT_VBORDER * (sbr != nvsb - 1) + yoff;
          int xsize = (nhb << mi_wide_l2[pli]) +
                      OD_FILT_HBORDER * (sbc != nhsb - 1) + xoff;
          clpf_strength = gi % CLPF_STRENGTHS;
          if (clpf_strength == 0)
            copy_sb16_16(&in[(-yoff * OD_FILT_BSTRIDE - xoff)], OD_FILT_BSTRIDE,
                         src[pli],
                         (sbr * MAX_MIB_SIZE << mi_high_l2[pli]) - yoff,
                         (sbc * MAX_MIB_SIZE << mi_wide_l2[pli]) - xoff,
                         stride[pli], ysize, xsize);
          od_dering(clpf_strength ? NULL : (uint8_t *)in, OD_FILT_BSTRIDE,
                    tmp_dst, in, xdec[pli], ydec[pli], dir, &dirinit, var, pli,
                    dlist, dering_count, threshold,
                    clpf_strength + (clpf_strength == 3), clpf_damping,
                    dering_damping, coeff_shift, clpf_strength != 0, 1);
          curr_mse = compute_dering_dist(
              ref_coeff[pli] +
                  (sbr * MAX_MIB_SIZE << mi_high_l2[pli]) * stride[pli] +
                  (sbc * MAX_MIB_SIZE << mi_wide_l2[pli]),
              stride[pli], tmp_dst, dlist, dering_count, bsize[pli],
              coeff_shift, pli);
          if (pli < 2)
            mse[pli][sb_count][gi] = curr_mse;
          else
            mse[1][sb_count][gi] += curr_mse;
          sb_index[sb_count] =
              MAX_MIB_SIZE * sbr * cm->mi_stride + MAX_MIB_SIZE * sbc;
        }
      }
      sb_count++;
    }
  }
  nb_strength_bits = 0;
  /* Search for different number of signalling bits. */
  for (i = 0; i <= 3; i++) {
    int j;
    int best_lev0[CDEF_MAX_STRENGTHS];
    int best_lev1[CDEF_MAX_STRENGTHS] = { 0 };
    nb_strengths = 1 << i;
    if (nplanes >= 3)
      tot_mse = joint_strength_search_dual(best_lev0, best_lev1, nb_strengths,
                                           mse, sb_count);
    else
      tot_mse =
          joint_strength_search(best_lev0, nb_strengths, mse[0], sb_count);
    /* Count superblock signalling cost. */
    tot_mse += (uint64_t)(sb_count * lambda * i);
    /* Count header signalling cost. */
    tot_mse += (uint64_t)(nb_strengths * lambda * CDEF_STRENGTH_BITS);
    if (tot_mse < best_tot_mse) {
      best_tot_mse = tot_mse;
      nb_strength_bits = i;
      for (j = 0; j < 1 << nb_strength_bits; j++) {
        cm->cdef_strengths[j] = best_lev0[j];
        cm->cdef_uv_strengths[j] = best_lev1[j];
      }
    }
  }
  nb_strengths = 1 << nb_strength_bits;

  cm->cdef_bits = nb_strength_bits;
  cm->nb_cdef_strengths = nb_strengths;
  for (i = 0; i < sb_count; i++) {
    int gi;
    int best_gi;
    uint64_t best_mse = (uint64_t)1 << 63;
    best_gi = 0;
    for (gi = 0; gi < cm->nb_cdef_strengths; gi++) {
      uint64_t curr = mse[0][i][cm->cdef_strengths[gi]];
      if (nplanes >= 3) curr += mse[1][i][cm->cdef_uv_strengths[gi]];
      if (curr < best_mse) {
        best_gi = gi;
        best_mse = curr;
      }
    }
    selected_strength[i] = best_gi;
    cm->mi_grid_visible[sb_index[i]]->mbmi.cdef_strength = best_gi;
  }
  cm->cdef_dering_damping = dering_damping;
  cm->cdef_clpf_damping = clpf_damping;
  aom_free(mse[0]);
  aom_free(mse[1]);
  for (pli = 0; pli < nplanes; pli++) {
    aom_free(src[pli]);
    aom_free(ref_coeff[pli]);
  }
  aom_free(sb_index);
  aom_free(selected_strength);
}
