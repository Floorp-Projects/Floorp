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
 * This file contains the Accelerate algorithm that is used to reduce
 * the delay by removing a part of the audio stream.
 */

#include "dsp.h"

#include "signal_processing_library.h"

#include "dsp_helpfunctions.h"
#include "neteq_error_codes.h"

#define ACCELERATE_CORR_LEN 50
#define ACCELERATE_MIN_LAG 10
#define ACCELERATE_MAX_LAG 60
#define ACCELERATE_DOWNSAMPLED_LEN (ACCELERATE_CORR_LEN + ACCELERATE_MAX_LAG)

/* Scratch usage:

 Type	        Name                size    startpos    endpos
 WebRtc_Word16  pw16_downSampSpeech 110     0           109
 WebRtc_Word32  pw32_corr           2*50    110         209
 WebRtc_Word16  pw16_corr           50      0           49

 Total: 110+2*50
 */

#define	 SCRATCH_PW16_DS_SPEECH			0
#define	 SCRATCH_PW32_CORR				ACCELERATE_DOWNSAMPLED_LEN
#define	 SCRATCH_PW16_CORR				0

/****************************************************************************
 * WebRtcNetEQ_Accelerate(...)
 *
 * This function tries to shorten the audio data by removing one or several
 * pitch periods. The operation is only carried out if the correlation is
 * strong or if the signal energy is very low.
 *
 * Input:
 *		- inst			: NetEQ DSP instance
 *      - scratchPtr    : Pointer to scratch vector.
 *		- decoded	    : Pointer to newly decoded speech.
 *		- len           : Length of decoded speech.
 *      - BGNonly       : If non-zero, Accelerate will only remove the last 
 *                        DEFAULT_TIME_ADJUST seconds of the input.
 *                        No signal matching is done.
 *
 * Output:
 *		- inst			: Updated instance
 *		- outData		: Pointer to a memory space where the output data
 *						  should be stored
 *		- pw16_len		: Number of samples written to outData.
 *
 * Return value			:  0 - Ok
 *						  <0 - Error
 */

int WebRtcNetEQ_Accelerate(DSPInst_t *inst,
#ifdef SCRATCH
                           WebRtc_Word16 *pw16_scratchPtr,
#endif
                           const WebRtc_Word16 *pw16_decoded, int len,
                           WebRtc_Word16 *pw16_outData, WebRtc_Word16 *pw16_len,
                           WebRtc_Word16 BGNonly)
{

#ifdef SCRATCH
    /* Use scratch memory for internal temporary vectors */
    WebRtc_Word16 *pw16_downSampSpeech = pw16_scratchPtr + SCRATCH_PW16_DS_SPEECH;
    WebRtc_Word32 *pw32_corr = (WebRtc_Word32*) (pw16_scratchPtr + SCRATCH_PW32_CORR);
    WebRtc_Word16 *pw16_corr = pw16_scratchPtr + SCRATCH_PW16_CORR;
#else
    /* Allocate memory for temporary vectors */
    WebRtc_Word16 pw16_downSampSpeech[ACCELERATE_DOWNSAMPLED_LEN];
    WebRtc_Word32 pw32_corr[ACCELERATE_CORR_LEN];
    WebRtc_Word16 pw16_corr[ACCELERATE_CORR_LEN];
#endif
    WebRtc_Word16 w16_decodedMax = 0;
    WebRtc_Word16 w16_tmp;
    WebRtc_Word16 w16_tmp2;
    WebRtc_Word32 w32_tmp;
    WebRtc_Word32 w32_tmp2;

    const WebRtc_Word16 w16_startLag = ACCELERATE_MIN_LAG;
    const WebRtc_Word16 w16_endLag = ACCELERATE_MAX_LAG;
    const WebRtc_Word16 w16_corrLen = ACCELERATE_CORR_LEN;
    const WebRtc_Word16 *pw16_vec1, *pw16_vec2;
    WebRtc_Word16 *pw16_vectmp;
    WebRtc_Word16 w16_inc, w16_startfact;
    WebRtc_Word16 w16_bestIndex, w16_bestVal;
    WebRtc_Word16 w16_VAD = 1;
    WebRtc_Word16 fsMult;
    WebRtc_Word16 fsMult120;
    WebRtc_Word32 w32_en1, w32_en2, w32_cc;
    WebRtc_Word16 w16_en1, w16_en2;
    WebRtc_Word16 w16_en1Scale, w16_en2Scale;
    WebRtc_Word16 w16_sqrtEn1En2;
    WebRtc_Word16 w16_bestCorr = 0;
    int ok;

#ifdef NETEQ_STEREO
    MasterSlaveInfo *msInfo = inst->msInfo;
#endif

    fsMult = WebRtcNetEQ_CalcFsMult(inst->fs); /* Calculate fs/8000 */

    /* Pre-calculate common multiplication with fsMult */
    fsMult120 = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16(fsMult, 120); /* 15 ms */

    inst->ExpandInst.w16_consecExp = 0; /* Last was not expand any more */

    /* Sanity check for len variable; must be (almost) 30 ms 
     (120*fsMult + max(bestIndex)) */
    if (len < (WebRtc_Word16) WEBRTC_SPL_MUL_16_16((120 + 119), fsMult))
    {
        /* Length of decoded data too short */
        inst->w16_mode = MODE_UNSUCCESS_ACCELERATE;
        *pw16_len = len;

        /* simply move all data from decoded to outData */
        WEBRTC_SPL_MEMMOVE_W16(pw16_outData, pw16_decoded, (WebRtc_Word16) len);

        return NETEQ_OTHER_ERROR;
    }

    /***********************************/
    /* Special operations for BGN only */
    /***********************************/

    /* Check if "background noise only" flag is set */
    if (BGNonly)
    {
        /* special operation for BGN only; simply remove a chunk of data */
        w16_bestIndex = DEFAULT_TIME_ADJUST * WEBRTC_SPL_LSHIFT_W16(fsMult, 3); /* X*fs/1000 */

        /* Sanity check for bestIndex */
        if (w16_bestIndex > len)
        { /* not good, do nothing instead */
            inst->w16_mode = MODE_UNSUCCESS_ACCELERATE;
            *pw16_len = len;

            /* simply move all data from decoded to outData */
            WEBRTC_SPL_MEMMOVE_W16(pw16_outData, pw16_decoded, (WebRtc_Word16) len);

            return NETEQ_OTHER_ERROR;
        }

        /* set length parameter */
        *pw16_len = len - w16_bestIndex; /* we remove bestIndex samples */

        /* copy to output */
        WEBRTC_SPL_MEMMOVE_W16(pw16_outData, pw16_decoded, *pw16_len);

        /* set mode */
        inst->w16_mode = MODE_LOWEN_ACCELERATE;

        /* update statistics */
        inst->statInst.accelerateLength += w16_bestIndex;

        return 0;
    } /* end of special code for BGN mode */

#ifdef NETEQ_STEREO

    /* Sanity for msInfo */
    if (msInfo == NULL)
    {
        /* this should not happen here */
        return MASTER_SLAVE_ERROR;
    }

    if (msInfo->msMode != NETEQ_SLAVE)
    {
        /* Find correlation lag only for non-slave instances */

#endif

        /****************************************************************/
        /* Find the strongest correlation lag by downsampling to 4 kHz, */
        /* calculating correlation for downsampled signal and finding   */
        /* the strongest correlation peak.                              */
        /****************************************************************/

        /* find maximum absolute value */
        w16_decodedMax = WebRtcSpl_MaxAbsValueW16(pw16_decoded, (WebRtc_Word16) len);

        /* downsample the decoded speech to 4 kHz */
        ok = WebRtcNetEQ_DownSampleTo4kHz(pw16_decoded, len, inst->fs, pw16_downSampSpeech,
            ACCELERATE_DOWNSAMPLED_LEN, 1 /* compensate delay*/);
        if (ok != 0)
        {
            /* error */
            inst->w16_mode = MODE_UNSUCCESS_ACCELERATE;
            *pw16_len = len;
            /* simply move all data from decoded to outData */
            WEBRTC_SPL_MEMMOVE_W16(pw16_outData, pw16_decoded, (WebRtc_Word16) len);
            return NETEQ_OTHER_ERROR;
        }

        /*
         * Set scaling factor for cross correlation to protect against overflow
         * (log2(50) => 6)
         */
        w16_tmp = 6 - WebRtcSpl_NormW32(WEBRTC_SPL_MUL_16_16(w16_decodedMax, w16_decodedMax));
        w16_tmp = WEBRTC_SPL_MAX(0, w16_tmp);

        /* Perform correlation from lag 10 to lag 60 in 4 kHz domain */
        WebRtcNetEQ_CrossCorr(
            pw32_corr, &pw16_downSampSpeech[w16_endLag],
            &pw16_downSampSpeech[w16_endLag - w16_startLag], w16_corrLen,
            (WebRtc_Word16) (w16_endLag - w16_startLag), w16_tmp, -1);

        /* Normalize correlation to 14 bits and put in a WebRtc_Word16 vector */
        w32_tmp = WebRtcSpl_MaxAbsValueW32(pw32_corr, w16_corrLen);
        w16_tmp = 17 - WebRtcSpl_NormW32(w32_tmp);
        w16_tmp = WEBRTC_SPL_MAX(0, w16_tmp);

        WebRtcSpl_VectorBitShiftW32ToW16(pw16_corr, w16_corrLen, pw32_corr, w16_tmp);

#ifdef NETEQ_STEREO
    } /* end if (msInfo->msMode != NETEQ_SLAVE) */

    if ((msInfo->msMode == NETEQ_MASTER) || (msInfo->msMode == NETEQ_MONO))
    {
        /* Find the strongest correlation peak by using the parabolic fit method */
        WebRtcNetEQ_PeakDetection(pw16_corr, (WebRtc_Word16) w16_corrLen, 1, fsMult,
            &w16_bestIndex, &w16_bestVal);
        /* 0 <= bestIndex <= (2*corrLen - 1)*fsMult = 99*fsMult */

        /* Compensate bestIndex for displaced starting position */
        w16_bestIndex = w16_bestIndex + w16_startLag * WEBRTC_SPL_LSHIFT_W16(fsMult, 1);
        /* 20*fsMult <= bestIndex <= 119*fsMult */

        msInfo->bestIndex = w16_bestIndex;
    }
    else if (msInfo->msMode == NETEQ_SLAVE)
    {
        if (msInfo->extraInfo == ACC_FAIL)
        {
            /* Master has signaled an unsuccessful accelerate */
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
        return MASTER_SLAVE_ERROR;
    }

#else /* NETEQ_STEREO */

    /* Find the strongest correlation peak by using the parabolic fit method */
    WebRtcNetEQ_PeakDetection(pw16_corr, (WebRtc_Word16) w16_corrLen, 1, fsMult,
        &w16_bestIndex, &w16_bestVal);
    /* 0 <= bestIndex <= (2*corrLen - 1)*fsMult = 99*fsMult */

    /* Compensate bestIndex for displaced starting position */
    w16_bestIndex = w16_bestIndex + w16_startLag * WEBRTC_SPL_LSHIFT_W16(fsMult, 1);
    /* 20*fsMult <= bestIndex <= 119*fsMult */

#endif /* NETEQ_STEREO */

#ifdef NETEQ_STEREO

    if (msInfo->msMode != NETEQ_SLAVE)
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
        w32_en1 = WebRtcNetEQ_DotW16W16((WebRtc_Word16*) pw16_vec1,
            (WebRtc_Word16*) pw16_vec1, w16_bestIndex, w16_tmp);
        w32_en2 = WebRtcNetEQ_DotW16W16((WebRtc_Word16*) pw16_vec2,
            (WebRtc_Word16*) pw16_vec2, w16_bestIndex, w16_tmp);

        /* Calculate cross-correlation at the found lag */
        w32_cc = WebRtcNetEQ_DotW16W16((WebRtc_Word16*) pw16_vec1, (WebRtc_Word16*) pw16_vec2,
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
        w16_tmp2 = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(w32_tmp2, w16_tmp2);
        w32_tmp2 = WEBRTC_SPL_MUL_16_16(w16_bestIndex, w16_tmp2);

        /* Scale w32_tmp properly before comparing with w32_tmp2 */
        /* (w16_tmp is scaling before energy calculation, thus 2*w16_tmp) */
        if (WebRtcSpl_NormW32(w32_tmp) < WEBRTC_SPL_LSHIFT_W32(w16_tmp,1))
        {
            /* Cannot scale only w32_tmp, must scale w32_temp2 too */
            WebRtc_Word16 tempshift = WebRtcSpl_NormW32(w32_tmp);
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

            /* Convert energies to WebRtc_Word16 */
            w16_en1 = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(w32_en1, w16_en1Scale);
            w16_en2 = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(w32_en2, w16_en2Scale);

            /* Calculate energy product */
            w32_tmp = WEBRTC_SPL_MUL_16_16(w16_en1, w16_en2);

            /* Calculate square-root of energy product */
            w16_sqrtEn1En2 = (WebRtc_Word16) WebRtcSpl_SqrtFloor(w32_tmp);

            /* Calculate cc/sqrt(en1*en2) in Q14 */
            w16_tmp = 14 - WEBRTC_SPL_RSHIFT_W16(w16_en1Scale+w16_en2Scale, 1);
            w32_cc = WEBRTC_SPL_SHIFT_W32(w32_cc, w16_tmp);
            w32_cc = WEBRTC_SPL_MAX(0, w32_cc); /* Don't divide with negative number */
            w16_bestCorr = (WebRtc_Word16) WebRtcSpl_DivW32W16(w32_cc, w16_sqrtEn1En2);
            w16_bestCorr = WEBRTC_SPL_MIN(16384, w16_bestCorr); /* set maximum to 1.0 */
        }

#ifdef NETEQ_STEREO

    } /* end if (msInfo->msMode != NETEQ_SLAVE) */

#endif /* NETEQ_STEREO */

    /************************************************/
    /* Check accelerate criteria and remove samples */
    /************************************************/

    /* Check for strong correlation (>0.9) or passive speech */
#ifdef NETEQ_STEREO
    if ((((w16_bestCorr > 14746) || (w16_VAD == 0)) && (msInfo->msMode != NETEQ_SLAVE))
        || ((msInfo->msMode == NETEQ_SLAVE) && (msInfo->extraInfo != ACC_FAIL)))
#else
    if ((w16_bestCorr > 14746) || (w16_VAD == 0))
#endif
    {
        /* Do accelerate operation by overlap add */

        /*
         * Calculate cross-fading slope so that the fading factor goes from
         * 1 (16384 in Q14) to 0 in one pitch period (bestIndex).
         */
        w16_inc = (WebRtc_Word16) WebRtcSpl_DivW32W16((WebRtc_Word32) 16384,
            (WebRtc_Word16) (w16_bestIndex + 1)); /* in Q14 */

        /* Initiate fading factor */
        w16_startfact = 16384 - w16_inc;

        /* vec1 starts at 15 ms minus one pitch period */
        pw16_vec1 = &pw16_decoded[fsMult120 - w16_bestIndex];
        /* vec2 start at 15 ms */
        pw16_vec2 = &pw16_decoded[fsMult120];

        /* Copy unmodified part [0 to 15 ms minus 1 pitch period] */
        w16_tmp = (fsMult120 - w16_bestIndex);
        WEBRTC_SPL_MEMMOVE_W16(pw16_outData, pw16_decoded, w16_tmp);

        /* Generate interpolated part of length bestIndex (1 pitch period) */
        pw16_vectmp = pw16_outData + w16_tmp; /* start of interpolation output */
        /* Reuse mixing function from Expand */
        WebRtcNetEQ_MixVoiceUnvoice(pw16_vectmp, (WebRtc_Word16*) pw16_vec1,
            (WebRtc_Word16*) pw16_vec2, &w16_startfact, w16_inc, w16_bestIndex);

        /* Move the last part (also unmodified) */
        /* Take from decoded at 15 ms + 1 pitch period */
        pw16_vec2 = &pw16_decoded[fsMult120 + w16_bestIndex];
        WEBRTC_SPL_MEMMOVE_W16(&pw16_outData[fsMult120], pw16_vec2,
            (WebRtc_Word16) (len - fsMult120 - w16_bestIndex));

        /* Set the mode flag */
        if (w16_VAD)
        {
            inst->w16_mode = MODE_SUCCESS_ACCELERATE;
        }
        else
        {
            inst->w16_mode = MODE_LOWEN_ACCELERATE;
        }

        /* Calculate resulting length = original length - pitch period */
        *pw16_len = len - w16_bestIndex;

        /* Update in-call statistics */
        inst->statInst.accelerateLength += w16_bestIndex;

        return 0;
    }
    else
    {
        /* Accelerate not allowed */

#ifdef NETEQ_STEREO
        /* Signal to slave(s) that this was unsuccessful */
        if (msInfo->msMode == NETEQ_MASTER)
        {
            msInfo->extraInfo = ACC_FAIL;
        }
#endif

        /* Set mode flag to unsuccessful accelerate */
        inst->w16_mode = MODE_UNSUCCESS_ACCELERATE;

        /* Length is unmodified */
        *pw16_len = len;

        /* Simply move all data from decoded to outData */
        WEBRTC_SPL_MEMMOVE_W16(pw16_outData, pw16_decoded, (WebRtc_Word16) len);

        return 0;
    }
}

#undef SCRATCH_PW16_DS_SPEECH
#undef SCRATCH_PW32_CORR
#undef SCRATCH_PW16_CORR
