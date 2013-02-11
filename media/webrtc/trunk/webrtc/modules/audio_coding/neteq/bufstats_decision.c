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
 * This file contains the function where the main decision logic for buffer level
 * adaptation happens.
 */

#include "buffer_stats.h"

#include <assert.h>

#include "signal_processing_library.h"

#include "automode.h"
#include "neteq_defines.h"
#include "neteq_error_codes.h"
#include "webrtc_neteq.h"

#define NETEQ_BUFSTAT_20MS_Q7 2560 /* = 20 ms in Q7  */

WebRtc_UWord16 WebRtcNetEQ_BufstatsDecision(BufstatsInst_t *inst, WebRtc_Word16 frameSize,
                                            WebRtc_Word32 cur_size, WebRtc_UWord32 targetTS,
                                            WebRtc_UWord32 availableTS, int noPacket,
                                            int cngPacket, int prevPlayMode,
                                            enum WebRtcNetEQPlayoutMode playoutMode,
                                            int timestampsPerCall, int NoOfExpandCalls,
                                            WebRtc_Word16 fs_mult,
                                            WebRtc_Word16 lastModeBGNonly, int playDtmf)
{

    int currentDelayMs;
    WebRtc_Word32 currSizeSamples = cur_size;
    WebRtc_Word16 extraDelayPacketsQ8 = 0;

    /* Avoid overflow if the buffer size should be really large (cur_size is limited 256ms) */
    WebRtc_Word32 curr_sizeQ7 = WEBRTC_SPL_LSHIFT_W32(cur_size, 4);
    WebRtc_UWord16 level_limit_hi, level_limit_lo;

    inst->Automode_inst.prevTimeScale &= (prevPlayMode == MODE_SUCCESS_ACCELERATE
        || prevPlayMode == MODE_LOWEN_ACCELERATE || prevPlayMode == MODE_SUCCESS_PREEMPTIVE
        || prevPlayMode == MODE_LOWEN_PREEMPTIVE);

    if ((prevPlayMode != MODE_RFC3389CNG) && (prevPlayMode != MODE_CODEC_INTERNAL_CNG))
    {
        /*
         * Do not update buffer history if currently playing CNG
         * since it will bias the filtered buffer level.
         */
        WebRtcNetEQ_BufferLevelFilter(cur_size, &(inst->Automode_inst), timestampsPerCall,
            fs_mult);
    }
    else
    {
        /* only update time counters */
        inst->Automode_inst.packetIatCountSamp += timestampsPerCall; /* packet inter-arrival time */
        inst->Automode_inst.peakIatCountSamp += timestampsPerCall; /* peak inter-arrival time */
        inst->Automode_inst.timescaleHoldOff >>= 1; /* time-scaling limiter */
    }
    cur_size = WEBRTC_SPL_MIN(curr_sizeQ7, WEBRTC_SPL_WORD16_MAX);

    /* Calculate VQmon related variables */
    /* avgDelay = avgDelay*(511/512) + currentDelay*(1/512) (sample ms delay in Q8) */
    inst->avgDelayMsQ8 = (WebRtc_Word16) (WEBRTC_SPL_MUL_16_16_RSFT(inst->avgDelayMsQ8,511,9)
        + (cur_size >> 9));

    /* Update maximum delay if needed */
    currentDelayMs = (curr_sizeQ7 >> 7);
    if (currentDelayMs > inst->maxDelayMs)
    {
        inst->maxDelayMs = currentDelayMs;
    }

    /* NetEQ is on with normal or steaming mode */
    if (playoutMode == kPlayoutOn || playoutMode == kPlayoutStreaming)
    {
        /* Guard for errors, so that it should not get stuck in error mode */
        if (prevPlayMode == MODE_ERROR)
        {
            if (noPacket)
            {
                return BUFSTATS_DO_EXPAND;
            }
            else
            {
                return BUFSTAT_REINIT;
            }
        }

        if (prevPlayMode != MODE_EXPAND && prevPlayMode != MODE_FADE_TO_BGN)
        {
            inst->w16_noExpand = 1;
        }
        else
        {
            inst->w16_noExpand = 0;
        }

        if (cngPacket)
        {
            /* signed difference between wanted and available TS */
            WebRtc_Word32 diffTS = (inst->uw32_CNGplayedTS + targetTS) - availableTS;
            int32_t optimal_level_samp = (inst->Automode_inst.optBufLevel *
                inst->Automode_inst.packetSpeechLenSamp) >> 8;
            int32_t excess_waiting_time_samp = -diffTS - optimal_level_samp;

            if (excess_waiting_time_samp > optimal_level_samp / 2)
            {
                /* The waiting time for this packet will be longer than 1.5
                 * times the wanted buffer delay. Advance the clock to cut
                 * waiting time down to the optimal.
                 */
                inst->uw32_CNGplayedTS += excess_waiting_time_samp;
                diffTS += excess_waiting_time_samp;
            }

            if ((diffTS) < 0 && (prevPlayMode == MODE_RFC3389CNG))
            {
                /* Not time to play this packet yet. Wait another round before using this
                 * packet. Keep on playing CNG from previous CNG parameters. */
                return BUFSTATS_DO_RFC3389CNG_NOPACKET;
            }

            /* otherwise, go for the CNG packet now */
            return BUFSTATS_DO_RFC3389CNG_PACKET;
        }

        /*Check for expand/cng */
        if (noPacket)
        {
            if (inst->w16_cngOn == CNG_RFC3389_ON)
            {
                /* keep on playing CNG */
                return BUFSTATS_DO_RFC3389CNG_NOPACKET;
            }
            else if (inst->w16_cngOn == CNG_INTERNAL_ON)
            {
                /* keep on playing internal CNG */
                return BUFSTATS_DO_INTERNAL_CNG_NOPACKET;
            }
            else if (playDtmf == 1)
            {
                /* we have not audio data, but can play DTMF */
                return BUFSTATS_DO_DTMF_ONLY;
            }
            else
            {
                /* nothing to play => do Expand */
                return BUFSTATS_DO_EXPAND;
            }
        }

        /*
         * If the expand period was very long, reset NetEQ since it is likely that the
         * sender was restarted.
         */
        if (NoOfExpandCalls > REINIT_AFTER_EXPANDS) return BUFSTAT_REINIT_DECODER;

        /* Calculate extra delay in Q8 packets */
        if (inst->Automode_inst.extraDelayMs > 0 && inst->Automode_inst.packetSpeechLenSamp
            > 0)
        {
            extraDelayPacketsQ8 = WebRtcSpl_DivW32W16ResW16(
                (WEBRTC_SPL_MUL(inst->Automode_inst.extraDelayMs, 8 * fs_mult) << 8),
                inst->Automode_inst.packetSpeechLenSamp);
            /* (extra delay in samples in Q8) */
        }

        /* Check if needed packet is available */
        if (targetTS == availableTS)
        {

            /* If last mode was not expand, and there is no DTMF to play */
            if (inst->w16_noExpand == 1 && playDtmf == 0)
            {
                /* If so check for accelerate */

                level_limit_lo = ((inst->Automode_inst.optBufLevel) >> 1) /* 50 % */
                    + ((inst->Automode_inst.optBufLevel) >> 2); /* ... + 25% = 75% */

                /* set upper limit to optBufLevel, but make sure that window is at least 20ms */
                level_limit_hi = WEBRTC_SPL_MAX(inst->Automode_inst.optBufLevel,
                    level_limit_lo +
                    WebRtcSpl_DivW32W16ResW16((WEBRTC_SPL_MUL(20*8, fs_mult) << 8),
                        inst->Automode_inst.packetSpeechLenSamp));

                /* if extra delay is non-zero, add it */
                if (extraDelayPacketsQ8 > 0)
                {
                    level_limit_hi += extraDelayPacketsQ8;
                    level_limit_lo += extraDelayPacketsQ8;
                }

                if (((inst->Automode_inst.buffLevelFilt >= level_limit_hi) &&
                    (inst->Automode_inst.timescaleHoldOff == 0)) ||
                    (inst->Automode_inst.buffLevelFilt >= level_limit_hi << 2))
                {
                    /*
                     * Buffer level higher than limit and time-scaling allowed,
                     * OR buffer level _really_ high.
                     */
                    return BUFSTATS_DO_ACCELERATE;
                }
                else if ((inst->Automode_inst.buffLevelFilt < level_limit_lo)
                    && (inst->Automode_inst.timescaleHoldOff == 0))
                {
                    return BUFSTATS_DO_PREEMPTIVE_EXPAND;
                }
            }
            return BUFSTATS_DO_NORMAL;
        }

        /* Check for Merge */
        else if (availableTS > targetTS)
        {

            /* Check that we do not play a packet "too early" */
            if ((prevPlayMode == MODE_EXPAND)
                && (availableTS - targetTS
                    < (WebRtc_UWord32) WEBRTC_SPL_MUL_16_16((WebRtc_Word16)timestampsPerCall,
                        (WebRtc_Word16)REINIT_AFTER_EXPANDS))
                && (NoOfExpandCalls < MAX_WAIT_FOR_PACKET)
                && (availableTS
                    > targetTS
                        + WEBRTC_SPL_MUL_16_16((WebRtc_Word16)timestampsPerCall,
                            (WebRtc_Word16)NoOfExpandCalls))
                && (inst->Automode_inst.buffLevelFilt <= inst->Automode_inst.optBufLevel
                    + extraDelayPacketsQ8))
            {
                if (playDtmf == 1)
                {
                    /* we still have DTMF to play, so do not perform expand */
                    return BUFSTATS_DO_DTMF_ONLY;
                }
                else
                {
                    /* nothing to play */
                    return BUFSTATS_DO_EXPAND;
                }
            }

            /* If previous was CNG period or BGNonly then no merge is needed */
            if ((prevPlayMode == MODE_RFC3389CNG) || (prevPlayMode == MODE_CODEC_INTERNAL_CNG)
                || lastModeBGNonly)
            {
                /*
                 * Keep the same delay as before the CNG (or maximum 70 ms in buffer as safety
                 * precaution), but make sure that the number of samples in buffer is no
                 * higher than 4 times the optimal level.
                 */
                WebRtc_Word32 diffTS = (inst->uw32_CNGplayedTS + targetTS) - availableTS;
                if (diffTS >= 0
                    || (WEBRTC_SPL_MUL_16_16_RSFT( inst->Automode_inst.optBufLevel
                        + extraDelayPacketsQ8,
                        inst->Automode_inst.packetSpeechLenSamp, 6) < currSizeSamples))
                {
                    /* it is time to play this new packet */
                    return BUFSTATS_DO_NORMAL;
                }
                else
                {
                    /* it is too early to play this new packet => keep on playing CNG */
                    if (prevPlayMode == MODE_RFC3389CNG)
                    {
                        return BUFSTATS_DO_RFC3389CNG_NOPACKET;
                    }
                    else if (prevPlayMode == MODE_CODEC_INTERNAL_CNG)
                    {
                        return BUFSTATS_DO_INTERNAL_CNG_NOPACKET;
                    }
                    else if (playDtmf == 1)
                    {
                        /* we have not audio data, but can play DTMF */
                        return BUFSTATS_DO_DTMF_ONLY;
                    }
                    else /* lastModeBGNonly */
                    {
                        /* signal expand, but this will result in BGN again */
                        return BUFSTATS_DO_EXPAND;
                    }
                }
            }

            /* Do not merge unless we have done a Expand before (for complexity reasons) */
            if ((inst->w16_noExpand == 0) || ((frameSize < timestampsPerCall) && (cur_size
                > NETEQ_BUFSTAT_20MS_Q7)))
            {
                return BUFSTATS_DO_MERGE;
            }
            else if (playDtmf == 1)
            {
                /* play DTMF instead of expand */
                return BUFSTATS_DO_DTMF_ONLY;
            }
            else
            {
                return BUFSTATS_DO_EXPAND;
            }
        }
    }
    else
    { /* kPlayoutOff or kPlayoutFax */
        if (cngPacket)
        {
            if (((WebRtc_Word32) ((inst->uw32_CNGplayedTS + targetTS) - availableTS)) >= 0)
            {
                /* time to play this packet now */
                return BUFSTATS_DO_RFC3389CNG_PACKET;
            }
            else
            {
                /* wait before playing this packet */
                return BUFSTATS_DO_RFC3389CNG_NOPACKET;
            }
        }
        if (noPacket)
        {
            /*
             * No packet =>
             * 1. If in CNG mode play as usual
             * 2. Otherwise use other method to generate data and hold TS value
             */
            if (inst->w16_cngOn == CNG_RFC3389_ON)
            {
                /* keep on playing CNG */
                return BUFSTATS_DO_RFC3389CNG_NOPACKET;
            }
            else if (inst->w16_cngOn == CNG_INTERNAL_ON)
            {
                /* keep on playing internal CNG */
                return BUFSTATS_DO_INTERNAL_CNG_NOPACKET;
            }
            else
            {
                /* nothing to play => invent some data to play out */
                if (playoutMode == kPlayoutOff)
                {
                    return BUFSTATS_DO_ALTERNATIVE_PLC;
                }
                else if (playoutMode == kPlayoutFax)
                {
                    return BUFSTATS_DO_AUDIO_REPETITION;
                }
                else
                {
                    /* UNDEFINED, should not get here... */
                    assert(0);
                    return BUFSTAT_REINIT;
                }
            }
        }
        else if (targetTS == availableTS)
        {
            return BUFSTATS_DO_NORMAL;
        }
        else
        {
            if (((WebRtc_Word32) ((inst->uw32_CNGplayedTS + targetTS) - availableTS)) >= 0)
            {
                return BUFSTATS_DO_NORMAL;
            }
            else if (playoutMode == kPlayoutOff)
            {
                /*
                 * If currently playing CNG, continue with that. Don't increase TS
                 * since uw32_CNGplayedTS will be increased.
                 */
                if (inst->w16_cngOn == CNG_RFC3389_ON)
                {
                    return BUFSTATS_DO_RFC3389CNG_NOPACKET;
                }
                else if (inst->w16_cngOn == CNG_INTERNAL_ON)
                {
                    return BUFSTATS_DO_INTERNAL_CNG_NOPACKET;
                }
                else
                {
                    /*
                     * Otherwise, do PLC and increase TS while waiting for the time to
                     * play this packet.
                     */
                    return BUFSTATS_DO_ALTERNATIVE_PLC_INC_TS;
                }
            }
            else if (playoutMode == kPlayoutFax)
            {
                /*
                 * If currently playing CNG, continue with that don't increase TS since
                 * uw32_CNGplayedTS will be increased.
                 */
                if (inst->w16_cngOn == CNG_RFC3389_ON)
                {
                    return BUFSTATS_DO_RFC3389CNG_NOPACKET;
                }
                else if (inst->w16_cngOn == CNG_INTERNAL_ON)
                {
                    return BUFSTATS_DO_INTERNAL_CNG_NOPACKET;
                }
                else
                {
                    /*
                     * Otherwise, do audio repetition and increase TS while waiting for the
                     * time to play this packet.
                     */
                    return BUFSTATS_DO_AUDIO_REPETITION_INC_TS;
                }
            }
            else
            {
                /* UNDEFINED, should not get here... */
                assert(0);
                return BUFSTAT_REINIT;
            }
        }
    }
    /* We should not get here (but sometimes we do anyway...) */
    return BUFSTAT_REINIT;
}

