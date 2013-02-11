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
 * This file contains all functions used to arithmetically
 * encode the iSAC bistream.
 *
 */

#include <stddef.h>

#include "arith_routins.h"
#include "spectrum_ar_model_tables.h"
#include "pitch_gain_tables.h"
#include "pitch_lag_tables.h"
#include "entropy_coding.h"
#include "lpc_tables.h"
#include "settings.h"
#include "signal_processing_library.h"


/*
  This function implements the fix-point correspondant function to lrint.

  FLP: (WebRtc_Word32)floor(flt+.499999999999)
  FIP: (fixVal+roundVal)>>qDomain

  where roundVal = 2^(qDomain-1) = 1<<(qDomain-1)

*/
static __inline WebRtc_Word32 CalcLrIntQ(WebRtc_Word32 fixVal, WebRtc_Word16 qDomain) {
  WebRtc_Word32 intgr;
  WebRtc_Word32 roundVal;

  roundVal = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)1, qDomain-1);
  intgr = WEBRTC_SPL_RSHIFT_W32(fixVal+roundVal, qDomain);

  return intgr;
}

/*
  __inline WebRtc_UWord32 stepwise(WebRtc_Word32 dinQ10) {

  WebRtc_Word32 ind, diQ10, dtQ10;

  diQ10 = dinQ10;
  if (diQ10 < DPMIN_Q10)
  diQ10 = DPMIN_Q10;
  if (diQ10 >= DPMAX_Q10)
  diQ10 = DPMAX_Q10 - 1;

  dtQ10 = diQ10 - DPMIN_Q10;*/ /* Q10 + Q10 = Q10 */
/* ind = (dtQ10 * 5) >> 10;  */ /* 2^10 / 5 = 0.2 in Q10  */
/* Q10 -> Q0 */

/* return rpointsFIX_Q10[ind];

   }
*/

/* logN(x) = logN(2)*log2(x) = 0.6931*log2(x). Output in Q8. */
/* The input argument X to logN(X) is 2^17 times higher than the
   input floating point argument Y to log(Y), since the X value
   is a Q17 value. This can be compensated for after the call, by
   subraction a value Z for each Q-step. One Q-step means that
   X gets 2 thimes higher, i.e. Z = logN(2)*256 = 0.693147180559*256 =
   177.445678 should be subtracted (since logN() returns a Q8 value).
   For a X value in Q17, the value 177.445678*17 = 3017 should be
   subtracted */
static WebRtc_Word16 CalcLogN(WebRtc_Word32 arg) {
  WebRtc_Word16 zeros, log2, frac, logN;

  zeros=WebRtcSpl_NormU32(arg);
  frac=(WebRtc_Word16)WEBRTC_SPL_RSHIFT_U32(WEBRTC_SPL_LSHIFT_W32(arg, zeros)&0x7FFFFFFF, 23);
  log2=(WebRtc_Word16)(WEBRTC_SPL_LSHIFT_W32(31-zeros, 8)+frac); // log2(x) in Q8
  logN=(WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(log2,22713,15); //Q8*Q15 log(2) = 0.693147 = 22713 in Q15
  logN=logN+11; //Scalar compensation which minimizes the (log(x)-logN(x))^2 error over all x.

  return logN;
}


/*
  expN(x) = 2^(a*x), where a = log2(e) ~= 1.442695

  Input:  Q8  (WebRtc_Word16)
  Output: Q17 (WebRtc_Word32)

  a = log2(e) = log2(exp(1)) ~= 1.442695  ==>  a = 23637 in Q14 (1.442688)
  To this value, 700 is added or subtracted in order to get an average error
  nearer zero, instead of always same-sign.
*/

static WebRtc_Word32 CalcExpN(WebRtc_Word16 x) {
  WebRtc_Word16 ax, axINT, axFRAC;
  WebRtc_Word16 exp16;
  WebRtc_Word32 exp;

  if (x>=0) {
    //  ax=(WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(x, 23637-700, 14); //Q8
    ax=(WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(x, 23637, 14); //Q8
    axINT = WEBRTC_SPL_RSHIFT_W16(ax, 8); //Q0
    axFRAC = ax&0x00FF;
    exp16 = WEBRTC_SPL_LSHIFT_W32(1, axINT); //Q0
    axFRAC = axFRAC+256; //Q8
    exp = WEBRTC_SPL_MUL_16_16(exp16, axFRAC); // Q0*Q8 = Q8
    exp = WEBRTC_SPL_LSHIFT_W32(exp, 9); //Q17
  } else {
    //  ax=(WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(x, 23637+700, 14); //Q8
    ax=(WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(x, 23637, 14); //Q8
    ax = -ax;
    axINT = 1 + WEBRTC_SPL_RSHIFT_W16(ax, 8); //Q0
    axFRAC = 0x00FF - (ax&0x00FF);
    exp16 = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(32768, axINT); //Q15
    axFRAC = axFRAC+256; //Q8
    exp = WEBRTC_SPL_MUL_16_16(exp16, axFRAC); // Q15*Q8 = Q23
    exp = WEBRTC_SPL_RSHIFT_W32(exp, 6); //Q17
  }

  return exp;
}


/* compute correlation from power spectrum */
static void CalcCorrelation(WebRtc_Word32 *PSpecQ12, WebRtc_Word32 *CorrQ7)
{
  WebRtc_Word32 summ[FRAMESAMPLES/8];
  WebRtc_Word32 diff[FRAMESAMPLES/8];
  WebRtc_Word32 sum;
  int k, n;

  for (k = 0; k < FRAMESAMPLES/8; k++) {
    summ[k] = WEBRTC_SPL_RSHIFT_W32(PSpecQ12[k] + PSpecQ12[FRAMESAMPLES/4-1 - k] + 16, 5);
    diff[k] = WEBRTC_SPL_RSHIFT_W32(PSpecQ12[k] - PSpecQ12[FRAMESAMPLES/4-1 - k] + 16, 5);
  }

  sum = 2;
  for (n = 0; n < FRAMESAMPLES/8; n++)
    sum += summ[n];
  CorrQ7[0] = sum;

  for (k = 0; k < AR_ORDER; k += 2) {
    sum = 0;
    for (n = 0; n < FRAMESAMPLES/8; n++)
      sum += WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(WebRtcIsacfix_kCos[k][n], diff[n]) + 256, 9);
    CorrQ7[k+1] = sum;
  }

  for (k=1; k<AR_ORDER; k+=2) {
    sum = 0;
    for (n = 0; n < FRAMESAMPLES/8; n++)
      sum += WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(WebRtcIsacfix_kCos[k][n], summ[n]) + 256, 9);
    CorrQ7[k+1] = sum;
  }
}


/* compute inverse AR power spectrum */
static void CalcInvArSpec(const WebRtc_Word16 *ARCoefQ12,
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
    sum += WEBRTC_SPL_MUL(ARCoefQ12[n], ARCoefQ12[n]);    /* Q24 */
  sum = WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(WEBRTC_SPL_RSHIFT_W32(sum, 6), 65) + 32768, 16);    /* result in Q8 */
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
      sum += WEBRTC_SPL_MUL(ARCoefQ12[n-k], ARCoefQ12[n]);  /* Q24 */
    sum = WEBRTC_SPL_RSHIFT_W32(sum, 15);
    CorrQ11[k] = WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(sum, tmpGain) + round, shftVal);
  }
  sum = WEBRTC_SPL_LSHIFT_W32(CorrQ11[0], 7);
  for (n = 0; n < FRAMESAMPLES/8; n++)
    CurveQ16[n] = sum;

  for (k = 1; k < AR_ORDER; k += 2) {
    for (n = 0; n < FRAMESAMPLES/8; n++)
      CurveQ16[n] += WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(WebRtcIsacfix_kCos[k][n], CorrQ11[k+1]) + 2, 2);
  }

  CS_ptrQ9 = WebRtcIsacfix_kCos[0];

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
    CS_ptrQ9 = WebRtcIsacfix_kCos[k];
    for (n = 0; n < FRAMESAMPLES/8; n++)
      diffQ16[n] += WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(CS_ptrQ9[n], WEBRTC_SPL_RSHIFT_W32(CorrQ11[k+1], shftVal)) + 2, 2);
  }

  for (k=0; k<FRAMESAMPLES/8; k++) {
    CurveQ16[FRAMESAMPLES/4-1 - k] = CurveQ16[k] - WEBRTC_SPL_LSHIFT_W32(diffQ16[k], shftVal);
    CurveQ16[k] += WEBRTC_SPL_LSHIFT_W32(diffQ16[k], shftVal);
  }
}

static void CalcRootInvArSpec(const WebRtc_Word16 *ARCoefQ12,
                              const WebRtc_Word32 gainQ10,
                              WebRtc_UWord16 *CurveQ8)
{
  WebRtc_Word32 CorrQ11[AR_ORDER+1];
  WebRtc_Word32 sum, tmpGain;
  WebRtc_Word32 summQ16[FRAMESAMPLES/8];
  WebRtc_Word32 diffQ16[FRAMESAMPLES/8];

  const WebRtc_Word16 *CS_ptrQ9;
  int k, n, i;
  WebRtc_Word16 round, shftVal = 0, sh;
  WebRtc_Word32 res, in_sqrt, newRes;

  sum = 0;
  for (n = 0; n < AR_ORDER+1; n++)
    sum += WEBRTC_SPL_MUL(ARCoefQ12[n], ARCoefQ12[n]);    /* Q24 */
  sum = WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(WEBRTC_SPL_RSHIFT_W32(sum, 6), 65) + 32768, 16);    /* result in Q8 */
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
      sum += WEBRTC_SPL_MUL(ARCoefQ12[n-k], ARCoefQ12[n]);  /* Q24 */
    sum = WEBRTC_SPL_RSHIFT_W32(sum, 15);
    CorrQ11[k] = WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(sum, tmpGain) + round, shftVal);
  }
  sum = WEBRTC_SPL_LSHIFT_W32(CorrQ11[0], 7);
  for (n = 0; n < FRAMESAMPLES/8; n++)
    summQ16[n] = sum;

  for (k = 1; k < (AR_ORDER); k += 2) {
    for (n = 0; n < FRAMESAMPLES/8; n++)
      summQ16[n] += WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_32_16(CorrQ11[k+1],WebRtcIsacfix_kCos[k][n]) + 2, 2);
  }

  CS_ptrQ9 = WebRtcIsacfix_kCos[0];

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
    CS_ptrQ9 = WebRtcIsacfix_kCos[k];
    for (n = 0; n < FRAMESAMPLES/8; n++)
      diffQ16[n] += WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(CS_ptrQ9[n], WEBRTC_SPL_RSHIFT_W32(CorrQ11[k+1], shftVal)) + 2, 2);
  }

  in_sqrt = summQ16[0] + WEBRTC_SPL_LSHIFT_W32(diffQ16[0], shftVal);

  /* convert to magnitude spectrum, by doing square-roots (modified from SPLIB)  */
  res = WEBRTC_SPL_LSHIFT_W32(1, WEBRTC_SPL_RSHIFT_W16(WebRtcSpl_GetSizeInBits(in_sqrt), 1));

  for (k = 0; k < FRAMESAMPLES/8; k++)
  {
    in_sqrt = summQ16[k] + WEBRTC_SPL_LSHIFT_W32(diffQ16[k], shftVal);
    i = 10;

    /* make in_sqrt positive to prohibit sqrt of negative values */
    if(in_sqrt<0)
      in_sqrt=-in_sqrt;

    newRes = WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_DIV(in_sqrt, res) + res, 1);
    do
    {
      res = newRes;
      newRes = WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_DIV(in_sqrt, res) + res, 1);
    } while (newRes != res && i-- > 0);

    CurveQ8[k] = (WebRtc_Word16)newRes;
  }
  for (k = FRAMESAMPLES/8; k < FRAMESAMPLES/4; k++) {

    in_sqrt = summQ16[FRAMESAMPLES/4-1 - k] - WEBRTC_SPL_LSHIFT_W32(diffQ16[FRAMESAMPLES/4-1 - k], shftVal);
    i = 10;

    /* make in_sqrt positive to prohibit sqrt of negative values */
    if(in_sqrt<0)
      in_sqrt=-in_sqrt;

    newRes = WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_DIV(in_sqrt, res) + res, 1);
    do
    {
      res = newRes;
      newRes = WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_DIV(in_sqrt, res) + res, 1);
    } while (newRes != res && i-- > 0);

    CurveQ8[k] = (WebRtc_Word16)newRes;
  }

}



/* generate array of dither samples in Q7 */
static void GenerateDitherQ7(WebRtc_Word16 *bufQ7,
                             WebRtc_UWord32 seed,
                             WebRtc_Word16 length,
                             WebRtc_Word16 AvgPitchGain_Q12)
{
  int   k;
  WebRtc_Word16 dither1_Q7, dither2_Q7, dither_gain_Q14, shft;

  if (AvgPitchGain_Q12 < 614)  /* this threshold should be equal to that in decode_spec() */
  {
    for (k = 0; k < length-2; k += 3)
    {
      /* new random unsigned WebRtc_Word32 */
      seed = WEBRTC_SPL_UMUL(seed, 196314165) + 907633515;

      /* fixed-point dither sample between -64 and 64 (Q7) */
      dither1_Q7 = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((WebRtc_Word32)seed + 16777216, 25); // * 128/4294967295

      /* new random unsigned WebRtc_Word32 */
      seed = WEBRTC_SPL_UMUL(seed, 196314165) + 907633515;

      /* fixed-point dither sample between -64 and 64 */
      dither2_Q7 = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(seed + 16777216, 25);

      shft = (WebRtc_Word16)(WEBRTC_SPL_RSHIFT_U32(seed, 25) & 15);
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
    dither_gain_Q14 = (WebRtc_Word16)(22528 - WEBRTC_SPL_MUL(10, AvgPitchGain_Q12));

    /* dither on half of the coefficients */
    for (k = 0; k < length-1; k += 2)
    {
      /* new random unsigned WebRtc_Word32 */
      seed = WEBRTC_SPL_UMUL(seed, 196314165) + 907633515;

      /* fixed-point dither sample between -64 and 64 */
      dither1_Q7 = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((WebRtc_Word32)seed + 16777216, 25);

      /* dither sample is placed in either even or odd index */
      shft = (WebRtc_Word16)(WEBRTC_SPL_RSHIFT_U32(seed, 25) & 1);     /* either 0 or 1 */

      bufQ7[k + shft] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(dither_gain_Q14, dither1_Q7) + 8192, 14);
      bufQ7[k + 1 - shft] = 0;
    }
  }
}




/*
 * function to decode the complex spectrum from the bitstream
 * returns the total number of bytes in the stream
 */
WebRtc_Word16 WebRtcIsacfix_DecodeSpec(Bitstr_dec *streamdata,
                                       WebRtc_Word16 *frQ7,
                                       WebRtc_Word16 *fiQ7,
                                       WebRtc_Word16 AvgPitchGain_Q12)
{
  WebRtc_Word16  data[FRAMESAMPLES];
  WebRtc_Word32  invARSpec2_Q16[FRAMESAMPLES/4];
  WebRtc_Word16  ARCoefQ12[AR_ORDER+1];
  WebRtc_Word16  RCQ15[AR_ORDER];
  WebRtc_Word16  gainQ10;
  WebRtc_Word32  gain2_Q10;
  WebRtc_Word16  len;
  int          k;

  /* create dither signal */
  GenerateDitherQ7(data, streamdata->W_upper, FRAMESAMPLES, AvgPitchGain_Q12); /* Dither is output in vector 'Data' */

  /* decode model parameters */
  if (WebRtcIsacfix_DecodeRcCoef(streamdata, RCQ15) < 0)
    return -ISAC_RANGE_ERROR_DECODE_SPECTRUM;


  WebRtcSpl_ReflCoefToLpc(RCQ15, AR_ORDER, ARCoefQ12);

  if (WebRtcIsacfix_DecodeGain2(streamdata, &gain2_Q10) < 0)
    return -ISAC_RANGE_ERROR_DECODE_SPECTRUM;

  /* compute inverse AR power spectrum */
  CalcInvArSpec(ARCoefQ12, gain2_Q10, invARSpec2_Q16);

  /* arithmetic decoding of spectrum */
  /* 'data' input and output. Input = Dither */
  len = WebRtcIsacfix_DecLogisticMulti2(data, streamdata, invARSpec2_Q16, (WebRtc_Word16)FRAMESAMPLES);

  if (len<1)
    return -ISAC_RANGE_ERROR_DECODE_SPECTRUM;

  /* subtract dither and scale down spectral samples with low SNR */
  if (AvgPitchGain_Q12 <= 614)
  {
    for (k = 0; k < FRAMESAMPLES; k += 4)
    {
      gainQ10 = WebRtcSpl_DivW32W16ResW16(WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)30, 10),
                                              (WebRtc_Word16)WEBRTC_SPL_RSHIFT_U32(invARSpec2_Q16[k>>2] + (WebRtc_UWord32)2195456, 16));
      *frQ7++ = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(data[ k ], gainQ10) + 512, 10);
      *fiQ7++ = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(data[k+1], gainQ10) + 512, 10);
      *frQ7++ = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(data[k+2], gainQ10) + 512, 10);
      *fiQ7++ = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(data[k+3], gainQ10) + 512, 10);
    }
  }
  else
  {
    for (k = 0; k < FRAMESAMPLES; k += 4)
    {
      gainQ10 = WebRtcSpl_DivW32W16ResW16(WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)36, 10),
                                              (WebRtc_Word16)WEBRTC_SPL_RSHIFT_U32(invARSpec2_Q16[k>>2] + (WebRtc_UWord32)2654208, 16));
      *frQ7++ = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(data[ k ], gainQ10) + 512, 10);
      *fiQ7++ = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(data[k+1], gainQ10) + 512, 10);
      *frQ7++ = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(data[k+2], gainQ10) + 512, 10);
      *fiQ7++ = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(data[k+3], gainQ10) + 512, 10);
    }
  }

  return len;
}


int WebRtcIsacfix_EncodeSpec(const WebRtc_Word16 *fr,
                             const WebRtc_Word16 *fi,
                             Bitstr_enc *streamdata,
                             WebRtc_Word16 AvgPitchGain_Q12)
{
  WebRtc_Word16  dataQ7[FRAMESAMPLES];
  WebRtc_Word32  PSpec[FRAMESAMPLES/4];
  WebRtc_UWord16 invARSpecQ8[FRAMESAMPLES/4];
  WebRtc_Word32  CorrQ7[AR_ORDER+1];
  WebRtc_Word32  CorrQ7_norm[AR_ORDER+1];
  WebRtc_Word16  RCQ15[AR_ORDER];
  WebRtc_Word16  ARCoefQ12[AR_ORDER+1];
  WebRtc_Word32  gain2_Q10;
  WebRtc_Word16  val;
  WebRtc_Word32  nrg;
  WebRtc_UWord32 sum;
  WebRtc_Word16  lft_shft;
  WebRtc_Word16  status;
  int          k, n, j;


  /* create dither_float signal */
  GenerateDitherQ7(dataQ7, streamdata->W_upper, FRAMESAMPLES, AvgPitchGain_Q12);

  /* add dither and quantize, and compute power spectrum */
  /* Vector dataQ7 contains Dither in Q7 */
  for (k = 0; k < FRAMESAMPLES; k += 4)
  {
    val = ((*fr++ + dataQ7[k]   + 64) & 0xFF80) - dataQ7[k]; /* Data = Dither */
    dataQ7[k] = val;            /* New value in Data */
    sum = WEBRTC_SPL_UMUL(val, val);

    val = ((*fi++ + dataQ7[k+1] + 64) & 0xFF80) - dataQ7[k+1]; /* Data = Dither */
    dataQ7[k+1] = val;            /* New value in Data */
    sum += WEBRTC_SPL_UMUL(val, val);

    val = ((*fr++ + dataQ7[k+2] + 64) & 0xFF80) - dataQ7[k+2]; /* Data = Dither */
    dataQ7[k+2] = val;            /* New value in Data */
    sum += WEBRTC_SPL_UMUL(val, val);

    val = ((*fi++ + dataQ7[k+3] + 64) & 0xFF80) - dataQ7[k+3]; /* Data = Dither */
    dataQ7[k+3] = val;            /* New value in Data */
    sum += WEBRTC_SPL_UMUL(val, val);

    PSpec[k>>2] = WEBRTC_SPL_RSHIFT_U32(sum, 2);
  }

  /* compute correlation from power spectrum */
  CalcCorrelation(PSpec, CorrQ7);


  /* find AR coefficients */
  /* number of bit shifts to 14-bit normalize CorrQ7[0] (leaving room for sign) */
  lft_shft = WebRtcSpl_NormW32(CorrQ7[0]) - 18;

  if (lft_shft > 0) {
    for (k=0; k<AR_ORDER+1; k++)
      CorrQ7_norm[k] = WEBRTC_SPL_LSHIFT_W32(CorrQ7[k], lft_shft);
  } else {
    for (k=0; k<AR_ORDER+1; k++)
      CorrQ7_norm[k] = WEBRTC_SPL_RSHIFT_W32(CorrQ7[k], -lft_shft);
  }

  /* find RC coefficients */
  WebRtcSpl_AutoCorrToReflCoef(CorrQ7_norm, AR_ORDER, RCQ15);

  /* quantize & code RC Coef */
  status = WebRtcIsacfix_EncodeRcCoef(RCQ15, streamdata);
  if (status < 0) {
    return status;
  }

  /* RC -> AR coefficients */
  WebRtcSpl_ReflCoefToLpc(RCQ15, AR_ORDER, ARCoefQ12);

  /* compute ARCoef' * Corr * ARCoef in Q19 */
  nrg = 0;
  for (j = 0; j <= AR_ORDER; j++) {
    for (n = 0; n <= j; n++)
      nrg += WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(ARCoefQ12[j], WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(CorrQ7_norm[j-n], ARCoefQ12[n]) + 256, 9)) + 4, 3);
    for (n = j+1; n <= AR_ORDER; n++)
      nrg += WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(ARCoefQ12[j], WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL(CorrQ7_norm[n-j], ARCoefQ12[n]) + 256, 9)) + 4, 3);
  }

  if (lft_shft > 0)
    nrg = WEBRTC_SPL_RSHIFT_W32(nrg, lft_shft);
  else
    nrg = WEBRTC_SPL_LSHIFT_W32(nrg, -lft_shft);

  if(nrg>131072)
    gain2_Q10 = WebRtcSpl_DivResultInQ31(FRAMESAMPLES >> 2, nrg);  /* also shifts 31 bits to the left! */
  else
    gain2_Q10 = WEBRTC_SPL_RSHIFT_W32(FRAMESAMPLES, 2);

  /* quantize & code gain2_Q10 */
  if (WebRtcIsacfix_EncodeGain2(&gain2_Q10, streamdata))
    return -1;

  /* compute inverse AR magnitude spectrum */
  CalcRootInvArSpec(ARCoefQ12, gain2_Q10, invARSpecQ8);


  /* arithmetic coding of spectrum */
  status = WebRtcIsacfix_EncLogisticMulti2(streamdata, dataQ7, invARSpecQ8, (WebRtc_Word16)FRAMESAMPLES);
  if ( status )
    return( status );

  return 0;
}


/* Matlab's LAR definition */
static void Rc2LarFix(const WebRtc_Word16 *rcQ15, WebRtc_Word32 *larQ17, WebRtc_Word16 order) {

  /*

    This is a piece-wise implemenetation of a rc2lar-function (all values in the comment
    are Q15 values and  are based on [0 24956/32768 30000/32768 32500/32768], i.e.
    [0.76159667968750   0.91552734375000   0.99182128906250]

    x0  x1           a                 k              x0(again)         b
    ==================================================================================
    0.00 0.76:   0                  2.625997508581   0                  0
    0.76 0.91:   2.000012018559     7.284502668663   0.761596679688    -3.547841027073
    0.91 0.99:   3.121320351712    31.115835041229   0.915527343750   -25.366077452148
    0.99 1.00:   5.495270168700   686.663805654056   0.991821289063  -675.552510708011

    The implementation is y(x)= a + (x-x0)*k, but this can be simplified to

    y(x) = a-x0*k + x*k = b + x*k, where b = a-x0*k

    akx=[0                 2.625997508581   0
    2.000012018559     7.284502668663   0.761596679688
    3.121320351712    31.115835041229   0.915527343750
    5.495270168700   686.663805654056   0.991821289063];

    b = akx(:,1) - akx(:,3).*akx(:,2)

    [ 0.0
    -3.547841027073
    -25.366077452148
    -675.552510708011]

  */

  int k;
  WebRtc_Word16 rc;
  WebRtc_Word32 larAbsQ17;

  for (k = 0; k < order; k++) {

    rc = WEBRTC_SPL_ABS_W16(rcQ15[k]); //Q15

    /* Calculate larAbsQ17 in Q17 from rc in Q15 */

    if (rc<24956) {  //0.7615966 in Q15
      // (Q15*Q13)>>11 = Q17
      larAbsQ17 = WEBRTC_SPL_MUL_16_16_RSFT(rc, 21512, 11);
    } else if (rc<30000) { //0.91552734375 in Q15
      // Q17 + (Q15*Q12)>>10 = Q17
      larAbsQ17 = -465024 + WEBRTC_SPL_MUL_16_16_RSFT(rc, 29837, 10);
    } else if (rc<32500) { //0.99182128906250 in Q15
      // Q17 + (Q15*Q10)>>8 = Q17
      larAbsQ17 = -3324784 + WEBRTC_SPL_MUL_16_16_RSFT(rc, 31863, 8);
    } else  {
      // Q17 + (Q15*Q5)>>3 = Q17
      larAbsQ17 = -88546020 + WEBRTC_SPL_MUL_16_16_RSFT(rc, 21973, 3);
    }

    if (rcQ15[k]>0) {
      larQ17[k] = larAbsQ17;
    } else {
      larQ17[k] = -larAbsQ17;
    }
  }
}


static void Lar2RcFix(const WebRtc_Word32 *larQ17, WebRtc_Word16 *rcQ15,  WebRtc_Word16 order) {

  /*
    This is a piece-wise implemenetation of a lar2rc-function
    See comment in Rc2LarFix() about details.
  */

  int k;
  WebRtc_Word16 larAbsQ11;
  WebRtc_Word32 rc;

  for (k = 0; k < order; k++) {

    larAbsQ11 = (WebRtc_Word16) WEBRTC_SPL_ABS_W32(WEBRTC_SPL_RSHIFT_W32(larQ17[k]+32,6)); //Q11

    if (larAbsQ11<4097) { //2.000012018559 in Q11
      // Q11*Q16>>12 = Q15
      rc = WEBRTC_SPL_MUL_16_16_RSFT(larAbsQ11, 24957, 12);
    } else if (larAbsQ11<6393) { //3.121320351712 in Q11
      // (Q11*Q17 + Q13)>>13 = Q15
      rc = WEBRTC_SPL_RSHIFT_W32((WEBRTC_SPL_MUL_16_16(larAbsQ11, 17993) + 130738688), 13);
    } else if (larAbsQ11<11255) { //5.495270168700 in Q11
      // (Q11*Q19 + Q30)>>15 = Q15
      rc = WEBRTC_SPL_RSHIFT_W32((WEBRTC_SPL_MUL_16_16(larAbsQ11, 16850) + 875329820), 15);
    } else  {
      // (Q11*Q24>>16 + Q19)>>4 = Q15
      rc = WEBRTC_SPL_RSHIFT_W32(((WEBRTC_SPL_MUL_16_16_RSFT(larAbsQ11, 24433, 16)) + 515804), 4);
    }

    if (larQ17[k]<=0) {
      rc = -rc;
    }

    rcQ15[k] = (WebRtc_Word16) rc;  // Q15
  }
}

static void Poly2LarFix(WebRtc_Word16 *lowbandQ15,
                        WebRtc_Word16 orderLo,
                        WebRtc_Word16 *hibandQ15,
                        WebRtc_Word16 orderHi,
                        WebRtc_Word16 Nsub,
                        WebRtc_Word32 *larsQ17) {

  int k, n;
  WebRtc_Word32 *outpQ17;
  WebRtc_Word16 orderTot;
  WebRtc_Word32 larQ17[MAX_ORDER];   // Size 7+6 is enough

  orderTot = (orderLo + orderHi);
  outpQ17 = larsQ17;
  for (k = 0; k < Nsub; k++) {

    Rc2LarFix(lowbandQ15, larQ17, orderLo);

    for (n = 0; n < orderLo; n++)
      outpQ17[n] = larQ17[n]; //Q17

    Rc2LarFix(hibandQ15, larQ17, orderHi);

    for (n = 0; n < orderHi; n++)
      outpQ17[n + orderLo] = larQ17[n]; //Q17;

    outpQ17 += orderTot;
    lowbandQ15 += orderLo;
    hibandQ15 += orderHi;
  }
}


static void Lar2polyFix(WebRtc_Word32 *larsQ17,
                        WebRtc_Word16 *lowbandQ15,
                        WebRtc_Word16 orderLo,
                        WebRtc_Word16 *hibandQ15,
                        WebRtc_Word16 orderHi,
                        WebRtc_Word16 Nsub) {

  int k, n;
  WebRtc_Word16 orderTot;
  WebRtc_Word16 *outplQ15, *outphQ15;
  WebRtc_Word32 *inpQ17;
  WebRtc_Word16 rcQ15[7+6];

  orderTot = (orderLo + orderHi);
  outplQ15 = lowbandQ15;
  outphQ15 = hibandQ15;
  inpQ17 = larsQ17;
  for (k = 0; k < Nsub; k++) {

    /* gains not handled here as in the FLP version */

    /* Low band */
    Lar2RcFix(&inpQ17[0], rcQ15, orderLo);
    for (n = 0; n < orderLo; n++)
      outplQ15[n] = rcQ15[n]; // Refl. coeffs

    /* High band */
    Lar2RcFix(&inpQ17[orderLo], rcQ15, orderHi);
    for (n = 0; n < orderHi; n++)
      outphQ15[n] = rcQ15[n]; // Refl. coeffs

    inpQ17 += orderTot;
    outplQ15 += orderLo;
    outphQ15 += orderHi;
  }
}

int WebRtcIsacfix_DecodeLpc(WebRtc_Word32 *gain_lo_hiQ17,
                            WebRtc_Word16 *LPCCoef_loQ15,
                            WebRtc_Word16 *LPCCoef_hiQ15,
                            Bitstr_dec *streamdata,
                            WebRtc_Word16 *outmodel) {

  WebRtc_Word32 larsQ17[KLT_ORDER_SHAPE]; // KLT_ORDER_GAIN+KLT_ORDER_SHAPE == (ORDERLO+ORDERHI)*SUBFRAMES
  int err;

  err = WebRtcIsacfix_DecodeLpcCoef(streamdata, larsQ17, gain_lo_hiQ17, outmodel);
  if (err<0)  // error check
    return -ISAC_RANGE_ERROR_DECODE_LPC;

  Lar2polyFix(larsQ17, LPCCoef_loQ15, ORDERLO, LPCCoef_hiQ15, ORDERHI, SUBFRAMES);

  return 0;
}

/* decode & dequantize LPC Coef */
int WebRtcIsacfix_DecodeLpcCoef(Bitstr_dec *streamdata,
                                WebRtc_Word32 *LPCCoefQ17,
                                WebRtc_Word32 *gain_lo_hiQ17,
                                WebRtc_Word16 *outmodel)
{
  int j, k, n;
  int err;
  WebRtc_Word16 pos, pos2, posg, poss, offsg, offss, offs2;
  WebRtc_Word16 gainpos;
  WebRtc_Word16 model;
  WebRtc_Word16 index_QQ[KLT_ORDER_SHAPE];
  WebRtc_Word32 tmpcoeffs_gQ17[KLT_ORDER_GAIN];
  WebRtc_Word32 tmpcoeffs2_gQ21[KLT_ORDER_GAIN];
  WebRtc_Word16 tmpcoeffs_sQ10[KLT_ORDER_SHAPE];
  WebRtc_Word32 tmpcoeffs_sQ17[KLT_ORDER_SHAPE];
  WebRtc_Word32 tmpcoeffs2_sQ18[KLT_ORDER_SHAPE];
  WebRtc_Word32 sumQQ;
  WebRtc_Word16 sumQQ16;
  WebRtc_Word32 tmp32;



  /* entropy decoding of model number */
  err = WebRtcIsacfix_DecHistOneStepMulti(&model, streamdata, WebRtcIsacfix_kModelCdfPtr, WebRtcIsacfix_kModelInitIndex, 1);
  if (err<0)  // error check
    return err;

  /* entropy decoding of quantization indices */
  err = WebRtcIsacfix_DecHistOneStepMulti(index_QQ, streamdata, WebRtcIsacfix_kCdfShapePtr[model], WebRtcIsacfix_kInitIndexShape[model], KLT_ORDER_SHAPE);
  if (err<0)  // error check
    return err;
  /* find quantization levels for coefficients */
  for (k=0; k<KLT_ORDER_SHAPE; k++) {
    tmpcoeffs_sQ10[WebRtcIsacfix_kSelIndShape[k]] = WebRtcIsacfix_kLevelsShapeQ10[WebRtcIsacfix_kOfLevelsShape[model]+WebRtcIsacfix_kOffsetShape[model][k] + index_QQ[k]];
  }

  err = WebRtcIsacfix_DecHistOneStepMulti(index_QQ, streamdata, WebRtcIsacfix_kCdfGainPtr[model], WebRtcIsacfix_kInitIndexGain[model], KLT_ORDER_GAIN);
  if (err<0)  // error check
    return err;
  /* find quantization levels for coefficients */
  for (k=0; k<KLT_ORDER_GAIN; k++) {
    tmpcoeffs_gQ17[WebRtcIsacfix_kSelIndGain[k]] = WebRtcIsacfix_kLevelsGainQ17[WebRtcIsacfix_kOfLevelsGain[model]+ WebRtcIsacfix_kOffsetGain[model][k] + index_QQ[k]];
  }


  /* inverse KLT  */

  /* left transform */  // Transpose matrix!
  offsg = 0;
  offss = 0;
  posg = 0;
  poss = 0;
  for (j=0; j<SUBFRAMES; j++) {
    offs2 = 0;
    for (k=0; k<2; k++) {
      sumQQ = 0;
      pos = offsg;
      pos2 = offs2;
      for (n=0; n<2; n++) {
        sumQQ += (WEBRTC_SPL_MUL_16_32_RSFT16(WebRtcIsacfix_kT1GainQ15[model][pos2], tmpcoeffs_gQ17[pos]<<5)); // (Q15*Q17)>>(16-5) = Q21
        pos++;
        pos2++;
      }
      tmpcoeffs2_gQ21[posg] = sumQQ; //Q21
      posg++;
      offs2 += 2;
    }
    offs2 = 0;

    for (k=0; k<LPC_SHAPE_ORDER; k++) {
      sumQQ = 0;
      pos = offss;
      pos2 = offs2;
      for (n=0; n<LPC_SHAPE_ORDER; n++) {
        sumQQ += WEBRTC_SPL_MUL_16_16_RSFT(tmpcoeffs_sQ10[pos], WebRtcIsacfix_kT1ShapeQ15[model][pos2], 7); // (Q10*Q15)>>7 = Q18
        pos++;
        pos2++;
      }
      tmpcoeffs2_sQ18[poss] = sumQQ; //Q18
      poss++;
      offs2 += LPC_SHAPE_ORDER;
    }
    offsg += 2;
    offss += LPC_SHAPE_ORDER;
  }

  /* right transform */ // Transpose matrix
  offsg = 0;
  offss = 0;
  posg = 0;
  poss = 0;
  for (j=0; j<SUBFRAMES; j++) {
    posg = offsg;
    for (k=0; k<2; k++) {
      sumQQ = 0;
      pos = k;
      pos2 = j;
      for (n=0; n<SUBFRAMES; n++) {
        sumQQ += WEBRTC_SPL_LSHIFT_W32(WEBRTC_SPL_MUL_16_32_RSFT16(WebRtcIsacfix_kT2GainQ15[model][pos2], tmpcoeffs2_gQ21[pos]), 1); // (Q15*Q21)>>(16-1) = Q21
        pos += 2;
        pos2 += SUBFRAMES;

      }
      tmpcoeffs_gQ17[posg] = WEBRTC_SPL_RSHIFT_W32(sumQQ, 4);
      posg++;
    }
    poss = offss;
    for (k=0; k<LPC_SHAPE_ORDER; k++) {
      sumQQ = 0;
      pos = k;
      pos2 = j;
      for (n=0; n<SUBFRAMES; n++) {
        sumQQ += (WEBRTC_SPL_MUL_16_32_RSFT16(WebRtcIsacfix_kT2ShapeQ15[model][pos2], tmpcoeffs2_sQ18[pos])); // (Q15*Q18)>>16 = Q17
        pos += LPC_SHAPE_ORDER;
        pos2 += SUBFRAMES;
      }
      tmpcoeffs_sQ17[poss] = sumQQ;
      poss++;
    }
    offsg += 2;
    offss += LPC_SHAPE_ORDER;
  }

  /* scaling, mean addition, and gain restoration */
  gainpos = 0;
  posg = 0;poss = 0;pos=0;
  for (k=0; k<SUBFRAMES; k++) {

    /* log gains */
    sumQQ16 = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(tmpcoeffs_gQ17[posg], 2+9); //Divide by 4 and get Q17 to Q8, i.e. shift 2+9
    sumQQ16 += WebRtcIsacfix_kMeansGainQ8[model][posg];
    sumQQ = CalcExpN(sumQQ16); // Q8 in and Q17 out
    gain_lo_hiQ17[gainpos] = sumQQ; //Q17
    gainpos++;
    posg++;

    sumQQ16 = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(tmpcoeffs_gQ17[posg], 2+9); //Divide by 4 and get Q17 to Q8, i.e. shift 2+9
    sumQQ16 += WebRtcIsacfix_kMeansGainQ8[model][posg];
    sumQQ = CalcExpN(sumQQ16); // Q8 in and Q17 out
    gain_lo_hiQ17[gainpos] = sumQQ; //Q17
    gainpos++;
    posg++;

    /* lo band LAR coeffs */
    for (n=0; n<ORDERLO; n++, pos++, poss++) {
      tmp32 = WEBRTC_SPL_MUL_16_32_RSFT16(31208, tmpcoeffs_sQ17[poss]); // (Q16*Q17)>>16 = Q17, with 1/2.1 = 0.47619047619 ~= 31208 in Q16
      tmp32 = tmp32 + WebRtcIsacfix_kMeansShapeQ17[model][poss]; // Q17+Q17 = Q17
      LPCCoefQ17[pos] = tmp32;
    }

    /* hi band LAR coeffs */
    for (n=0; n<ORDERHI; n++, pos++, poss++) {
      tmp32 = WEBRTC_SPL_LSHIFT_W32(WEBRTC_SPL_MUL_16_32_RSFT16(18204, tmpcoeffs_sQ17[poss]), 3); // ((Q13*Q17)>>16)<<3 = Q17, with 1/0.45 = 2.222222222222 ~= 18204 in Q13
      tmp32 = tmp32 + WebRtcIsacfix_kMeansShapeQ17[model][poss]; // Q17+Q17 = Q17
      LPCCoefQ17[pos] = tmp32;
    }
  }


  *outmodel=model;

  return 0;
}

/* estimate codel length of LPC Coef */
static int EstCodeLpcCoef(WebRtc_Word32 *LPCCoefQ17,
                          WebRtc_Word32 *gain_lo_hiQ17,
                          WebRtc_Word16 *model,
                          WebRtc_Word32 *sizeQ11,
                          Bitstr_enc *streamdata,
                          ISAC_SaveEncData_t* encData,
                          transcode_obj *transcodingParam) {
  int j, k, n;
  WebRtc_Word16 posQQ, pos2QQ, gainpos;
  WebRtc_Word16  pos, pos2, poss, posg, offsg, offss, offs2;
  WebRtc_Word16 index_gQQ[KLT_ORDER_GAIN], index_sQQ[KLT_ORDER_SHAPE];
  WebRtc_Word16 index_ovr_gQQ[KLT_ORDER_GAIN], index_ovr_sQQ[KLT_ORDER_SHAPE];
  WebRtc_Word32 BitsQQ;

  WebRtc_Word16 tmpcoeffs_gQ6[KLT_ORDER_GAIN];
  WebRtc_Word32 tmpcoeffs_gQ17[KLT_ORDER_GAIN];
  WebRtc_Word32 tmpcoeffs_sQ17[KLT_ORDER_SHAPE];
  WebRtc_Word32 tmpcoeffs2_gQ21[KLT_ORDER_GAIN];
  WebRtc_Word32 tmpcoeffs2_sQ17[KLT_ORDER_SHAPE];
  WebRtc_Word32 sumQQ;
  WebRtc_Word32 tmp32;
  WebRtc_Word16 sumQQ16;
  int status = 0;

  /* write LAR coefficients to statistics file */
  /* Save data for creation of multiple bitstreams (and transcoding) */
  if (encData != NULL) {
    for (k=0; k<KLT_ORDER_GAIN; k++) {
      encData->LPCcoeffs_g[KLT_ORDER_GAIN*encData->startIdx + k] = gain_lo_hiQ17[k];
    }
  }

  /* log gains, mean removal and scaling */
  posg = 0;poss = 0;pos=0; gainpos=0;

  for (k=0; k<SUBFRAMES; k++) {
    /* log gains */

    /* The input argument X to logN(X) is 2^17 times higher than the
       input floating point argument Y to log(Y), since the X value
       is a Q17 value. This can be compensated for after the call, by
       subraction a value Z for each Q-step. One Q-step means that
       X gets 2 times higher, i.e. Z = logN(2)*256 = 0.693147180559*256 =
       177.445678 should be subtracted (since logN() returns a Q8 value).
       For a X value in Q17, the value 177.445678*17 = 3017 should be
       subtracted */
    tmpcoeffs_gQ6[posg] = CalcLogN(gain_lo_hiQ17[gainpos])-3017; //Q8
    tmpcoeffs_gQ6[posg] -= WebRtcIsacfix_kMeansGainQ8[0][posg]; //Q8, but Q6 after not-needed mult. by 4
    posg++; gainpos++;

    tmpcoeffs_gQ6[posg] = CalcLogN(gain_lo_hiQ17[gainpos])-3017; //Q8
    tmpcoeffs_gQ6[posg] -= WebRtcIsacfix_kMeansGainQ8[0][posg]; //Q8, but Q6 after not-needed mult. by 4
    posg++; gainpos++;

    /* lo band LAR coeffs */
    for (n=0; n<ORDERLO; n++, poss++, pos++) {
      tmp32 = LPCCoefQ17[pos] - WebRtcIsacfix_kMeansShapeQ17[0][poss]; //Q17
      tmp32 = WEBRTC_SPL_MUL_16_32_RSFT16(17203, tmp32<<3); // tmp32 = 2.1*tmp32
      tmpcoeffs_sQ17[poss] = tmp32; //Q17
    }

    /* hi band LAR coeffs */
    for (n=0; n<ORDERHI; n++, poss++, pos++) {
      tmp32 = LPCCoefQ17[pos] - WebRtcIsacfix_kMeansShapeQ17[0][poss]; //Q17
      tmp32 = WEBRTC_SPL_MUL_16_32_RSFT16(14746, tmp32<<1); // tmp32 = 0.45*tmp32
      tmpcoeffs_sQ17[poss] = tmp32; //Q17
    }

  }


  /* KLT  */

  /* left transform */
  offsg = 0;
  offss = 0;
  for (j=0; j<SUBFRAMES; j++) {
    posg = offsg;
    for (k=0; k<2; k++) {
      sumQQ = 0;
      pos = offsg;
      pos2 = k;
      for (n=0; n<2; n++) {
        sumQQ += WEBRTC_SPL_MUL_16_16(tmpcoeffs_gQ6[pos], WebRtcIsacfix_kT1GainQ15[0][pos2]); //Q21 = Q6*Q15
        pos++;
        pos2 += 2;
      }
      tmpcoeffs2_gQ21[posg] = sumQQ;
      posg++;
    }
    poss = offss;
    for (k=0; k<LPC_SHAPE_ORDER; k++) {
      sumQQ = 0;
      pos = offss;
      pos2 = k;
      for (n=0; n<LPC_SHAPE_ORDER; n++) {
        sumQQ += (WEBRTC_SPL_MUL_16_32_RSFT16(WebRtcIsacfix_kT1ShapeQ15[0][pos2], tmpcoeffs_sQ17[pos]<<1)); // (Q15*Q17)>>(16-1) = Q17
        pos++;
        pos2 += LPC_SHAPE_ORDER;
      }
      tmpcoeffs2_sQ17[poss] = sumQQ; //Q17
      poss++;
    }
    offsg += 2;
    offss += LPC_SHAPE_ORDER;
  }

  /* right transform */
  offsg = 0;
  offss = 0;
  offs2 = 0;
  for (j=0; j<SUBFRAMES; j++) {
    posg = offsg;
    for (k=0; k<2; k++) {
      sumQQ = 0;
      pos = k;
      pos2 = offs2;
      for (n=0; n<SUBFRAMES; n++) {
        sumQQ += WEBRTC_SPL_LSHIFT_W32(WEBRTC_SPL_MUL_16_32_RSFT16(WebRtcIsacfix_kT2GainQ15[0][pos2], tmpcoeffs2_gQ21[pos]), 1); // (Q15*Q21)>>(16-1) = Q21
        pos += 2;
        pos2++;
      }
      tmpcoeffs_gQ17[posg] = WEBRTC_SPL_RSHIFT_W32(sumQQ, 4);
      posg++;
    }
    poss = offss;
    for (k=0; k<LPC_SHAPE_ORDER; k++) {
      sumQQ = 0;
      pos = k;
      pos2 = offs2;
      for (n=0; n<SUBFRAMES; n++) {
        sumQQ += (WEBRTC_SPL_MUL_16_32_RSFT16(WebRtcIsacfix_kT2ShapeQ15[0][pos2], tmpcoeffs2_sQ17[pos]<<1)); // (Q15*Q17)>>(16-1) = Q17
        pos += LPC_SHAPE_ORDER;
        pos2++;
      }
      tmpcoeffs_sQ17[poss] = sumQQ;
      poss++;
    }
    offs2 += SUBFRAMES;
    offsg += 2;
    offss += LPC_SHAPE_ORDER;
  }

  /* quantize coefficients */

  BitsQQ = 0;
  for (k=0; k<KLT_ORDER_GAIN; k++) //ATTN: ok?
  {
    posQQ = WebRtcIsacfix_kSelIndGain[k];
    pos2QQ= (WebRtc_Word16)CalcLrIntQ(tmpcoeffs_gQ17[posQQ], 17);

    index_gQQ[k] = pos2QQ + WebRtcIsacfix_kQuantMinGain[k]; //ATTN: ok?
    if (index_gQQ[k] < 0) {
      index_gQQ[k] = 0;
    }
    else if (index_gQQ[k] > WebRtcIsacfix_kMaxIndGain[k]) {
      index_gQQ[k] = WebRtcIsacfix_kMaxIndGain[k];
    }
    index_ovr_gQQ[k] = WebRtcIsacfix_kOffsetGain[0][k]+index_gQQ[k];
    posQQ = WebRtcIsacfix_kOfLevelsGain[0] + index_ovr_gQQ[k];

    /* Save data for creation of multiple bitstreams */
    if (encData != NULL) {
      encData->LPCindex_g[KLT_ORDER_GAIN*encData->startIdx + k] = index_gQQ[k];
    }

    /* determine number of bits */
    sumQQ = WebRtcIsacfix_kCodeLenGainQ11[posQQ]; //Q11
    BitsQQ += sumQQ;
  }

  for (k=0; k<KLT_ORDER_SHAPE; k++) //ATTN: ok?
  {
    index_sQQ[k] = (WebRtc_Word16)(CalcLrIntQ(tmpcoeffs_sQ17[WebRtcIsacfix_kSelIndShape[k]], 17) + WebRtcIsacfix_kQuantMinShape[k]); //ATTN: ok?

    if (index_sQQ[k] < 0)
      index_sQQ[k] = 0;
    else if (index_sQQ[k] > WebRtcIsacfix_kMaxIndShape[k])
      index_sQQ[k] = WebRtcIsacfix_kMaxIndShape[k];
    index_ovr_sQQ[k] = WebRtcIsacfix_kOffsetShape[0][k]+index_sQQ[k];

    posQQ = WebRtcIsacfix_kOfLevelsShape[0] + index_ovr_sQQ[k];
    sumQQ = WebRtcIsacfix_kCodeLenShapeQ11[posQQ]; //Q11
    BitsQQ += sumQQ;
  }



  *model = 0;
  *sizeQ11=BitsQQ;

  /* entropy coding of model number */
  status = WebRtcIsacfix_EncHistMulti(streamdata, model, WebRtcIsacfix_kModelCdfPtr, 1);
  if (status < 0) {
    return status;
  }

  /* entropy coding of quantization indices - shape only */
  status = WebRtcIsacfix_EncHistMulti(streamdata, index_sQQ, WebRtcIsacfix_kCdfShapePtr[0], KLT_ORDER_SHAPE);
  if (status < 0) {
    return status;
  }

  /* Save data for creation of multiple bitstreams */
  if (encData != NULL) {
    for (k=0; k<KLT_ORDER_SHAPE; k++)
    {
      encData->LPCindex_s[KLT_ORDER_SHAPE*encData->startIdx + k] = index_sQQ[k];
    }
  }
  /* save the state of the bitstream object 'streamdata' for the possible bit-rate reduction */
  transcodingParam->full         = streamdata->full;
  transcodingParam->stream_index = streamdata->stream_index;
  transcodingParam->streamval    = streamdata->streamval;
  transcodingParam->W_upper      = streamdata->W_upper;
  transcodingParam->beforeLastWord     = streamdata->stream[streamdata->stream_index-1];
  transcodingParam->lastWord     = streamdata->stream[streamdata->stream_index];

  /* entropy coding of index */
  status = WebRtcIsacfix_EncHistMulti(streamdata, index_gQQ, WebRtcIsacfix_kCdfGainPtr[0], KLT_ORDER_GAIN);
  if (status < 0) {
    return status;
  }

  /* find quantization levels for shape coefficients */
  for (k=0; k<KLT_ORDER_SHAPE; k++) {
    tmpcoeffs_sQ17[WebRtcIsacfix_kSelIndShape[k]] = WEBRTC_SPL_MUL(128, WebRtcIsacfix_kLevelsShapeQ10[WebRtcIsacfix_kOfLevelsShape[0]+index_ovr_sQQ[k]]);

  }
  /* inverse KLT  */

  /* left transform */  // Transpose matrix!
  offss = 0;
  poss = 0;
  for (j=0; j<SUBFRAMES; j++) {
    offs2 = 0;
    for (k=0; k<LPC_SHAPE_ORDER; k++) {
      sumQQ = 0;
      pos = offss;
      pos2 = offs2;
      for (n=0; n<LPC_SHAPE_ORDER; n++) {
        sumQQ += (WEBRTC_SPL_MUL_16_32_RSFT16(WebRtcIsacfix_kT1ShapeQ15[0][pos2], tmpcoeffs_sQ17[pos]<<1)); // (Q15*Q17)>>(16-1) = Q17
        pos++;
        pos2++;
      }
      tmpcoeffs2_sQ17[poss] = sumQQ;

      poss++;
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
      sumQQ = 0;
      pos = k;
      pos2 = j;
      for (n=0; n<SUBFRAMES; n++) {
        sumQQ += (WEBRTC_SPL_MUL_16_32_RSFT16(WebRtcIsacfix_kT2ShapeQ15[0][pos2], tmpcoeffs2_sQ17[pos]<<1)); // (Q15*Q17)>>(16-1) = Q17
        pos += LPC_SHAPE_ORDER;
        pos2 += SUBFRAMES;
      }
      tmpcoeffs_sQ17[poss] = sumQQ;
      poss++;
    }
    offss += LPC_SHAPE_ORDER;
  }

  /* scaling, mean addition, and gain restoration */
  poss = 0;pos=0;
  for (k=0; k<SUBFRAMES; k++) {

    /* lo band LAR coeffs */
    for (n=0; n<ORDERLO; n++, pos++, poss++) {
      tmp32 = WEBRTC_SPL_MUL_16_32_RSFT16(31208, tmpcoeffs_sQ17[poss]); // (Q16*Q17)>>16 = Q17, with 1/2.1 = 0.47619047619 ~= 31208 in Q16
      tmp32 = tmp32 + WebRtcIsacfix_kMeansShapeQ17[0][poss]; // Q17+Q17 = Q17
      LPCCoefQ17[pos] = tmp32;
    }

    /* hi band LAR coeffs */
    for (n=0; n<ORDERHI; n++, pos++, poss++) {
      tmp32 = WEBRTC_SPL_LSHIFT_W32(WEBRTC_SPL_MUL_16_32_RSFT16(18204, tmpcoeffs_sQ17[poss]), 3); // ((Q13*Q17)>>16)<<3 = Q17, with 1/0.45 = 2.222222222222 ~= 18204 in Q13
      tmp32 = tmp32 + WebRtcIsacfix_kMeansShapeQ17[0][poss]; // Q17+Q17 = Q17
      LPCCoefQ17[pos] = tmp32;
    }

  }

  //to update tmpcoeffs_gQ17 to the proper state
  for (k=0; k<KLT_ORDER_GAIN; k++) {
    tmpcoeffs_gQ17[WebRtcIsacfix_kSelIndGain[k]] = WebRtcIsacfix_kLevelsGainQ17[WebRtcIsacfix_kOfLevelsGain[0]+index_ovr_gQQ[k]];
  }



  /* find quantization levels for coefficients */

  /* left transform */
  offsg = 0;
  posg = 0;
  for (j=0; j<SUBFRAMES; j++) {
    offs2 = 0;
    for (k=0; k<2; k++) {
      sumQQ = 0;
      pos = offsg;
      pos2 = offs2;
      for (n=0; n<2; n++) {
        sumQQ += (WEBRTC_SPL_MUL_16_32_RSFT16(WebRtcIsacfix_kT1GainQ15[0][pos2], tmpcoeffs_gQ17[pos])<<1); // (Q15*Q17)>>(16-1) = Q17
        pos++;
        pos2++;
      }
      tmpcoeffs2_gQ21[posg] = WEBRTC_SPL_LSHIFT_W32(sumQQ, 4); //Q17<<4 = Q21
      posg++;
      offs2 += 2;
    }
    offsg += 2;
  }

  /* right transform */ // Transpose matrix
  offsg = 0;
  posg = 0;
  for (j=0; j<SUBFRAMES; j++) {
    posg = offsg;
    for (k=0; k<2; k++) {
      sumQQ = 0;
      pos = k;
      pos2 = j;
      for (n=0; n<SUBFRAMES; n++) {
        sumQQ += WEBRTC_SPL_LSHIFT_W32(WEBRTC_SPL_MUL_16_32_RSFT16(WebRtcIsacfix_kT2GainQ15[0][pos2], tmpcoeffs2_gQ21[pos]), 1); // (Q15*Q21)>>(16-1) = Q21
        pos += 2;
        pos2 += SUBFRAMES;
      }
      tmpcoeffs_gQ17[posg] = WEBRTC_SPL_RSHIFT_W32(sumQQ, 4);
      posg++;
    }
    offsg += 2;
  }

  /* scaling, mean addition, and gain restoration */
  posg = 0;
  gainpos = 0;
  for (k=0; k<2*SUBFRAMES; k++) {

    sumQQ16 = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(tmpcoeffs_gQ17[posg], 2+9); //Divide by 4 and get Q17 to Q8, i.e. shift 2+9
    sumQQ16 += WebRtcIsacfix_kMeansGainQ8[0][posg];
    sumQQ = CalcExpN(sumQQ16); // Q8 in and Q17 out
    gain_lo_hiQ17[gainpos] = sumQQ; //Q17

    gainpos++;
    pos++;posg++;
  }

  return 0;
}

int WebRtcIsacfix_EstCodeLpcGain(WebRtc_Word32 *gain_lo_hiQ17,
                                 Bitstr_enc *streamdata,
                                 ISAC_SaveEncData_t* encData) {
  int j, k, n;
  WebRtc_Word16 posQQ, pos2QQ, gainpos;
  WebRtc_Word16  pos, pos2, posg, offsg, offs2;
  WebRtc_Word16 index_gQQ[KLT_ORDER_GAIN];

  WebRtc_Word16 tmpcoeffs_gQ6[KLT_ORDER_GAIN];
  WebRtc_Word32 tmpcoeffs_gQ17[KLT_ORDER_GAIN];
  WebRtc_Word32 tmpcoeffs2_gQ21[KLT_ORDER_GAIN];
  WebRtc_Word32 sumQQ;
  int status = 0;

  /* write LAR coefficients to statistics file */
  /* Save data for creation of multiple bitstreams (and transcoding) */
  if (encData != NULL) {
    for (k=0; k<KLT_ORDER_GAIN; k++) {
      encData->LPCcoeffs_g[KLT_ORDER_GAIN*encData->startIdx + k] = gain_lo_hiQ17[k];
    }
  }

  /* log gains, mean removal and scaling */
  posg = 0; pos = 0; gainpos = 0;

  for (k=0; k<SUBFRAMES; k++) {
    /* log gains */

    /* The input argument X to logN(X) is 2^17 times higher than the
       input floating point argument Y to log(Y), since the X value
       is a Q17 value. This can be compensated for after the call, by
       subraction a value Z for each Q-step. One Q-step means that
       X gets 2 times higher, i.e. Z = logN(2)*256 = 0.693147180559*256 =
       177.445678 should be subtracted (since logN() returns a Q8 value).
       For a X value in Q17, the value 177.445678*17 = 3017 should be
       subtracted */
    tmpcoeffs_gQ6[posg] = CalcLogN(gain_lo_hiQ17[gainpos])-3017; //Q8
    tmpcoeffs_gQ6[posg] -= WebRtcIsacfix_kMeansGainQ8[0][posg]; //Q8, but Q6 after not-needed mult. by 4
    posg++; gainpos++;

    tmpcoeffs_gQ6[posg] = CalcLogN(gain_lo_hiQ17[gainpos])-3017; //Q8
    tmpcoeffs_gQ6[posg] -= WebRtcIsacfix_kMeansGainQ8[0][posg]; //Q8, but Q6 after not-needed mult. by 4
    posg++; gainpos++;
  }


  /* KLT  */

  /* left transform */
  offsg = 0;
  for (j=0; j<SUBFRAMES; j++) {
    posg = offsg;
    for (k=0; k<2; k++) {
      sumQQ = 0;
      pos = offsg;
      pos2 = k;
      for (n=0; n<2; n++) {
        sumQQ += WEBRTC_SPL_MUL_16_16(tmpcoeffs_gQ6[pos], WebRtcIsacfix_kT1GainQ15[0][pos2]); //Q21 = Q6*Q15
        pos++;
        pos2 += 2;
      }
      tmpcoeffs2_gQ21[posg] = sumQQ;
      posg++;
    }
    offsg += 2;
  }

  /* right transform */
  offsg = 0;
  offs2 = 0;
  for (j=0; j<SUBFRAMES; j++) {
    posg = offsg;
    for (k=0; k<2; k++) {
      sumQQ = 0;
      pos = k;
      pos2 = offs2;
      for (n=0; n<SUBFRAMES; n++) {
        sumQQ += WEBRTC_SPL_LSHIFT_W32(WEBRTC_SPL_MUL_16_32_RSFT16(WebRtcIsacfix_kT2GainQ15[0][pos2], tmpcoeffs2_gQ21[pos]), 1); // (Q15*Q21)>>(16-1) = Q21
        pos += 2;
        pos2++;
      }
      tmpcoeffs_gQ17[posg] = WEBRTC_SPL_RSHIFT_W32(sumQQ, 4);
      posg++;
    }
    offsg += 2;
    offs2 += SUBFRAMES;
  }

  /* quantize coefficients */

  for (k=0; k<KLT_ORDER_GAIN; k++) //ATTN: ok?
  {
    posQQ = WebRtcIsacfix_kSelIndGain[k];
    pos2QQ= (WebRtc_Word16)CalcLrIntQ(tmpcoeffs_gQ17[posQQ], 17);

    index_gQQ[k] = pos2QQ + WebRtcIsacfix_kQuantMinGain[k]; //ATTN: ok?
    if (index_gQQ[k] < 0) {
      index_gQQ[k] = 0;
    }
    else if (index_gQQ[k] > WebRtcIsacfix_kMaxIndGain[k]) {
      index_gQQ[k] = WebRtcIsacfix_kMaxIndGain[k];
    }

    /* Save data for creation of multiple bitstreams */
    if (encData != NULL) {
      encData->LPCindex_g[KLT_ORDER_GAIN*encData->startIdx + k] = index_gQQ[k];
    }
  }

  /* entropy coding of index */
  status = WebRtcIsacfix_EncHistMulti(streamdata, index_gQQ, WebRtcIsacfix_kCdfGainPtr[0], KLT_ORDER_GAIN);
  if (status < 0) {
    return status;
  }

  return 0;
}


int WebRtcIsacfix_EncodeLpc(WebRtc_Word32 *gain_lo_hiQ17,
                            WebRtc_Word16 *LPCCoef_loQ15,
                            WebRtc_Word16 *LPCCoef_hiQ15,
                            WebRtc_Word16 *model,
                            WebRtc_Word32 *sizeQ11,
                            Bitstr_enc *streamdata,
                            ISAC_SaveEncData_t* encData,
                            transcode_obj *transcodeParam)
{
  int status = 0;
  WebRtc_Word32 larsQ17[KLT_ORDER_SHAPE]; // KLT_ORDER_SHAPE == (ORDERLO+ORDERHI)*SUBFRAMES
  // = (6+12)*6 == 108

  Poly2LarFix(LPCCoef_loQ15, ORDERLO, LPCCoef_hiQ15, ORDERHI, SUBFRAMES, larsQ17);

  status = EstCodeLpcCoef(larsQ17, gain_lo_hiQ17, model, sizeQ11, streamdata, encData, transcodeParam);
  if (status < 0) {
    return (status);
  }

  Lar2polyFix(larsQ17, LPCCoef_loQ15, ORDERLO, LPCCoef_hiQ15, ORDERHI, SUBFRAMES);

  return 0;
}


/* decode & dequantize RC */
int WebRtcIsacfix_DecodeRcCoef(Bitstr_dec *streamdata, WebRtc_Word16 *RCQ15)
{
  int k, err;
  WebRtc_Word16 index[AR_ORDER];

  /* entropy decoding of quantization indices */
  err = WebRtcIsacfix_DecHistOneStepMulti(index, streamdata, WebRtcIsacfix_kRcCdfPtr, WebRtcIsacfix_kRcInitInd, AR_ORDER);
  if (err<0)  // error check
    return err;

  /* find quantization levels for reflection coefficients */
  for (k=0; k<AR_ORDER; k++)
  {
    RCQ15[k] = *(WebRtcIsacfix_kRcLevPtr[k] + index[k]);
  }

  return 0;
}



/* quantize & code RC */
int WebRtcIsacfix_EncodeRcCoef(WebRtc_Word16 *RCQ15, Bitstr_enc *streamdata)
{
  int k;
  WebRtc_Word16 index[AR_ORDER];
  int status;

  /* quantize reflection coefficients (add noise feedback?) */
  for (k=0; k<AR_ORDER; k++)
  {
    index[k] = WebRtcIsacfix_kRcInitInd[k];

    if (RCQ15[k] > WebRtcIsacfix_kRcBound[index[k]])
    {
      while (RCQ15[k] > WebRtcIsacfix_kRcBound[index[k] + 1])
        index[k]++;
    }
    else
    {
      while (RCQ15[k] < WebRtcIsacfix_kRcBound[--index[k]]) ;
    }

    RCQ15[k] = *(WebRtcIsacfix_kRcLevPtr[k] + index[k]);
  }


  /* entropy coding of quantization indices */
  status = WebRtcIsacfix_EncHistMulti(streamdata, index, WebRtcIsacfix_kRcCdfPtr, AR_ORDER);

  /* If error in WebRtcIsacfix_EncHistMulti(), status will be negative, otherwise 0 */
  return status;
}


/* decode & dequantize squared Gain */
int WebRtcIsacfix_DecodeGain2(Bitstr_dec *streamdata, WebRtc_Word32 *gainQ10)
{
  int err;
  WebRtc_Word16 index;

  /* entropy decoding of quantization index */
  err = WebRtcIsacfix_DecHistOneStepMulti(
      &index,
      streamdata,
      WebRtcIsacfix_kGainPtr,
      WebRtcIsacfix_kGainInitInd,
      1);
  /* error check */
  if (err<0) {
    return err;
  }

  /* find quantization level */
  *gainQ10 = WebRtcIsacfix_kGain2Lev[index];

  return 0;
}



/* quantize & code squared Gain */
int WebRtcIsacfix_EncodeGain2(WebRtc_Word32 *gainQ10, Bitstr_enc *streamdata)
{
  WebRtc_Word16 index;
  int status = 0;

  /* find quantization index */
  index = WebRtcIsacfix_kGainInitInd[0];
  if (*gainQ10 > WebRtcIsacfix_kGain2Bound[index])
  {
    while (*gainQ10 > WebRtcIsacfix_kGain2Bound[index + 1])
      index++;
  }
  else
  {
    while (*gainQ10 < WebRtcIsacfix_kGain2Bound[--index]) ;
  }

  /* dequantize */
  *gainQ10 = WebRtcIsacfix_kGain2Lev[index];

  /* entropy coding of quantization index */
  status = WebRtcIsacfix_EncHistMulti(streamdata, &index, WebRtcIsacfix_kGainPtr, 1);

  /* If error in WebRtcIsacfix_EncHistMulti(), status will be negative, otherwise 0 */
  return status;
}


/* code and decode Pitch Gains and Lags functions */

/* decode & dequantize Pitch Gains */
int WebRtcIsacfix_DecodePitchGain(Bitstr_dec *streamdata, WebRtc_Word16 *PitchGains_Q12)
{
  int err;
  WebRtc_Word16 index_comb;
  const WebRtc_UWord16 *pitch_gain_cdf_ptr[1];

  /* entropy decoding of quantization indices */
  *pitch_gain_cdf_ptr = WebRtcIsacfix_kPitchGainCdf;
  err = WebRtcIsacfix_DecHistBisectMulti(&index_comb, streamdata, pitch_gain_cdf_ptr, WebRtcIsacfix_kCdfTableSizeGain, 1);
  /* error check, Q_mean_Gain.. tables are of size 144 */
  if ((err < 0) || (index_comb < 0) || (index_comb >= 144))
    return -ISAC_RANGE_ERROR_DECODE_PITCH_GAIN;

  /* unquantize back to pitch gains by table look-up */
  PitchGains_Q12[0] = WebRtcIsacfix_kPitchGain1[index_comb];
  PitchGains_Q12[1] = WebRtcIsacfix_kPitchGain2[index_comb];
  PitchGains_Q12[2] = WebRtcIsacfix_kPitchGain3[index_comb];
  PitchGains_Q12[3] = WebRtcIsacfix_kPitchGain4[index_comb];

  return 0;
}


/* quantize & code Pitch Gains */
int WebRtcIsacfix_EncodePitchGain(WebRtc_Word16 *PitchGains_Q12, Bitstr_enc *streamdata, ISAC_SaveEncData_t* encData)
{
  int k,j;
  WebRtc_Word16 SQ15[PITCH_SUBFRAMES];
  WebRtc_Word16 index[3];
  WebRtc_Word16 index_comb;
  const WebRtc_UWord16 *pitch_gain_cdf_ptr[1];
  WebRtc_Word32 CQ17;
  int status = 0;


  /* get the approximate arcsine (almost linear)*/
  for (k=0; k<PITCH_SUBFRAMES; k++)
    SQ15[k] = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16_RSFT(PitchGains_Q12[k],33,2); //Q15


  /* find quantization index; only for the first three transform coefficients */
  for (k=0; k<3; k++)
  {
    /*  transform */
    CQ17=0;
    for (j=0; j<PITCH_SUBFRAMES; j++) {
      CQ17 += WEBRTC_SPL_MUL_16_16_RSFT(WebRtcIsacfix_kTransform[k][j], SQ15[j],10); // Q17
    }

    index[k] = (WebRtc_Word16)((CQ17 + 8192)>>14); // Rounding and scaling with stepsize (=1/0.125=8)

    /* check that the index is not outside the boundaries of the table */
    if (index[k] < WebRtcIsacfix_kLowerlimiGain[k]) index[k] = WebRtcIsacfix_kLowerlimiGain[k];
    else if (index[k] > WebRtcIsacfix_kUpperlimitGain[k]) index[k] = WebRtcIsacfix_kUpperlimitGain[k];
    index[k] -= WebRtcIsacfix_kLowerlimiGain[k];
  }

  /* calculate unique overall index */
  index_comb = (WebRtc_Word16)(WEBRTC_SPL_MUL(WebRtcIsacfix_kMultsGain[0], index[0]) +
                               WEBRTC_SPL_MUL(WebRtcIsacfix_kMultsGain[1], index[1]) + index[2]);

  /* unquantize back to pitch gains by table look-up */
  // (Y)
  PitchGains_Q12[0] = WebRtcIsacfix_kPitchGain1[index_comb];
  PitchGains_Q12[1] = WebRtcIsacfix_kPitchGain2[index_comb];
  PitchGains_Q12[2] = WebRtcIsacfix_kPitchGain3[index_comb];
  PitchGains_Q12[3] = WebRtcIsacfix_kPitchGain4[index_comb];


  /* entropy coding of quantization pitch gains */
  *pitch_gain_cdf_ptr = WebRtcIsacfix_kPitchGainCdf;
  status = WebRtcIsacfix_EncHistMulti(streamdata, &index_comb, pitch_gain_cdf_ptr, 1);
  if (status < 0) {
    return status;
  }

  /* Save data for creation of multiple bitstreams */
  if (encData != NULL) {
    encData->pitchGain_index[encData->startIdx] = index_comb;
  }

  return 0;
}



/* Pitch LAG */


/* decode & dequantize Pitch Lags */
int WebRtcIsacfix_DecodePitchLag(Bitstr_dec *streamdata,
                                 WebRtc_Word16 *PitchGain_Q12,
                                 WebRtc_Word16 *PitchLags_Q7)
{
  int k, err;
  WebRtc_Word16 index[PITCH_SUBFRAMES];
  const WebRtc_Word16 *mean_val2Q10, *mean_val4Q10;

  const WebRtc_Word16 *lower_limit;
  const WebRtc_UWord16 *init_index;
  const WebRtc_UWord16 *cdf_size;
  const WebRtc_UWord16 **cdf;

  WebRtc_Word32 meangainQ12;
  WebRtc_Word32 CQ11, CQ10,tmp32a,tmp32b;
  WebRtc_Word16 shft,tmp16a,tmp16c;

  meangainQ12=0;
  for (k = 0; k < 4; k++)
    meangainQ12 += PitchGain_Q12[k];

  meangainQ12 = WEBRTC_SPL_RSHIFT_W32(meangainQ12, 2);  // Get average

  /* voicing classificiation */
  if (meangainQ12 <= 819) {                 // mean_gain < 0.2
    shft = -1;        // StepSize=2.0;
    cdf = WebRtcIsacfix_kPitchLagPtrLo;
    cdf_size = WebRtcIsacfix_kPitchLagSizeLo;
    mean_val2Q10 = WebRtcIsacfix_kMeanLag2Lo;
    mean_val4Q10 = WebRtcIsacfix_kMeanLag4Lo;
    lower_limit = WebRtcIsacfix_kLowerLimitLo;
    init_index = WebRtcIsacfix_kInitIndLo;
  } else if (meangainQ12 <= 1638) {            // mean_gain < 0.4
    shft = 0;        // StepSize=1.0;
    cdf = WebRtcIsacfix_kPitchLagPtrMid;
    cdf_size = WebRtcIsacfix_kPitchLagSizeMid;
    mean_val2Q10 = WebRtcIsacfix_kMeanLag2Mid;
    mean_val4Q10 = WebRtcIsacfix_kMeanLag4Mid;
    lower_limit = WebRtcIsacfix_kLowerLimitMid;
    init_index = WebRtcIsacfix_kInitIndMid;
  } else {
    shft = 1;        // StepSize=0.5;
    cdf = WebRtcIsacfix_kPitchLagPtrHi;
    cdf_size = WebRtcIsacfix_kPitchLagSizeHi;
    mean_val2Q10 = WebRtcIsacfix_kMeanLag2Hi;
    mean_val4Q10 = WebRtcIsacfix_kMeanLag4Hi;
    lower_limit = WebRtcIsacfix_kLowerLimitHi;
    init_index = WebRtcIsacfix_kInitIndHi;
  }

  /* entropy decoding of quantization indices */
  err = WebRtcIsacfix_DecHistBisectMulti(index, streamdata, cdf, cdf_size, 1);
  if ((err<0) || (index[0]<0))  // error check
    return -ISAC_RANGE_ERROR_DECODE_PITCH_LAG;

  err = WebRtcIsacfix_DecHistOneStepMulti(index+1, streamdata, cdf+1, init_index, 3);
  if (err<0)  // error check
    return -ISAC_RANGE_ERROR_DECODE_PITCH_LAG;


  /* unquantize back to transform coefficients and do the inverse transform: S = T'*C */
  CQ11 = ((WebRtc_Word32)index[0] + lower_limit[0]);  // Q0
  CQ11 = WEBRTC_SPL_SHIFT_W32(CQ11,11-shft); // Scale with StepSize, Q11
  for (k=0; k<PITCH_SUBFRAMES; k++) {
    tmp32a =  WEBRTC_SPL_MUL_16_32_RSFT11(WebRtcIsacfix_kTransform[0][k], CQ11);
    tmp16a = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(tmp32a, 5);
    PitchLags_Q7[k] = tmp16a;
  }

  CQ10 = mean_val2Q10[index[1]];
  for (k=0; k<PITCH_SUBFRAMES; k++) {
    tmp32b =  (WebRtc_Word32) WEBRTC_SPL_MUL_16_16_RSFT((WebRtc_Word16) WebRtcIsacfix_kTransform[1][k], (WebRtc_Word16) CQ10,10);
    tmp16c = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(tmp32b, 5);
    PitchLags_Q7[k] += tmp16c;
  }

  CQ10 = mean_val4Q10[index[3]];
  for (k=0; k<PITCH_SUBFRAMES; k++) {
    tmp32b =  (WebRtc_Word32) WEBRTC_SPL_MUL_16_16_RSFT((WebRtc_Word16) WebRtcIsacfix_kTransform[3][k], (WebRtc_Word16) CQ10,10);
    tmp16c = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(tmp32b, 5);
    PitchLags_Q7[k] += tmp16c;
  }

  return 0;
}



/* quantize & code Pitch Lags */
int WebRtcIsacfix_EncodePitchLag(WebRtc_Word16 *PitchLagsQ7,WebRtc_Word16 *PitchGain_Q12,
                                 Bitstr_enc *streamdata, ISAC_SaveEncData_t* encData)
{
  int k, j;
  WebRtc_Word16 index[PITCH_SUBFRAMES];
  WebRtc_Word32 meangainQ12, CQ17;
  WebRtc_Word32 CQ11, CQ10,tmp32a;

  const WebRtc_Word16 *mean_val2Q10,*mean_val4Q10;
  const WebRtc_Word16 *lower_limit, *upper_limit;
  const WebRtc_UWord16 **cdf;
  WebRtc_Word16 shft, tmp16a, tmp16b, tmp16c;
  WebRtc_Word32 tmp32b;
  int status = 0;

  /* compute mean pitch gain */
  meangainQ12=0;
  for (k = 0; k < 4; k++)
    meangainQ12 += PitchGain_Q12[k];

  meangainQ12 = WEBRTC_SPL_RSHIFT_W32(meangainQ12, 2);

  /* Save data for creation of multiple bitstreams */
  if (encData != NULL) {
    encData->meanGain[encData->startIdx] = meangainQ12;
  }

  /* voicing classificiation */
  if (meangainQ12 <= 819) {                 // mean_gain < 0.2
    shft = -1;        // StepSize=2.0;
    cdf = WebRtcIsacfix_kPitchLagPtrLo;
    mean_val2Q10 = WebRtcIsacfix_kMeanLag2Lo;
    mean_val4Q10 = WebRtcIsacfix_kMeanLag4Lo;
    lower_limit = WebRtcIsacfix_kLowerLimitLo;
    upper_limit = WebRtcIsacfix_kUpperLimitLo;
  } else if (meangainQ12 <= 1638) {            // mean_gain < 0.4
    shft = 0;        // StepSize=1.0;
    cdf = WebRtcIsacfix_kPitchLagPtrMid;
    mean_val2Q10 = WebRtcIsacfix_kMeanLag2Mid;
    mean_val4Q10 = WebRtcIsacfix_kMeanLag4Mid;
    lower_limit = WebRtcIsacfix_kLowerLimitMid;
    upper_limit = WebRtcIsacfix_kUpperLimitMid;
  } else {
    shft = 1;        // StepSize=0.5;
    cdf = WebRtcIsacfix_kPitchLagPtrHi;
    mean_val2Q10 = WebRtcIsacfix_kMeanLag2Hi;
    mean_val4Q10 = WebRtcIsacfix_kMeanLag4Hi;
    lower_limit = WebRtcIsacfix_kLowerLimitHi;
    upper_limit = WebRtcIsacfix_kUpperLimitHi;
  }

  /* find quantization index */
  for (k=0; k<4; k++)
  {
    /*  transform */
    CQ17=0;
    for (j=0; j<PITCH_SUBFRAMES; j++)
      CQ17 += WEBRTC_SPL_MUL_16_16_RSFT(WebRtcIsacfix_kTransform[k][j], PitchLagsQ7[j],2); // Q17

    CQ17 = WEBRTC_SPL_SHIFT_W32(CQ17,shft); // Scale with StepSize

    /* quantize */
    tmp16b = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(CQ17 + 65536, 17 );
    index[k] =  tmp16b;

    /* check that the index is not outside the boundaries of the table */
    if (index[k] < lower_limit[k]) index[k] = lower_limit[k];
    else if (index[k] > upper_limit[k]) index[k] = upper_limit[k];
    index[k] -= lower_limit[k];

    /* Save data for creation of multiple bitstreams */
    if(encData != NULL) {
      encData->pitchIndex[PITCH_SUBFRAMES*encData->startIdx + k] = index[k];
    }
  }

  /* unquantize back to transform coefficients and do the inverse transform: S = T'*C */
  CQ11 = (index[0] + lower_limit[0]);  // Q0
  CQ11 = WEBRTC_SPL_SHIFT_W32(CQ11,11-shft); // Scale with StepSize, Q11

  for (k=0; k<PITCH_SUBFRAMES; k++) {
    tmp32a =  WEBRTC_SPL_MUL_16_32_RSFT11(WebRtcIsacfix_kTransform[0][k], CQ11); // Q12
    tmp16a = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(tmp32a, 5);// Q7
    PitchLagsQ7[k] = tmp16a;
  }

  CQ10 = mean_val2Q10[index[1]];
  for (k=0; k<PITCH_SUBFRAMES; k++) {
    tmp32b =  (WebRtc_Word32) WEBRTC_SPL_MUL_16_16_RSFT((WebRtc_Word16) WebRtcIsacfix_kTransform[1][k], (WebRtc_Word16) CQ10,10);
    tmp16c = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(tmp32b, 5); // Q7
    PitchLagsQ7[k] += tmp16c;
  }

  CQ10 = mean_val4Q10[index[3]];
  for (k=0; k<PITCH_SUBFRAMES; k++) {
    tmp32b =  (WebRtc_Word32) WEBRTC_SPL_MUL_16_16_RSFT((WebRtc_Word16) WebRtcIsacfix_kTransform[3][k], (WebRtc_Word16) CQ10,10);
    tmp16c = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(tmp32b, 5); // Q7
    PitchLagsQ7[k] += tmp16c;
  }

  /* entropy coding of quantization pitch lags */
  status = WebRtcIsacfix_EncHistMulti(streamdata, index, cdf, PITCH_SUBFRAMES);

  /* If error in WebRtcIsacfix_EncHistMulti(), status will be negative, otherwise 0 */
  return status;
}



/* Routines for inband signaling of bandwitdh estimation */
/* Histograms based on uniform distribution of indices */
/* Move global variables later! */


/* cdf array for frame length indicator */
const WebRtc_UWord16 kFrameLenCdf[4] = {
  0, 21845, 43690, 65535};

/* pointer to cdf array for frame length indicator */
const WebRtc_UWord16 *kFrameLenCdfPtr[1] = {kFrameLenCdf};

/* initial cdf index for decoder of frame length indicator */
const WebRtc_UWord16 kFrameLenInitIndex[1] = {1};


int WebRtcIsacfix_DecodeFrameLen(Bitstr_dec *streamdata,
                                 WebRtc_Word16 *framesamples)
{

  int err;
  WebRtc_Word16 frame_mode;

  err = 0;
  /* entropy decoding of frame length [1:30ms,2:60ms] */
  err = WebRtcIsacfix_DecHistOneStepMulti(&frame_mode, streamdata, kFrameLenCdfPtr, kFrameLenInitIndex, 1);
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


int WebRtcIsacfix_EncodeFrameLen(WebRtc_Word16 framesamples, Bitstr_enc *streamdata) {

  int status;
  WebRtc_Word16 frame_mode;

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

  status = WebRtcIsacfix_EncHistMulti(streamdata, &frame_mode, kFrameLenCdfPtr, 1);

  return status;
}

/* cdf array for estimated bandwidth */
const WebRtc_UWord16 kBwCdf[25] = {
  0, 2731, 5461, 8192, 10923, 13653, 16384, 19114, 21845, 24576, 27306, 30037,
  32768, 35498, 38229, 40959, 43690, 46421, 49151, 51882, 54613, 57343, 60074,
  62804, 65535};

/* pointer to cdf array for estimated bandwidth */
const WebRtc_UWord16 *kBwCdfPtr[1] = {kBwCdf};

/* initial cdf index for decoder of estimated bandwidth*/
const WebRtc_UWord16 kBwInitIndex[1] = {7};


int WebRtcIsacfix_DecodeSendBandwidth(Bitstr_dec *streamdata, WebRtc_Word16 *BWno) {

  int err;
  WebRtc_Word16 BWno32;

  /* entropy decoding of sender's BW estimation [0..23] */
  err = WebRtcIsacfix_DecHistOneStepMulti(&BWno32, streamdata, kBwCdfPtr, kBwInitIndex, 1);
  if (err<0)  // error check
    return -ISAC_RANGE_ERROR_DECODE_BANDWIDTH;
  *BWno = (WebRtc_Word16)BWno32;
  return err;

}


int WebRtcIsacfix_EncodeReceiveBandwidth(WebRtc_Word16 *BWno, Bitstr_enc *streamdata)
{
  int status = 0;
  /* entropy encoding of receiver's BW estimation [0..23] */
  status = WebRtcIsacfix_EncHistMulti(streamdata, BWno, kBwCdfPtr, 1);

  return status;
}

/* estimate codel length of LPC Coef */
void WebRtcIsacfix_TranscodeLpcCoef(WebRtc_Word32 *gain_lo_hiQ17,
                                    WebRtc_Word16 *index_gQQ) {
  int j, k, n;
  WebRtc_Word16 posQQ, pos2QQ;
  WebRtc_Word16  pos, pos2, posg, offsg, offs2, gainpos;
  WebRtc_Word32 tmpcoeffs_gQ6[KLT_ORDER_GAIN];
  WebRtc_Word32 tmpcoeffs_gQ17[KLT_ORDER_GAIN];
  WebRtc_Word32 tmpcoeffs2_gQ21[KLT_ORDER_GAIN];
  WebRtc_Word32 sumQQ;


  /* log gains, mean removal and scaling */
  posg = 0;pos=0; gainpos=0;

  for (k=0; k<SUBFRAMES; k++) {
    /* log gains */

    /* The input argument X to logN(X) is 2^17 times higher than the
       input floating point argument Y to log(Y), since the X value
       is a Q17 value. This can be compensated for after the call, by
       subraction a value Z for each Q-step. One Q-step means that
       X gets 2 times higher, i.e. Z = logN(2)*256 = 0.693147180559*256 =
       177.445678 should be subtracted (since logN() returns a Q8 value).
       For a X value in Q17, the value 177.445678*17 = 3017 should be
       subtracted */
    tmpcoeffs_gQ6[posg] = CalcLogN(gain_lo_hiQ17[gainpos])-3017; //Q8
    tmpcoeffs_gQ6[posg] -= WebRtcIsacfix_kMeansGainQ8[0][posg]; //Q8, but Q6 after not-needed mult. by 4
    posg++; gainpos++;

    tmpcoeffs_gQ6[posg] = CalcLogN(gain_lo_hiQ17[gainpos])-3017; //Q8
    tmpcoeffs_gQ6[posg] -= WebRtcIsacfix_kMeansGainQ8[0][posg]; //Q8, but Q6 after not-needed mult. by 4
    posg++; gainpos++;

  }


  /* KLT  */

  /* left transform */
  offsg = 0;
  for (j=0; j<SUBFRAMES; j++) {
    posg = offsg;
    for (k=0; k<2; k++) {
      sumQQ = 0;
      pos = offsg;
      pos2 = k;
      for (n=0; n<2; n++) {
        sumQQ += WEBRTC_SPL_MUL_16_16(tmpcoeffs_gQ6[pos], WebRtcIsacfix_kT1GainQ15[0][pos2]); //Q21 = Q6*Q15
        pos++;
        pos2 += 2;
      }
      tmpcoeffs2_gQ21[posg] = sumQQ;
      posg++;
    }

    offsg += 2;
  }

  /* right transform */
  offsg = 0;
  offs2 = 0;
  for (j=0; j<SUBFRAMES; j++) {
    posg = offsg;
    for (k=0; k<2; k++) {
      sumQQ = 0;
      pos = k;
      pos2 = offs2;
      for (n=0; n<SUBFRAMES; n++) {
        sumQQ += WEBRTC_SPL_LSHIFT_W32(WEBRTC_SPL_MUL_16_32_RSFT16(WebRtcIsacfix_kT2GainQ15[0][pos2], tmpcoeffs2_gQ21[pos]), 1); // (Q15*Q21)>>(16-1) = Q21
        pos += 2;
        pos2++;
      }
      tmpcoeffs_gQ17[posg] = WEBRTC_SPL_RSHIFT_W32(sumQQ, 4);
      posg++;
    }
    offsg += 2;
    offs2 += SUBFRAMES;
  }

  /* quantize coefficients */
  for (k=0; k<KLT_ORDER_GAIN; k++) //ATTN: ok?
  {
    posQQ = WebRtcIsacfix_kSelIndGain[k];
    pos2QQ= (WebRtc_Word16)CalcLrIntQ(tmpcoeffs_gQ17[posQQ], 17);

    index_gQQ[k] = pos2QQ + WebRtcIsacfix_kQuantMinGain[k]; //ATTN: ok?
    if (index_gQQ[k] < 0) {
      index_gQQ[k] = 0;
    }
    else if (index_gQQ[k] > WebRtcIsacfix_kMaxIndGain[k]) {
      index_gQQ[k] = WebRtcIsacfix_kMaxIndGain[k];
    }
  }
}
