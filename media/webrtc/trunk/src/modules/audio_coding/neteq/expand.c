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
 * This is the function to expand from the speech history, to produce concealment data or
 * increasing delay.
 */

#include "dsp.h"

#include <assert.h>

#include "signal_processing_library.h"

#include "dsp_helpfunctions.h"
#include "neteq_error_codes.h"

#define CHECK_NO_OF_CORRMAX        3
#define DISTLEN                    20
#define LPCANALASYSLEN           160

/* Scratch usage:

 Type            Name                    size            startpos        endpos
 (First part of first expand)
 WebRtc_Word16  pw16_bestCorrIndex      3               0               2
 WebRtc_Word16  pw16_bestCorr           3               3               5
 WebRtc_Word16  pw16_bestDistIndex      3               6               8
 WebRtc_Word16  pw16_bestDist           3               9               11
 WebRtc_Word16  pw16_corrVec            102*fs/8000     12              11+102*fs/8000
 func           WebRtcNetEQ_Correlator  232             12+102*fs/8000  243+102*fs/8000

 (Second part of first expand)
 WebRtc_Word32  pw32_corr2              99*fs/8000+1    0               99*fs/8000
 WebRtc_Word32  pw32_autoCorr           2*7             0               13
 WebRtc_Word16  pw16_rc                 6               14              19

 Signal combination:
 WebRtc_Word16  pw16_randVec            30+120*fs/8000  0               29+120*fs/8000
 WebRtc_Word16  pw16_scaledRandVec      125*fs/8000     30+120*fs/8000  29+245*fs/8000
 WebRtc_Word16  pw16_unvoicedVecSpace   10+125*fs/8000  30+245*fs/8000  39+370*fs/8000

 Total: 40+370*fs/8000 (size depends on UNVOICED_LPC_ORDER and BGN_LPC_ORDER)
 */

#if ((BGN_LPC_ORDER > 10) || (UNVOICED_LPC_ORDER > 10)) && (defined SCRATCH)
#error BGN_LPC_ORDER and/or BGN_LPC_ORDER are too large for current scratch memory allocation
#endif

#define     SCRATCH_PW16_BEST_CORR_INDEX    0
#define     SCRATCH_PW16_BEST_CORR          3
#define     SCRATCH_PW16_BEST_DIST_INDEX    6
#define     SCRATCH_PW16_BEST_DIST          9
#define     SCRATCH_PW16_CORR_VEC           12
#define     SCRATCH_PW16_CORR2              0
#define     SCRATCH_PW32_AUTO_CORR          0
#define     SCRATCH_PW16_RC                 14
#define     SCRATCH_PW16_RAND_VEC           0

#if (defined(NETEQ_48KHZ_WIDEBAND)) 
#define     SCRATCH_NETEQDSP_CORRELATOR     624
#define     SCRATCH_PW16_SCALED_RAND_VEC    750
#define     SCRATCH_PW16_UNVOICED_VEC_SPACE 1500
#elif (defined(NETEQ_32KHZ_WIDEBAND)) 
#define     SCRATCH_NETEQDSP_CORRELATOR     420
#define     SCRATCH_PW16_SCALED_RAND_VEC    510
#define     SCRATCH_PW16_UNVOICED_VEC_SPACE 1010
#elif (defined(NETEQ_WIDEBAND)) 
#define     SCRATCH_NETEQDSP_CORRELATOR     216
#define     SCRATCH_PW16_SCALED_RAND_VEC    270
#define     SCRATCH_PW16_UNVOICED_VEC_SPACE 520
#else    /* NB */
#define     SCRATCH_NETEQDSP_CORRELATOR     114
#define     SCRATCH_PW16_SCALED_RAND_VEC    150
#define     SCRATCH_PW16_UNVOICED_VEC_SPACE 275
#endif

/****************************************************************************
 * WebRtcNetEQ_Expand(...)
 *
 * This function produces one "chunk" of expansion data (PLC audio). The
 * length of the produced audio depends on the speech history.
 *
 * Input:
 *      - inst          : DSP instance
 *      - scratchPtr    : Pointer to scratch vector
 *      - outdata       : Pointer to a memory space where the output data
 *                        should be stored
 *      - BGNonly       : If non-zero, "expand" will only produce background noise.
 *      - pw16_len      : Desired number of samples (only for BGN mode).
 *
 * Output:
 *      - inst          : Updated instance
 *      - pw16_len      : Number of samples that were output from NetEq
 *
 * Return value         :  0 - Ok
 *                        <0 - Error
 */

int WebRtcNetEQ_Expand(DSPInst_t *inst,
#ifdef SCRATCH
                       WebRtc_Word16 *pw16_scratchPtr,
#endif
                       WebRtc_Word16 *pw16_outData, WebRtc_Word16 *pw16_len,
                       WebRtc_Word16 BGNonly)
{

    WebRtc_Word16 fs_mult;
    ExpandInst_t *ExpandState = &(inst->ExpandInst);
    BGNInst_t *BGNState = &(inst->BGNInst);
    int i;
#ifdef SCRATCH
    WebRtc_Word16 *pw16_randVec = pw16_scratchPtr + SCRATCH_PW16_RAND_VEC;
    WebRtc_Word16 *pw16_scaledRandVec = pw16_scratchPtr + SCRATCH_PW16_SCALED_RAND_VEC;
    WebRtc_Word16 *pw16_unvoicedVecSpace = pw16_scratchPtr + SCRATCH_PW16_UNVOICED_VEC_SPACE;
#else
    WebRtc_Word16 pw16_randVec[FSMULT * 120 + 30]; /* 150 for NB and 270 for WB */
    WebRtc_Word16 pw16_scaledRandVec[FSMULT * 125]; /* 125 for NB and 250 for WB */
    WebRtc_Word16 pw16_unvoicedVecSpace[BGN_LPC_ORDER + FSMULT * 125];
#endif
    /* 125 for NB and 250 for WB etc. Reuse pw16_outData[] for this vector */
    WebRtc_Word16 *pw16_voicedVecStorage = pw16_outData;
    WebRtc_Word16 *pw16_voicedVec = &pw16_voicedVecStorage[ExpandState->w16_overlap];
    WebRtc_Word16 *pw16_unvoicedVec = pw16_unvoicedVecSpace + UNVOICED_LPC_ORDER;
    WebRtc_Word16 *pw16_cngVec = pw16_unvoicedVecSpace + BGN_LPC_ORDER;
    WebRtc_Word16 w16_expVecsLen, w16_lag = 0, w16_expVecPos;
    WebRtc_Word16 w16_randLen;
    WebRtc_Word16 w16_vfractionChange; /* in Q14 */
    WebRtc_Word16 w16_winMute = 0, w16_winMuteInc = 0, w16_winUnMute = 0, w16_winUnMuteInc = 0;
    WebRtc_Word32 w32_tmp;
    WebRtc_Word16 w16_tmp, w16_tmp2;
    WebRtc_Word16 stability;
    enum BGNMode bgnMode = inst->BGNInst.bgnMode;

    /* Pre-calculate common multiplications with fs_mult */
    WebRtc_Word16 fsMult4;
    WebRtc_Word16 fsMult20;
    WebRtc_Word16 fsMult120;
    WebRtc_Word16 fsMultDistLen;
    WebRtc_Word16 fsMultLPCAnalasysLen;

#ifdef NETEQ_STEREO
    MasterSlaveInfo *msInfo = inst->msInfo;
#endif

    /* fs is WebRtc_UWord16 (to hold fs=48000) */
    fs_mult = WebRtcNetEQ_CalcFsMult(inst->fs); /* calculate fs/8000 */

    /* Pre-calculate common multiplications with fs_mult */
    fsMult4 = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16(fs_mult, 4);
    fsMult20 = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16(fs_mult, 20);
    fsMult120 = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16(fs_mult, 120);
    fsMultDistLen = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16(fs_mult, DISTLEN);
    fsMultLPCAnalasysLen = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16(fs_mult, LPCANALASYSLEN);

    /*
     * Perform all the initial setup if it's the first expansion.
     * If background noise (BGN) only, this setup is not needed.
     */
    if (ExpandState->w16_consecExp == 0 && !BGNonly)
    {
        /* Setup more variables */
#ifdef SCRATCH
        WebRtc_Word32 *pw32_autoCorr = (WebRtc_Word32*) (pw16_scratchPtr
            + SCRATCH_PW32_AUTO_CORR);
        WebRtc_Word16 *pw16_rc = pw16_scratchPtr + SCRATCH_PW16_RC;
        WebRtc_Word16 *pw16_bestCorrIndex = pw16_scratchPtr + SCRATCH_PW16_BEST_CORR_INDEX;
        WebRtc_Word16 *pw16_bestCorr = pw16_scratchPtr + SCRATCH_PW16_BEST_CORR;
        WebRtc_Word16 *pw16_bestDistIndex = pw16_scratchPtr + SCRATCH_PW16_BEST_DIST_INDEX;
        WebRtc_Word16 *pw16_bestDist = pw16_scratchPtr + SCRATCH_PW16_BEST_DIST;
        WebRtc_Word16 *pw16_corrVec = pw16_scratchPtr + SCRATCH_PW16_CORR_VEC;
        WebRtc_Word32 *pw32_corr2 = (WebRtc_Word32*) (pw16_scratchPtr + SCRATCH_PW16_CORR2);
#else
        WebRtc_Word32 pw32_autoCorr[UNVOICED_LPC_ORDER+1];
        WebRtc_Word16 pw16_rc[UNVOICED_LPC_ORDER];
        WebRtc_Word16 pw16_corrVec[FSMULT*102]; /* 102 for NB */
        WebRtc_Word16 pw16_bestCorrIndex[CHECK_NO_OF_CORRMAX];
        WebRtc_Word16 pw16_bestCorr[CHECK_NO_OF_CORRMAX];
        WebRtc_Word16 pw16_bestDistIndex[CHECK_NO_OF_CORRMAX];
        WebRtc_Word16 pw16_bestDist[CHECK_NO_OF_CORRMAX];
        WebRtc_Word32 pw32_corr2[(99*FSMULT)+1];
#endif
        WebRtc_Word32 pw32_bestDist[CHECK_NO_OF_CORRMAX];
        WebRtc_Word16 w16_ind = 0;
        WebRtc_Word16 w16_corrVecLen;
        WebRtc_Word16 w16_corrScale;
        WebRtc_Word16 w16_distScale;
        WebRtc_Word16 w16_indMin, w16_indMax;
        WebRtc_Word16 w16_len;
        WebRtc_Word32 w32_en1, w32_en2, w32_cc;
        WebRtc_Word16 w16_en1Scale, w16_en2Scale;
        WebRtc_Word16 w16_en1, w16_en2;
        WebRtc_Word32 w32_en1_mul_en2;
        WebRtc_Word16 w16_sqrt_en1en2;
        WebRtc_Word16 w16_ccShiftL;
        WebRtc_Word16 w16_bestcorr; /* Correlation in Q14 */
        WebRtc_Word16 *pw16_vec1, *pw16_vec2;
        WebRtc_Word16 w16_factor;
        WebRtc_Word16 w16_DistLag, w16_CorrLag, w16_diffLag;
        WebRtc_Word16 w16_energyLen;
        WebRtc_Word16 w16_slope;
        WebRtc_Word16 w16_startInd;
        WebRtc_Word16 w16_noOfcorr2;
        WebRtc_Word16 w16_scale;

        /* Initialize some variables */
        ExpandState->w16_lagsDirection = 1;
        ExpandState->w16_lagsPosition = -1;
        ExpandState->w16_expandMuteFactor = 16384; /* Start from 1.0 (Q14) */
        BGNState->w16_mutefactor = 0; /* Start with 0 gain for BGN (value in Q14) */
        inst->w16_seedInc = 1;

#ifdef NETEQ_STEREO
        /* Sanity for msInfo */
        if (msInfo == NULL)
        {
            /* this should not happen here */
            return MASTER_SLAVE_ERROR;
        }

        /*
         * Do not calculate correlations for slave instance(s)
         * unless lag info from master is corrupt
         */
        if ((msInfo->msMode != NETEQ_SLAVE)
        || ((msInfo->distLag <= 0) || (msInfo->corrLag <= 0)))
        {
#endif
            /* Calculate correlation vector in downsampled domain (4 kHz sample rate) */
            w16_corrVecLen = WebRtcNetEQ_Correlator(inst,
#ifdef SCRATCH
                pw16_scratchPtr + SCRATCH_NETEQDSP_CORRELATOR,
#endif
                inst->pw16_speechHistory, inst->w16_speechHistoryLen, pw16_corrVec,
                &w16_corrScale);

            /* Find peaks in correlation vector using parabolic fit method */
            WebRtcNetEQ_PeakDetection(pw16_corrVec, w16_corrVecLen, CHECK_NO_OF_CORRMAX, fs_mult,
                pw16_bestCorrIndex, pw16_bestCorr);

            /*
             * Adjust peak locations; cross-correlation lags start at 2.5 ms
             * (20*fs_mult samples)
             */
            pw16_bestCorrIndex[0] += fsMult20;
            pw16_bestCorrIndex[1] += fsMult20;
            pw16_bestCorrIndex[2] += fsMult20;

            /* Calculate distortion around the 3 (CHECK_NO_OF_CORRMAX) best lags */
            w16_distScale = 0;
            for (i = 0; i < CHECK_NO_OF_CORRMAX; i++)
            {
                w16_tmp = fsMult20;
                w16_tmp2 = pw16_bestCorrIndex[i] - fsMult4;
                w16_indMin = WEBRTC_SPL_MAX(w16_tmp, w16_tmp2);
                w16_tmp = fsMult120 - 1;
                w16_tmp2 = pw16_bestCorrIndex[i] + fsMult4;
                w16_indMax = WEBRTC_SPL_MIN(w16_tmp, w16_tmp2);

                pw16_bestDistIndex[i] = WebRtcNetEQ_MinDistortion(
                    &(inst->pw16_speechHistory[inst->w16_speechHistoryLen - fsMultDistLen]),
                    w16_indMin, w16_indMax, fsMultDistLen, &pw32_bestDist[i]);

                w16_distScale
                    = WEBRTC_SPL_MAX(16 - WebRtcSpl_NormW32(pw32_bestDist[i]), w16_distScale);

            }

            /* Shift the distortion values to fit in WebRtc_Word16 */
            WebRtcSpl_VectorBitShiftW32ToW16(pw16_bestDist, CHECK_NO_OF_CORRMAX, pw32_bestDist,
                w16_distScale);

            /*
             * Find index of maximum criteria, where crit[i] = bestCorr[i])/(bestDist[i])
             * Do this by a cross multiplication.
             */

            w32_en1 = WEBRTC_SPL_MUL_16_16((WebRtc_Word32) pw16_bestCorr[0],pw16_bestDist[1]);
            w32_en2 = WEBRTC_SPL_MUL_16_16((WebRtc_Word32) pw16_bestCorr[1],pw16_bestDist[0]);
            if (w32_en1 >= w32_en2)
            {
                /* 0 wins over 1 */
                w32_en1
                    = WEBRTC_SPL_MUL_16_16((WebRtc_Word32) pw16_bestCorr[0], pw16_bestDist[2]);
                w32_en2
                    = WEBRTC_SPL_MUL_16_16((WebRtc_Word32) pw16_bestCorr[2], pw16_bestDist[0]);
                if (w32_en1 >= w32_en2)
                {
                    /* 0 wins over 2 */
                    w16_ind = 0;
                }
                else
                {
                    /* 2 wins over 0 */
                    w16_ind = 2;
                }
            }
            else
            {
                /* 1 wins over 0 */
                w32_en1
                    = WEBRTC_SPL_MUL_16_16((WebRtc_Word32) pw16_bestCorr[1],pw16_bestDist[2]);
                w32_en2
                    = WEBRTC_SPL_MUL_16_16((WebRtc_Word32) pw16_bestCorr[2],pw16_bestDist[1]);
                if ((WebRtc_Word32) w32_en1 >= (WebRtc_Word32) w32_en2)
                {
                    /* 1 wins over 2 */
                    w16_ind = 1;
                }
                else
                {
                    /* 2 wins over 1 */
                    w16_ind = 2;
                }
            }

#ifdef NETEQ_STEREO
        }

        /* Store DistLag and CorrLag of the position with highest criteria */
        if ((msInfo->msMode == NETEQ_MASTER) || (msInfo->msMode == NETEQ_MONO)
            || ((msInfo->msMode == NETEQ_SLAVE) && (msInfo->distLag <= 0 || msInfo->corrLag
                <= 0)))
        {
            /* lags not provided externally */
            w16_DistLag = pw16_bestDistIndex[w16_ind];
            w16_CorrLag = pw16_bestCorrIndex[w16_ind];
            if (msInfo->msMode == NETEQ_MASTER)
            {
                msInfo->distLag = w16_DistLag;
                msInfo->corrLag = w16_CorrLag;
            }
        }
        else if (msInfo->msMode == NETEQ_SLAVE)
        {
            /* lags provided externally (from master) */
            w16_DistLag = msInfo->distLag;
            w16_CorrLag = msInfo->corrLag;

            /* sanity for lag values */
            if ((w16_DistLag <= 0) || (w16_CorrLag <= 0))
            {
                return MASTER_SLAVE_ERROR;
            }
        }
        else
        {
            /* Invalid mode */
            return MASTER_SLAVE_ERROR;
        }
#else /* not NETEQ_STEREO */
        w16_DistLag = pw16_bestDistIndex[w16_ind];
        w16_CorrLag = pw16_bestCorrIndex[w16_ind];
#endif

        ExpandState->w16_maxLag = WEBRTC_SPL_MAX(w16_DistLag, w16_CorrLag);

        /* Calculate the exact best correlation (in the range within CorrLag-DistLag) */
        w16_len = w16_DistLag + 10;
        w16_len = WEBRTC_SPL_MIN(w16_len, fsMult120);
        w16_len = WEBRTC_SPL_MAX(w16_len, 60 * fs_mult);

        w16_startInd = WEBRTC_SPL_MIN(w16_DistLag, w16_CorrLag);
        w16_noOfcorr2 = WEBRTC_SPL_ABS_W16((w16_DistLag-w16_CorrLag)) + 1;
        /* w16_noOfcorr2 maximum value is 99*fs_mult + 1 */

        /* Calculate suitable scaling */
        w16_tmp
            = WebRtcSpl_MaxAbsValueW16(
                &inst->pw16_speechHistory[inst->w16_speechHistoryLen - w16_len - w16_startInd
                    - w16_noOfcorr2],
                (WebRtc_Word16) (w16_len + w16_startInd + w16_noOfcorr2 - 1));
        w16_corrScale = ((31 - WebRtcSpl_NormW32(WEBRTC_SPL_MUL_16_16(w16_tmp, w16_tmp)))
            + (31 - WebRtcSpl_NormW32(w16_len))) - 31;
        w16_corrScale = WEBRTC_SPL_MAX(0, w16_corrScale);

        /*
         * Perform the correlation, store in pw32_corr2
         */

        WebRtcNetEQ_CrossCorr(pw32_corr2,
            &(inst->pw16_speechHistory[inst->w16_speechHistoryLen - w16_len]),
            &(inst->pw16_speechHistory[inst->w16_speechHistoryLen - w16_len - w16_startInd]),
            w16_len, w16_noOfcorr2, w16_corrScale, -1);

        /* Find maximizing index */
        w16_ind = WebRtcSpl_MaxIndexW32(pw32_corr2, w16_noOfcorr2);
        w32_cc = pw32_corr2[w16_ind]; /* this is maximum correlation */
        w16_ind = w16_ind + w16_startInd; /* correct index for start offset */

        /* Calculate energies */
        w32_en1 = WebRtcNetEQ_DotW16W16(
            &(inst->pw16_speechHistory[inst->w16_speechHistoryLen - w16_len]),
            &(inst->pw16_speechHistory[inst->w16_speechHistoryLen - w16_len]), w16_len,
            w16_corrScale);
        w32_en2 = WebRtcNetEQ_DotW16W16(
            &(inst->pw16_speechHistory[inst->w16_speechHistoryLen - w16_len - w16_ind]),
            &(inst->pw16_speechHistory[inst->w16_speechHistoryLen - w16_len - w16_ind]),
            w16_len, w16_corrScale);

        /* Calculate the correlation value w16_bestcorr */
        if ((w32_en1 > 0) && (w32_en2 > 0))
        {
            w16_en1Scale = 16 - WebRtcSpl_NormW32(w32_en1);
            w16_en1Scale = WEBRTC_SPL_MAX(0, w16_en1Scale);
            w16_en2Scale = 16 - WebRtcSpl_NormW32(w32_en2);
            w16_en2Scale = WEBRTC_SPL_MAX(0, w16_en2Scale);
            /* Make sure total scaling is even (to simplify scale factor after sqrt) */
            if ((w16_en1Scale + w16_en2Scale) & 1)
            {
                /* if sum is odd */
                w16_en1Scale += 1;
            }
            w16_en1 = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(w32_en1, w16_en1Scale);
            w16_en2 = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(w32_en2, w16_en2Scale);
            w32_en1_mul_en2 = WEBRTC_SPL_MUL_16_16(w16_en1, w16_en2);
            w16_sqrt_en1en2 = (WebRtc_Word16) WebRtcSpl_SqrtFloor(w32_en1_mul_en2);

            /* Calculate cc/sqrt(en1*en2) in Q14 */
            w16_ccShiftL = 14 - ((w16_en1Scale + w16_en2Scale) >> 1);
            w32_cc = WEBRTC_SPL_SHIFT_W32(w32_cc, w16_ccShiftL);
            w16_bestcorr = (WebRtc_Word16) WebRtcSpl_DivW32W16(w32_cc, w16_sqrt_en1en2);
            w16_bestcorr = WEBRTC_SPL_MIN(16384, w16_bestcorr); /* set maximum to 1.0 */

        }
        else
        {
            /* if either en1 or en2 is zero */
            w16_bestcorr = 0;
        }

        /*
         * Extract the two vectors, pw16_expVecs[0][] and pw16_expVecs[1][],
         * from the SpeechHistory[]
         */
        w16_expVecsLen = ExpandState->w16_maxLag + ExpandState->w16_overlap;
        pw16_vec1 = &(inst->pw16_speechHistory[inst->w16_speechHistoryLen - w16_expVecsLen]);
        pw16_vec2 = pw16_vec1 - w16_DistLag;
        /* Normalize the second vector to the same energy as the first */
        w32_en1 = WebRtcNetEQ_DotW16W16(pw16_vec1, pw16_vec1, w16_expVecsLen, w16_corrScale);
        w32_en2 = WebRtcNetEQ_DotW16W16(pw16_vec2, pw16_vec2, w16_expVecsLen, w16_corrScale);

        /*
         * Confirm that energy factor sqrt(w32_en1/w32_en2) is within difference 0.5 - 2.0
         * w32_en1/w32_en2 within 0.25 - 4
         */
        if (((w32_en1 >> 2) < w32_en2) && ((w32_en1) > (w32_en2 >> 2)))
        {

            /* Energy constraint fulfilled => use both vectors and scale them accordingly */
            w16_en2Scale = 16 - WebRtcSpl_NormW32(w32_en2);
            w16_en2Scale = WEBRTC_SPL_MAX(0, w16_en2Scale);
            w16_en1Scale = w16_en2Scale - 13;

            /* calculate w32_en1/w32_en2 in Q13 */
            w32_en1_mul_en2 = WebRtcSpl_DivW32W16(
                WEBRTC_SPL_SHIFT_W32(w32_en1, -w16_en1Scale),
                (WebRtc_Word16) (WEBRTC_SPL_RSHIFT_W32(w32_en2, w16_en2Scale)));

            /* calculate factor in Q13 (sqrt of en1/en2 in Q26) */
            w16_factor = (WebRtc_Word16) WebRtcSpl_SqrtFloor(
                WEBRTC_SPL_LSHIFT_W32(w32_en1_mul_en2, 13));

            /* Copy the two vectors and give them the same energy */

            WEBRTC_SPL_MEMCPY_W16(ExpandState->pw16_expVecs[0], pw16_vec1, w16_expVecsLen);
            WebRtcSpl_AffineTransformVector(ExpandState->pw16_expVecs[1], pw16_vec2,
                w16_factor, 4096, 13, w16_expVecsLen);

        }
        else
        {
            /* Energy change constraint not fulfilled => only use last vector */

            WEBRTC_SPL_MEMCPY_W16(ExpandState->pw16_expVecs[0], pw16_vec1, w16_expVecsLen);
            WEBRTC_SPL_MEMCPY_W16(ExpandState->pw16_expVecs[1], ExpandState->pw16_expVecs[0],
                w16_expVecsLen);

            /* Set the w16_factor since it is used by muting slope */
            if (((w32_en1 >> 2) < w32_en2) || (w32_en2 == 0))
            {
                w16_factor = 4096; /* 0.5 in Q13*/
            }
            else
            {
                w16_factor = 16384; /* 2.0 in Q13*/
            }
        }

        /* Set the 3 lag values */
        w16_diffLag = w16_DistLag - w16_CorrLag;
        if (w16_diffLag == 0)
        {
            /* DistLag and CorrLag are equal */
            ExpandState->w16_lags[0] = w16_DistLag;
            ExpandState->w16_lags[1] = w16_DistLag;
            ExpandState->w16_lags[2] = w16_DistLag;
        }
        else
        {
            /* DistLag and CorrLag are not equal; use different combinations of the two */
            ExpandState->w16_lags[0] = w16_DistLag; /* DistLag only */
            ExpandState->w16_lags[1] = ((w16_DistLag + w16_CorrLag) >> 1); /* 50/50 */
            /* Third lag, move one half-step towards CorrLag (in both cases) */
            if (w16_diffLag > 0)
            {
                ExpandState->w16_lags[2] = (w16_DistLag + w16_CorrLag - 1) >> 1;
            }
            else
            {
                ExpandState->w16_lags[2] = (w16_DistLag + w16_CorrLag + 1) >> 1;
            }
        }

        /*************************************************
         * Calculate the LPC and the gain of the filters *
         *************************************************/

        /* Calculate scale value needed for autocorrelation */
        w16_tmp = WebRtcSpl_MaxAbsValueW16(
            &(inst->pw16_speechHistory[inst->w16_speechHistoryLen - fsMultLPCAnalasysLen]),
            fsMultLPCAnalasysLen);

        w16_tmp = 16 - WebRtcSpl_NormW32(w16_tmp);
        w16_tmp = WEBRTC_SPL_MIN(w16_tmp,0);
        w16_tmp = (w16_tmp << 1) + 7;
        w16_tmp = WEBRTC_SPL_MAX(w16_tmp,0);

        /* set w16_ind to simplify the following expressions */
        w16_ind = inst->w16_speechHistoryLen - fsMultLPCAnalasysLen - UNVOICED_LPC_ORDER;

        /* store first UNVOICED_LPC_ORDER samples in pw16_rc */

        WEBRTC_SPL_MEMCPY_W16(pw16_rc, &inst->pw16_speechHistory[w16_ind], UNVOICED_LPC_ORDER);

        /* set first samples to zero */
        WebRtcSpl_MemSetW16(&inst->pw16_speechHistory[w16_ind], 0, UNVOICED_LPC_ORDER);

        /* Calculate UNVOICED_LPC_ORDER+1 lags of the ACF */

        WebRtcNetEQ_CrossCorr(
            pw32_autoCorr, &(inst->pw16_speechHistory[w16_ind + UNVOICED_LPC_ORDER]),
            &(inst->pw16_speechHistory[w16_ind + UNVOICED_LPC_ORDER]), fsMultLPCAnalasysLen,
            UNVOICED_LPC_ORDER + 1, w16_tmp, -1);

        /* Recover the stored samples from pw16_rc */

        WEBRTC_SPL_MEMCPY_W16(&inst->pw16_speechHistory[w16_ind], pw16_rc, UNVOICED_LPC_ORDER);

        if (pw32_autoCorr[0] > 0)
        { /* check that variance is positive */

            /* estimate AR filter parameters using Levinson-Durbin algorithm
             (UNVOICED_LPC_ORDER+1 filter coefficients) */
            stability = WebRtcSpl_LevinsonDurbin(pw32_autoCorr, ExpandState->pw16_arFilter,
                pw16_rc, UNVOICED_LPC_ORDER);

            /* Only update BGN if filter is stable */
            if (stability != 1)
            {
                /* Set first coefficient to 4096 (1.0 in Q12)*/
                ExpandState->pw16_arFilter[0] = 4096;
                /* Set remaining UNVOICED_LPC_ORDER coefficients to zero */
                WebRtcSpl_MemSetW16(ExpandState->pw16_arFilter + 1, 0, UNVOICED_LPC_ORDER);
            }

        }

        if (w16_DistLag < 40)
        {
            w16_energyLen = 2 * w16_DistLag;
        }
        else
        {
            w16_energyLen = w16_DistLag;
        }
        w16_randLen = w16_energyLen + 30; /* Startup part */

        /* Extract a noise segment */
        if (w16_randLen <= RANDVEC_NO_OF_SAMPLES)
        {
            WEBRTC_SPL_MEMCPY_W16(pw16_randVec,
                (WebRtc_Word16*) WebRtcNetEQ_kRandnTbl, w16_randLen);
        }
        else
        { /* only applies to SWB where length could be larger than 256 */
#if FSMULT >= 2  /* Makes pw16_randVec longer than RANDVEC_NO_OF_SAMPLES. */
            WEBRTC_SPL_MEMCPY_W16(pw16_randVec, (WebRtc_Word16*) WebRtcNetEQ_kRandnTbl,
                RANDVEC_NO_OF_SAMPLES);
            inst->w16_seedInc = (inst->w16_seedInc + 2) & (RANDVEC_NO_OF_SAMPLES - 1);
            assert(w16_randLen <= FSMULT * 120 + 30);
            WebRtcNetEQ_RandomVec(&inst->uw16_seed, &pw16_randVec[RANDVEC_NO_OF_SAMPLES],
                (WebRtc_Word16) (w16_randLen - RANDVEC_NO_OF_SAMPLES), inst->w16_seedInc);
#else
            assert(0);
#endif
        }

        /* Set up state vector and calculate scale factor for unvoiced filtering */

        WEBRTC_SPL_MEMCPY_W16(ExpandState->pw16_arState,
            &(inst->pw16_speechHistory[inst->w16_speechHistoryLen - UNVOICED_LPC_ORDER]),
            UNVOICED_LPC_ORDER);
        WEBRTC_SPL_MEMCPY_W16(pw16_unvoicedVec - UNVOICED_LPC_ORDER,
            &(inst->pw16_speechHistory[inst->w16_speechHistoryLen - 128 - UNVOICED_LPC_ORDER]),
            UNVOICED_LPC_ORDER);
        WebRtcSpl_FilterMAFastQ12(&inst->pw16_speechHistory[inst->w16_speechHistoryLen - 128],
            pw16_unvoicedVec, ExpandState->pw16_arFilter, UNVOICED_LPC_ORDER + 1, 128);
        if (WebRtcSpl_MaxAbsValueW16(pw16_unvoicedVec, 128) > 4000)
        {
            w16_scale = 4;
        }
        else
        {
            w16_scale = 0;
        }
        w32_tmp = WebRtcNetEQ_DotW16W16(pw16_unvoicedVec, pw16_unvoicedVec, 128, w16_scale);

        /* Normalize w32_tmp to 28 or 29 bits to preserve sqrt() accuracy */
        w16_tmp = WebRtcSpl_NormW32(w32_tmp) - 3;
        w16_tmp += ((w16_tmp & 0x1) ^ 0x1); /* Make sure we do an odd number of shifts since we
         from earlier have 7 shifts from dividing with 128.*/
        w32_tmp = WEBRTC_SPL_SHIFT_W32(w32_tmp, w16_tmp);
        w32_tmp = WebRtcSpl_SqrtFloor(w32_tmp);
        ExpandState->w16_arGainScale = 13 + ((w16_tmp + 7 - w16_scale) >> 1);
        ExpandState->w16_arGain = (WebRtc_Word16) w32_tmp;

        /********************************************************************
         * Calculate vfraction from bestcorr                                *
         * if (bestcorr>0.480665)                                           *
         *     vfraction = ((bestcorr-0.4)/(1-0.4)).^2                      *
         * else    vfraction = 0                                            *
         *                                                                  *
         * approximation (coefficients in Q12):                             *
         * if (x>0.480665)    (y(x)<0.3)                                    *
         *   y(x) = -1.264421 + 4.8659148*x - 4.0092827*x^2 + 1.4100529*x^3 *
         * else y(x) = 0;                                                   *
         ********************************************************************/

        if (w16_bestcorr > 7875)
        {
            /* if x>0.480665 */
            WebRtc_Word16 w16_x1, w16_x2, w16_x3;
            w16_x1 = w16_bestcorr;
            w32_tmp = WEBRTC_SPL_MUL_16_16((WebRtc_Word32) w16_x1, w16_x1);
            w16_x2 = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(w32_tmp, 14);
            w32_tmp = WEBRTC_SPL_MUL_16_16(w16_x1, w16_x2);
            w16_x3 = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(w32_tmp, 14);
            w32_tmp
                = (WebRtc_Word32) WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32) WebRtcNetEQ_kMixFractionFuncTbl[0], 14);
            w32_tmp
                += (WebRtc_Word32) WEBRTC_SPL_MUL_16_16(WebRtcNetEQ_kMixFractionFuncTbl[1], w16_x1);
            w32_tmp
                += (WebRtc_Word32) WEBRTC_SPL_MUL_16_16(WebRtcNetEQ_kMixFractionFuncTbl[2], w16_x2);
            w32_tmp
                += (WebRtc_Word32) WEBRTC_SPL_MUL_16_16(WebRtcNetEQ_kMixFractionFuncTbl[3], w16_x3);
            ExpandState->w16_vFraction = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(w32_tmp, 12);
            ExpandState->w16_vFraction = WEBRTC_SPL_MIN(ExpandState->w16_vFraction, 16384);
            ExpandState->w16_vFraction = WEBRTC_SPL_MAX(ExpandState->w16_vFraction, 0);
        }
        else
        {
            ExpandState->w16_vFraction = 0;
        }

        /***********************************************************************
         * Calculate muting slope, reuse value from earlier scaling of ExpVecs *
         ***********************************************************************/
        w16_slope = w16_factor;

        if (w16_slope > 12288)
        {
            /* w16_slope > 1.5 ? */
            /* Calculate (1-(1/slope))/w16_DistLag = (slope-1)/(w16_DistLag*slope) */
            w32_tmp = w16_slope - 8192;
            w32_tmp = WEBRTC_SPL_LSHIFT_W32(w32_tmp, 12); /* Value in Q25 (13+12=25) */
            w16_tmp = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16_RSFT(w16_DistLag,
                w16_slope, 8); /* Value in Q5  (13-8=5)  */
            w16_tmp = (WebRtc_Word16) WebRtcSpl_DivW32W16(w32_tmp,
                w16_tmp); /* Res in Q20 (25-5=20) */

            if (w16_slope > 14746)
            { /* w16_slope > 1.8 ? */
                ExpandState->w16_muteSlope = (w16_tmp + 1) >> 1;
            }
            else
            {
                ExpandState->w16_muteSlope = (w16_tmp + 4) >> 3;
            }
            ExpandState->w16_onset = 1;
        }
        else if (ExpandState->w16_vFraction > 13107)
        {
            /* w16_vFraction > 0.8 ? */
            if (w16_slope > 8028)
            {
                /* w16_vFraction > 0.98 ? */
                ExpandState->w16_muteSlope = 0;
            }
            else
            {
                /* Calculate (1-slope)/w16_DistLag */
                w32_tmp = 8192 - w16_slope;
                w32_tmp = WEBRTC_SPL_LSHIFT_W32(w32_tmp, 7); /* Value in Q20 (13+7=20) */
                ExpandState->w16_muteSlope = (WebRtc_Word16) WebRtcSpl_DivW32W16(w32_tmp,
                    w16_DistLag); /* Res in Q20 (20-0=20) */
            }
            ExpandState->w16_onset = 0;
        }
        else
        {
            /*
             * Use the minimum of 0.005 (0.9 on 50 samples in NB and the slope)
             * and ((1-slope)/w16_DistLag)
             */
            w32_tmp = 8192 - w16_slope;
            w32_tmp = WEBRTC_SPL_LSHIFT_W32(w32_tmp, 7); /* Value in Q20 (13+7=20) */
            w32_tmp = WEBRTC_SPL_MAX(w32_tmp, 0);
            ExpandState->w16_muteSlope = (WebRtc_Word16) WebRtcSpl_DivW32W16(w32_tmp,
                w16_DistLag); /* Res   in Q20    (20-0=20) */
            w16_tmp = WebRtcNetEQ_k5243div[fs_mult]; /* 0.005/fs_mult = 5243/fs_mult */
            ExpandState->w16_muteSlope = WEBRTC_SPL_MAX(w16_tmp, ExpandState->w16_muteSlope);
            ExpandState->w16_onset = 0;
        }
    }
    else
    {
        /* This is not the first Expansion, parameters are already estimated. */

        /* Extract a noise segment */
        if (BGNonly) /* If we should produce nothing but background noise */
        {
            if (*pw16_len > 0)
            {
                /*
                 * Set length to input parameter length, but not more than length
                 * of pw16_randVec
                 */
                w16_lag = WEBRTC_SPL_MIN(*pw16_len, FSMULT * 120 + 30);
            }
            else
            {
                /* set length to 15 ms */
                w16_lag = fsMult120;
            }
            w16_randLen = w16_lag;
        }
        else
        {
            w16_randLen = ExpandState->w16_maxLag;
        }

        if (w16_randLen <= RANDVEC_NO_OF_SAMPLES)
        {
            inst->w16_seedInc = (inst->w16_seedInc + 2) & (RANDVEC_NO_OF_SAMPLES - 1);
            WebRtcNetEQ_RandomVec(&inst->uw16_seed, pw16_randVec, w16_randLen,
                inst->w16_seedInc);
        }
        else
        { /* only applies to SWB where length could be larger than 256 */
#if FSMULT >= 2  /* Makes pw16_randVec longer than RANDVEC_NO_OF_SAMPLES. */
            inst->w16_seedInc = (inst->w16_seedInc + 2) & (RANDVEC_NO_OF_SAMPLES - 1);
            WebRtcNetEQ_RandomVec(&inst->uw16_seed, pw16_randVec, RANDVEC_NO_OF_SAMPLES,
                inst->w16_seedInc);
            inst->w16_seedInc = (inst->w16_seedInc + 2) & (RANDVEC_NO_OF_SAMPLES - 1);
            assert(w16_randLen <= FSMULT * 120 + 30);
            WebRtcNetEQ_RandomVec(&inst->uw16_seed, &pw16_randVec[RANDVEC_NO_OF_SAMPLES],
                (WebRtc_Word16) (w16_randLen - RANDVEC_NO_OF_SAMPLES), inst->w16_seedInc);
#else
            assert(0);
#endif
        }
    } /* end if(first expand or BGNonly) ... else ... */

    if (!BGNonly) /* Voiced and unvoiced parts not used if generating BGN only */
    {

        /*************************************************
         * Generate signal                               *
         *************************************************/

        /*
         * Voiced part
         */

        /* Linearly mute the use_vfraction value from 1 to vfraction */
        if (ExpandState->w16_consecExp == 0)
        {
            ExpandState->w16_currentVFraction = 16384; /* 1.0 in Q14 */
        }

        ExpandState->w16_lagsPosition = ExpandState->w16_lagsPosition
            + ExpandState->w16_lagsDirection;

        /* Change direction if needed */
        if (ExpandState->w16_lagsPosition == 0)
        {
            ExpandState->w16_lagsDirection = 1;
        }
        if (ExpandState->w16_lagsPosition == 2)
        {
            ExpandState->w16_lagsDirection = -1;
        }

        /* Generate a weighted vector with the selected lag */
        w16_expVecsLen = ExpandState->w16_maxLag + ExpandState->w16_overlap;
        w16_lag = ExpandState->w16_lags[ExpandState->w16_lagsPosition];
        /* Copy lag+overlap data */
        w16_expVecPos = w16_expVecsLen - w16_lag - ExpandState->w16_overlap;
        w16_tmp = w16_lag + ExpandState->w16_overlap;
        if (ExpandState->w16_lagsPosition == 0)
        {
            WEBRTC_SPL_MEMCPY_W16(pw16_voicedVecStorage,
                &(ExpandState->pw16_expVecs[0][w16_expVecPos]), w16_tmp);
        }
        else if (ExpandState->w16_lagsPosition == 1)
        {
            WebRtcSpl_ScaleAndAddVectorsWithRound(&ExpandState->pw16_expVecs[0][w16_expVecPos], 3,
                &ExpandState->pw16_expVecs[1][w16_expVecPos], 1, 2, pw16_voicedVecStorage,
                w16_tmp);

        }
        else if (ExpandState->w16_lagsPosition == 2)
        {
            WebRtcSpl_ScaleAndAddVectorsWithRound(&ExpandState->pw16_expVecs[0][w16_expVecPos], 1,
                &ExpandState->pw16_expVecs[1][w16_expVecPos], 1, 1, pw16_voicedVecStorage,
                w16_tmp);
        }

        if (inst->fs == 8000)
        {
            /* Windowing in Q15 */
            w16_winMute = NETEQ_OVERLAP_WINMUTE_8KHZ_START;
            w16_winMuteInc = NETEQ_OVERLAP_WINMUTE_8KHZ_INC;
            w16_winUnMute = NETEQ_OVERLAP_WINUNMUTE_8KHZ_START;
            w16_winUnMuteInc = NETEQ_OVERLAP_WINUNMUTE_8KHZ_INC;
#ifdef NETEQ_WIDEBAND
        }
        else if (inst->fs == 16000)
        {
            /* Windowing in Q15 */
            w16_winMute = NETEQ_OVERLAP_WINMUTE_16KHZ_START;
            w16_winMuteInc = NETEQ_OVERLAP_WINMUTE_16KHZ_INC;
            w16_winUnMute = NETEQ_OVERLAP_WINUNMUTE_16KHZ_START;
            w16_winUnMuteInc = NETEQ_OVERLAP_WINUNMUTE_16KHZ_INC;
#endif
#ifdef NETEQ_32KHZ_WIDEBAND
        }
        else if (inst->fs == 32000)
        {
            /* Windowing in Q15 */
            w16_winMute = NETEQ_OVERLAP_WINMUTE_32KHZ_START;
            w16_winMuteInc = NETEQ_OVERLAP_WINMUTE_32KHZ_INC;
            w16_winUnMute = NETEQ_OVERLAP_WINUNMUTE_32KHZ_START;
            w16_winUnMuteInc = NETEQ_OVERLAP_WINUNMUTE_32KHZ_INC;
#endif
#ifdef NETEQ_48KHZ_WIDEBAND
        }
        else /* if (inst->fs==48000) */
        {
            /* Windowing in Q15 */
            w16_winMute = NETEQ_OVERLAP_WINMUTE_48KHZ_START;
            w16_winMuteInc = NETEQ_OVERLAP_WINMUTE_48KHZ_INC;
            w16_winUnMute = NETEQ_OVERLAP_WINUNMUTE_48KHZ_START;
            w16_winUnMuteInc = NETEQ_OVERLAP_WINUNMUTE_48KHZ_INC;
#endif
        }

        /* Smooth the expanded if it has not been muted to or vfraction is larger than 0.5 */
        if ((ExpandState->w16_expandMuteFactor > 819) && (ExpandState->w16_currentVFraction
            > 8192))
        {
            for (i = 0; i < ExpandState->w16_overlap; i++)
            {
                /* Do overlap add between new vector and overlap */
                ExpandState->pw16_overlapVec[i] = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(
                    WEBRTC_SPL_MUL_16_16(ExpandState->pw16_overlapVec[i], w16_winMute) +
                    WEBRTC_SPL_MUL_16_16(
                        WEBRTC_SPL_MUL_16_16_RSFT(ExpandState->w16_expandMuteFactor,
                            pw16_voicedVecStorage[i], 14), w16_winUnMute) + 16384, 15);
                w16_winMute += w16_winMuteInc;
                w16_winUnMute += w16_winUnMuteInc;
            }
        }
        else if (ExpandState->w16_expandMuteFactor == 0
#ifdef NETEQ_STEREO
            && msInfo->msMode == NETEQ_MONO /* only if mono mode is selected */
#endif
        )
        {
            /* if ExpandState->w16_expandMuteFactor = 0 => all is CNG component 
             set the output length to 15ms (for best CNG production) */
            w16_tmp = fsMult120;
            ExpandState->w16_maxLag = w16_tmp;
            ExpandState->w16_lags[0] = w16_tmp;
            ExpandState->w16_lags[1] = w16_tmp;
            ExpandState->w16_lags[2] = w16_tmp;
        }

        /*
         * Unvoiced part
         */

        WEBRTC_SPL_MEMCPY_W16(pw16_unvoicedVec - UNVOICED_LPC_ORDER,
            ExpandState->pw16_arState,
            UNVOICED_LPC_ORDER);
        if (ExpandState->w16_arGainScale > 0)
        {
            w32_tmp = ((WebRtc_Word32) 1) << (ExpandState->w16_arGainScale - 1);
        }
        else
        {
            w32_tmp = 0;
        }

        /* Note that shift value can be >16 which complicates things for some DSPs */
        WebRtcSpl_AffineTransformVector(pw16_scaledRandVec, pw16_randVec,
            ExpandState->w16_arGain, w32_tmp, ExpandState->w16_arGainScale, w16_lag);

        WebRtcSpl_FilterARFastQ12(pw16_scaledRandVec, pw16_unvoicedVec,
            ExpandState->pw16_arFilter, UNVOICED_LPC_ORDER + 1, w16_lag);

        WEBRTC_SPL_MEMCPY_W16(ExpandState->pw16_arState,
            &(pw16_unvoicedVec[w16_lag - UNVOICED_LPC_ORDER]),
            UNVOICED_LPC_ORDER);

        /*
         * Voiced + Unvoiced
         */

        /* For lag = 
         <=31*fs_mult         => go from 1 to 0 in about 8 ms
         (>=31..<=63)*fs_mult => go from 1 to 0 in about 16 ms
         >=64*fs_mult         => go from 1 to 0 in about 32 ms
         */
        w16_tmp = (31 - WebRtcSpl_NormW32(ExpandState->w16_maxLag)) - 5; /* getbits(w16_maxLag) -5 */
        w16_vfractionChange = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(256, w16_tmp);
        if (ExpandState->w16_stopMuting == 1)
        {
            w16_vfractionChange = 0;
        }

        /* Create combined signal (unmuted) by shifting in more and more of unvoiced part */
        w16_tmp = 8 - w16_tmp; /* getbits(w16_vfractionChange) */
        w16_tmp = (ExpandState->w16_currentVFraction - ExpandState->w16_vFraction) >> w16_tmp;
        w16_tmp = WEBRTC_SPL_MIN(w16_tmp, w16_lag);
        WebRtcNetEQ_MixVoiceUnvoice(pw16_outData, pw16_voicedVec, pw16_unvoicedVec,
            &ExpandState->w16_currentVFraction, w16_vfractionChange, w16_tmp);

        if (w16_tmp < w16_lag)
        {
            if (w16_vfractionChange != 0)
            {
                ExpandState->w16_currentVFraction = ExpandState->w16_vFraction;
            }
            w16_tmp2 = 16384 - ExpandState->w16_currentVFraction;
            WebRtcSpl_ScaleAndAddVectorsWithRound(pw16_voicedVec + w16_tmp,
                ExpandState->w16_currentVFraction, pw16_unvoicedVec + w16_tmp, w16_tmp2, 14,
                pw16_outData + w16_tmp, (WebRtc_Word16) (w16_lag - w16_tmp));
        }

        /* Select muting factor */
        if (ExpandState->w16_consecExp == 3)
        {
            /* 0.95 on 50 samples in NB (0.0010/fs_mult in Q20) */
            ExpandState->w16_muteSlope = WEBRTC_SPL_MAX(ExpandState->w16_muteSlope,
                WebRtcNetEQ_k1049div[fs_mult]);
        }
        if (ExpandState->w16_consecExp == 7)
        {
            /* 0.90 on 50 samples in NB (0.0020/fs_mult in Q20) */
            ExpandState->w16_muteSlope = WEBRTC_SPL_MAX(ExpandState->w16_muteSlope,
                WebRtcNetEQ_k2097div[fs_mult]);
        }

        /* Mute segment according to slope value */
        if ((ExpandState->w16_consecExp != 0) || (ExpandState->w16_onset != 1))
        {
            /* Mute to the previous level, then continue with the muting */
            WebRtcSpl_AffineTransformVector(pw16_outData, pw16_outData,
                ExpandState->w16_expandMuteFactor, 8192, 14, w16_lag);

            if ((ExpandState->w16_stopMuting != 1))
            {
                WebRtcNetEQ_MuteSignal(pw16_outData, ExpandState->w16_muteSlope, w16_lag);

                w16_tmp = 16384 - (WebRtc_Word16) ((WEBRTC_SPL_MUL_16_16(w16_lag,
                    ExpandState->w16_muteSlope) + 8192) >> 6); /* 20-14 = 6 */
                w16_tmp = (WebRtc_Word16) ((WEBRTC_SPL_MUL_16_16(w16_tmp,
                    ExpandState->w16_expandMuteFactor) + 8192) >> 14);

                /* Guard against getting stuck with very small (but sometimes audible) gain */
                if ((ExpandState->w16_consecExp > 3) && (w16_tmp
                    >= ExpandState->w16_expandMuteFactor))
                {
                    ExpandState->w16_expandMuteFactor = 0;
                }
                else
                {
                    ExpandState->w16_expandMuteFactor = w16_tmp;
                }
            }
        }

    } /* end if(!BGNonly) */

    /*
     * BGN
     */

    if (BGNState->w16_initialized == 1)
    {
        /* BGN parameters are initialized; use them */

        WEBRTC_SPL_MEMCPY_W16(pw16_cngVec - BGN_LPC_ORDER,
            BGNState->pw16_filterState,
            BGN_LPC_ORDER);

        if (BGNState->w16_scaleShift > 1)
        {
            w32_tmp = ((WebRtc_Word32) 1) << (BGNState->w16_scaleShift - 1);
        }
        else
        {
            w32_tmp = 0;
        }

        /* Scale random vector to correct energy level */
        /* Note that shift value can be >16 which complicates things for some DSPs */
        WebRtcSpl_AffineTransformVector(pw16_scaledRandVec, pw16_randVec,
            BGNState->w16_scale, w32_tmp, BGNState->w16_scaleShift, w16_lag);

        WebRtcSpl_FilterARFastQ12(pw16_scaledRandVec, pw16_cngVec, BGNState->pw16_filter,
            BGN_LPC_ORDER + 1, w16_lag);

        WEBRTC_SPL_MEMCPY_W16(BGNState->pw16_filterState,
            &(pw16_cngVec[w16_lag-BGN_LPC_ORDER]),
            BGN_LPC_ORDER);

        /* Unmute the insertion of background noise */

        if (bgnMode == BGN_FADE && ExpandState->w16_consecExp >= FADE_BGN_TIME
            && BGNState->w16_mutefactor > 0)
        {
            /* fade BGN to zero */
            /* calculate muting slope, approx 2^18/fsHz */
            WebRtc_Word16 muteFactor;
            if (fs_mult == 1)
            {
                muteFactor = -32;
            }
            else if (fs_mult == 2)
            {
                muteFactor = -16;
            }
            else if (fs_mult == 4)
            {
                muteFactor = -8;
            }
            else
            {
                muteFactor = -5;
            }
            /* use UnmuteSignal function with negative slope */
            WebRtcNetEQ_UnmuteSignal(pw16_cngVec, &BGNState->w16_mutefactor, /* In Q14 */
            pw16_cngVec, muteFactor, /* In Q20 */
            w16_lag);
        }
        else if (BGNState->w16_mutefactor < 16384 && !BGNonly)
        {
            /* if (w16_mutefactor < 1)  and not BGN only (since then we use no muting) */

            /*
             * If BGN_OFF, or if BNG_FADE has started fading,
             * mutefactor should not be increased.
             */
            if (ExpandState->w16_stopMuting != 1 && bgnMode != BGN_OFF && !(bgnMode
                == BGN_FADE && ExpandState->w16_consecExp >= FADE_BGN_TIME))
            {
                WebRtcNetEQ_UnmuteSignal(pw16_cngVec, &BGNState->w16_mutefactor, /* In Q14 */
                pw16_cngVec, ExpandState->w16_muteSlope, /* In Q20 */
                w16_lag);
            }
            else
            {
                /* BGN_ON and stop muting, or
                 * BGN_OFF (mute factor is always 0), or
                 * BGN_FADE has reached 0 */
                WebRtcSpl_AffineTransformVector(pw16_cngVec, pw16_cngVec,
                    BGNState->w16_mutefactor, 8192, 14, w16_lag);
            }
        }
    }
    else
    {
        /* BGN parameters have not been initialized; use zero noise */
        WebRtcSpl_MemSetW16(pw16_cngVec, 0, w16_lag);
    }

    if (BGNonly)
    {
        /* Copy BGN to outdata */
        for (i = 0; i < w16_lag; i++)
        {
            pw16_outData[i] = pw16_cngVec[i];
        }
    }
    else
    {
        /* Add CNG vector to the Voiced + Unvoiced vectors */
        for (i = 0; i < w16_lag; i++)
        {
            pw16_outData[i] = pw16_outData[i] + pw16_cngVec[i];
        }

        /* increase call number */
        ExpandState->w16_consecExp = ExpandState->w16_consecExp + 1;
        if (ExpandState->w16_consecExp < 0) /* Guard against overflow */
            ExpandState->w16_consecExp = FADE_BGN_TIME; /* "Arbitrary" large num of expands */
    }

    inst->w16_mode = MODE_EXPAND;
    *pw16_len = w16_lag;

    /* Update in-call and post-call statistics */
    if (ExpandState->w16_stopMuting != 1 || BGNonly)
    {
        /*
         * Only do this if StopMuting != 1 or if explicitly BGNonly, otherwise Expand is
         * called from Merge or Normal and special measures must be taken.
         */
        inst->statInst.expandLength += (WebRtc_UWord32) *pw16_len;
        if (ExpandState->w16_expandMuteFactor == 0 || BGNonly)
        {
            /* Only noise expansion */
            inst->statInst.expandedNoiseSamples += *pw16_len;
        }
        else
        {
            /* Voice expand (note: not necessarily _voiced_) */
            inst->statInst.expandedVoiceSamples += *pw16_len;
        }
    }

    return 0;
}

/****************************************************************************
 * WebRtcNetEQ_GenerateBGN(...)
 *
 * This function generates and writes len samples of background noise to the
 * output vector. The Expand function will be called repeatedly until the
 * correct number of samples is produced.
 *
 * Input:
 *      - inst          : NetEq instance, i.e. the user that requests more
 *                        speech/audio data
 *      - scratchPtr    : Pointer to scratch vector
 *      - len           : Desired length of produced BGN.
 *
 *
 * Output:
 *      - pw16_outData  : Pointer to a memory space where the output data
 *                        should be stored
 *
 * Return value         : >=0 - Number of noise samples produced and written
 *                              to output
 *                        -1  - Error
 */

int WebRtcNetEQ_GenerateBGN(DSPInst_t *inst,
#ifdef SCRATCH
                            WebRtc_Word16 *pw16_scratchPtr,
#endif
                            WebRtc_Word16 *pw16_outData, WebRtc_Word16 len)
{

    WebRtc_Word16 pos = 0;
    WebRtc_Word16 tempLen = len;

    while (tempLen > 0)
    {
        /* while we still need more noise samples, call Expand to obtain background noise */
        WebRtcNetEQ_Expand(inst,
#ifdef SCRATCH
            pw16_scratchPtr,
#endif
            &pw16_outData[pos], &tempLen, 1 /*BGNonly*/);

        pos += tempLen; /* we got this many samples */
        tempLen = len - pos; /* this is the number of samples we still need */
    }

    return pos;
}

#undef   SCRATCH_PW16_BEST_CORR_INDEX
#undef   SCRATCH_PW16_BEST_CORR
#undef   SCRATCH_PW16_BEST_DIST_INDEX
#undef   SCRATCH_PW16_BEST_DIST
#undef   SCRATCH_PW16_CORR_VEC
#undef   SCRATCH_PW16_CORR2
#undef   SCRATCH_PW32_AUTO_CORR
#undef   SCRATCH_PW16_RC
#undef   SCRATCH_PW16_RAND_VEC
#undef   SCRATCH_NETEQDSP_CORRELATOR
#undef   SCRATCH_PW16_SCALED_RAND_VEC
#undef   SCRATCH_PW16_UNVOICED_VEC_SPACE

