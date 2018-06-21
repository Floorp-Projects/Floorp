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

#include "config/aom_scale_rtcd.h"

#include "aom/aom_integer.h"
#include "av1/common/cdef.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/reconinter.h"
#include "av1/encoder/encoder.h"

#define REDUCED_PRI_STRENGTHS 8
#define REDUCED_TOTAL_STRENGTHS (REDUCED_PRI_STRENGTHS * CDEF_SEC_STRENGTHS)
#define TOTAL_STRENGTHS (CDEF_PRI_STRENGTHS * CDEF_SEC_STRENGTHS)

static int priconv[REDUCED_PRI_STRENGTHS] = { 0, 1, 2, 3, 5, 7, 10, 13 };

/* Search for the best strength to add as an option, knowing we
   already selected nb_strengths options. */
static uint64_t search_one(int *lev, int nb_strengths,
                           uint64_t mse[][TOTAL_STRENGTHS], int sb_count,
                           int fast) {
  uint64_t tot_mse[TOTAL_STRENGTHS];
  const int total_strengths = fast ? REDUCED_TOTAL_STRENGTHS : TOTAL_STRENGTHS;
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
    for (j = 0; j < total_strengths; j++) {
      uint64_t best = best_mse;
      if (mse[i][j] < best) best = mse[i][j];
      tot_mse[j] += best;
    }
  }
  for (j = 0; j < total_strengths; j++) {
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
                                uint64_t (**mse)[TOTAL_STRENGTHS], int sb_count,
                                int fast) {
  uint64_t tot_mse[TOTAL_STRENGTHS][TOTAL_STRENGTHS];
  int i, j;
  uint64_t best_tot_mse = (uint64_t)1 << 63;
  int best_id0 = 0;
  int best_id1 = 0;
  const int total_strengths = fast ? REDUCED_TOTAL_STRENGTHS : TOTAL_STRENGTHS;
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
    for (j = 0; j < total_strengths; j++) {
      int k;
      for (k = 0; k < total_strengths; k++) {
        uint64_t best = best_mse;
        uint64_t curr = mse[0][i][j];
        curr += mse[1][i][k];
        if (curr < best) best = curr;
        tot_mse[j][k] += best;
      }
    }
  }
  for (j = 0; j < total_strengths; j++) {
    int k;
    for (k = 0; k < total_strengths; k++) {
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
                                      int sb_count, int fast) {
  uint64_t best_tot_mse;
  int i;
  best_tot_mse = (uint64_t)1 << 63;
  /* Greedy search: add one strength options at a time. */
  for (i = 0; i < nb_strengths; i++) {
    best_tot_mse = search_one(best_lev, i, mse, sb_count, fast);
  }
  /* Trying to refine the greedy search by reconsidering each
     already-selected option. */
  if (!fast) {
    for (i = 0; i < 4 * nb_strengths; i++) {
      int j;
      for (j = 0; j < nb_strengths - 1; j++) best_lev[j] = best_lev[j + 1];
      best_tot_mse =
          search_one(best_lev, nb_strengths - 1, mse, sb_count, fast);
    }
  }
  return best_tot_mse;
}

/* Search for the set of luma+chroma strengths that minimizes mse. */
static uint64_t joint_strength_search_dual(int *best_lev0, int *best_lev1,
                                           int nb_strengths,
                                           uint64_t (**mse)[TOTAL_STRENGTHS],
                                           int sb_count, int fast) {
  uint64_t best_tot_mse;
  int i;
  best_tot_mse = (uint64_t)1 << 63;
  /* Greedy search: add one strength options at a time. */
  for (i = 0; i < nb_strengths; i++) {
    best_tot_mse =
        search_one_dual(best_lev0, best_lev1, i, mse, sb_count, fast);
  }
  /* Trying to refine the greedy search by reconsidering each
     already-selected option. */
  for (i = 0; i < 4 * nb_strengths; i++) {
    int j;
    for (j = 0; j < nb_strengths - 1; j++) {
      best_lev0[j] = best_lev0[j + 1];
      best_lev1[j] = best_lev1[j + 1];
    }
    best_tot_mse = search_one_dual(best_lev0, best_lev1, nb_strengths - 1, mse,
                                   sb_count, fast);
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
      .5 + (sum_d2 + sum_s2 - 2 * sum_sd) * .5 *
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
uint64_t compute_cdef_dist(uint16_t *dst, int dstride, uint16_t *src,
                           cdef_list *dlist, int cdef_count, BLOCK_SIZE bsize,
                           int coeff_shift, int pli) {
  uint64_t sum = 0;
  int bi, bx, by;
  if (bsize == BLOCK_8X8) {
    for (bi = 0; bi < cdef_count; bi++) {
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
    for (bi = 0; bi < cdef_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      sum += mse_4x4_16bit(&dst[(by << 3) * dstride + (bx << 2)], dstride,
                           &src[bi << (3 + 2)], 4);
      sum += mse_4x4_16bit(&dst[((by << 3) + 4) * dstride + (bx << 2)], dstride,
                           &src[(bi << (3 + 2)) + 4 * 4], 4);
    }
  } else if (bsize == BLOCK_8X4) {
    for (bi = 0; bi < cdef_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      sum += mse_4x4_16bit(&dst[(by << 2) * dstride + (bx << 3)], dstride,
                           &src[bi << (2 + 3)], 8);
      sum += mse_4x4_16bit(&dst[(by << 2) * dstride + (bx << 3) + 4], dstride,
                           &src[(bi << (2 + 3)) + 4], 8);
    }
  } else {
    assert(bsize == BLOCK_4X4);
    for (bi = 0; bi < cdef_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      sum += mse_4x4_16bit(&dst[(by << 2) * dstride + (bx << 2)], dstride,
                           &src[bi << (2 + 2)], 4);
    }
  }
  return sum >> 2 * coeff_shift;
}

void av1_cdef_search(YV12_BUFFER_CONFIG *frame, const YV12_BUFFER_CONFIG *ref,
                     AV1_COMMON *cm, MACROBLOCKD *xd, int fast) {
  int r, c;
  int fbr, fbc;
  uint16_t *src[3];
  uint16_t *ref_coeff[3];
  static cdef_list dlist[MI_SIZE_128X128 * MI_SIZE_128X128];
  int dir[CDEF_NBLOCKS][CDEF_NBLOCKS] = { { 0 } };
  int var[CDEF_NBLOCKS][CDEF_NBLOCKS] = { { 0 } };
  int stride[3];
  int bsize[3];
  int mi_wide_l2[3];
  int mi_high_l2[3];
  int xdec[3];
  int ydec[3];
  int pli;
  int cdef_count;
  int coeff_shift = AOMMAX(cm->bit_depth - 8, 0);
  uint64_t best_tot_mse = (uint64_t)1 << 63;
  uint64_t tot_mse;
  int sb_count;
  int nvfb = (cm->mi_rows + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;
  int nhfb = (cm->mi_cols + MI_SIZE_64X64 - 1) / MI_SIZE_64X64;
  int *sb_index = aom_malloc(nvfb * nhfb * sizeof(*sb_index));
  int *selected_strength = aom_malloc(nvfb * nhfb * sizeof(*sb_index));
  uint64_t(*mse[2])[TOTAL_STRENGTHS];
  int pri_damping = 3 + (cm->base_qindex >> 6);
  int sec_damping = 3 + (cm->base_qindex >> 6);
  int i;
  int nb_strengths;
  int nb_strength_bits;
  int quantizer;
  double lambda;
  const int num_planes = av1_num_planes(cm);
  const int total_strengths = fast ? REDUCED_TOTAL_STRENGTHS : TOTAL_STRENGTHS;
  DECLARE_ALIGNED(32, uint16_t, inbuf[CDEF_INBUF_SIZE]);
  uint16_t *in;
  DECLARE_ALIGNED(32, uint16_t, tmp_dst[1 << (MAX_SB_SIZE_LOG2 * 2)]);
  quantizer =
      av1_ac_quant_Q3(cm->base_qindex, 0, cm->bit_depth) >> (cm->bit_depth - 8);
  lambda = .12 * quantizer * quantizer / 256.;

  av1_setup_dst_planes(xd->plane, cm->seq_params.sb_size, frame, 0, 0, 0,
                       num_planes);
  mse[0] = aom_malloc(sizeof(**mse) * nvfb * nhfb);
  mse[1] = aom_malloc(sizeof(**mse) * nvfb * nhfb);
  for (pli = 0; pli < num_planes; pli++) {
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
        if (cm->use_highbitdepth) {
          src[pli][r * stride[pli] + c] = CONVERT_TO_SHORTPTR(
              xd->plane[pli].dst.buf)[r * xd->plane[pli].dst.stride + c];
          ref_coeff[pli][r * stride[pli] + c] =
              CONVERT_TO_SHORTPTR(ref_buffer)[r * ref_stride + c];
        } else {
          src[pli][r * stride[pli] + c] =
              xd->plane[pli].dst.buf[r * xd->plane[pli].dst.stride + c];
          ref_coeff[pli][r * stride[pli] + c] = ref_buffer[r * ref_stride + c];
        }
      }
    }
  }
  in = inbuf + CDEF_VBORDER * CDEF_BSTRIDE + CDEF_HBORDER;
  sb_count = 0;
  for (fbr = 0; fbr < nvfb; ++fbr) {
    for (fbc = 0; fbc < nhfb; ++fbc) {
      int nvb, nhb;
      int gi;
      int dirinit = 0;
      nhb = AOMMIN(MI_SIZE_64X64, cm->mi_cols - MI_SIZE_64X64 * fbc);
      nvb = AOMMIN(MI_SIZE_64X64, cm->mi_rows - MI_SIZE_64X64 * fbr);
      int hb_step = 1;
      int vb_step = 1;
      BLOCK_SIZE bs = BLOCK_64X64;
      MB_MODE_INFO *const mbmi =
          cm->mi_grid_visible[MI_SIZE_64X64 * fbr * cm->mi_stride +
                              MI_SIZE_64X64 * fbc];
      if (((fbc & 1) &&
           (mbmi->sb_type == BLOCK_128X128 || mbmi->sb_type == BLOCK_128X64)) ||
          ((fbr & 1) &&
           (mbmi->sb_type == BLOCK_128X128 || mbmi->sb_type == BLOCK_64X128)))
        continue;
      if (mbmi->sb_type == BLOCK_128X128 || mbmi->sb_type == BLOCK_128X64 ||
          mbmi->sb_type == BLOCK_64X128)
        bs = mbmi->sb_type;
      if (bs == BLOCK_128X128 || bs == BLOCK_128X64) {
        nhb = AOMMIN(MI_SIZE_128X128, cm->mi_cols - MI_SIZE_64X64 * fbc);
        hb_step = 2;
      }
      if (bs == BLOCK_128X128 || bs == BLOCK_64X128) {
        nvb = AOMMIN(MI_SIZE_128X128, cm->mi_rows - MI_SIZE_64X64 * fbr);
        vb_step = 2;
      }
      // No filtering if the entire filter block is skipped
      if (sb_all_skip(cm, fbr * MI_SIZE_64X64, fbc * MI_SIZE_64X64)) continue;
      cdef_count = sb_compute_cdef_list(cm, fbr * MI_SIZE_64X64,
                                        fbc * MI_SIZE_64X64, dlist, bs);
      for (pli = 0; pli < num_planes; pli++) {
        for (i = 0; i < CDEF_INBUF_SIZE; i++) inbuf[i] = CDEF_VERY_LARGE;
        for (gi = 0; gi < total_strengths; gi++) {
          int threshold;
          uint64_t curr_mse;
          int sec_strength;
          threshold = gi / CDEF_SEC_STRENGTHS;
          if (fast) threshold = priconv[threshold];
          /* We avoid filtering the pixels for which some of the pixels to
             average
             are outside the frame. We could change the filter instead, but it
             would add special cases for any future vectorization. */
          int yoff = CDEF_VBORDER * (fbr != 0);
          int xoff = CDEF_HBORDER * (fbc != 0);
          int ysize = (nvb << mi_high_l2[pli]) +
                      CDEF_VBORDER * (fbr + vb_step < nvfb) + yoff;
          int xsize = (nhb << mi_wide_l2[pli]) +
                      CDEF_HBORDER * (fbc + hb_step < nhfb) + xoff;
          sec_strength = gi % CDEF_SEC_STRENGTHS;
          copy_sb16_16(&in[(-yoff * CDEF_BSTRIDE - xoff)], CDEF_BSTRIDE,
                       src[pli],
                       (fbr * MI_SIZE_64X64 << mi_high_l2[pli]) - yoff,
                       (fbc * MI_SIZE_64X64 << mi_wide_l2[pli]) - xoff,
                       stride[pli], ysize, xsize);
          cdef_filter_fb(NULL, tmp_dst, CDEF_BSTRIDE, in, xdec[pli], ydec[pli],
                         dir, &dirinit, var, pli, dlist, cdef_count, threshold,
                         sec_strength + (sec_strength == 3), pri_damping,
                         sec_damping, coeff_shift);
          curr_mse = compute_cdef_dist(
              ref_coeff[pli] +
                  (fbr * MI_SIZE_64X64 << mi_high_l2[pli]) * stride[pli] +
                  (fbc * MI_SIZE_64X64 << mi_wide_l2[pli]),
              stride[pli], tmp_dst, dlist, cdef_count, bsize[pli], coeff_shift,
              pli);
          if (pli < 2)
            mse[pli][sb_count][gi] = curr_mse;
          else
            mse[1][sb_count][gi] += curr_mse;
          sb_index[sb_count] =
              MI_SIZE_64X64 * fbr * cm->mi_stride + MI_SIZE_64X64 * fbc;
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
    if (num_planes >= 3)
      tot_mse = joint_strength_search_dual(best_lev0, best_lev1, nb_strengths,
                                           mse, sb_count, fast);
    else
      tot_mse = joint_strength_search(best_lev0, nb_strengths, mse[0], sb_count,
                                      fast);
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
      if (num_planes >= 3) curr += mse[1][i][cm->cdef_uv_strengths[gi]];
      if (curr < best_mse) {
        best_gi = gi;
        best_mse = curr;
      }
    }
    selected_strength[i] = best_gi;
    cm->mi_grid_visible[sb_index[i]]->cdef_strength = best_gi;
  }

  if (fast) {
    for (int j = 0; j < nb_strengths; j++) {
      cm->cdef_strengths[j] =
          priconv[cm->cdef_strengths[j] / CDEF_SEC_STRENGTHS] *
              CDEF_SEC_STRENGTHS +
          (cm->cdef_strengths[j] % CDEF_SEC_STRENGTHS);
      cm->cdef_uv_strengths[j] =
          priconv[cm->cdef_uv_strengths[j] / CDEF_SEC_STRENGTHS] *
              CDEF_SEC_STRENGTHS +
          (cm->cdef_uv_strengths[j] % CDEF_SEC_STRENGTHS);
    }
  }
  cm->cdef_pri_damping = pri_damping;
  cm->cdef_sec_damping = sec_damping;
  aom_free(mse[0]);
  aom_free(mse[1]);
  for (pli = 0; pli < num_planes; pli++) {
    aom_free(src[pli]);
    aom_free(ref_coeff[pli]);
  }
  aom_free(sb_index);
  aom_free(selected_strength);
}
