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
 * RTCP statistics reporting.
 */

#ifndef RTCP_H
#define RTCP_H

#include "typedefs.h"

typedef struct
{
    WebRtc_UWord16 cycles; /* The number of wrap-arounds for the sequence number */
    WebRtc_UWord16 max_seq; /* The maximum sequence number received
     (starts from 0 again after wrap around) */
    WebRtc_UWord16 base_seq; /* The sequence number of the first packet that arrived */
    WebRtc_UWord32 received; /* The number of packets that has been received */
    WebRtc_UWord32 rec_prior; /* Number of packets received when last report was generated */
    WebRtc_UWord32 exp_prior; /* Number of packets that should have been received if no
     packets were lost. Stored value from last report. */
    WebRtc_UWord32 jitter; /* Jitter statistics at this instance (calculated according to RFC) */
    WebRtc_Word32 transit; /* Clock difference for previous packet (RTPtimestamp - LOCALtime_rec) */
} WebRtcNetEQ_RTCP_t;

/****************************************************************************
 * WebRtcNetEQ_RTCPInit(...)
 *
 * This function calculates the parameters that are needed for the RTCP 
 * report.
 *
 * Input:
 *		- RTCP_inst		: RTCP instance, that contains information about the 
 *						  packets that have been received etc.
 *		- seqNo			: Packet number of the first received frame.
 *
 * Return value			:  0 - Ok
 *						  -1 - Error
 */

int WebRtcNetEQ_RTCPInit(WebRtcNetEQ_RTCP_t *RTCP_inst, WebRtc_UWord16 uw16_seqNo);

/****************************************************************************
 * WebRtcNetEQ_RTCPUpdate(...)
 *
 * This function calculates the parameters that are needed for the RTCP 
 * report.
 *
 * Input:
 *		- RTCP_inst		: RTCP instance, that contains information about the 
 *						  packets that have been received etc.
 *		- seqNo			: Packet number of the first received frame.
 *		- timeStamp		: Time stamp from the RTP header.
 *		- recTime		: Time (in RTP timestamps) when this packet was received.
 *
 * Return value			:  0 - Ok
 *						  -1 - Error
 */

int WebRtcNetEQ_RTCPUpdate(WebRtcNetEQ_RTCP_t *RTCP_inst, WebRtc_UWord16 uw16_seqNo,
                           WebRtc_UWord32 uw32_timeStamp, WebRtc_UWord32 uw32_recTime);

/****************************************************************************
 * WebRtcNetEQ_RTCPGetStats(...)
 *
 * This function calculates the parameters that are needed for the RTCP 
 * report.
 *
 * Input:
 *		- RTCP_inst		: RTCP instance, that contains information about the 
 *						  packets that have been received etc.
 *      - doNotReset    : If non-zero, the fraction lost statistics will not
 *                        be reset.
 *
 * Output:
 *		- RTCP_inst		: Updated RTCP information (some statistics are 
 *						  reset when generating this report)
 *		- fraction_lost : Number of lost RTP packets divided by the number of
 *						  expected packets, since the last RTCP Report.
 *		- cum_lost		: Cumulative number of lost packets during this 
 *						  session.
 *		- ext_max		: Extended highest sequence number received.
 *		- jitter		: Inter-arrival jitter.
 *
 * Return value			:  0 - Ok
 *						  -1 - Error
 */

int WebRtcNetEQ_RTCPGetStats(WebRtcNetEQ_RTCP_t *RTCP_inst,
                             WebRtc_UWord16 *puw16_fraction_lost,
                             WebRtc_UWord32 *puw32_cum_lost, WebRtc_UWord32 *puw32_ext_max,
                             WebRtc_UWord32 *puw32_jitter, WebRtc_Word16 doNotReset);

#endif
