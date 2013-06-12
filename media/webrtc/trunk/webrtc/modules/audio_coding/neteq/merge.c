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
 * This is the function to merge a new packet with expanded data after a packet loss.
 */

#include "dsp.h"

#include "signal_processing_library.h"

#include "dsp_helpfunctions.h"
#include "neteq_error_codes.h"

/****************************************************************************
 * WebRtcNetEQ_Merge(...)
 *
 * This function...
 *
 * Input:
 *      - inst          : NetEQ DSP instance
 *      - scratchPtr    : Pointer to scratch vector.
 *      - decoded       : Pointer to new decoded speech.
 *      - len           : Number of samples in pw16_decoded.
 *
 *
 * Output:
 *      - inst          : Updated user information
 *      - outData       : Pointer to a memory space where the output data
 *                        should be stored
 *      - pw16_len      : Number of samples written to pw16_outData
 *
 * Return value         :  0 - Ok
 *                        <0 - Error
 */

/* Scratch usage:

 Type           Name                    size            startpos        endpos
 WebRtc_Word16  pw16_expanded           210*fs/8000     0               209*fs/8000
 WebRtc_Word16  pw16_expandedLB         100             210*fs/8000     99+210*fs/8000
 WebRtc_Word16  pw16_decodedLB          40              100+210*fs/8000 139+210*fs/8000
 WebRtc_Word32  pw32_corr               2*60            140+210*fs/8000 260+210*fs/8000
 WebRtc_Word16  pw16_corrVec            68              210*fs/8000     67+210*fs/8000

 [gap in scratch vector]

 func           WebRtcNetEQ_Expand      40+370*fs/8000  126*fs/8000     39+496*fs/8000

 Total:  40+496*fs/8000
 */

#define SCRATCH_pw16_expanded          0
#if (defined(NETEQ_48KHZ_WIDEBAND)) 
#define SCRATCH_pw16_expandedLB        1260
#define SCRATCH_pw16_decodedLB         1360
#define SCRATCH_pw32_corr              1400
#define SCRATCH_pw16_corrVec           1260
#define SCRATCH_NETEQ_EXPAND            756
#elif (defined(NETEQ_32KHZ_WIDEBAND)) 
#define SCRATCH_pw16_expandedLB        840
#define SCRATCH_pw16_decodedLB         940
#define SCRATCH_pw32_corr              980
#define SCRATCH_pw16_corrVec           840
#define SCRATCH_NETEQ_EXPAND            504
#elif (defined(NETEQ_WIDEBAND)) 
#define SCRATCH_pw16_expandedLB        420
#define SCRATCH_pw16_decodedLB         520
#define SCRATCH_pw32_corr              560
#define SCRATCH_pw16_corrVec           420
#define SCRATCH_NETEQ_EXPAND            252
#else    /* NB */
#define SCRATCH_pw16_expandedLB        210
#define SCRATCH_pw16_decodedLB         310
#define SCRATCH_pw32_corr              350
#define SCRATCH_pw16_corrVec           210
#define SCRATCH_NETEQ_EXPAND            126
#endif

int WebRtcNetEQ_Merge(DSPInst_t *inst,
#ifdef SCRATCH
                      WebRtc_Word16 *pw16_scratchPtr,
#endif
                      WebRtc_Word16 *pw16_decoded, int len, WebRtc_Word16 *pw16_outData,
                      WebRtc_Word16 *pw16_len)
{

    WebRtc_Word16 fs_mult;
    WebRtc_Word16 fs_shift;
    WebRtc_Word32 w32_En_new_frame, w32_En_old_frame;
    WebRtc_Word16 w16_expmax, w16_newmax;
    WebRtc_Word16 w16_tmp, w16_tmp2;
    WebRtc_Word32 w32_tmp;
#ifdef SCRATCH
    WebRtc_Word16 *pw16_expanded = pw16_scratchPtr + SCRATCH_pw16_expanded;
    WebRtc_Word16 *pw16_expandedLB = pw16_scratchPtr + SCRATCH_pw16_expandedLB;
    WebRtc_Word16 *pw16_decodedLB = pw16_scratchPtr + SCRATCH_pw16_decodedLB;
    WebRtc_Word32 *pw32_corr = (WebRtc_Word32*) (pw16_scratchPtr + SCRATCH_pw32_corr);
    WebRtc_Word16 *pw16_corrVec = pw16_scratchPtr + SCRATCH_pw16_corrVec;
#else
    WebRtc_Word16 pw16_expanded[(125+80+5)*FSMULT];
    WebRtc_Word16 pw16_expandedLB[100];
    WebRtc_Word16 pw16_decodedLB[40];
    WebRtc_Word32 pw32_corr[60];
    WebRtc_Word16 pw16_corrVec[4+60+4];
#endif
    WebRtc_Word16 *pw16_corr = &pw16_corrVec[4];
    WebRtc_Word16 w16_stopPos = 0, w16_bestIndex, w16_interpLen;
    WebRtc_Word16 w16_bestVal; /* bestVal is dummy */
    WebRtc_Word16 w16_startfact, w16_inc;
    WebRtc_Word16 w16_expandedLen;
    WebRtc_Word16 w16_startPos;
    WebRtc_Word16 w16_expLen, w16_newLen = 0;
    WebRtc_Word16 *pw16_decodedOut;
    WebRtc_Word16 w16_muted;

    int w16_decodedLen = len;

#ifdef NETEQ_STEREO
    MasterSlaveInfo *msInfo = inst->msInfo;
#endif

    fs_mult = WebRtcSpl_DivW32W16ResW16(inst->fs, 8000);
    fs_shift = 30 - WebRtcSpl_NormW32(fs_mult); /* Note that this is not "exact" for 48kHz */

    /*************************************
     * Generate data to merge with
     *************************************/
    /*
     * Check how much data that is left since earlier
     * (at least there should be the overlap)...
     */
    w16_startPos = inst->endPosition - inst->curPosition;
    /* Get one extra expansion to merge and overlap with */
    inst->ExpandInst.w16_stopMuting = 1;
    inst->ExpandInst.w16_lagsDirection = 1; /* make sure we get the "optimal" lag */
    inst->ExpandInst.w16_lagsPosition = -1; /* out of the 3 possible ones */
    w16_expandedLen = 0; /* Does not fill any function currently */

    if (w16_startPos >= 210 * FSMULT)
    {
        /*
         * The number of samples available in the sync buffer is more than what fits in
         * pw16_expanded.Keep the first 210*FSMULT samples, but shift them towards the end of
         * the buffer. This is ok, since all of the buffer will be expand data anyway, so as
         * long as the beginning is left untouched, we're fine.
         */

        w16_tmp = w16_startPos - 210 * FSMULT; /* length difference */

        WEBRTC_SPL_MEMMOVE_W16(&inst->speechBuffer[inst->curPosition+w16_tmp] ,
                               &inst->speechBuffer[inst->curPosition], 210*FSMULT);

        inst->curPosition += w16_tmp; /* move start position of sync buffer accordingly */
        w16_startPos = 210 * FSMULT; /* this is the truncated length */
    }

    WebRtcNetEQ_Expand(inst,
#ifdef SCRATCH
        pw16_scratchPtr + SCRATCH_NETEQ_EXPAND,
#endif
        pw16_expanded, /* let Expand write to beginning of pw16_expanded to avoid overflow */
        &w16_newLen, 0);

    /*
     * Now shift the data in pw16_expanded to where it belongs.
     * Truncate all that ends up outside the vector.
     */

    WEBRTC_SPL_MEMMOVE_W16(&pw16_expanded[w16_startPos], pw16_expanded,
                           WEBRTC_SPL_MIN(w16_newLen,
                               WEBRTC_SPL_MAX(210*FSMULT - w16_startPos, 0) ) );

    inst->ExpandInst.w16_stopMuting = 0;

    /* Copy what is left since earlier into the expanded vector */

    WEBRTC_SPL_MEMCPY_W16(pw16_expanded, &inst->speechBuffer[inst->curPosition], w16_startPos);

    /*
     * Do "ugly" copy and paste from the expanded in order to generate more data
     * to correlate (but not interpolate) with.
     */
    w16_expandedLen = (120 + 80 + 2) * fs_mult;
    w16_expLen = w16_startPos + w16_newLen;

    if (w16_expLen < w16_expandedLen)
    {
        while ((w16_expLen + w16_newLen) < w16_expandedLen)
        {
            WEBRTC_SPL_MEMCPY_W16(&pw16_expanded[w16_expLen], &pw16_expanded[w16_startPos],
                w16_newLen);
            w16_expLen += w16_newLen;
        }

        /* Copy last part (fraction of a whole expansion) */

        WEBRTC_SPL_MEMCPY_W16(&pw16_expanded[w16_expLen], &pw16_expanded[w16_startPos],
                              (w16_expandedLen-w16_expLen));
    }
    w16_expLen = w16_expandedLen;

    /* Adjust muting factor (main muting factor times expand muting factor) */
    inst->w16_muteFactor
        = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16_RSFT(inst->w16_muteFactor,
            inst->ExpandInst.w16_expandMuteFactor, 14);

    /* Adjust muting factor if new vector is more or less of the BGN energy */
    len = WEBRTC_SPL_MIN(64*fs_mult, w16_decodedLen);
    w16_expmax = WebRtcSpl_MaxAbsValueW16(pw16_expanded, (WebRtc_Word16) len);
    w16_newmax = WebRtcSpl_MaxAbsValueW16(pw16_decoded, (WebRtc_Word16) len);

    /* Calculate energy of old data */
    w16_tmp = 6 + fs_shift - WebRtcSpl_NormW32(WEBRTC_SPL_MUL_16_16(w16_expmax, w16_expmax));
    w16_tmp = WEBRTC_SPL_MAX(w16_tmp,0);
    w32_En_old_frame = WebRtcNetEQ_DotW16W16(pw16_expanded, pw16_expanded, len, w16_tmp);

    /* Calculate energy of new data */
    w16_tmp2 = 6 + fs_shift - WebRtcSpl_NormW32(WEBRTC_SPL_MUL_16_16(w16_newmax, w16_newmax));
    w16_tmp2 = WEBRTC_SPL_MAX(w16_tmp2,0);
    w32_En_new_frame = WebRtcNetEQ_DotW16W16(pw16_decoded, pw16_decoded, len, w16_tmp2);

    /* Align to same Q-domain */
    if (w16_tmp2 > w16_tmp)
    {
        w32_En_old_frame = WEBRTC_SPL_RSHIFT_W32(w32_En_old_frame, (w16_tmp2-w16_tmp));
    }
    else
    {
        w32_En_new_frame = WEBRTC_SPL_RSHIFT_W32(w32_En_new_frame, (w16_tmp-w16_tmp2));
    }

    /* Calculate muting factor to use for new frame */
    if (w32_En_new_frame > w32_En_old_frame)
    {
        /* Normalize w32_En_new_frame to 14 bits */
        w16_tmp = WebRtcSpl_NormW32(w32_En_new_frame) - 17;
        w32_En_new_frame = WEBRTC_SPL_SHIFT_W32(w32_En_new_frame, w16_tmp);

        /*
         * Put w32_En_old_frame in a domain 14 higher, so that
         * w32_En_old_frame/w32_En_new_frame is in Q14
         */
        w16_tmp = w16_tmp + 14;
        w32_En_old_frame = WEBRTC_SPL_SHIFT_W32(w32_En_old_frame, w16_tmp);
        w16_tmp
            = WebRtcSpl_DivW32W16ResW16(w32_En_old_frame, (WebRtc_Word16) w32_En_new_frame);
        /* Calculate sqrt(w32_En_old_frame/w32_En_new_frame) in Q14 */
        w16_muted = (WebRtc_Word16) WebRtcSpl_SqrtFloor(
            WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)w16_tmp,14));
    }
    else
    {
        w16_muted = 16384; /* Set = 1.0 when old frame has higher energy than new */
    }

    /* Set the raise the continued muting factor w16_muted if w16_muteFactor is lower */
    if (w16_muted > inst->w16_muteFactor)
    {
        inst->w16_muteFactor = WEBRTC_SPL_MIN(w16_muted, 16384);
    }

#ifdef NETEQ_STEREO

    /* Sanity for msInfo */
    if (msInfo == NULL)
    {
        /* this should not happen here */
        return MASTER_SLAVE_ERROR;
    }

    /* do not downsample and calculate correlations for slave instance(s) */
    if ((msInfo->msMode == NETEQ_MASTER) || (msInfo->msMode == NETEQ_MONO))
    {
#endif

        /*********************************************
         * Downsample to 4kHz and find best overlap
         *********************************************/

        /* Downsample to 4 kHz */
        if (inst->fs == 8000)
        {
            WebRtcSpl_DownsampleFast(&pw16_expanded[2], (WebRtc_Word16) (w16_expandedLen - 2),
                pw16_expandedLB, (WebRtc_Word16) (100),
                (WebRtc_Word16*) WebRtcNetEQ_kDownsample8kHzTbl, (WebRtc_Word16) 3,
                (WebRtc_Word16) 2, (WebRtc_Word16) 0);
            if (w16_decodedLen <= 80)
            {
                /* Not quite long enough, so we have to cheat a bit... */
                WebRtc_Word16 temp_len = w16_decodedLen - 2;
                w16_tmp = temp_len / 2;
                WebRtcSpl_DownsampleFast(&pw16_decoded[2], temp_len,
                                         pw16_decodedLB, w16_tmp,
                                         (WebRtc_Word16*) WebRtcNetEQ_kDownsample8kHzTbl,
                    (WebRtc_Word16) 3, (WebRtc_Word16) 2, (WebRtc_Word16) 0);
                WebRtcSpl_MemSetW16(&pw16_decodedLB[w16_tmp], 0, (40 - w16_tmp));
            }
            else
            {
                WebRtcSpl_DownsampleFast(&pw16_decoded[2],
                    (WebRtc_Word16) (w16_decodedLen - 2), pw16_decodedLB,
                    (WebRtc_Word16) (40), (WebRtc_Word16*) WebRtcNetEQ_kDownsample8kHzTbl,
                    (WebRtc_Word16) 3, (WebRtc_Word16) 2, (WebRtc_Word16) 0);
            }
#ifdef NETEQ_WIDEBAND
        }
        else if (inst->fs==16000)
        {
            WebRtcSpl_DownsampleFast(
                &pw16_expanded[4], (WebRtc_Word16)(w16_expandedLen-4),
                pw16_expandedLB, (WebRtc_Word16)(100),
                (WebRtc_Word16*)WebRtcNetEQ_kDownsample16kHzTbl, (WebRtc_Word16)5,
                (WebRtc_Word16)4, (WebRtc_Word16)0);
            if (w16_decodedLen<=160)
            {
                /* Not quite long enough, so we have to cheat a bit... */
                WebRtc_Word16 temp_len = w16_decodedLen - 4;
                w16_tmp = temp_len / 4;
                WebRtcSpl_DownsampleFast(
                    &pw16_decoded[4], temp_len,
                    pw16_decodedLB, w16_tmp,
                    (WebRtc_Word16*)WebRtcNetEQ_kDownsample16kHzTbl, (WebRtc_Word16)5,
                    (WebRtc_Word16)4, (WebRtc_Word16)0);
                WebRtcSpl_MemSetW16(&pw16_decodedLB[w16_tmp], 0, (40-w16_tmp));
            }
            else
            {
                WebRtcSpl_DownsampleFast(
                    &pw16_decoded[4], (WebRtc_Word16)(w16_decodedLen-4),
                    pw16_decodedLB, (WebRtc_Word16)(40),
                    (WebRtc_Word16*)WebRtcNetEQ_kDownsample16kHzTbl, (WebRtc_Word16)5,
                    (WebRtc_Word16)4, (WebRtc_Word16)0);
            }
#endif
#ifdef NETEQ_32KHZ_WIDEBAND
        }
        else if (inst->fs==32000)
        {
            /*
             * TODO(hlundin) Why is the offset into pw16_expanded 6?
             */
            WebRtcSpl_DownsampleFast(
                &pw16_expanded[6], (WebRtc_Word16)(w16_expandedLen-6),
                pw16_expandedLB, (WebRtc_Word16)(100),
                (WebRtc_Word16*)WebRtcNetEQ_kDownsample32kHzTbl, (WebRtc_Word16)7,
                (WebRtc_Word16)8, (WebRtc_Word16)0);
            if (w16_decodedLen<=320)
            {
                /* Not quite long enough, so we have to cheat a bit... */
                WebRtc_Word16 temp_len = w16_decodedLen - 6;
                w16_tmp = temp_len / 8;
                WebRtcSpl_DownsampleFast(
                      &pw16_decoded[6], temp_len,
                      pw16_decodedLB, w16_tmp,
                      (WebRtc_Word16*)WebRtcNetEQ_kDownsample32kHzTbl, (WebRtc_Word16)7,
                      (WebRtc_Word16)8, (WebRtc_Word16)0);
                WebRtcSpl_MemSetW16(&pw16_decodedLB[w16_tmp], 0, (40-w16_tmp));
            }
            else
            {
                WebRtcSpl_DownsampleFast(
                    &pw16_decoded[6], (WebRtc_Word16)(w16_decodedLen-6),
                    pw16_decodedLB, (WebRtc_Word16)(40),
                    (WebRtc_Word16*)WebRtcNetEQ_kDownsample32kHzTbl, (WebRtc_Word16)7,
                    (WebRtc_Word16)8, (WebRtc_Word16)0);
            }
#endif
#ifdef NETEQ_48KHZ_WIDEBAND
        }
        else /* if (inst->fs==48000) */
        {
            /*
             * TODO(hlundin) Why is the offset into pw16_expanded 6?
             */
            WebRtcSpl_DownsampleFast(
                &pw16_expanded[6], (WebRtc_Word16)(w16_expandedLen-6),
                pw16_expandedLB, (WebRtc_Word16)(100),
                (WebRtc_Word16*)WebRtcNetEQ_kDownsample48kHzTbl, (WebRtc_Word16)7,
                (WebRtc_Word16)12, (WebRtc_Word16)0);
            if (w16_decodedLen<=320)
            {
                /* Not quite long enough, so we have to cheat a bit... */
                /*
                 * TODO(hlundin): Is this correct? Downsampling is a factor 12
                 * but w16_tmp = temp_len / 8.
                 * (Was w16_tmp = ((w16_decodedLen-6)>>3) before re-write.)
                 */
                WebRtc_Word16 temp_len = w16_decodedLen - 6;
                w16_tmp = temp_len / 8;
                WebRtcSpl_DownsampleFast(
                    &pw16_decoded[6], temp_len,
                    pw16_decodedLB, w16_tmp,
                    (WebRtc_Word16*)WebRtcNetEQ_kDownsample48kHzTbl, (WebRtc_Word16)7,
                    (WebRtc_Word16)12, (WebRtc_Word16)0);
                WebRtcSpl_MemSetW16(&pw16_decodedLB[w16_tmp], 0, (40-w16_tmp));
            }
            else
            {
                WebRtcSpl_DownsampleFast(
                    &pw16_decoded[6], (WebRtc_Word16)(w16_decodedLen-6),
                    pw16_decodedLB, (WebRtc_Word16)(40),
                    (WebRtc_Word16*)WebRtcNetEQ_kDownsample48kHzTbl, (WebRtc_Word16)7,
                    (WebRtc_Word16)12, (WebRtc_Word16)0);
            }
#endif
        }

        /* Calculate correlation without any normalization (40 samples) */
        w16_tmp = WebRtcSpl_DivW32W16ResW16((WebRtc_Word32) inst->ExpandInst.w16_maxLag,
            (WebRtc_Word16) (fs_mult * 2)) + 1;
        w16_stopPos = WEBRTC_SPL_MIN(60, w16_tmp);
        w32_tmp = WEBRTC_SPL_MUL_16_16(w16_expmax, w16_newmax);
        if (w32_tmp > 26843546)
        {
            w16_tmp = 3;
        }
        else
        {
            w16_tmp = 0;
        }

        WebRtcNetEQ_CrossCorr(pw32_corr, pw16_decodedLB, pw16_expandedLB, 40,
            (WebRtc_Word16) w16_stopPos, w16_tmp, 1);

        /* Normalize correlation to 14 bits and put in a WebRtc_Word16 vector */
        WebRtcSpl_MemSetW16(pw16_corrVec, 0, (4 + 60 + 4));
        w32_tmp = WebRtcSpl_MaxAbsValueW32(pw32_corr, w16_stopPos);
        w16_tmp = 17 - WebRtcSpl_NormW32(w32_tmp);
        w16_tmp = WEBRTC_SPL_MAX(0, w16_tmp);

        WebRtcSpl_VectorBitShiftW32ToW16(pw16_corr, w16_stopPos, pw32_corr, w16_tmp);

        /* Calculate allowed starting point for peak finding.
         The peak location bestIndex must fulfill two criteria:
         (1) w16_bestIndex+w16_decodedLen < inst->timestampsPerCall+inst->ExpandInst.w16_overlap
         (2) w16_bestIndex+w16_decodedLen < w16_startPos */
        w16_tmp = WEBRTC_SPL_MAX(0, WEBRTC_SPL_MAX(w16_startPos,
                inst->timestampsPerCall+inst->ExpandInst.w16_overlap) - w16_decodedLen);
        /* Downscale starting index to 4kHz domain */
        w16_tmp2 = WebRtcSpl_DivW32W16ResW16((WebRtc_Word32) w16_tmp,
            (WebRtc_Word16) (fs_mult << 1));

#ifdef NETEQ_STEREO
    } /* end if (msInfo->msMode != NETEQ_SLAVE)  */

    if ((msInfo->msMode == NETEQ_MASTER) || (msInfo->msMode == NETEQ_MONO))
    {
        /* This is master or mono instance; find peak */
        WebRtcNetEQ_PeakDetection(&pw16_corr[w16_tmp2], w16_stopPos, 1, fs_mult, &w16_bestIndex,
            &w16_bestVal);
        w16_bestIndex += w16_tmp; /* compensate for modified starting index */
        msInfo->bestIndex = w16_bestIndex;
    }
    else if (msInfo->msMode == NETEQ_SLAVE)
    {
        /* Get peak location from master instance */
        w16_bestIndex = msInfo->bestIndex;
    }
    else
    {
        /* Invalid mode */
        return MASTER_SLAVE_ERROR;
    }

#else /* NETEQ_STEREO */

    /* Find peak */
    WebRtcNetEQ_PeakDetection(&pw16_corr[w16_tmp2], w16_stopPos, 1, fs_mult, &w16_bestIndex,
        &w16_bestVal);
    w16_bestIndex += w16_tmp; /* compensate for modified starting index */

#endif /* NETEQ_STEREO */

    /*
     * Ensure that underrun does not occur for 10ms case => we have to get at least
     * 10ms + overlap . (This should never happen thanks to the above modification of
     * peak-finding starting point.)
     * */
    while ((w16_bestIndex + w16_decodedLen) < (inst->timestampsPerCall
        + inst->ExpandInst.w16_overlap) || w16_bestIndex + w16_decodedLen < w16_startPos)
    {
        w16_bestIndex += w16_newLen; /* Jump one lag ahead */
    }
    pw16_decodedOut = pw16_outData + w16_bestIndex;

    /* Mute the new decoded data if needed (and unmute it linearly) */
    w16_interpLen = WEBRTC_SPL_MIN(60*fs_mult,
        w16_expandedLen-w16_bestIndex); /* this is the overlapping part of pw16_expanded */
    w16_interpLen = WEBRTC_SPL_MIN(w16_interpLen, w16_decodedLen);
    w16_inc = WebRtcSpl_DivW32W16ResW16(4194,
        fs_mult); /* in Q20, 0.004 for NB and 0.002 for WB */
    if (inst->w16_muteFactor < 16384)
    {
        WebRtcNetEQ_UnmuteSignal(pw16_decoded, &inst->w16_muteFactor, pw16_decoded, w16_inc,
            (WebRtc_Word16) w16_interpLen);
        WebRtcNetEQ_UnmuteSignal(&pw16_decoded[w16_interpLen], &inst->w16_muteFactor,
            &pw16_decodedOut[w16_interpLen], w16_inc,
            (WebRtc_Word16) (w16_decodedLen - w16_interpLen));
    }
    else
    {
        /* No muting needed */

        WEBRTC_SPL_MEMMOVE_W16(&pw16_decodedOut[w16_interpLen], &pw16_decoded[w16_interpLen],
            (w16_decodedLen-w16_interpLen));
    }

    /* Do overlap and interpolate linearly */
    w16_inc = WebRtcSpl_DivW32W16ResW16(16384, (WebRtc_Word16) (w16_interpLen + 1)); /* Q14 */
    w16_startfact = (16384 - w16_inc);
    WEBRTC_SPL_MEMMOVE_W16(pw16_outData, pw16_expanded, w16_bestIndex);
    WebRtcNetEQ_MixVoiceUnvoice(pw16_decodedOut, &pw16_expanded[w16_bestIndex], pw16_decoded,
        &w16_startfact, w16_inc, w16_interpLen);

    inst->w16_mode = MODE_MERGE;
    inst->ExpandInst.w16_consecExp = 0; /* Last was not expand any more */

    /* New added length (w16_startPos samples were borrowed) */
    *pw16_len = w16_bestIndex + w16_decodedLen - w16_startPos;

    /* Update VQmon parameter */
    inst->w16_concealedTS += (*pw16_len - w16_decodedLen);
    inst->w16_concealedTS = WEBRTC_SPL_MAX(0, inst->w16_concealedTS);

    /* Update in-call and post-call statistics */
    if (inst->ExpandInst.w16_expandMuteFactor == 0)
    {
        /* expansion generates noise only */
        inst->statInst.expandedNoiseSamples += (*pw16_len - w16_decodedLen);
    }
    else
    {
        /* expansion generates more than only noise */
        inst->statInst.expandedVoiceSamples += (*pw16_len - w16_decodedLen);
    }
    inst->statInst.expandLength += (*pw16_len - w16_decodedLen);


    /* Copy back the first part of the data to the speechHistory */

    WEBRTC_SPL_MEMCPY_W16(&inst->speechBuffer[inst->curPosition], pw16_outData, w16_startPos);


    /* Move data to within outData */

    WEBRTC_SPL_MEMMOVE_W16(pw16_outData, &pw16_outData[w16_startPos], (*pw16_len));

    return 0;
}

#undef     SCRATCH_pw16_expanded
#undef     SCRATCH_pw16_expandedLB
#undef     SCRATCH_pw16_decodedLB
#undef     SCRATCH_pw32_corr
#undef     SCRATCH_pw16_corrVec
#undef     SCRATCH_NETEQ_EXPAND
