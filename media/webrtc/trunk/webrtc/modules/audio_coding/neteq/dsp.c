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
 * This file contains some DSP initialization functions and 
 * constant table definitions.
 */

#include "dsp.h"

#include "signal_processing_library.h"

#include "neteq_error_codes.h"

/* Filter coefficients used when downsampling from the indicated 
 sample rates (8, 16, 32, 48 kHz) to 4 kHz.
 Coefficients are in Q12. */

/* {0.3, 0.4, 0.3} */
const int16_t WebRtcNetEQ_kDownsample8kHzTbl[] = { 1229, 1638, 1229 };

#ifdef NETEQ_WIDEBAND
/* {0.15, 0.2, 0.3, 0.2, 0.15} */
const int16_t WebRtcNetEQ_kDownsample16kHzTbl[] =
{   614, 819, 1229, 819, 614};
#endif

#ifdef NETEQ_32KHZ_WIDEBAND
/* {0.1425, 0.1251, 0.1525, 0.1628, 0.1525, 0.1251, 0.1425} */
const int16_t WebRtcNetEQ_kDownsample32kHzTbl[] =
{   584, 512, 625, 667, 625, 512, 584};
#endif

#ifdef NETEQ_48KHZ_WIDEBAND
/* {0.2487, 0.0952, 0.1042, 0.1074, 0.1042, 0.0952, 0.2487} */
const int16_t WebRtcNetEQ_kDownsample48kHzTbl[] =
{   1019, 390, 427, 440, 427, 390, 1019};
#endif

/* Constants used in expand function WebRtcNetEQ_Expand */

/* Q12: -1.264421 + 4.8659148*x - 4.0092827*x^2 + 1.4100529*x^3 */
const int16_t WebRtcNetEQ_kMixFractionFuncTbl[4] = { -5179, 19931, -16422, 5776 };

/* Tabulated divisions to save complexity */
/* 1049/{0, .., 6} */
const int16_t WebRtcNetEQ_k1049div[7] = { 0, 1049, 524, 349, 262, 209, 174 };

/* 2097/{0, .., 6} */
const int16_t WebRtcNetEQ_k2097div[7] = { 0, 2097, 1048, 699, 524, 419, 349 };

/* 5243/{0, .., 6} */
const int16_t WebRtcNetEQ_k5243div[7] = { 0, 5243, 2621, 1747, 1310, 1048, 873 };

#ifdef WEBRTC_NETEQ_40BITACC_TEST
/*
 * Run NetEQ with simulated 40-bit accumulator to run bit-exact to a DSP
 * implementation where the main (spl and NetEQ) functions have been
 * 40-bit optimized. For testing purposes.
 */

/****************************************************************************
 * WebRtcNetEQ_40BitAccCrossCorr(...)
 *
 * Calculates the Cross correlation between two sequences seq1 and seq2. Seq1
 * is fixed and seq2 slides as the pointer is increased with step
 *
 * Input:
 *		- seq1			: First sequence (fixed throughout the correlation)
 *		- seq2			: Second sequence (slided step_seq2 for each 
 *						  new correlation)
 *		- dimSeq		: Number of samples to use in the cross correlation.
 *                        Should be no larger than 1024 to avoid overflow.
 *		- dimCrossCorr	: Number of CrossCorrelations to calculate (start 
 *						  position for seq2 is updated for each new one)
 *		- rShift			: Number of right shifts to use
 *		- step_seq2		: How many (positive or negative) steps the seq2 
 *						  pointer should be updated for each new cross 
 *						  correlation value
 *
 * Output:
 *		- crossCorr		: The cross correlation in Q-rShift
 */

void WebRtcNetEQ_40BitAccCrossCorr(int32_t *crossCorr,
    int16_t *seq1,
    int16_t *seq2,
    int16_t dimSeq,
    int16_t dimCrossCorr,
    int16_t rShift,
    int16_t step_seq2)
{
    int i, j;
    int16_t *seq1Ptr, *seq2Ptr;
    int64_t acc;

    for (i = 0; i < dimCrossCorr; i++)
    {
        /* Set the pointer to the static vector, set the pointer to
         the sliding vector and initialize crossCorr */
        seq1Ptr = seq1;
        seq2Ptr = seq2 + (step_seq2 * i);
        acc = 0;

        /* Perform the cross correlation */
        for (j = 0; j < dimSeq; j++)
        {
            acc += WEBRTC_SPL_MUL_16_16((*seq1Ptr), (*seq2Ptr));
            seq1Ptr++;
            seq2Ptr++;
        }

        (*crossCorr) = (int32_t) (acc >> rShift);
        crossCorr++;
    }
}

/****************************************************************************
 * WebRtcNetEQ_40BitAccDotW16W16(...)
 *
 * Calculates the dot product between two vectors (int16_t)
 *
 * Input:
 *		- vector1		: Vector 1
 *		- vector2		: Vector 2
 *		- len			: Number of samples in vector
 *                        Should be no larger than 1024 to avoid overflow.
 *		- scaling		: The number of left shifts required to avoid overflow 
 *						  in the dot product
 * Return value			: The dot product
 */

int32_t WebRtcNetEQ_40BitAccDotW16W16(int16_t *vector1,
    int16_t *vector2,
    int len,
    int scaling)
{
    int32_t sum;
    int i;
    int64_t acc;

    acc = 0;
    for (i = 0; i < len; i++)
    {
        acc += WEBRTC_SPL_MUL_16_16(*vector1++, *vector2++);
    }

    sum = (int32_t) (acc >> scaling);

    return(sum);
}

#endif /* WEBRTC_NETEQ_40BITACC_TEST */

/****************************************************************************
 * WebRtcNetEQ_DSPInit(...)
 *
 * Initializes DSP side of NetEQ.
 *
 * Input:
 *		- inst			: NetEq DSP instance 
 *      - fs            : Initial sample rate (may change when decoding data)
 *
 * Output:
 *		- inst			: Updated instance
 *
 * Return value			: 0 - ok
 *                      : non-zero - error
 */

int WebRtcNetEQ_DSPInit(DSPInst_t *inst, uint16_t fs)
{

    int res = 0;
    int16_t fs_mult;

    /* Pointers and values to save before clearing the instance */
#ifdef NETEQ_CNG_CODEC
    void *savedPtr1 = inst->CNG_Codec_inst;
#endif
    void *savedPtr2 = inst->pw16_readAddress;
    void *savedPtr3 = inst->pw16_writeAddress;
    void *savedPtr4 = inst->main_inst;
#ifdef NETEQ_VAD
    void *savedVADptr = inst->VADInst.VADState;
    VADInitFunction savedVADinit = inst->VADInst.initFunction;
    VADSetmodeFunction savedVADsetmode = inst->VADInst.setmodeFunction;
    VADFunction savedVADfunc = inst->VADInst.VADFunction;
    int16_t savedVADEnabled = inst->VADInst.VADEnabled;
    int savedVADMode = inst->VADInst.VADMode;
#endif /* NETEQ_VAD */
    DSPStats_t saveStats;
    int16_t saveMsPerCall = inst->millisecondsPerCall;
    enum BGNMode saveBgnMode = inst->BGNInst.bgnMode;
#ifdef NETEQ_STEREO
    MasterSlaveInfo* saveMSinfo = inst->msInfo;
#endif

    /* copy contents of statInst to avoid clearing */WEBRTC_SPL_MEMCPY_W16(&saveStats, &(inst->statInst),
        sizeof(DSPStats_t)/sizeof(int16_t));

    /* check that the sample rate is valid */
    if ((fs != 8000)
#ifdef NETEQ_WIDEBAND
    &&(fs!=16000)
#endif
#ifdef NETEQ_32KHZ_WIDEBAND
    &&(fs!=32000)
#endif
#ifdef NETEQ_48KHZ_WIDEBAND
    &&(fs!=48000)
#endif
    )
    {
        /* invalid rate */
        return (CODEC_DB_UNSUPPORTED_FS);
    }

    /* calcualte fs/8000 */
    fs_mult = WebRtcSpl_DivW32W16ResW16(fs, 8000);

    /* Set everything to zero since most variables should be zero at start */
    WebRtcSpl_MemSetW16((int16_t *) inst, 0, sizeof(DSPInst_t) / sizeof(int16_t));

    /* Restore saved pointers  */
#ifdef NETEQ_CNG_CODEC
    inst->CNG_Codec_inst = (CNG_dec_inst *)savedPtr1;
#endif
    inst->pw16_readAddress = (int16_t *) savedPtr2;
    inst->pw16_writeAddress = (int16_t *) savedPtr3;
    inst->main_inst = savedPtr4;
#ifdef NETEQ_VAD
    inst->VADInst.VADState = savedVADptr;
    inst->VADInst.initFunction = savedVADinit;
    inst->VADInst.setmodeFunction = savedVADsetmode;
    inst->VADInst.VADFunction = savedVADfunc;
    inst->VADInst.VADEnabled = savedVADEnabled;
    inst->VADInst.VADMode = savedVADMode;
#endif /* NETEQ_VAD */

    /* Initialize main part */
    inst->fs = fs;
    inst->millisecondsPerCall = saveMsPerCall;
    inst->timestampsPerCall = inst->millisecondsPerCall * 8 * fs_mult;
    inst->ExpandInst.w16_overlap = 5 * fs_mult;
    inst->endPosition = 565 * fs_mult;
    inst->curPosition = inst->endPosition - inst->ExpandInst.w16_overlap;
    inst->w16_seedInc = 1;
    inst->uw16_seed = 777;
    inst->w16_muteFactor = 16384; /* 1.0 in Q14 */
    inst->w16_frameLen = 3 * inst->timestampsPerCall; /* Dummy initialize to 30ms */

    inst->w16_speechHistoryLen = 256 * fs_mult;
    inst->pw16_speechHistory = &inst->speechBuffer[inst->endPosition
        - inst->w16_speechHistoryLen];
    inst->ExpandInst.pw16_overlapVec = &(inst->pw16_speechHistory[inst->w16_speechHistoryLen
        - inst->ExpandInst.w16_overlap]);

    /* Reusage of memory in speechBuffer inside Expand */
    inst->ExpandInst.pw16_expVecs[0] = &inst->speechBuffer[0];
    inst->ExpandInst.pw16_expVecs[1] = &inst->speechBuffer[126 * fs_mult];
    inst->ExpandInst.pw16_arState = &inst->speechBuffer[2 * 126 * fs_mult];
    inst->ExpandInst.pw16_arFilter = &inst->speechBuffer[2 * 126 * fs_mult
        + UNVOICED_LPC_ORDER];
    /* Ends at 2*126*fs_mult+UNVOICED_LPC_ORDER+(UNVOICED_LPC_ORDER+1) */

    inst->ExpandInst.w16_expandMuteFactor = 16384; /* 1.0 in Q14 */

    /* Initialize BGN part */
    inst->BGNInst.pw16_filter[0] = 4096;
    inst->BGNInst.w16_scale = 20000;
    inst->BGNInst.w16_scaleShift = 24;
    inst->BGNInst.w32_energyUpdate = 500000;
    inst->BGNInst.w32_energyUpdateLow = 0;
    inst->BGNInst.w32_energy = 2500;
    inst->BGNInst.w16_initialized = 0;
    inst->BGNInst.bgnMode = saveBgnMode;

    /* Recreate statistics counters */WEBRTC_SPL_MEMCPY_W16(&(inst->statInst), &saveStats,
        sizeof(DSPStats_t)/sizeof(int16_t));

#ifdef NETEQ_STEREO
    /* Write back the pointer. */
    inst->msInfo = saveMSinfo;
#endif

#ifdef NETEQ_CNG_CODEC
    if (inst->CNG_Codec_inst!=NULL)
    {
        /* initialize comfort noise generator */
        res |= WebRtcCng_InitDec(inst->CNG_Codec_inst);
    }
#endif

#ifdef NETEQ_VAD
    /* initialize PostDecode VAD instance
     (don't bother checking for NULL instance, this is done inside init function) */
    res |= WebRtcNetEQ_InitVAD(&inst->VADInst, fs);
#endif /* NETEQ_VAD */

    return (res);
}

/****************************************************************************
 * WebRtcNetEQ_AddressInit(...)
 *
 * Initializes the shared-memory communication on the DSP side.
 *
 * Input:
 *		- inst			    : NetEQ DSP instance 
 *      - data2McuAddress   : Pointer to memory where DSP writes / MCU reads
 *      - data2DspAddress   : Pointer to memory where MCU writes / DSP reads
 *      - mainInst          : NetEQ main instance
 *
 * Output:
 *		- inst			    : Updated instance
 *
 * Return value			    : 0 - ok
 */

int WebRtcNetEQ_AddressInit(DSPInst_t *inst, const void *data2McuAddress,
                            const void *data2DspAddress, const void *mainInst)
{

    /* set shared-memory addresses in the DSP instance */
    inst->pw16_readAddress = (int16_t *) data2DspAddress;
    inst->pw16_writeAddress = (int16_t *) data2McuAddress;

    /* set pointer to main NetEQ instance */
    inst->main_inst = (void *) mainInst;

    /* set output frame size to 10 ms = 80 samples in narrowband */
    inst->millisecondsPerCall = 10;
    inst->timestampsPerCall = 80;

    return (0);

}

/****************************************************************************
 * NETEQDSP_clearInCallStats(...)
 *
 * Reset in-call statistics variables on DSP side.
 *
 * Input:
 *		- inst			    : NetEQ DSP instance 
 *
 * Output:
 *		- inst			    : Updated instance
 *
 * Return value			    : 0 - ok
 */

int WebRtcNetEQ_ClearInCallStats(DSPInst_t *inst)
{
    /* Reset statistics counters */
    inst->statInst.accelerateLength = 0;
    inst->statInst.expandLength = 0;
    inst->statInst.preemptiveLength = 0;
    inst->statInst.addedSamples = 0;
    return (0);
}

/****************************************************************************
 * WebRtcNetEQ_ClearPostCallStats(...)
 *
 * Reset post-call statistics variables on DSP side.
 *
 * Input:
 *		- inst			    : NetEQ DSP instance 
 *
 * Output:
 *		- inst			    : Updated instance
 *
 * Return value			    : 0 - ok
 */

int WebRtcNetEQ_ClearPostCallStats(DSPInst_t *inst)
{

    /* Reset statistics counters */
    inst->statInst.expandedVoiceSamples = 0;
    inst->statInst.expandedNoiseSamples = 0;
    return (0);
}

/****************************************************************************
 * WebRtcNetEQ_ClearActivityStats(...)
 *
 * Reset processing activity statistics.
 *
 * Input:
 *    - inst          : NetEQ DSP instance
 *
 * Output:
 *    - inst          : Updated instance
 *
 */

void WebRtcNetEQ_ClearActivityStats(DSPInst_t *inst) {
  memset(&inst->activity_stats, 0, sizeof(ActivityStats));
}

#ifdef NETEQ_VAD

/****************************************************************************
 * WebRtcNetEQ_InitVAD(...)
 *
 * Initializes post-decode VAD instance.
 *
 * Input:
 *		- VADinst		: PostDecodeVAD instance
 *      - fs            : Initial sample rate
 *
 * Output:
 *		- VADinst		: Updated instance
 *
 * Return value			:  0 - Ok
 *						  -1 - Error
 */

int WebRtcNetEQ_InitVAD(PostDecodeVAD_t *VADInst, uint16_t fs)
{

    int res = 0;

    /* initially, disable the post-decode VAD */
    VADInst->VADEnabled = 0;

    if (VADInst->VADState != NULL /* if VAD state is provided */
        && VADInst->initFunction != NULL /* and all function ... */
        && VADInst->setmodeFunction != NULL /* ... pointers ... */
        && VADInst->VADFunction != NULL) /* ... are defined */
    {
        res = VADInst->initFunction( VADInst->VADState ); /* call VAD init function */
        res |= WebRtcNetEQ_SetVADModeInternal( VADInst, VADInst->VADMode );

        if (res!=0)
        {
            /* something is wrong; play it safe and set the VADstate to NULL */
            VADInst->VADState = NULL;
        }
        else if (fs<=16000)
        {
            /* enable VAD if NB or WB (VAD cannot handle SWB) */
            VADInst->VADEnabled = 1;
        }
    }

    /* reset SID/CNG interval counter */
    VADInst->SIDintervalCounter = 0;

    /* initialize with active-speaker decision */
    VADInst->VADDecision = 1;

    return(res);

}

/****************************************************************************
 * WebRtcNetEQ_SetVADModeInternal(...)
 *
 * Set the VAD mode in the VAD struct, and communicate it to the VAD instance 
 * if it exists.
 *
 * Input:
 *		- VADinst		: PostDecodeVAD instance
 *      - mode          : Mode number passed on to the VAD function
 *
 * Output:
 *		- VADinst		: Updated instance
 *
 * Return value			:  0 - Ok
 *						  -1 - Error
 */

int WebRtcNetEQ_SetVADModeInternal(PostDecodeVAD_t *VADInst, int mode)
{

    int res = 0;

    VADInst->VADMode = mode;

    if (VADInst->VADState != NULL)
    {
        /* call setmode function */
        res = VADInst->setmodeFunction(VADInst->VADState, mode);
    }

    return(res);

}

#endif /* NETEQ_VAD */

/****************************************************************************
 * WebRtcNetEQ_FlushSpeechBuffer(...)
 *
 * Flush the speech buffer.
 *
 * Input:
 *		- inst			: NetEq DSP instance 
 *
 * Output:
 *		- inst			: Updated instance
 *
 * Return value			: 0 - ok
 *                      : non-zero - error
 */

int WebRtcNetEQ_FlushSpeechBuffer(DSPInst_t *inst)
{
    int16_t fs_mult;

    /* calcualte fs/8000 */
    fs_mult = WebRtcSpl_DivW32W16ResW16(inst->fs, 8000);

    /* clear buffer */
    WebRtcSpl_MemSetW16(inst->speechBuffer, 0, SPEECH_BUF_SIZE);
    inst->endPosition = 565 * fs_mult;
    inst->curPosition = inst->endPosition - inst->ExpandInst.w16_overlap;

    return 0;
}

