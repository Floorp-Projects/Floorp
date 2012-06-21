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
 * This file contains the function for handling "normal" speech operation.
 */
#include "dsp.h"

#include "signal_processing_library.h"

#include "dsp_helpfunctions.h"

/* Scratch usage:

 Type           Name                    size            startpos        endpos
 WebRtc_Word16  pw16_expanded           125*fs/8000     0               125*fs/8000-1

 func           WebRtcNetEQ_Expand      40+370*fs/8000  125*fs/8000     39+495*fs/8000

 Total:  40+495*fs/8000
 */

#define     SCRATCH_PW16_EXPANDED           0
#if (defined(NETEQ_48KHZ_WIDEBAND)) 
#define     SCRATCH_NETEQ_EXPAND    756
#elif (defined(NETEQ_32KHZ_WIDEBAND)) 
#define     SCRATCH_NETEQ_EXPAND    504
#elif (defined(NETEQ_WIDEBAND)) 
#define     SCRATCH_NETEQ_EXPAND    252
#else    /* NB */
#define     SCRATCH_NETEQ_EXPAND    126
#endif

/****************************************************************************
 * WebRtcNetEQ_Normal(...)
 *
 * This function has the possibility to modify data that is played out in Normal
 * mode, for example adjust the gain of the signal. The length of the signal 
 * can not be changed.
 *
 * Input:
 *      - inst          : NetEq instance, i.e. the user that requests more
 *                        speech/audio data
 *      - scratchPtr    : Pointer to scratch vector
 *      - decoded       : Pointer to vector of new data from decoder
 *                        (Vector contents may be altered by the function)
 *      - len           : Number of input samples
 *
 * Output:
 *      - inst          : Updated user information
 *      - outData       : Pointer to a memory space where the output data
 *                        should be stored
 *      - pw16_len      : Pointer to variable where the number of samples
 *                        produced will be written
 *
 * Return value         : >=0 - Number of samples written to outData
 *                         -1 - Error
 */

int WebRtcNetEQ_Normal(DSPInst_t *inst,
#ifdef SCRATCH
                       WebRtc_Word16 *pw16_scratchPtr,
#endif
                       WebRtc_Word16 *pw16_decoded, WebRtc_Word16 len,
                       WebRtc_Word16 *pw16_outData, WebRtc_Word16 *pw16_len)
{

    int i;
    WebRtc_Word16 fs_mult;
    WebRtc_Word16 fs_shift;
    WebRtc_Word32 w32_En_speech;
    WebRtc_Word16 enLen;
    WebRtc_Word16 w16_muted;
    WebRtc_Word16 w16_inc, w16_frac;
    WebRtc_Word16 w16_tmp;
    WebRtc_Word32 w32_tmp;

    /* Sanity check */
    if (len < 0)
    {
        /* Cannot have negative length of input vector */
        return (-1);
    }

    if (len == 0)
    {
        /* Still got some data to play => continue with the same mode */
        *pw16_len = len;
        return (len);
    }

    fs_mult = WebRtcSpl_DivW32W16ResW16(inst->fs, 8000);
    fs_shift = 30 - WebRtcSpl_NormW32(fs_mult); /* Note that this is not "exact" for 48kHz */

    /*
     * Check if last RecOut call resulted in an Expand or a FadeToBGN. If so, we have to take
     * care of some cross-fading and unmuting.
     */
    if (inst->w16_mode == MODE_EXPAND || inst->w16_mode == MODE_FADE_TO_BGN)
    {

        /* Define memory where temporary result from Expand algorithm can be stored. */
#ifdef SCRATCH
        WebRtc_Word16 *pw16_expanded = pw16_scratchPtr + SCRATCH_PW16_EXPANDED;
#else
        WebRtc_Word16 pw16_expanded[FSMULT * 125];
#endif
        WebRtc_Word16 expandedLen = 0;
        WebRtc_Word16 w16_decodedMax;

        /* Find largest value in new data */
        w16_decodedMax = WebRtcSpl_MaxAbsValueW16(pw16_decoded, (WebRtc_Word16) len);

        /* Generate interpolation data using Expand */
        /* First, set Expand parameters to appropriate values. */
        inst->ExpandInst.w16_lagsPosition = 0;
        inst->ExpandInst.w16_lagsDirection = 0;
        inst->ExpandInst.w16_stopMuting = 1; /* Do not mute signal any more */

        /* Call Expand */
        WebRtcNetEQ_Expand(inst,
#ifdef SCRATCH
            pw16_scratchPtr + SCRATCH_NETEQ_EXPAND,
#endif
            pw16_expanded, &expandedLen, (WebRtc_Word16) (inst->w16_mode == MODE_FADE_TO_BGN));

        inst->ExpandInst.w16_stopMuting = 0; /* Restore value */
        inst->ExpandInst.w16_consecExp = 0; /* Last was not Expand any more */

        /* Adjust muting factor (main muting factor times expand muting factor) */
        if (inst->w16_mode == MODE_FADE_TO_BGN)
        {
            /* If last mode was FadeToBGN, the mute factor should be zero. */
            inst->w16_muteFactor = 0;
        }
        else
        {
            /* w16_muteFactor * w16_expandMuteFactor */
            inst->w16_muteFactor
                = (WebRtc_Word16) WEBRTC_SPL_MUL_16_16_RSFT(inst->w16_muteFactor,
                    inst->ExpandInst.w16_expandMuteFactor, 14);
        }

        /* Adjust muting factor if needed (to BGN level) */
        enLen = WEBRTC_SPL_MIN(fs_mult<<6, len); /* min( fs_mult * 64, len ) */
        w16_tmp = 6 + fs_shift - WebRtcSpl_NormW32(
            WEBRTC_SPL_MUL_16_16(w16_decodedMax, w16_decodedMax));
        w16_tmp = WEBRTC_SPL_MAX(w16_tmp, 0);
        w32_En_speech = WebRtcNetEQ_DotW16W16(pw16_decoded, pw16_decoded, enLen, w16_tmp);
        w32_En_speech = WebRtcSpl_DivW32W16(w32_En_speech, (WebRtc_Word16) (enLen >> w16_tmp));

        if ((w32_En_speech != 0) && (w32_En_speech > inst->BGNInst.w32_energy))
        {
            /* Normalize new frame energy to 15 bits */
            w16_tmp = WebRtcSpl_NormW32(w32_En_speech) - 16;
            /* we want inst->BGNInst.energy/En_speech in Q14 */
            w32_tmp = WEBRTC_SPL_SHIFT_W32(inst->BGNInst.w32_energy, (w16_tmp+14));
            w16_tmp = (WebRtc_Word16) WEBRTC_SPL_SHIFT_W32(w32_En_speech, w16_tmp);
            w16_tmp = (WebRtc_Word16) WebRtcSpl_DivW32W16(w32_tmp, w16_tmp);
            w16_muted = (WebRtc_Word16) WebRtcSpl_SqrtFloor(
                WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32) w16_tmp,
                    14)); /* w16_muted in Q14 (sqrt(Q28)) */
        }
        else
        {
            w16_muted = 16384; /* 1.0 in Q14 */
        }
        if (w16_muted > inst->w16_muteFactor)
        {
            inst->w16_muteFactor = WEBRTC_SPL_MIN(w16_muted, 16384);
        }

        /* If muted increase by 0.64 for every 20 ms (NB/WB 0.0040/0.0020 in Q14) */
        w16_inc = WebRtcSpl_DivW32W16ResW16(64, fs_mult);
        for (i = 0; i < len; i++)
        {
            /* scale with mute factor */
            w32_tmp = WEBRTC_SPL_MUL_16_16(pw16_decoded[i], inst->w16_muteFactor);
            /* shift 14 with proper rounding */
            pw16_decoded[i] = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32((w32_tmp + 8192), 14);
            /* increase mute_factor towards 16384 */
            inst->w16_muteFactor = WEBRTC_SPL_MIN(16384, (inst->w16_muteFactor+w16_inc));
        }

        /*
         * Interpolate the expanded data into the new vector
         * (NB/WB/SWB32/SWB40 8/16/32/32 samples)
         */
        fs_shift = WEBRTC_SPL_MIN(3, fs_shift); /* Set to 3 for >32kHz */
        w16_inc = 4 >> fs_shift;
        w16_frac = w16_inc;
        for (i = 0; i < 8 * fs_mult; i++)
        {
            pw16_decoded[i] = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(
                (WEBRTC_SPL_MUL_16_16(w16_frac, pw16_decoded[i]) +
                    WEBRTC_SPL_MUL_16_16((32 - w16_frac), pw16_expanded[i]) + 8),
                5);
            w16_frac += w16_inc;
        }

#ifdef NETEQ_CNG_CODEC
    }
    else if (inst->w16_mode==MODE_RFC3389CNG)
    { /* previous was RFC 3389 CNG...*/
        WebRtc_Word16 pw16_CngInterp[32];
        /* Reset mute factor and start up fresh */
        inst->w16_muteFactor = 16384;
        if (inst->CNG_Codec_inst != NULL)
        {
            /* Generate long enough for 32kHz */
            if(WebRtcCng_Generate(inst->CNG_Codec_inst,pw16_CngInterp, 32, 0)<0)
            {
                /* error returned; set return vector to all zeros */
                WebRtcSpl_MemSetW16(pw16_CngInterp, 0, 32);
            }
        }
        else
        {
            /*
             * If no CNG instance is defined, just copy from the decoded data.
             * (This will result in interpolating the decoded with itself.)
             */
            WEBRTC_SPL_MEMCPY_W16(pw16_CngInterp, pw16_decoded, fs_mult * 8);
        }
        /*
         * Interpolate the CNG into the new vector
         * (NB/WB/SWB32kHz/SWB48kHz 8/16/32/32 samples)
         */
        fs_shift = WEBRTC_SPL_MIN(3, fs_shift); /* Set to 3 for >32kHz */
        w16_inc = 4>>fs_shift;
        w16_frac = w16_inc;
        for (i = 0; i < 8 * fs_mult; i++)
        {
            pw16_decoded[i] = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(
                (WEBRTC_SPL_MUL_16_16(w16_frac, pw16_decoded[i]) +
                    WEBRTC_SPL_MUL_16_16((32-w16_frac), pw16_CngInterp[i]) + 8),
                5);
            w16_frac += w16_inc;
        }
#endif

    }
    else if (inst->w16_muteFactor < 16384)
    {
        /*
         * Previous was neither of Expand, FadeToBGN or RFC3389_CNG, but we are still
         * ramping up from previous muting.
         * If muted increase by 0.64 for every 20 ms (NB/WB 0.0040/0.0020 in Q14)
         */
        w16_inc = WebRtcSpl_DivW32W16ResW16(64, fs_mult);
        for (i = 0; i < len; i++)
        {
            /* scale with mute factor */
            w32_tmp = WEBRTC_SPL_MUL_16_16(pw16_decoded[i], inst->w16_muteFactor);
            /* shift 14 with proper rounding */
            pw16_decoded[i] = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32((w32_tmp + 8192), 14);
            /* increase mute_factor towards 16384 */
            inst->w16_muteFactor = WEBRTC_SPL_MIN(16384, (inst->w16_muteFactor+w16_inc));
        }
    }

    /* Copy data to other buffer */WEBRTC_SPL_MEMMOVE_W16(pw16_outData, pw16_decoded, len);

    inst->w16_mode = MODE_NORMAL;
    *pw16_len = len;
    return (len);

}

#undef SCRATCH_PW16_EXPANDED
#undef SCRATCH_NETEQ_EXPAND

