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
 * lattice.c
 *
 * Contains the normalized lattice filter routines (MA and AR) for iSAC codec
 *
 */

#include "codec.h"
#include "settings.h"

#define LATTICE_MUL_32_32_RSFT16(a32a, a32b, b32)                  \
  ((WebRtc_Word32)(WEBRTC_SPL_MUL(a32a, b32) + (WEBRTC_SPL_MUL_16_32_RSFT16(a32b, b32))))
/* This macro is FORBIDDEN to use elsewhere than in a function in this file and
   its corresponding neon version. It might give unpredictable results, since a
   general WebRtc_Word32*WebRtc_Word32 multiplication results in a 64 bit value.
   The result is then shifted just 16 steps to the right, giving need for 48
   bits, i.e. in the generel case, it will NOT fit in a WebRtc_Word32. In the
   cases used in here, the WebRtc_Word32 will be enough, since (for a good
   reason) the involved multiplicands aren't big enough to overflow a
   WebRtc_Word32 after shifting right 16 bits. I have compared the result of a
   multiplication between t32 and tmp32, done in two ways:
   1) Using (WebRtc_Word32) (((float)(tmp32))*((float)(tmp32b))/65536.0);
   2) Using LATTICE_MUL_32_32_RSFT16(t16a, t16b, tmp32b);
   By running 25 files, I haven't found any bigger diff than 64 - this was in the
   case when  method 1) gave 650235648 and 2) gave 650235712.
*/

/* Function prototype: filtering ar_g_Q0[] and ar_f_Q0[] through an AR filter
   with coefficients cth_Q15[] and sth_Q15[].
   Implemented for both generic and ARMv7 platforms.
 */
void WebRtcIsacfix_FilterArLoop(int16_t* ar_g_Q0,
                                int16_t* ar_f_Q0,
                                int16_t* cth_Q15,
                                int16_t* sth_Q15,
                                int16_t order_coef);

/* Inner loop used for function WebRtcIsacfix_NormLatticeFilterMa().
   It does:
   for 0 <= n < HALF_SUBFRAMELEN - 1:
     *ptr2 = input2 * (*ptr2) + input0 * (*ptr0));
     *ptr1 = input1 * (*ptr0) + input0 * (*ptr2);
*/
void WebRtcIsacfix_FilterMaLoopC(int16_t input0,  // Filter coefficient
                                 int16_t input1,  // Filter coefficient
                                 int32_t input2,  // Inverse coeff. (1/input1)
                                 int32_t* ptr0,   // Sample buffer
                                 int32_t* ptr1,   // Sample buffer
                                 int32_t* ptr2) { // Sample buffer
  int n = 0;

  // Separate the 32-bit variable input2 into two 16-bit integers (high 16 and
  // low 16 bits), for using LATTICE_MUL_32_32_RSFT16 in the loop.
  int16_t t16a = (int16_t)(input2 >> 16);
  int16_t t16b = (int16_t)input2;
  if (t16b < 0) t16a++;

  // The loop filtering the samples *ptr0, *ptr1, *ptr2 with filter coefficients
  // input0, input1, and input2.
  for(n = 0; n < HALF_SUBFRAMELEN - 1; n++, ptr0++, ptr1++, ptr2++) {
    int32_t tmp32a = 0;
    int32_t tmp32b = 0;

    // Calculate *ptr2 = input2 * (*ptr2 + input0 * (*ptr0));
    tmp32a = WEBRTC_SPL_MUL_16_32_RSFT15(input0, *ptr0); // Q15 * Q15 >> 15 = Q15
    tmp32b = *ptr2 + tmp32a; // Q15 + Q15 = Q15
    *ptr2 = LATTICE_MUL_32_32_RSFT16(t16a, t16b, tmp32b);

    // Calculate *ptr1 = input1 * (*ptr0) + input0 * (*ptr2);
    tmp32a = WEBRTC_SPL_MUL_16_32_RSFT15(input1, *ptr0); // Q15*Q15>>15 = Q15
    tmp32b = WEBRTC_SPL_MUL_16_32_RSFT15(input0, *ptr2); // Q15*Q15>>15 = Q15
    *ptr1 = tmp32a + tmp32b; // Q15 + Q15 = Q15
  }
}

// Declare a function pointer.
FilterMaLoopFix WebRtcIsacfix_FilterMaLoopFix;

/* filter the signal using normalized lattice filter */
/* MA filter */
void WebRtcIsacfix_NormLatticeFilterMa(WebRtc_Word16 orderCoef,
                                       WebRtc_Word32 *stateGQ15,
                                       WebRtc_Word16 *lat_inQ0,
                                       WebRtc_Word16 *filt_coefQ15,
                                       WebRtc_Word32 *gain_lo_hiQ17,
                                       WebRtc_Word16 lo_hi,
                                       WebRtc_Word16 *lat_outQ9)
{
  WebRtc_Word16 sthQ15[MAX_AR_MODEL_ORDER];
  WebRtc_Word16 cthQ15[MAX_AR_MODEL_ORDER];

  int u, i, k, n;
  WebRtc_Word16 temp2,temp3;
  WebRtc_Word16 ord_1 = orderCoef+1;
  WebRtc_Word32 inv_cthQ16[MAX_AR_MODEL_ORDER];

  WebRtc_Word32 gain32, fQtmp;
  WebRtc_Word16 gain16;
  WebRtc_Word16 gain_sh;

  WebRtc_Word32 tmp32, tmp32b;
  WebRtc_Word32 fQ15vec[HALF_SUBFRAMELEN];
  WebRtc_Word32 gQ15[MAX_AR_MODEL_ORDER+1][HALF_SUBFRAMELEN];
  WebRtc_Word16 sh;
  WebRtc_Word16 t16a;
  WebRtc_Word16 t16b;

  for (u=0;u<SUBFRAMES;u++)
  {
    int32_t temp1 = WEBRTC_SPL_MUL_16_16(u, HALF_SUBFRAMELEN);

    /* set the Direct Form coefficients */
    temp2 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16(u, orderCoef);
    temp3 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16(2, u)+lo_hi;

    /* compute lattice filter coefficients */
    memcpy(sthQ15, &filt_coefQ15[temp2], orderCoef * sizeof(WebRtc_Word16));

    WebRtcSpl_SqrtOfOneMinusXSquared(sthQ15, orderCoef, cthQ15);

    /* compute the gain */
    gain32 = gain_lo_hiQ17[temp3];
    gain_sh = WebRtcSpl_NormW32(gain32);
    gain32 = WEBRTC_SPL_LSHIFT_W32(gain32, gain_sh); //Q(17+gain_sh)

    for (k=0;k<orderCoef;k++)
    {
      gain32 = WEBRTC_SPL_MUL_16_32_RSFT15(cthQ15[k], gain32); //Q15*Q(17+gain_sh)>>15 = Q(17+gain_sh)
      inv_cthQ16[k] = WebRtcSpl_DivW32W16((WebRtc_Word32)2147483647, cthQ15[k]); // 1/cth[k] in Q31/Q15 = Q16
    }
    gain16 = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(gain32, 16); //Q(1+gain_sh)

    /* normalized lattice filter */
    /*****************************/

    /* initial conditions */
    for (i=0;i<HALF_SUBFRAMELEN;i++)
    {
      fQ15vec[i] = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)lat_inQ0[i + temp1], 15); //Q15
      gQ15[0][i] = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)lat_inQ0[i + temp1], 15); //Q15
    }


    fQtmp = fQ15vec[0];

    /* get the state of f&g for the first input, for all orders */
    for (i=1;i<ord_1;i++)
    {
      // Calculate f[i][0] = inv_cth[i-1]*(f[i-1][0] + sth[i-1]*stateG[i-1]);
      tmp32 = WEBRTC_SPL_MUL_16_32_RSFT15(sthQ15[i-1], stateGQ15[i-1]);//Q15*Q15>>15 = Q15
      tmp32b= fQtmp + tmp32; //Q15+Q15=Q15
      tmp32 = inv_cthQ16[i-1]; //Q16
      t16a = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(tmp32, 16);
      t16b = (WebRtc_Word16) (tmp32-WEBRTC_SPL_LSHIFT_W32(((WebRtc_Word32)t16a), 16));
      if (t16b<0) t16a++;
      tmp32 = LATTICE_MUL_32_32_RSFT16(t16a, t16b, tmp32b);
      fQtmp = tmp32; // Q15

      // Calculate g[i][0] = cth[i-1]*stateG[i-1] + sth[i-1]* f[i][0];
      tmp32  = WEBRTC_SPL_MUL_16_32_RSFT15(cthQ15[i-1], stateGQ15[i-1]); //Q15*Q15>>15 = Q15
      tmp32b = WEBRTC_SPL_MUL_16_32_RSFT15(sthQ15[i-1], fQtmp); //Q15*Q15>>15 = Q15
      tmp32  = tmp32 + tmp32b;//Q15+Q15 = Q15
      gQ15[i][0] = tmp32; // Q15
    }

    /* filtering */
    /* save the states */
    for(k=0;k<orderCoef;k++)
    {
      // for 0 <= n < HALF_SUBFRAMELEN - 1:
      //   f[k+1][n+1] = inv_cth[k]*(f[k][n+1] + sth[k]*g[k][n]);
      //   g[k+1][n+1] = cth[k]*g[k][n] + sth[k]* f[k+1][n+1];
      WebRtcIsacfix_FilterMaLoopFix(sthQ15[k], cthQ15[k], inv_cthQ16[k],
                                    &gQ15[k][0], &gQ15[k+1][1], &fQ15vec[1]);
    }

    fQ15vec[0] = fQtmp;

    for(n=0;n<HALF_SUBFRAMELEN;n++)
    {
      //gain32 = WEBRTC_SPL_RSHIFT_W32(gain32, gain_sh); // Q(17+gain_sh) -> Q17
      tmp32 = WEBRTC_SPL_MUL_16_32_RSFT16(gain16, fQ15vec[n]); //Q(1+gain_sh)*Q15>>16 = Q(gain_sh)
      sh = 9-gain_sh; //number of needed shifts to reach Q9
      t16a = (WebRtc_Word16) WEBRTC_SPL_SHIFT_W32(tmp32, sh);
      lat_outQ9[n + temp1] = t16a;
    }

    /* save the states */
    for (i=0;i<ord_1;i++)
    {
      stateGQ15[i] = gQ15[i][HALF_SUBFRAMELEN-1];
    }
    //process next frame
  }

  return;
}





/* ----------------AR filter-------------------------*/
/* filter the signal using normalized lattice filter */
void WebRtcIsacfix_NormLatticeFilterAr(WebRtc_Word16 orderCoef,
                                       WebRtc_Word16 *stateGQ0,
                                       WebRtc_Word32 *lat_inQ25,
                                       WebRtc_Word16 *filt_coefQ15,
                                       WebRtc_Word32 *gain_lo_hiQ17,
                                       WebRtc_Word16 lo_hi,
                                       WebRtc_Word16 *lat_outQ0)
{
  int ii,n,k,i,u;
  WebRtc_Word16 sthQ15[MAX_AR_MODEL_ORDER];
  WebRtc_Word16 cthQ15[MAX_AR_MODEL_ORDER];
  WebRtc_Word32 tmp32;


  WebRtc_Word16 tmpAR;
  WebRtc_Word16 ARfQ0vec[HALF_SUBFRAMELEN];
  WebRtc_Word16 ARgQ0vec[MAX_AR_MODEL_ORDER+1];

  WebRtc_Word32 inv_gain32;
  WebRtc_Word16 inv_gain16;
  WebRtc_Word16 den16;
  WebRtc_Word16 sh;

  WebRtc_Word16 temp2,temp3;
  WebRtc_Word16 ord_1 = orderCoef+1;

  for (u=0;u<SUBFRAMES;u++)
  {
    int32_t temp1 = WEBRTC_SPL_MUL_16_16(u, HALF_SUBFRAMELEN);

    //set the denominator and numerator of the Direct Form
    temp2 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16(u, orderCoef);
    temp3 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16(2, u) + lo_hi;

    for (ii=0; ii<orderCoef; ii++) {
      sthQ15[ii] = filt_coefQ15[temp2+ii];
    }

    WebRtcSpl_SqrtOfOneMinusXSquared(sthQ15, orderCoef, cthQ15);

    /* Simulation of the 25 files shows that maximum value in
       the vector gain_lo_hiQ17[] is 441344, which means that
       it is log2((2^31)/441344) = 12.2 shifting bits from
       saturation. Therefore, it should be safe to use Q27 instead
       of Q17. */

    tmp32 = WEBRTC_SPL_LSHIFT_W32(gain_lo_hiQ17[temp3], 10); // Q27

    for (k=0;k<orderCoef;k++) {
      tmp32 = WEBRTC_SPL_MUL_16_32_RSFT15(cthQ15[k], tmp32); // Q15*Q27>>15 = Q27
    }

    sh = WebRtcSpl_NormW32(tmp32); // tmp32 is the gain
    den16 = (WebRtc_Word16) WEBRTC_SPL_SHIFT_W32(tmp32, sh-16); //Q(27+sh-16) = Q(sh+11) (all 16 bits are value bits)
    inv_gain32 = WebRtcSpl_DivW32W16((WebRtc_Word32)2147483647, den16); // 1/gain in Q31/Q(sh+11) = Q(20-sh)

    //initial conditions
    inv_gain16 = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(inv_gain32, 2); // 1/gain in Q(20-sh-2) = Q(18-sh)

    for (i=0;i<HALF_SUBFRAMELEN;i++)
    {

      tmp32 = WEBRTC_SPL_LSHIFT_W32(lat_inQ25[i + temp1], 1); //Q25->Q26
      tmp32 = WEBRTC_SPL_MUL_16_32_RSFT16(inv_gain16, tmp32); //lat_in[]*inv_gain in (Q(18-sh)*Q26)>>16 = Q(28-sh)
      tmp32 = WEBRTC_SPL_SHIFT_W32(tmp32, -(28-sh)); // lat_in[]*inv_gain in Q0

      ARfQ0vec[i] = (WebRtc_Word16)WebRtcSpl_SatW32ToW16(tmp32); // Q0
    }

    for (i=orderCoef-1;i>=0;i--) //get the state of f&g for the first input, for all orders
    {
      tmp32 = WEBRTC_SPL_RSHIFT_W32(((WEBRTC_SPL_MUL_16_16(cthQ15[i],ARfQ0vec[0])) - (WEBRTC_SPL_MUL_16_16(sthQ15[i],stateGQ0[i])) + 16384), 15);
      tmpAR = (WebRtc_Word16)WebRtcSpl_SatW32ToW16(tmp32); // Q0

      tmp32 = WEBRTC_SPL_RSHIFT_W32(((WEBRTC_SPL_MUL_16_16(sthQ15[i],ARfQ0vec[0])) + (WEBRTC_SPL_MUL_16_16(cthQ15[i], stateGQ0[i])) + 16384), 15);
      ARgQ0vec[i+1] = (WebRtc_Word16)WebRtcSpl_SatW32ToW16(tmp32); // Q0
      ARfQ0vec[0] = tmpAR;
    }
    ARgQ0vec[0] = ARfQ0vec[0];

    // Filter ARgQ0vec[] and ARfQ0vec[] through coefficients cthQ15[] and sthQ15[].
    WebRtcIsacfix_FilterArLoop(ARgQ0vec, ARfQ0vec, cthQ15, sthQ15, orderCoef);

    for(n=0;n<HALF_SUBFRAMELEN;n++)
    {
      lat_outQ0[n + temp1] = ARfQ0vec[n];
    }


    /* cannot use memcpy in the following */

    for (i=0;i<ord_1;i++)
    {
      stateGQ0[i] = ARgQ0vec[i];
    }
  }

  return;
}
