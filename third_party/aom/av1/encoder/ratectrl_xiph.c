/*
 * Copyright (c) 2001-2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "av1/common/odintrin.h"
#include "av1/encoder/ratectrl_xiph.h"

#define OD_Q57(v) ((int64_t)((uint64_t)(v) << 57))
#define OD_F_Q45(v) ((int64_t)(((v) * ((int64_t)1 << 45))))
#define OD_F_Q12(v) ((int32_t)(((v) * ((int32_t)1 << 12))))

/*A rough lookup table for tan(x), 0 <= x < pi/2.
  The values are Q12 fixed-point and spaced at 5 degree intervals.
  These decisions are somewhat arbitrary, but sufficient for the 2nd order
   Bessel follower below.
  Values of x larger than 85 degrees are extrapolated from the last interval,
   which is way off, but "good enough".*/
static uint16_t OD_ROUGH_TAN_LOOKUP[18] = { 0,     358,   722,  1098, 1491,
                                            1910,  2365,  2868, 3437, 4096,
                                            4881,  5850,  7094, 8784, 11254,
                                            15286, 23230, 46817 };

/*alpha is Q24 in the range [0,0.5).
  The return values is 5.12.*/
static int od_warp_alpha(int alpha) {
  int i;
  int d;
  int t0;
  int t1;
  i = alpha * 36 >> 24;
  if (i >= 17) i = 16;
  t0 = OD_ROUGH_TAN_LOOKUP[i];
  t1 = OD_ROUGH_TAN_LOOKUP[i + 1];
  d = alpha * 36 - (i << 24);
  return (int)((((int64_t)t0 << 32) + ((t1 - t0) << 8) * (int64_t)d) >> 32);
}

static const int64_t OD_ATANH_LOG2[32] = {
  0x32B803473F7AD0F4LL, 0x2F2A71BD4E25E916LL, 0x2E68B244BB93BA06LL,
  0x2E39FB9198CE62E4LL, 0x2E2E683F68565C8FLL, 0x2E2B850BE2077FC1LL,
  0x2E2ACC58FE7B78DBLL, 0x2E2A9E2DE52FD5F2LL, 0x2E2A92A338D53EECLL,
  0x2E2A8FC08F5E19B6LL, 0x2E2A8F07E51A485ELL, 0x2E2A8ED9BA8AF388LL,
  0x2E2A8ECE2FE7384ALL, 0x2E2A8ECB4D3E4B1ALL, 0x2E2A8ECA94940FE8LL,
  0x2E2A8ECA6669811DLL, 0x2E2A8ECA5ADEDD6ALL, 0x2E2A8ECA57FC347ELL,
  0x2E2A8ECA57438A43LL, 0x2E2A8ECA57155FB4LL, 0x2E2A8ECA5709D510LL,
  0x2E2A8ECA5706F267LL, 0x2E2A8ECA570639BDLL, 0x2E2A8ECA57060B92LL,
  0x2E2A8ECA57060008LL, 0x2E2A8ECA5705FD25LL, 0x2E2A8ECA5705FC6CLL,
  0x2E2A8ECA5705FC3ELL, 0x2E2A8ECA5705FC33LL, 0x2E2A8ECA5705FC30LL,
  0x2E2A8ECA5705FC2FLL, 0x2E2A8ECA5705FC2FLL
};

static int od_ilog64(int64_t v) {
  static const unsigned char OD_DEBRUIJN_IDX64[64] = {
    0,  1,  2,  7,  3,  13, 8,  19, 4,  25, 14, 28, 9,  34, 20, 40,
    5,  17, 26, 38, 15, 46, 29, 48, 10, 31, 35, 54, 21, 50, 41, 57,
    63, 6,  12, 18, 24, 27, 33, 39, 16, 37, 45, 47, 30, 53, 49, 56,
    62, 11, 23, 32, 36, 44, 52, 55, 61, 22, 43, 51, 60, 42, 59, 58
  };
  int ret;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v |= v >> 32;
  ret = (int)v & 1;
  v = (v >> 1) + 1;
  ret += OD_DEBRUIJN_IDX64[v * UINT64_C(0x218A392CD3D5DBF) >> 58 & 0x3F];
  return ret;
}

/*Computes the binary exponential of logq57.
  input: a log base 2 in Q57 format
  output: a 64 bit integer in Q0 (no fraction) */
static int64_t od_bexp64(int64_t logq57) {
  int64_t w;
  int64_t z;
  int ipart;
  ipart = (int)(logq57 >> 57);
  if (ipart < 0) return 0;
  if (ipart >= 63) return 0x7FFFFFFFFFFFFFFFLL;
  z = logq57 - OD_Q57(ipart);
  if (z) {
    int64_t mask;
    int64_t wlo;
    int i;
    /*C doesn't give us 64x64->128 muls, so we use CORDIC.
      This is not particularly fast, but it's not being used in time-critical
       code; it is very accurate.*/
    /*z is the fractional part of the log in Q62 format.
      We need 1 bit of headroom since the magnitude can get larger than 1
       during the iteration, and a sign bit.*/
    z <<= 5;
    /*w is the exponential in Q61 format (since it also needs headroom and can
       get as large as 2.0); we could get another bit if we dropped the sign,
       but we'll recover that bit later anyway.
      Ideally this should start out as
        \lim_{n->\infty} 2^{61}/\product_{i=1}^n \sqrt{1-2^{-2i}}
       but in order to guarantee convergence we have to repeat iterations 4,
        13 (=3*4+1), and 40 (=3*13+1, etc.), so it winds up somewhat larger.*/
    w = 0x26A3D0E401DD846DLL;
    for (i = 0;; i++) {
      mask = -(z < 0);
      w += ((w >> (i + 1)) + mask) ^ mask;
      z -= (OD_ATANH_LOG2[i] + mask) ^ mask;
      /*Repeat iteration 4.*/
      if (i >= 3) break;
      z *= 2;
    }
    for (;; i++) {
      mask = -(z < 0);
      w += ((w >> (i + 1)) + mask) ^ mask;
      z -= (OD_ATANH_LOG2[i] + mask) ^ mask;
      /*Repeat iteration 13.*/
      if (i >= 12) break;
      z *= 2;
    }
    for (; i < 32; i++) {
      mask = -(z < 0);
      w += ((w >> (i + 1)) + mask) ^ mask;
      z = (z - ((OD_ATANH_LOG2[i] + mask) ^ mask)) * 2;
    }
    wlo = 0;
    /*Skip the remaining iterations unless we really require that much
       precision.
      We could have bailed out earlier for smaller iparts, but that would
       require initializing w from a table, as the limit doesn't converge to
       61-bit precision until n=30.*/
    if (ipart > 30) {
      /*For these iterations, we just update the low bits, as the high bits
         can't possibly be affected.
        OD_ATANH_LOG2 has also converged (it actually did so one iteration
         earlier, but that's no reason for an extra special case).*/
      for (;; i++) {
        mask = -(z < 0);
        wlo += ((w >> i) + mask) ^ mask;
        z -= (OD_ATANH_LOG2[31] + mask) ^ mask;
        /*Repeat iteration 40.*/
        if (i >= 39) break;
        z <<= 1;
      }
      for (; i < 61; i++) {
        mask = -(z < 0);
        wlo += ((w >> i) + mask) ^ mask;
        z = (z - ((OD_ATANH_LOG2[31] + mask) ^ mask)) << 1;
      }
    }
    w = (w << 1) + wlo;
  } else {
    w = (int64_t)1 << 62;
  }
  if (ipart < 62) {
    w = ((w >> (61 - ipart)) + 1) >> 1;
  }
  return w;
}

/*Computes the binary log of w
  input: a 64-bit integer in Q0 (no fraction)
  output: a 64-bit log in Q57 */
static int64_t od_blog64(int64_t w) {
  int64_t z;
  int ipart;
  if (w <= 0) return -1;
  ipart = od_ilog64(w) - 1;
  if (ipart > 61) {
    w >>= ipart - 61;
  } else {
    w <<= 61 - ipart;
  }
  z = 0;
  if (w & (w - 1)) {
    int64_t x;
    int64_t y;
    int64_t u;
    int64_t mask;
    int i;
    /*C doesn't give us 64x64->128 muls, so we use CORDIC.
      This is not particularly fast, but it's not being used in time-critical
       code; it is very accurate.*/
    /*z is the fractional part of the log in Q61 format.*/
    /*x and y are the cosh() and sinh(), respectively, in Q61 format.
      We are computing z = 2*atanh(y/x) = 2*atanh((w - 1)/(w + 1)).*/
    x = w + ((int64_t)1 << 61);
    y = w - ((int64_t)1 << 61);
    for (i = 0; i < 4; i++) {
      mask = -(y < 0);
      z += ((OD_ATANH_LOG2[i] >> i) + mask) ^ mask;
      u = x >> (i + 1);
      x -= ((y >> (i + 1)) + mask) ^ mask;
      y -= (u + mask) ^ mask;
    }
    /*Repeat iteration 4.*/
    for (i--; i < 13; i++) {
      mask = -(y < 0);
      z += ((OD_ATANH_LOG2[i] >> i) + mask) ^ mask;
      u = x >> (i + 1);
      x -= ((y >> (i + 1)) + mask) ^ mask;
      y -= (u + mask) ^ mask;
    }
    /*Repeat iteration 13.*/
    for (i--; i < 32; i++) {
      mask = -(y < 0);
      z += ((OD_ATANH_LOG2[i] >> i) + mask) ^ mask;
      u = x >> (i + 1);
      x -= ((y >> (i + 1)) + mask) ^ mask;
      y -= (u + mask) ^ mask;
    }
    /*OD_ATANH_LOG2 has converged.*/
    for (; i < 40; i++) {
      mask = -(y < 0);
      z += ((OD_ATANH_LOG2[31] >> i) + mask) ^ mask;
      u = x >> (i + 1);
      x -= ((y >> (i + 1)) + mask) ^ mask;
      y -= (u + mask) ^ mask;
    }
    /*Repeat iteration 40.*/
    for (i--; i < 62; i++) {
      mask = -(y < 0);
      z += ((OD_ATANH_LOG2[31] >> i) + mask) ^ mask;
      u = x >> (i + 1);
      x -= ((y >> (i + 1)) + mask) ^ mask;
      y -= (u + mask) ^ mask;
    }
    z = (z + 8) >> 4;
  }
  return OD_Q57(ipart) + z;
}

/*Convenience function converts Q57 value to a clamped 32-bit Q24 value
  in: input in Q57 format.
  Return: same number in Q24 */
static int32_t od_q57_to_q24(int64_t in) {
  int64_t ret;
  ret = (in + ((int64_t)1 << 32)) >> 33;
  /*0x80000000 is automatically converted to unsigned on 32-bit systems.
    -0x7FFFFFFF-1 is needed to avoid "promoting" the whole expression to
    unsigned.*/
  return (int32_t)OD_CLAMPI(-0x7FFFFFFF - 1, ret, 0x7FFFFFFF);
}

/*Binary exponential of log_scale with 24-bit fractional precision and
   saturation.
  log_scale: A binary logarithm in Q57 format.
  Return: The binary exponential in Q24 format, saturated to 2**31-1 if
   log_scale was too large.*/
static int32_t od_bexp64_q24(int64_t log_scale) {
  if (log_scale < OD_Q57(8)) {
    int64_t ret;
    ret = od_bexp64(log_scale + OD_Q57(24));
    return ret < 0x7FFFFFFF ? (int32_t)ret : 0x7FFFFFFF;
  }
  return 0x7FFFFFFF;
}

/*Re-initialize Bessel filter coefficients with the specified delay.
  This does not alter the x/y state, but changes the reaction time of the
   filter.
  Altering the time constant of a reactive filter without alterning internal
   state is something that has to be done carefuly, but our design operates at
   high enough delays and with small enough time constant changes to make it
   safe.*/
static void od_iir_bessel2_reinit(od_iir_bessel2 *f, int delay) {
  int alpha;
  int64_t one48;
  int64_t warp;
  int64_t k1;
  int64_t k2;
  int64_t d;
  int64_t a;
  int64_t ik2;
  int64_t b1;
  int64_t b2;
  /*This borrows some code from an unreleased version of Postfish.
    See the recipe at http://unicorn.us.com/alex/2polefilters.html for details
     on deriving the filter coefficients.*/
  /*alpha is Q24*/
  alpha = (1 << 24) / delay;
  one48 = (int64_t)1 << 48;
  /*warp is 7.12*/
  warp = OD_MAXI(od_warp_alpha(alpha), 1);
  /*k1 is 9.12*/
  k1 = 3 * warp;
  /*k2 is 16.24.*/
  k2 = k1 * warp;
  /*d is 16.15.*/
  d = ((((1 << 12) + k1) << 12) + k2 + 256) >> 9;
  /*a is 0.32, since d is larger than both 1.0 and k2.*/
  a = (k2 << 23) / d;
  /*ik2 is 25.24.*/
  ik2 = one48 / k2;
  /*b1 is Q56; in practice, the integer ranges between -2 and 2.*/
  b1 = 2 * a * (ik2 - (1 << 24));
  /*b2 is Q56; in practice, the integer ranges between -2 and 2.*/
  b2 = (one48 << 8) - ((4 * a) << 24) - b1;
  /*All of the filter parameters are Q24.*/
  f->c[0] = (int32_t)((b1 + ((int64_t)1 << 31)) >> 32);
  f->c[1] = (int32_t)((b2 + ((int64_t)1 << 31)) >> 32);
  f->g = (int32_t)((a + 128) >> 8);
}

/*Initialize a 2nd order low-pass Bessel filter with the corresponding delay
   and initial value.
  value is Q24.*/
static void od_iir_bessel2_init(od_iir_bessel2 *f, int delay, int32_t value) {
  od_iir_bessel2_reinit(f, delay);
  f->y[1] = f->y[0] = f->x[1] = f->x[0] = value;
}

static int64_t od_iir_bessel2_update(od_iir_bessel2 *f, int32_t x) {
  int64_t c0;
  int64_t c1;
  int64_t g;
  int64_t x0;
  int64_t x1;
  int64_t y0;
  int64_t y1;
  int64_t ya;
  c0 = f->c[0];
  c1 = f->c[1];
  g = f->g;
  x0 = f->x[0];
  x1 = f->x[1];
  y0 = f->y[0];
  y1 = f->y[1];
  ya = ((x + x0 * 2 + x1) * g + y0 * c0 + y1 * c1 + (1 << 23)) >> 24;
  f->x[1] = (int32_t)x0;
  f->x[0] = x;
  f->y[1] = (int32_t)y0;
  f->y[0] = (int32_t)ya;
  return ya;
}

static void od_enc_rc_reset(od_rc_state *rc) {
  int64_t npixels;
  int64_t ibpp;
  rc->bits_per_frame = (int64_t)(rc->target_bitrate / rc->framerate);
  /*Insane framerates or frame sizes mean insane bitrates.
    Let's not get carried away.*/
  if (rc->bits_per_frame > 0x400000000000LL) {
    rc->bits_per_frame = (int64_t)0x400000000000LL;
  } else {
    if (rc->bits_per_frame < 32) {
      rc->bits_per_frame = 32;
    }
  }
  rc->reservoir_frame_delay = OD_MAXI(rc->reservoir_frame_delay, 12);
  rc->reservoir_max = rc->bits_per_frame * rc->reservoir_frame_delay;
  /*Start with a buffer fullness and fullness target of 50% */
  rc->reservoir_target = (rc->reservoir_max + 1) >> 1;
  rc->reservoir_fullness = rc->reservoir_target;
  /*Pick exponents and initial scales for quantizer selection.*/
  npixels = rc->frame_width * (int64_t)rc->frame_height;
  rc->log_npixels = od_blog64(npixels);
  ibpp = npixels / rc->bits_per_frame;
  /*All of these initial scale/exp values are from Theora, and have not yet
     been adapted to Daala, so they're certainly wrong.
    The B-frame values especially are simply copies of the P-frame values.*/
  if (ibpp < 1) {
    rc->exp[OD_I_FRAME] = 59;
    rc->log_scale[OD_I_FRAME] = od_blog64(1997) - OD_Q57(OD_COEFF_SHIFT);
  } else if (ibpp < 2) {
    rc->exp[OD_I_FRAME] = 55;
    rc->log_scale[OD_I_FRAME] = od_blog64(1604) - OD_Q57(OD_COEFF_SHIFT);
  } else {
    rc->exp[OD_I_FRAME] = 48;
    rc->log_scale[OD_I_FRAME] = od_blog64(834) - OD_Q57(OD_COEFF_SHIFT);
  }
  if (ibpp < 4) {
    rc->exp[OD_P_FRAME] = 100;
    rc->log_scale[OD_P_FRAME] = od_blog64(2249) - OD_Q57(OD_COEFF_SHIFT);
  } else if (ibpp < 8) {
    rc->exp[OD_P_FRAME] = 95;
    rc->log_scale[OD_P_FRAME] = od_blog64(1751) - OD_Q57(OD_COEFF_SHIFT);
  } else {
    rc->exp[OD_P_FRAME] = 73;
    rc->log_scale[OD_P_FRAME] = od_blog64(1260) - OD_Q57(OD_COEFF_SHIFT);
  }
  /*Golden P-frames both use the same log_scale and exp modeling
     values as regular P-frames and the same scale follower.
    For convenience in the rate calculation code, we maintain a copy of
    the scale and exp values in OD_GOLDEN_P_FRAME.*/
  rc->exp[OD_GOLDEN_P_FRAME] = rc->exp[OD_P_FRAME];
  rc->log_scale[OD_GOLDEN_P_FRAME] = rc->log_scale[OD_P_FRAME];
  rc->exp[OD_ALTREF_P_FRAME] = rc->exp[OD_P_FRAME];
  rc->log_scale[OD_ALTREF_P_FRAME] = rc->log_scale[OD_P_FRAME];
  /*We clamp the actual I and B frame delays to a minimum of 10 to work within
     the range of values where later incrementing the delay works as designed.
    10 is not an exact choice, but rather a good working trade-off.*/
  rc->inter_p_delay = 10;
  rc->inter_delay_target = rc->reservoir_frame_delay >> 1;
  memset(rc->frame_count, 0, sizeof(rc->frame_count));
  /*Drop-frame tracking is concerned with more than just the basic three frame
     types.
    It needs to track boosted and cut subtypes (of which there is only one
     right now, OD_GOLDEN_P_FRAME). */
  rc->prev_drop_count[OD_I_FRAME] = 0;
  rc->log_drop_scale[OD_I_FRAME] = OD_Q57(0);
  rc->prev_drop_count[OD_P_FRAME] = 0;
  rc->log_drop_scale[OD_P_FRAME] = OD_Q57(0);
  rc->prev_drop_count[OD_GOLDEN_P_FRAME] = 0;
  rc->log_drop_scale[OD_GOLDEN_P_FRAME] = OD_Q57(0);
  rc->prev_drop_count[OD_ALTREF_P_FRAME] = 0;
  rc->log_drop_scale[OD_ALTREF_P_FRAME] = OD_Q57(0);
  /*Set up second order followers, initialized according to corresponding
     time constants.*/
  od_iir_bessel2_init(&rc->scalefilter[OD_I_FRAME], 4,
                      od_q57_to_q24(rc->log_scale[OD_I_FRAME]));
  od_iir_bessel2_init(&rc->scalefilter[OD_P_FRAME], rc->inter_p_delay,
                      od_q57_to_q24(rc->log_scale[OD_P_FRAME]));
  od_iir_bessel2_init(&rc->vfrfilter[OD_I_FRAME], 4,
                      od_bexp64_q24(rc->log_drop_scale[OD_I_FRAME]));
  od_iir_bessel2_init(&rc->vfrfilter[OD_P_FRAME], 4,
                      od_bexp64_q24(rc->log_drop_scale[OD_P_FRAME]));
  od_iir_bessel2_init(&rc->vfrfilter[OD_GOLDEN_P_FRAME], 4,
                      od_bexp64_q24(rc->log_drop_scale[OD_GOLDEN_P_FRAME]));
  od_iir_bessel2_init(&rc->vfrfilter[OD_ALTREF_P_FRAME], 4,
                      od_bexp64_q24(rc->log_drop_scale[OD_ALTREF_P_FRAME]));
}

int od_enc_rc_resize(od_rc_state *rc) {
  /*If encoding has not yet begun, reset the buffer state.*/
  if (rc->cur_frame == 0) {
    od_enc_rc_reset(rc);
  } else {
    int idt;
    /*Otherwise, update the bounds on the buffer, but not the current
       fullness.*/
    rc->bits_per_frame = (int64_t)(rc->target_bitrate / rc->framerate);
    /*Insane framerates or frame sizes mean insane bitrates.
      Let's not get carried away.*/
    if (rc->bits_per_frame > 0x400000000000LL) {
      rc->bits_per_frame = (int64_t)0x400000000000LL;
    } else {
      if (rc->bits_per_frame < 32) {
        rc->bits_per_frame = 32;
      }
    }
    rc->reservoir_frame_delay = OD_MAXI(rc->reservoir_frame_delay, 12);
    rc->reservoir_max = rc->bits_per_frame * rc->reservoir_frame_delay;
    rc->reservoir_target =
        ((rc->reservoir_max + 1) >> 1) +
        ((rc->bits_per_frame + 2) >> 2) *
            OD_MINI(rc->keyframe_rate, rc->reservoir_frame_delay);
    /*Update the INTER-frame scale filter delay.
      We jump to it immediately if we've already seen enough frames; otherwise
       it is simply set as the new target.*/
    rc->inter_delay_target = idt = OD_MAXI(rc->reservoir_frame_delay >> 1, 10);
    if (idt < OD_MINI(rc->inter_p_delay, rc->frame_count[OD_P_FRAME])) {
      od_iir_bessel2_init(&rc->scalefilter[OD_P_FRAME], idt,
                          rc->scalefilter[OD_P_FRAME].y[0]);
      rc->inter_p_delay = idt;
    }
  }
  return 0;
}

int od_enc_rc_init(od_rc_state *rc, int64_t bitrate, int delay_ms) {
  if (rc->framerate <= 0) return 1;
  if (rc->target_bitrate > 0) {
    /*State has already been initialized; rather than reinitialize,
      adjust the buffering for the new target rate. */
    rc->target_bitrate = bitrate;
    return od_enc_rc_resize(rc);
  }
  rc->target_quantizer = 0;
  rc->target_bitrate = bitrate;
  rc->rate_bias = 0;
  if (bitrate > 0) {
    /* The buffer size is clamped between [12, 256], this interval is short
       enough to
       allow reaction, but long enough to allow looking into the next GOP
       (avoiding
       the case where the last frames before an I-frame get starved).
       The 12 frame minimum gives us some chance to distribute bit estimation
       errors in the worst case. The 256 frame maximum means we'll require 8-10
       seconds
       of pre-buffering at 24-30 fps, which is not unreasonable.*/
    rc->reservoir_frame_delay =
        (int)OD_MINI((delay_ms / 1000) * rc->framerate, 256);
    rc->drop_frames = 1;
    rc->cap_overflow = 1;
    rc->cap_underflow = 0;
    rc->twopass_state = 0;
    od_enc_rc_reset(rc);
  }
  return 0;
}

/*Scale the number of frames by the number of expected drops/duplicates.*/
static int od_rc_scale_drop(od_rc_state *rc, int frame_type, int nframes) {
  if (rc->prev_drop_count[frame_type] > 0 ||
      rc->log_drop_scale[frame_type] > OD_Q57(0)) {
    int64_t dup_scale;
    dup_scale = od_bexp64(((rc->log_drop_scale[frame_type] +
                            od_blog64(rc->prev_drop_count[frame_type] + 1)) >>
                           1) +
                          OD_Q57(8));
    if (dup_scale < nframes << 8) {
      int dup_scalei;
      dup_scalei = (int)dup_scale;
      if (dup_scalei > 0) {
        nframes = ((nframes << 8) + dup_scalei - 1) / dup_scalei;
      }
    } else {
      nframes = !!nframes;
    }
  }
  return nframes;
}

/*Closed form version of frame determination code.
  Used by rate control to predict frame types and subtypes into the future.
  No side effects, may be called any number of times.
  Note that it ignores end-of-file conditions; one-pass planning *should*
   ignore end-of-file. */
int od_frame_type(od_rc_state *rc, int64_t coding_frame_count, int *is_golden,
                  int *is_altref, int64_t *ip_count) {
  int frame_type;
  if (coding_frame_count == 0) {
    *is_golden = 1;
    *is_altref = 1;
    *ip_count = 0;
    frame_type = OD_I_FRAME;
  } else {
    int keyrate = rc->keyframe_rate;
    if (rc->closed_gop) {
      int ip_per_gop;
      int gop_n;
      int gop_i;
      ip_per_gop = (keyrate - 1) / 2;
      gop_n = coding_frame_count / keyrate;
      gop_i = coding_frame_count - gop_n * keyrate;
      *ip_count = gop_n * ip_per_gop + (gop_i > 0) + (gop_i - 1);
      frame_type = gop_i == 0 ? OD_I_FRAME : OD_P_FRAME;
    } else {
      int ip_per_gop;
      int gop_n;
      int gop_i;
      ip_per_gop = (keyrate);
      gop_n = (coding_frame_count - 1) / keyrate;
      gop_i = coding_frame_count - gop_n * keyrate - 1;
      *ip_count = (coding_frame_count > 0) + gop_n * ip_per_gop + (gop_i);
      frame_type = gop_i / 1 < ip_per_gop - 1 ? OD_P_FRAME : OD_I_FRAME;
    }
  }
  *is_golden =
      (*ip_count % rc->goldenframe_rate) == 0 || frame_type == OD_I_FRAME;
  *is_altref = (*ip_count % rc->altref_rate) == 0 || frame_type == OD_I_FRAME;
  return frame_type;
}

/*Count frames types forward from the current frame up to but not including
   the last I-frame in reservoir_frame_delay.
  If reservoir_frame_delay contains no I-frames (or the current frame is the
   only I-frame), count all reservoir_frame_delay frames.
  Returns the number of frames counted.
  Right now, this implementation is simple, brute-force, and expensive.
  It is also easy to understand and debug.
  TODO: replace with a virtual FIFO that keeps running totals as
   repeating the counting over-and-over will have a performance impact on
   whole-file 2pass usage.*/
static int frame_type_count(od_rc_state *rc, int nframes[OD_FRAME_NSUBTYPES]) {
  int i;
  int j;
  int acc[OD_FRAME_NSUBTYPES];
  int count;
  int reservoir_frames;
  int reservoir_frame_delay;
  memset(nframes, 0, OD_FRAME_NSUBTYPES * sizeof(*nframes));
  memset(acc, 0, sizeof(acc));
  count = 0;
  reservoir_frames = 0;
#if 1
  /*Go ahead and count past end-of-stream.
    We won't nail the exact bitrate on short files that end with a partial
     GOP, but we also won't [potentially] destroy the quality of the last few
     frames in that same case when we suddenly find out the stream is ending
     before the original planning horizon.*/
  reservoir_frame_delay = rc->reservoir_frame_delay;
#else
  /*Don't count past the end of the stream (once we know where end-of-stream
     is).*/
  reservoir_frame_delay =
      rc->end_of_input ? rc->input_size + 1 : rc->reservoir_frame_delay;
#endif
  for (i = 0; i < reservoir_frame_delay; i++) {
    int frame_type;
    int is_golden;
    int is_altref;
    int64_t dummy;
    frame_type =
        od_frame_type(rc, rc->cur_frame + i, &is_golden, &is_altref, &dummy);
    switch (frame_type) {
      case OD_I_FRAME: {
        for (j = 0; j < OD_FRAME_NSUBTYPES; j++) nframes[j] += acc[j];
        reservoir_frames += count;
        memset(acc, 0, sizeof(acc));
        acc[OD_I_FRAME] = 1;
        count = 1;
        break;
      }
      case OD_P_FRAME: {
        if (is_golden) {
          ++acc[OD_GOLDEN_P_FRAME];
          ++count;
        } else if (is_altref) {
          ++acc[OD_ALTREF_P_FRAME];
          ++count;
        } else {
          ++acc[OD_P_FRAME];
          ++count;
        }
        break;
      }
    }
  }
  /*If there were no I-frames at all, or only the first frame was an I-frame,
     the accumulators never flushed and still contain the counts for the
     entire buffer.
    In both these cases, we return these counts.
    Otherwise, we discard what remains in the accumulators as they contain
     the counts from and past the last I-frame.*/
  if (reservoir_frames == 0) {
    for (i = 0; i < OD_FRAME_NSUBTYPES; i++) nframes[i] = acc[i];
    reservoir_frames += count;
  }
  return reservoir_frames;
}

static int convert_to_ac_quant(int q, int bit_depth) {
  return lrint(av1_convert_qindex_to_q(q, bit_depth));
}

int od_enc_rc_select_quantizers_and_lambdas(od_rc_state *rc,
                                            int is_golden_frame,
                                            int is_altref_frame, int frame_type,
                                            int *bottom_idx, int *top_idx) {
  int frame_subtype;
  int64_t log_cur_scale;
  int lossy_quantizer_min;
  int lossy_quantizer_max;
  double mqp_i = OD_MQP_I;
  double mqp_p = OD_MQP_P;
  double mqp_gp = OD_MQP_GP;
  double mqp_ap = OD_MQP_AP;
  int reservoir_frames;
  int nframes[OD_FRAME_NSUBTYPES];
  int32_t mqp_Q12[OD_FRAME_NSUBTYPES];
  int64_t dqp_Q45[OD_FRAME_NSUBTYPES];
  /*Verify the closed-form frame type determination code matches what the
     input queue set.*/
  /*One pseudo-non-closed-form caveat:
    Once we've seen end-of-input, the batched frame determination code
     suppresses the last open-GOP's I-frame (since it would only be
     useful for the next GOP, which doesn't exist).
     Thus, don't check one the input queue is drained.*/
  if (!rc->end_of_input) {
    int closed_form_type;
    int closed_form_golden;
    int closed_form_altref;
    int64_t closed_form_cur_frame;
    closed_form_type =
        od_frame_type(rc, rc->cur_frame, &closed_form_golden,
                      &closed_form_altref, &closed_form_cur_frame);
    OD_UNUSED(closed_form_type);
    OD_UNUSED(is_altref_frame);
    assert(closed_form_type == frame_type);
    assert(closed_form_cur_frame == rc->cur_frame);
    assert(closed_form_altref == is_altref_frame);
    assert(closed_form_golden == is_golden_frame);
  }

  log_cur_scale = (int64_t)rc->scalefilter[frame_type].y[0] << 33;

  /*Count the various types and classes of frames.*/
  reservoir_frames = frame_type_count(rc, nframes);
  nframes[OD_I_FRAME] = od_rc_scale_drop(rc, OD_I_FRAME, nframes[OD_I_FRAME]);
  nframes[OD_P_FRAME] = od_rc_scale_drop(rc, OD_P_FRAME, nframes[OD_P_FRAME]);
  nframes[OD_GOLDEN_P_FRAME] =
      od_rc_scale_drop(rc, OD_GOLDEN_P_FRAME, nframes[OD_GOLDEN_P_FRAME]);
  nframes[OD_ALTREF_P_FRAME] =
      od_rc_scale_drop(rc, OD_ALTREF_P_FRAME, nframes[OD_ALTREF_P_FRAME]);

  switch (rc->twopass_state) {
    default: break;
    case 1: {
      /*Pass 1 mode: use a fixed qi value.*/
      return rc->firstpass_quant;
    } break;
    case 2: {
      int i;
      int64_t scale_sum[OD_FRAME_NSUBTYPES];
      int qti;
      /*Pass 2 mode: we know exactly how much of each frame type there is in
         the current buffer window, and have estimates for the scales.*/
      for (i = 0; i < OD_FRAME_NSUBTYPES; i++) {
        nframes[i] = rc->nframes[i];
        nframes[i] = rc->nframes[i];
        scale_sum[i] = rc->scale_sum[i];
      }
      /*If we're not using the same frame type as in pass 1 (because someone
         changed the keyframe interval), remove that scale estimate.
        We'll add in a replacement for the correct frame type below.*/
      qti = rc->cur_metrics.frame_type;
      if (qti != frame_type) {
        nframes[qti]--;
        scale_sum[qti] -= od_bexp64_q24(rc->cur_metrics.log_scale);
      }
      /*Compute log_scale estimates for each frame type from the pass-1 scales
         we measured in the current window.*/
      for (qti = 0; qti < OD_FRAME_NSUBTYPES; qti++) {
        rc->log_scale[qti] = nframes[qti] > 0
                                 ? od_blog64(scale_sum[qti]) -
                                       od_blog64(nframes[qti]) - OD_Q57(24)
                                 : -rc->log_npixels;
      }
      /*If we're not using the same frame type as in pass 1, add a scale
         estimate for the corresponding frame using the current low-pass
         filter value.
        This is mostly to ensure we have a valid estimate even when pass 1 had
         no frames of this type in the buffer window.
        TODO: We could also plan ahead and figure out how many keyframes we'll
         be forced to add in the current buffer window.*/
      qti = rc->cur_metrics.frame_type;
      if (qti != frame_type) {
        int64_t scale;
        scale = rc->log_scale[frame_type] < OD_Q57(23)
                    ? od_bexp64(rc->log_scale[frame_type] + OD_Q57(24))
                    : 0x7FFFFFFFFFFFLL;
        scale *= nframes[frame_type];
        nframes[frame_type]++;
        scale += od_bexp64_q24(log_cur_scale >> 33);
        rc->log_scale[frame_type] =
            od_blog64(scale) - od_blog64(nframes[qti]) - OD_Q57(24);
      } else {
        log_cur_scale = (int64_t)rc->cur_metrics.log_scale << 33;
      }
    } break;
  }

  /*Quantizer selection sticks to the codable, lossy portion of the quantizer
    range.*/
  lossy_quantizer_min = convert_to_ac_quant(rc->minq, rc->bit_depth);
  lossy_quantizer_max = convert_to_ac_quant(rc->maxq, rc->bit_depth);
  frame_subtype = frame_type;
  /*Stash quantizer modulation by frame type.*/
  mqp_Q12[OD_I_FRAME] = OD_F_Q12(mqp_i);
  mqp_Q12[OD_P_FRAME] = OD_F_Q12(mqp_p);
  mqp_Q12[OD_GOLDEN_P_FRAME] = OD_F_Q12(mqp_gp);
  mqp_Q12[OD_ALTREF_P_FRAME] = OD_F_Q12(mqp_ap);
  dqp_Q45[OD_I_FRAME] = OD_F_Q45(OD_DQP_I);
  dqp_Q45[OD_P_FRAME] = OD_F_Q45(OD_DQP_P);
  dqp_Q45[OD_GOLDEN_P_FRAME] = OD_F_Q45(OD_DQP_GP);
  dqp_Q45[OD_ALTREF_P_FRAME] = OD_F_Q45(OD_DQP_AP);
  /*Is rate control active?*/
  if (rc->target_bitrate <= 0) {
    /*Rate control is not active; derive quantizer directly from
      quality parameter and frame type. */
    /*Can't use the OD_LOSSLESS macro, as it uses state.quantizer to intuit,
      and we've not set it yet.*/
    if (rc->quality == 0) {
      /*Lossless coding requested.*/
      rc->base_quantizer = 0;
      rc->target_quantizer = 0;
    } else {
      int64_t log_quantizer;

      /* Adjust the modulation constants using the last frame's quantizer. */
      double mqp_delta = (255 - rc->target_quantizer) / 2000.0f;
      mqp_i -= mqp_delta;
      mqp_p += mqp_delta;
      mqp_gp -= mqp_delta;
      mqp_Q12[OD_I_FRAME] = OD_F_Q12(mqp_i);
      mqp_Q12[OD_P_FRAME] = OD_F_Q12(mqp_p);
      mqp_Q12[OD_GOLDEN_P_FRAME] = OD_F_Q12(mqp_gp);
      mqp_Q12[OD_ALTREF_P_FRAME] = OD_F_Q12(mqp_ap);

      if (rc->quality == -1) {
        /*A quality of -1 means quality was unset; use a default.*/
        rc->base_quantizer = convert_to_ac_quant(10, rc->bit_depth);
      } else {
        rc->base_quantizer = convert_to_ac_quant(rc->quality, rc->bit_depth);
      }

      if (rc->periodic_boosts && !is_golden_frame) {
        int pattern_rate = (rc->goldenframe_rate >> 1);
        int dist_to_golden = rc->cur_frame % pattern_rate;
        int dist_away_golden = pattern_rate - dist_to_golden;
        int boost = dist_to_golden;
        if (dist_away_golden > dist_to_golden) boost = dist_away_golden;
        boost -= pattern_rate;
        boost *= (rc->base_quantizer) / OD_PERIODIC_BOOST_DIV;
        rc->base_quantizer = rc->base_quantizer + boost;
      }

      /*As originally written, qp modulation is applied to the coded quantizer.
        Because we now have and use a more precise target quantizer for various
        calculation, that needs to be modulated as well.
        Calculate what is, effectively, a fractional coded quantizer. */
      /*Get the log2 quantizer in Q57 (normalized for coefficient shift).*/
      log_quantizer = od_blog64(rc->base_quantizer) - OD_Q57(OD_COEFF_SHIFT);
      /*log_quantizer to Q21.*/
      log_quantizer >>= 36;
      /*scale log quantizer, result is Q33.*/
      log_quantizer *= OD_LOG_QUANTIZER_BASE_Q12;
      /*Add Q33 offset to Q33 log_quantizer.*/
      log_quantizer += OD_LOG_QUANTIZER_OFFSET_Q45 >> 12;
      /*Modulate quantizer according to frame type; result is Q45.*/
      log_quantizer *= mqp_Q12[frame_subtype];
      /*Add Q45 boost/cut to Q45 fractional coded quantizer.*/
      log_quantizer += dqp_Q45[frame_subtype];
      /*Back to log2 quantizer in Q57.*/
      log_quantizer = (log_quantizer - OD_LOG_QUANTIZER_OFFSET_Q45) *
                          OD_LOG_QUANTIZER_EXP_Q12 +
                      OD_Q57(OD_COEFF_SHIFT);
      /*Convert Q57 log2 quantizer to unclamped linear target quantizer value.*/
      rc->target_quantizer = od_bexp64(log_quantizer);
    }
  } else {
    int clamp;
    int64_t rate_bias;
    int64_t rate_total;
    int base_quantizer;
    int64_t log_quantizer;
    int qlo;
    int qhi;
    int i;
    /*We clamp the allowed amount of qi change (after initialization).*/
    clamp = rc->cur_frame > 0;
    /*Figure out how to re-distribute bits so that we hit our fullness target
       before the last keyframe in our current buffer window (after the current
       frame), or the end of the buffer window, whichever comes first.*/
    /*Single pass only right now.*/
    /*If we've been missing our target, add a penalty term.*/
    rate_bias = (rc->rate_bias / (rc->cur_frame + 1000)) * reservoir_frames;
    /*rate_total is the total bits available over the next
       reservoir_frames frames.*/
    rate_total = rc->reservoir_fullness - rc->reservoir_target + rate_bias +
                 reservoir_frames * rc->bits_per_frame;
    /*Find a target quantizer that meets our rate target for the specific mix
       of frame types we'll have over the next frame_delay frames.
      We model the rate<->quantizer relationship as:
       rate = scale*(quantizer**-exp)
      In this case, we have our desired rate, an exponent selected in setup,
       and a scale that's been measured over our frame history, so we're
       solving for the quantizer.
      Exponentiation with arbitrary exponents is expensive, so we work in
       the binary log domain (binary exp and log aren't too bad):
       rate = e2(log2_scale - log2_quantizer * exp)
      There's no easy closed form solution, so we bisection search for it.*/
    /*We do not currently allow rate control to select lossless encoding.*/
    qlo = 1;
    /*If there's a quality specified, it's used to select the
       coarsest base quantizer we can select.
      Otherwise we can use up to and including the coarsest codable
       quantizer.*/
    if (rc->quality > 0)
      qhi = convert_to_ac_quant(rc->quality, rc->bit_depth);
    else
      qhi = lossy_quantizer_max;
    base_quantizer = (qlo + qhi) >> 1;
    while (qlo < qhi) {
      volatile int64_t log_base_quantizer;
      int64_t diff;
      int64_t bits;
      /*Count bits contributed by each frame type using the model.*/
      bits = 0;
      log_base_quantizer = od_blog64(base_quantizer);
      for (i = 0; i < OD_FRAME_NSUBTYPES; i++) {
        /*Modulate base quantizer by frame type.*/
        /*Get the log2 quantizer in Q57 (normalized for coefficient shift).*/
        log_quantizer = log_base_quantizer - OD_Q57(OD_COEFF_SHIFT);
        /*log_quantizer to Q21.*/
        log_quantizer >>= 36;
        /*scale log quantizer, result is Q33.*/
        log_quantizer *= OD_LOG_QUANTIZER_BASE_Q12;
        /*Add Q33 offset to Q33 log_quantizer.*/
        log_quantizer += OD_LOG_QUANTIZER_OFFSET_Q45 >> 12;
        /*Modulate quantizer according to frame type; result is Q45.*/
        log_quantizer *= mqp_Q12[i];
        /*Add Q45 boost/cut to Q45 fractional coded quantizer.*/
        log_quantizer += dqp_Q45[i];
        /*Back to log2 quantizer in Q57.*/
        log_quantizer = (log_quantizer - OD_LOG_QUANTIZER_OFFSET_Q45) *
                            OD_LOG_QUANTIZER_EXP_Q12 +
                        OD_Q57(OD_COEFF_SHIFT);
        /*Clamp modulated quantizer values.*/
        log_quantizer = OD_CLAMPI(od_blog64(lossy_quantizer_min), log_quantizer,
                                  od_blog64(lossy_quantizer_max));
        /* All the fields here are Q57 except for the exponent which is Q6.*/
        bits += nframes[i] * od_bexp64(rc->log_scale[i] + rc->log_npixels -
                                       (log_quantizer >> 6) * rc->exp[i]);
      }
      diff = bits - rate_total;
      if (diff > 0) {
        qlo = base_quantizer + 1;
      } else if (diff < 0) {
        qhi = base_quantizer - 1;
      } else {
        break;
      }
      base_quantizer = (qlo + qhi) >> 1;
    }
    /*If this was not one of the initial frames, limit the change in base
       quantizer to within [0.8*Q,1.2*Q], where Q is the previous frame's
       base quantizer.*/
    if (clamp) {
      base_quantizer = OD_CLAMPI((rc->base_quantizer * 0x0CCCD + 0x8000) >> 16,
                                 base_quantizer,
                                 (rc->base_quantizer * 0x13333 + 0x8000) >> 16);
    }
    /*Modulate chosen base quantizer to produce target quantizer.*/
    log_quantizer = od_blog64(base_quantizer);
    /*Get the log2 quantizer in Q57 (normalized for coefficient shift).*/
    log_quantizer -= OD_Q57(OD_COEFF_SHIFT);
    /*log_quantizer to Q21.*/
    log_quantizer >>= 36;
    /*scale log quantizer, result is Q33.*/
    log_quantizer *= OD_LOG_QUANTIZER_BASE_Q12;
    /*Add Q33 offset to Q33 log_quantizer.*/
    log_quantizer += OD_LOG_QUANTIZER_OFFSET_Q45 >> 12;
    /*Modulate quantizer according to frame type; result is Q45.*/
    log_quantizer *= mqp_Q12[frame_subtype];
    /*Add Q45 boost/cut to Q45 fractional coded quantizer.*/
    log_quantizer += dqp_Q45[frame_subtype];
    /*Back to log2 quantizer in Q57.*/
    log_quantizer = (log_quantizer - OD_LOG_QUANTIZER_OFFSET_Q45) *
                        OD_LOG_QUANTIZER_EXP_Q12 +
                    OD_Q57(OD_COEFF_SHIFT);
    /*Clamp modulated quantizer values.*/
    log_quantizer = OD_CLAMPI(od_blog64(lossy_quantizer_min), log_quantizer,
                              od_blog64(lossy_quantizer_max));
    /*The above allocation looks only at the total rate we'll accumulate in
       the next reservoir_frame_delay frames.
      However we could overflow the bit reservoir on the very next frame, so
       check for that here if we're not using a soft target.*/
    if (rc->cap_overflow) {
      int64_t margin;
      int64_t soft_limit;
      int64_t log_soft_limit;
      int64_t log_scale_pixels;
      int64_t exp;
      int64_t log_qexp;
      /*Allow 3% of the buffer for prediction error.
        This should be plenty, and we don't mind if we go a bit over; we only
         want to keep these bits from being completely wasted.*/
      margin = (rc->reservoir_max + 31) >> 5;
      /*We want to use at least this many bits next frame.*/
      soft_limit = rc->reservoir_fullness + rc->bits_per_frame -
                   (rc->reservoir_max - margin);
      log_soft_limit = od_blog64(soft_limit);
      /*If we're predicting we won't use that many bits...*/
      log_scale_pixels = rc->log_scale[frame_subtype] + rc->log_npixels;
      exp = rc->exp[frame_subtype];
      log_qexp = (log_quantizer >> 6) * exp;
      if (log_scale_pixels - log_qexp < log_soft_limit) {
        /*Scale the adjustment based on how far into the margin we are.*/
        log_qexp += ((log_scale_pixels - log_soft_limit - log_qexp) >> 32) *
                    (OD_MINI(margin, soft_limit) << 32) / margin;
        log_quantizer = (((log_qexp + (exp >> 1)) / exp) << 6);
      }
    }
    /*We just checked we don't overflow the reservoir next frame, now check
       we don't underflow and bust the budget (when not using a soft target).
      Disabled when a quality bound is set; if we saturate quantizer to the
       maximum possible size when we have a limiting max quality, the
       resulting lambda can cause strange behavior.*/
    if (rc->quality == -1) {
      int64_t exp;
      int64_t log_qexp;
      int64_t log_scale_pixels;
      int64_t log_hard_limit;
      /*Compute the maximum number of bits we can use in the next frame.
        Allow 50% of the rate for a single frame for prediction error.
        This may not be enough for keyframes or sudden changes in
         complexity.*/
      log_hard_limit =
          od_blog64(rc->reservoir_fullness + (rc->bits_per_frame >> 1));
      /*If we're predicting we'll use more than this...*/
      log_scale_pixels = rc->log_scale[frame_subtype] + rc->log_npixels;
      exp = rc->exp[frame_subtype];
      log_qexp = (log_quantizer >> 6) * exp;
      if (log_scale_pixels - log_qexp > log_hard_limit) {
        /*Force the target to hit our limit exactly.*/
        log_qexp = log_scale_pixels - log_hard_limit;
        log_quantizer = (log_qexp + (exp >> 1)) / exp << 6;
        /*If that target is unreasonable, oh well; we'll have to drop.*/
        log_quantizer = OD_MAXI(log_quantizer, od_blog64(lossy_quantizer_max));
      }
    }
    /*Compute a final estimate of the number of bits we plan to use, update
       the running rate bias measurement.*/
    {
      int64_t log_qexp;
      int64_t log_scale_pixels;
      log_scale_pixels = rc->log_scale[frame_subtype] + rc->log_npixels;
      log_qexp = (log_quantizer >> 6) * rc->exp[frame_subtype];
      rc->rate_bias += od_bexp64(log_scale_pixels - log_qexp);
    }
    rc->target_quantizer = od_bexp64(log_quantizer);
    /*The various cappings and adjustments may have altered the log_quantizer
       target significantly.
      We can either update the base quantizer to be consistent with the
       target or let it track separately.
      Theora behavior effectively keeps them consistent, as it regenerates
       the effective base quantizer from the target each frame rather than
       saving both.
      For Daala, it's easier to allow them to track separately.
      For now, allow them to track separately and see how it behaves.*/
    rc->base_quantizer = base_quantizer;
  }
  *bottom_idx = lossy_quantizer_min;
  *top_idx = lossy_quantizer_max;
  rc->target_quantizer = av1_qindex_from_ac(
      OD_CLAMPI(lossy_quantizer_min, rc->target_quantizer, lossy_quantizer_max),
      rc->bit_depth);
  return rc->target_quantizer;
}

int od_enc_rc_update_state(od_rc_state *rc, int64_t bits, int is_golden_frame,
                           int is_altref_frame, int frame_type, int droppable) {
  int dropped;
  dropped = 0;
  /*Update rate control only if rate control is active.*/
  if (rc->target_bitrate > 0) {
    int64_t log_scale;
    int frame_subtype;
    frame_subtype = frame_type;
    /*Track non-golden and golden P frame drops separately.*/
    if (is_golden_frame && frame_type == OD_P_FRAME)
      frame_subtype = OD_GOLDEN_P_FRAME;
    else if (is_altref_frame && frame_type == OD_P_FRAME)
      frame_subtype = OD_ALTREF_P_FRAME;
    if (bits <= 0) {
      /*We didn't code any blocks in this frame.*/
      log_scale = OD_Q57(-64);
      bits = 0;
      ++rc->prev_drop_count[frame_subtype];
    } else {
      int64_t log_bits;
      int64_t log_qexp;
      /*Compute the estimated scale factor for this frame type.*/
      log_bits = od_blog64(bits);
      log_qexp = od_blog64(rc->target_quantizer);
      log_qexp = (log_qexp >> 6) * (rc->exp[frame_type]);
      log_scale = OD_MINI(log_bits - rc->log_npixels + log_qexp, OD_Q57(16));
    }

    switch (rc->twopass_state) {
      case 1: {
        int golden, altref;
        int64_t ipc;
        rc->cur_metrics.frame_type =
            od_frame_type(rc, rc->cur_frame, &golden, &altref, &ipc);
        /*Pass 1 mode: save the metrics for this frame.*/
        rc->cur_metrics.log_scale = od_q57_to_q24(log_scale);
      } break;
      case 2: {
        /*Pass 2 mode:*/
        int m_frame_type = rc->cur_metrics.frame_type;
        rc->nframes[m_frame_type]--;
        rc->scale_sum[m_frame_type] -= od_bexp64_q24(rc->cur_metrics.log_scale);
      } break;
    }

    if (bits > 0) {
      od_iir_bessel2 *f;
      /*If this is the first example of the given frame type we've
         seen, we immediately replace the default scale factor guess
         with the estimate we just computed using the first frame.*/
      if (rc->frame_count[frame_type] == 0) {
        f = rc->scalefilter + frame_type;
        f->y[1] = f->y[0] = f->x[1] = f->x[0] = od_q57_to_q24(log_scale);
        rc->log_scale[frame_type] = log_scale;
      } else {
        /*Lengthen the time constant for the inter filters as we collect more
           frame statistics, until we reach our target.*/
        if (frame_type != OD_I_FRAME &&
            rc->inter_p_delay < rc->inter_delay_target &&
            rc->frame_count[frame_type] >= rc->inter_p_delay) {
          od_iir_bessel2_reinit(&rc->scalefilter[frame_type],
                                ++rc->inter_p_delay);
        }
        /*Update the low-pass scale filter for this frame type
           regardless of whether or not we drop this frame.*/
        rc->log_scale[frame_type] =
            od_iir_bessel2_update(rc->scalefilter + frame_type,
                                  od_q57_to_q24(log_scale))
            << 33;
      }
      /*If this frame busts our budget, it must be dropped.*/
      if (droppable && rc->reservoir_fullness + rc->bits_per_frame < bits) {
        ++rc->prev_drop_count[frame_subtype];
        bits = 0;
        dropped = 1;
      } else {
        uint32_t drop_count;
        /*Update a low-pass filter to estimate the "real" frame rate taking
           drops into account.
          This is only done if the frame is coded, as it needs the final
           count of dropped frames.*/
        drop_count = rc->prev_drop_count[frame_subtype] + 1;
        if (drop_count > 0x7F) {
          drop_count = 0x7FFFFFFF;
        } else {
          drop_count <<= 24;
        }
        rc->log_drop_scale[frame_subtype] =
            od_blog64(od_iir_bessel2_update(rc->vfrfilter + frame_subtype,
                                            drop_count)) -
            OD_Q57(24);
        /*Zero the drop count for this frame.
          It will be increased if we drop frames.*/
        rc->prev_drop_count[frame_subtype] = 0;
      }
      /*Increment the frame count for filter adaptation purposes.*/
      if (!rc->twopass_state) rc->frame_count[frame_type]++;
    }
    rc->reservoir_fullness += rc->bits_per_frame - bits;
    /*If we're too quick filling the buffer and overflow is capped,
      that rate is lost forever.*/
    if (rc->cap_overflow && rc->reservoir_fullness > rc->reservoir_max) {
      rc->reservoir_fullness = rc->reservoir_max;
    }
    /*If we're too quick draining the buffer and underflow is capped,
      don't try to make up that rate later.*/
    if (rc->cap_underflow && rc->reservoir_fullness < 0) {
      rc->reservoir_fullness = 0;
    }
    /*Adjust the bias for the real bits we've used.*/
    rc->rate_bias -= bits;
  }
  return dropped;
}

static INLINE void od_rc_buffer_val(od_rc_state *rc, int64_t val, int bytes) {
  while (bytes-- > 0) {
    rc->twopass_buffer[rc->twopass_buffer_bytes++] = (uint8_t)(val & 0xFF);
    val >>= 8;
  }
}

static INLINE int64_t od_rc_unbuffer_val(od_rc_state *rc, int bytes) {
  int64_t ret = 0;
  int shift = 0;
  while (bytes-- > 0) {
    ret |= ((int64_t)rc->twopass_buffer[rc->twopass_buffer_bytes++]) << shift;
    shift += 8;
  }
  return ret;
}

int od_enc_rc_2pass_out(od_rc_state *rc, struct aom_codec_pkt_list *pkt_list,
                        int summary) {
  int i;
  struct aom_codec_cx_pkt pkt;
  rc->twopass_buffer = rc->firstpass_buffer;
  rc->twopass_buffer_bytes = 0;
  if (!rc->twopass_state) {
    rc->twopass_state = 1;
    for (i = 0; i < OD_FRAME_NSUBTYPES; i++) {
      rc->frame_count[i] = 0;
      rc->exp[i] = 0;
      rc->scale_sum[i] = 0;
    }
  }
  if (summary) {
    od_rc_buffer_val(rc, OD_RC_2PASS_MAGIC, 4);
    od_rc_buffer_val(rc, OD_RC_2PASS_VERSION, 1);
    for (i = 0; i < OD_FRAME_NSUBTYPES; i++) {
      od_rc_buffer_val(rc, rc->frame_count[i], 4);
      od_rc_buffer_val(rc, rc->exp[i], 4);
      od_rc_buffer_val(rc, rc->scale_sum[i], 8);
    }
  } else {
    int frame_type = rc->cur_metrics.frame_type;
    rc->scale_sum[frame_type] += od_bexp64_q24(rc->cur_metrics.log_scale);
    rc->frame_count[frame_type]++;
    od_rc_buffer_val(rc, rc->cur_metrics.frame_type, 1);
    od_rc_buffer_val(rc, rc->cur_metrics.log_scale, 4);
  }
  pkt.data.twopass_stats.buf = rc->firstpass_buffer;
  pkt.data.twopass_stats.sz = rc->twopass_buffer_bytes;
  pkt.kind = AOM_CODEC_STATS_PKT;
  aom_codec_pkt_list_add(pkt_list, &pkt);
  return 0;
}

int od_enc_rc_2pass_in(od_rc_state *rc) {
  /* Enable pass 2 mode if this is the first call. */
  if (rc->twopass_state == 0) {
    uint32_t i, total_frames = 0;

    if (!rc->twopass_allframes_buf ||
        rc->twopass_allframes_buf_size < OD_RC_2PASS_MIN)
      return -1;

    /* Find summary packet at the end */
    rc->twopass_buffer = rc->twopass_allframes_buf;
    rc->twopass_buffer +=
        rc->twopass_allframes_buf_size - OD_RC_2PASS_SUMMARY_SZ;
    rc->twopass_buffer_bytes = 0;

    if (od_rc_unbuffer_val(rc, 4) != OD_RC_2PASS_MAGIC) return -1;
    if (od_rc_unbuffer_val(rc, 1) != OD_RC_2PASS_VERSION) return -1;

    for (i = 0; i < OD_FRAME_NSUBTYPES; i++) {
      rc->frame_count[i] = od_rc_unbuffer_val(rc, 4);
      rc->exp[i] = od_rc_unbuffer_val(rc, 4);
      rc->scale_sum[i] = od_rc_unbuffer_val(rc, 8);
      rc->nframes[i] = rc->frame_count[i];
      total_frames += rc->frame_count[i];
    }

    if (total_frames < 1) return -1;

    if (total_frames * OD_RC_2PASS_PACKET_SZ > rc->twopass_allframes_buf_size)
      return -1;

    od_enc_rc_reset(rc);

    /* Everything looks ok */
    rc->twopass_buffer = rc->twopass_allframes_buf;
    rc->twopass_state = 2;
    rc->twopass_buffer_bytes = 0;
  }

  rc->cur_metrics.frame_type = od_rc_unbuffer_val(rc, 1);
  rc->cur_metrics.log_scale = od_rc_unbuffer_val(rc, 4);

  return 0;
}
