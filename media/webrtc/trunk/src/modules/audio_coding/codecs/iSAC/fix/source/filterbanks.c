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
 * filterbanks.c
 *
 * This file contains function 
 * WebRtcIsacfix_SplitAndFilter, and WebRtcIsacfix_FilterAndCombine
 * which implement filterbanks that produce decimated lowpass and
 * highpass versions of a signal, and performs reconstruction.
 *
 */

#include "codec.h"
#include "filterbank_tables.h"
#include "settings.h"


static void AllpassFilter2FixDec16(WebRtc_Word16 *InOut16, //Q0
                                   const WebRtc_Word16 *APSectionFactors, //Q15
                                   WebRtc_Word16 lengthInOut,
                                   WebRtc_Word16 NumberOfSections,
                                   WebRtc_Word32 *FilterState) //Q16
{
  int n, j;
  WebRtc_Word32 a, b;

  for (j=0; j<NumberOfSections; j++) {
    for (n=0;n<lengthInOut;n++) {


      a = WEBRTC_SPL_MUL_16_16(APSectionFactors[j], InOut16[n]); //Q15*Q0=Q15
      a = WEBRTC_SPL_LSHIFT_W32(a, 1); // Q15 -> Q16
      b = WEBRTC_SPL_ADD_SAT_W32(a, FilterState[j]); //Q16+Q16=Q16
      a = WEBRTC_SPL_MUL_16_16_RSFT(-APSectionFactors[j], (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(b, 16), 0); //Q15*Q0=Q15
      FilterState[j] = WEBRTC_SPL_ADD_SAT_W32(WEBRTC_SPL_LSHIFT_W32(a,1), WEBRTC_SPL_LSHIFT_W32((WebRtc_UWord32)InOut16[n],16)); // Q15<<1 + Q0<<16 = Q16 + Q16 = Q16
      InOut16[n] = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(b, 16); //Save as Q0

    }
  }

}


static void HighpassFilterFixDec32(
    WebRtc_Word16 *io,   /* Q0:input Q0: Output */
    WebRtc_Word16 len, /* length of input, Input */
    const WebRtc_Word16 *coeff, /* Coeff: [Q30hi Q30lo Q30hi Q30lo Q35hi Q35lo Q35hi Q35lo] */
    WebRtc_Word32 *state) /* Q4:filter state Input/Output */
{
  int k;
  WebRtc_Word32 a, b, c, in;



  for (k=0; k<len; k++) {
    in = (WebRtc_Word32)io[k];
    /* Q35 * Q4 = Q39 ; shift 32 bit => Q7 */
    a = WEBRTC_SPL_MUL_32_32_RSFT32(coeff[2*2], coeff[2*2+1], state[0]);
    b = WEBRTC_SPL_MUL_32_32_RSFT32(coeff[2*3], coeff[2*3+1], state[1]);

    c = ((WebRtc_Word32)in) + WEBRTC_SPL_RSHIFT_W32(a+b, 7); // Q0
    //c = WEBRTC_SPL_RSHIFT_W32(c, 1); // Q-1
    io[k] = (WebRtc_Word16)WebRtcSpl_SatW32ToW16(c);  // Write output as Q0

    /* Q30 * Q4 = Q34 ; shift 32 bit => Q2 */
    a = WEBRTC_SPL_MUL_32_32_RSFT32(coeff[2*0], coeff[2*0+1], state[0]);
    b = WEBRTC_SPL_MUL_32_32_RSFT32(coeff[2*1], coeff[2*1+1], state[1]);

    c = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)in, 2) - a - b; // New state in Q2
    c= (WebRtc_Word32)WEBRTC_SPL_SAT((WebRtc_Word32)536870911, c, (WebRtc_Word32)-536870912); // Check for wrap-around

    state[1] = state[0];
    state[0] = WEBRTC_SPL_LSHIFT_W32(c, 2); // Write state as Q4

  }
}


void WebRtcIsacfix_SplitAndFilter1(WebRtc_Word16 *pin,
                                   WebRtc_Word16 *LP16,
                                   WebRtc_Word16 *HP16,
                                   PreFiltBankstr *prefiltdata)
{
  /* Function WebRtcIsacfix_SplitAndFilter */
  /* This function creates low-pass and high-pass decimated versions of part of
     the input signal, and part of the signal in the input 'lookahead buffer'. */

  int k;

  WebRtc_Word16 tempin_ch1[FRAMESAMPLES/2 + QLOOKAHEAD];
  WebRtc_Word16 tempin_ch2[FRAMESAMPLES/2 + QLOOKAHEAD];
  WebRtc_Word32 tmpState[WEBRTC_SPL_MUL_16_16(2,(QORDER-1))]; /* 4 */


  /* High pass filter */
  HighpassFilterFixDec32(pin, FRAMESAMPLES, WebRtcIsacfix_kHpStCoeffInQ30, prefiltdata->HPstates_fix);


  /* First Channel */
  for (k=0;k<FRAMESAMPLES/2;k++) {
    tempin_ch1[QLOOKAHEAD + k] = pin[1+WEBRTC_SPL_MUL_16_16(2, k)];
  }
  for (k=0;k<QLOOKAHEAD;k++) {
    tempin_ch1[k]=prefiltdata->INLABUF1_fix[k];
    prefiltdata->INLABUF1_fix[k]=pin[FRAMESAMPLES+1-WEBRTC_SPL_MUL_16_16(2, QLOOKAHEAD)+WEBRTC_SPL_MUL_16_16(2, k)];
  }

  /* Second Channel.  This is exactly like the first channel, except that the
     even samples are now filtered instead (lower channel). */
  for (k=0;k<FRAMESAMPLES/2;k++) {
    tempin_ch2[QLOOKAHEAD+k] = pin[WEBRTC_SPL_MUL_16_16(2, k)];
  }
  for (k=0;k<QLOOKAHEAD;k++) {
    tempin_ch2[k]=prefiltdata->INLABUF2_fix[k];
    prefiltdata->INLABUF2_fix[k]=pin[FRAMESAMPLES-WEBRTC_SPL_MUL_16_16(2, QLOOKAHEAD)+WEBRTC_SPL_MUL_16_16(2, k)];
  }


  /*obtain polyphase components by forward all-pass filtering through each channel */
  /* The all pass filtering automatically updates the filter states which are exported in the
     prefiltdata structure */
  AllpassFilter2FixDec16(tempin_ch1,WebRtcIsacfix_kUpperApFactorsQ15, FRAMESAMPLES/2 , NUMBEROFCHANNELAPSECTIONS, prefiltdata->INSTAT1_fix);
  AllpassFilter2FixDec16(tempin_ch2,WebRtcIsacfix_kLowerApFactorsQ15, FRAMESAMPLES/2 , NUMBEROFCHANNELAPSECTIONS, prefiltdata->INSTAT2_fix);

  for (k=0;k<WEBRTC_SPL_MUL_16_16(2, (QORDER-1));k++)
    tmpState[k] = prefiltdata->INSTAT1_fix[k];
  AllpassFilter2FixDec16(tempin_ch1 + FRAMESAMPLES/2,WebRtcIsacfix_kUpperApFactorsQ15, QLOOKAHEAD , NUMBEROFCHANNELAPSECTIONS, tmpState);
  for (k=0;k<WEBRTC_SPL_MUL_16_16(2, (QORDER-1));k++)
    tmpState[k] = prefiltdata->INSTAT2_fix[k];
  AllpassFilter2FixDec16(tempin_ch2 + FRAMESAMPLES/2,WebRtcIsacfix_kLowerApFactorsQ15, QLOOKAHEAD , NUMBEROFCHANNELAPSECTIONS, tmpState);


  /* Now Construct low-pass and high-pass signals as combinations of polyphase components */
  for (k=0; k<FRAMESAMPLES/2 + QLOOKAHEAD; k++) {
    WebRtc_Word32 tmp1, tmp2, tmp3;
    tmp1 = (WebRtc_Word32)tempin_ch1[k]; // Q0 -> Q0
    tmp2 = (WebRtc_Word32)tempin_ch2[k]; // Q0 -> Q0
    tmp3 = (WebRtc_Word32)WEBRTC_SPL_RSHIFT_W32((tmp1 + tmp2), 1);/* low pass signal*/
    LP16[k] = (WebRtc_Word16)WebRtcSpl_SatW32ToW16(tmp3); /*low pass */
    tmp3 = (WebRtc_Word32)WEBRTC_SPL_RSHIFT_W32((tmp1 - tmp2), 1);/* high pass signal*/
    HP16[k] = (WebRtc_Word16)WebRtcSpl_SatW32ToW16(tmp3); /*high pass */
  }

}/*end of WebRtcIsacfix_SplitAndFilter */


#ifdef WEBRTC_ISAC_FIX_NB_CALLS_ENABLED

/* Without lookahead */
void WebRtcIsacfix_SplitAndFilter2(WebRtc_Word16 *pin,
                                   WebRtc_Word16 *LP16,
                                   WebRtc_Word16 *HP16,
                                   PreFiltBankstr *prefiltdata)
{
  /* Function WebRtcIsacfix_SplitAndFilter2 */
  /* This function creates low-pass and high-pass decimated versions of part of
     the input signal. */

  int k;

  WebRtc_Word16 tempin_ch1[FRAMESAMPLES/2];
  WebRtc_Word16 tempin_ch2[FRAMESAMPLES/2];


  /* High pass filter */
  HighpassFilterFixDec32(pin, FRAMESAMPLES, WebRtcIsacfix_kHpStCoeffInQ30, prefiltdata->HPstates_fix);


  /* First Channel */
  for (k=0;k<FRAMESAMPLES/2;k++) {
    tempin_ch1[k] = pin[1+WEBRTC_SPL_MUL_16_16(2, k)];
  }

  /* Second Channel.  This is exactly like the first channel, except that the
     even samples are now filtered instead (lower channel). */
  for (k=0;k<FRAMESAMPLES/2;k++) {
    tempin_ch2[k] = pin[WEBRTC_SPL_MUL_16_16(2, k)];
  }


  /*obtain polyphase components by forward all-pass filtering through each channel */
  /* The all pass filtering automatically updates the filter states which are exported in the
     prefiltdata structure */
  AllpassFilter2FixDec16(tempin_ch1,WebRtcIsacfix_kUpperApFactorsQ15, FRAMESAMPLES/2 , NUMBEROFCHANNELAPSECTIONS, prefiltdata->INSTAT1_fix);
  AllpassFilter2FixDec16(tempin_ch2,WebRtcIsacfix_kLowerApFactorsQ15, FRAMESAMPLES/2 , NUMBEROFCHANNELAPSECTIONS, prefiltdata->INSTAT2_fix);


  /* Now Construct low-pass and high-pass signals as combinations of polyphase components */
  for (k=0; k<FRAMESAMPLES/2; k++) {
    WebRtc_Word32 tmp1, tmp2, tmp3;
    tmp1 = (WebRtc_Word32)tempin_ch1[k]; // Q0 -> Q0
    tmp2 = (WebRtc_Word32)tempin_ch2[k]; // Q0 -> Q0
    tmp3 = (WebRtc_Word32)WEBRTC_SPL_RSHIFT_W32((tmp1 + tmp2), 1);/* low pass signal*/
    LP16[k] = (WebRtc_Word16)WebRtcSpl_SatW32ToW16(tmp3); /*low pass */
    tmp3 = (WebRtc_Word32)WEBRTC_SPL_RSHIFT_W32((tmp1 - tmp2), 1);/* high pass signal*/
    HP16[k] = (WebRtc_Word16)WebRtcSpl_SatW32ToW16(tmp3); /*high pass */
  }

}/*end of WebRtcIsacfix_SplitAndFilter */

#endif



//////////////////////////////////////////////////////////
////////// Combining
/* Function WebRtcIsacfix_FilterAndCombine */
/* This is a decoder function that takes the decimated
   length FRAMESAMPLES/2 input low-pass and
   high-pass signals and creates a reconstructed fullband
   output signal of length FRAMESAMPLES. WebRtcIsacfix_FilterAndCombine
   is the sibling function of WebRtcIsacfix_SplitAndFilter */
/* INPUTS:
   inLP: a length FRAMESAMPLES/2 array of input low-pass
   samples.
   inHP: a length FRAMESAMPLES/2 array of input high-pass
   samples.
   postfiltdata: input data structure containing the filterbank
   states from the previous decoding iteration.
   OUTPUTS:
   Out: a length FRAMESAMPLES array of output reconstructed
   samples (fullband) based on the input low-pass and
   high-pass signals.
   postfiltdata: the input data structure containing the filterbank
   states is updated for the next decoding iteration */
void WebRtcIsacfix_FilterAndCombine1(WebRtc_Word16 *tempin_ch1,
                                     WebRtc_Word16 *tempin_ch2,
                                     WebRtc_Word16 *out16,
                                     PostFiltBankstr *postfiltdata)
{
  int k;
  WebRtc_Word16 in[FRAMESAMPLES];

  /* all-pass filter the new upper channel signal. HOWEVER, use the all-pass filter factors
     that were used as a lower channel at the encoding side.  So at the decoder, the
     corresponding all-pass filter factors for each channel are swapped.*/

  AllpassFilter2FixDec16(tempin_ch1, WebRtcIsacfix_kLowerApFactorsQ15, FRAMESAMPLES/2, NUMBEROFCHANNELAPSECTIONS,postfiltdata->STATE_0_UPPER_fix);

  /* Now, all-pass filter the new lower channel signal. But since all-pass filter factors
     at the decoder are swapped from the ones at the encoder, the 'upper' channel
     all-pass filter factors (kUpperApFactors) are used to filter this new lower channel signal */

  AllpassFilter2FixDec16(tempin_ch2, WebRtcIsacfix_kUpperApFactorsQ15, FRAMESAMPLES/2, NUMBEROFCHANNELAPSECTIONS,postfiltdata->STATE_0_LOWER_fix);

  /* Merge outputs to form the full length output signal.*/
  for (k=0;k<FRAMESAMPLES/2;k++) {
    in[WEBRTC_SPL_MUL_16_16(2, k)]=tempin_ch2[k];
    in[WEBRTC_SPL_MUL_16_16(2, k)+1]=tempin_ch1[k];
  }

  /* High pass filter */
  HighpassFilterFixDec32(in, FRAMESAMPLES, WebRtcIsacfix_kHPStCoeffOut1Q30, postfiltdata->HPstates1_fix);
  HighpassFilterFixDec32(in, FRAMESAMPLES, WebRtcIsacfix_kHPStCoeffOut2Q30, postfiltdata->HPstates2_fix);

  for (k=0;k<FRAMESAMPLES;k++) {
    out16[k] = in[k];
  }
}


#ifdef WEBRTC_ISAC_FIX_NB_CALLS_ENABLED
/* Function WebRtcIsacfix_FilterAndCombine */
/* This is a decoder function that takes the decimated
   length len/2 input low-pass and
   high-pass signals and creates a reconstructed fullband
   output signal of length len. WebRtcIsacfix_FilterAndCombine
   is the sibling function of WebRtcIsacfix_SplitAndFilter */
/* INPUTS:
   inLP: a length len/2 array of input low-pass
   samples.
   inHP: a length len/2 array of input high-pass
   samples.
   postfiltdata: input data structure containing the filterbank
   states from the previous decoding iteration.
   OUTPUTS:
   Out: a length len array of output reconstructed
   samples (fullband) based on the input low-pass and
   high-pass signals.
   postfiltdata: the input data structure containing the filterbank
   states is updated for the next decoding iteration */
void WebRtcIsacfix_FilterAndCombine2(WebRtc_Word16 *tempin_ch1,
                                     WebRtc_Word16 *tempin_ch2,
                                     WebRtc_Word16 *out16,
                                     PostFiltBankstr *postfiltdata,
                                     WebRtc_Word16 len)
{
  int k;
  WebRtc_Word16 in[FRAMESAMPLES];

  /* all-pass filter the new upper channel signal. HOWEVER, use the all-pass filter factors
     that were used as a lower channel at the encoding side.  So at the decoder, the
     corresponding all-pass filter factors for each channel are swapped.*/

  AllpassFilter2FixDec16(tempin_ch1, WebRtcIsacfix_kLowerApFactorsQ15,(WebRtc_Word16) (len/2), NUMBEROFCHANNELAPSECTIONS,postfiltdata->STATE_0_UPPER_fix);

  /* Now, all-pass filter the new lower channel signal. But since all-pass filter factors
     at the decoder are swapped from the ones at the encoder, the 'upper' channel
     all-pass filter factors (kUpperApFactors) are used to filter this new lower channel signal */

  AllpassFilter2FixDec16(tempin_ch2, WebRtcIsacfix_kUpperApFactorsQ15, (WebRtc_Word16) (len/2), NUMBEROFCHANNELAPSECTIONS,postfiltdata->STATE_0_LOWER_fix);

  /* Merge outputs to form the full length output signal.*/
  for (k=0;k<len/2;k++) {
    in[WEBRTC_SPL_MUL_16_16(2, k)]=tempin_ch2[k];
    in[WEBRTC_SPL_MUL_16_16(2, k)+1]=tempin_ch1[k];
  }

  /* High pass filter */
  HighpassFilterFixDec32(in, len, WebRtcIsacfix_kHPStCoeffOut1Q30, postfiltdata->HPstates1_fix);
  HighpassFilterFixDec32(in, len, WebRtcIsacfix_kHPStCoeffOut2Q30, postfiltdata->HPstates2_fix);

  for (k=0;k<len;k++) {
    out16[k] = in[k];
  }
}

#endif
