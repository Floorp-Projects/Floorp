/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * entropy_coding.c
 *
 * This header file defines all of the functions used to arithmetically
 * encode the iSAC bistream
 *
 */


#include "entropy_coding.h"
#include "settings.h"
#include "arith_routines.h"
#include "signal_processing_library.h"
#include "spectrum_ar_model_tables.h"
#include "lpc_tables.h"
#include "pitch_gain_tables.h"
#include "pitch_lag_tables.h"
#include "encode_lpc_swb.h"
#include "lpc_shape_swb12_tables.h"
#include "lpc_shape_swb16_tables.h"
#include "lpc_gain_swb_tables.h"
#include "os_specific_inline.h"

#include <math.h>
#include <string.h>

static const WebRtc_UWord16 kLpcVecPerSegmentUb12 = 5;
static const WebRtc_UWord16 kLpcVecPerSegmentUb16 = 4;

/* coefficients for the stepwise rate estimation */
static const WebRtc_Word32 kRPointsQ10[100] = {
  14495,  14295,  14112,  13944,  13788,  13643,  13459,  13276,  13195,  13239,
  13243,  13191,  13133,  13216,  13263,  13330,  13316,  13242,  13191,  13106,
  12942,  12669,  12291,  11840,  11361,  10795,  10192,  9561,  8934,  8335,
  7750,  7161,  6589,  6062,  5570,  5048,  4548,  4069,  3587,  3143,
  2717,  2305,  1915,  1557,  1235,  963,  720,  541,  423,  366,
  369,  435,  561,  750,  1001,  1304,  1626,  1989,  2381,  2793,
  3219,  3656,  4134,  4612,  5106,  5629,  6122,  6644,  7216,  7801,
  8386,  8987,  9630,  10255,  10897,  11490,  11950,  12397,  12752,  12999,
  13175,  13258,  13323,  13290,  13296,  13335,  13113,  13255,  13347,  13355,
  13298,  13247,  13313,  13155,  13267,  13313,  13374,  13446,  13525,  13609};


/* cdf array for encoder bandwidth (12 vs 16 kHz) indicator */
static const WebRtc_UWord16 kOneBitEqualProbCdf[3] = {
  0, 32768, 65535 };

/* pointer to cdf array for encoder bandwidth (12 vs 16 kHz) indicator */
static const WebRtc_UWord16 *kOneBitEqualProbCdf_ptr[1] = {
    kOneBitEqualProbCdf };

/* initial cdf index for decoder of encoded bandwidth (12 vs 16 kHz) indicator */
static const WebRtc_UWord16 kOneBitEqualProbInitIndex[1] = {1};


/* coefficients for the stepwise rate estimation */


static const WebRtc_Word32 acnQ10 =  426;
static const WebRtc_Word32 bcnQ10 = -581224;
static const WebRtc_Word32 ccnQ10 =  722631;
static const WebRtc_Word32 lbcnQ10 = -402874;
#define DPMIN_Q10     -10240 // -10.00 in Q10
#define DPMAX_Q10      10240 // 10.00 in Q10
#define MINBITS_Q10    10240  /* 10.0 in Q10 */
#define IS_SWB_12KHZ       1

__inline WebRtc_UWord32 stepwise(WebRtc_Word32 dinQ10) {

  WebRtc_Word32 ind, diQ10, dtQ10;

  diQ10 = dinQ10;
  if (diQ10 < DPMIN_Q10)
    diQ10 = DPMIN_Q10;
  if (diQ10 >= DPMAX_Q10)
    diQ10 = DPMAX_Q10 - 1;

  dtQ10 = diQ10 - DPMIN_Q10; /* Q10 + Q10 = Q10 */
  ind = (dtQ10 * 5) >> 10;   /* 2^10 / 5 = 0.2 in Q10  */
  /* Q10 -> Q0 */

  return kRPointsQ10[ind];
}


__inline short log2_Q10_B( int x )
{
  int zeros;
  short frac;

  zeros = WebRtcSpl_NormU32( x );
  frac = ((unsigned int)(x << zeros) & 0x7FFFFFFF) >> 21;
  return (short) (((31 - zeros) << 10) + frac);
}



/* compute correlation from power spectrum */
static void WebRtcIsac_FindCorrelation(WebRtc_Word32 *PSpecQ12, WebRtc_Word32 *CorrQ7)
{
  WebRtc_Word32 summ[FRAMESAMPLES/8];
  WebRtc_Word32 diff[FRAMESAMPLES/8];
  const WebRtc_Word16 *CS_ptrQ9;
  WebRtc_Word32 sum;
  int k, n;

  for (k = 0; k < FRAMESAMPLES/8; k++) {
    summ[k] = (PSpecQ12[k] + PSpecQ12[FRAMESAMPLES_QUARTER-1 - k] + 16) >> 5;
    diff[k] = (PSpecQ12[k] - PSpecQ12[FRAMESAMPLES_QUARTER-1 - k] + 16) >> 5;
  }

  sum = 2;
  for (n = 0; n < FRAMESAMPLES/8; n++)
    sum += summ[n];
  CorrQ7[0] = sum;

  for (k = 0; k < AR_ORDER; k += 2) {
    sum = 0;
    CS_ptrQ9 = WebRtcIsac_kCos[k];
    for (n = 0; n < FRAMESAMPLES/8; n++)
      sum += (CS_ptrQ9[n] * diff[n] + 256) >> 9;
    CorrQ7[k+1] = sum;
  }

  for (k=1; k<AR_ORDER; k+=2) {
    sum = 0;
    CS_ptrQ9 = WebRtcIsac_kCos[k];
    for (n = 0; n < FRAMESAMPLES/8; n++)
      sum += (CS_ptrQ9[n] * summ[n] + 256) >> 9;
    CorrQ7[k+1] = sum;
  }
}

/* compute inverse AR power spectrum */
/* Changed to the function used in iSAC FIX for compatibility reasons */
static void WebRtcIsac_FindInvArSpec(const WebRtc_Word16 *ARCoefQ12,
                                     const WebRtc_Word32 gainQ10,
                                     WebRtc_Word32 *CurveQ16)
{
  WebRtc_Word32 CorrQ11[AR_ORDER+1];
  WebRtc_Word32 sum, tmpGain;
  WebRtc_Word32 diffQ16[FRAMESAMPLES/8];
  const WebRtc_Word16 *CS_ptrQ9;
  int k, n;
  WebRtc_Word16 round, shftVal = 0, sh;

  sum = 0;
  for (n = 0; n < AR_ORDER+1; n++)
    sum += WEBRTC_SPL_MUL(ARCoefQ12[n], ARCoefQ12[n]);   /* Q24 */
  sum = WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(WEBRTC_SPL_RSHIFT_W32(sum, 6), 65) + 32768, 16); /* result in Q8 */
  CorrQ11[0] = WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(sum, gainQ10) + 256, 9);

  /* To avoid overflow, we shift down gainQ10 if it is large. We will not lose any precision */
  if(gainQ10>400000){
    tmpGain = WEBRTC_SPL_RSHIFT_W32(gainQ10, 3);
    round = 32;
    shftVal = 6;
  } else {
    tmpGain = gainQ10;
    round = 256;
    shftVal = 9;
  }

  for (k = 1; k < AR_ORDER+1; k++) {
    sum = 16384;
    for (n = k; n < AR_ORDER+1; n++)
      sum += WEBRTC_SPL_MUL(ARCoefQ12[n-k], ARCoefQ12[n]); /* Q24 */
    sum = WEBRTC_SPL_RSHIFT_W32(sum, 15);
    CorrQ11[k] = WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(sum, tmpGain) + round, shftVal);
  }
  sum = WEBRTC_SPL_LSHIFT_W32(CorrQ11[0], 7);
  for (n = 0; n < FRAMESAMPLES/8; n++)
    CurveQ16[n] = sum;

  for (k = 1; k < AR_ORDER; k += 2) {
    //CS_ptrQ9 = WebRtcIsac_kCos[k];
    for (n = 0; n < FRAMESAMPLES/8; n++)
      CurveQ16[n] += WEBRTC_SPL_RSHIFT_W32(
          WEBRTC_SPL_MUL(WebRtcIsac_kCos[k][n], CorrQ11[k+1]) + 2, 2);
  }

  CS_ptrQ9 = WebRtcIsac_kCos[0];

  /* If CorrQ11[1] too large we avoid getting overflow in the calculation by shifting */
  sh=WebRtcSpl_NormW32(CorrQ11[1]);
  if (CorrQ11[1]==0) /* Use next correlation */
    sh=WebRtcSpl_NormW32(CorrQ11[2]);

  if (sh<9)
    shftVal = 9 - sh;
  else
    shftVal = 0;

  for (n = 0; n < FRAMESAMPLES/8; n++)
    diffQ16[n] = WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(CS_ptrQ9[n], WEBRTC_SPL_RSHIFT_W32(CorrQ11[1], shftVal)) + 2, 2);
  for (k = 2; k < AR_ORDER; k += 2) {
    CS_ptrQ9 = WebRtcIsac_kCos[k];
    for (n = 0; n < FRAMESAMPLES/8; n++)
      diffQ16[n] += WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(CS_ptrQ9[n], WEBRTC_SPL_RSHIFT_W32(CorrQ11[k+1], shftVal)) + 2, 2);
  }

  for (k=0; k<FRAMESAMPLES/8; k++) {
    CurveQ16[FRAMESAMPLES_QUARTER-1 - k] = CurveQ16[k] - WEBRTC_SPL_LSHIFT_W32(diffQ16[k], shftVal);
    CurveQ16[k] += WEBRTC_SPL_LSHIFT_W32(diffQ16[k], shftVal);
  }
}

/* generate array of dither samples in Q7 */
static void GenerateDitherQ7Lb(WebRtc_Word16 *bufQ7,
                               WebRtc_UWord32 seed,
                               int length,
                               WebRtc_Word16 AvgPitchGain_Q12)
{
  int   k, shft;
  WebRtc_Word16 dither1_Q7, dither2_Q7, dither_gain_Q14;

  if (AvgPitchGain_Q12 < 614)  /* this threshold should be equal to that in decode_spec() */
  {
    for (k = 0; k < length-2; k += 3)
    {
      /* new random unsigned int */
      seed = (seed * 196314165) + 907633515;

      /* fixed-point dither sample between -64 and 64 (Q7) */
      dither1_Q7 = (WebRtc_Word16)(((int)seed + 16777216)>>25); // * 128/4294967295

      /* new random unsigned int */
      seed = (seed * 196314165) + 907633515;

      /* fixed-point dither sample between -64 and 64 */
      dither2_Q7 = (WebRtc_Word16)(((int)seed + 16777216)>>25);

      shft = (seed >> 25) & 15;
      if (shft < 5)
      {
        bufQ7[k]   = dither1_Q7;
        bufQ7[k+1] = dither2_Q7;
        bufQ7[k+2] = 0;
      }
      else if (shft < 10)
      {
        bufQ7[k]   = dither1_Q7;
        bufQ7[k+1] = 0;
        bufQ7[k+2] = dither2_Q7;
      }
      else
      {
        bufQ7[k]   = 0;
        bufQ7[k+1] = dither1_Q7;
        bufQ7[k+2] = dither2_Q7;
      }
    }
  }
  else
  {
    dither_gain_Q14 = (WebRtc_Word16)(22528 - 10 * AvgPitchGain_Q12);

    /* dither on half of the coefficients */
    for (k = 0; k < length-1; k += 2)
    {
      /* new random unsigned int */
      seed = (seed * 196314165) + 907633515;

      /* fixed-point dither sample between -64 and 64 */
      dither1_Q7 = (WebRtc_Word16)(((int)seed + 16777216)>>25);

      /* dither sample is placed in either even or odd index */
      shft = (seed >> 25) & 1;     /* either 0 or 1 */

      bufQ7[k + shft] = (((dither_gain_Q14 * dither1_Q7) + 8192)>>14);
      bufQ7[k + 1 - shft] = 0;
    }
  }
}



/******************************************************************************
 * GenerateDitherQ7LbUB()
 *
 * generate array of dither samples in Q7 There are less zeros in dither
 * vector compared to GenerateDitherQ7Lb.
 *
 * A uniform random number generator with the range of [-64 64] is employed
 * but the generated dithers are scaled by 0.35, a heuristic scaling.
 *
 * Input:
 *      -seed               : the initial seed for the random number generator.
 *      -length             : the number of dither values to be generated.
 *
 * Output:
 *      -bufQ7              : pointer to a buffer where dithers are written to.
 */
static void GenerateDitherQ7LbUB(
    WebRtc_Word16 *bufQ7,
    WebRtc_UWord32 seed,
    int length)
{
  int k;
  for (k = 0; k < length; k++) {
    /* new random unsigned int */
    seed = (seed * 196314165) + 907633515;

    /* fixed-point dither sample between -64 and 64 (Q7) */
    // * 128/4294967295
    bufQ7[k] = (WebRtc_Word16)(((int)seed + 16777216)>>25);

    // scale by 0.35
    bufQ7[k] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(bufQ7[k],
                                                           2048, 13);
  }
}



/*
 * function to decode the complex spectrum from the bit stream
 * returns the total number of bytes in the stream
 */
int WebRtcIsac_DecodeSpecLb(Bitstr *streamdata,
                            double *fr,
                            double *fi,
                            WebRtc_Word16 AvgPitchGain_Q12)
{
  WebRtc_Word16  DitherQ7[FRAMESAMPLES];
  WebRtc_Word16  data[FRAMESAMPLES];
  WebRtc_Word32  invARSpec2_Q16[FRAMESAMPLES_QUARTER];
  WebRtc_UWord16 invARSpecQ8[FRAMESAMPLES_QUARTER];
  WebRtc_Word16  ARCoefQ12[AR_ORDER+1];
  WebRtc_Word16  RCQ15[AR_ORDER];
  WebRtc_Word16  gainQ10;
  WebRtc_Word32  gain2_Q10, res;
  WebRtc_Word32  in_sqrt;
  WebRtc_Word32  newRes;
  int            k, len, i;

  /* create dither signal */
  GenerateDitherQ7Lb(DitherQ7, streamdata->W_upper, FRAMESAMPLES, AvgPitchGain_Q12);

  /* decode model parameters */
  if (WebRtcIsac_DecodeRc(streamdata, RCQ15) < 0)
    return -ISAC_RANGE_ERROR_DECODE_SPECTRUM;

  WebRtcSpl_ReflCoefToLpc(RCQ15, AR_ORDER, ARCoefQ12);

  if (WebRtcIsac_DecodeGain2(streamdata, &gain2_Q10) < 0)
    return -ISAC_RANGE_ERROR_DECODE_SPECTRUM;

  /* compute inverse AR power spectrum */
  WebRtcIsac_FindInvArSpec(ARCoefQ12, gain2_Q10, invARSpec2_Q16);

  /* convert to magnitude spectrum, by doing square-roots (modified from SPLIB) */
  res = 1 << (WebRtcSpl_GetSizeInBits(invARSpec2_Q16[0]) >> 1);
  for (k = 0; k < FRAMESAMPLES_QUARTER; k++)
  {
    in_sqrt = invARSpec2_Q16[k];
    i = 10;

    /* Negative values make no sense for a real sqrt-function. */
    if (in_sqrt<0)
      in_sqrt=-in_sqrt;

    newRes = (in_sqrt / res + res) >> 1;
    do
    {
      res = newRes;
      newRes = (in_sqrt / res + res) >> 1;
    } while (newRes != res && i-- > 0);

    invARSpecQ8[k] = (WebRtc_Word16)newRes;
  }

  /* arithmetic decoding of spectrum */
  if ((len = WebRtcIsac_DecLogisticMulti2(data, streamdata, invARSpecQ8, DitherQ7,
                                          FRAMESAMPLES, !IS_SWB_12KHZ)) <1)
    return -ISAC_RANGE_ERROR_DECODE_SPECTRUM;

  /* subtract dither and scale down spectral samples with low SNR */
  if (AvgPitchGain_Q12 <= 614)
  {
    for (k = 0; k < FRAMESAMPLES; k += 4)
    {
      gainQ10 = WebRtcSpl_DivW32W16ResW16(30 << 10,
                                              (WebRtc_Word16)((invARSpec2_Q16[k>>2] + (32768 + (33 << 16))) >> 16));
      *fr++ = (double)((data[ k ] * gainQ10 + 512) >> 10) / 128.0;
      *fi++ = (double)((data[k+1] * gainQ10 + 512) >> 10) / 128.0;
      *fr++ = (double)((data[k+2] * gainQ10 + 512) >> 10) / 128.0;
      *fi++ = (double)((data[k+3] * gainQ10 + 512) >> 10) / 128.0;
    }
  }
  else
  {
    for (k = 0; k < FRAMESAMPLES; k += 4)
    {
      gainQ10 = WebRtcSpl_DivW32W16ResW16(36 << 10,
                                              (WebRtc_Word16)((invARSpec2_Q16[k>>2] + (32768 + (40 << 16))) >> 16));
      *fr++ = (double)((data[ k ] * gainQ10 + 512) >> 10) / 128.0;
      *fi++ = (double)((data[k+1] * gainQ10 + 512) >> 10) / 128.0;
      *fr++ = (double)((data[k+2] * gainQ10 + 512) >> 10) / 128.0;
      *fi++ = (double)((data[k+3] * gainQ10 + 512) >> 10) / 128.0;
    }
  }

  return len;
}

/******************************************************************************
 * WebRtcIsac_DecodeSpecUB16()
 * Decode real and imaginary part of the DFT coefficients, given a bit-stream.
 * This function is called when the codec is in 0-16 kHz bandwidth.
 * The decoded DFT coefficient can be transformed to time domain by
 * WebRtcIsac_Time2Spec().
 *
 * Input:
 *  - streamdata            : pointer to a stucture containg the encoded
 *                            data and theparameters needed for entropy
 *                            coding.
 *
 * Output:
 *      -*fr                : pointer to a buffer where the real part of DFT
 *                            coefficients are written to.
 *      -*fi                : pointer to a buffer where the imaginary part
 *                            of DFT coefficients are written to.
 *
 * Return value             : < 0 if an error occures
 *                              0 if succeeded.
 */
int WebRtcIsac_DecodeSpecUB16(
    Bitstr*     streamdata,
    double*     fr,
    double*     fi)
{
  WebRtc_Word16  DitherQ7[FRAMESAMPLES];
  WebRtc_Word16  data[FRAMESAMPLES];
  WebRtc_Word32  invARSpec2_Q16[FRAMESAMPLES_QUARTER];
  WebRtc_UWord16 invARSpecQ8[FRAMESAMPLES_QUARTER];
  WebRtc_Word16  ARCoefQ12[AR_ORDER+1];
  WebRtc_Word16  RCQ15[AR_ORDER];
  WebRtc_Word32  gain2_Q10, res;
  WebRtc_Word32  in_sqrt;
  WebRtc_Word32  newRes;
  int            k, len, i, j;

  /* create dither signal */
  GenerateDitherQ7LbUB(DitherQ7, streamdata->W_upper, FRAMESAMPLES);

  /* decode model parameters */
  if (WebRtcIsac_DecodeRc(streamdata, RCQ15) < 0)
    return -ISAC_RANGE_ERROR_DECODE_SPECTRUM;

  WebRtcSpl_ReflCoefToLpc(RCQ15, AR_ORDER, ARCoefQ12);

  if (WebRtcIsac_DecodeGain2(streamdata, &gain2_Q10) < 0)
    return -ISAC_RANGE_ERROR_DECODE_SPECTRUM;

  /* compute inverse AR power spectrum */
  WebRtcIsac_FindInvArSpec(ARCoefQ12, gain2_Q10, invARSpec2_Q16);

  /* convert to magnitude spectrum, by doing square-roots (modified from SPLIB) */
  res = 1 << (WebRtcSpl_GetSizeInBits(invARSpec2_Q16[0]) >> 1);
  for (k = 0; k < FRAMESAMPLES_QUARTER; k++)
  {
    in_sqrt = invARSpec2_Q16[k];
    i = 10;

    /* Negative values make no sense for a real sqrt-function. */
    if (in_sqrt<0)
      in_sqrt=-in_sqrt;

    newRes = (in_sqrt / res + res) >> 1;
    do
    {
      res = newRes;
      newRes = (in_sqrt / res + res) >> 1;
    } while (newRes != res && i-- > 0);

    invARSpecQ8[k] = (WebRtc_Word16)newRes;
  }

  /* arithmetic decoding of spectrum */
  if ((len = WebRtcIsac_DecLogisticMulti2(data, streamdata, invARSpecQ8,
                                          DitherQ7, FRAMESAMPLES, !IS_SWB_12KHZ)) <1)
    return -ISAC_RANGE_ERROR_DECODE_SPECTRUM;

  /* re-arrange DFT coefficients and scale down */
  for (j = 0, k = 0; k < FRAMESAMPLES; k += 4, j++)
  {
    fr[j] = (double)data[ k ] / 128.0;
    fi[j] = (double)data[k+1] / 128.0;
    fr[(FRAMESAMPLES_HALF) - 1 - j] = (double)data[k+2] / 128.0;
    fi[(FRAMESAMPLES_HALF) - 1 - j] = (double)data[k+3] / 128.0;

  }
  return len;
}




/******************************************************************************
 * WebRtcIsac_DecodeSpecUB12()
 * Decode real and imaginary part of the DFT coefficients, given a bit-stream.
 * This function is called when the codec is in 0-12 kHz bandwidth.
 * The decoded DFT coefficient can be transformed to time domain by
 * WebRtcIsac_Time2Spec().
 *
 * Input:
 *  - streamdata            : pointer to a stucture containg the encoded
 *                            data and theparameters needed for entropy
 *                            coding.
 *
 * Output:
 *      -*fr                : pointer to a buffer where the real part of DFT
 *                            coefficients are written to.
 *      -*fi                : pointer to a buffer where the imaginary part
 *                            of DFT coefficients are written to.
 *
 * Return value             : < 0 if an error occures
 *                              0 if succeeded.
 */
int WebRtcIsac_DecodeSpecUB12(
    Bitstr *streamdata,
    double *fr,
    double *fi)
{
  WebRtc_Word16  DitherQ7[FRAMESAMPLES];
  WebRtc_Word16  data[FRAMESAMPLES];
  WebRtc_Word32  invARSpec2_Q16[FRAMESAMPLES_QUARTER];
  WebRtc_UWord16 invARSpecQ8[FRAMESAMPLES_QUARTER];
  WebRtc_Word16  ARCoefQ12[AR_ORDER+1];
  WebRtc_Word16  RCQ15[AR_ORDER];
  WebRtc_Word32  gain2_Q10;
  WebRtc_Word32  res;
  WebRtc_Word32  in_sqrt;
  WebRtc_Word32  newRes;
  int            k, len, i;

  /* create dither signal */
  GenerateDitherQ7LbUB(DitherQ7, streamdata->W_upper, FRAMESAMPLES);

  /* decode model parameters */
  if (WebRtcIsac_DecodeRc(streamdata, RCQ15) < 0)
    return -ISAC_RANGE_ERROR_DECODE_SPECTRUM;

  WebRtcSpl_ReflCoefToLpc(RCQ15, AR_ORDER, ARCoefQ12);

  if (WebRtcIsac_DecodeGain2(streamdata, &gain2_Q10) < 0)
    return -ISAC_RANGE_ERROR_DECODE_SPECTRUM;


  /* compute inverse AR power spectrum */
  WebRtcIsac_FindInvArSpec(ARCoefQ12, gain2_Q10, invARSpec2_Q16);

  /* convert to magnitude spectrum, by doing square-roots (modified from SPLIB) */
  res = 1 << (WebRtcSpl_GetSizeInBits(invARSpec2_Q16[0]) >> 1);
  for (k = 0; k < FRAMESAMPLES_QUARTER; k++)
  {
    in_sqrt = invARSpec2_Q16[k];
    i = 10;

    /* Negative values make no sense for a real sqrt-function. */
    if (in_sqrt<0)
      in_sqrt=-in_sqrt;

    newRes = (in_sqrt / res + res) >> 1;
    do
    {
      res = newRes;
      newRes = (in_sqrt / res + res) >> 1;
    } while (newRes != res && i-- > 0);

    invARSpecQ8[k] = (WebRtc_Word16)newRes;
  }

  /* arithmetic decoding of spectrum */
  if ((len = WebRtcIsac_DecLogisticMulti2(data, streamdata,
                                          invARSpecQ8, DitherQ7, (FRAMESAMPLES_HALF), IS_SWB_12KHZ)) < 1)
  {
    return -ISAC_RANGE_ERROR_DECODE_SPECTRUM;
  }

  for (k = 0, i = 0; k < FRAMESAMPLES_HALF; k += 4)
  {
    fr[i] = (double)data[ k ] / 128.0;
    fi[i] = (double)data[k+1] / 128.0;
    i++;
    fr[i] = (double)data[k+2] / 128.0;
    fi[i] = (double)data[k+3] / 128.0;
    i++;
  }

  // The second half of real and imaginary coefficients is zero. This is
  // due to using the old FFT module which requires two signals as input
  // while in 0-12 kHz mode we only have 8-12 kHz band, and the second signal
  // is set to zero
  memset(&fr[FRAMESAMPLES_QUARTER], 0, FRAMESAMPLES_QUARTER * sizeof(double));
  memset(&fi[FRAMESAMPLES_QUARTER], 0, FRAMESAMPLES_QUARTER * sizeof(double));

  return len;
}





int WebRtcIsac_EncodeSpecLb(const WebRtc_Word16 *fr,
                            const WebRtc_Word16 *fi,
                            Bitstr *streamdata,
                            WebRtc_Word16 AvgPitchGain_Q12)
{
  WebRtc_Word16  ditherQ7[FRAMESAMPLES];
  WebRtc_Word16  dataQ7[FRAMESAMPLES];
  WebRtc_Word32  PSpec[FRAMESAMPLES_QUARTER];
  WebRtc_Word32  invARSpec2_Q16[FRAMESAMPLES_QUARTER];
  WebRtc_UWord16 invARSpecQ8[FRAMESAMPLES_QUARTER];
  WebRtc_Word32  CorrQ7[AR_ORDER+1];
  WebRtc_Word32  CorrQ7_norm[AR_ORDER+1];
  WebRtc_Word16  RCQ15[AR_ORDER];
  WebRtc_Word16  ARCoefQ12[AR_ORDER+1];
  WebRtc_Word32  gain2_Q10;
  WebRtc_Word16  val;
  WebRtc_Word32  nrg, res;
  WebRtc_UWord32 sum;
  WebRtc_Word32  in_sqrt;
  WebRtc_Word32  newRes;
  WebRtc_Word16  err;
  WebRtc_UWord32  nrg_u32;
  int            shift_var;
  int          k, n, j, i;


  /* create dither_float signal */
  GenerateDitherQ7Lb(ditherQ7, streamdata->W_upper, FRAMESAMPLES, AvgPitchGain_Q12);

  /* add dither and quantize, and compute power spectrum */
  for (k = 0; k < FRAMESAMPLES; k += 4)
  {
    val = ((*fr++ + ditherQ7[k]   + 64) & 0xFF80) - ditherQ7[k];
    dataQ7[k] = val;
    sum = val * val;

    val = ((*fi++ + ditherQ7[k+1] + 64) & 0xFF80) - ditherQ7[k+1];
    dataQ7[k+1] = val;
    sum += val * val;

    val = ((*fr++ + ditherQ7[k+2] + 64) & 0xFF80) - ditherQ7[k+2];
    dataQ7[k+2] = val;
    sum += val * val;

    val = ((*fi++ + ditherQ7[k+3] + 64) & 0xFF80) - ditherQ7[k+3];
    dataQ7[k+3] = val;
    sum += val * val;

    PSpec[k>>2] = sum >> 2;
  }

  /* compute correlation from power spectrum */
  WebRtcIsac_FindCorrelation(PSpec, CorrQ7);


  /* find AR coefficients */
  /* number of bit shifts to 14-bit normalize CorrQ7[0] (leaving room for sign) */
  shift_var = WebRtcSpl_NormW32(CorrQ7[0]) - 18;

  if (shift_var > 0) {
    for (k=0; k<AR_ORDER+1; k++) {
      CorrQ7_norm[k] = CorrQ7[k] << shift_var;
    }
  } else {
    for (k=0; k<AR_ORDER+1; k++) {
      CorrQ7_norm[k] = CorrQ7[k] >> (-shift_var);
    }
  }

  /* find RC coefficients */
  WebRtcSpl_AutoCorrToReflCoef(CorrQ7_norm, AR_ORDER, RCQ15);

  /* quantize & code RC Coefficient */
  WebRtcIsac_EncodeRc(RCQ15, streamdata);

  /* RC -> AR coefficients */
  WebRtcSpl_ReflCoefToLpc(RCQ15, AR_ORDER, ARCoefQ12);

  /* compute ARCoef' * Corr * ARCoef in Q19 */
  nrg = 0;
  for (j = 0; j <= AR_ORDER; j++) {
    for (n = 0; n <= j; n++) {
      nrg += ( ARCoefQ12[j] * ((CorrQ7_norm[j-n] * ARCoefQ12[n] + 256) >> 9) + 4 ) >> 3;
    }
    for (n = j+1; n <= AR_ORDER; n++) {
      nrg += ( ARCoefQ12[j] * ((CorrQ7_norm[n-j] * ARCoefQ12[n] + 256) >> 9) + 4 ) >> 3;
    }
  }

  nrg_u32 = (WebRtc_UWord32)nrg;
  if (shift_var > 0) {
    nrg_u32 = nrg_u32 >> shift_var;
  } else {
    nrg_u32 = nrg_u32 << (-shift_var);
  }

  if (nrg_u32 > 0x7FFFFFFF)
    nrg = 0x7FFFFFFF;
  else
    nrg = (WebRtc_Word32)nrg_u32;

  gain2_Q10 = WebRtcSpl_DivResultInQ31(FRAMESAMPLES_QUARTER, nrg);  /* also shifts 31 bits to the left! */

  /* quantize & code gain2_Q10 */
  if (WebRtcIsac_EncodeGain2(&gain2_Q10, streamdata)) {
    return -1;
  }

  /* compute inverse AR power spectrum */
  WebRtcIsac_FindInvArSpec(ARCoefQ12, gain2_Q10, invARSpec2_Q16);

  /* convert to magnitude spectrum, by doing square-roots (modified from SPLIB) */
  res = 1 << (WebRtcSpl_GetSizeInBits(invARSpec2_Q16[0]) >> 1);
  for (k = 0; k < FRAMESAMPLES_QUARTER; k++)
  {
    in_sqrt = invARSpec2_Q16[k];
    i = 10;

    /* Negative values make no sense for a real sqrt-function. */
    if (in_sqrt<0)
      in_sqrt=-in_sqrt;

    newRes = (in_sqrt / res + res) >> 1;
    do
    {
      res = newRes;
      newRes = (in_sqrt / res + res) >> 1;
    } while (newRes != res && i-- > 0);

    invARSpecQ8[k] = (WebRtc_Word16)newRes;
  }

  /* arithmetic coding of spectrum */
  err = WebRtcIsac_EncLogisticMulti2(streamdata, dataQ7, invARSpecQ8,
                                     FRAMESAMPLES, !IS_SWB_12KHZ);
  if (err < 0)
  {
    return (err);
  }

  return 0;
}


/******************************************************************************
 * WebRtcIsac_EncodeSpecUB16()
 * Quantize and encode real and imaginary part of the DFT coefficients.
 * This function is called when the codec is in 0-16 kHz bandwidth.
 * The real and imaginary part are computed by calling WebRtcIsac_Time2Spec().
 *
 *
 * Input:
 *      -*fr                : pointer to a buffer where the real part of DFT
 *                            coefficients are stored.
 *      -*fi                : pointer to a buffer where the imaginary part
 *                            of DFT coefficients are stored.
 *
 * Output:
 *  - streamdata            : pointer to a stucture containg the encoded
 *                            data and theparameters needed for entropy
 *                            coding.
 *
 * Return value             : < 0 if an error occures
 *                              0 if succeeded.
 */
int WebRtcIsac_EncodeSpecUB16(
    const WebRtc_Word16* fr,
    const WebRtc_Word16* fi,
    Bitstr*            streamdata)
{
  WebRtc_Word16  ditherQ7[FRAMESAMPLES];
  WebRtc_Word16  dataQ7[FRAMESAMPLES];
  WebRtc_Word32  PSpec[FRAMESAMPLES_QUARTER];
  WebRtc_Word32  invARSpec2_Q16[FRAMESAMPLES_QUARTER];
  WebRtc_UWord16 invARSpecQ8[FRAMESAMPLES_QUARTER];
  WebRtc_Word32  CorrQ7[AR_ORDER+1];
  WebRtc_Word32  CorrQ7_norm[AR_ORDER+1];
  WebRtc_Word16  RCQ15[AR_ORDER];
  WebRtc_Word16  ARCoefQ12[AR_ORDER+1];
  WebRtc_Word32  gain2_Q10;
  WebRtc_Word16  val;
  WebRtc_Word32  nrg, res;
  WebRtc_UWord32 sum;
  WebRtc_Word32  in_sqrt;
  WebRtc_Word32  newRes;
  WebRtc_Word16  err;
  WebRtc_UWord32 nrg_u32;
  int            shift_var;
  int          k, n, j, i;

  /* create dither_float signal */
  GenerateDitherQ7LbUB(ditherQ7, streamdata->W_upper, FRAMESAMPLES);

  /* add dither and quantize, and compute power spectrum */
  for (j = 0, k = 0; k < FRAMESAMPLES; k += 4, j++)
  {
    val = ((fr[j] + ditherQ7[k]   + 64) & 0xFF80) - ditherQ7[k];
    dataQ7[k] = val;
    sum = val * val;

    val = ((fi[j] + ditherQ7[k+1] + 64) & 0xFF80) - ditherQ7[k+1];
    dataQ7[k+1] = val;
    sum += val * val;

    val = ((fr[(FRAMESAMPLES_HALF) - 1 - j] + ditherQ7[k+2] + 64) &
           0xFF80) - ditherQ7[k+2];
    dataQ7[k+2] = val;
    sum += val * val;

    val = ((fi[(FRAMESAMPLES_HALF) - 1 - j] + ditherQ7[k+3] + 64) &
           0xFF80) - ditherQ7[k+3];
    dataQ7[k+3] = val;
    sum += val * val;

    PSpec[k>>2] = sum >> 2;
  }

  /* compute correlation from power spectrum */
  WebRtcIsac_FindCorrelation(PSpec, CorrQ7);


  /* find AR coefficients
     number of bit shifts to 14-bit normalize CorrQ7[0]
     (leaving room for sign) */
  shift_var = WebRtcSpl_NormW32(CorrQ7[0]) - 18;

  if (shift_var > 0) {
    for (k=0; k<AR_ORDER+1; k++) {
      CorrQ7_norm[k] = CorrQ7[k] << shift_var;
    }
  } else {
    for (k=0; k<AR_ORDER+1; k++) {
      CorrQ7_norm[k] = CorrQ7[k] >> (-shift_var);
    }
  }

  /* find RC coefficients */
  WebRtcSpl_AutoCorrToReflCoef(CorrQ7_norm, AR_ORDER, RCQ15);

  /* quantize & code RC Coef */
  WebRtcIsac_EncodeRc(RCQ15, streamdata);

  /* RC -> AR coefficients */
  WebRtcSpl_ReflCoefToLpc(RCQ15, AR_ORDER, ARCoefQ12);

  /* compute ARCoef' * Corr * ARCoef in Q19 */
  nrg = 0;
  for (j = 0; j <= AR_ORDER; j++) {
    for (n = 0; n <= j; n++) {
      nrg += ( ARCoefQ12[j] * ((CorrQ7_norm[j-n] * ARCoefQ12[n] +
                                256) >> 9) + 4 ) >> 3;
    }
    for (n = j+1; n <= AR_ORDER; n++) {
      nrg += ( ARCoefQ12[j] * ((CorrQ7_norm[n-j] * ARCoefQ12[n] +
                                256) >> 9) + 4 ) >> 3;
    }
  }
  nrg_u32 = (WebRtc_UWord32)nrg;
  if (shift_var > 0) {
    nrg_u32 = nrg_u32 >> shift_var;
  } else {
    nrg_u32 = nrg_u32 << (-shift_var);
  }

  if (nrg_u32 > 0x7FFFFFFF)
    nrg = 0x7FFFFFFF;
  else
    nrg = (WebRtc_Word32)nrg_u32;

  gain2_Q10 = WebRtcSpl_DivResultInQ31(FRAMESAMPLES_QUARTER, nrg);  /* also shifts 31 bits to the left! */

  /* quantize & code gain2_Q10 */
  if (WebRtcIsac_EncodeGain2(&gain2_Q10, streamdata)) {
    return -1;
  }

  /* compute inverse AR power spectrum */
  WebRtcIsac_FindInvArSpec(ARCoefQ12, gain2_Q10, invARSpec2_Q16);

  /* convert to magnitude spectrum, by doing square-roots (modified from SPLIB) */
  res = 1 << (WebRtcSpl_GetSizeInBits(invARSpec2_Q16[0]) >> 1);
  for (k = 0; k < FRAMESAMPLES_QUARTER; k++)
  {
    in_sqrt = invARSpec2_Q16[k];
    i = 10;

    /* Negative values make no sense for a real sqrt-function. */
    if (in_sqrt<0)
      in_sqrt=-in_sqrt;

    newRes = (in_sqrt / res + res) >> 1;
    do
    {
      res = newRes;
      newRes = (in_sqrt / res + res) >> 1;
    } while (newRes != res && i-- > 0);

    invARSpecQ8[k] = (WebRtc_Word16)newRes;
  }

  /* arithmetic coding of spectrum */
  err = WebRtcIsac_EncLogisticMulti2(streamdata, dataQ7, invARSpecQ8,
                                     FRAMESAMPLES, !IS_SWB_12KHZ);
  if (err < 0)
  {
    return (err);
  }

  return 0;
}




int WebRtcIsac_EncodeSpecUB12(const WebRtc_Word16 *fr,
                                const WebRtc_Word16 *fi,
                                Bitstr *streamdata)
{
  WebRtc_Word16  ditherQ7[FRAMESAMPLES];
  WebRtc_Word16  dataQ7[FRAMESAMPLES];
  WebRtc_Word32  PSpec[FRAMESAMPLES_QUARTER];
  WebRtc_Word32  invARSpec2_Q16[FRAMESAMPLES_QUARTER];
  WebRtc_UWord16 invARSpecQ8[FRAMESAMPLES_QUARTER];
  WebRtc_Word32  CorrQ7[AR_ORDER+1];
  WebRtc_Word32  CorrQ7_norm[AR_ORDER+1];
  WebRtc_Word16  RCQ15[AR_ORDER];
  WebRtc_Word16  ARCoefQ12[AR_ORDER+1];
  WebRtc_Word32  gain2_Q10;
  WebRtc_Word16  val;
  WebRtc_Word32  nrg, res;
  WebRtc_UWord32 sum;
  WebRtc_Word32  in_sqrt;
  WebRtc_Word32  newRes;
  WebRtc_Word16  err;
  int            shift_var;
  int          k, n, j, i;
  WebRtc_UWord32 nrg_u32;

  /* create dither_float signal */
  GenerateDitherQ7LbUB(ditherQ7, streamdata->W_upper, FRAMESAMPLES);

  /* add dither and quantize, and compute power spectrum */
  for (k = 0, j = 0; k < (FRAMESAMPLES_HALF); k += 4)
  {
    val = ((*fr++ + ditherQ7[k]   + 64) & 0xFF80) - ditherQ7[k];
    dataQ7[k] = val;
    sum = (val) * (val);

    val = ((*fi++ + ditherQ7[k+1] + 64) & 0xFF80) - ditherQ7[k+1];
    dataQ7[k+1] = val;
    sum += (val) * (val);

    if(j < FRAMESAMPLES_QUARTER)
    {
      PSpec[j] = sum >> 1;
      j++;
    }

    val = ((*fr++ + ditherQ7[k+2] + 64) & 0xFF80) - ditherQ7[k+2];
    dataQ7[k+2] = val;
    sum = (val) * (val);

    val = ((*fi++ + ditherQ7[k+3] + 64) & 0xFF80) - ditherQ7[k+3];
    dataQ7[k+3] = val;
    sum += (val) * (val);

    if(j < FRAMESAMPLES_QUARTER)
    {
      PSpec[j] = sum >> 1;
      j++;
    }
  }
  /* compute correlation from power spectrum */
  WebRtcIsac_FindCorrelation(PSpec, CorrQ7);


  /* find AR coefficients */
  /* number of bit shifts to 14-bit normalize CorrQ7[0] (leaving room for sign) */
  shift_var = WebRtcSpl_NormW32(CorrQ7[0]) - 18;

  if (shift_var > 0) {
    for (k=0; k<AR_ORDER+1; k++) {
      CorrQ7_norm[k] = CorrQ7[k] << shift_var;
    }
  } else {
    for (k=0; k<AR_ORDER+1; k++) {
      CorrQ7_norm[k] = CorrQ7[k] >> (-shift_var);
    }
  }

  /* find RC coefficients */
  WebRtcSpl_AutoCorrToReflCoef(CorrQ7_norm, AR_ORDER, RCQ15);

  /* quantize & code RC Coef */
  WebRtcIsac_EncodeRc(RCQ15, streamdata);


  /* RC -> AR coefficients */
  WebRtcSpl_ReflCoefToLpc(RCQ15, AR_ORDER, ARCoefQ12);


  /* compute ARCoef' * Corr * ARCoef in Q19 */
  nrg = 0;
  for (j = 0; j <= AR_ORDER; j++) {
    for (n = 0; n <= j; n++) {
      nrg += ( ARCoefQ12[j] * ((CorrQ7_norm[j-n] * ARCoefQ12[n] + 256) >> 9) + 4 ) >> 3;
    }
    for (n = j+1; n <= AR_ORDER; n++) {
      nrg += ( ARCoefQ12[j] * ((CorrQ7_norm[n-j] * ARCoefQ12[n] + 256) >> 9) + 4 ) >> 3;
    }
  }

  nrg_u32 = (WebRtc_UWord32)nrg;
  if (shift_var > 0) {
    nrg_u32 = nrg_u32 >> shift_var;
  } else {
    nrg_u32 = nrg_u32 << (-shift_var);
  }

  if (nrg_u32 > 0x7FFFFFFF) {
    nrg = 0x7FFFFFFF;
  } else {
    nrg = (WebRtc_Word32)nrg_u32;
  }

  gain2_Q10 = WebRtcSpl_DivResultInQ31(FRAMESAMPLES_QUARTER, nrg);  /* also shifts 31 bits to the left! */

  /* quantize & code gain2_Q10 */
  if (WebRtcIsac_EncodeGain2(&gain2_Q10, streamdata)) {
    return -1;
  }

  /* compute inverse AR power spectrum */
  WebRtcIsac_FindInvArSpec(ARCoefQ12, gain2_Q10, invARSpec2_Q16);

  /* convert to magnitude spectrum, by doing square-roots (modified from SPLIB) */
  res = 1 << (WebRtcSpl_GetSizeInBits(invARSpec2_Q16[0]) >> 1);
  for (k = 0; k < FRAMESAMPLES_QUARTER; k++)
  {
    in_sqrt = invARSpec2_Q16[k];
    i = 10;

    /* Negative values make no sense for a real sqrt-function. */
    if (in_sqrt<0)
      in_sqrt=-in_sqrt;

    newRes = (in_sqrt / res + res) >> 1;
    do
    {
      res = newRes;
      newRes = (in_sqrt / res + res) >> 1;
    } while (newRes != res && i-- > 0);

    invARSpecQ8[k] = (WebRtc_Word16)newRes;
  }

  /* arithmetic coding of spectrum */
  err = WebRtcIsac_EncLogisticMulti2(streamdata, dataQ7, invARSpecQ8,
                                     (FRAMESAMPLES_HALF), IS_SWB_12KHZ);
  if (err < 0)
  {
    return (err);
  }

  return 0;
}



/* step-up */
void WebRtcIsac_Rc2Poly(double *RC, int N, double *a)
{
  int m, k;
  double tmp[MAX_AR_MODEL_ORDER];

  a[0] = 1.0;
  tmp[0] = 1.0;
  for (m=1; m<=N; m++) {
    /* copy */
    for (k=1; k<m; k++)
      tmp[k] = a[k];

    a[m] = RC[m-1];
    for (k=1; k<m; k++)
      a[k] += RC[m-1] * tmp[m-k];
  }
  return;
}

/* step-down */
void WebRtcIsac_Poly2Rc(double *a, int N, double *RC)
{
  int m, k;
  double tmp[MAX_AR_MODEL_ORDER];
  double tmp_inv;

  RC[N-1] = a[N];
  for (m=N-1; m>0; m--) {
    tmp_inv = 1.0 / (1.0 - RC[m]*RC[m]);
    for (k=1; k<=m; k++)
      tmp[k] = (a[k] - RC[m] * a[m-k+1]) * tmp_inv;

    for (k=1; k<m; k++)
      a[k] = tmp[k];

    RC[m-1] = tmp[m];
  }
  return;
}


#define MAX_ORDER 100


void WebRtcIsac_Rc2Lar(const double *refc, double *lar, int order) {  /* Matlab's LAR definition */

  int k;

  for (k = 0; k < order; k++) {
    lar[k] = log((1 + refc[k]) / (1 - refc[k]));
  }

}


void WebRtcIsac_Lar2Rc(const double *lar, double *refc,  int order) {

  int k;
  double tmp;

  for (k = 0; k < order; k++) {
    tmp = exp(lar[k]);
    refc[k] = (tmp - 1) / (tmp + 1);
  }
}

void WebRtcIsac_Poly2Lar(double *lowband, int orderLo, double *hiband, int orderHi, int Nsub, double *lars) {

  int k, n, orderTot;
  double poly[MAX_ORDER], lar[MAX_ORDER], rc[MAX_ORDER], *inpl, *inph, *outp;

  orderTot = (orderLo + orderHi + 2);
  inpl = lowband;
  inph = hiband;
  outp = lars;
  poly[0] = 1.0;
  for (k = 0; k < Nsub; k++) {
    /* gains */
    outp[0] = inpl[0];
    outp[1] = inph[0];

    /* Low band */
    for (n = 1; n <= orderLo; n++)
      poly[n] = inpl[n];
    WebRtcIsac_Poly2Rc(poly, orderLo, rc);
    WebRtcIsac_Rc2Lar(rc, lar, orderLo);
    for (n = 0; n < orderLo; n++)
      outp[n + 2] = lar[n];

    /* High band */
    for (n = 1; n <= orderHi; n++)
      poly[n] = inph[n];
    WebRtcIsac_Poly2Rc(poly, orderHi, rc);
    WebRtcIsac_Rc2Lar(rc, lar, orderHi);
    for (n = 0; n < orderHi; n++)
      outp[n + orderLo + 2] = lar[n];

    inpl += orderLo + 1;
    inph += orderHi + 1;
    outp += orderTot;
  }
}



WebRtc_Word16
WebRtcIsac_Poly2LarUB(
    double* lpcVecs,
    WebRtc_Word16 bandwidth)
{
  double      poly[MAX_ORDER];
  double      rc[MAX_ORDER];
  double*     ptrIO;
  WebRtc_Word16 vecCntr;
  WebRtc_Word16 vecSize;
  WebRtc_Word16 numVec;

  vecSize = UB_LPC_ORDER;
  switch(bandwidth)
  {
    case isac12kHz:
      {
        numVec  = UB_LPC_VEC_PER_FRAME;
        break;
      }
    case isac16kHz:
      {
        numVec  = UB16_LPC_VEC_PER_FRAME;
        break;
      }
    default:
      return -1;
  }

  ptrIO = lpcVecs;
  poly[0] = 1.0;
  for(vecCntr = 0; vecCntr < numVec; vecCntr++)
  {
    memcpy(&poly[1], ptrIO, sizeof(double) * vecSize);
    WebRtcIsac_Poly2Rc(poly, vecSize, rc);
    WebRtcIsac_Rc2Lar(rc, ptrIO, vecSize);
    ptrIO += vecSize;
  }
  return 0;
}



void WebRtcIsac_Lar2Poly(double *lars, double *lowband, int orderLo, double *hiband, int orderHi, int Nsub) {

  int k, n, orderTot;
  double poly[MAX_ORDER], lar[MAX_ORDER], rc[MAX_ORDER], *outpl, *outph, *inp;

  orderTot = (orderLo + orderHi + 2);
  outpl = lowband;
  outph = hiband;
  inp = lars;
  for (k = 0; k < Nsub; k++) {
    /* gains */
    outpl[0] = inp[0];
    outph[0] = inp[1];

    /* Low band */
    for (n = 0; n < orderLo; n++)
      lar[n] = inp[n + 2];
    WebRtcIsac_Lar2Rc(lar, rc, orderLo);
    WebRtcIsac_Rc2Poly(rc, orderLo, poly);
    for (n = 1; n <= orderLo; n++)
      outpl[n] = poly[n];

    /* High band */
    for (n = 0; n < orderHi; n++)
      lar[n] = inp[n + orderLo + 2];
    WebRtcIsac_Lar2Rc(lar, rc, orderHi);
    WebRtcIsac_Rc2Poly(rc, orderHi, poly);
    for (n = 1; n <= orderHi; n++)
      outph[n] = poly[n];

    outpl += orderLo + 1;
    outph += orderHi + 1;
    inp += orderTot;
  }
}

// assumes 2 LAR vectors interpolates to 'numPolyVec' A-polynomials
void
WebRtcIsac_Lar2PolyInterpolUB(
    double* larVecs,
    double* percepFilterParams,
    int     numPolyVecs) // includes the first and the last point of the interval
{

  int polyCntr, coeffCntr;
  double larInterpol[UB_LPC_ORDER];
  double rc[UB_LPC_ORDER];
  double delta[UB_LPC_ORDER];

  // calculate the step-size for linear interpolation coefficients
  for(coeffCntr = 0; coeffCntr < UB_LPC_ORDER; coeffCntr++)
  {
    delta[coeffCntr] = (larVecs[UB_LPC_ORDER + coeffCntr] -
                        larVecs[coeffCntr]) / (numPolyVecs - 1);
  }

  for(polyCntr = 0; polyCntr < numPolyVecs; polyCntr++)
  {
    for(coeffCntr = 0; coeffCntr < UB_LPC_ORDER; coeffCntr++)
    {
      larInterpol[coeffCntr] = larVecs[coeffCntr] +
          delta[coeffCntr] * polyCntr;
    }
    WebRtcIsac_Lar2Rc(larInterpol, rc, UB_LPC_ORDER);

    // convert to A-polynomial, the following function returns A[0] = 1;
    // which is written where gains had to be written. Then we write the
    // gain (outside this function). This way we say a memcpy
    WebRtcIsac_Rc2Poly(rc, UB_LPC_ORDER, percepFilterParams);
    percepFilterParams += (UB_LPC_ORDER + 1);
  }
}

int WebRtcIsac_DecodeLpc(Bitstr *streamdata, double *LPCCoef_lo, double *LPCCoef_hi, int *outmodel) {

  double lars[KLT_ORDER_GAIN + KLT_ORDER_SHAPE];
  int err;

  err = WebRtcIsac_DecodeLpcCoef(streamdata, lars, outmodel);
  if (err<0)  // error check
    return -ISAC_RANGE_ERROR_DECODE_LPC;

  WebRtcIsac_Lar2Poly(lars, LPCCoef_lo, ORDERLO, LPCCoef_hi, ORDERHI, SUBFRAMES);

  return 0;
}

WebRtc_Word16
WebRtcIsac_DecodeInterpolLpcUb(
    Bitstr*     streamdata,
    double*     percepFilterParams,
    WebRtc_Word16 bandwidth)
{

  double lpcCoeff[UB_LPC_ORDER * UB16_LPC_VEC_PER_FRAME];
  int err;
  int interpolCntr;
  int subframeCntr;
  WebRtc_Word16 numSegments;
  WebRtc_Word16 numVecPerSegment;
  WebRtc_Word16 numGains;

  double percepFilterGains[SUBFRAMES<<1];
  double* ptrOutParam = percepFilterParams;

  err = WebRtcIsac_DecodeLpcCoefUB(streamdata, lpcCoeff, percepFilterGains,
                                   bandwidth);

  // error check
  if (err<0)
  {
    return -ISAC_RANGE_ERROR_DECODE_LPC;
  }

  switch(bandwidth)
  {
    case isac12kHz:
      {
        numGains = SUBFRAMES;
        numSegments = UB_LPC_VEC_PER_FRAME - 1;
        numVecPerSegment = kLpcVecPerSegmentUb12;
        break;
      }
    case isac16kHz:
      {
        numGains = SUBFRAMES << 1;
        numSegments = UB16_LPC_VEC_PER_FRAME - 1;
        numVecPerSegment = kLpcVecPerSegmentUb16;
        break;
      }
    default:
      return -1;
  }



  for(interpolCntr = 0; interpolCntr < numSegments; interpolCntr++)
  {
    WebRtcIsac_Lar2PolyInterpolUB(
        &lpcCoeff[interpolCntr * UB_LPC_ORDER], ptrOutParam,
        numVecPerSegment + 1);

    ptrOutParam += ((numVecPerSegment) *
                    (UB_LPC_ORDER + 1));
  }

  ptrOutParam = percepFilterParams;

  if(bandwidth == isac16kHz)
  {
    ptrOutParam += (1 + UB_LPC_ORDER);
  }

  for(subframeCntr = 0; subframeCntr < numGains; subframeCntr++)
  {
    *ptrOutParam = percepFilterGains[subframeCntr];
    ptrOutParam += (1 + UB_LPC_ORDER);
  }

  return 0;
}


/* decode & dequantize LPC Coef */
int WebRtcIsac_DecodeLpcCoef(Bitstr *streamdata, double *LPCCoef, int *outmodel)
{
  int j, k, n, model, pos, pos2, posg, poss, offsg, offss, offs2;
  int index_g[KLT_ORDER_GAIN], index_s[KLT_ORDER_SHAPE];
  double tmpcoeffs_g[KLT_ORDER_GAIN],tmpcoeffs_s[KLT_ORDER_SHAPE];
  double tmpcoeffs2_g[KLT_ORDER_GAIN], tmpcoeffs2_s[KLT_ORDER_SHAPE];
  double sum;
  int err;


  /* entropy decoding of model number */
  err = WebRtcIsac_DecHistOneStepMulti(&model, streamdata, WebRtcIsac_kQKltModelCdfPtr, WebRtcIsac_kQKltModelInitIndex, 1);
  if (err<0)  // error check
    return err;

  /* entropy decoding of quantization indices */
  err = WebRtcIsac_DecHistOneStepMulti(index_s, streamdata, WebRtcIsac_kQKltCdfPtrShape[model], WebRtcIsac_kQKltInitIndexShape[model], KLT_ORDER_SHAPE);
  if (err<0)  // error check
    return err;
  err = WebRtcIsac_DecHistOneStepMulti(index_g, streamdata, WebRtcIsac_kQKltCdfPtrGain[model], WebRtcIsac_kQKltInitIndexGain[model], KLT_ORDER_GAIN);
  if (err<0)  // error check
    return err;


  /* find quantization levels for coefficients */
  for (k=0; k<KLT_ORDER_SHAPE; k++) {
    tmpcoeffs_s[WebRtcIsac_kQKltSelIndShape[k]] = WebRtcIsac_kQKltLevelsShape[WebRtcIsac_kQKltOfLevelsShape[model]+WebRtcIsac_kQKltOffsetShape[model][k] + index_s[k]];
  }
  for (k=0; k<KLT_ORDER_GAIN; k++) {
    tmpcoeffs_g[WebRtcIsac_kQKltSelIndGain[k]] = WebRtcIsac_kQKltLevelsGain[WebRtcIsac_kQKltOfLevelsGain[model]+ WebRtcIsac_kQKltOffsetGain[model][k] + index_g[k]];
  }


  /* inverse KLT  */

  /* left transform */  // Transpose matrix!
  offsg = 0;
  offss = 0;
  posg = 0;
  poss = 0;
  for (j=0; j<SUBFRAMES; j++) {
    offs2 = 0;
    for (k=0; k<LPC_GAIN_ORDER; k++) {
      sum = 0;
      pos = offsg;
      pos2 = offs2;
      for (n=0; n<LPC_GAIN_ORDER; n++)
        sum += tmpcoeffs_g[pos++] * WebRtcIsac_kKltT1Gain[model][pos2++];
      tmpcoeffs2_g[posg++] = sum;
      offs2 += LPC_GAIN_ORDER;
    }
    offs2 = 0;
    for (k=0; k<LPC_SHAPE_ORDER; k++) {
      sum = 0;
      pos = offss;
      pos2 = offs2;
      for (n=0; n<LPC_SHAPE_ORDER; n++)
        sum += tmpcoeffs_s[pos++] * WebRtcIsac_kKltT1Shape[model][pos2++];
      tmpcoeffs2_s[poss++] = sum;
      offs2 += LPC_SHAPE_ORDER;
    }
    offsg += LPC_GAIN_ORDER;
    offss += LPC_SHAPE_ORDER;
  }


  /* right transform */ // Transpose matrix
  offsg = 0;
  offss = 0;
  posg = 0;
  poss = 0;
  for (j=0; j<SUBFRAMES; j++) {
    posg = offsg;
    for (k=0; k<LPC_GAIN_ORDER; k++) {
      sum = 0;
      pos = k;
      pos2 = j;
      for (n=0; n<SUBFRAMES; n++) {
        sum += tmpcoeffs2_g[pos] * WebRtcIsac_kKltT2Gain[model][pos2];
        pos += LPC_GAIN_ORDER;
        pos2 += SUBFRAMES;

      }
      tmpcoeffs_g[posg++] = sum;
    }
    poss = offss;
    for (k=0; k<LPC_SHAPE_ORDER; k++) {
      sum = 0;
      pos = k;
      pos2 = j;
      for (n=0; n<SUBFRAMES; n++) {
        sum += tmpcoeffs2_s[pos] * WebRtcIsac_kKltT2Shape[model][pos2];
        pos += LPC_SHAPE_ORDER;
        pos2 += SUBFRAMES;
      }
      tmpcoeffs_s[poss++] = sum;
    }
    offsg += LPC_GAIN_ORDER;
    offss += LPC_SHAPE_ORDER;
  }


  /* scaling, mean addition, and gain restoration */
  posg = 0;poss = 0;pos=0;
  for (k=0; k<SUBFRAMES; k++) {

    /* log gains */
    LPCCoef[pos] = tmpcoeffs_g[posg] / LPC_GAIN_SCALE;
    LPCCoef[pos] += WebRtcIsac_kLpcMeansGain[model][posg];
    LPCCoef[pos] = exp(LPCCoef[pos]);
    pos++;posg++;
    LPCCoef[pos] = tmpcoeffs_g[posg] / LPC_GAIN_SCALE;
    LPCCoef[pos] += WebRtcIsac_kLpcMeansGain[model][posg];
    LPCCoef[pos] = exp(LPCCoef[pos]);
    pos++;posg++;

    /* lo band LAR coeffs */
    for (n=0; n<LPC_LOBAND_ORDER; n++, pos++, poss++) {
      LPCCoef[pos] = tmpcoeffs_s[poss] / LPC_LOBAND_SCALE;
      LPCCoef[pos] += WebRtcIsac_kLpcMeansShape[model][poss];
    }

    /* hi band LAR coeffs */
    for (n=0; n<LPC_HIBAND_ORDER; n++, pos++, poss++) {
      LPCCoef[pos] = tmpcoeffs_s[poss] / LPC_HIBAND_SCALE;
      LPCCoef[pos] += WebRtcIsac_kLpcMeansShape[model][poss];
    }
  }


  *outmodel=model;

  return 0;
}

/* estimate codel length of LPC Coef */
void WebRtcIsac_EncodeLar(double *LPCCoef, int *model, double *size, Bitstr *streamdata, ISAC_SaveEncData_t* encData) {
  int j, k, n, bmodel, pos, pos2, poss, posg, offsg, offss, offs2;
  int index_g[KLT_ORDER_GAIN], index_s[KLT_ORDER_SHAPE];
  int index_ovr_g[KLT_ORDER_GAIN], index_ovr_s[KLT_ORDER_SHAPE];
  double Bits;
  double tmpcoeffs_g[KLT_ORDER_GAIN], tmpcoeffs_s[KLT_ORDER_SHAPE];
  double tmpcoeffs2_g[KLT_ORDER_GAIN], tmpcoeffs2_s[KLT_ORDER_SHAPE];
  double sum;

  /* Only one LPC model remains in iSAC. Tables for other models are saved for compatibility reasons. */
  bmodel = 0;

  /* log gains, mean removal and scaling */
  posg = 0;poss = 0;pos=0;

  for (k=0; k<SUBFRAMES; k++) {
    /* log gains */
    tmpcoeffs_g[posg] = log(LPCCoef[pos]);
    tmpcoeffs_g[posg] -= WebRtcIsac_kLpcMeansGain[bmodel][posg];
    tmpcoeffs_g[posg] *= LPC_GAIN_SCALE;
    posg++;pos++;

    tmpcoeffs_g[posg] = log(LPCCoef[pos]);
    tmpcoeffs_g[posg] -= WebRtcIsac_kLpcMeansGain[bmodel][posg];
    tmpcoeffs_g[posg] *= LPC_GAIN_SCALE;
    posg++;pos++;

    /* lo band LAR coeffs */
    for (n=0; n<LPC_LOBAND_ORDER; n++, poss++, pos++) {
      tmpcoeffs_s[poss] = LPCCoef[pos] - WebRtcIsac_kLpcMeansShape[bmodel][poss];
      tmpcoeffs_s[poss] *= LPC_LOBAND_SCALE;
    }

    /* hi band LAR coeffs */
    for (n=0; n<LPC_HIBAND_ORDER; n++, poss++, pos++) {
      tmpcoeffs_s[poss] = LPCCoef[pos] - WebRtcIsac_kLpcMeansShape[bmodel][poss];
      tmpcoeffs_s[poss] *= LPC_HIBAND_SCALE;
    }
  }

  /* KLT  */

  /* left transform */
  offsg = 0;
  offss = 0;
  for (j=0; j<SUBFRAMES; j++) {
    posg = offsg;
    for (k=0; k<LPC_GAIN_ORDER; k++) {
      sum = 0;
      pos = offsg;
      pos2 = k;
      for (n=0; n<LPC_GAIN_ORDER; n++) {
        sum += tmpcoeffs_g[pos++] * WebRtcIsac_kKltT1Gain[bmodel][pos2];
        pos2 += LPC_GAIN_ORDER;
      }
      tmpcoeffs2_g[posg++] = sum;
    }
    poss = offss;
    for (k=0; k<LPC_SHAPE_ORDER; k++) {
      sum = 0;
      pos = offss;
      pos2 = k;
      for (n=0; n<LPC_SHAPE_ORDER; n++) {
        sum += tmpcoeffs_s[pos++] * WebRtcIsac_kKltT1Shape[bmodel][pos2];
        pos2 += LPC_SHAPE_ORDER;
      }
      tmpcoeffs2_s[poss++] = sum;
    }
    offsg += LPC_GAIN_ORDER;
    offss += LPC_SHAPE_ORDER;
  }

  /* right transform */
  offsg = 0;
  offss = 0;
  offs2 = 0;
  for (j=0; j<SUBFRAMES; j++) {
    posg = offsg;
    for (k=0; k<LPC_GAIN_ORDER; k++) {
      sum = 0;
      pos = k;
      pos2 = offs2;
      for (n=0; n<SUBFRAMES; n++) {
        sum += tmpcoeffs2_g[pos] * WebRtcIsac_kKltT2Gain[bmodel][pos2++];
        pos += LPC_GAIN_ORDER;
      }
      tmpcoeffs_g[posg++] = sum;
    }
    poss = offss;
    for (k=0; k<LPC_SHAPE_ORDER; k++) {
      sum = 0;
      pos = k;
      pos2 = offs2;
      for (n=0; n<SUBFRAMES; n++) {
        sum += tmpcoeffs2_s[pos] * WebRtcIsac_kKltT2Shape[bmodel][pos2++];
        pos += LPC_SHAPE_ORDER;
      }
      tmpcoeffs_s[poss++] = sum;
    }
    offs2 += SUBFRAMES;
    offsg += LPC_GAIN_ORDER;
    offss += LPC_SHAPE_ORDER;
  }

  /* quantize coefficients */

  Bits = 0.0;
  for (k=0; k<KLT_ORDER_GAIN; k++) //ATTN: ok?
  {
    pos = WebRtcIsac_kQKltSelIndGain[k];
    pos2= WebRtcIsac_lrint(tmpcoeffs_g[pos] / KLT_STEPSIZE);
    index_g[k] = (pos2) + WebRtcIsac_kQKltQuantMinGain[k]; //ATTN: ok?
    if (index_g[k] < 0) {
      index_g[k] = 0;
    }
    else if (index_g[k] > WebRtcIsac_kQKltMaxIndGain[k])
      index_g[k] = WebRtcIsac_kQKltMaxIndGain[k];
    index_ovr_g[k] = WebRtcIsac_kQKltOffsetGain[bmodel][k]+index_g[k];
    pos = WebRtcIsac_kQKltOfLevelsGain[bmodel] + index_ovr_g[k];

    /* determine number of bits */
    sum = WebRtcIsac_kQKltCodeLenGain[pos];
    Bits += sum;
  }

  for (k=0; k<KLT_ORDER_SHAPE; k++) //ATTN: ok?
  {
    index_s[k] = (WebRtcIsac_lrint(tmpcoeffs_s[WebRtcIsac_kQKltSelIndShape[k]] / KLT_STEPSIZE)) + WebRtcIsac_kQKltQuantMinShape[k]; //ATTN: ok?
    if (index_s[k] < 0)
      index_s[k] = 0;
    else if (index_s[k] > WebRtcIsac_kQKltMaxIndShape[k])
      index_s[k] = WebRtcIsac_kQKltMaxIndShape[k];
    index_ovr_s[k] = WebRtcIsac_kQKltOffsetShape[bmodel][k]+index_s[k];
    pos = WebRtcIsac_kQKltOfLevelsShape[bmodel] + index_ovr_s[k];
    sum = WebRtcIsac_kQKltCodeLenShape[pos];
    Bits += sum;
  }


  /* Only one model remains in this version of the code, model = 0 */
  *model=bmodel;
  *size=Bits;

  /* entropy coding of model number */
  WebRtcIsac_EncHistMulti(streamdata, model, WebRtcIsac_kQKltModelCdfPtr, 1);

  /* entropy coding of quantization indices - shape only */
  WebRtcIsac_EncHistMulti(streamdata, index_s, WebRtcIsac_kQKltCdfPtrShape[bmodel], KLT_ORDER_SHAPE);

  /* Save data for creation of multiple bit streams */
  encData->LPCmodel[encData->startIdx] = 0;
  for (k=0; k<KLT_ORDER_SHAPE; k++)
  {
    encData->LPCindex_s[KLT_ORDER_SHAPE*encData->startIdx + k] = index_s[k];
  }

  /* find quantization levels for shape coefficients */
  for (k=0; k<KLT_ORDER_SHAPE; k++) {
    tmpcoeffs_s[WebRtcIsac_kQKltSelIndShape[k]] = WebRtcIsac_kQKltLevelsShape[WebRtcIsac_kQKltOfLevelsShape[bmodel]+index_ovr_s[k]];
  }
  /* inverse KLT  */
  /* left transform */  // Transpose matrix!
  offss = 0;
  poss = 0;
  for (j=0; j<SUBFRAMES; j++) {
    offs2 = 0;
    for (k=0; k<LPC_SHAPE_ORDER; k++) {
      sum = 0;
      pos = offss;
      pos2 = offs2;
      for (n=0; n<LPC_SHAPE_ORDER; n++)
        sum += tmpcoeffs_s[pos++] * WebRtcIsac_kKltT1Shape[bmodel][pos2++];
      tmpcoeffs2_s[poss++] = sum;
      offs2 += LPC_SHAPE_ORDER;
    }
    offss += LPC_SHAPE_ORDER;
  }


  /* right transform */ // Transpose matrix
  offss = 0;
  poss = 0;
  for (j=0; j<SUBFRAMES; j++) {
    poss = offss;
    for (k=0; k<LPC_SHAPE_ORDER; k++) {
      sum = 0;
      pos = k;
      pos2 = j;
      for (n=0; n<SUBFRAMES; n++) {
        sum += tmpcoeffs2_s[pos] * WebRtcIsac_kKltT2Shape[bmodel][pos2];
        pos += LPC_SHAPE_ORDER;
        pos2 += SUBFRAMES;
      }
      tmpcoeffs_s[poss++] = sum;
    }
    offss += LPC_SHAPE_ORDER;
  }

  /* scaling, mean addition, and gain restoration */
  poss = 0;pos=0;
  for (k=0; k<SUBFRAMES; k++) {

    /* log gains */
    pos+=2;

    /* lo band LAR coeffs */
    for (n=0; n<LPC_LOBAND_ORDER; n++, pos++, poss++) {
      LPCCoef[pos] = tmpcoeffs_s[poss] / LPC_LOBAND_SCALE;
      LPCCoef[pos] += WebRtcIsac_kLpcMeansShape[bmodel][poss];
    }

    /* hi band LAR coeffs */
    for (n=0; n<LPC_HIBAND_ORDER; n++, pos++, poss++) {
      LPCCoef[pos] = tmpcoeffs_s[poss] / LPC_HIBAND_SCALE;
      LPCCoef[pos] += WebRtcIsac_kLpcMeansShape[bmodel][poss];
    }

  }

}


void WebRtcIsac_EncodeLpcLb(double *LPCCoef_lo, double *LPCCoef_hi, int *model,
                            double *size, Bitstr *streamdata, ISAC_SaveEncData_t* encData) {

  double lars[KLT_ORDER_GAIN+KLT_ORDER_SHAPE];
  int k;

  WebRtcIsac_Poly2Lar(LPCCoef_lo, ORDERLO, LPCCoef_hi, ORDERHI, SUBFRAMES, lars);
  WebRtcIsac_EncodeLar(lars, model, size, streamdata, encData);
  WebRtcIsac_Lar2Poly(lars, LPCCoef_lo, ORDERLO, LPCCoef_hi, ORDERHI, SUBFRAMES);
  /* Save data for creation of multiple bit streams (and transcoding) */
  for (k=0; k<(ORDERLO+1)*SUBFRAMES; k++) {
    encData->LPCcoeffs_lo[(ORDERLO+1)*SUBFRAMES*encData->startIdx + k] = LPCCoef_lo[k];
  }
  for (k=0; k<(ORDERHI+1)*SUBFRAMES; k++) {
    encData->LPCcoeffs_hi[(ORDERHI+1)*SUBFRAMES*encData->startIdx + k] = LPCCoef_hi[k];
  }
}


WebRtc_Word16
WebRtcIsac_EncodeLpcUB(
    double*                  lpcVecs,
    Bitstr*                  streamdata,
    double*                  interpolLPCCoeff,
    WebRtc_Word16              bandwidth,
    ISACUBSaveEncDataStruct* encData)
{

  double    U[UB_LPC_ORDER * UB16_LPC_VEC_PER_FRAME];
  int     idx[UB_LPC_ORDER * UB16_LPC_VEC_PER_FRAME];
  int interpolCntr;

  WebRtcIsac_Poly2LarUB(lpcVecs, bandwidth);
  WebRtcIsac_RemoveLarMean(lpcVecs, bandwidth);
  WebRtcIsac_DecorrelateIntraVec(lpcVecs, U, bandwidth);
  WebRtcIsac_DecorrelateInterVec(U, lpcVecs, bandwidth);
  WebRtcIsac_QuantizeUncorrLar(lpcVecs, idx, bandwidth);

  WebRtcIsac_CorrelateInterVec(lpcVecs, U, bandwidth);
  WebRtcIsac_CorrelateIntraVec(U, lpcVecs, bandwidth);
  WebRtcIsac_AddLarMean(lpcVecs, bandwidth);

  switch(bandwidth)
  {
    case isac12kHz:
      {
        // Stor the indices to be used for multiple encoding.
        memcpy(encData->indexLPCShape, idx, UB_LPC_ORDER * UB_LPC_VEC_PER_FRAME *
               sizeof(int));
        WebRtcIsac_EncHistMulti(streamdata, idx, WebRtcIsac_kLpcShapeCdfMatUb12,
                                UB_LPC_ORDER * UB_LPC_VEC_PER_FRAME);
        for(interpolCntr = 0; interpolCntr < UB_INTERPOL_SEGMENTS; interpolCntr++)
        {
          WebRtcIsac_Lar2PolyInterpolUB(lpcVecs,
                                        interpolLPCCoeff, kLpcVecPerSegmentUb12 + 1);
          lpcVecs += UB_LPC_ORDER;
          interpolLPCCoeff += (kLpcVecPerSegmentUb12 * (UB_LPC_ORDER + 1));
        }
        break;
      }
    case isac16kHz:
      {
        // Stor the indices to be used for multiple encoding.
        memcpy(encData->indexLPCShape, idx, UB_LPC_ORDER * UB16_LPC_VEC_PER_FRAME *
               sizeof(int));
        WebRtcIsac_EncHistMulti(streamdata, idx, WebRtcIsac_kLpcShapeCdfMatUb16,
                                UB_LPC_ORDER * UB16_LPC_VEC_PER_FRAME);
        for(interpolCntr = 0; interpolCntr < UB16_INTERPOL_SEGMENTS; interpolCntr++)
        {
          WebRtcIsac_Lar2PolyInterpolUB(lpcVecs,
                                        interpolLPCCoeff, kLpcVecPerSegmentUb16 + 1);
          lpcVecs += UB_LPC_ORDER;
          interpolLPCCoeff += (kLpcVecPerSegmentUb16 * (UB_LPC_ORDER + 1));
        }
        break;
      }
    default:
      return -1;
  }
  return 0;
}

void WebRtcIsac_EncodeLpcGainLb(double *LPCCoef_lo, double *LPCCoef_hi, int model, Bitstr *streamdata, ISAC_SaveEncData_t* encData) {

  int j, k, n, pos, pos2, posg, offsg, offs2;
  int index_g[KLT_ORDER_GAIN];
  int index_ovr_g[KLT_ORDER_GAIN];
  double tmpcoeffs_g[KLT_ORDER_GAIN];
  double tmpcoeffs2_g[KLT_ORDER_GAIN];
  double sum;

  /* log gains, mean removal and scaling */
  posg = 0;
  for (k=0; k<SUBFRAMES; k++) {
    tmpcoeffs_g[posg] = log(LPCCoef_lo[(LPC_LOBAND_ORDER+1)*k]);
    tmpcoeffs_g[posg] -= WebRtcIsac_kLpcMeansGain[model][posg];
    tmpcoeffs_g[posg] *= LPC_GAIN_SCALE;
    posg++;
    tmpcoeffs_g[posg] = log(LPCCoef_hi[(LPC_HIBAND_ORDER+1)*k]);
    tmpcoeffs_g[posg] -= WebRtcIsac_kLpcMeansGain[model][posg];
    tmpcoeffs_g[posg] *= LPC_GAIN_SCALE;
    posg++;
  }

  /* KLT  */

  /* left transform */
  offsg = 0;
  for (j=0; j<SUBFRAMES; j++) {
    posg = offsg;
    for (k=0; k<LPC_GAIN_ORDER; k++) {
      sum = 0;
      pos = offsg;
      pos2 = k;
      for (n=0; n<LPC_GAIN_ORDER; n++) {
        sum += tmpcoeffs_g[pos++] * WebRtcIsac_kKltT1Gain[model][pos2];
        pos2 += LPC_GAIN_ORDER;
      }
      tmpcoeffs2_g[posg++] = sum;
    }
    offsg += LPC_GAIN_ORDER;
  }

  /* right transform */
  offsg = 0;
  offs2 = 0;
  for (j=0; j<SUBFRAMES; j++) {
    posg = offsg;
    for (k=0; k<LPC_GAIN_ORDER; k++) {
      sum = 0;
      pos = k;
      pos2 = offs2;
      for (n=0; n<SUBFRAMES; n++) {
        sum += tmpcoeffs2_g[pos] * WebRtcIsac_kKltT2Gain[model][pos2++];
        pos += LPC_GAIN_ORDER;
      }
      tmpcoeffs_g[posg++] = sum;
    }
    offs2 += SUBFRAMES;
    offsg += LPC_GAIN_ORDER;
  }


  /* quantize coefficients */
  for (k=0; k<KLT_ORDER_GAIN; k++) {

    /* get index */
    pos = WebRtcIsac_kQKltSelIndGain[k];
    pos2= WebRtcIsac_lrint(tmpcoeffs_g[pos] / KLT_STEPSIZE);
    index_g[k] = (pos2) + WebRtcIsac_kQKltQuantMinGain[k];
    if (index_g[k] < 0) {
      index_g[k] = 0;
    }
    else if (index_g[k] > WebRtcIsac_kQKltMaxIndGain[k]) {
      index_g[k] = WebRtcIsac_kQKltMaxIndGain[k];
    }
    index_ovr_g[k] = WebRtcIsac_kQKltOffsetGain[model][k]+index_g[k];

    /* find quantization levels for coefficients */
    tmpcoeffs_g[WebRtcIsac_kQKltSelIndGain[k]] = WebRtcIsac_kQKltLevelsGain[WebRtcIsac_kQKltOfLevelsGain[model]+index_ovr_g[k]];

    /* Save data for creation of multiple bit streams */
    encData->LPCindex_g[KLT_ORDER_GAIN*encData->startIdx + k] = index_g[k];
  }


  /* entropy coding of quantization indices - gain */
  WebRtcIsac_EncHistMulti(streamdata, index_g, WebRtcIsac_kQKltCdfPtrGain[model], KLT_ORDER_GAIN);

  /* find quantization levels for coefficients */

  /* left transform */
  offsg = 0;
  posg = 0;
  for (j=0; j<SUBFRAMES; j++) {
    offs2 = 0;
    for (k=0; k<LPC_GAIN_ORDER; k++) {
      sum = 0;
      pos = offsg;
      pos2 = offs2;
      for (n=0; n<LPC_GAIN_ORDER; n++)
        sum += tmpcoeffs_g[pos++] * WebRtcIsac_kKltT1Gain[model][pos2++];
      tmpcoeffs2_g[posg++] = sum;
      offs2 += LPC_GAIN_ORDER;
    }
    offsg += LPC_GAIN_ORDER;
  }

  /* right transform */ // Transpose matrix
  offsg = 0;
  posg = 0;
  for (j=0; j<SUBFRAMES; j++) {
    posg = offsg;
    for (k=0; k<LPC_GAIN_ORDER; k++) {
      sum = 0;
      pos = k;
      pos2 = j;
      for (n=0; n<SUBFRAMES; n++) {
        sum += tmpcoeffs2_g[pos] * WebRtcIsac_kKltT2Gain[model][pos2];
        pos += LPC_GAIN_ORDER;
        pos2 += SUBFRAMES;
      }
      tmpcoeffs_g[posg++] = sum;
    }
    offsg += LPC_GAIN_ORDER;
  }


  /* scaling, mean addition, and gain restoration */
  posg = 0;
  for (k=0; k<SUBFRAMES; k++) {
    sum = tmpcoeffs_g[posg] / LPC_GAIN_SCALE;
    sum += WebRtcIsac_kLpcMeansGain[model][posg];
    LPCCoef_lo[k*(LPC_LOBAND_ORDER+1)] = exp(sum);
    pos++;posg++;
    sum = tmpcoeffs_g[posg] / LPC_GAIN_SCALE;
    sum += WebRtcIsac_kLpcMeansGain[model][posg];
    LPCCoef_hi[k*(LPC_HIBAND_ORDER+1)] = exp(sum);
    pos++;posg++;
  }

}

void
WebRtcIsac_EncodeLpcGainUb(
    double* lpGains,
    Bitstr* streamdata,
    int*    lpcGainIndex)
{
  double U[UB_LPC_GAIN_DIM];
  int idx[UB_LPC_GAIN_DIM];
  WebRtcIsac_ToLogDomainRemoveMean(lpGains);
  WebRtcIsac_DecorrelateLPGain(lpGains, U);
  WebRtcIsac_QuantizeLpcGain(U, idx);
  // Store the index for re-encoding for FEC.
  memcpy(lpcGainIndex, idx, UB_LPC_GAIN_DIM * sizeof(int));
  WebRtcIsac_CorrelateLpcGain(U, lpGains);
  WebRtcIsac_AddMeanToLinearDomain(lpGains);
  WebRtcIsac_EncHistMulti(streamdata, idx, WebRtcIsac_kLpcGainCdfMat, UB_LPC_GAIN_DIM);
}


void
WebRtcIsac_StoreLpcGainUb(
    double* lpGains,
    Bitstr* streamdata)
{
  double U[UB_LPC_GAIN_DIM];
  int idx[UB_LPC_GAIN_DIM];
  WebRtcIsac_ToLogDomainRemoveMean(lpGains);
  WebRtcIsac_DecorrelateLPGain(lpGains, U);
  WebRtcIsac_QuantizeLpcGain(U, idx);
  WebRtcIsac_EncHistMulti(streamdata, idx, WebRtcIsac_kLpcGainCdfMat, UB_LPC_GAIN_DIM);
}



WebRtc_Word16
WebRtcIsac_DecodeLpcGainUb(
    double* lpGains,
    Bitstr* streamdata)
{
  double U[UB_LPC_GAIN_DIM];
  int idx[UB_LPC_GAIN_DIM];
  int err;
  err = WebRtcIsac_DecHistOneStepMulti(idx, streamdata,
                                       WebRtcIsac_kLpcGainCdfMat, WebRtcIsac_kLpcGainEntropySearch,
                                       UB_LPC_GAIN_DIM);
  if(err < 0)
  {
    return -1;
  }
  WebRtcIsac_DequantizeLpcGain(idx, U);
  WebRtcIsac_CorrelateLpcGain(U, lpGains);
  WebRtcIsac_AddMeanToLinearDomain(lpGains);
  return 0;
}



/* decode & dequantize RC */
int WebRtcIsac_DecodeRc(Bitstr *streamdata, WebRtc_Word16 *RCQ15)
{
  int k, err;
  int index[AR_ORDER];

  /* entropy decoding of quantization indices */
  err = WebRtcIsac_DecHistOneStepMulti(index, streamdata, WebRtcIsac_kQArRcCdfPtr,
                                       WebRtcIsac_kQArRcInitIndex, AR_ORDER);
  if (err<0)  // error check
    return err;

  /* find quantization levels for reflection coefficients */
  for (k=0; k<AR_ORDER; k++)
  {
    RCQ15[k] = *(WebRtcIsac_kQArRcLevelsPtr[k] + index[k]);
  }

  return 0;
}



/* quantize & code RC */
void WebRtcIsac_EncodeRc(WebRtc_Word16 *RCQ15, Bitstr *streamdata)
{
  int k;
  int index[AR_ORDER];

  /* quantize reflection coefficients (add noise feedback?) */
  for (k=0; k<AR_ORDER; k++)
  {
    index[k] = WebRtcIsac_kQArRcInitIndex[k];

    if (RCQ15[k] > WebRtcIsac_kQArBoundaryLevels[index[k]])
    {
      while (RCQ15[k] > WebRtcIsac_kQArBoundaryLevels[index[k] + 1])
        index[k]++;
    }
    else
    {
      while (RCQ15[k] < WebRtcIsac_kQArBoundaryLevels[--index[k]]) ;
    }

    RCQ15[k] = *(WebRtcIsac_kQArRcLevelsPtr[k] + index[k]);
  }


  /* entropy coding of quantization indices */
  WebRtcIsac_EncHistMulti(streamdata, index, WebRtcIsac_kQArRcCdfPtr, AR_ORDER);
}


/* decode & dequantize squared Gain */
int WebRtcIsac_DecodeGain2(Bitstr *streamdata, WebRtc_Word32 *gainQ10)
{
  int index, err;

  /* entropy decoding of quantization index */
  err = WebRtcIsac_DecHistOneStepMulti(&index, streamdata, WebRtcIsac_kQGainCdf_ptr,
                                       WebRtcIsac_kQGainInitIndex, 1);
  if (err<0)  // error check
    return err;

  /* find quantization level */
  *gainQ10 = WebRtcIsac_kQGain2Levels[index];

  return 0;
}



/* quantize & code squared Gain */
int WebRtcIsac_EncodeGain2(WebRtc_Word32 *gainQ10, Bitstr *streamdata)
{
  int index;


  /* find quantization index */
  index = WebRtcIsac_kQGainInitIndex[0];
  if (*gainQ10 > WebRtcIsac_kQGain2BoundaryLevels[index])
  {
    while (*gainQ10 > WebRtcIsac_kQGain2BoundaryLevels[index + 1])
      index++;
  }
  else
  {
    while (*gainQ10 < WebRtcIsac_kQGain2BoundaryLevels[--index]) ;
  }

  /* dequantize */
  *gainQ10 = WebRtcIsac_kQGain2Levels[index];


  /* entropy coding of quantization index */
  WebRtcIsac_EncHistMulti(streamdata, &index, WebRtcIsac_kQGainCdf_ptr, 1);

  return 0;
}


/* code and decode Pitch Gains and Lags functions */

/* decode & dequantize Pitch Gains */
int WebRtcIsac_DecodePitchGain(Bitstr *streamdata, WebRtc_Word16 *PitchGains_Q12)
{
  int index_comb, err;
  const WebRtc_UWord16 *WebRtcIsac_kQPitchGainCdf_ptr[1];

  /* entropy decoding of quantization indices */
  *WebRtcIsac_kQPitchGainCdf_ptr = WebRtcIsac_kQPitchGainCdf;
  err = WebRtcIsac_DecHistBisectMulti(&index_comb, streamdata, WebRtcIsac_kQPitchGainCdf_ptr, WebRtcIsac_kQCdfTableSizeGain, 1);
  /* error check, Q_mean_Gain.. tables are of size 144 */
  if ((err<0) || (index_comb<0) || (index_comb>144))
    return -ISAC_RANGE_ERROR_DECODE_PITCH_GAIN;

  /* unquantize back to pitch gains by table look-up */
  PitchGains_Q12[0] = WebRtcIsac_kQMeanGain1Q12[index_comb];
  PitchGains_Q12[1] = WebRtcIsac_kQMeanGain2Q12[index_comb];
  PitchGains_Q12[2] = WebRtcIsac_kQMeanGain3Q12[index_comb];
  PitchGains_Q12[3] = WebRtcIsac_kQMeanGain4Q12[index_comb];

  return 0;
}


/* quantize & code Pitch Gains */
void WebRtcIsac_EncodePitchGain(WebRtc_Word16 *PitchGains_Q12, Bitstr *streamdata, ISAC_SaveEncData_t* encData)
{
  int k,j;
  double C;
  double S[PITCH_SUBFRAMES];
  int index[3];
  int index_comb;
  const WebRtc_UWord16 *WebRtcIsac_kQPitchGainCdf_ptr[1];
  double PitchGains[PITCH_SUBFRAMES] = {0,0,0,0};

  /* take the asin */
  for (k=0; k<PITCH_SUBFRAMES; k++)
  {
    PitchGains[k] = ((float)PitchGains_Q12[k])/4096;
    S[k] = asin(PitchGains[k]);
  }


  /* find quantization index; only for the first three transform coefficients */
  for (k=0; k<3; k++)
  {
    /*  transform */
    C = 0.0;
    for (j=0; j<PITCH_SUBFRAMES; j++)
      C += WebRtcIsac_kTransform[k][j] * S[j];

    /* quantize */
    index[k] = WebRtcIsac_lrint(C / PITCH_GAIN_STEPSIZE);

    /* check that the index is not outside the boundaries of the table */
    if (index[k] < WebRtcIsac_kIndexLowerLimitGain[k]) index[k] = WebRtcIsac_kIndexLowerLimitGain[k];
    else if (index[k] > WebRtcIsac_kIndexUpperLimitGain[k]) index[k] = WebRtcIsac_kIndexUpperLimitGain[k];
    index[k] -= WebRtcIsac_kIndexLowerLimitGain[k];
  }

  /* calculate unique overall index */
  index_comb = WebRtcIsac_kIndexMultsGain[0] * index[0] + WebRtcIsac_kIndexMultsGain[1] * index[1] + index[2];

  /* unquantize back to pitch gains by table look-up */
  PitchGains_Q12[0] = WebRtcIsac_kQMeanGain1Q12[index_comb];
  PitchGains_Q12[1] = WebRtcIsac_kQMeanGain2Q12[index_comb];
  PitchGains_Q12[2] = WebRtcIsac_kQMeanGain3Q12[index_comb];
  PitchGains_Q12[3] = WebRtcIsac_kQMeanGain4Q12[index_comb];

  /* entropy coding of quantization pitch gains */
  *WebRtcIsac_kQPitchGainCdf_ptr = WebRtcIsac_kQPitchGainCdf;
  WebRtcIsac_EncHistMulti(streamdata, &index_comb, WebRtcIsac_kQPitchGainCdf_ptr, 1);
  encData->pitchGain_index[encData->startIdx] = index_comb;

}



/* Pitch LAG */


/* decode & dequantize Pitch Lags */
int WebRtcIsac_DecodePitchLag(Bitstr *streamdata, WebRtc_Word16 *PitchGain_Q12, double *PitchLags)
{
  int k, err;
  double StepSize;
  double C;
  int index[PITCH_SUBFRAMES];
  double mean_gain;
  const double *mean_val2, *mean_val3, *mean_val4;
  const WebRtc_Word16 *lower_limit;
  const WebRtc_UWord16 *init_index;
  const WebRtc_UWord16 *cdf_size;
  const WebRtc_UWord16 **cdf;

  //(Y)
  double PitchGain[4]={0,0,0,0};
  //

  /* compute mean pitch gain */
  mean_gain = 0.0;
  for (k = 0; k < 4; k++)
  {
    //(Y)
    PitchGain[k] = ((float)PitchGain_Q12[k])/4096;
    //(Y)
    mean_gain += PitchGain[k];
  }
  mean_gain /= 4.0;

  /* voicing classificiation */
  if (mean_gain < 0.2) {
    StepSize = WebRtcIsac_kQPitchLagStepsizeLo;
    cdf = WebRtcIsac_kQPitchLagCdfPtrLo;
    cdf_size = WebRtcIsac_kQPitchLagCdfSizeLo;
    mean_val2 = WebRtcIsac_kQMeanLag2Lo;
    mean_val3 = WebRtcIsac_kQMeanLag3Lo;
    mean_val4 = WebRtcIsac_kQMeanLag4Lo;
    lower_limit = WebRtcIsac_kQIndexLowerLimitLagLo;
    init_index = WebRtcIsac_kQInitIndexLagLo;
  } else if (mean_gain < 0.4) {
    StepSize = WebRtcIsac_kQPitchLagStepsizeMid;
    cdf = WebRtcIsac_kQPitchLagCdfPtrMid;
    cdf_size = WebRtcIsac_kQPitchLagCdfSizeMid;
    mean_val2 = WebRtcIsac_kQMeanLag2Mid;
    mean_val3 = WebRtcIsac_kQMeanLag3Mid;
    mean_val4 = WebRtcIsac_kQMeanLag4Mid;
    lower_limit = WebRtcIsac_kQIndexLowerLimitLagMid;
    init_index = WebRtcIsac_kQInitIndexLagMid;
  } else {
    StepSize = WebRtcIsac_kQPitchLagStepsizeHi;
    cdf = WebRtcIsac_kQPitchLagCdfPtrHi;
    cdf_size = WebRtcIsac_kQPitchLagCdfSizeHi;
    mean_val2 = WebRtcIsac_kQMeanLag2Hi;
    mean_val3 = WebRtcIsac_kQMeanLag3Hi;
    mean_val4 = WebRtcIsac_kQMeanLag4Hi;
    lower_limit = WebRtcIsac_kQindexLowerLimitLagHi;
    init_index = WebRtcIsac_kQInitIndexLagHi;
  }

  /* entropy decoding of quantization indices */
  err = WebRtcIsac_DecHistBisectMulti(index, streamdata, cdf, cdf_size, 1);
  if ((err<0) || (index[0]<0))  // error check
    return -ISAC_RANGE_ERROR_DECODE_PITCH_LAG;

  err = WebRtcIsac_DecHistOneStepMulti(index+1, streamdata, cdf+1, init_index, 3);
  if (err<0)  // error check
    return -ISAC_RANGE_ERROR_DECODE_PITCH_LAG;


  /* unquantize back to transform coefficients and do the inverse transform: S = T'*C */
  C = (index[0] + lower_limit[0]) * StepSize;
  for (k=0; k<PITCH_SUBFRAMES; k++)
    PitchLags[k] = WebRtcIsac_kTransformTranspose[k][0] * C;
  C = mean_val2[index[1]];
  for (k=0; k<PITCH_SUBFRAMES; k++)
    PitchLags[k] += WebRtcIsac_kTransformTranspose[k][1] * C;
  C = mean_val3[index[2]];
  for (k=0; k<PITCH_SUBFRAMES; k++)
    PitchLags[k] += WebRtcIsac_kTransformTranspose[k][2] * C;
  C = mean_val4[index[3]];
  for (k=0; k<PITCH_SUBFRAMES; k++)
    PitchLags[k] += WebRtcIsac_kTransformTranspose[k][3] * C;

  return 0;
}



/* quantize & code Pitch Lags */
void WebRtcIsac_EncodePitchLag(double* PitchLags, WebRtc_Word16* PitchGain_Q12, Bitstr* streamdata, ISAC_SaveEncData_t* encData)
{
  int k, j;
  double StepSize;
  double C;
  int index[PITCH_SUBFRAMES];
  double mean_gain;
  const double *mean_val2, *mean_val3, *mean_val4;
  const WebRtc_Word16 *lower_limit, *upper_limit;
  const WebRtc_UWord16 **cdf;

  //(Y)
  double PitchGain[4]={0,0,0,0};
  //

  /* compute mean pitch gain */
  mean_gain = 0.0;
  for (k = 0; k < 4; k++)
  {
    //(Y)
    PitchGain[k] = ((float)PitchGain_Q12[k])/4096;
    //(Y)
    mean_gain += PitchGain[k];
  }
  mean_gain /= 4.0;

  /* Save data for creation of multiple bit streams */
  encData->meanGain[encData->startIdx] = mean_gain;

  /* voicing classification */
  if (mean_gain < 0.2) {
    StepSize = WebRtcIsac_kQPitchLagStepsizeLo;
    cdf = WebRtcIsac_kQPitchLagCdfPtrLo;
    mean_val2 = WebRtcIsac_kQMeanLag2Lo;
    mean_val3 = WebRtcIsac_kQMeanLag3Lo;
    mean_val4 = WebRtcIsac_kQMeanLag4Lo;
    lower_limit = WebRtcIsac_kQIndexLowerLimitLagLo;
    upper_limit = WebRtcIsac_kQIndexUpperLimitLagLo;
  } else if (mean_gain < 0.4) {
    StepSize = WebRtcIsac_kQPitchLagStepsizeMid;
    cdf = WebRtcIsac_kQPitchLagCdfPtrMid;
    mean_val2 = WebRtcIsac_kQMeanLag2Mid;
    mean_val3 = WebRtcIsac_kQMeanLag3Mid;
    mean_val4 = WebRtcIsac_kQMeanLag4Mid;
    lower_limit = WebRtcIsac_kQIndexLowerLimitLagMid;
    upper_limit = WebRtcIsac_kQIndexUpperLimitLagMid;
  } else {
    StepSize = WebRtcIsac_kQPitchLagStepsizeHi;
    cdf = WebRtcIsac_kQPitchLagCdfPtrHi;
    mean_val2 = WebRtcIsac_kQMeanLag2Hi;
    mean_val3 = WebRtcIsac_kQMeanLag3Hi;
    mean_val4 = WebRtcIsac_kQMeanLag4Hi;
    lower_limit = WebRtcIsac_kQindexLowerLimitLagHi;
    upper_limit = WebRtcIsac_kQindexUpperLimitLagHi;
  }


  /* find quantization index */
  for (k=0; k<4; k++)
  {
    /*  transform */
    C = 0.0;
    for (j=0; j<PITCH_SUBFRAMES; j++)
      C += WebRtcIsac_kTransform[k][j] * PitchLags[j];

    /* quantize */
    index[k] = WebRtcIsac_lrint(C / StepSize);

    /* check that the index is not outside the boundaries of the table */
    if (index[k] < lower_limit[k]) index[k] = lower_limit[k];
    else if (index[k] > upper_limit[k]) index[k] = upper_limit[k];
    index[k] -= lower_limit[k];

    /* Save data for creation of multiple bit streams */
    encData->pitchIndex[PITCH_SUBFRAMES*encData->startIdx + k] = index[k];
  }

  /* unquantize back to transform coefficients and do the inverse transform: S = T'*C */
  C = (index[0] + lower_limit[0]) * StepSize;
  for (k=0; k<PITCH_SUBFRAMES; k++)
    PitchLags[k] = WebRtcIsac_kTransformTranspose[k][0] * C;
  C = mean_val2[index[1]];
  for (k=0; k<PITCH_SUBFRAMES; k++)
    PitchLags[k] += WebRtcIsac_kTransformTranspose[k][1] * C;
  C = mean_val3[index[2]];
  for (k=0; k<PITCH_SUBFRAMES; k++)
    PitchLags[k] += WebRtcIsac_kTransformTranspose[k][2] * C;
  C = mean_val4[index[3]];
  for (k=0; k<PITCH_SUBFRAMES; k++)
    PitchLags[k] += WebRtcIsac_kTransformTranspose[k][3] * C;


  /* entropy coding of quantization pitch lags */
  WebRtcIsac_EncHistMulti(streamdata, index, cdf, PITCH_SUBFRAMES);

}



/* Routines for in-band signaling of bandwidth estimation */
/* Histograms based on uniform distribution of indices */
/* Move global variables later! */


/* cdf array for frame length indicator */
const WebRtc_UWord16 WebRtcIsac_kFrameLengthCdf[4] = {
  0, 21845, 43690, 65535};

/* pointer to cdf array for frame length indicator */
const WebRtc_UWord16 *WebRtcIsac_kFrameLengthCdf_ptr[1] = {WebRtcIsac_kFrameLengthCdf};

/* initial cdf index for decoder of frame length indicator */
const WebRtc_UWord16 WebRtcIsac_kFrameLengthInitIndex[1] = {1};


int WebRtcIsac_DecodeFrameLen(Bitstr *streamdata,
                              WebRtc_Word16 *framesamples)
{

  int frame_mode, err;

  err = 0;
  /* entropy decoding of frame length [1:30ms,2:60ms] */
  err = WebRtcIsac_DecHistOneStepMulti(&frame_mode, streamdata, WebRtcIsac_kFrameLengthCdf_ptr, WebRtcIsac_kFrameLengthInitIndex, 1);
  if (err<0)  // error check
    return -ISAC_RANGE_ERROR_DECODE_FRAME_LENGTH;

  switch(frame_mode) {
    case 1:
      *framesamples = 480; /* 30ms */
      break;
    case 2:
      *framesamples = 960; /* 60ms */
      break;
    default:
      err = -ISAC_DISALLOWED_FRAME_MODE_DECODER;
  }

  return err;
}

int WebRtcIsac_EncodeFrameLen(WebRtc_Word16 framesamples, Bitstr *streamdata) {

  int frame_mode, status;

  status = 0;
  frame_mode = 0;
  /* entropy coding of frame length [1:480 samples,2:960 samples] */
  switch(framesamples) {
    case 480:
      frame_mode = 1;
      break;
    case 960:
      frame_mode = 2;
      break;
    default:
      status = - ISAC_DISALLOWED_FRAME_MODE_ENCODER;
  }

  if (status < 0)
    return status;

  WebRtcIsac_EncHistMulti(streamdata, &frame_mode, WebRtcIsac_kFrameLengthCdf_ptr, 1);

  return status;
}

/* cdf array for estimated bandwidth */
static const WebRtc_UWord16 kBwCdf[25] = {
  0, 2731, 5461, 8192, 10923, 13653, 16384, 19114, 21845, 24576, 27306, 30037,
  32768, 35498, 38229, 40959, 43690, 46421, 49151, 51882, 54613, 57343, 60074,
  62804, 65535};

/* pointer to cdf array for estimated bandwidth */
static const WebRtc_UWord16 *kBwCdfPtr[1] = { kBwCdf };

/* initial cdf index for decoder of estimated bandwidth*/
static const WebRtc_UWord16 kBwInitIndex[1] = { 7 };


int WebRtcIsac_DecodeSendBW(Bitstr *streamdata, WebRtc_Word16 *BWno) {

  int BWno32, err;

  /* entropy decoding of sender's BW estimation [0..23] */
  err = WebRtcIsac_DecHistOneStepMulti(&BWno32, streamdata, kBwCdfPtr, kBwInitIndex, 1);
  if (err<0)  // error check
    return -ISAC_RANGE_ERROR_DECODE_BANDWIDTH;
  *BWno = (WebRtc_Word16)BWno32;
  return err;

}

void WebRtcIsac_EncodeReceiveBw(int *BWno, Bitstr *streamdata) {

  /* entropy encoding of receiver's BW estimation [0..23] */
  WebRtcIsac_EncHistMulti(streamdata, BWno, kBwCdfPtr, 1);

}


/* estimate code length of LPC Coef */
void WebRtcIsac_TranscodeLPCCoef(double *LPCCoef_lo, double *LPCCoef_hi, int model,
                                 int *index_g) {

  int j, k, n, pos, pos2, posg, offsg, offs2;
  int index_ovr_g[KLT_ORDER_GAIN];
  double tmpcoeffs_g[KLT_ORDER_GAIN];
  double tmpcoeffs2_g[KLT_ORDER_GAIN];
  double sum;

  /* log gains, mean removal and scaling */
  posg = 0;
  for (k=0; k<SUBFRAMES; k++) {
    tmpcoeffs_g[posg] = log(LPCCoef_lo[(LPC_LOBAND_ORDER+1)*k]);
    tmpcoeffs_g[posg] -= WebRtcIsac_kLpcMeansGain[model][posg];
    tmpcoeffs_g[posg] *= LPC_GAIN_SCALE;
    posg++;
    tmpcoeffs_g[posg] = log(LPCCoef_hi[(LPC_HIBAND_ORDER+1)*k]);
    tmpcoeffs_g[posg] -= WebRtcIsac_kLpcMeansGain[model][posg];
    tmpcoeffs_g[posg] *= LPC_GAIN_SCALE;
    posg++;
  }

  /* KLT  */

  /* left transform */
  offsg = 0;
  for (j=0; j<SUBFRAMES; j++) {
    posg = offsg;
    for (k=0; k<LPC_GAIN_ORDER; k++) {
      sum = 0;
      pos = offsg;
      pos2 = k;
      for (n=0; n<LPC_GAIN_ORDER; n++) {
        sum += tmpcoeffs_g[pos++] * WebRtcIsac_kKltT1Gain[model][pos2];
        pos2 += LPC_GAIN_ORDER;
      }
      tmpcoeffs2_g[posg++] = sum;
    }
    offsg += LPC_GAIN_ORDER;
  }

  /* right transform */
  offsg = 0;
  offs2 = 0;
  for (j=0; j<SUBFRAMES; j++) {
    posg = offsg;
    for (k=0; k<LPC_GAIN_ORDER; k++) {
      sum = 0;
      pos = k;
      pos2 = offs2;
      for (n=0; n<SUBFRAMES; n++) {
        sum += tmpcoeffs2_g[pos] * WebRtcIsac_kKltT2Gain[model][pos2++];
        pos += LPC_GAIN_ORDER;
      }
      tmpcoeffs_g[posg++] = sum;
    }
    offs2 += SUBFRAMES;
    offsg += LPC_GAIN_ORDER;
  }


  /* quantize coefficients */
  for (k=0; k<KLT_ORDER_GAIN; k++) {

    /* get index */
    pos = WebRtcIsac_kQKltSelIndGain[k];
    pos2= WebRtcIsac_lrint(tmpcoeffs_g[pos] / KLT_STEPSIZE);
    index_g[k] = (pos2) + WebRtcIsac_kQKltQuantMinGain[k];
    if (index_g[k] < 0) {
      index_g[k] = 0;
    }
    else if (index_g[k] > WebRtcIsac_kQKltMaxIndGain[k]) {
      index_g[k] = WebRtcIsac_kQKltMaxIndGain[k];
    }
    index_ovr_g[k] = WebRtcIsac_kQKltOffsetGain[model][k]+index_g[k];

    /* find quantization levels for coefficients */
    tmpcoeffs_g[WebRtcIsac_kQKltSelIndGain[k]] = WebRtcIsac_kQKltLevelsGain[WebRtcIsac_kQKltOfLevelsGain[model]+index_ovr_g[k]];
  }
}


/* decode & dequantize LPC Coef */
int
WebRtcIsac_DecodeLpcCoefUB(
    Bitstr*     streamdata,
    double*     lpcVecs,
    double*     percepFilterGains,
    WebRtc_Word16 bandwidth)
{
  int  index_s[KLT_ORDER_SHAPE];

  double U[UB_LPC_ORDER * UB16_LPC_VEC_PER_FRAME];
  int err;

  /* entropy decoding of quantization indices */
  switch(bandwidth)
  {
    case isac12kHz:
      {
        err = WebRtcIsac_DecHistOneStepMulti(index_s, streamdata,
                                             WebRtcIsac_kLpcShapeCdfMatUb12, WebRtcIsac_kLpcShapeEntropySearchUb12,
                                             UB_LPC_ORDER * UB_LPC_VEC_PER_FRAME);
        break;
      }
    case isac16kHz:
      {
        err = WebRtcIsac_DecHistOneStepMulti(index_s, streamdata,
                                             WebRtcIsac_kLpcShapeCdfMatUb16, WebRtcIsac_kLpcShapeEntropySearchUb16,
                                             UB_LPC_ORDER * UB16_LPC_VEC_PER_FRAME);
        break;
      }
    default:
      return -1;
  }

  if (err<0)  // error check
  {
    return err;
  }

  WebRtcIsac_DequantizeLpcParam(index_s, lpcVecs, bandwidth);
  WebRtcIsac_CorrelateInterVec(lpcVecs, U, bandwidth);
  WebRtcIsac_CorrelateIntraVec(U, lpcVecs, bandwidth);
  WebRtcIsac_AddLarMean(lpcVecs, bandwidth);


  WebRtcIsac_DecodeLpcGainUb(percepFilterGains, streamdata);

  if(bandwidth == isac16kHz)
  {
    // decode another set of Gains
    WebRtcIsac_DecodeLpcGainUb(&percepFilterGains[SUBFRAMES], streamdata);
  }

  return 0;
}

WebRtc_Word16
WebRtcIsac_EncodeBandwidth(
    enum ISACBandwidth bandwidth,
    Bitstr*            streamData)
{
  int bandwidthMode;
  switch(bandwidth)
  {
    case isac12kHz:
      {
        bandwidthMode = 0;
        break;
      }
    case isac16kHz:
      {
        bandwidthMode = 1;
        break;
      }
    default:
      return -ISAC_DISALLOWED_ENCODER_BANDWIDTH;
  }

  WebRtcIsac_EncHistMulti(streamData, &bandwidthMode,
                          kOneBitEqualProbCdf_ptr, 1);
  return 0;
}

WebRtc_Word16
WebRtcIsac_DecodeBandwidth(
    Bitstr*             streamData,
    enum ISACBandwidth* bandwidth)
{
  int bandwidthMode;

  if(WebRtcIsac_DecHistOneStepMulti(&bandwidthMode, streamData,
                                    kOneBitEqualProbCdf_ptr,
                                    kOneBitEqualProbInitIndex, 1) < 0)
  {
    // error check
    return -ISAC_RANGE_ERROR_DECODE_BANDWITH;
  }

  switch(bandwidthMode)
  {
    case 0:
      {
        *bandwidth = isac12kHz;
        break;
      }
    case 1:
      {
        *bandwidth = isac16kHz;
        break;
      }
    default:
      return -ISAC_DISALLOWED_BANDWIDTH_MODE_DECODER;
  }
  return 0;
}

WebRtc_Word16
WebRtcIsac_EncodeJitterInfo(
    WebRtc_Word32 jitterIndex,
    Bitstr*     streamData)
{
  // This is to avoid LINUX warning until we change 'int' to
  // 'Word32'
  int intVar;

  if((jitterIndex < 0) || (jitterIndex > 1))
  {
    return -1;
  }
  intVar = (int)(jitterIndex);
  // Use the same CDF table as for bandwidth
  // both take two values with equal probability
  WebRtcIsac_EncHistMulti(streamData, &intVar,
                          kOneBitEqualProbCdf_ptr, 1);
  return 0;

}

WebRtc_Word16
WebRtcIsac_DecodeJitterInfo(
    Bitstr*      streamData,
    WebRtc_Word32* jitterInfo)
{
  int intVar;

  // Use the same CDF table as for bandwidth
  // both take two values with equal probability
  if(WebRtcIsac_DecHistOneStepMulti(&intVar, streamData,
                                    kOneBitEqualProbCdf_ptr,
                                    kOneBitEqualProbInitIndex, 1) < 0)
  {
    // error check
    return -ISAC_RANGE_ERROR_DECODE_BANDWITH;
  }
  *jitterInfo = (WebRtc_Word16)(intVar);
  return 0;
}
