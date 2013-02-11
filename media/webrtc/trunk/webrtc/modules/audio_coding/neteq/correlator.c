/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "dsp.h"

#include "signal_processing_library.h"

#include "dsp_helpfunctions.h"

/* Scratch usage:

 Type           Name                size  startpos  endpos
 WebRtc_Word16  pw16_corrVec        62    0         61
 WebRtc_Word16  pw16_data_ds        124   0         123
 WebRtc_Word32  pw32_corr           2*54  124       231

 Total:  232
 */

#define	 SCRATCH_pw16_corrVec			0
#define	 SCRATCH_pw16_data_ds			0
#define	 SCRATCH_pw32_corr				124

#define NETEQ_CORRELATOR_DSVECLEN 		124	/* 124 = 60 + 10 + 54 */

WebRtc_Word16 WebRtcNetEQ_Correlator(DSPInst_t *inst,
#ifdef SCRATCH
                                     WebRtc_Word16 *pw16_scratchPtr,
#endif
                                     WebRtc_Word16 *pw16_data,
                                     WebRtc_Word16 w16_dataLen,
                                     WebRtc_Word16 *pw16_corrOut,
                                     WebRtc_Word16 *pw16_corrScale)
{
    WebRtc_Word16 w16_corrLen = 60;
#ifdef SCRATCH
    WebRtc_Word16 *pw16_data_ds = pw16_scratchPtr + SCRATCH_pw16_corrVec;
    WebRtc_Word32 *pw32_corr = (WebRtc_Word32*) (pw16_scratchPtr + SCRATCH_pw32_corr);
    /*	WebRtc_Word16 *pw16_corrVec = pw16_scratchPtr + SCRATCH_pw16_corrVec;*/
#else
    WebRtc_Word16 pw16_data_ds[NETEQ_CORRELATOR_DSVECLEN];
    WebRtc_Word32 pw32_corr[54];
    /*	WebRtc_Word16 pw16_corrVec[4+54+4];*/
#endif
    /*	WebRtc_Word16 *pw16_corr=&pw16_corrVec[4];*/
    WebRtc_Word16 w16_maxVal;
    WebRtc_Word32 w32_maxVal;
    WebRtc_Word16 w16_normVal;
    WebRtc_Word16 w16_normVal2;
    /*	WebRtc_Word16 w16_corrUpsLen;*/
    WebRtc_Word16 *pw16_B = NULL;
    WebRtc_Word16 w16_Blen = 0;
    WebRtc_Word16 w16_factor = 0;

    /* Set constants depending on frequency used */
    if (inst->fs == 8000)
    {
        w16_Blen = 3;
        w16_factor = 2;
        pw16_B = (WebRtc_Word16*) WebRtcNetEQ_kDownsample8kHzTbl;
#ifdef NETEQ_WIDEBAND
    }
    else if (inst->fs==16000)
    {
        w16_Blen = 5;
        w16_factor = 4;
        pw16_B = (WebRtc_Word16*)WebRtcNetEQ_kDownsample16kHzTbl;
#endif
#ifdef NETEQ_32KHZ_WIDEBAND
    }
    else if (inst->fs==32000)
    {
        w16_Blen = 7;
        w16_factor = 8;
        pw16_B = (WebRtc_Word16*)WebRtcNetEQ_kDownsample32kHzTbl;
#endif
#ifdef NETEQ_48KHZ_WIDEBAND
    }
    else /* if inst->fs==48000 */
    {
        w16_Blen = 7;
        w16_factor = 12;
        pw16_B = (WebRtc_Word16*)WebRtcNetEQ_kDownsample48kHzTbl;
#endif
    }

    /* Downsample data in order to work on a 4 kHz sampled signal */
    WebRtcSpl_DownsampleFast(
        pw16_data + w16_dataLen - (NETEQ_CORRELATOR_DSVECLEN * w16_factor),
        (WebRtc_Word16) (NETEQ_CORRELATOR_DSVECLEN * w16_factor), pw16_data_ds,
        NETEQ_CORRELATOR_DSVECLEN, pw16_B, w16_Blen, w16_factor, (WebRtc_Word16) 0);

    /* Normalize downsampled vector to using entire 16 bit */
    w16_maxVal = WebRtcSpl_MaxAbsValueW16(pw16_data_ds, 124);
    w16_normVal = 16 - WebRtcSpl_NormW32((WebRtc_Word32) w16_maxVal);
    WebRtcSpl_VectorBitShiftW16(pw16_data_ds, NETEQ_CORRELATOR_DSVECLEN, pw16_data_ds,
        w16_normVal);

    /* Correlate from lag 10 to lag 60 (20..120 in NB and 40..240 in WB) */

    WebRtcNetEQ_CrossCorr(
        pw32_corr, &pw16_data_ds[NETEQ_CORRELATOR_DSVECLEN - w16_corrLen],
        &pw16_data_ds[NETEQ_CORRELATOR_DSVECLEN - w16_corrLen - 10], 60, 54,
        6 /*maxValue... shifts*/, -1);

    /*
     * Move data from w32 to w16 vector.
     * Normalize downsampled vector to using all 14 bits
     */
    w32_maxVal = WebRtcSpl_MaxAbsValueW32(pw32_corr, 54);
    w16_normVal2 = 18 - WebRtcSpl_NormW32(w32_maxVal);
    w16_normVal2 = WEBRTC_SPL_MAX(w16_normVal2, 0);

    WebRtcSpl_VectorBitShiftW32ToW16(pw16_corrOut, 54, pw32_corr, w16_normVal2);

    /* Total scale factor (right shifts) of correlation value */
    *pw16_corrScale = 2 * w16_normVal + 6 + w16_normVal2;

    return (50 + 1);
}

#undef	 SCRATCH_pw16_corrVec
#undef	 SCRATCH_pw16_data_ds
#undef	 SCRATCH_pw32_corr

