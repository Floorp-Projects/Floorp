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
 * This file contains the function for updating the background noise estimate.
 */

#include "dsp.h"

#include "signal_processing_library.h"

#include "dsp_helpfunctions.h"

/* Scratch usage: 
 Designed for BGN_LPC_ORDER <= 10

 Type           Name            size   startpos  endpos
 WebRtc_Word32  pw32_autoCorr   22     0         21  (Length (BGN_LPC_ORDER + 1)*2)
 WebRtc_Word16  pw16_tempVec    10     22        31	(Length BGN_LPC_ORDER)
 WebRtc_Word16  pw16_rc         10     32        41	(Length BGN_LPC_ORDER)
 WebRtc_Word16  pw16_outVec     74     0         73  (Length BGN_LPC_ORDER + 64)

 Total: 74
 */

#if (BGN_LPC_ORDER > 10) && (defined SCRATCH)
#error BGN_LPC_ORDER is too large for current scratch memory allocation
#endif

#define	 SCRATCH_PW32_AUTO_CORR			0
#define	 SCRATCH_PW16_TEMP_VEC			22
#define	 SCRATCH_PW16_RC				32
#define	 SCRATCH_PW16_OUT_VEC			0

#define NETEQFIX_BGNFRAQINCQ16	229 /* 0.0035 in Q16 */

/****************************************************************************
 * WebRtcNetEQ_BGNUpdate(...)
 *
 * This function updates the background noise parameter estimates.
 *
 * Input:
 *		- inst			: NetEQ instance, where the speech history is stored.
 *      - scratchPtr    : Pointer to scratch vector.
 *
 * Output:
 *		- inst			: Updated information about the BGN characteristics.
 *
 * Return value			: No return value
 */

void WebRtcNetEQ_BGNUpdate(
#ifdef SCRATCH
                           DSPInst_t *inst, WebRtc_Word16 *pw16_scratchPtr
#else
                           DSPInst_t *inst
#endif
)
{
    const WebRtc_Word16 w16_vecLen = 256;
    BGNInst_t *BGN_Inst = &(inst->BGNInst);
#ifdef SCRATCH
    WebRtc_Word32 *pw32_autoCorr = (WebRtc_Word32*) (pw16_scratchPtr + SCRATCH_PW32_AUTO_CORR);
    WebRtc_Word16 *pw16_tempVec = pw16_scratchPtr + SCRATCH_PW16_TEMP_VEC;
    WebRtc_Word16 *pw16_rc = pw16_scratchPtr + SCRATCH_PW16_RC;
    WebRtc_Word16 *pw16_outVec = pw16_scratchPtr + SCRATCH_PW16_OUT_VEC;
#else
    WebRtc_Word32 pw32_autoCorr[BGN_LPC_ORDER + 1];
    WebRtc_Word16 pw16_tempVec[BGN_LPC_ORDER];
    WebRtc_Word16 pw16_outVec[BGN_LPC_ORDER + 64];
    WebRtc_Word16 pw16_rc[BGN_LPC_ORDER];
#endif
    WebRtc_Word16 pw16_A[BGN_LPC_ORDER + 1];
    WebRtc_Word32 w32_tmp;
    WebRtc_Word16 *pw16_vec;
    WebRtc_Word16 w16_maxSample;
    WebRtc_Word16 w16_tmp, w16_tmp2;
    WebRtc_Word16 w16_enSampleShift;
    WebRtc_Word32 w32_en, w32_enBGN;
    WebRtc_Word32 w32_enUpdateThreashold;
    WebRtc_Word16 stability;

    pw16_vec = inst->pw16_speechHistory + inst->w16_speechHistoryLen - w16_vecLen;

#ifdef NETEQ_VAD
    if( !inst->VADInst.VADEnabled /* we are not using post-decode VAD */
        || inst->VADInst.VADDecision == 0 )
    { /* ... or, post-decode VAD says passive speaker */
#endif /* NETEQ_VAD */

    /*Insert zeros to guarantee that boundary values do not distort autocorrelation */
    WEBRTC_SPL_MEMCPY_W16(pw16_tempVec, pw16_vec - BGN_LPC_ORDER, BGN_LPC_ORDER);
    WebRtcSpl_MemSetW16(pw16_vec - BGN_LPC_ORDER, 0, BGN_LPC_ORDER);

    w16_maxSample = WebRtcSpl_MaxAbsValueW16(pw16_vec, w16_vecLen);
    w16_tmp = 8 /* log2(w16_veclen) = 8 */
        - WebRtcSpl_NormW32(WEBRTC_SPL_MUL_16_16(w16_maxSample, w16_maxSample));
    w16_tmp = WEBRTC_SPL_MAX(0, w16_tmp);

    WebRtcNetEQ_CrossCorr(pw32_autoCorr, pw16_vec, pw16_vec, w16_vecLen, BGN_LPC_ORDER + 1,
        w16_tmp, -1);

    /* Copy back data */
    WEBRTC_SPL_MEMCPY_W16(pw16_vec - BGN_LPC_ORDER, pw16_tempVec, BGN_LPC_ORDER);

    w16_enSampleShift = 8 - w16_tmp; /* Number of shifts to get energy/sample */
    /* pw32_autoCorr[0]>>w16_enSampleShift */
    w32_en = WEBRTC_SPL_RSHIFT_W32(pw32_autoCorr[0], w16_enSampleShift);
    if ((w32_en < BGN_Inst->w32_energyUpdate
#ifdef NETEQ_VAD
        /* post-decode VAD disabled and w32_en sufficiently low */
         && !inst->VADInst.VADEnabled)
    /* ... or, post-decode VAD says passive speaker */
    || (inst->VADInst.VADEnabled && inst->VADInst.VADDecision == 0)
#else
    ) /* just close the extra parenthesis */
#endif /* NETEQ_VAD */
    )
    {
        /* Generate LPC coefficients */
        if (pw32_autoCorr[0] > 0)
        {
            /* regardless of whether the filter is actually updated or not,
             update energy threshold levels, since we have in fact observed
             a low energy signal */
            if (w32_en < BGN_Inst->w32_energyUpdate)
            {
                /* Never get under 1.0 in average sample energy */
                BGN_Inst->w32_energyUpdate = WEBRTC_SPL_MAX(w32_en, 1);
                BGN_Inst->w32_energyUpdateLow = 0;
            }

            stability = WebRtcSpl_LevinsonDurbin(pw32_autoCorr, pw16_A, pw16_rc, BGN_LPC_ORDER);
            /* Only update BGN if filter is stable */
            if (stability != 1)
            {
                return;
            }
        }
        else
        {
            /* Do not update */
            return;
        }
        /* Generate the CNG gain factor by looking at the energy of the residual */
        WebRtcSpl_FilterMAFastQ12(pw16_vec + w16_vecLen - 64, pw16_outVec, pw16_A,
            BGN_LPC_ORDER + 1, 64);
        w32_enBGN = WebRtcNetEQ_DotW16W16(pw16_outVec, pw16_outVec, 64, 0);
        /* Dot product should never overflow since it is BGN and residual! */

        /*
         * Check spectral flatness
         * Comparing the residual variance with the input signal variance tells
         * if the spectrum is flat or not.
         * (20*w32_enBGN) >= (w32_en<<6)
         * Also ensure that the energy is non-zero.
         */
        if ((WEBRTC_SPL_MUL_32_16(w32_enBGN, 20) >= WEBRTC_SPL_LSHIFT_W32(w32_en, 6))
            && (w32_en > 0))
        {
            /* spectrum is flat enough; save filter parameters */

            WEBRTC_SPL_MEMCPY_W16(BGN_Inst->pw16_filter, pw16_A, BGN_LPC_ORDER+1);
            WEBRTC_SPL_MEMCPY_W16(BGN_Inst->pw16_filterState,
                pw16_vec + w16_vecLen - BGN_LPC_ORDER, BGN_LPC_ORDER);

            /* Save energy level */
            BGN_Inst->w32_energy = WEBRTC_SPL_MAX(w32_en, 1);

            /* Update energy threshold levels */
            /* Never get under 1.0 in average sample energy */
            BGN_Inst->w32_energyUpdate = WEBRTC_SPL_MAX(w32_en, 1);
            BGN_Inst->w32_energyUpdateLow = 0;

            /* Normalize w32_enBGN to 29 or 30 bits before sqrt */
            w16_tmp2 = WebRtcSpl_NormW32(w32_enBGN) - 1;
            if (w16_tmp2 & 0x1)
            {
                w16_tmp2 -= 1; /* Even number of shifts required */
            }
            w32_enBGN = WEBRTC_SPL_SHIFT_W32(w32_enBGN, w16_tmp2);

            /* Calculate scale and shift factor */
            BGN_Inst->w16_scale = (WebRtc_Word16) WebRtcSpl_SqrtFloor(w32_enBGN);
            BGN_Inst->w16_scaleShift = 13 + ((6 + w16_tmp2) >> 1); /* RANDN table is in Q13, */
            /* 6=log2(64) */

            BGN_Inst->w16_initialized = 1;
        }

    }
    else
    {
        /*
         * Will only happen if post-decode VAD is disabled and w32_en is not low enough.
         * Increase the threshold for update so that it increases by a factor 4 in four
         * seconds.
         * energy = energy * 1.0035
         */
        w32_tmp = WEBRTC_SPL_MUL_16_16_RSFT(NETEQFIX_BGNFRAQINCQ16,
            BGN_Inst->w32_energyUpdateLow, 16);
        w32_tmp += WEBRTC_SPL_MUL_16_16(NETEQFIX_BGNFRAQINCQ16,
            (WebRtc_Word16)(BGN_Inst->w32_energyUpdate & 0xFF));
        w32_tmp += (WEBRTC_SPL_MUL_16_16(NETEQFIX_BGNFRAQINCQ16,
            (WebRtc_Word16)((BGN_Inst->w32_energyUpdate>>8) & 0xFF)) << 8);
        BGN_Inst->w32_energyUpdateLow += w32_tmp;

        BGN_Inst->w32_energyUpdate += WEBRTC_SPL_MUL_16_16(NETEQFIX_BGNFRAQINCQ16,
            (WebRtc_Word16)(BGN_Inst->w32_energyUpdate>>16));
        BGN_Inst->w32_energyUpdate += BGN_Inst->w32_energyUpdateLow >> 16;
        BGN_Inst->w32_energyUpdateLow = (BGN_Inst->w32_energyUpdateLow & 0x0FFFF);

        /* Update maximum energy */
        /* Decrease by a factor 1/1024 each time */
        BGN_Inst->w32_energyMax = BGN_Inst->w32_energyMax - (BGN_Inst->w32_energyMax >> 10);
        if (w32_en > BGN_Inst->w32_energyMax)
        {
            BGN_Inst->w32_energyMax = w32_en;
        }

        /* Set update level to at the minimum 60.21dB lower then the maximum energy */
        w32_enUpdateThreashold = (BGN_Inst->w32_energyMax + 524288) >> 20;
        if (w32_enUpdateThreashold > BGN_Inst->w32_energyUpdate)
        {
            BGN_Inst->w32_energyUpdate = w32_enUpdateThreashold;
        }
    }

#ifdef NETEQ_VAD
} /* closing initial if-statement */
#endif /* NETEQ_VAD */

    return;
}

#undef	 SCRATCH_PW32_AUTO_CORR
#undef	 SCRATCH_PW16_TEMP_VEC
#undef	 SCRATCH_PW16_RC
#undef	 SCRATCH_PW16_OUT_VEC

