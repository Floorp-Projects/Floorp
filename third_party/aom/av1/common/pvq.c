/*
 * Copyright (c) 2001-2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/* clang-format off */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "odintrin.h"
#include "partition.h"
#include "pvq.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Imported from encode.c in daala */
/* These are the PVQ equivalent of quantization matrices, except that
   the values are per-band. */
#define OD_MASKING_DISABLED 0
#define OD_MASKING_ENABLED 1

const unsigned char OD_LUMA_QM_Q4[2][OD_QM_SIZE] = {
/* Flat quantization for PSNR. The DC component isn't 16 because the DC
   magnitude compensation is done here for inter (Haar DC doesn't need it).
   Masking disabled: */
 {
  16, 16,
  16, 16, 16, 16,
  16, 16, 16, 16, 16, 16,
  16, 16, 16, 16, 16, 16, 16, 16
 },
/* The non-flat AC coefficients compensate for the non-linear scaling caused
   by activity masking. The values are currently hand-tuned so that the rate
   of each band remains roughly constant when enabling activity masking
   on intra.
   Masking enabled: */
 {
  16, 16,
  16, 18, 28, 32,
  16, 14, 20, 20, 28, 32,
  16, 11, 14, 14, 17, 17, 22, 28
 }
};

const unsigned char OD_CHROMA_QM_Q4[2][OD_QM_SIZE] = {
/* Chroma quantization is different because of the reduced lapping.
   FIXME: Use the same matrix as luma for 4:4:4.
   Masking disabled: */
 {
  16, 16,
  16, 16, 16, 16,
  16, 16, 16, 16, 16, 16,
  16, 16, 16, 16, 16, 16, 16, 16
 },
/* The AC part is flat for chroma because it has no activity masking.
   Masking enabled: */
 {
  16, 16,
  16, 16, 16, 16,
  16, 16, 16, 16, 16, 16,
  16, 16, 16, 16, 16, 16, 16, 16
 }
};

/* No interpolation, always use od_flat_qm_q4, but use a different scale for
   each plane.
   FIXME: Add interpolation and properly tune chroma. */
const od_qm_entry OD_DEFAULT_QMS[2][2][OD_NPLANES_MAX] = {
  /* Masking disabled */
  { { { 4, 256, OD_LUMA_QM_Q4[OD_MASKING_DISABLED] },
      { 4, 256, OD_CHROMA_QM_Q4[OD_MASKING_DISABLED] },
      { 4, 256, OD_CHROMA_QM_Q4[OD_MASKING_DISABLED] } },
    { { 0, 0, NULL},
      { 0, 0, NULL},
      { 0, 0, NULL} } },
  /* Masking enabled */
  { { { 4, 256, OD_LUMA_QM_Q4[OD_MASKING_ENABLED] },
      { 4, 256, OD_CHROMA_QM_Q4[OD_MASKING_ENABLED] },
      { 4, 256, OD_CHROMA_QM_Q4[OD_MASKING_ENABLED] } },
    { { 0, 0, NULL},
      { 0, 0, NULL},
      { 0, 0, NULL} } }
};

/* Constants for the beta parameter, which controls how activity masking is
   used.
   beta = 1 / (1 - alpha), so when beta is 1, alpha is 0 and activity
   masking is disabled. When beta is 1.5, activity masking is used. Note that
   activity masking is neither used for 4x4 blocks nor for chroma. */
#define OD_BETA(b) OD_QCONST32(b, OD_BETA_SHIFT)
static const od_val16 OD_PVQ_BETA4_LUMA[1] = {OD_BETA(1.)};
static const od_val16 OD_PVQ_BETA8_LUMA[4] = {OD_BETA(1.), OD_BETA(1.),
 OD_BETA(1.), OD_BETA(1.)};
static const od_val16 OD_PVQ_BETA16_LUMA[7] = {OD_BETA(1.), OD_BETA(1.),
 OD_BETA(1.), OD_BETA(1.), OD_BETA(1.), OD_BETA(1.), OD_BETA(1.)};
static const od_val16 OD_PVQ_BETA32_LUMA[10] = {OD_BETA(1.), OD_BETA(1.),
 OD_BETA(1.), OD_BETA(1.), OD_BETA(1.), OD_BETA(1.), OD_BETA(1.), OD_BETA(1.),
 OD_BETA(1.), OD_BETA(1.)};

static const od_val16 OD_PVQ_BETA4_LUMA_MASKING[1] = {OD_BETA(1.)};
static const od_val16 OD_PVQ_BETA8_LUMA_MASKING[4] = {OD_BETA(1.5),
 OD_BETA(1.5), OD_BETA(1.5), OD_BETA(1.5)};
static const od_val16 OD_PVQ_BETA16_LUMA_MASKING[7] = {OD_BETA(1.5),
 OD_BETA(1.5), OD_BETA(1.5), OD_BETA(1.5), OD_BETA(1.5), OD_BETA(1.5),
 OD_BETA(1.5)};
static const od_val16 OD_PVQ_BETA32_LUMA_MASKING[10] = {OD_BETA(1.5),
 OD_BETA(1.5), OD_BETA(1.5), OD_BETA(1.5), OD_BETA(1.5), OD_BETA(1.5),
 OD_BETA(1.5), OD_BETA(1.5), OD_BETA(1.5), OD_BETA(1.5)};

static const od_val16 OD_PVQ_BETA4_CHROMA[1] = {OD_BETA(1.)};
static const od_val16 OD_PVQ_BETA8_CHROMA[4] = {OD_BETA(1.), OD_BETA(1.),
 OD_BETA(1.), OD_BETA(1.)};
static const od_val16 OD_PVQ_BETA16_CHROMA[7] = {OD_BETA(1.), OD_BETA(1.),
 OD_BETA(1.), OD_BETA(1.), OD_BETA(1.), OD_BETA(1.), OD_BETA(1.)};
static const od_val16 OD_PVQ_BETA32_CHROMA[10] = {OD_BETA(1.), OD_BETA(1.),
 OD_BETA(1.), OD_BETA(1.), OD_BETA(1.), OD_BETA(1.), OD_BETA(1.), OD_BETA(1.),
 OD_BETA(1.), OD_BETA(1.)};

const od_val16 *const OD_PVQ_BETA[2][OD_NPLANES_MAX][OD_TXSIZES + 1] = {
 {{OD_PVQ_BETA4_LUMA, OD_PVQ_BETA8_LUMA,
   OD_PVQ_BETA16_LUMA, OD_PVQ_BETA32_LUMA},
  {OD_PVQ_BETA4_CHROMA, OD_PVQ_BETA8_CHROMA,
   OD_PVQ_BETA16_CHROMA, OD_PVQ_BETA32_CHROMA},
  {OD_PVQ_BETA4_CHROMA, OD_PVQ_BETA8_CHROMA,
   OD_PVQ_BETA16_CHROMA, OD_PVQ_BETA32_CHROMA}},
 {{OD_PVQ_BETA4_LUMA_MASKING, OD_PVQ_BETA8_LUMA_MASKING,
   OD_PVQ_BETA16_LUMA_MASKING, OD_PVQ_BETA32_LUMA_MASKING},
  {OD_PVQ_BETA4_CHROMA, OD_PVQ_BETA8_CHROMA,
   OD_PVQ_BETA16_CHROMA, OD_PVQ_BETA32_CHROMA},
  {OD_PVQ_BETA4_CHROMA, OD_PVQ_BETA8_CHROMA,
   OD_PVQ_BETA16_CHROMA, OD_PVQ_BETA32_CHROMA}}
};


void od_interp_qm(unsigned char *out, int q, const od_qm_entry *entry1,
  const od_qm_entry *entry2) {
  int i;
  if (entry2 == NULL || entry2->qm_q4 == NULL
   || q < entry1->interp_q << OD_COEFF_SHIFT) {
    /* Use entry1. */
    for (i = 0; i < OD_QM_SIZE; i++) {
      out[i] = OD_MINI(255, entry1->qm_q4[i]*entry1->scale_q8 >> 8);
    }
  }
  else if (entry1 == NULL || entry1->qm_q4 == NULL
   || q > entry2->interp_q << OD_COEFF_SHIFT) {
    /* Use entry2. */
    for (i = 0; i < OD_QM_SIZE; i++) {
      out[i] = OD_MINI(255, entry2->qm_q4[i]*entry2->scale_q8 >> 8);
    }
  }
  else {
    /* Interpolate between entry1 and entry2. The interpolation is linear
       in terms of log(q) vs log(m*scale). Considering that we're ultimately
       multiplying the result it makes sense, but we haven't tried other
       interpolation methods. */
    double x;
    const unsigned char *m1;
    const unsigned char *m2;
    int q1;
    int q2;
    m1 = entry1->qm_q4;
    m2 = entry2->qm_q4;
    q1 = entry1->interp_q << OD_COEFF_SHIFT;
    q2 = entry2->interp_q << OD_COEFF_SHIFT;
    x = (log(q)-log(q1))/(log(q2)-log(q1));
    for (i = 0; i < OD_QM_SIZE; i++) {
      out[i] = OD_MINI(255, (int)floor(.5 + (1./256)*exp(
       x*log(m2[i]*entry2->scale_q8) + (1 - x)*log(m1[i]*entry1->scale_q8))));
    }
  }
}

void od_adapt_pvq_ctx_reset(od_pvq_adapt_ctx *state, int is_keyframe) {
  od_pvq_codeword_ctx *ctx;
  int i;
  int pli;
  int bs;
  ctx = &state->pvq_codeword_ctx;
  OD_CDFS_INIT_DYNAMIC(state->pvq_param_model[0].cdf);
  OD_CDFS_INIT_DYNAMIC(state->pvq_param_model[1].cdf);
  OD_CDFS_INIT_DYNAMIC(state->pvq_param_model[2].cdf);
  for (i = 0; i < 2*OD_TXSIZES; i++) {
    ctx->pvq_adapt[4*i + OD_ADAPT_K_Q8] = 384;
    ctx->pvq_adapt[4*i + OD_ADAPT_SUM_EX_Q8] = 256;
    ctx->pvq_adapt[4*i + OD_ADAPT_COUNT_Q8] = 104;
    ctx->pvq_adapt[4*i + OD_ADAPT_COUNT_EX_Q8] = 128;
  }
  OD_CDFS_INIT_DYNAMIC(ctx->pvq_k1_cdf);
  for (pli = 0; pli < OD_NPLANES_MAX; pli++) {
    for (bs = 0; bs < OD_TXSIZES; bs++)
    for (i = 0; i < PVQ_MAX_PARTITIONS; i++) {
      state->pvq_exg[pli][bs][i] = 2 << 16;
    }
  }
  for (i = 0; i < OD_TXSIZES*PVQ_MAX_PARTITIONS; i++) {
    state->pvq_ext[i] = is_keyframe ? 24576 : 2 << 16;
  }
  OD_CDFS_INIT_DYNAMIC(state->pvq_gaintheta_cdf);
  OD_CDFS_INIT_Q15(state->pvq_skip_dir_cdf);
  OD_CDFS_INIT_DYNAMIC(ctx->pvq_split_cdf);
}

/* QMs are arranged from smallest to largest blocksizes, first for
   blocks with decimation=0, followed by blocks with decimation=1.*/
int od_qm_offset(int bs, int xydec)
{
    return xydec*OD_QM_STRIDE + OD_QM_OFFSET(bs);
}

#if defined(OD_FLOAT_PVQ)
#define OD_DEFAULT_MAG 1.0
#else
#define OD_DEFAULT_MAG OD_QM_SCALE
#endif

/* Initialize the quantization matrix. */
// Note: When hybrid transform and corresponding scan order is used by PVQ,
// we don't need seperate qm and qm_inv for each transform type,
// because AOM does not do magnitude compensation (i.e. simplay x16 for all coeffs).
void od_init_qm(int16_t *x, int16_t *x_inv, const int *qm) {
  int i;
  int j;
  int16_t y[OD_TXSIZE_MAX*OD_TXSIZE_MAX];
  int16_t y_inv[OD_TXSIZE_MAX*OD_TXSIZE_MAX];
  int16_t *x1;
  int16_t *x1_inv;
  int off;
  int bs;
  int xydec;
  for (bs = 0; bs < OD_TXSIZES; bs++) {
    for (xydec = 0; xydec < 2; xydec++) {
      off = od_qm_offset(bs, xydec);
      x1 = x + off;
      x1_inv = x_inv + off;
      for (i = 0; i < 4 << bs; i++) {
        for (j = 0; j < 4 << bs; j++) {
          /*This will ultimately be clamped to fit in 16 bits.*/
          od_val32 mag;
          int16_t ytmp;
          mag = OD_DEFAULT_MAG;
          if (i != 0 || j != 0) {
#if defined(OD_FLOAT_PVQ)
            mag /= 0.0625*qm[(i << 1 >> bs)*8 + (j << 1 >> bs)];
#else
            int qmv;
            qmv = qm[(i << 1 >> bs)*8 + (j << 1 >> bs)];
            mag *= 16;
            mag = (mag + (qmv >> 1))/qmv;
#endif
            OD_ASSERT(mag > 0.0);
          }
          /*Convert to fit in 16 bits.*/
#if defined(OD_FLOAT_PVQ)
          y[i*(4 << bs) + j] = (int16_t)OD_MINI(OD_QM_SCALE_MAX,
           (int32_t)floor(.5 + mag*OD_QM_SCALE));
          y_inv[i*(4 << bs) + j] = (int16_t)floor(.5
           + OD_QM_SCALE*OD_QM_INV_SCALE/(double)y[i*(4 << bs) + j]);
#else
          y[i*(4 << bs) + j] = (int16_t)OD_MINI(OD_QM_SCALE_MAX, mag);
          ytmp = y[i*(4 << bs) + j];
          y_inv[i*(4 << bs) + j] = (int16_t)((OD_QM_SCALE*OD_QM_INV_SCALE
           + (ytmp >> 1))/ytmp);
#endif
        }
      }
      od_raster_to_coding_order_16(x1, 4 << bs, y, 4 << bs);
      od_raster_to_coding_order_16(x1_inv, 4 << bs, y_inv, 4 << bs);
    }
  }
}

/* Maps each possible size (n) in the split k-tokenizer to a different value.
   Possible values of n are:
   2, 3, 4, 7, 8, 14, 15, 16, 31, 32, 63, 64, 127, 128
   Since we don't care about the order (even in the bit-stream) the simplest
   ordering (implemented here) is:
   14, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128 */
int od_pvq_size_ctx(int n) {
  int logn;
  int odd;
  logn = OD_ILOG(n - 1);
  odd = n & 1;
  return 2*logn - 1 - odd - 7*(n == 14);
}

/* Maps a length n to a context for the (k=1, n<=16) coder, with a special
   case when n is the original length (orig_length=1) of the vector (i.e. we
   haven't split it yet). For orig_length=0, we use the same mapping as
   od_pvq_size_ctx() up to n=16. When orig_length=1, we map lengths
   7, 8, 14, 15 to contexts 8 to 11. */
int od_pvq_k1_ctx(int n, int orig_length) {
  if (orig_length) return 8 + 2*(n > 8) + (n & 1);
  else return od_pvq_size_ctx(n);
}

/* Indexing for the packed quantization matrices. */
int od_qm_get_index(int bs, int band) {
  /* The -band/3 term is due to the fact that we force corresponding horizontal
     and vertical bands to have the same quantization. */
  OD_ASSERT(bs >= 0 && bs < OD_TXSIZES);
  return bs*(bs + 1) + band - band/3;
}

#if !defined(OD_FLOAT_PVQ)
/*See celt/mathops.c in Opus and tools/cos_search.c.*/
static int16_t od_pvq_cos_pi_2(int16_t x)
{
  int16_t x2;
  x2 = OD_MULT16_16_Q15(x, x);
  return OD_MINI(32767, (1073758164 - x*x + x2*(-7654 + OD_MULT16_16_Q16(x2,
   16573 + OD_MULT16_16_Q16(-2529, x2)))) >> 15);
}
#endif

/*Approximates cos(x) for -pi < x < pi.
  Input is in OD_THETA_SCALE.*/
od_val16 od_pvq_cos(od_val32 x) {
#if defined(OD_FLOAT_PVQ)
  return cos(x);
#else
  /*Wrap x around by masking, since cos is periodic.*/
  x = x & 0x0001ffff;
  if (x > (1 << 16)) {
    x = (1 << 17) - x;
  }
  if (x & 0x00007fff) {
    if (x < (1 << 15)) {
       return od_pvq_cos_pi_2((int16_t)x);
    }
    else {
      return -od_pvq_cos_pi_2((int16_t)(65536 - x));
    }
  }
  else {
    if (x & 0x0000ffff) {
      return 0;
    }
    else if (x & 0x0001ffff) {
      return -32767;
    }
    else {
      return 32767;
    }
  }
#endif
}

/*Approximates sin(x) for 0 <= x < pi.
  Input is in OD_THETA_SCALE.*/
od_val16 od_pvq_sin(od_val32 x) {
#if defined(OD_FLOAT_PVQ)
  return sin(x);
#else
  return od_pvq_cos(32768 - x);
#endif
}

#if !defined(OD_FLOAT_PVQ)
/* Computes an upper-bound on the number of bits required to store the L2 norm
   of a vector (excluding sign). */
int od_vector_log_mag(const od_coeff *x, int n) {
  int i;
  int32_t sum;
  sum = 0;
  for (i = 0; i < n; i++) {
    int16_t tmp;
    tmp = x[i] >> 8;
    sum += tmp*(int32_t)tmp;
  }
  /* We add one full bit (instead of rounding OD_ILOG() up) for safety because
     the >> 8 above causes the sum to be slightly underestimated. */
  return 8 + 1 + OD_ILOG(n + sum)/2;
}
#endif

/** Computes Householder reflection that aligns the reference r to the
 *  dimension in r with the greatest absolute value. The reflection
 *  vector is returned in r.
 *
 * @param [in,out]  r      reference vector to be reflected, reflection
 *                         also returned in r
 * @param [in]      n      number of dimensions in r
 * @param [in]      gr     gain of reference vector
 * @param [out]     sign   sign of reflection
 * @return                 dimension number to which reflection aligns
 **/
int od_compute_householder(od_val16 *r, int n, od_val32 gr, int *sign,
 int shift) {
  int m;
  int i;
  int s;
  od_val16 maxr;
  OD_UNUSED(shift);
  /* Pick component with largest magnitude. Not strictly
   * necessary, but it helps numerical stability */
  m = 0;
  maxr = 0;
  for (i = 0; i < n; i++) {
    if (OD_ABS(r[i]) > maxr) {
      maxr = OD_ABS(r[i]);
      m = i;
    }
  }
  s = r[m] > 0 ? 1 : -1;
  /* This turns r into a Householder reflection vector that would reflect
   * the original r[] to e_m */
  r[m] += OD_SHR_ROUND(gr*s, shift);
  *sign = s;
  return m;
}

#if !defined(OD_FLOAT_PVQ)
#define OD_RCP_INSHIFT 15
#define OD_RCP_OUTSHIFT 14
static od_val16 od_rcp(od_val16 x)
{
  int i;
  od_val16 n;
  od_val16 r;
  i = OD_ILOG(x) - 1;
  /*n is Q15 with range [0,1).*/
  n = OD_VSHR_ROUND(x, i - OD_RCP_INSHIFT) - (1 << OD_RCP_INSHIFT);
  /*Start with a linear approximation:
    r = 1.8823529411764706-0.9411764705882353*n.
    The coefficients and the result are Q14 in the range [15420,30840].*/
  r = 30840 + OD_MULT16_16_Q15(-15420, n);
  /*Perform two Newton iterations:
    r -= r*((r*n)-1.Q15)
       = r*((r*n)+(r-1.Q15)).*/
  r = r - OD_MULT16_16_Q15(r, (OD_MULT16_16_Q15(r, n) + r - 32768));
  /*We subtract an extra 1 in the second iteration to avoid overflow; it also
     neatly compensates for truncation error in the rest of the process.*/
  r = r - (1 + OD_MULT16_16_Q15(r, OD_MULT16_16_Q15(r, n) + r - 32768));
  /*r is now the Q15 solution to 2/(n+1), with a maximum relative error
     of 7.05346E-5, a (relative) RMSE of 2.14418E-5, and a peak absolute
     error of 1.24665/32768.*/
  return OD_VSHR_ROUND(r, i - OD_RCP_OUTSHIFT);
}
#endif

/** Applies Householder reflection from compute_householder(). The
 * reflection is its own inverse.
 *
 * @param [out]     out    reflected vector
 * @param [in]      x      vector to be reflected
 * @param [in]      r      reflection
 * @param [in]      n      number of dimensions in x,r
 */
void od_apply_householder(od_val16 *out, const od_val16 *x, const od_val16 *r,
 int n) {
  int i;
  od_val32 proj;
  od_val16 proj_1;
  od_val32 l2r;
#if !defined(OD_FLOAT_PVQ)
  od_val16 proj_norm;
  od_val16 l2r_norm;
  od_val16 rcp;
  int proj_shift;
  int l2r_shift;
  int outshift;
#endif
  /*FIXME: Can we get l2r and/or l2r_shift from an earlier computation?*/
  l2r = 0;
  for (i = 0; i < n; i++) {
    l2r += OD_MULT16_16(r[i], r[i]);
  }
  /* Apply Householder reflection */
  proj = 0;
  for (i = 0; i < n; i++) {
    proj += OD_MULT16_16(r[i], x[i]);
  }
#if defined(OD_FLOAT_PVQ)
  proj_1 = proj*2./(1e-100 + l2r);
  for (i = 0; i < n; i++) {
    out[i] = x[i] - r[i]*proj_1;
  }
#else
  /*l2r_norm is [0.5, 1.0[ in Q15.*/
  l2r_shift = (OD_ILOG(l2r) - 1) - 14;
  l2r_norm = OD_VSHR_ROUND(l2r, l2r_shift);
  rcp = od_rcp(l2r_norm);
  proj_shift = (OD_ILOG(abs(proj)) - 1) - 14;
  /*proj_norm is [0.5, 1.0[ in Q15.*/
  proj_norm = OD_VSHR_ROUND(proj, proj_shift);
  proj_1 = OD_MULT16_16_Q15(proj_norm, rcp);
  /*The proj*2. in the float code becomes -1 in the final outshift.
    The sign of l2r_shift is positive since we're taking the reciprocal of
     l2r_norm and this is a right shift.*/
  outshift = OD_MINI(30, OD_RCP_OUTSHIFT - proj_shift - 1 + l2r_shift);
  if (outshift >= 0) {
    for (i = 0; i < n; i++) {
      int32_t tmp;
      tmp = OD_MULT16_16(r[i], proj_1);
      tmp = OD_SHR_ROUND(tmp, outshift);
      out[i] = x[i] - tmp;
    }
  }
  else {
    /*FIXME: Can we make this case impossible?
      Right now, if r[] is all zeros except for 1, 2, or 3 ones, and
       if x[] is all zeros except for large values at the same position as the
       ones in r[], then we can end up with a shift of -1.*/
    for (i = 0; i < n; i++) {
      int32_t tmp;
      tmp = OD_MULT16_16(r[i], proj_1);
      tmp = OD_SHL(tmp, -outshift);
      out[i] = x[i] - tmp;
    }
  }
#endif
}

#if !defined(OD_FLOAT_PVQ)
static od_val16 od_beta_rcp(od_val16 beta){
  if (beta == OD_BETA(1.))
    return OD_BETA(1.);
  else if (beta == OD_BETA(1.5))
    return OD_BETA(1./1.5);
  else {
    od_val16 rcp_beta;
    /*Shift by 1 less, transposing beta to range [.5, .75] and thus < 32768.*/
    rcp_beta = od_rcp(beta << (OD_RCP_INSHIFT - 1 - OD_BETA_SHIFT));
    return OD_SHR_ROUND(rcp_beta, OD_RCP_OUTSHIFT + 1 - OD_BETA_SHIFT);
  }
}

#define OD_EXP2_INSHIFT 15
#define OD_EXP2_FRACSHIFT 15
#define OD_EXP2_OUTSHIFT 15
static const int32_t OD_EXP2_C[5] = {32768, 22709, 7913, 1704, 443};
/*Output is [1.0, 2.0) in Q(OD_EXP2_FRACSHIFT).
  It does not include the integer offset, which is added in od_exp2 after the
   final shift).*/
static int32_t od_exp2_frac(int32_t x)
{
  return OD_MULT16_16_Q15(x, (OD_EXP2_C[1] + OD_MULT16_16_Q15(x,
   (OD_EXP2_C[2] + OD_MULT16_16_Q15(x, (OD_EXP2_C[3]
   + OD_MULT16_16_Q15(x, OD_EXP2_C[4])))))));
}

/** Base-2 exponential approximation (2^x) with Q15 input and output.*/
static int32_t od_exp2(int32_t x)
{
  int integer;
  int32_t frac;
  integer = x >> OD_EXP2_INSHIFT;
  if (integer > 14)
    return 0x7f000000;
  else if (integer < -15)
    return 0;
  frac = od_exp2_frac(x - OD_SHL(integer, OD_EXP2_INSHIFT));
  return OD_VSHR_ROUND(OD_EXP2_C[0] + frac, -integer) + 1;
}

#define OD_LOG2_INSHIFT 15
#define OD_LOG2_OUTSHIFT 15
#define OD_LOG2_INSCALE_1 (1./(1 << OD_LOG2_INSHIFT))
#define OD_LOG2_OUTSCALE (1 << OD_LOG2_OUTSHIFT)
static int16_t od_log2(int16_t x)
{
  return x + OD_MULT16_16_Q15(x, (14482 + OD_MULT16_16_Q15(x, (-23234
   + OD_MULT16_16_Q15(x, (13643 + OD_MULT16_16_Q15(x, (-6403
   + OD_MULT16_16_Q15(x, 1515)))))))));
}

static int32_t od_pow(int32_t x, od_val16 beta)
{
  int16_t t;
  int xshift;
  int log2_x;
  od_val32 logr;
  /*FIXME: this conditional is to avoid doing log2(0).*/
  if (x == 0)
    return 0;
  log2_x = (OD_ILOG(x) - 1);
  xshift = log2_x - OD_LOG2_INSHIFT;
  /*t should be in range [0.0, 1.0[ in Q(OD_LOG2_INSHIFT).*/
  t = OD_VSHR(x, xshift) - (1 << OD_LOG2_INSHIFT);
  /*log2(g/OD_COMPAND_SCALE) = log2(x) - OD_COMPAND_SHIFT in
     Q(OD_LOG2_OUTSHIFT).*/
  logr = od_log2(t) + (log2_x - OD_COMPAND_SHIFT)*OD_LOG2_OUTSCALE;
  logr = (od_val32)OD_MULT16_32_QBETA(beta, logr);
  return od_exp2(logr);
}
#endif

/** Gain companding: raises gain to the power 1/beta for activity masking.
 *
 * @param [in]  g     real (uncompanded) gain
 * @param [in]  q0    uncompanded quality parameter
 * @param [in]  beta  activity masking beta param (exponent)
 * @return            g^(1/beta)
 */
static od_val32 od_gain_compand(od_val32 g, int q0, od_val16 beta) {
#if defined(OD_FLOAT_PVQ)
  if (beta == 1) return OD_CGAIN_SCALE*g/(double)q0;
  else {
    return OD_CGAIN_SCALE*OD_COMPAND_SCALE*pow(g*OD_COMPAND_SCALE_1,
     1./beta)/(double)q0;
  }
#else
  if (beta == OD_BETA(1)) return (OD_CGAIN_SCALE*g + (q0 >> 1))/q0;
  else {
    int32_t expr;
    expr = od_pow(g, od_beta_rcp(beta));
    expr <<= OD_CGAIN_SHIFT + OD_COMPAND_SHIFT - OD_EXP2_OUTSHIFT;
    return (expr + (q0 >> 1))/q0;
  }
#endif
}

#if !defined(OD_FLOAT_PVQ)
#define OD_SQRT_INSHIFT 16
#define OD_SQRT_OUTSHIFT 15
static int16_t od_rsqrt_norm(int16_t x);

static int16_t od_sqrt_norm(int32_t x)
{
  OD_ASSERT(x < 65536);
  return OD_MINI(OD_SHR_ROUND(x*od_rsqrt_norm(x), OD_SQRT_OUTSHIFT), 32767);
}

static int16_t od_sqrt(int32_t x, int *sqrt_shift)
{
  int k;
  int s;
  int32_t t;
  if (x == 0) {
    *sqrt_shift = 0;
     return 0;
  }
  OD_ASSERT(x < (1 << 30));
  k = ((OD_ILOG(x) - 1) >> 1);
  /*t is x in the range [0.25, 1) in QINSHIFT, or x*2^(-s).
    Shift by log2(x) - log2(0.25*(1 << INSHIFT)) to ensure 0.25 lower bound.*/
  s = 2*k - (OD_SQRT_INSHIFT - 2);
  t = OD_VSHR(x, s);
  /*We want to express od_sqrt() in terms of od_sqrt_norm(), which is
     defined as (2^OUTSHIFT)*sqrt(t*(2^-INSHIFT)) with t=x*(2^-s).
    This simplifies to 2^(OUTSHIFT-(INSHIFT/2)-(s/2))*sqrt(x), so the caller
     needs to shift right by OUTSHIFT - INSHIFT/2 - s/2.*/
  *sqrt_shift = OD_SQRT_OUTSHIFT - ((s + OD_SQRT_INSHIFT) >> 1);
  return od_sqrt_norm(t);
}
#endif

/** Gain expanding: raises gain to the power beta for activity masking.
 *
 * @param [in]  cg    companded gain
 * @param [in]  q0    uncompanded quality parameter
 * @param [in]  beta  activity masking beta param (exponent)
 * @return            g^beta
 */
od_val32 od_gain_expand(od_val32 cg0, int q0, od_val16 beta) {
  if (beta == OD_BETA(1)) {
    /*The multiply fits into 28 bits because the expanded gain has a range from
       0 to 2^20.*/
    return OD_SHR_ROUND(cg0*q0, OD_CGAIN_SHIFT);
  }
  else if (beta == OD_BETA(1.5)) {
#if defined(OD_FLOAT_PVQ)
    double cg;
    cg = cg0*OD_CGAIN_SCALE_1;
    cg *= q0*OD_COMPAND_SCALE_1;
    return OD_COMPAND_SCALE*cg*sqrt(cg);
#else
    int32_t irt;
    int64_t tmp;
    int sqrt_inshift;
    int sqrt_outshift;
    /*cg0 is in Q(OD_CGAIN_SHIFT) and we need to divide it by
       2^OD_COMPAND_SHIFT.*/
    irt = od_sqrt(cg0*q0, &sqrt_outshift);
    sqrt_inshift = (OD_CGAIN_SHIFT + OD_COMPAND_SHIFT) >> 1;
    /*tmp is in Q(OD_CGAIN_SHIFT + OD_COMPAND_SHIFT).*/
    tmp = cg0*q0*(int64_t)irt;
    /*Expanded gain must be in Q(OD_COMPAND_SHIFT), thus OD_COMPAND_SHIFT is
       not included here.*/
    return OD_MAXI(1,
        OD_VSHR_ROUND(tmp, OD_CGAIN_SHIFT + sqrt_outshift + sqrt_inshift));
#endif
  }
  else {
#if defined(OD_FLOAT_PVQ)
    /*Expanded gain must be in Q(OD_COMPAND_SHIFT), hence the multiply by
       OD_COMPAND_SCALE.*/
    double cg;
    cg = cg0*OD_CGAIN_SCALE_1;
    return OD_COMPAND_SCALE*pow(cg*q0*OD_COMPAND_SCALE_1, beta);
#else
    int32_t expr;
    int32_t cg;
    cg = OD_SHR_ROUND(cg0*q0, OD_CGAIN_SHIFT);
    expr = od_pow(cg, beta);
    /*Expanded gain must be in Q(OD_COMPAND_SHIFT), hence the subtraction by
       OD_COMPAND_SHIFT.*/
    return OD_MAXI(1, OD_SHR_ROUND(expr, OD_EXP2_OUTSHIFT - OD_COMPAND_SHIFT));
#endif
  }
}

/** Computes the raw and quantized/companded gain of a given input
 * vector
 *
 * @param [in]      x      vector of input data
 * @param [in]      n      number of elements in vector x
 * @param [in]      q0     quantizer
 * @param [out]     g      raw gain
 * @param [in]      beta   activity masking beta param
 * @param [in]      bshift shift to be applied to raw gain
 * @return                 quantized/companded gain
 */
od_val32 od_pvq_compute_gain(const od_val16 *x, int n, int q0, od_val32 *g,
 od_val16 beta, int bshift) {
  int i;
  od_val32 acc;
#if !defined(OD_FLOAT_PVQ)
  od_val32 irt;
  int sqrt_shift;
#else
  OD_UNUSED(bshift);
#endif
  acc = 0;
  for (i = 0; i < n; i++) {
    acc += x[i]*(od_val32)x[i];
  }
#if defined(OD_FLOAT_PVQ)
  *g = sqrt(acc);
#else
  irt = od_sqrt(acc, &sqrt_shift);
  *g = OD_VSHR_ROUND(irt, sqrt_shift - bshift);
#endif
  /* Normalize gain by quantization step size and apply companding
     (if ACTIVITY != 1). */
  return od_gain_compand(*g, q0, beta);
}

/** Compute theta quantization range from quantized/companded gain
 *
 * @param [in]      qcg    quantized companded gain value
 * @param [in]      beta   activity masking beta param
 * @return                 max theta value
 */
int od_pvq_compute_max_theta(od_val32 qcg, od_val16 beta){
  /* Set angular resolution (in ra) to match the encoded gain */
#if defined(OD_FLOAT_PVQ)
  int ts = (int)floor(.5 + qcg*OD_CGAIN_SCALE_1*M_PI/(2*beta));
#else
  int ts = OD_SHR_ROUND(qcg*OD_MULT16_16_QBETA(OD_QCONST32(M_PI/2,
   OD_CGAIN_SHIFT), od_beta_rcp(beta)), OD_CGAIN_SHIFT*2);
#endif
  /* Special case for low gains -- will need to be tuned anyway */
  if (qcg < OD_QCONST32(1.4, OD_CGAIN_SHIFT)) ts = 1;
  return ts;
}

/** Decode quantized theta value from coded value
 *
 * @param [in]      t          quantized companded gain value
 * @param [in]      max_theta  maximum theta value
 * @return                     decoded theta value
 */
od_val32 od_pvq_compute_theta(int t, int max_theta) {
  if (max_theta != 0) {
#if defined(OD_FLOAT_PVQ)
    return OD_MINI(t, max_theta - 1)*.5*M_PI/max_theta;
#else
    return (OD_MAX_THETA_SCALE*OD_MINI(t, max_theta - 1)
     + (max_theta >> 1))/max_theta;
#endif
  }
  else return 0;
}

#define OD_SQRT_TBL_SHIFT (10)

#define OD_ITHETA_SHIFT 15
/** Compute the number of pulses used for PVQ encoding a vector from
 * available metrics (encode and decode side)
 *
 * @param [in]      qcg        quantized companded gain value
 * @param [in]      itheta     quantized PVQ error angle theta
 * @param [in]      noref      indicates present or lack of reference
 *                             (prediction)
 * @param [in]      n          number of elements to be coded
 * @param [in]      beta       activity masking beta param
 * @return                     number of pulses to use for coding
 */
int od_pvq_compute_k(od_val32 qcg, int itheta, int noref, int n,
    od_val16 beta) {
#if !defined(OD_FLOAT_PVQ)
  /*Lookup table for sqrt(n+3/2) and sqrt(n+2/2) in Q10.
    Real max values are 32792 and 32784, but clamped to stay within 16 bits.
    Update with tools/gen_sqrt_tbl if needed.*/
  static const od_val16 od_sqrt_table[2][13] = {
   {0, 0, 0, 0, 2290, 2985, 4222, 0, 8256, 0, 16416, 0, 32767},
   {0, 0, 0, 0, 2401, 3072, 4284, 0, 8287, 0, 16432, 0, 32767}};
#endif
  if (noref) {
    if (qcg == 0) return 0;
    if (n == 15 && qcg == OD_CGAIN_SCALE && beta > OD_BETA(1.25)) {
      return 1;
    }
    else {
#if defined(OD_FLOAT_PVQ)
      return OD_MAXI(1, (int)floor(.5 + (qcg*OD_CGAIN_SCALE_1 - .2)*
       sqrt((n + 3)/2)/beta));
#else
      od_val16 rt;
      OD_ASSERT(OD_ILOG(n + 1) < 13);
      rt = od_sqrt_table[1][OD_ILOG(n + 1)];
      /*FIXME: get rid of 64-bit mul.*/
      return OD_MAXI(1, OD_SHR_ROUND((int64_t)((qcg
       - (int64_t)OD_QCONST32(.2, OD_CGAIN_SHIFT))*
       OD_MULT16_16_QBETA(od_beta_rcp(beta), rt)), OD_CGAIN_SHIFT
       + OD_SQRT_TBL_SHIFT));
#endif
    }
  }
  else {
    if (itheta == 0) return 0;
    /* Sets K according to gain and theta, based on the high-rate
       PVQ distortion curves (see PVQ document). Low-rate will have to be
       perceptually tuned anyway. We subtract 0.2 from the radius as an
       approximation for the fact that the coefficients aren't identically
       distributed within a band so at low gain the number of dimensions that
       are likely to have a pulse is less than n. */
#if defined(OD_FLOAT_PVQ)
    return OD_MAXI(1, (int)floor(.5 + (itheta - .2)*sqrt((n + 2)/2)));
#else
    od_val16 rt;
    OD_ASSERT(OD_ILOG(n + 1) < 13);
    rt = od_sqrt_table[0][OD_ILOG(n + 1)];
    /*FIXME: get rid of 64-bit mul.*/
    return OD_MAXI(1, OD_VSHR_ROUND(((OD_SHL(itheta, OD_ITHETA_SHIFT)
     - OD_QCONST32(.2, OD_ITHETA_SHIFT)))*(int64_t)rt,
     OD_SQRT_TBL_SHIFT + OD_ITHETA_SHIFT));
#endif
  }
}

#if !defined(OD_FLOAT_PVQ)
#define OD_RSQRT_INSHIFT 16
#define OD_RSQRT_OUTSHIFT 14
/** Reciprocal sqrt approximation where the input is in the range [0.25,1) in
     Q16 and the output is in the range (1.0, 2.0] in Q14).
    Error is always within +/1 of round(1/sqrt(t))*/
static int16_t od_rsqrt_norm(int16_t t)
{
  int16_t n;
  int32_t r;
  int32_t r2;
  int32_t ry;
  int32_t y;
  int32_t ret;
  /* Range of n is [-16384,32767] ([-0.5,1) in Q15).*/
  n = t - 32768;
  OD_ASSERT(n >= -16384);
  /*Get a rough initial guess for the root.
    The optimal minimax quadratic approximation (using relative error) is
     r = 1.437799046117536+n*(-0.823394375837328+n*0.4096419668459485).
    Coefficients here, and the final result r, are Q14.*/
  r = (23565 + OD_MULT16_16_Q15(n, (-13481 + OD_MULT16_16_Q15(n, 6711))));
  /*We want y = t*r*r-1 in Q15, but t is 32-bit Q16 and r is Q14.
    We can compute the result from n and r using Q15 multiplies with some
     adjustment, carefully done to avoid overflow.*/
  r2 = r*r;
  y = (((r2 >> 15)*n + r2) >> 12) - 131077;
  ry = r*y;
  /*Apply a 2nd-order Householder iteration: r += r*y*(y*0.375-0.5).
    This yields the Q14 reciprocal square root of the Q16 t, with a maximum
     relative error of 1.04956E-4, a (relative) RMSE of 2.80979E-5, and a peak
     absolute error of 2.26591/16384.*/
  ret = r + ((((ry >> 16)*(3*y) >> 3) - ry) >> 18);
  OD_ASSERT(ret >= 16384 && ret < 32768);
  return (int16_t)ret;
}

static int16_t od_rsqrt(int32_t x, int *rsqrt_shift)
{
   int k;
   int s;
   int16_t t;
   k = (OD_ILOG(x) - 1) >> 1;
  /*t is x in the range [0.25, 1) in QINSHIFT, or x*2^(-s).
    Shift by log2(x) - log2(0.25*(1 << INSHIFT)) to ensure 0.25 lower bound.*/
   s = 2*k - (OD_RSQRT_INSHIFT - 2);
   t = OD_VSHR(x, s);
   /*We want to express od_rsqrt() in terms of od_rsqrt_norm(), which is
      defined as (2^OUTSHIFT)/sqrt(t*(2^-INSHIFT)) with t=x*(2^-s).
     This simplifies to 2^(OUTSHIFT+(INSHIFT/2)+(s/2))/sqrt(x), so the caller
      needs to shift right by OUTSHIFT + INSHIFT/2 + s/2.*/
   *rsqrt_shift = OD_RSQRT_OUTSHIFT + ((s + OD_RSQRT_INSHIFT) >> 1);
   return od_rsqrt_norm(t);
}
#endif

/** Synthesizes one parition of coefficient values from a PVQ-encoded
 * vector.  This 'partial' version is called by the encode loop where
 * the Householder reflection has already been computed and there's no
 * need to recompute it.
 *
 * @param [out]     xcoeff  output coefficient partition (x in math doc)
 * @param [in]      ypulse  PVQ-encoded values (y in the math doc); in
 *                          the noref case, this vector has n entries,
 *                          in the reference case it contains n-1 entries
 *                          (the m-th entry is not included)
 * @param [in]      r       reference vector (prediction)
 * @param [in]      n       number of elements in this partition
 * @param [in]      noref   indicates presence or lack of prediction
 * @param [in]      g       decoded quantized vector gain
 * @param [in]      theta   decoded theta (prediction error)
 * @param [in]      m       alignment dimension of Householder reflection
 * @param [in]      s       sign of Householder reflection
 * @param [in]      qm_inv  inverse of the QM with magnitude compensation
 */
void od_pvq_synthesis_partial(od_coeff *xcoeff, const od_coeff *ypulse,
 const od_val16 *r16, int n, int noref, od_val32 g, od_val32 theta, int m, int s,
 const int16_t *qm_inv) {
  int i;
  int yy;
  od_val32 scale;
  int nn;
#if !defined(OD_FLOAT_PVQ)
  int gshift;
  int qshift;
#endif
  OD_ASSERT(g != 0);
  nn = n-(!noref); /* when noref==0, vector in is sized n-1 */
  yy = 0;
  for (i = 0; i < nn; i++)
    yy += ypulse[i]*(int32_t)ypulse[i];
#if !defined(OD_FLOAT_PVQ)
  /* Shift required for the magnitude of the pre-qm synthesis to be guaranteed
     to fit in 16 bits. In practice, the range will be 8192-16384 after scaling
     most of the time. */
  gshift = OD_MAXI(0, OD_ILOG(g) - 14);
#endif
  /*scale is g/sqrt(yy) in Q(16-gshift) so that x[]*scale has a norm that fits
     in 16 bits.*/
  if (yy == 0) scale = 0;
#if defined(OD_FLOAT_PVQ)
  else {
    scale = g/sqrt(yy);
  }
#else
  else {
    int rsqrt_shift;
    int16_t rsqrt;
    /*FIXME: should be < int64_t*/
    int64_t tmp;
    rsqrt = od_rsqrt(yy, &rsqrt_shift);
    tmp = rsqrt*(int64_t)g;
    scale = OD_VSHR_ROUND(tmp, rsqrt_shift + gshift - 16);
  }
  /* Shift to apply after multiplying by the inverse QM, taking into account
     gshift. */
  qshift = OD_QM_INV_SHIFT - gshift;
#endif
  if (noref) {
    for (i = 0; i < n; i++) {
      od_val32 x;
      /* This multiply doesn't round, so it introduces some bias.
         It would be nice (but not critical) to fix this. */
      x = (od_val32)OD_MULT16_32_Q16(ypulse[i], scale);
#if defined(OD_FLOAT_PVQ)
      xcoeff[i] = (od_coeff)floor(.5
       + x*(qm_inv[i]*OD_QM_INV_SCALE_1));
#else
      xcoeff[i] = OD_SHR_ROUND(x*qm_inv[i], qshift);
#endif
    }
  }
  else{
    od_val16 x[MAXN];
    scale = OD_ROUND32(scale*OD_TRIG_SCALE_1*od_pvq_sin(theta));
    /* The following multiply doesn't round, but it's probably OK since
       the Householder reflection is likely to undo most of the resulting
       bias. */
    for (i = 0; i < m; i++)
      x[i] = OD_MULT16_32_Q16(ypulse[i], scale);
    x[m] = OD_ROUND16(-s*(OD_SHR_ROUND(g, gshift))*OD_TRIG_SCALE_1*
     od_pvq_cos(theta));
    for (i = m; i < nn; i++)
      x[i+1] = OD_MULT16_32_Q16(ypulse[i], scale);
    od_apply_householder(x, x, r16, n);
    for (i = 0; i < n; i++) {
#if defined(OD_FLOAT_PVQ)
      xcoeff[i] = (od_coeff)floor(.5 + (x[i]*(qm_inv[i]*OD_QM_INV_SCALE_1)));
#else
      xcoeff[i] = OD_SHR_ROUND(x[i]*qm_inv[i], qshift);
#endif
    }
  }
}
