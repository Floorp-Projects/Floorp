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
 * encode.c
 *
 * This file contains definition of funtions for encoding.
 * Decoding of upper-band, including 8-12 kHz, when the bandwidth is
 * 0-12 kHz, and 8-16 kHz, when the bandwidth is 0-16 kHz.
 *
 */

#include <stdlib.h>
#include <string.h>

#include "structs.h"
#include "codec.h"
#include "pitch_estimator.h"
#include "entropy_coding.h"
#include "arith_routines.h"
#include "pitch_gain_tables.h"
#include "pitch_lag_tables.h"
#include "spectrum_ar_model_tables.h"
#include "lpc_tables.h"
#include "lpc_analysis.h"
#include "bandwidth_estimator.h"
#include "lpc_shape_swb12_tables.h"
#include "lpc_shape_swb16_tables.h"
#include "lpc_gain_swb_tables.h"


#define UB_LOOKAHEAD 24

/*
  Rate allocation tables of lower and upper-band bottleneck for
  12kHz & 16kHz bandwidth.

  12 kHz bandwidth
  -----------------
  The overall bottleneck of the coder is between 38 kbps and 45 kbps. We have
  considered 7 enteries, uniformly distributed in this interval, i.e. 38,
  39.17, 40.33, 41.5, 42.67, 43.83 and 45. For every entery, the lower-band
  and the upper-band bottlenecks are specified in
  'kLowerBandBitRate12' and 'kUpperBandBitRate12'
  tables, respectively. E.g. the overall rate of 41.5 kbps corresponts to a
  bottleneck of 31 kbps for lower-band and 27 kbps for upper-band. Given an
  overall bottleneck of the codec, we use linear interpolation to get
  lower-band and upper-band bottlenecks.

  16 kHz bandwidth
  -----------------
  The overall bottleneck of the coder is between 50 kbps and 56 kbps. We have
  considered 7 enteries, uniformly distributed in this interval, i.e. 50, 51.2,
  52.4, 53.6, 54.8 and 56. For every entery, the lower-band and the upper-band
  bottlenecks are specified in 'kLowerBandBitRate16' and
  'kUpperBandBitRate16' tables, respectively. E.g. the overall rate
  of 53.6 kbps corresponts to a bottleneck of 32 kbps for lower-band and 30
  kbps for upper-band. Given an overall bottleneck of the codec, we use linear
  interpolation to get lower-band and upper-band bottlenecks.

*/

//     38  39.17  40.33   41.5  42.67  43.83     45
static const WebRtc_Word16 kLowerBandBitRate12[7] = {
  29000, 30000, 30000, 31000, 31000, 32000, 32000};
static const WebRtc_Word16 kUpperBandBitRate12[7] = {
  25000, 25000, 27000, 27000, 29000, 29000, 32000};

//    50     51.2  52.4   53.6   54.8    56
static const WebRtc_Word16 kLowerBandBitRate16[6] = {
  31000, 31000, 32000, 32000, 32000, 32000};
static const WebRtc_Word16 kUpperBandBitRate16[6] = {
  28000, 29000, 29000, 30000, 31000, 32000};

/******************************************************************************
 * WebRtcIsac_RateAllocation()
 * Internal function to perform a rate-allocation for upper and lower-band,
 * given a total rate.
 *
 * Input:
 *   - inRateBitPerSec           : a total bottleneck in bits/sec.
 *
 * Output:
 *   - rateLBBitPerSec           : a bottleneck allocated to the lower-band
 *                                 in bits/sec.
 *   - rateUBBitPerSec           : a bottleneck allocated to the upper-band
 *                                 in bits/sec.
 *
 * Return value                  : 0 if rate allocation has been successful.
 *                                -1 if failed to allocate rates.
 */

WebRtc_Word16
WebRtcIsac_RateAllocation(
    WebRtc_Word32         inRateBitPerSec,
    double*             rateLBBitPerSec,
    double*             rateUBBitPerSec,
    enum ISACBandwidth* bandwidthKHz)
{
  WebRtc_Word16 idx;
  double idxD;
  double idxErr;
  if(inRateBitPerSec < 38000)
  {
    // If the given overall bottleneck is less than 38000 then
    // then codec has to operate in wideband mode, i.e. 8 kHz
    // bandwidth.
    *rateLBBitPerSec = (WebRtc_Word16)((inRateBitPerSec > 32000)?
                                       32000:inRateBitPerSec);
    *rateUBBitPerSec = 0;
    *bandwidthKHz = isac8kHz;
  }
  else if((inRateBitPerSec >= 38000) && (inRateBitPerSec < 50000))
  {
    // At a bottleneck between 38 and 50 kbps the codec is operating
    // at 12 kHz bandwidth. Using xxxBandBitRate12[] to calculates
    // upper/lower bottleneck

    // find the bottlenecks by linear interpolation
    // step is (45000 - 38000)/6.0 we use the inverse of it.
    const double stepSizeInv = 8.5714286e-4;
    idxD = (inRateBitPerSec - 38000) * stepSizeInv;
    idx = (idxD >= 6)? 6:((WebRtc_Word16)idxD);
    idxErr = idxD - idx;
    *rateLBBitPerSec = kLowerBandBitRate12[idx];
    *rateUBBitPerSec = kUpperBandBitRate12[idx];

    if(idx < 6)
    {
      *rateLBBitPerSec += (WebRtc_Word16)(idxErr *
                                          (kLowerBandBitRate12[idx + 1] -
                                           kLowerBandBitRate12[idx]));
      *rateUBBitPerSec += (WebRtc_Word16)(idxErr *
                                          (kUpperBandBitRate12[idx + 1] -
                                           kUpperBandBitRate12[idx]));
    }

    *bandwidthKHz = isac12kHz;
  }
  else if((inRateBitPerSec >= 50000) && (inRateBitPerSec <= 56000))
  {
    // A bottleneck between 50 and 56 kbps corresponds to bandwidth
    // of 16 kHz. Using xxxBandBitRate16[] to calculates
    // upper/lower bottleneck

    // find the bottlenecks by linear interpolation
    // step is (56000 - 50000)/5 we use the inverse of it
    const double stepSizeInv = 8.3333333e-4;
    idxD = (inRateBitPerSec - 50000) * stepSizeInv;
    idx = (idxD >= 5)? 5:((WebRtc_Word16)idxD);
    idxErr = idxD - idx;
    *rateLBBitPerSec = kLowerBandBitRate16[idx];
    *rateUBBitPerSec  = kUpperBandBitRate16[idx];

    if(idx < 5)
    {
      *rateLBBitPerSec += (WebRtc_Word16)(idxErr *
                                          (kLowerBandBitRate16[idx + 1] -
                                           kLowerBandBitRate16[idx]));

      *rateUBBitPerSec += (WebRtc_Word16)(idxErr *
                                          (kUpperBandBitRate16[idx + 1] -
                                           kUpperBandBitRate16[idx]));
    }

    *bandwidthKHz = isac16kHz;
  }
  else
  {
    // Out-of-range botlteneck value.
    return -1;
  }

  // limit the values.
  *rateLBBitPerSec = (*rateLBBitPerSec > 32000)? 32000:*rateLBBitPerSec;
  *rateUBBitPerSec = (*rateUBBitPerSec > 32000)? 32000:*rateUBBitPerSec;

  return 0;
}



int
WebRtcIsac_EncodeLb(
    float*           in,
    ISACLBEncStruct* ISACencLB_obj,
    WebRtc_Word16      codingMode,
    WebRtc_Word16      bottleneckIndex)
{
  int stream_length = 0;
  int err;
  int k;
  int iterCntr;

  double lofilt_coef[(ORDERLO+1)*SUBFRAMES];
  double hifilt_coef[(ORDERHI+1)*SUBFRAMES];
  float LP[FRAMESAMPLES_HALF];
  float HP[FRAMESAMPLES_HALF];

  double LP_lookahead[FRAMESAMPLES_HALF];
  double HP_lookahead[FRAMESAMPLES_HALF];
  double LP_lookahead_pf[FRAMESAMPLES_HALF + QLOOKAHEAD];
  double LPw[FRAMESAMPLES_HALF];

  double HPw[FRAMESAMPLES_HALF];
  double LPw_pf[FRAMESAMPLES_HALF];
  WebRtc_Word16 fre[FRAMESAMPLES_HALF];   /* Q7 */
  WebRtc_Word16 fim[FRAMESAMPLES_HALF];   /* Q7 */

  double PitchLags[4];
  double PitchGains[4];
  WebRtc_Word16 PitchGains_Q12[4];
  WebRtc_Word16 AvgPitchGain_Q12;

  int frame_mode; /* 0 for 30ms, 1 for 60ms */
  int processed_samples, status = 0;

  double bits_gains;
  int bmodel;

  transcode_obj transcodingParam;
  double bytesLeftSpecCoding;
  WebRtc_UWord16 payloadLimitBytes;

  /* copy new frame length and bottle neck rate only for the first
     10 ms data */
  if (ISACencLB_obj->buffer_index == 0) {
    /* set the framelength for the next packet */
    ISACencLB_obj->current_framesamples = ISACencLB_obj->new_framelength;
  }
  /* frame_mode is 0 (30 ms) or 1 (60 ms)  */
  frame_mode = ISACencLB_obj->current_framesamples/MAX_FRAMESAMPLES;
  /* processed_samples: 480 (30, 60 ms) */
  processed_samples = ISACencLB_obj->current_framesamples/(frame_mode+1);

  /* buffer speech samples (by 10ms packet) until the framelength */
  /* is reached (30 or 60 ms)                                     */
  /****************************************************************/

  /* fill the buffer with 10ms input data */
  for (k = 0; k < FRAMESAMPLES_10ms; k++) {
    ISACencLB_obj->data_buffer_float[k + ISACencLB_obj->buffer_index] =
        in[k];
  }

  /* if buffersize is not equal to current framesize then increase index
     and return. We do no encoding untill we have enough audio.  */
  if (ISACencLB_obj->buffer_index + FRAMESAMPLES_10ms != processed_samples) {
    ISACencLB_obj->buffer_index += FRAMESAMPLES_10ms;
    return 0;
  }
  /* if buffer reached the right size, reset index and continue with
     encoding the frame */
  ISACencLB_obj->buffer_index = 0;

  /* end of buffer function */
  /**************************/

  /* encoding */
  /************/

  if (frame_mode == 0 || ISACencLB_obj->frame_nb == 0 ) {
    // This is to avoid Linux warnings until we change 'int' to 'Word32'
    // at all places.
    int intVar;
    /* reset bitstream */
    ISACencLB_obj->bitstr_obj.W_upper = 0xFFFFFFFF;
    ISACencLB_obj->bitstr_obj.streamval = 0;
    ISACencLB_obj->bitstr_obj.stream_index = 0;

    if((codingMode == 0) && (frame_mode == 0) &&
       (ISACencLB_obj->enforceFrameSize == 0)) {
      ISACencLB_obj->new_framelength =
          WebRtcIsac_GetNewFrameLength(ISACencLB_obj->bottleneck,
                                       ISACencLB_obj->current_framesamples);
    }

    ISACencLB_obj->s2nr = WebRtcIsac_GetSnr(
        ISACencLB_obj->bottleneck, ISACencLB_obj->current_framesamples);

    /* encode frame length */
    status = WebRtcIsac_EncodeFrameLen(
        ISACencLB_obj->current_framesamples, &ISACencLB_obj->bitstr_obj);
    if (status < 0) {
      /* Wrong frame size */
      return status;
    }
    /* Save framelength for multiple packets memory */
    ISACencLB_obj->SaveEnc_obj.framelength =
        ISACencLB_obj->current_framesamples;

    /* To be used for Redundant Coding */
    ISACencLB_obj->lastBWIdx = bottleneckIndex;
    intVar = (int)bottleneckIndex;
    WebRtcIsac_EncodeReceiveBw(&intVar, &ISACencLB_obj->bitstr_obj);
  }

  /* split signal in two bands */
  WebRtcIsac_SplitAndFilterFloat(ISACencLB_obj->data_buffer_float, LP, HP,
                                 LP_lookahead, HP_lookahead,    &ISACencLB_obj->prefiltbankstr_obj );

  /* estimate pitch parameters and pitch-filter lookahead signal */
  WebRtcIsac_PitchAnalysis(LP_lookahead, LP_lookahead_pf,
                           &ISACencLB_obj->pitchanalysisstr_obj, PitchLags, PitchGains);

  /* encode in FIX Q12 */

  /* convert PitchGain to Fixed point */
  for (k=0;k<PITCH_SUBFRAMES;k++) {
    PitchGains_Q12[k] = (WebRtc_Word16)(PitchGains[k] * 4096.0);
  }

  /* Set where to store data in multiple packets memory */
  if (frame_mode == 0 || ISACencLB_obj->frame_nb == 0)
  {
    ISACencLB_obj->SaveEnc_obj.startIdx = 0;
  } else {
    ISACencLB_obj->SaveEnc_obj.startIdx = 1;
  }

  /* quantize & encode pitch parameters */
  WebRtcIsac_EncodePitchGain(PitchGains_Q12, &ISACencLB_obj->bitstr_obj,
                             &ISACencLB_obj->SaveEnc_obj);
  WebRtcIsac_EncodePitchLag(PitchLags, PitchGains_Q12,
                            &ISACencLB_obj->bitstr_obj, &ISACencLB_obj->SaveEnc_obj);

  AvgPitchGain_Q12 = (PitchGains_Q12[0] + PitchGains_Q12[1] +
                      PitchGains_Q12[2] + PitchGains_Q12[3])>>2;

  /* find coefficients for perceptual pre-filters */
  WebRtcIsac_GetLpcCoefLb(LP_lookahead_pf, HP_lookahead,
                          &ISACencLB_obj->maskfiltstr_obj, ISACencLB_obj->s2nr,
                          PitchGains_Q12, lofilt_coef, hifilt_coef);

  /* code LPC model and shape - gains not quantized yet */
  WebRtcIsac_EncodeLpcLb(lofilt_coef, hifilt_coef,  &bmodel, &bits_gains,
                         &ISACencLB_obj->bitstr_obj, &ISACencLB_obj->SaveEnc_obj);

  /* convert PitchGains back to FLOAT for pitchfilter_pre */
  for (k = 0; k < 4; k++) {
    PitchGains[k] = ((float)PitchGains_Q12[k])/4096;
  }

  /* Store the state of arithmetic coder before coding LPC gains */
  transcodingParam.W_upper      = ISACencLB_obj->bitstr_obj.W_upper;
  transcodingParam.stream_index = ISACencLB_obj->bitstr_obj.stream_index;
  transcodingParam.streamval    = ISACencLB_obj->bitstr_obj.streamval;
  transcodingParam.stream[0]    = ISACencLB_obj->bitstr_obj.stream[
      ISACencLB_obj->bitstr_obj.stream_index - 2];
  transcodingParam.stream[1]    = ISACencLB_obj->bitstr_obj.stream[
      ISACencLB_obj->bitstr_obj.stream_index - 1];
  transcodingParam.stream[2]    = ISACencLB_obj->bitstr_obj.stream[
      ISACencLB_obj->bitstr_obj.stream_index];

  /* Store LPC Gains before encoding them */
  for(k = 0; k < SUBFRAMES; k++) {
    transcodingParam.loFiltGain[k] = lofilt_coef[(LPC_LOBAND_ORDER+1)*k];
    transcodingParam.hiFiltGain[k] = hifilt_coef[(LPC_HIBAND_ORDER+1)*k];
  }

  /* Code gains */
  WebRtcIsac_EncodeLpcGainLb(lofilt_coef, hifilt_coef,  bmodel,
                             &ISACencLB_obj->bitstr_obj, &ISACencLB_obj->SaveEnc_obj);

  /* Get the correct value for the payload limit and calculate the
     number of bytes left for coding the spectrum.*/
  if((frame_mode == 1) && (ISACencLB_obj->frame_nb == 0)) {
    /* It is a 60ms and we are in the first 30ms then the limit at
       this point should be half of the assigned value */
    payloadLimitBytes = ISACencLB_obj->payloadLimitBytes60 >> 1;
  }
  else if (frame_mode == 0) {
    /* It is a 30ms frame */
    /* Subract 3 because termination process may add 3 bytes */
    payloadLimitBytes = ISACencLB_obj->payloadLimitBytes30 - 3;
  } else {
    /* This is the second half of a 60ms frame. */
    /* Subract 3 because termination process may add 3 bytes */
    payloadLimitBytes = ISACencLB_obj->payloadLimitBytes60 - 3;
  }
  bytesLeftSpecCoding = payloadLimitBytes - transcodingParam.stream_index;

  /* perceptual pre-filtering (using normalized lattice filter) */
  /* low-band filtering */
  WebRtcIsac_NormLatticeFilterMa(ORDERLO,
                                  ISACencLB_obj->maskfiltstr_obj.PreStateLoF,
                                  ISACencLB_obj->maskfiltstr_obj.PreStateLoG, LP, lofilt_coef, LPw);
  /* high-band filtering */
  WebRtcIsac_NormLatticeFilterMa(ORDERHI,
                                  ISACencLB_obj->maskfiltstr_obj.PreStateHiF,
                                  ISACencLB_obj->maskfiltstr_obj.PreStateHiG, HP, hifilt_coef, HPw);


  /* pitch filter */
  WebRtcIsac_PitchfilterPre(LPw, LPw_pf, &ISACencLB_obj->pitchfiltstr_obj,
                             PitchLags, PitchGains);

  /* transform */
  WebRtcIsac_Time2Spec(LPw_pf, HPw, fre, fim, &ISACencLB_obj->fftstr_obj);


  /* Save data for multiple packets memory */
  for (k = 0; k < FRAMESAMPLES_HALF; k++) {
    ISACencLB_obj->SaveEnc_obj.fre[k +
                                   ISACencLB_obj->SaveEnc_obj.startIdx*FRAMESAMPLES_HALF] = fre[k];
    ISACencLB_obj->SaveEnc_obj.fim[k +
                                   ISACencLB_obj->SaveEnc_obj.startIdx*FRAMESAMPLES_HALF] = fim[k];
  }
  ISACencLB_obj->SaveEnc_obj.AvgPitchGain[
      ISACencLB_obj->SaveEnc_obj.startIdx] = AvgPitchGain_Q12;

  /* quantization and lossless coding */
  err = WebRtcIsac_EncodeSpecLb(fre, fim, &ISACencLB_obj->bitstr_obj,
                                AvgPitchGain_Q12);
  if ((err < 0) && (err != -ISAC_DISALLOWED_BITSTREAM_LENGTH)) {
    /* There has been an error but it was not too large payload
       (we can cure too large payload) */
    if (frame_mode == 1 && ISACencLB_obj->frame_nb == 1) {
      /* If this is the second 30ms of a 60ms frame reset
         this such that in the next call encoder starts fresh. */
      ISACencLB_obj->frame_nb = 0;
    }
    return err;
  }
  iterCntr = 0;
  while((ISACencLB_obj->bitstr_obj.stream_index > payloadLimitBytes) ||
        (err == -ISAC_DISALLOWED_BITSTREAM_LENGTH)) {
    double bytesSpecCoderUsed;
    double transcodeScale;

    if(iterCntr >= MAX_PAYLOAD_LIMIT_ITERATION) {
      /* We were not able to limit the payload size */
      if((frame_mode == 1) && (ISACencLB_obj->frame_nb == 0)) {
        /* This was the first 30ms of a 60ms frame. Although
           the payload is larger than it should be but we let
           the second 30ms be encoded. Maybe together we
           won't exceed the limit. */
        ISACencLB_obj->frame_nb = 1;
        return 0;
      } else if((frame_mode == 1) && (ISACencLB_obj->frame_nb == 1)) {
        ISACencLB_obj->frame_nb = 0;
      }

      if(err != -ISAC_DISALLOWED_BITSTREAM_LENGTH) {
        return -ISAC_PAYLOAD_LARGER_THAN_LIMIT;
      } else {
        return status;
      }
    }

    if(err == -ISAC_DISALLOWED_BITSTREAM_LENGTH) {
      bytesSpecCoderUsed = STREAM_SIZE_MAX;
      // being coservative
      transcodeScale = bytesLeftSpecCoding / bytesSpecCoderUsed * 0.5;
    } else {
      bytesSpecCoderUsed = ISACencLB_obj->bitstr_obj.stream_index -
          transcodingParam.stream_index;
      transcodeScale = bytesLeftSpecCoding / bytesSpecCoderUsed;
    }

    /* To be safe, we reduce the scale depending on
       the number of iterations. */
    transcodeScale *= (1.0 - (0.9 * (double)iterCntr /
                              (double)MAX_PAYLOAD_LIMIT_ITERATION));

    /* Scale the LPC Gains */
    for (k = 0; k < SUBFRAMES; k++) {
      lofilt_coef[(LPC_LOBAND_ORDER+1) * k] =
          transcodingParam.loFiltGain[k] * transcodeScale;
      hifilt_coef[(LPC_HIBAND_ORDER+1) * k] =
          transcodingParam.hiFiltGain[k] * transcodeScale;
      transcodingParam.loFiltGain[k] =
          lofilt_coef[(LPC_LOBAND_ORDER+1) * k];
      transcodingParam.hiFiltGain[k] =
          hifilt_coef[(LPC_HIBAND_ORDER+1) * k];
    }

    /* Scale DFT coefficients */
    for (k = 0; k < FRAMESAMPLES_HALF; k++) {
      fre[k] = (WebRtc_Word16)(fre[k] * transcodeScale);
      fim[k] = (WebRtc_Word16)(fim[k] * transcodeScale);
    }

    /* Save data for multiple packets memory */
    for (k = 0; k < FRAMESAMPLES_HALF; k++) {
      ISACencLB_obj->SaveEnc_obj.fre[k +
                                     ISACencLB_obj->SaveEnc_obj.startIdx * FRAMESAMPLES_HALF] =
          fre[k];
      ISACencLB_obj->SaveEnc_obj.fim[k +
                                     ISACencLB_obj->SaveEnc_obj.startIdx * FRAMESAMPLES_HALF] =
          fim[k];
    }

    /* Re-store the state of arithmetic coder before coding LPC gains */
    ISACencLB_obj->bitstr_obj.W_upper = transcodingParam.W_upper;
    ISACencLB_obj->bitstr_obj.stream_index = transcodingParam.stream_index;
    ISACencLB_obj->bitstr_obj.streamval = transcodingParam.streamval;
    ISACencLB_obj->bitstr_obj.stream[transcodingParam.stream_index - 2] =
        transcodingParam.stream[0];
    ISACencLB_obj->bitstr_obj.stream[transcodingParam.stream_index - 1] =
        transcodingParam.stream[1];
    ISACencLB_obj->bitstr_obj.stream[transcodingParam.stream_index] =
        transcodingParam.stream[2];

    /* Code gains */
    WebRtcIsac_EncodeLpcGainLb(lofilt_coef, hifilt_coef,  bmodel,
                               &ISACencLB_obj->bitstr_obj, &ISACencLB_obj->SaveEnc_obj);

    /* Update the number of bytes left for encoding the spectrum */
    bytesLeftSpecCoding = payloadLimitBytes -
        transcodingParam.stream_index;

    /* Encode the spectrum */
    err = WebRtcIsac_EncodeSpecLb(fre, fim, &ISACencLB_obj->bitstr_obj,
                                  AvgPitchGain_Q12);
    if((err < 0) && (err != -ISAC_DISALLOWED_BITSTREAM_LENGTH)) {
      /* There has been an error but it was not too large
         payload (we can cure too large payload) */
      if (frame_mode == 1 && ISACencLB_obj->frame_nb == 1) {
        /* If this is the second 30ms of a 60ms frame reset
           this such that in the next call encoder starts fresh. */
        ISACencLB_obj->frame_nb = 0;
      }
      return err;
    }
    iterCntr++;
  }

  /* i.e. 60 ms framesize and just processed the first 30ms, */
  /* go back to main function to buffer the other 30ms speech frame */
  if (frame_mode == 1)
  {
    if(ISACencLB_obj->frame_nb == 0)
    {
      ISACencLB_obj->frame_nb = 1;
      return 0;
    }
    else if(ISACencLB_obj->frame_nb == 1)
    {
      ISACencLB_obj->frame_nb = 0;
      /* also update the framelength for next packet,
         in Adaptive mode only */
      if (codingMode == 0 && (ISACencLB_obj->enforceFrameSize == 0))
      {
        ISACencLB_obj->new_framelength =
            WebRtcIsac_GetNewFrameLength(ISACencLB_obj->bottleneck,
                                         ISACencLB_obj->current_framesamples);
      }
    }
  }
  else
  {
    ISACencLB_obj->frame_nb = 0;
  }

  /* complete arithmetic coding */
  stream_length = WebRtcIsac_EncTerminate(&ISACencLB_obj->bitstr_obj);

  return stream_length;
}

int
WebRtcIsac_EncodeUb16(
    float*           in,
    ISACUBEncStruct* ISACencUB_obj,
    WebRtc_Word32      jitterInfo)
{
  int err;
  int k;

  double lpcVecs[UB_LPC_ORDER * UB16_LPC_VEC_PER_FRAME];
  double percepFilterParams[(1 + UB_LPC_ORDER) * (SUBFRAMES<<1) +
                            (1 + UB_LPC_ORDER)];

  double LP_lookahead[FRAMESAMPLES];
  WebRtc_Word16 fre[FRAMESAMPLES_HALF];   /* Q7 */
  WebRtc_Word16 fim[FRAMESAMPLES_HALF];   /* Q7 */

  int status = 0;

  double varscale[2];
  double corr[SUBFRAMES<<1][UB_LPC_ORDER + 1];
  double lpcGains[SUBFRAMES<<1];
  transcode_obj transcodingParam;
  double bytesLeftSpecCoding;
  WebRtc_UWord16 payloadLimitBytes;
  WebRtc_UWord16 iterCntr;
  double s2nr;

  /* buffer speech samples (by 10ms packet) until the framelength is   */
  /* reached (30 or 60 ms)                                             */
  /*********************************************************************/

  /* fill the buffer with 10ms input data */
  for (k = 0; k < FRAMESAMPLES_10ms; k++) {
    ISACencUB_obj->data_buffer_float[k + ISACencUB_obj->buffer_index] =
        in[k];
  }

  /* if buffersize is not equal to current framesize, and end of file is
     not reached yet, we don't do encoding unless we have the whole frame */
  if (ISACencUB_obj->buffer_index + FRAMESAMPLES_10ms < FRAMESAMPLES) {
    ISACencUB_obj->buffer_index += FRAMESAMPLES_10ms;
    return 0;
  }

  /* end of buffer function */
  /**************************/

  /* encoding */
  /************/

  /* reset bitstream */
  ISACencUB_obj->bitstr_obj.W_upper = 0xFFFFFFFF;
  ISACencUB_obj->bitstr_obj.streamval = 0;
  ISACencUB_obj->bitstr_obj.stream_index = 0;

  /* bandwidth estimation and coding */
  /* To be used for Redundant Coding */
  WebRtcIsac_EncodeJitterInfo(jitterInfo, &ISACencUB_obj->bitstr_obj);

  status = WebRtcIsac_EncodeBandwidth(isac16kHz,
                                      &ISACencUB_obj->bitstr_obj);
  if (status < 0) {
    return status;
  }

  s2nr = WebRtcIsac_GetSnr(ISACencUB_obj->bottleneck,
                                 FRAMESAMPLES);

  memcpy(lpcVecs, ISACencUB_obj->lastLPCVec, UB_LPC_ORDER * sizeof(double));

  for (k = 0; k < FRAMESAMPLES; k++) {
    LP_lookahead[k] = ISACencUB_obj->data_buffer_float[UB_LOOKAHEAD + k];
  }

  /* find coefficients for perceptual pre-filters */
  WebRtcIsac_GetLpcCoefUb(LP_lookahead, &ISACencUB_obj->maskfiltstr_obj,
                            &lpcVecs[UB_LPC_ORDER], corr, varscale, isac16kHz);

  memcpy(ISACencUB_obj->lastLPCVec,
         &lpcVecs[(UB16_LPC_VEC_PER_FRAME - 1) * (UB_LPC_ORDER)],
         sizeof(double) * UB_LPC_ORDER);

  /* code LPC model and shape - gains not quantized yet */
  WebRtcIsac_EncodeLpcUB(lpcVecs, &ISACencUB_obj->bitstr_obj,
                           percepFilterParams, isac16kHz, &ISACencUB_obj->SaveEnc_obj);


  // the first set of lpc parameters are from the last sub-frame of
  // the previous frame. so we don't care about them
  WebRtcIsac_GetLpcGain(s2nr, &percepFilterParams[UB_LPC_ORDER + 1],
                       (SUBFRAMES<<1), lpcGains, corr, varscale);

  /* Store the state of arithmetic coder before coding LPC gains */
  transcodingParam.stream_index = ISACencUB_obj->bitstr_obj.stream_index;
  transcodingParam.W_upper      = ISACencUB_obj->bitstr_obj.W_upper;
  transcodingParam.streamval    = ISACencUB_obj->bitstr_obj.streamval;
  transcodingParam.stream[0]    = ISACencUB_obj->bitstr_obj.stream[
      ISACencUB_obj->bitstr_obj.stream_index - 2];
  transcodingParam.stream[1]    = ISACencUB_obj->bitstr_obj.stream[
      ISACencUB_obj->bitstr_obj.stream_index - 1];
  transcodingParam.stream[2]    = ISACencUB_obj->bitstr_obj.stream[
      ISACencUB_obj->bitstr_obj.stream_index];

  /* Store LPC Gains before encoding them */
  for(k = 0; k < SUBFRAMES; k++) {
    transcodingParam.loFiltGain[k] = lpcGains[k];
    transcodingParam.hiFiltGain[k] = lpcGains[SUBFRAMES + k];
  }

  // Store the gains for multiple encoding
  memcpy(ISACencUB_obj->SaveEnc_obj.lpcGain, lpcGains, (SUBFRAMES << 1) * sizeof(double));

  WebRtcIsac_EncodeLpcGainUb(lpcGains, &ISACencUB_obj->bitstr_obj,
                             ISACencUB_obj->SaveEnc_obj.lpcGainIndex);
  WebRtcIsac_EncodeLpcGainUb(&lpcGains[SUBFRAMES], &ISACencUB_obj->bitstr_obj,
                             &ISACencUB_obj->SaveEnc_obj.lpcGainIndex[SUBFRAMES]);

  /* Get the correct value for the payload limit and calculate the number of
     bytes left for coding the spectrum. It is a 30ms frame
     Subract 3 because termination process may add 3 bytes */
  payloadLimitBytes = ISACencUB_obj->maxPayloadSizeBytes -
      ISACencUB_obj->numBytesUsed - 3;
  bytesLeftSpecCoding = payloadLimitBytes -
      ISACencUB_obj->bitstr_obj.stream_index;

  for (k = 0; k < (SUBFRAMES<<1); k++) {
    percepFilterParams[k*(UB_LPC_ORDER + 1) + (UB_LPC_ORDER + 1)] =
        lpcGains[k];
  }

  /* perceptual pre-filtering (using normalized lattice filter) */
  /* first half-frame filtering */
  WebRtcIsac_NormLatticeFilterMa(UB_LPC_ORDER,
                                  ISACencUB_obj->maskfiltstr_obj.PreStateLoF,
                                  ISACencUB_obj->maskfiltstr_obj.PreStateLoG,
                                  &ISACencUB_obj->data_buffer_float[0],
                                  &percepFilterParams[UB_LPC_ORDER + 1],
                                  &LP_lookahead[0]);

  /* Second half-frame filtering */
  WebRtcIsac_NormLatticeFilterMa(UB_LPC_ORDER,
                                  ISACencUB_obj->maskfiltstr_obj.PreStateLoF,
                                  ISACencUB_obj->maskfiltstr_obj.PreStateLoG,
                                  &ISACencUB_obj->data_buffer_float[FRAMESAMPLES_HALF],
                                  &percepFilterParams[(UB_LPC_ORDER + 1) + SUBFRAMES *
                                                      (UB_LPC_ORDER + 1)], &LP_lookahead[FRAMESAMPLES_HALF]);

  WebRtcIsac_Time2Spec(&LP_lookahead[0], &LP_lookahead[FRAMESAMPLES_HALF],
                      fre, fim, &ISACencUB_obj->fftstr_obj);

  //Store FFT coefficients for multiple encoding
  memcpy(&ISACencUB_obj->SaveEnc_obj.realFFT, fre,
         FRAMESAMPLES_HALF * sizeof(WebRtc_Word16));

  memcpy(&ISACencUB_obj->SaveEnc_obj.imagFFT, fim,
         FRAMESAMPLES_HALF * sizeof(WebRtc_Word16));

  // Prepare the audio buffer for the next packet
  // move the last 3 ms to the beginning of the buffer
  memcpy(ISACencUB_obj->data_buffer_float,
         &ISACencUB_obj->data_buffer_float[FRAMESAMPLES],
         LB_TOTAL_DELAY_SAMPLES * sizeof(float));
  // start writing with 3 ms delay to compensate for the delay
  // of the lower-band.
  ISACencUB_obj->buffer_index = LB_TOTAL_DELAY_SAMPLES;

  // Save the bit-stream object at this point for FEC.
  memcpy(&ISACencUB_obj->SaveEnc_obj.bitStreamObj,
         &ISACencUB_obj->bitstr_obj, sizeof(Bitstr));

  /* quantization and lossless coding */
  err = WebRtcIsac_EncodeSpecUB16(fre, fim, &ISACencUB_obj->bitstr_obj);
  if ((err < 0) && (err != -ISAC_DISALLOWED_BITSTREAM_LENGTH)) {
    return err;
  }

  iterCntr = 0;
  while((ISACencUB_obj->bitstr_obj.stream_index > payloadLimitBytes) ||
        (err == -ISAC_DISALLOWED_BITSTREAM_LENGTH)) {
    double bytesSpecCoderUsed;
    double transcodeScale;

    if (iterCntr >= MAX_PAYLOAD_LIMIT_ITERATION) {
      /* We were not able to limit the payload size */
      return -ISAC_PAYLOAD_LARGER_THAN_LIMIT;
    }

    if (err == -ISAC_DISALLOWED_BITSTREAM_LENGTH) {
      bytesSpecCoderUsed = STREAM_SIZE_MAX;
      // being conservative
      transcodeScale = bytesLeftSpecCoding / bytesSpecCoderUsed * 0.5;
    } else {
      bytesSpecCoderUsed = ISACencUB_obj->bitstr_obj.stream_index -
          transcodingParam.stream_index;
      transcodeScale = bytesLeftSpecCoding / bytesSpecCoderUsed;
    }

    /* To be safe, we reduce the scale depending on the
       number of iterations. */
    transcodeScale *= (1.0 - (0.9 * (double)iterCntr/
                              (double)MAX_PAYLOAD_LIMIT_ITERATION));

    /* Scale the LPC Gains */
    for (k = 0; k < SUBFRAMES; k++) {
      transcodingParam.loFiltGain[k] *= transcodeScale;
      transcodingParam.hiFiltGain[k] *= transcodeScale;
    }

    /* Scale DFT coefficients */
    for (k = 0; k < FRAMESAMPLES_HALF; k++) {
      fre[k] = (WebRtc_Word16)(fre[k] * transcodeScale + 0.5);
      fim[k] = (WebRtc_Word16)(fim[k] * transcodeScale + 0.5);
    }

    //Store FFT coefficients for multiple encoding
    memcpy(&ISACencUB_obj->SaveEnc_obj.realFFT, fre,
           FRAMESAMPLES_HALF * sizeof(WebRtc_Word16));

    memcpy(&ISACencUB_obj->SaveEnc_obj.imagFFT, fim,
           FRAMESAMPLES_HALF * sizeof(WebRtc_Word16));


    /* Store the state of arithmetic coder before coding LPC gains */
    ISACencUB_obj->bitstr_obj.W_upper = transcodingParam.W_upper;

    ISACencUB_obj->bitstr_obj.stream_index = transcodingParam.stream_index;

    ISACencUB_obj->bitstr_obj.streamval = transcodingParam.streamval;

    ISACencUB_obj->bitstr_obj.stream[transcodingParam.stream_index - 2] =
        transcodingParam.stream[0];

    ISACencUB_obj->bitstr_obj.stream[transcodingParam.stream_index - 1] =
        transcodingParam.stream[1];

    ISACencUB_obj->bitstr_obj.stream[transcodingParam.stream_index] =
        transcodingParam.stream[2];

    // Store the gains for multiple encoding
    memcpy(ISACencUB_obj->SaveEnc_obj.lpcGain, lpcGains,
           (SUBFRAMES << 1) * sizeof(double));

    WebRtcIsac_EncodeLpcGainUb(transcodingParam.loFiltGain,
                               &ISACencUB_obj->bitstr_obj,
                               ISACencUB_obj->SaveEnc_obj.lpcGainIndex);
    WebRtcIsac_EncodeLpcGainUb(transcodingParam.hiFiltGain,
                               &ISACencUB_obj->bitstr_obj,
                               &ISACencUB_obj->SaveEnc_obj.lpcGainIndex[SUBFRAMES]);

    /* Update the number of bytes left for encoding the spectrum */
    bytesLeftSpecCoding = payloadLimitBytes -
        ISACencUB_obj->bitstr_obj.stream_index;

    // Save the bit-stream object at this point for FEC.
    memcpy(&ISACencUB_obj->SaveEnc_obj.bitStreamObj,
           &ISACencUB_obj->bitstr_obj, sizeof(Bitstr));

    /* Encode the spectrum */
    err = WebRtcIsac_EncodeSpecUB16(fre, fim, &ISACencUB_obj->bitstr_obj);
    if ((err < 0) && (err != -ISAC_DISALLOWED_BITSTREAM_LENGTH)) {
      /* There has been an error but it was not too large payload
         (we can cure too large payload) */
      return err;
    }
    iterCntr++;
  }

  /* complete arithmetic coding */
  return WebRtcIsac_EncTerminate(&ISACencUB_obj->bitstr_obj);
}


int
WebRtcIsac_EncodeUb12(
    float*           in,
    ISACUBEncStruct* ISACencUB_obj,
    WebRtc_Word32      jitterInfo)
{
  int err;
  int k;
  int iterCntr;

  double lpcVecs[UB_LPC_ORDER * UB_LPC_VEC_PER_FRAME];

  double percepFilterParams[(1 + UB_LPC_ORDER) * SUBFRAMES];
  float LP[FRAMESAMPLES_HALF];
  float HP[FRAMESAMPLES_HALF];

  double LP_lookahead[FRAMESAMPLES_HALF];
  double HP_lookahead[FRAMESAMPLES_HALF];
  double LPw[FRAMESAMPLES_HALF];

  double HPw[FRAMESAMPLES_HALF];
  WebRtc_Word16 fre[FRAMESAMPLES_HALF];   /* Q7 */
  WebRtc_Word16 fim[FRAMESAMPLES_HALF];   /* Q7 */

  int status = 0;

  double varscale[1];

  double corr[UB_LPC_GAIN_DIM][UB_LPC_ORDER + 1];
  double lpcGains[SUBFRAMES];
  transcode_obj transcodingParam;
  double bytesLeftSpecCoding;
  WebRtc_UWord16 payloadLimitBytes;
  double s2nr;

  /* buffer speech samples (by 10ms packet) until the framelength is  */
  /* reached (30 or 60 ms)                                            */
  /********************************************************************/

  /* fill the buffer with 10ms input data */
  for (k=0; k<FRAMESAMPLES_10ms; k++) {
    ISACencUB_obj->data_buffer_float[k + ISACencUB_obj->buffer_index] =
        in[k];
  }

  /* if buffer-size is not equal to current frame-size then increase the
     index and return. We do the encoding when we have enough audio.     */
  if (ISACencUB_obj->buffer_index + FRAMESAMPLES_10ms < FRAMESAMPLES) {
    ISACencUB_obj->buffer_index += FRAMESAMPLES_10ms;
    return 0;
  }
  /* if buffer reached the right size, reset index and continue
     with encoding the frame */
  ISACencUB_obj->buffer_index = 0;

  /* end of buffer function */
  /**************************/

  /* encoding */
  /************/

  /* reset bitstream */
  ISACencUB_obj->bitstr_obj.W_upper = 0xFFFFFFFF;
  ISACencUB_obj->bitstr_obj.streamval = 0;
  ISACencUB_obj->bitstr_obj.stream_index = 0;

  /* bandwidth estimation and coding */
  /* To be used for Redundant Coding */
  WebRtcIsac_EncodeJitterInfo(jitterInfo, &ISACencUB_obj->bitstr_obj);

  status = WebRtcIsac_EncodeBandwidth(isac12kHz,
                                      &ISACencUB_obj->bitstr_obj);
  if (status < 0) {
    return status;
  }


  s2nr = WebRtcIsac_GetSnr(ISACencUB_obj->bottleneck,
                                 FRAMESAMPLES);

  /* split signal in two bands */
  WebRtcIsac_SplitAndFilterFloat(ISACencUB_obj->data_buffer_float, HP, LP,
                                 HP_lookahead, LP_lookahead, &ISACencUB_obj->prefiltbankstr_obj);

  /* find coefficients for perceptual pre-filters */
  WebRtcIsac_GetLpcCoefUb(LP_lookahead, &ISACencUB_obj->maskfiltstr_obj,
                            lpcVecs, corr, varscale, isac12kHz);

  /* code LPC model and shape - gains not quantized yet */
  WebRtcIsac_EncodeLpcUB(lpcVecs, &ISACencUB_obj->bitstr_obj,
                           percepFilterParams, isac12kHz, &ISACencUB_obj->SaveEnc_obj);

  WebRtcIsac_GetLpcGain(s2nr, percepFilterParams, SUBFRAMES, lpcGains,
                       corr, varscale);
  
  /* Store the state of arithmetic coder before coding LPC gains */
  transcodingParam.W_upper = ISACencUB_obj->bitstr_obj.W_upper;

  transcodingParam.stream_index = ISACencUB_obj->bitstr_obj.stream_index;

  transcodingParam.streamval = ISACencUB_obj->bitstr_obj.streamval;

  transcodingParam.stream[0] = ISACencUB_obj->bitstr_obj.stream[
      ISACencUB_obj->bitstr_obj.stream_index - 2];

  transcodingParam.stream[1] = ISACencUB_obj->bitstr_obj.stream[
      ISACencUB_obj->bitstr_obj.stream_index - 1];

  transcodingParam.stream[2] = ISACencUB_obj->bitstr_obj.stream[
      ISACencUB_obj->bitstr_obj.stream_index];

  /* Store LPC Gains before encoding them */
  for(k = 0; k < SUBFRAMES; k++) {
    transcodingParam.loFiltGain[k] = lpcGains[k];
  }

  // Store the gains for multiple encoding
  memcpy(ISACencUB_obj->SaveEnc_obj.lpcGain, lpcGains, SUBFRAMES *
         sizeof(double));

  WebRtcIsac_EncodeLpcGainUb(lpcGains, &ISACencUB_obj->bitstr_obj,
                             ISACencUB_obj->SaveEnc_obj.lpcGainIndex);

  for(k = 0; k < SUBFRAMES; k++) {
    percepFilterParams[k*(UB_LPC_ORDER + 1)] = lpcGains[k];
  }

  /* perceptual pre-filtering (using normalized lattice filter) */
  /* low-band filtering */
  WebRtcIsac_NormLatticeFilterMa(UB_LPC_ORDER,
                                  ISACencUB_obj->maskfiltstr_obj.PreStateLoF,
                                  ISACencUB_obj->maskfiltstr_obj.PreStateLoG, LP, percepFilterParams,
                                  LPw);

  /* Get the correct value for the payload limit and calculate the number
     of bytes left for coding the spectrum. It is a 30ms frame Subract 3
     because termination process may add 3 bytes */
  payloadLimitBytes = ISACencUB_obj->maxPayloadSizeBytes -
      ISACencUB_obj->numBytesUsed - 3;
  bytesLeftSpecCoding = payloadLimitBytes -
      ISACencUB_obj->bitstr_obj.stream_index;

  memset(HPw, 0, sizeof(double) * FRAMESAMPLES_HALF);

  /* transform */
  WebRtcIsac_Time2Spec(LPw, HPw, fre, fim, &ISACencUB_obj->fftstr_obj);

  //Store real FFT coefficients for multiple encoding
  memcpy(&ISACencUB_obj->SaveEnc_obj.realFFT, fre,
         FRAMESAMPLES_HALF * sizeof(WebRtc_Word16));

  //Store imaginary FFT coefficients for multiple encoding
  memcpy(&ISACencUB_obj->SaveEnc_obj.imagFFT, fim,
         FRAMESAMPLES_HALF * sizeof(WebRtc_Word16));

  // Save the bit-stream object at this point for FEC.
  memcpy(&ISACencUB_obj->SaveEnc_obj.bitStreamObj,
         &ISACencUB_obj->bitstr_obj, sizeof(Bitstr));

  /* quantization and lossless coding */
  err = WebRtcIsac_EncodeSpecUB12(fre, fim, &ISACencUB_obj->bitstr_obj);
  if ((err < 0) && (err != -ISAC_DISALLOWED_BITSTREAM_LENGTH)) {
    /* There has been an error but it was not too large
       payload (we can cure too large payload) */
    return err;
  }
  iterCntr = 0;
  while((ISACencUB_obj->bitstr_obj.stream_index > payloadLimitBytes) ||
        (err == -ISAC_DISALLOWED_BITSTREAM_LENGTH)) {
    double bytesSpecCoderUsed;
    double transcodeScale;

    if (iterCntr >= MAX_PAYLOAD_LIMIT_ITERATION) {
      /* We were not able to limit the payload size */
      return -ISAC_PAYLOAD_LARGER_THAN_LIMIT;
    }

    if (err == -ISAC_DISALLOWED_BITSTREAM_LENGTH) {
      bytesSpecCoderUsed = STREAM_SIZE_MAX;
      // being coservative
      transcodeScale = bytesLeftSpecCoding / bytesSpecCoderUsed * 0.5;
    } else {
      bytesSpecCoderUsed = ISACencUB_obj->bitstr_obj.stream_index -
          transcodingParam.stream_index;
      transcodeScale = bytesLeftSpecCoding / bytesSpecCoderUsed;
    }

    /* To be safe, we reduce the scale depending on the
       number of iterations. */
    transcodeScale *= (1.0 - (0.9 * (double)iterCntr/
                              (double)MAX_PAYLOAD_LIMIT_ITERATION));

    /* Scale the LPC Gains */
    for (k = 0; k < SUBFRAMES; k++) {
      transcodingParam.loFiltGain[k] *= transcodeScale;
    }

    /* Scale DFT coefficients */
    for (k = 0; k < FRAMESAMPLES_HALF; k++) {
      fre[k] = (WebRtc_Word16)(fre[k] * transcodeScale + 0.5);
      fim[k] = (WebRtc_Word16)(fim[k] * transcodeScale + 0.5);
    }

    //Store real FFT coefficients for multiple encoding
    memcpy(&ISACencUB_obj->SaveEnc_obj.realFFT, fre,
           FRAMESAMPLES_HALF * sizeof(WebRtc_Word16));

    //Store imaginary FFT coefficients for multiple encoding
    memcpy(&ISACencUB_obj->SaveEnc_obj.imagFFT, fim,
           FRAMESAMPLES_HALF * sizeof(WebRtc_Word16));


    /* Re-store the state of arithmetic coder before coding LPC gains */
    ISACencUB_obj->bitstr_obj.W_upper = transcodingParam.W_upper;

    ISACencUB_obj->bitstr_obj.stream_index = transcodingParam.stream_index;

    ISACencUB_obj->bitstr_obj.streamval = transcodingParam.streamval;

    ISACencUB_obj->bitstr_obj.stream[transcodingParam.stream_index - 2] =
        transcodingParam.stream[0];

    ISACencUB_obj->bitstr_obj.stream[transcodingParam.stream_index - 1] =
        transcodingParam.stream[1];

    ISACencUB_obj->bitstr_obj.stream[transcodingParam.stream_index] =
        transcodingParam.stream[2];

    // Store the gains for multiple encoding
    memcpy(&ISACencUB_obj->SaveEnc_obj.lpcGain, lpcGains, SUBFRAMES *
           sizeof(double));

    // encode LPC gain and store quantization indices. HAving quantization
    // indices reduces transcoding complexity if 'scale factor' is 1.
    WebRtcIsac_EncodeLpcGainUb(transcodingParam.loFiltGain,
                               &ISACencUB_obj->bitstr_obj,
                               ISACencUB_obj->SaveEnc_obj.lpcGainIndex);

    // Save the bit-stream object at this point for FEC.
    memcpy(&ISACencUB_obj->SaveEnc_obj.bitStreamObj,
           &ISACencUB_obj->bitstr_obj, sizeof(Bitstr));

    /* Update the number of bytes left for encoding the spectrum */
    bytesLeftSpecCoding = payloadLimitBytes -
        ISACencUB_obj->bitstr_obj.stream_index;

    /* Encode the spectrum */
    err = WebRtcIsac_EncodeSpecUB12(fre, fim,
                                      &ISACencUB_obj->bitstr_obj);
    if ((err < 0) && (err != -ISAC_DISALLOWED_BITSTREAM_LENGTH)) {
      /* There has been an error but it was not too large payload
         (we can cure too large payload) */
      return err;
    }
    iterCntr++;
  }

  /* complete arithmetic coding */
  return WebRtcIsac_EncTerminate(&ISACencUB_obj->bitstr_obj);
}






/* This function is used to create a new bitstream with new BWE.
   The same data as previously encoded with the function WebRtcIsac_Encoder().
   The data needed is taken from the struct, where it was stored
   when calling the encoder. */

int WebRtcIsac_EncodeStoredDataLb(
    const ISAC_SaveEncData_t* ISACSavedEnc_obj,
    Bitstr*                   ISACBitStr_obj,
    int                       BWnumber,
    float                     scale)
{
  int ii;
  int status;
  int BWno = BWnumber;

  const WebRtc_UWord16 *WebRtcIsac_kQPitchGainCdf_ptr[1];
  const WebRtc_UWord16 **cdf;

  double tmpLPCcoeffs_lo[(ORDERLO+1)*SUBFRAMES*2];
  double tmpLPCcoeffs_hi[(ORDERHI+1)*SUBFRAMES*2];
  int tmpLPCindex_g[12*2];
  WebRtc_Word16 tmp_fre[FRAMESAMPLES], tmp_fim[FRAMESAMPLES];

  /* Sanity Check - possible values for BWnumber is 0 - 23 */
  if ((BWnumber < 0) || (BWnumber > 23)) {
    return -ISAC_RANGE_ERROR_BW_ESTIMATOR;
  }

  /* reset bitstream */
  ISACBitStr_obj->W_upper = 0xFFFFFFFF;
  ISACBitStr_obj->streamval = 0;
  ISACBitStr_obj->stream_index = 0;

  /* encode frame length */
  status = WebRtcIsac_EncodeFrameLen(ISACSavedEnc_obj->framelength,
                                     ISACBitStr_obj);
  if (status < 0) {
    /* Wrong frame size */
    return status;
  }

  /* Transcoding                                                 */
  if ((scale > 0.0) && (scale < 1.0)) {
    /* Compensate LPC gain */
    for (ii = 0;
         ii < ((ORDERLO + 1)* SUBFRAMES * (1 + ISACSavedEnc_obj->startIdx));
         ii++) {
      tmpLPCcoeffs_lo[ii] = scale *  ISACSavedEnc_obj->LPCcoeffs_lo[ii];
    }
    for (ii = 0;
         ii < ((ORDERHI + 1) * SUBFRAMES *(1 + ISACSavedEnc_obj->startIdx));
         ii++) {
      tmpLPCcoeffs_hi[ii] = scale *  ISACSavedEnc_obj->LPCcoeffs_hi[ii];
    }
    /* Scale DFT */
    for (ii = 0;
         ii < (FRAMESAMPLES_HALF * (1 + ISACSavedEnc_obj->startIdx));
         ii++) {
      tmp_fre[ii] = (WebRtc_Word16)((scale) *
                                    (float)ISACSavedEnc_obj->fre[ii]) ;
      tmp_fim[ii] = (WebRtc_Word16)((scale) *
                                    (float)ISACSavedEnc_obj->fim[ii]) ;
    }
  } else {
    for (ii = 0;
         ii < (KLT_ORDER_GAIN * (1 + ISACSavedEnc_obj->startIdx));
         ii++) {
      tmpLPCindex_g[ii] =  ISACSavedEnc_obj->LPCindex_g[ii];
    }
    for (ii = 0;
         ii < (FRAMESAMPLES_HALF * (1 + ISACSavedEnc_obj->startIdx));
         ii++) {
      tmp_fre[ii] = ISACSavedEnc_obj->fre[ii];
      tmp_fim[ii] = ISACSavedEnc_obj->fim[ii];
    }
  }

  /* encode bandwidth estimate */
  WebRtcIsac_EncodeReceiveBw(&BWno, ISACBitStr_obj);

  /* Loop over number of 30 msec */
  for (ii = 0; ii <= ISACSavedEnc_obj->startIdx; ii++) {
    /* encode pitch gains */
    *WebRtcIsac_kQPitchGainCdf_ptr = WebRtcIsac_kQPitchGainCdf;
    WebRtcIsac_EncHistMulti(ISACBitStr_obj,
                            &ISACSavedEnc_obj->pitchGain_index[ii], WebRtcIsac_kQPitchGainCdf_ptr, 1);

    /* entropy coding of quantization pitch lags */
    /* voicing classificiation */
    if (ISACSavedEnc_obj->meanGain[ii] < 0.2) {
      cdf = WebRtcIsac_kQPitchLagCdfPtrLo;
    } else if (ISACSavedEnc_obj->meanGain[ii] < 0.4) {
      cdf = WebRtcIsac_kQPitchLagCdfPtrMid;
    } else {
      cdf = WebRtcIsac_kQPitchLagCdfPtrHi;
    }
    WebRtcIsac_EncHistMulti(ISACBitStr_obj,
                            &ISACSavedEnc_obj->pitchIndex[PITCH_SUBFRAMES*ii], cdf,
                            PITCH_SUBFRAMES);

    /* LPC */
    /* entropy coding of model number */
    WebRtcIsac_EncHistMulti(ISACBitStr_obj,
                            &ISACSavedEnc_obj->LPCmodel[ii], WebRtcIsac_kQKltModelCdfPtr, 1);

    /* entropy coding of quantization indices - LPC shape only */
    WebRtcIsac_EncHistMulti(ISACBitStr_obj,
                            &ISACSavedEnc_obj->LPCindex_s[KLT_ORDER_SHAPE*ii],
                            WebRtcIsac_kQKltCdfPtrShape[ISACSavedEnc_obj->LPCmodel[ii]],
                            KLT_ORDER_SHAPE);

    /* If transcoding, get new LPC gain indices */
    if (scale < 1.0) {
      WebRtcIsac_TranscodeLPCCoef(&tmpLPCcoeffs_lo[(ORDERLO+1) *
                                                   SUBFRAMES*ii], &tmpLPCcoeffs_hi[(ORDERHI+1)*SUBFRAMES*ii],
                                  ISACSavedEnc_obj->LPCmodel[ii],
                                  &tmpLPCindex_g[KLT_ORDER_GAIN * ii]);
    }

    /* entropy coding of quantization indices - LPC gain */
    WebRtcIsac_EncHistMulti(ISACBitStr_obj,
                            &tmpLPCindex_g[KLT_ORDER_GAIN*ii], WebRtcIsac_kQKltCdfPtrGain[
                                ISACSavedEnc_obj->LPCmodel[ii]], KLT_ORDER_GAIN);

    /* quantization and lossless coding */
    status = WebRtcIsac_EncodeSpecLb(&tmp_fre[ii*FRAMESAMPLES_HALF],
                                     &tmp_fim[ii*FRAMESAMPLES_HALF], ISACBitStr_obj,
                                     ISACSavedEnc_obj->AvgPitchGain[ii]);
    if (status < 0) {
      return status;
    }
  }

  /* complete arithmetic coding */
  return WebRtcIsac_EncTerminate(ISACBitStr_obj);
}




int WebRtcIsac_EncodeStoredDataUb12(
    const ISACUBSaveEncDataStruct* ISACSavedEnc_obj,
    Bitstr*                        bitStream,
    WebRtc_Word32                    jitterInfo,
    float                          scale)
{
  int n;
  int err;
  double lpcGain[SUBFRAMES];
  WebRtc_Word16 realFFT[FRAMESAMPLES_HALF];
  WebRtc_Word16 imagFFT[FRAMESAMPLES_HALF];

  /* reset bitstream */
  bitStream->W_upper = 0xFFFFFFFF;
  bitStream->streamval = 0;
  bitStream->stream_index = 0;

  // Encode jitter index
  WebRtcIsac_EncodeJitterInfo(jitterInfo, bitStream);

  err = WebRtcIsac_EncodeBandwidth(isac12kHz, bitStream);
  if(err < 0)
  {
    return err;
  }

  // Encode LPC-shape
  WebRtcIsac_EncHistMulti(bitStream, ISACSavedEnc_obj->indexLPCShape,
                          WebRtcIsac_kLpcShapeCdfMatUb12, UB_LPC_ORDER * UB_LPC_VEC_PER_FRAME);


  // we only consider scales between zero and one.
  if((scale <= 0.0) || (scale > 1.0))
  {
    scale = 1.0f;
  }

  if(scale == 1.0f)
  {
    //memcpy(lpcGain, ISACSavedEnc_obj->lpcGain, SUBFRAMES * sizeof(double));
    WebRtcIsac_EncHistMulti(bitStream, ISACSavedEnc_obj->lpcGainIndex,
                            WebRtcIsac_kLpcGainCdfMat, UB_LPC_GAIN_DIM);
    // store FFT coefficients
    err = WebRtcIsac_EncodeSpecUB12(ISACSavedEnc_obj->realFFT,
                                      ISACSavedEnc_obj->imagFFT, bitStream);
  }
  else
  {
    /* scale lpc gain and FFT coefficients */
    for(n = 0; n < SUBFRAMES; n++)
    {
      lpcGain[n] = scale * ISACSavedEnc_obj->lpcGain[n];
    }
    // store lpc gain
    WebRtcIsac_StoreLpcGainUb(lpcGain, bitStream);
    for(n = 0; n < FRAMESAMPLES_HALF; n++)
    {
      realFFT[n] = (WebRtc_Word16)(scale * (float)ISACSavedEnc_obj->realFFT[n] + 0.5f);
      imagFFT[n] = (WebRtc_Word16)(scale * (float)ISACSavedEnc_obj->imagFFT[n] + 0.5f);
    }
    // store FFT coefficients
    err = WebRtcIsac_EncodeSpecUB12(realFFT, imagFFT, bitStream);
  }
  if(err < 0)
  {
    // error happened while encoding FFT coefficients.
    return err;
  }

  /* complete arithmetic coding */
  return WebRtcIsac_EncTerminate(bitStream);
}


int
WebRtcIsac_EncodeStoredDataUb16(
    const ISACUBSaveEncDataStruct* ISACSavedEnc_obj,
    Bitstr*                        bitStream,
    WebRtc_Word32                    jitterInfo,
    float                          scale)
{
  int n;
  int err;
  double lpcGain[SUBFRAMES << 1];
  WebRtc_Word16 realFFT[FRAMESAMPLES_HALF];
  WebRtc_Word16 imagFFT[FRAMESAMPLES_HALF];

  /* reset bitstream */
  bitStream->W_upper = 0xFFFFFFFF;
  bitStream->streamval = 0;
  bitStream->stream_index = 0;

  // Encode jitter index
  WebRtcIsac_EncodeJitterInfo(jitterInfo, bitStream);

  err = WebRtcIsac_EncodeBandwidth(isac16kHz, bitStream);
  if(err < 0)
  {
    return err;
  }

  WebRtcIsac_EncHistMulti(bitStream, ISACSavedEnc_obj->indexLPCShape,
                          WebRtcIsac_kLpcShapeCdfMatUb16, UB_LPC_ORDER * UB16_LPC_VEC_PER_FRAME);

  // we only consider scales between zero and one.
  if((scale <= 0.0) || (scale > 1.0))
  {
    scale = 1.0f;
  }

  if(scale == 1.0f)
  {
    // store gains
    WebRtcIsac_EncHistMulti(bitStream, ISACSavedEnc_obj->lpcGainIndex,
                            WebRtcIsac_kLpcGainCdfMat, UB_LPC_GAIN_DIM);
    WebRtcIsac_EncHistMulti(bitStream, &ISACSavedEnc_obj->lpcGainIndex[SUBFRAMES],
                            WebRtcIsac_kLpcGainCdfMat, UB_LPC_GAIN_DIM);
    // store FFT coefficients
    err = WebRtcIsac_EncodeSpecUB16(ISACSavedEnc_obj->realFFT,
                                      ISACSavedEnc_obj->imagFFT, bitStream);

  }
  else
  {
    /* Scale Gain */
    for(n = 0; n < SUBFRAMES; n++)
    {
      lpcGain[n] = scale * ISACSavedEnc_obj->lpcGain[n];
      lpcGain[n + SUBFRAMES] = scale * ISACSavedEnc_obj->lpcGain[n + SUBFRAMES];
    }
    // store lpc gain
    WebRtcIsac_StoreLpcGainUb(lpcGain, bitStream);
    WebRtcIsac_StoreLpcGainUb(&lpcGain[SUBFRAMES], bitStream);
    /* scale FFT coefficients */
    for(n = 0; n < FRAMESAMPLES_HALF; n++)
    {
      realFFT[n] = (WebRtc_Word16)(scale * (float)ISACSavedEnc_obj->realFFT[n] + 0.5f);
      imagFFT[n] = (WebRtc_Word16)(scale * (float)ISACSavedEnc_obj->imagFFT[n] + 0.5f);
    }
    // store FFT coefficients
    err = WebRtcIsac_EncodeSpecUB16(realFFT, imagFFT, bitStream);
  }

  if(err < 0)
  {
    // error happened while encoding FFT coefficients.
    return err;
  }

  /* complete arithmetic coding */
  return WebRtcIsac_EncTerminate(bitStream);
}


WebRtc_Word16
WebRtcIsac_GetRedPayloadUb(
    const ISACUBSaveEncDataStruct* ISACSavedEncObj,
    Bitstr*                        bitStreamObj,
    enum ISACBandwidth             bandwidth)
{
  int n;
  WebRtc_Word16 status;
  WebRtc_Word16 realFFT[FRAMESAMPLES_HALF];
  WebRtc_Word16 imagFFT[FRAMESAMPLES_HALF];

  // store bit-stream object.
  memcpy(bitStreamObj, &ISACSavedEncObj->bitStreamObj, sizeof(Bitstr));

  // Scale FFT coefficients.
  for(n = 0; n < FRAMESAMPLES_HALF; n++)
  {
    realFFT[n] = (WebRtc_Word16)((float)ISACSavedEncObj->realFFT[n] *
                                 RCU_TRANSCODING_SCALE_UB + 0.5);
    imagFFT[n] = (WebRtc_Word16)((float)ISACSavedEncObj->imagFFT[n] *
                                 RCU_TRANSCODING_SCALE_UB + 0.5);
  }

  switch(bandwidth)
  {
    case isac12kHz:
      {
        status = WebRtcIsac_EncodeSpecUB12(realFFT, imagFFT, bitStreamObj);
        break;
      }
    case isac16kHz:
      {
        status = WebRtcIsac_EncodeSpecUB16(realFFT, imagFFT, bitStreamObj);
        break;
      }
    default:
      return -1;
  }

  if(status < 0)
  {
    // error happened
    return status;
  }
  else
  {
    // terminate entropy coding
    return WebRtcIsac_EncTerminate(bitStreamObj);
  }
}
