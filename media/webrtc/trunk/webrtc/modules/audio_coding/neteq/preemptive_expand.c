/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file contains the Pre-emptive Expand algorithm that is used to increase
 * the delay by repeating a part of the audio stream.
 */

#include "dsp.h"

#include "signal_processing_library.h"

#include "dsp_helpfunctions.h"
#include "neteq_error_codes.h"

#define PREEMPTIVE_CORR_LEN 50
#define PREEMPTIVE_MIN_LAG 10
#define PREEMPTIVE_MAX_LAG 60
#define PREEMPTIVE_DOWNSAMPLED_LEN (PREEMPTIVE_CORR_LEN + PREEMPTIVE_MAX_LAG)

/* Scratch usage:

 Type             Name                 size            startpos         endpos
 int16_t    pw16_downSampSpeech  110             0                109
 int32_t    pw32_corr            2*50            110              209
 int16_t    pw16_corr            50              0                49

 Total: 110+2*50
 */

#define     SCRATCH_PW16_DS_SPEECH           0
#define     SCRATCH_PW32_CORR                PREEMPTIVE_DOWNSAMPLED_LEN
#define     SCRATCH_PW16_CORR                0

/****************************************************************************
 * WebRtcNetEQ_PreEmptiveExpand(...)
 *
 * This function tries to extend the audio data by repeating one or several
 * pitch periods. The operation is only carried out if the correlation is
 * strong or if the signal energy is very low. The algorithm is the
 * reciprocal of the Accelerate algorithm.
 *
 * Input:
 *      - inst          : NetEQ DSP instance
 *      - scratchPtr    : Pointer to scratch vector.
 *      - decoded       : Pointer to newly decoded speech.
 *      - len           : Length of decoded speech.
 *      - oldDataLen    : Length of the part of decoded that has already been played out.
 *      - BGNonly       : If non-zero, Pre-emptive Expand will only copy 
 *                        the first DEFAULT_TIME_ADJUST seconds of the
 *                        input and append to the end. No signal matching is
 *                        done.
 *
 * Output:
 *      - inst          : Updated instance
 *      - outData       : Pointer to a memory space where the output data
 *                        should be stored. The vector must be at least
 *                        min(len + 120*fs/8000, NETEQ_MAX_OUTPUT_SIZE)
 *                        elements long.
 *      - pw16_len      : Number of samples written to outData.
 *
 * Return value         :  0 - Ok
 *                        <0 - Error
 */

int WebRtcNetEQ_PreEmptiveExpand(DSPInst_t *inst,
#ifdef SCRATCH
                                 int16_t *pw16_scratchPtr,
#endif
                                 const int16_t *pw16_decoded, int len, int oldDataLen,
                                 int16_t *pw16_outData, int16_t *pw16_len,
                                 int16_t BGNonly)
{

#ifdef SCRATCH
    /* Use scratch memory for internal temporary vectors */
    int16_t *pw16_downSampSpeech = pw16_scratchPtr + SCRATCH_PW16_DS_SPEECH;
    int32_t *pw32_corr = (int32_t*) (pw16_scratchPtr + SCRATCH_PW32_CORR);
    int16_t *pw16_corr = pw16_scratchPtr + SCRATCH_PW16_CORR;
#else
    /* Allocate memory for temporary vectors */
    int16_t pw16_downSampSpeech[PREEMPTIVE_DOWNSAMPLED_LEN];
    int32_t pw32_corr[PREEMPTIVE_CORR_LEN];
    int16_t pw16_corr[PREEMPTIVE_CORR_LEN];
#endif
    int16_t w16_decodedMax = 0;
    int16_t w16_tmp = 0;
    int16_t w16_tmp2;
    int32_t w32_tmp;
    int32_t w32_tmp2;

    const int16_t w16_startLag = PREEMPTIVE_MIN_LAG;
    const int16_t w16_endLag = PREEMPTIVE_MAX_LAG;
    const int16_t w16_corrLen = PREEMPTIVE_CORR_LEN;
    const int16_t *pw16_vec1, *pw16_vec2;
    int16_t *pw16_vectmp;
    int16_t w16_inc, w16_startfact;
    int16_t w16_bestIndex, w16_bestVal;
    int16_t w16_VAD = 1;
    int16_t fsMult;
    int16_t fsMult120;
    int32_t w32_en1, w32_en2, w32_cc;
    int16_t w16_en1, w16_en2;
    int16_t w16_en1Scale, w16_en2Scale;
    int16_t w16_sqrtEn1En2;
    int16_t w16_bestCorr = 0;
    int ok;

#ifdef NETEQ_STEREO
    MasterSlaveInfo *msInfo = inst->msInfo;
#endif

    fsMult = WebRtcNetEQ_CalcFsMult(inst->fs); /* Calculate fs/8000 */

    /* Pre-calculate common multiplication with fsMult */
    fsMult120 = (int16_t) WEBRTC_SPL_MUL_16_16(fsMult, 120); /* 15 ms */

    inst->ExpandInst.w16_consecExp = 0; /* Last was not expand any more */

    /*
     * Sanity check for len variable; must be (almost) 30 ms (120*fsMult + max(bestIndex)).
     * Also, the new part must be at least .625 ms (w16_overlap).
     */
    if (len < (int16_t) WEBRTC_SPL_MUL_16_16((120 + 119), fsMult) || oldDataLen >= len
        - inst->ExpandInst.w16_overlap)
    {
        /* Length of decoded data too short */
        inst->w16_mode = MODE_UNSUCCESS_PREEMPTIVE;
        *pw16_len = len;

        
        /* simply move all data from decoded to outData */

        WEBRTC_SPL_MEMMOVE_W16(pw16_outData, pw16_decoded, (int16_t) len);

        return NETEQ_OTHER_ERROR;
    }

    /***********************************/
    /* Special operations for BGN only */
    /***********************************/

    /* Check if "background noise only" flag is set */
    if (BGNonly)
    {
        /* special operation for BGN only; simply insert a chunk of data */
        w16_bestIndex = DEFAULT_TIME_ADJUST * (fsMult << 3); /* X*fs/1000 */

        /* Sanity check for bestIndex */
        if (w16_bestIndex > len)
        { /* not good, do nothing instead */
            inst->w16_mode = MODE_UNSUCCESS_PREEMPTIVE;
            *pw16_len = len;


            /* simply move all data from decoded to outData */

            WEBRTC_SPL_MEMMOVE_W16(pw16_outData, pw16_decoded, (int16_t) len);

            return NETEQ_OTHER_ERROR;
        }

        /* set length parameter */
        *pw16_len = len + w16_bestIndex;


        /* copy to output */

        WEBRTC_SPL_MEMMOVE_W16(pw16_outData, pw16_decoded, len);
        WEBRTC_SPL_MEMCPY_W16(&pw16_outData[len], pw16_decoded, w16_bestIndex);

        /* set mode */
        inst->w16_mode = MODE_LOWEN_PREEMPTIVE;

        /* update statistics */
        inst->statInst.preemptiveLength += w16_bestIndex;
        /* Short-term activity statistics. */
        inst->activity_stats.preemptive_expand_bgn_samples += w16_bestIndex;

        return 0;
    } /* end of special code for BGN mode */

#ifdef NETEQ_STEREO

    /* Sanity for msInfo */
    if (msInfo == NULL)
    {
        /* this should not happen here */
        return MASTER_SLAVE_ERROR;
    }

    if ((msInfo->msMode == NETEQ_MASTER) || (msInfo->msMode == NETEQ_MONO))
    {
        /* Find correlation lag only for non-slave instances */

#endif

        /****************************************************************/
        /* Find the strongest correlation lag by downsampling to 4 kHz, */
        /* calculating correlation for downsampled signal and finding   */
        /* the strongest correlation peak.                              */
        /****************************************************************/

        /* find maximum absolute value */
        w16_decodedMax = WebRtcSpl_MaxAbsValueW16(pw16_decoded, (int16_t) len);

        /* downsample the decoded speech to 4 kHz */
        ok = WebRtcNetEQ_DownSampleTo4kHz(pw16_decoded, len, inst->fs, pw16_downSampSpeech,
            PREEMPTIVE_DOWNSAMPLED_LEN, 1 /* compensate delay*/);
        if (ok != 0)
        {
            /* error */
            inst->w16_mode = MODE_UNSUCCESS_PREEMPTIVE;
            *pw16_len = len;


            /* simply move all data from decoded to outData */

            WEBRTC_SPL_MEMMOVE_W16(pw16_outData, pw16_decoded, (int16_t) len);

            return NETEQ_OTHER_ERROR;
        }

        /*
         * Set scaling factor for cross correlation to protect against
         * overflow (log2(50) => 6)
         */
        w16_tmp = 6 - WebRtcSpl_NormW32(WEBRTC_SPL_MUL_16_16(w16_decodedMax, w16_decodedMax));
        w16_tmp = WEBRTC_SPL_MAX(0, w16_tmp);

        /* Perform correlation from lag 10 to lag 60 in 4 kHz domain */WebRtcNetEQ_CrossCorr(
            pw32_corr, &pw16_downSampSpeech[w16_endLag],
            &pw16_downSampSpeech[w16_endLag - w16_startLag], w16_corrLen,
            (int16_t) (w16_endLag - w16_startLag), w16_tmp, -1);

        /* Normalize correlation to 14 bits and put in a int16_t vector */
        w32_tmp = WebRtcSpl_MaxAbsValueW32(pw32_corr, w16_corrLen);
        w16_tmp = 17 - WebRtcSpl_NormW32(w32_tmp);
        w16_tmp = WEBRTC_SPL_MAX(0, w16_tmp);

        WebRtcSpl_VectorBitShiftW32ToW16(pw16_corr, w16_corrLen, pw32_corr, w16_tmp);

        /* Find limits for peak finding, in order to avoid overful NetEQ algorithm buffer. */
        /* Calculate difference between MAX_OUTPUT_SIZE and len in 4 kHz domain. */
        w16_tmp = WebRtcSpl_DivW32W16ResW16((int32_t) (NETEQ_MAX_OUTPUT_SIZE - len),
            (int16_t) (fsMult << 1)) - w16_startLag;
        w16_tmp = WEBRTC_SPL_MIN(w16_corrLen, w16_tmp); /* no more than corrLen = 50 */

#ifdef NETEQ_STEREO
    } /* end if (msInfo->msMode != NETEQ_SLAVE) */

    if ((msInfo->msMode == NETEQ_MASTER) || (msInfo->msMode == NETEQ_MONO))
    {
        /* Find the strongest correlation peak by using the parabolic fit method */
        WebRtcNetEQ_PeakDetection(pw16_corr, w16_tmp, 1, fsMult, &w16_bestIndex, &w16_bestVal);
        /* 0 <= bestIndex <= (2*w16_tmp - 1)*fsMult <= (2*corrLen - 1)*fsMult = 99*fsMult */

        /* Compensate bestIndex for displaced starting position */
        w16_bestIndex = w16_bestIndex + w16_startLag * WEBRTC_SPL_LSHIFT_W16(fsMult, 1);
        /* 20*fsMult <= bestIndex <= 119*fsMult */

        msInfo->bestIndex = w16_bestIndex;
    }
    else if (msInfo->msMode == NETEQ_SLAVE)
    {
        if (msInfo->extraInfo == PE_EXP_FAIL)
        {
            /* Master has signaled an unsuccessful preemptive expand */
            w16_bestIndex = 0;
        }
        else
        {
            /* Get best index from master */
            w16_bestIndex = msInfo->bestIndex;
        }
    }
    else
    {
        /* Invalid mode */
        return (MASTER_SLAVE_ERROR);
    }

#else /* NETEQ_STEREO */

    /* Find the strongest correlation peak by using the parabolic fit method */
    WebRtcNetEQ_PeakDetection(pw16_corr, w16_tmp, 1, fsMult, &w16_bestIndex, &w16_bestVal);
    /* 0 <= bestIndex <= (2*w16_tmp - 1)*fsMult <= (2*corrLen - 1)*fsMult = 99*fsMult */

    /* Compensate bestIndex for displaced starting position */
    w16_bestIndex = w16_bestIndex + w16_startLag * WEBRTC_SPL_LSHIFT_W16(fsMult, 1);
    /* 20*fsMult <= bestIndex <= 119*fsMult */

#endif /* NETEQ_STEREO */

#ifdef NETEQ_STEREO

    if ((msInfo->msMode == NETEQ_MASTER) || (msInfo->msMode == NETEQ_MONO))
    {
        /* Calculate correlation only for non-slave instances */

#endif /* NETEQ_STEREO */

        /*****************************************************/
        /* Calculate correlation bestCorr for the found lag. */
        /* Also do a simple VAD decision.                    */
        /*****************************************************/

        /*
         * Calculate scaling to ensure that bestIndex samples can be square-summed
         * without overflowing
         */
        w16_tmp = (31
            - WebRtcSpl_NormW32(WEBRTC_SPL_MUL_16_16(w16_decodedMax, w16_decodedMax)));
        w16_tmp += (31 - WebRtcSpl_NormW32(w16_bestIndex));
        w16_tmp -= 31;
        w16_tmp = WEBRTC_SPL_MAX(0, w16_tmp);

        /* vec1 starts at 15 ms minus one pitch period */
        pw16_vec1 = &pw16_decoded[fsMult120 - w16_bestIndex];
        /* vec2 start at 15 ms */
        pw16_vec2 = &pw16_decoded[fsMult120];

        /* Calculate energies for vec1 and vec2 */
        w32_en1 = WebRtcNetEQ_DotW16W16((int16_t*) pw16_vec1,
            (int16_t*) pw16_vec1, w16_bestIndex, w16_tmp);
        w32_en2 = WebRtcNetEQ_DotW16W16((int16_t*) pw16_vec2,
            (int16_t*) pw16_vec2, w16_bestIndex, w16_tmp);

        /* Calculate cross-correlation at the found lag */
        w32_cc = WebRtcNetEQ_DotW16W16((int16_t*) pw16_vec1, (int16_t*) pw16_vec2,
            w16_bestIndex, w16_tmp);

        /* Check VAD constraint 
         ((en1+en2)/(2*bestIndex)) <= 8*inst->BGNInst.energy */
        w32_tmp = WEBRTC_SPL_RSHIFT_W32(w32_en1 + w32_en2, 4); /* (en1+en2)/(2*8) */
        if (inst->BGNInst.w16_initialized == 1)
        {
            w32_tmp2 = inst->BGNInst.w32_energy;
        }
        else
        {
            /* if BGN parameters have not been estimated, use a fixed threshold */
            w32_tmp2 = 75000;
        }
        w16_tmp2 = 16 - WebRtcSpl_NormW32(w32_tmp2);
        w16_tmp2 = WEBRTC_SPL_MAX(0, w16_tmp2);
        w32_tmp = WEBRTC_SPL_RSHIFT_W32(w32_tmp, w16_tmp2);
        w16_tmp2 = (int16_t) WEBRTC_SPL_RSHIFT_W32(w32_tmp2, w16_tmp2);
        w32_tmp2 = WEBRTC_SPL_MUL_16_16(w16_bestIndex, w16_tmp2);

        /* Scale w32_tmp properly before comparing with w32_tmp2 */
        /* (w16_tmp is scaling before energy calculation, thus 2*w16_tmp) */
        if (WebRtcSpl_NormW32(w32_tmp) < WEBRTC_SPL_LSHIFT_W32(w16_tmp,1))
        {
            /* Cannot scale only w32_tmp, must scale w32_temp2 too */
            int16_t tempshift = WebRtcSpl_NormW32(w32_tmp);
            w32_tmp = WEBRTC_SPL_LSHIFT_W32(w32_tmp, tempshift);
            w32_tmp2 = WEBRTC_SPL_RSHIFT_W32(w32_tmp2,
                WEBRTC_SPL_LSHIFT_W32(w16_tmp,1) - tempshift);
        }
        else
        {
            w32_tmp = WEBRTC_SPL_LSHIFT_W32(w32_tmp,
                WEBRTC_SPL_LSHIFT_W32(w16_tmp,1));
        }

        if (w32_tmp <= w32_tmp2) /*((en1+en2)/(2*bestIndex)) <= 8*inst->BGNInst.energy */
        {
            /* The signal seems to be passive speech */
            w16_VAD = 0;
            w16_bestCorr = 0; /* Correlation does not matter */

            /* For low energy expansion, the new data can be less than 15 ms,
             but we must ensure that bestIndex is not larger than the new data. */
            w16_bestIndex = WEBRTC_SPL_MIN( w16_bestIndex, len - oldDataLen );
        }
        else
        {
            /* The signal is active speech */
            w16_VAD = 1;

            /* Calculate correlation (cc/sqrt(en1*en2)) */

            /* Start with calculating scale values */
            w16_en1Scale = 16 - WebRtcSpl_NormW32(w32_en1);
            w16_en1Scale = WEBRTC_SPL_MAX(0, w16_en1Scale);
            w16_en2Scale = 16 - WebRtcSpl_NormW32(w32_en2);
            w16_en2Scale = WEBRTC_SPL_MAX(0, w16_en2Scale);

            /* Make sure total scaling is even (to simplify scale factor after sqrt) */
            if ((w16_en1Scale + w16_en2Scale) & 1)
            {
                w16_en1Scale += 1;
            }

            /* Convert energies to int16_t */
            w16_en1 = (int16_t) WEBRTC_SPL_RSHIFT_W32(w32_en1, w16_en1Scale);
            w16_en2 = (int16_t) WEBRTC_SPL_RSHIFT_W32(w32_en2, w16_en2Scale);

            /* Calculate energy product */
            w32_tmp = WEBRTC_SPL_MUL_16_16(w16_en1, w16_en2);

            /* Calculate square-root of energy product */
            w16_sqrtEn1En2 = (int16_t) WebRtcSpl_SqrtFloor(w32_tmp);

            /* Calculate cc/sqrt(en1*en2) in Q14 */
            w16_tmp = 14 - ((w16_en1Scale + w16_en2Scale) >> 1);
            w32_cc = WEBRTC_SPL_SHIFT_W32(w32_cc, w16_tmp);
            w32_cc = WEBRTC_SPL_MAX(0, w32_cc); /* Don't divide with negative number */
            w16_bestCorr = (int16_t) WebRtcSpl_DivW32W16(w32_cc, w16_sqrtEn1En2);
            w16_bestCorr = WEBRTC_SPL_MIN(16384, w16_bestCorr); /* set maximum to 1.0 */
        }

#ifdef NETEQ_STEREO

    } /* end if (msInfo->msMode != NETEQ_SLAVE) */

#endif /* NETEQ_STEREO */

    /*******************************************************/
    /* Check preemptive expand criteria and insert samples */
    /*******************************************************/

    /* Check for strong correlation (>0.9) and at least 15 ms new data, 
     or passive speech */
#ifdef NETEQ_STEREO
    if (((((w16_bestCorr > 14746) && (oldDataLen <= fsMult120)) || (w16_VAD == 0))
        && (msInfo->msMode != NETEQ_SLAVE)) || ((msInfo->msMode == NETEQ_SLAVE)
        && (msInfo->extraInfo != PE_EXP_FAIL)))
#else
    if (((w16_bestCorr > 14746) && (oldDataLen <= fsMult120))
        || (w16_VAD == 0))
#endif
    {
        /* Do expand operation by overlap add */

        /* Set length of the first part, not to be modified */
        int16_t w16_startIndex = WEBRTC_SPL_MAX(oldDataLen, fsMult120);

        /*
         * Calculate cross-fading slope so that the fading factor goes from
         * 1 (16384 in Q14) to 0 in one pitch period (bestIndex).
         */
        w16_inc = (int16_t) WebRtcSpl_DivW32W16((int32_t) 16384,
            (int16_t) (w16_bestIndex + 1)); /* in Q14 */

        /* Initiate fading factor */
        w16_startfact = 16384 - w16_inc;

        /* vec1 starts at 15 ms minus one pitch period */
        pw16_vec1 = &pw16_decoded[w16_startIndex - w16_bestIndex];
        /* vec2 start at 15 ms */
        pw16_vec2 = &pw16_decoded[w16_startIndex];


        /* Copy unmodified part [0 to 15 ms] */

        WEBRTC_SPL_MEMMOVE_W16(pw16_outData, pw16_decoded, w16_startIndex);

        /* Generate interpolated part of length bestIndex (1 pitch period) */
        pw16_vectmp = pw16_outData + w16_startIndex;
        /* Reuse mixing function from Expand */
        WebRtcNetEQ_MixVoiceUnvoice(pw16_vectmp, (int16_t*) pw16_vec2,
            (int16_t*) pw16_vec1, &w16_startfact, w16_inc, w16_bestIndex);

        /* Move the last part (also unmodified) */
        /* Take from decoded at 15 ms */
        pw16_vec2 = &pw16_decoded[w16_startIndex];
        WEBRTC_SPL_MEMMOVE_W16(&pw16_outData[w16_startIndex + w16_bestIndex], pw16_vec2,
            (int16_t) (len - w16_startIndex));

        /* Set the mode flag */
        if (w16_VAD)
        {
            inst->w16_mode = MODE_SUCCESS_PREEMPTIVE;
        }
        else
        {
            inst->w16_mode = MODE_LOWEN_PREEMPTIVE;
        }

        /* Calculate resulting length = original length + pitch period */
        *pw16_len = len + w16_bestIndex;

        /* Update in-call statistics */
        inst->statInst.preemptiveLength += w16_bestIndex;
        /* Short-term activity statistics. */
        inst->activity_stats.preemptive_expand_normal_samples += w16_bestIndex;
        return 0;
    }
    else
    {
        /* Preemptive Expand not allowed */

#ifdef NETEQ_STEREO
        /* Signal to slave(s) that this was unsuccessful */
        if (msInfo->msMode == NETEQ_MASTER)
        {
            msInfo->extraInfo = PE_EXP_FAIL;
        }
#endif

        /* Set mode flag to unsuccessful preemptive expand */
        inst->w16_mode = MODE_UNSUCCESS_PREEMPTIVE;

        /* Length is unmodified */
        *pw16_len = len;


        /* Simply move all data from decoded to outData */

        WEBRTC_SPL_MEMMOVE_W16(pw16_outData, pw16_decoded, (int16_t) len);

        return 0;
    }
}

#undef     SCRATCH_PW16_DS_SPEECH
#undef     SCRATCH_PW32_CORR
#undef     SCRATCH_PW16_CORR
