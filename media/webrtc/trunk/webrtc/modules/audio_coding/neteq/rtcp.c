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
 * Implementation of RTCP statistics reporting.
 */

#include "rtcp.h"

#include <string.h>

#include "signal_processing_library.h"

int WebRtcNetEQ_RTCPInit(WebRtcNetEQ_RTCP_t *RTCP_inst, uint16_t uw16_seqNo)
{
    /*
     * Initialize everything to zero and then set the start values for the RTP packet stream.
     */
    WebRtcSpl_MemSetW16((int16_t*) RTCP_inst, 0,
        sizeof(WebRtcNetEQ_RTCP_t) / sizeof(int16_t));
    RTCP_inst->base_seq = uw16_seqNo;
    RTCP_inst->max_seq = uw16_seqNo;
    return 0;
}

int WebRtcNetEQ_RTCPUpdate(WebRtcNetEQ_RTCP_t *RTCP_inst, uint16_t uw16_seqNo,
                           uint32_t uw32_timeStamp, uint32_t uw32_recTime)
{
    int16_t w16_SeqDiff;
    int32_t w32_TimeDiff;
    int32_t w32_JitterDiff;

    /*
     * Update number of received packets, and largest packet number received.
     */
    RTCP_inst->received++;
    w16_SeqDiff = uw16_seqNo - RTCP_inst->max_seq;
    if (w16_SeqDiff >= 0)
    {
        if (uw16_seqNo < RTCP_inst->max_seq)
        {
            /* Wrap around detected */
            RTCP_inst->cycles++;
        }
        RTCP_inst->max_seq = uw16_seqNo;
    }

    /* Calculate Jitter, and update previous timestamps */
    /* Note that the value in RTCP_inst->jitter is in Q4. */
    if (RTCP_inst->received > 1)
    {
        w32_TimeDiff = (uw32_recTime - (uw32_timeStamp - RTCP_inst->transit));
        w32_TimeDiff = WEBRTC_SPL_ABS_W32(w32_TimeDiff);
        w32_JitterDiff = WEBRTC_SPL_LSHIFT_W16(w32_TimeDiff, 4) - RTCP_inst->jitter;
        RTCP_inst->jitter = RTCP_inst->jitter + WEBRTC_SPL_RSHIFT_W32((w32_JitterDiff + 8), 4);
    }
    RTCP_inst->transit = (uw32_timeStamp - uw32_recTime);
    return 0;
}

int WebRtcNetEQ_RTCPGetStats(WebRtcNetEQ_RTCP_t *RTCP_inst,
                             uint16_t *puw16_fraction_lost,
                             uint32_t *puw32_cum_lost, uint32_t *puw32_ext_max,
                             uint32_t *puw32_jitter, int16_t doNotReset)
{
    uint32_t uw32_exp_nr, uw32_exp_interval, uw32_rec_interval;
    int32_t w32_lost;

    /* Extended highest sequence number received */
    *puw32_ext_max
        = (uint32_t) WEBRTC_SPL_LSHIFT_W32((uint32_t)RTCP_inst->cycles, 16)
            + RTCP_inst->max_seq;

    /*
     * Calculate expected number of packets and compare it to the number of packets that
     * were actually received => the cumulative number of packets lost can be extracted.
     */
    uw32_exp_nr = *puw32_ext_max - RTCP_inst->base_seq + 1;
    if (RTCP_inst->received == 0)
    {
        /* no packets received, assume none lost */
        *puw32_cum_lost = 0;
    }
    else if (uw32_exp_nr > RTCP_inst->received)
    {
        *puw32_cum_lost = uw32_exp_nr - RTCP_inst->received;
        if (*puw32_cum_lost > (uint32_t) 0xFFFFFF)
        {
            *puw32_cum_lost = 0xFFFFFF;
        }
    }
    else
    {
        *puw32_cum_lost = 0;
    }

    /* Fraction lost (Since last report) */
    uw32_exp_interval = uw32_exp_nr - RTCP_inst->exp_prior;
    if (!doNotReset)
    {
        RTCP_inst->exp_prior = uw32_exp_nr;
    }
    uw32_rec_interval = RTCP_inst->received - RTCP_inst->rec_prior;
    if (!doNotReset)
    {
        RTCP_inst->rec_prior = RTCP_inst->received;
    }
    w32_lost = (int32_t) (uw32_exp_interval - uw32_rec_interval);
    if (uw32_exp_interval == 0 || w32_lost <= 0 || RTCP_inst->received == 0)
    {
        *puw16_fraction_lost = 0;
    }
    else
    {
        *puw16_fraction_lost = (uint16_t) (WEBRTC_SPL_LSHIFT_W32(w32_lost, 8)
            / uw32_exp_interval);
    }
    if (*puw16_fraction_lost > 0xFF)
    {
        *puw16_fraction_lost = 0xFF;
    }

    /* Inter-arrival jitter */
    *puw32_jitter = (RTCP_inst->jitter) >> 4; /* scaling from Q4 */
    return 0;
}

