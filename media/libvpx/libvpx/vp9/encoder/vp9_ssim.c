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
#include "./vp9_rtcd.h"
#include "vpx_ports/mem.h"
#include "vp9/encoder/vp9_ssim.h"

void vp9_ssim_parms_16x16_c(uint8_t *s, int sp, uint8_t *r,
                            int rp, unsigned long *sum_s, unsigned long *sum_r,
                            unsigned long *sum_sq_s, unsigned long *sum_sq_r,
                            unsigned long *sum_sxr) {
  int i, j;
  for (i = 0; i < 16; i++, s += sp, r += rp) {
    for (j = 0; j < 16; j++) {
      *sum_s += s[j];
      *sum_r += r[j];
      *sum_sq_s += s[j] * s[j];
      *sum_sq_r += r[j] * r[j];
      *sum_sxr += s[j] * r[j];
    }
  }
}
void vp9_ssim_parms_8x8_c(uint8_t *s, int sp, uint8_t *r, int rp,
                          unsigned long *sum_s, unsigned long *sum_r,
                          unsigned long *sum_sq_s, unsigned long *sum_sq_r,
                          unsigned long *sum_sxr) {
  int i, j;
  for (i = 0; i < 8; i++, s += sp, r += rp) {
    for (j = 0; j < 8; j++) {
      *sum_s += s[j];
      *sum_r += r[j];
      *sum_sq_s += s[j] * s[j];
      *sum_sq_r += r[j] * r[j];
      *sum_sxr += s[j] * r[j];
    }
  }
}

#if CONFIG_VP9_HIGHBITDEPTH
void vp9_highbd_ssim_parms_8x8_c(uint16_t *s, int sp, uint16_t *r, int rp,
                                 uint32_t *sum_s, uint32_t *sum_r,
                                 uint32_t *sum_sq_s, uint32_t *sum_sq_r,
                                 uint32_t *sum_sxr) {
  int i, j;
  for (i = 0; i < 8; i++, s += sp, r += rp) {
    for (j = 0; j < 8; j++) {
      *sum_s += s[j];
      *sum_r += r[j];
      *sum_sq_s += s[j] * s[j];
      *sum_sq_r += r[j] * r[j];
      *sum_sxr += s[j] * r[j];
    }
  }
}
#endif  // CONFIG_VP9_HIGHBITDEPTH

static const int64_t cc1 =  26634;  // (64^2*(.01*255)^2
static const int64_t cc2 = 239708;  // (64^2*(.03*255)^2

static double similarity(unsigned long sum_s, unsigned long sum_r,
                         unsigned long sum_sq_s, unsigned long sum_sq_r,
                         unsigned long sum_sxr, int count) {
  int64_t ssim_n, ssim_d;
  int64_t c1, c2;

  // scale the constants by number of pixels
  c1 = (cc1 * count * count) >> 12;
  c2 = (cc2 * count * count) >> 12;

  ssim_n = (2 * sum_s * sum_r + c1) * ((int64_t) 2 * count * sum_sxr -
                                       (int64_t) 2 * sum_s * sum_r + c2);

  ssim_d = (sum_s * sum_s + sum_r * sum_r + c1) *
           ((int64_t)count * sum_sq_s - (int64_t)sum_s * sum_s +
            (int64_t)count * sum_sq_r - (int64_t) sum_r * sum_r + c2);

  return ssim_n * 1.0 / ssim_d;
}

static double ssim_8x8(uint8_t *s, int sp, uint8_t *r, int rp) {
  unsigned long sum_s = 0, sum_r = 0, sum_sq_s = 0, sum_sq_r = 0, sum_sxr = 0;
  vp9_ssim_parms_8x8(s, sp, r, rp, &sum_s, &sum_r, &sum_sq_s, &sum_sq_r,
                     &sum_sxr);
  return similarity(sum_s, sum_r, sum_sq_s, sum_sq_r, sum_sxr, 64);
}

#if CONFIG_VP9_HIGHBITDEPTH
static double highbd_ssim_8x8(uint16_t *s, int sp, uint16_t *r, int rp,
                              unsigned int bd) {
  uint32_t sum_s = 0, sum_r = 0, sum_sq_s = 0, sum_sq_r = 0, sum_sxr = 0;
  const int oshift = bd - 8;
  vp9_highbd_ssim_parms_8x8(s, sp, r, rp, &sum_s, &sum_r, &sum_sq_s, &sum_sq_r,
                            &sum_sxr);
  return similarity(sum_s >> oshift,
                    sum_r >> oshift,
                    sum_sq_s >> (2 * oshift),
                    sum_sq_r >> (2 * oshift),
                    sum_sxr >> (2 * oshift),
                    64);
}
#endif  // CONFIG_VP9_HIGHBITDEPTH

// We are using a 8x8 moving window with starting location of each 8x8 window
// on the 4x4 pixel grid. Such arrangement allows the windows to overlap
// block boundaries to penalize blocking artifacts.
double vp9_ssim2(uint8_t *img1, uint8_t *img2, int stride_img1,
                 int stride_img2, int width, int height) {
  int i, j;
  int samples = 0;
  double ssim_total = 0;

  // sample point start with each 4x4 location
  for (i = 0; i <= height - 8;
       i += 4, img1 += stride_img1 * 4, img2 += stride_img2 * 4) {
    for (j = 0; j <= width - 8; j += 4) {
      double v = ssim_8x8(img1 + j, stride_img1, img2 + j, stride_img2);
      ssim_total += v;
      samples++;
    }
  }
  ssim_total /= samples;
  return ssim_total;
}

#if CONFIG_VP9_HIGHBITDEPTH
double vp9_highbd_ssim2(uint8_t *img1, uint8_t *img2, int stride_img1,
                        int stride_img2, int width, int height,
                        unsigned int bd) {
  int i, j;
  int samples = 0;
  double ssim_total = 0;

  // sample point start with each 4x4 location
  for (i = 0; i <= height - 8;
       i += 4, img1 += stride_img1 * 4, img2 += stride_img2 * 4) {
    for (j = 0; j <= width - 8; j += 4) {
      double v = highbd_ssim_8x8(CONVERT_TO_SHORTPTR(img1 + j), stride_img1,
                                 CONVERT_TO_SHORTPTR(img2 + j), stride_img2,
                                 bd);
      ssim_total += v;
      samples++;
    }
  }
  ssim_total /= samples;
  return ssim_total;
}
#endif  // CONFIG_VP9_HIGHBITDEPTH

double vp9_calc_ssim(YV12_BUFFER_CONFIG *source, YV12_BUFFER_CONFIG *dest,
                     double *weight) {
  double a, b, c;
  double ssimv;

  a = vp9_ssim2(source->y_buffer, dest->y_buffer,
                source->y_stride, dest->y_stride,
                source->y_crop_width, source->y_crop_height);

  b = vp9_ssim2(source->u_buffer, dest->u_buffer,
                source->uv_stride, dest->uv_stride,
                source->uv_crop_width, source->uv_crop_height);

  c = vp9_ssim2(source->v_buffer, dest->v_buffer,
                source->uv_stride, dest->uv_stride,
                source->uv_crop_width, source->uv_crop_height);

  ssimv = a * .8 + .1 * (b + c);

  *weight = 1;

  return ssimv;
}

double vp9_calc_ssimg(YV12_BUFFER_CONFIG *source, YV12_BUFFER_CONFIG *dest,
                      double *ssim_y, double *ssim_u, double *ssim_v) {
  double ssim_all = 0;
  double a, b, c;

  a = vp9_ssim2(source->y_buffer, dest->y_buffer,
                source->y_stride, dest->y_stride,
                source->y_crop_width, source->y_crop_height);

  b = vp9_ssim2(source->u_buffer, dest->u_buffer,
                source->uv_stride, dest->uv_stride,
                source->uv_crop_width, source->uv_crop_height);

  c = vp9_ssim2(source->v_buffer, dest->v_buffer,
                source->uv_stride, dest->uv_stride,
                source->uv_crop_width, source->uv_crop_height);
  *ssim_y = a;
  *ssim_u = b;
  *ssim_v = c;
  ssim_all = (a * 4 + b + c) / 6;

  return ssim_all;
}

// traditional ssim as per: http://en.wikipedia.org/wiki/Structural_similarity
//
// Re working out the math ->
//
// ssim(x,y) =  (2*mean(x)*mean(y) + c1)*(2*cov(x,y)+c2) /
//   ((mean(x)^2+mean(y)^2+c1)*(var(x)+var(y)+c2))
//
// mean(x) = sum(x) / n
//
// cov(x,y) = (n*sum(xi*yi)-sum(x)*sum(y))/(n*n)
//
// var(x) = (n*sum(xi*xi)-sum(xi)*sum(xi))/(n*n)
//
// ssim(x,y) =
//   (2*sum(x)*sum(y)/(n*n) + c1)*(2*(n*sum(xi*yi)-sum(x)*sum(y))/(n*n)+c2) /
//   (((sum(x)*sum(x)+sum(y)*sum(y))/(n*n) +c1) *
//    ((n*sum(xi*xi) - sum(xi)*sum(xi))/(n*n)+
//     (n*sum(yi*yi) - sum(yi)*sum(yi))/(n*n)+c2)))
//
// factoring out n*n
//
// ssim(x,y) =
//   (2*sum(x)*sum(y) + n*n*c1)*(2*(n*sum(xi*yi)-sum(x)*sum(y))+n*n*c2) /
//   (((sum(x)*sum(x)+sum(y)*sum(y)) + n*n*c1) *
//    (n*sum(xi*xi)-sum(xi)*sum(xi)+n*sum(yi*yi)-sum(yi)*sum(yi)+n*n*c2))
//
// Replace c1 with n*n * c1 for the final step that leads to this code:
// The final step scales by 12 bits so we don't lose precision in the constants.

double ssimv_similarity(Ssimv *sv, int64_t n) {
  // Scale the constants by number of pixels.
  const int64_t c1 = (cc1 * n * n) >> 12;
  const int64_t c2 = (cc2 * n * n) >> 12;

  const double l = 1.0 * (2 * sv->sum_s * sv->sum_r + c1) /
      (sv->sum_s * sv->sum_s + sv->sum_r * sv->sum_r + c1);

  // Since these variables are unsigned sums, convert to double so
  // math is done in double arithmetic.
  const double v = (2.0 * n * sv->sum_sxr - 2 * sv->sum_s * sv->sum_r + c2)
      / (n * sv->sum_sq_s - sv->sum_s * sv->sum_s + n * sv->sum_sq_r
         - sv->sum_r * sv->sum_r + c2);

  return l * v;
}

// The first term of the ssim metric is a luminance factor.
//
// (2*mean(x)*mean(y) + c1)/ (mean(x)^2+mean(y)^2+c1)
//
// This luminance factor is super sensitive to the dark side of luminance
// values and completely insensitive on the white side.  check out 2 sets
// (1,3) and (250,252) the term gives ( 2*1*3/(1+9) = .60
// 2*250*252/ (250^2+252^2) => .99999997
//
// As a result in this tweaked version of the calculation in which the
// luminance is taken as percentage off from peak possible.
//
// 255 * 255 - (sum_s - sum_r) / count * (sum_s - sum_r) / count
//
double ssimv_similarity2(Ssimv *sv, int64_t n) {
  // Scale the constants by number of pixels.
  const int64_t c1 = (cc1 * n * n) >> 12;
  const int64_t c2 = (cc2 * n * n) >> 12;

  const double mean_diff = (1.0 * sv->sum_s - sv->sum_r) / n;
  const double l = (255 * 255 - mean_diff * mean_diff + c1) / (255 * 255 + c1);

  // Since these variables are unsigned, sums convert to double so
  // math is done in double arithmetic.
  const double v = (2.0 * n * sv->sum_sxr - 2 * sv->sum_s * sv->sum_r + c2)
      / (n * sv->sum_sq_s - sv->sum_s * sv->sum_s +
         n * sv->sum_sq_r - sv->sum_r * sv->sum_r + c2);

  return l * v;
}
void ssimv_parms(uint8_t *img1, int img1_pitch, uint8_t *img2, int img2_pitch,
                 Ssimv *sv) {
  vp9_ssim_parms_8x8(img1, img1_pitch, img2, img2_pitch,
                     &sv->sum_s, &sv->sum_r, &sv->sum_sq_s, &sv->sum_sq_r,
                     &sv->sum_sxr);
}

double vp9_get_ssim_metrics(uint8_t *img1, int img1_pitch,
                            uint8_t *img2, int img2_pitch,
                            int width, int height,
                            Ssimv *sv2, Metrics *m,
                            int do_inconsistency) {
  double dssim_total = 0;
  double ssim_total = 0;
  double ssim2_total = 0;
  double inconsistency_total = 0;
  int i, j;
  int c = 0;
  double norm;
  double old_ssim_total = 0;
  vp9_clear_system_state();
  // We can sample points as frequently as we like start with 1 per 4x4.
  for (i = 0; i < height; i += 4,
       img1 += img1_pitch * 4, img2 += img2_pitch * 4) {
    for (j = 0; j < width; j += 4, ++c) {
      Ssimv sv = {0};
      double ssim;
      double ssim2;
      double dssim;
      uint32_t var_new;
      uint32_t var_old;
      uint32_t mean_new;
      uint32_t mean_old;
      double ssim_new;
      double ssim_old;

      // Not sure there's a great way to handle the edge pixels
      // in ssim when using a window. Seems biased against edge pixels
      // however you handle this. This uses only samples that are
      // fully in the frame.
      if (j + 8 <= width && i + 8 <= height) {
        ssimv_parms(img1 + j, img1_pitch, img2 + j, img2_pitch, &sv);
      }

      ssim = ssimv_similarity(&sv, 64);
      ssim2 = ssimv_similarity2(&sv, 64);

      sv.ssim = ssim2;

      // dssim is calculated to use as an actual error metric and
      // is scaled up to the same range as sum square error.
      // Since we are subsampling every 16th point maybe this should be
      // *16 ?
      dssim = 255 * 255 * (1 - ssim2) / 2;

      // Here I introduce a new error metric: consistency-weighted
      // SSIM-inconsistency.  This metric isolates frames where the
      // SSIM 'suddenly' changes, e.g. if one frame in every 8 is much
      // sharper or blurrier than the others. Higher values indicate a
      // temporally inconsistent SSIM. There are two ideas at work:
      //
      // 1) 'SSIM-inconsistency': the total inconsistency value
      // reflects how much SSIM values are changing between this
      // source / reference frame pair and the previous pair.
      //
      // 2) 'consistency-weighted': weights de-emphasize areas in the
      // frame where the scene content has changed. Changes in scene
      // content are detected via changes in local variance and local
      // mean.
      //
      // Thus the overall measure reflects how inconsistent the SSIM
      // values are, over consistent regions of the frame.
      //
      // The metric has three terms:
      //
      // term 1 -> uses change in scene Variance to weight error score
      //  2 * var(Fi)*var(Fi-1) / (var(Fi)^2+var(Fi-1)^2)
      //  larger changes from one frame to the next mean we care
      //  less about consistency.
      //
      // term 2 -> uses change in local scene luminance to weight error
      //  2 * avg(Fi)*avg(Fi-1) / (avg(Fi)^2+avg(Fi-1)^2)
      //  larger changes from one frame to the next mean we care
      //  less about consistency.
      //
      // term3 -> measures inconsistency in ssim scores between frames
      //   1 - ( 2 * ssim(Fi)*ssim(Fi-1)/(ssim(Fi)^2+sssim(Fi-1)^2).
      //
      // This term compares the ssim score for the same location in 2
      // subsequent frames.
      var_new = sv.sum_sq_s - sv.sum_s * sv.sum_s / 64;
      var_old = sv2[c].sum_sq_s - sv2[c].sum_s * sv2[c].sum_s / 64;
      mean_new = sv.sum_s;
      mean_old = sv2[c].sum_s;
      ssim_new = sv.ssim;
      ssim_old = sv2[c].ssim;

      if (do_inconsistency) {
        // We do the metric once for every 4x4 block in the image. Since
        // we are scaling the error to SSE for use in a psnr calculation
        // 1.0 = 4x4x255x255 the worst error we can possibly have.
        static const double kScaling = 4. * 4 * 255 * 255;

        // The constants have to be non 0 to avoid potential divide by 0
        // issues other than that they affect kind of a weighting between
        // the terms.  No testing of what the right terms should be has been
        // done.
        static const double c1 = 1, c2 = 1, c3 = 1;

        // This measures how much consistent variance is in two consecutive
        // source frames. 1.0 means they have exactly the same variance.
        const double variance_term = (2.0 * var_old * var_new + c1) /
            (1.0 * var_old * var_old + 1.0 * var_new * var_new + c1);

        // This measures how consistent the local mean are between two
        // consecutive frames. 1.0 means they have exactly the same mean.
        const double mean_term = (2.0 * mean_old * mean_new + c2) /
            (1.0 * mean_old * mean_old + 1.0 * mean_new * mean_new + c2);

        // This measures how consistent the ssims of two
        // consecutive frames is. 1.0 means they are exactly the same.
        double ssim_term = pow((2.0 * ssim_old * ssim_new + c3) /
                               (ssim_old * ssim_old + ssim_new * ssim_new + c3),
                               5);

        double this_inconsistency;

        // Floating point math sometimes makes this > 1 by a tiny bit.
        // We want the metric to scale between 0 and 1.0 so we can convert
        // it to an snr scaled value.
        if (ssim_term > 1)
          ssim_term = 1;

        // This converts the consistency metric to an inconsistency metric
        // ( so we can scale it like psnr to something like sum square error.
        // The reason for the variance and mean terms is the assumption that
        // if there are big changes in the source we shouldn't penalize
        // inconsistency in ssim scores a bit less as it will be less visible
        // to the user.
        this_inconsistency = (1 - ssim_term) * variance_term * mean_term;

        this_inconsistency *= kScaling;
        inconsistency_total += this_inconsistency;
      }
      sv2[c] = sv;
      ssim_total += ssim;
      ssim2_total += ssim2;
      dssim_total += dssim;

      old_ssim_total += ssim_old;
    }
    old_ssim_total += 0;
  }

  norm = 1. / (width / 4) / (height / 4);
  ssim_total *= norm;
  ssim2_total *= norm;
  m->ssim2 = ssim2_total;
  m->ssim = ssim_total;
  if (old_ssim_total == 0)
    inconsistency_total = 0;

  m->ssimc = inconsistency_total;

  m->dssim = dssim_total;
  return inconsistency_total;
}


#if CONFIG_VP9_HIGHBITDEPTH
double vp9_highbd_calc_ssim(YV12_BUFFER_CONFIG *source,
                            YV12_BUFFER_CONFIG *dest,
                            double *weight, unsigned int bd) {
  double a, b, c;
  double ssimv;

  a = vp9_highbd_ssim2(source->y_buffer, dest->y_buffer,
                       source->y_stride, dest->y_stride,
                       source->y_crop_width, source->y_crop_height, bd);

  b = vp9_highbd_ssim2(source->u_buffer, dest->u_buffer,
                       source->uv_stride, dest->uv_stride,
                       source->uv_crop_width, source->uv_crop_height, bd);

  c = vp9_highbd_ssim2(source->v_buffer, dest->v_buffer,
                       source->uv_stride, dest->uv_stride,
                       source->uv_crop_width, source->uv_crop_height, bd);

  ssimv = a * .8 + .1 * (b + c);

  *weight = 1;

  return ssimv;
}

double vp9_highbd_calc_ssimg(YV12_BUFFER_CONFIG *source,
                             YV12_BUFFER_CONFIG *dest, double *ssim_y,
                             double *ssim_u, double *ssim_v, unsigned int bd) {
  double ssim_all = 0;
  double a, b, c;

  a = vp9_highbd_ssim2(source->y_buffer, dest->y_buffer,
                       source->y_stride, dest->y_stride,
                       source->y_crop_width, source->y_crop_height, bd);

  b = vp9_highbd_ssim2(source->u_buffer, dest->u_buffer,
                       source->uv_stride, dest->uv_stride,
                       source->uv_crop_width, source->uv_crop_height, bd);

  c = vp9_highbd_ssim2(source->v_buffer, dest->v_buffer,
                       source->uv_stride, dest->uv_stride,
                       source->uv_crop_width, source->uv_crop_height, bd);
  *ssim_y = a;
  *ssim_u = b;
  *ssim_v = c;
  ssim_all = (a * 4 + b + c) / 6;

  return ssim_all;
}
#endif  // CONFIG_VP9_HIGHBITDEPTH
