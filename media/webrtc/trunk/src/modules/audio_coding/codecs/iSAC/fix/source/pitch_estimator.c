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
 * pitch_estimator.c
 *
 * Pitch filter functions
 *
 */

#include <string.h>

#include "signal_processing_library.h"
#include "pitch_estimator.h"

/* log2[0.2, 0.5, 0.98] in Q8 */
static const WebRtc_Word16 kLogLagWinQ8[3] = {
  -594, -256, -7
};

/* [1 -0.75 0.25] in Q12 */
static const WebRtc_Word16 kACoefQ12[3] = {
  4096, -3072, 1024
};



static __inline WebRtc_Word32 Log2Q8( WebRtc_UWord32 x ) {

  WebRtc_Word32 zeros, lg2;
  WebRtc_Word16 frac;

  zeros=WebRtcSpl_NormU32(x);
  frac=(WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(((WebRtc_UWord32)(WEBRTC_SPL_LSHIFT_W32(x, zeros))&0x7FFFFFFF), 23);
  /* log2(magn(i)) */

  lg2= (WEBRTC_SPL_LSHIFT_W32((31-zeros), 8)+frac);
  return lg2;

}

static __inline WebRtc_Word16 Exp2Q10(WebRtc_Word16 x) { // Both in and out in Q10

  WebRtc_Word16 tmp16_1, tmp16_2;

  tmp16_2=(WebRtc_Word16)(0x0400|(x&0x03FF));
  tmp16_1=-(WebRtc_Word16)WEBRTC_SPL_RSHIFT_W16(x,10);
  if(tmp16_1>0)
    return (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W16(tmp16_2, tmp16_1);
  else
    return (WebRtc_Word16) WEBRTC_SPL_LSHIFT_W16(tmp16_2, -tmp16_1);

}



/* 1D parabolic interpolation . All input and output values are in Q8 */
static __inline void Intrp1DQ8(WebRtc_Word32 *x, WebRtc_Word32 *fx, WebRtc_Word32 *y, WebRtc_Word32 *fy) {

  WebRtc_Word16 sign1=1, sign2=1;
  WebRtc_Word32 r32, q32, t32, nom32, den32;
  WebRtc_Word16 t16, tmp16, tmp16_1;

  if ((fx[0]>0) && (fx[2]>0)) {
    r32=fx[1]-fx[2];
    q32=fx[0]-fx[1];
    nom32=q32+r32;
    den32=WEBRTC_SPL_MUL_32_16((q32-r32), 2);
    if (nom32<0)
      sign1=-1;
    if (den32<0)
      sign2=-1;

    /* t = (q32+r32)/(2*(q32-r32)) = (fx[0]-fx[1] + fx[1]-fx[2])/(2 * fx[0]-fx[1] - (fx[1]-fx[2]))*/
    /* (Signs are removed because WebRtcSpl_DivResultInQ31 can't handle negative numbers) */
    t32=WebRtcSpl_DivResultInQ31(WEBRTC_SPL_MUL_32_16(nom32, sign1),WEBRTC_SPL_MUL_32_16(den32, sign2)); /* t in Q31, without signs */

    t16=(WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(t32, 23);  /* Q8 */
    t16=t16*sign1*sign2;        /* t in Q8 with signs */

    *y = x[0]+t16;          /* Q8 */
    // *y = x[1]+t16;          /* Q8 */

    /* The following code calculates fy in three steps */
    /* fy = 0.5 * t * (t-1) * fx[0] + (1-t*t) * fx[1] + 0.5 * t * (t+1) * fx[2]; */

    /* Part I: 0.5 * t * (t-1) * fx[0] */
    tmp16_1=(WebRtc_Word16)WEBRTC_SPL_MUL_16_16(t16,t16); /* Q8*Q8=Q16 */
    tmp16_1 = WEBRTC_SPL_RSHIFT_W16(tmp16_1,2);  /* Q16>>2 = Q14 */
    t16 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16(t16, 64);           /* Q8<<6 = Q14  */
    tmp16 = tmp16_1-t16;
    *fy = WEBRTC_SPL_MUL_16_32_RSFT15(tmp16, fx[0]); /* (Q14 * Q8 >>15)/2 = Q8 */

    /* Part II: (1-t*t) * fx[1] */
    tmp16 = 16384-tmp16_1;        /* 1 in Q14 - Q14 */
    *fy += WEBRTC_SPL_MUL_16_32_RSFT14(tmp16, fx[1]);/* Q14 * Q8 >> 14 = Q8 */

    /* Part III: 0.5 * t * (t+1) * fx[2] */
    tmp16 = tmp16_1+t16;
    *fy += WEBRTC_SPL_MUL_16_32_RSFT15(tmp16, fx[2]);/* (Q14 * Q8 >>15)/2 = Q8 */
  } else {
    *y = x[0];
    *fy= fx[1];
  }
}


static void FindFour32(WebRtc_Word32 *in, WebRtc_Word16 length, WebRtc_Word16 *bestind)
{
  WebRtc_Word32 best[4]= {-100, -100, -100, -100};
  WebRtc_Word16 k;

  for (k=0; k<length; k++) {
    if (in[k] > best[3]) {
      if (in[k] > best[2]) {
        if (in[k] > best[1]) {
          if (in[k] > best[0]) { // The Best
            best[3] = best[2];
            bestind[3] = bestind[2];
            best[2] = best[1];
            bestind[2] = bestind[1];
            best[1] = best[0];
            bestind[1] = bestind[0];
            best[0] = in[k];
            bestind[0] = k;
          } else { // 2nd best
            best[3] = best[2];
            bestind[3] = bestind[2];
            best[2] = best[1];
            bestind[2] = bestind[1];
            best[1] = in[k];
            bestind[1] = k;
          }
        } else { // 3rd best
          best[3] = best[2];
          bestind[3] = bestind[2];
          best[2] = in[k];
          bestind[2] = k;
        }
      } else {  // 4th best
        best[3] = in[k];
        bestind[3] = k;
      }
    }
  }
}





static void PCorr2Q32(const WebRtc_Word16 *in, WebRtc_Word32 *logcorQ8)
{
  WebRtc_Word16 scaling,n,k;
  WebRtc_Word32 ysum32,csum32, lys, lcs;
  WebRtc_Word32 prod32, oneQ8;


  const WebRtc_Word16 *x, *inptr;

  oneQ8 = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)1, 8);  // 1.00 in Q8

  x = in + PITCH_MAX_LAG/2 + 2;
  scaling = WebRtcSpl_GetScalingSquare ((WebRtc_Word16 *) in, PITCH_CORR_LEN2, PITCH_CORR_LEN2);
  ysum32 = 1;
  csum32 = 0;
  x = in + PITCH_MAX_LAG/2 + 2;
  for (n = 0; n < PITCH_CORR_LEN2; n++) {
    ysum32 += WEBRTC_SPL_MUL_16_16_RSFT( (WebRtc_Word16) in[n],(WebRtc_Word16) in[n], scaling);  // Q0
    csum32 += WEBRTC_SPL_MUL_16_16_RSFT((WebRtc_Word16) x[n],(WebRtc_Word16) in[n], scaling); // Q0
  }

  logcorQ8 += PITCH_LAG_SPAN2 - 1;

  lys=Log2Q8((WebRtc_UWord32) ysum32); // Q8
  lys=WEBRTC_SPL_RSHIFT_W32(lys, 1); //sqrt(ysum);

  if (csum32>0) {

    lcs=Log2Q8((WebRtc_UWord32) csum32);   // 2log(csum) in Q8

    if (lcs>(lys + oneQ8) ){ // csum/sqrt(ysum) > 2 in Q8
      *logcorQ8 = lcs - lys;  // log2(csum/sqrt(ysum))
    } else {
      *logcorQ8 = oneQ8;  // 1.00
    }

  } else {
    *logcorQ8 = 0;
  }


  for (k = 1; k < PITCH_LAG_SPAN2; k++) {
    inptr = &in[k];
    ysum32 -= WEBRTC_SPL_MUL_16_16_RSFT( (WebRtc_Word16) in[k-1],(WebRtc_Word16) in[k-1], scaling);
    ysum32 += WEBRTC_SPL_MUL_16_16_RSFT( (WebRtc_Word16) in[PITCH_CORR_LEN2 + k - 1],(WebRtc_Word16) in[PITCH_CORR_LEN2 + k - 1], scaling);
    csum32 = 0;
    prod32 = WEBRTC_SPL_MUL_16_16_RSFT( (WebRtc_Word16) x[0],(WebRtc_Word16) inptr[0], scaling);

    for (n = 1; n < PITCH_CORR_LEN2; n++) {
      csum32 += prod32;
      prod32 = WEBRTC_SPL_MUL_16_16_RSFT( (WebRtc_Word16) x[n],(WebRtc_Word16) inptr[n], scaling);
    }

    csum32 += prod32;
    logcorQ8--;

    lys=Log2Q8((WebRtc_UWord32)ysum32); // Q8
    lys=WEBRTC_SPL_RSHIFT_W32(lys, 1); //sqrt(ysum);

    if (csum32>0) {

      lcs=Log2Q8((WebRtc_UWord32) csum32);   // 2log(csum) in Q8

      if (lcs>(lys + oneQ8) ){ // csum/sqrt(ysum) > 2
        *logcorQ8 = lcs - lys;  // log2(csum/sqrt(ysum))
      } else {
        *logcorQ8 = oneQ8;  // 1.00
      }

    } else {
      *logcorQ8 = 0;
    }
  }
}



void WebRtcIsacfix_InitialPitch(const WebRtc_Word16 *in, /* Q0 */
                                PitchAnalysisStruct *State,
                                WebRtc_Word16 *lagsQ7                   /* Q7 */
                                )
{
  WebRtc_Word16 buf_dec16[PITCH_CORR_LEN2+PITCH_CORR_STEP2+PITCH_MAX_LAG/2+2];
  WebRtc_Word32 *crrvecQ8_1,*crrvecQ8_2;
  WebRtc_Word32 cv1q[PITCH_LAG_SPAN2+2],cv2q[PITCH_LAG_SPAN2+2], peakvq[PITCH_LAG_SPAN2+2];
  int k;
  WebRtc_Word16 peaks_indq;
  WebRtc_Word16 peakiq[PITCH_LAG_SPAN2];
  WebRtc_Word32 corr;
  WebRtc_Word32 corr32, corr_max32, corr_max_o32;
  WebRtc_Word16 npkq;
  WebRtc_Word16 best4q[4]={0,0,0,0};
  WebRtc_Word32 xq[3],yq[1],fyq[1];
  WebRtc_Word32 *fxq;
  WebRtc_Word32 best_lag1q, best_lag2q;
  WebRtc_Word32 tmp32a,tmp32b,lag32,ratq;
  WebRtc_Word16 start;
  WebRtc_Word16 oldgQ12, tmp16a, tmp16b, gain_bias16,tmp16c, tmp16d, bias16;
  WebRtc_Word32 tmp32c,tmp32d, tmp32e;
  WebRtc_Word16 old_lagQ;
  WebRtc_Word32 old_lagQ8;
  WebRtc_Word32 lagsQ8[4];

  old_lagQ = State->PFstr_wght.oldlagQ7; // Q7
  old_lagQ8= WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)old_lagQ,1); //Q8

  oldgQ12= State->PFstr_wght.oldgainQ12;

  crrvecQ8_1=&cv1q[1];
  crrvecQ8_2=&cv2q[1];


  /* copy old values from state buffer */
  memcpy(buf_dec16, State->dec_buffer16, WEBRTC_SPL_MUL_16_16(sizeof(WebRtc_Word16), (PITCH_CORR_LEN2+PITCH_CORR_STEP2+PITCH_MAX_LAG/2-PITCH_FRAME_LEN/2+2)));

  /* decimation; put result after the old values */
  WebRtcIsacfix_DecimateAllpass32(in, State->decimator_state32, PITCH_FRAME_LEN,
                                  &buf_dec16[PITCH_CORR_LEN2+PITCH_CORR_STEP2+PITCH_MAX_LAG/2-PITCH_FRAME_LEN/2+2]);

  /* low-pass filtering */
  start= PITCH_CORR_LEN2+PITCH_CORR_STEP2+PITCH_MAX_LAG/2-PITCH_FRAME_LEN/2+2;
  WebRtcSpl_FilterARFastQ12(&buf_dec16[start],&buf_dec16[start],(WebRtc_Word16*)kACoefQ12,3, PITCH_FRAME_LEN/2);

  /* copy end part back into state buffer */
  for (k = 0; k < (PITCH_CORR_LEN2+PITCH_CORR_STEP2+PITCH_MAX_LAG/2-PITCH_FRAME_LEN/2+2); k++)
    State->dec_buffer16[k] = buf_dec16[k+PITCH_FRAME_LEN/2];


  /* compute correlation for first and second half of the frame */
  PCorr2Q32(buf_dec16, crrvecQ8_1);
  PCorr2Q32(buf_dec16 + PITCH_CORR_STEP2, crrvecQ8_2);


  /* bias towards pitch lag of previous frame */
  tmp32a = Log2Q8((WebRtc_UWord32) old_lagQ8) - 2304; // log2(0.5*oldlag) in Q8
  tmp32b = WEBRTC_SPL_MUL_16_16_RSFT(oldgQ12,oldgQ12, 10); //Q12 & * 4.0;
  gain_bias16 = (WebRtc_Word16) tmp32b;  //Q12
  if (gain_bias16 > 3276) gain_bias16 = 3276; // 0.8 in Q12


  for (k = 0; k < PITCH_LAG_SPAN2; k++)
  {
    if (crrvecQ8_1[k]>0) {
      tmp32b = Log2Q8((WebRtc_UWord32) (k + (PITCH_MIN_LAG/2-2)));
      tmp16a = (WebRtc_Word16) (tmp32b - tmp32a); // Q8 & fabs(ratio)<4
      tmp32c = WEBRTC_SPL_MUL_16_16_RSFT(tmp16a,tmp16a, 6); //Q10
      tmp16b = (WebRtc_Word16) tmp32c; // Q10 & <8
      tmp32d = WEBRTC_SPL_MUL_16_16_RSFT(tmp16b, 177 , 8); // mult with ln2 in Q8
      tmp16c = (WebRtc_Word16) tmp32d; // Q10 & <4
      tmp16d = Exp2Q10((WebRtc_Word16) -tmp16c); //Q10
      tmp32c = WEBRTC_SPL_MUL_16_16_RSFT(gain_bias16,tmp16d,13); // Q10  & * 0.5
      bias16 = (WebRtc_Word16) (1024 + tmp32c); // Q10
      tmp32b = Log2Q8((WebRtc_UWord32) bias16) - 2560; // Q10 in -> Q8 out with 10*2^8 offset
      crrvecQ8_1[k] += tmp32b ; // -10*2^8 offset
    }
  }

  /* taper correlation functions */
  for (k = 0; k < 3; k++) {
    crrvecQ8_1[k] += kLogLagWinQ8[k];
    crrvecQ8_2[k] += kLogLagWinQ8[k];

    crrvecQ8_1[PITCH_LAG_SPAN2-1-k] += kLogLagWinQ8[k];
    crrvecQ8_2[PITCH_LAG_SPAN2-1-k] += kLogLagWinQ8[k];
  }


  /* Make zeropadded corr vectors */
  cv1q[0]=0;
  cv2q[0]=0;
  cv1q[PITCH_LAG_SPAN2+1]=0;
  cv2q[PITCH_LAG_SPAN2+1]=0;
  corr_max32 = 0;

  for (k = 1; k <= PITCH_LAG_SPAN2; k++)
  {


    corr32=crrvecQ8_1[k-1];
    if (corr32 > corr_max32)
      corr_max32 = corr32;

    corr32=crrvecQ8_2[k-1];
    corr32 += -4; // Compensate for later (log2(0.99))

    if (corr32 > corr_max32)
      corr_max32 = corr32;

  }

  /* threshold value to qualify as a peak */
  // corr_max32 += -726; // log(0.14)/log(2.0) in Q8
  corr_max32 += -1000; // log(0.14)/log(2.0) in Q8
  corr_max_o32 = corr_max32;


  /* find peaks in corr1 */
  peaks_indq = 0;
  for (k = 1; k <= PITCH_LAG_SPAN2; k++)
  {
    corr32=cv1q[k];
    if (corr32>corr_max32) { // Disregard small peaks
      if ((corr32>=cv1q[k-1]) && (corr32>cv1q[k+1])) { // Peak?
        peakvq[peaks_indq] = corr32;
        peakiq[peaks_indq++] = k;
      }
    }
  }


  /* find highest interpolated peak */
  corr_max32=0;
  best_lag1q =0;
  if (peaks_indq > 0) {
    FindFour32(peakvq, (WebRtc_Word16) peaks_indq, best4q);
    npkq = WEBRTC_SPL_MIN(peaks_indq, 4);

    for (k=0;k<npkq;k++) {

      lag32 =  peakiq[best4q[k]];
      fxq = &cv1q[peakiq[best4q[k]]-1];
      xq[0]= lag32;
      xq[0] = WEBRTC_SPL_LSHIFT_W32(xq[0], 8);
      Intrp1DQ8(xq, fxq, yq, fyq);

      tmp32a= Log2Q8((WebRtc_UWord32) *yq) - 2048; // offset 8*2^8
      /* Bias towards short lags */
      /* log(pow(0.8, log(2.0 * *y )))/log(2.0) */
      tmp32b= WEBRTC_SPL_MUL_16_16_RSFT((WebRtc_Word16) tmp32a, -42, 8);
      tmp32c= tmp32b + 256;
      *fyq += tmp32c;
      if (*fyq > corr_max32) {
        corr_max32 = *fyq;
        best_lag1q = *yq;
      }
    }
    tmp32a = best_lag1q - OFFSET_Q8;
    tmp32b = WEBRTC_SPL_LSHIFT_W32(tmp32a, 1);
    lagsQ8[0] = tmp32b + PITCH_MIN_LAG_Q8;
    lagsQ8[1] = lagsQ8[0];
  } else {
    lagsQ8[0] = old_lagQ8;
    lagsQ8[1] = lagsQ8[0];
  }

  /* Bias towards constant pitch */
  tmp32a = lagsQ8[0] - PITCH_MIN_LAG_Q8;
  ratq = WEBRTC_SPL_RSHIFT_W32(tmp32a, 1) + OFFSET_Q8;

  for (k = 1; k <= PITCH_LAG_SPAN2; k++)
  {
    tmp32a = WEBRTC_SPL_LSHIFT_W32(k, 7); // 0.5*k Q8
    tmp32b = (WebRtc_Word32) (WEBRTC_SPL_LSHIFT_W32(tmp32a, 1)) - ratq; // Q8
    tmp32c = WEBRTC_SPL_MUL_16_16_RSFT((WebRtc_Word16) tmp32b, (WebRtc_Word16) tmp32b, 8); // Q8

    tmp32b = (WebRtc_Word32) tmp32c + (WebRtc_Word32)  WEBRTC_SPL_RSHIFT_W32(ratq, 1); // (k-r)^2 + 0.5 * r  Q8
    tmp32c = Log2Q8((WebRtc_UWord32) tmp32a) - 2048; // offset 8*2^8 , log2(0.5*k) Q8
    tmp32d = Log2Q8((WebRtc_UWord32) tmp32b) - 2048; // offset 8*2^8 , log2(0.5*k) Q8
    tmp32e =  tmp32c -tmp32d;

    cv2q[k] += WEBRTC_SPL_RSHIFT_W32(tmp32e, 1);

  }

  /* find peaks in corr2 */
  corr_max32 = corr_max_o32;
  peaks_indq = 0;

  for (k = 1; k <= PITCH_LAG_SPAN2; k++)
  {
    corr=cv2q[k];
    if (corr>corr_max32) { // Disregard small peaks
      if ((corr>=cv2q[k-1]) && (corr>cv2q[k+1])) { // Peak?
        peakvq[peaks_indq] = corr;
        peakiq[peaks_indq++] = k;
      }
    }
  }



  /* find highest interpolated peak */
  corr_max32 = 0;
  best_lag2q =0;
  if (peaks_indq > 0) {

    FindFour32(peakvq, (WebRtc_Word16) peaks_indq, best4q);
    npkq = WEBRTC_SPL_MIN(peaks_indq, 4);
    for (k=0;k<npkq;k++) {

      lag32 =  peakiq[best4q[k]];
      fxq = &cv2q[peakiq[best4q[k]]-1];

      xq[0]= lag32;
      xq[0] = WEBRTC_SPL_LSHIFT_W32(xq[0], 8);
      Intrp1DQ8(xq, fxq, yq, fyq);

      /* Bias towards short lags */
      /* log(pow(0.8, log(2.0f * *y )))/log(2.0f) */
      tmp32a= Log2Q8((WebRtc_UWord32) *yq) - 2048; // offset 8*2^8
      tmp32b= WEBRTC_SPL_MUL_16_16_RSFT((WebRtc_Word16) tmp32a, -82, 8);
      tmp32c= tmp32b + 256;
      *fyq += tmp32c;
      if (*fyq > corr_max32) {
        corr_max32 = *fyq;
        best_lag2q = *yq;
      }
    }

    tmp32a = best_lag2q - OFFSET_Q8;
    tmp32b = WEBRTC_SPL_LSHIFT_W32(tmp32a, 1);
    lagsQ8[2] = tmp32b + PITCH_MIN_LAG_Q8;
    lagsQ8[3] = lagsQ8[2];
  } else {
    lagsQ8[2] = lagsQ8[0];
    lagsQ8[3] = lagsQ8[0];
  }

  lagsQ7[0]=(WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(lagsQ8[0], 1);
  lagsQ7[1]=(WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(lagsQ8[1], 1);
  lagsQ7[2]=(WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(lagsQ8[2], 1);
  lagsQ7[3]=(WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(lagsQ8[3], 1);


}



void WebRtcIsacfix_PitchAnalysis(const WebRtc_Word16 *inn,               /* PITCH_FRAME_LEN samples */
                                 WebRtc_Word16 *outQ0,                  /* PITCH_FRAME_LEN+QLOOKAHEAD samples */
                                 PitchAnalysisStruct *State,
                                 WebRtc_Word16 *PitchLags_Q7,
                                 WebRtc_Word16 *PitchGains_Q12)
{
  WebRtc_Word16 inbufQ0[PITCH_FRAME_LEN + QLOOKAHEAD];
  WebRtc_Word16 k;

  /* inital pitch estimate */
  WebRtcIsacfix_InitialPitch(inn, State,  PitchLags_Q7);


  /* Calculate gain */
  WebRtcIsacfix_PitchFilterGains(inn, &(State->PFstr_wght), PitchLags_Q7, PitchGains_Q12);

  /* concatenate previous input's end and current input */
  for (k = 0; k < QLOOKAHEAD; k++) {
    inbufQ0[k] = State->inbuf[k];
  }
  for (k = 0; k < PITCH_FRAME_LEN; k++) {
    inbufQ0[k+QLOOKAHEAD] = (WebRtc_Word16) inn[k];
  }

  /* lookahead pitch filtering for masking analysis */
  WebRtcIsacfix_PitchFilter(inbufQ0, outQ0, &(State->PFstr), PitchLags_Q7,PitchGains_Q12, 2);


  /* store last part of input */
  for (k = 0; k < QLOOKAHEAD; k++) {
    State->inbuf[k] = inbufQ0[k + PITCH_FRAME_LEN];
  }
}
