/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/******************************************************************

 iLBC Speech Coder ANSI-C Source Code

 WebRtcIlbcfix_CbSearch.c

******************************************************************/

#include "defines.h"
#include "gain_quant.h"
#include "filtered_cb_vecs.h"
#include "constants.h"
#include "cb_mem_energy.h"
#include "interpolate_samples.h"
#include "cb_mem_energy_augmentation.h"
#include "cb_search_core.h"
#include "energy_inverse.h"
#include "augmented_cb_corr.h"
#include "cb_update_best_index.h"
#include "create_augmented_vec.h"

/*----------------------------------------------------------------*
 *  Search routine for codebook encoding and gain quantization.
 *----------------------------------------------------------------*/

void WebRtcIlbcfix_CbSearch(
    iLBC_Enc_Inst_t *iLBCenc_inst,
    /* (i) the encoder state structure */
    WebRtc_Word16 *index,  /* (o) Codebook indices */
    WebRtc_Word16 *gain_index, /* (o) Gain quantization indices */
    WebRtc_Word16 *intarget, /* (i) Target vector for encoding */
    WebRtc_Word16 *decResidual,/* (i) Decoded residual for codebook construction */
    WebRtc_Word16 lMem,  /* (i) Length of buffer */
    WebRtc_Word16 lTarget,  /* (i) Length of vector */
    WebRtc_Word16 *weightDenum,/* (i) weighting filter coefficients in Q12 */
    WebRtc_Word16 block  /* (i) the subblock number */
                            ) {
  WebRtc_Word16 i, j, stage, range;
  WebRtc_Word16 *pp, scale, tmp;
  WebRtc_Word16 bits, temp1, temp2;
  WebRtc_Word16 base_size;
  WebRtc_Word32 codedEner, targetEner;
  WebRtc_Word16 gains[CB_NSTAGES+1];
  WebRtc_Word16 *cb_vecPtr;
  WebRtc_Word16 indexOffset, sInd, eInd;
  WebRtc_Word32 CritMax=0;
  WebRtc_Word16 shTotMax=WEBRTC_SPL_WORD16_MIN;
  WebRtc_Word16 bestIndex=0;
  WebRtc_Word16 bestGain=0;
  WebRtc_Word16 indexNew, CritNewSh;
  WebRtc_Word32 CritNew;
  WebRtc_Word32 *cDotPtr;
  WebRtc_Word16 noOfZeros;
  WebRtc_Word16 *gainPtr;
  WebRtc_Word32 t32, tmpW32;
  WebRtc_Word16 *WebRtcIlbcfix_kGainSq5_ptr;
  /* Stack based */
  WebRtc_Word16 CBbuf[CB_MEML+LPC_FILTERORDER+CB_HALFFILTERLEN];
  WebRtc_Word32 cDot[128];
  WebRtc_Word32 Crit[128];
  WebRtc_Word16 targetVec[SUBL+LPC_FILTERORDER];
  WebRtc_Word16 cbvectors[CB_MEML + 1];  /* Adding one extra position for
                                            Coverity warnings. */
  WebRtc_Word16 codedVec[SUBL];
  WebRtc_Word16 interpSamples[20*4];
  WebRtc_Word16 interpSamplesFilt[20*4];
  WebRtc_Word16 energyW16[CB_EXPAND*128];
  WebRtc_Word16 energyShifts[CB_EXPAND*128];
  WebRtc_Word16 *inverseEnergy=energyW16;   /* Reuse memory */
  WebRtc_Word16 *inverseEnergyShifts=energyShifts; /* Reuse memory */
  WebRtc_Word16 *buf = &CBbuf[LPC_FILTERORDER];
  WebRtc_Word16 *target = &targetVec[LPC_FILTERORDER];
  WebRtc_Word16 *aug_vec = (WebRtc_Word16*)cDot;   /* length [SUBL], reuse memory */

  /* Determine size of codebook sections */

  base_size=lMem-lTarget+1;
  if (lTarget==SUBL) {
    base_size=lMem-19;
  }

  /* weighting of the CB memory */
  noOfZeros=lMem-WebRtcIlbcfix_kFilterRange[block];
  WebRtcSpl_MemSetW16(&buf[-LPC_FILTERORDER], 0, noOfZeros+LPC_FILTERORDER);
  WebRtcSpl_FilterARFastQ12(
      decResidual+noOfZeros, buf+noOfZeros,
      weightDenum, LPC_FILTERORDER+1, WebRtcIlbcfix_kFilterRange[block]);

  /* weighting of the target vector */
  WEBRTC_SPL_MEMCPY_W16(&target[-LPC_FILTERORDER], buf+noOfZeros+WebRtcIlbcfix_kFilterRange[block]-LPC_FILTERORDER, LPC_FILTERORDER);
  WebRtcSpl_FilterARFastQ12(
      intarget, target,
      weightDenum, LPC_FILTERORDER+1, lTarget);

  /* Store target, towards the end codedVec is calculated as
     the initial target minus the remaining target */
  WEBRTC_SPL_MEMCPY_W16(codedVec, target, lTarget);

  /* Find the highest absolute value to calculate proper
     vector scale factor (so that it uses 12 bits) */
  temp1 = WebRtcSpl_MaxAbsValueW16(buf, (WebRtc_Word16)lMem);
  temp2 = WebRtcSpl_MaxAbsValueW16(target, (WebRtc_Word16)lTarget);

  if ((temp1>0)&&(temp2>0)) {
    temp1 = WEBRTC_SPL_MAX(temp1, temp2);
    scale = WebRtcSpl_GetSizeInBits(WEBRTC_SPL_MUL_16_16(temp1, temp1));
  } else {
    /* temp1 or temp2 is negative (maximum was -32768) */
    scale = 30;
  }

  /* Scale to so that a mul-add 40 times does not overflow */
  scale = scale - 25;
  scale = WEBRTC_SPL_MAX(0, scale);

  /* Compute energy of the original target */
  targetEner = WebRtcSpl_DotProductWithScale(target, target, lTarget, scale);

  /* Prepare search over one more codebook section. This section
     is created by filtering the original buffer with a filter. */
  WebRtcIlbcfix_FilteredCbVecs(cbvectors, buf, lMem, WebRtcIlbcfix_kFilterRange[block]);

  range = WebRtcIlbcfix_kSearchRange[block][0];

  if(lTarget == SUBL) {
    /* Create the interpolated samples and store them for use in all stages */

    /* First section, non-filtered half of the cb */
    WebRtcIlbcfix_InterpolateSamples(interpSamples, buf, lMem);

    /* Second section, filtered half of the cb */
    WebRtcIlbcfix_InterpolateSamples(interpSamplesFilt, cbvectors, lMem);

    /* Compute the CB vectors' energies for the first cb section (non-filtered) */
    WebRtcIlbcfix_CbMemEnergyAugmentation(interpSamples, buf,
                                          scale, 20, energyW16, energyShifts);

    /* Compute the CB vectors' energies for the second cb section (filtered cb) */
    WebRtcIlbcfix_CbMemEnergyAugmentation(interpSamplesFilt, cbvectors,
                                          scale, (WebRtc_Word16)(base_size+20), energyW16, energyShifts);

    /* Compute the CB vectors' energies and store them in the vector
     * energyW16. Also the corresponding shift values are stored. The
     * energy values are used in all three stages. */
    WebRtcIlbcfix_CbMemEnergy(range, buf, cbvectors, lMem,
                              lTarget, energyW16+20, energyShifts+20, scale, base_size);

  } else {
    /* Compute the CB vectors' energies and store them in the vector
     * energyW16. Also the corresponding shift values are stored. The
     * energy values are used in all three stages. */
    WebRtcIlbcfix_CbMemEnergy(range, buf, cbvectors, lMem,
                              lTarget, energyW16, energyShifts, scale, base_size);

    /* Set the energy positions 58-63 and 122-127 to zero
       (otherwise they are uninitialized) */
    WebRtcSpl_MemSetW16(energyW16+range, 0, (base_size-range));
    WebRtcSpl_MemSetW16(energyW16+range+base_size, 0, (base_size-range));
  }

  /* Calculate Inverse Energy (energyW16 is already normalized
     and will contain the inverse energy in Q29 after this call */
  WebRtcIlbcfix_EnergyInverse(energyW16, base_size*CB_EXPAND);

  /* The gain value computed in the previous stage is used
   * as an upper limit to what the next stage gain value
   * is allowed to be. In stage 0, 16384 (1.0 in Q14) is used as
   * the upper limit. */
  gains[0] = 16384;

  for (stage=0; stage<CB_NSTAGES; stage++) {

    /* Set up memories */
    range = WebRtcIlbcfix_kSearchRange[block][stage];

    /* initialize search measures */
    CritMax=0;
    shTotMax=-100;
    bestIndex=0;
    bestGain=0;

    /* loop over lags 40+ in the first codebook section, full search */
    cb_vecPtr = buf+lMem-lTarget;

    /* Calculate all the cross correlations (augmented part of CB) */
    if (lTarget==SUBL) {
      WebRtcIlbcfix_AugmentedCbCorr(target, buf+lMem,
                                    interpSamples, cDot,
                                    20, 39, scale);
      cDotPtr=&cDot[20];
    } else {
      cDotPtr=cDot;
    }
    /* Calculate all the cross correlations (main part of CB) */
    WebRtcSpl_CrossCorrelation(cDotPtr, target, cb_vecPtr, lTarget, range, scale, -1);

    /* Adjust the search range for the augmented vectors */
    if (lTarget==SUBL) {
      range=WebRtcIlbcfix_kSearchRange[block][stage]+20;
    } else {
      range=WebRtcIlbcfix_kSearchRange[block][stage];
    }

    indexOffset=0;

    /* Search for best index in this part of the vector */
    WebRtcIlbcfix_CbSearchCore(
        cDot, range, stage, inverseEnergy,
        inverseEnergyShifts, Crit,
        &indexNew, &CritNew, &CritNewSh);

    /* Update the global best index and the corresponding gain */
    WebRtcIlbcfix_CbUpdateBestIndex(
        CritNew, CritNewSh, (WebRtc_Word16)(indexNew+indexOffset), cDot[indexNew+indexOffset],
        inverseEnergy[indexNew+indexOffset], inverseEnergyShifts[indexNew+indexOffset],
        &CritMax, &shTotMax, &bestIndex, &bestGain);

    sInd=bestIndex-(WebRtc_Word16)(CB_RESRANGE>>1);
    eInd=sInd+CB_RESRANGE;
    if (sInd<0) {
      eInd-=sInd;
      sInd=0;
    }
    if (eInd>=range) {
      eInd=range-1;
      sInd=eInd-CB_RESRANGE;
    }

    range = WebRtcIlbcfix_kSearchRange[block][stage];

    if (lTarget==SUBL) {
      i=sInd;
      if (sInd<20) {
        WebRtcIlbcfix_AugmentedCbCorr(target, cbvectors+lMem,
                                      interpSamplesFilt, cDot,
                                      (WebRtc_Word16)(sInd+20), (WebRtc_Word16)(WEBRTC_SPL_MIN(39, (eInd+20))), scale);
        i=20;
      }

      cDotPtr=&cDot[WEBRTC_SPL_MAX(0,(20-sInd))];
      cb_vecPtr = cbvectors+lMem-20-i;

      /* Calculate the cross correlations (main part of the filtered CB) */
      WebRtcSpl_CrossCorrelation(cDotPtr, target, cb_vecPtr, lTarget, (WebRtc_Word16)(eInd-i+1), scale, -1);

    } else {
      cDotPtr = cDot;
      cb_vecPtr = cbvectors+lMem-lTarget-sInd;

      /* Calculate the cross correlations (main part of the filtered CB) */
      WebRtcSpl_CrossCorrelation(cDotPtr, target, cb_vecPtr, lTarget, (WebRtc_Word16)(eInd-sInd+1), scale, -1);

    }

    /* Adjust the search range for the augmented vectors */
    indexOffset=base_size+sInd;

    /* Search for best index in this part of the vector */
    WebRtcIlbcfix_CbSearchCore(
        cDot, (WebRtc_Word16)(eInd-sInd+1), stage, inverseEnergy+indexOffset,
        inverseEnergyShifts+indexOffset, Crit,
        &indexNew, &CritNew, &CritNewSh);

    /* Update the global best index and the corresponding gain */
    WebRtcIlbcfix_CbUpdateBestIndex(
        CritNew, CritNewSh, (WebRtc_Word16)(indexNew+indexOffset), cDot[indexNew],
        inverseEnergy[indexNew+indexOffset], inverseEnergyShifts[indexNew+indexOffset],
        &CritMax, &shTotMax, &bestIndex, &bestGain);

    index[stage] = bestIndex;


    bestGain = WebRtcIlbcfix_GainQuant(bestGain,
                                       (WebRtc_Word16)WEBRTC_SPL_ABS_W16(gains[stage]), stage, &gain_index[stage]);

    /* Extract the best (according to measure) codebook vector
       Also adjust the index, so that the augmented vectors are last.
       Above these vectors were first...
    */

    if(lTarget==(STATE_LEN-iLBCenc_inst->state_short_len)) {

      if(index[stage]<base_size) {
        pp=buf+lMem-lTarget-index[stage];
      } else {
        pp=cbvectors+lMem-lTarget-
            index[stage]+base_size;
      }

    } else {

      if (index[stage]<base_size) {
        if (index[stage]>=20) {
          /* Adjust index and extract vector */
          index[stage]-=20;
          pp=buf+lMem-lTarget-index[stage];
        } else {
          /* Adjust index and extract vector */
          index[stage]+=(base_size-20);

          WebRtcIlbcfix_CreateAugmentedVec((WebRtc_Word16)(index[stage]-base_size+40),
                                           buf+lMem, aug_vec);
          pp = aug_vec;

        }
      } else {

        if ((index[stage] - base_size) >= 20) {
          /* Adjust index and extract vector */
          index[stage]-=20;
          pp=cbvectors+lMem-lTarget-
              index[stage]+base_size;
        } else {
          /* Adjust index and extract vector */
          index[stage]+=(base_size-20);
          WebRtcIlbcfix_CreateAugmentedVec((WebRtc_Word16)(index[stage]-2*base_size+40),
                                           cbvectors+lMem, aug_vec);
          pp = aug_vec;
        }
      }
    }

    /* Subtract the best codebook vector, according
       to measure, from the target vector */

    WebRtcSpl_AddAffineVectorToVector(target, pp, (WebRtc_Word16)(-bestGain), (WebRtc_Word32)8192, (WebRtc_Word16)14, (int)lTarget);

    /* record quantized gain */
    gains[stage+1] = bestGain;

  } /* end of Main Loop. for (stage=0;... */

  /* Calculte the coded vector (original target - what's left) */
  for (i=0;i<lTarget;i++) {
    codedVec[i]-=target[i];
  }

  /* Gain adjustment for energy matching */
  codedEner = WebRtcSpl_DotProductWithScale(codedVec, codedVec, lTarget, scale);

  j=gain_index[0];

  temp1 = (WebRtc_Word16)WebRtcSpl_NormW32(codedEner);
  temp2 = (WebRtc_Word16)WebRtcSpl_NormW32(targetEner);

  if(temp1 < temp2) {
    bits = 16 - temp1;
  } else {
    bits = 16 - temp2;
  }

  tmp = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16_RSFT(gains[1],gains[1], 14);

  targetEner = WEBRTC_SPL_MUL_16_16(
      WEBRTC_SPL_SHIFT_W32(targetEner, -bits), tmp);

  tmpW32 = ((WebRtc_Word32)(gains[1]-1))<<1;

  /* Pointer to the table that contains
     gain_sq5TblFIX * gain_sq5TblFIX in Q14 */
  gainPtr=(WebRtc_Word16*)WebRtcIlbcfix_kGainSq5Sq+gain_index[0];
  temp1 = (WebRtc_Word16)WEBRTC_SPL_SHIFT_W32(codedEner, -bits);

  WebRtcIlbcfix_kGainSq5_ptr = (WebRtc_Word16*)&WebRtcIlbcfix_kGainSq5[j];

  /* targetEner and codedEner are in Q(-2*scale) */
  for (i=gain_index[0];i<32;i++) {

    /* Change the index if
       (codedEnergy*gainTbl[i]*gainTbl[i])<(targetEn*gain[0]*gain[0]) AND
       gainTbl[i] < 2*gain[0]
    */

    t32 = WEBRTC_SPL_MUL_16_16(temp1, (*gainPtr));
    t32 = t32 - targetEner;
    if (t32 < 0) {
      if ((*WebRtcIlbcfix_kGainSq5_ptr) < tmpW32) {
        j=i;
        WebRtcIlbcfix_kGainSq5_ptr = (WebRtc_Word16*)&WebRtcIlbcfix_kGainSq5[i];
      }
    }
    gainPtr++;
  }
  gain_index[0]=j;

  return;
}
