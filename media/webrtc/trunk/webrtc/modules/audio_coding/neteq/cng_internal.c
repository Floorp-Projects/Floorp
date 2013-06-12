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
 * This file contains the function for obtaining comfort noise from noise parameters
 * according to IETF RFC 3389.
 */

#include "dsp.h"

#include "signal_processing_library.h"
#include "webrtc_cng.h"

#include "dsp_helpfunctions.h"
#include "neteq_error_codes.h"

/****************************************************************************
 * WebRtcNetEQ_Cng(...)
 *
 * This function produces CNG according to RFC 3389.
 *
 * Input:
 *      - inst          : NetEQ DSP instance
 *      - len           : Number of samples to produce (max 640 or
 *                        640 - fsHz*5/8000 for first-time CNG, governed by
 *                        the definition of WEBRTC_CNG_MAX_OUTSIZE_ORDER in
 *                        webrtc_cng.h)
 *
 * Output:
 *      - pw16_outData  : Output CNG
 *
 * Return value         :  0 - Ok
 *                        <0 - Error
 */

#ifdef NETEQ_CNG_CODEC
/* Must compile NetEQ with CNG support to enable this function */

int WebRtcNetEQ_Cng(DSPInst_t *inst, WebRtc_Word16 *pw16_outData, int len)
{
    WebRtc_Word16 w16_winMute = 0; /* mixing factor for overlap data */
    WebRtc_Word16 w16_winUnMute = 0; /* mixing factor for comfort noise */
    WebRtc_Word16 w16_winMuteInc = 0; /* mixing factor increment (negative) */
    WebRtc_Word16 w16_winUnMuteInc = 0; /* mixing factor increment */
    int i;

    /*
     * Check if last RecOut call was other than RFC3389,
     * that is, this call is the first of a CNG period.
     */
    if (inst->w16_mode != MODE_RFC3389CNG)
    {
        /* Reset generation and overlap slightly with old data */

        /* Generate len samples + overlap */
        if (WebRtcCng_Generate(inst->CNG_Codec_inst, pw16_outData,
            (WebRtc_Word16) (len + inst->ExpandInst.w16_overlap), 1) < 0)
        {
            /* error returned */
            return -WebRtcCng_GetErrorCodeDec(inst->CNG_Codec_inst);
        }

        /* Set windowing parameters depending on sample rate */
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
        else if (inst->fs == 48000)
        {
            /* Windowing in Q15 */
            w16_winMute = NETEQ_OVERLAP_WINMUTE_48KHZ_START;
            w16_winMuteInc = NETEQ_OVERLAP_WINMUTE_48KHZ_INC;
            w16_winUnMute = NETEQ_OVERLAP_WINUNMUTE_48KHZ_START;
            w16_winUnMuteInc = NETEQ_OVERLAP_WINUNMUTE_48KHZ_INC;
#endif
        }
        else
        {
            /* Unsupported sample rate (should not be possible) */
            return NETEQ_OTHER_ERROR;
        }

        /* Do overlap add between new vector and overlap */
        for (i = 0; i < inst->ExpandInst.w16_overlap; i++)
        {
            /* overlapVec[i] = WinMute * overlapVec[i] + WinUnMute * outData[i] */
            inst->ExpandInst.pw16_overlapVec[i] = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(
                WEBRTC_SPL_MUL_16_16(
                    inst->ExpandInst.pw16_overlapVec[i], w16_winMute) +
                WEBRTC_SPL_MUL_16_16(pw16_outData[i], w16_winUnMute)
                + 16384, 15); /* shift with proper rounding */

            w16_winMute += w16_winMuteInc; /* decrease mute factor (inc<0) */
            w16_winUnMute += w16_winUnMuteInc; /* increase unmute factor (inc>0) */

        }

        /*
         * Shift the contents of the outData buffer by overlap samples, since we
         * already used these first samples in the overlapVec above
         */

        WEBRTC_SPL_MEMMOVE_W16(pw16_outData, pw16_outData+inst->ExpandInst.w16_overlap, len);

    }
    else
    {
        /* This is a subsequent CNG call; no special overlap needed */

        /* Generate len samples */
        if (WebRtcCng_Generate(inst->CNG_Codec_inst, pw16_outData, (WebRtc_Word16) len, 0) < 0)
        {
            /* error returned */
            return -WebRtcCng_GetErrorCodeDec(inst->CNG_Codec_inst);
        }
    }

    return 0;

}

#endif /* NETEQ_CNG_CODEC */

