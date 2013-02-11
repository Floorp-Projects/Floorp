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
 * Packet buffer for DTMF messages.
 */

#ifndef DTMF_BUFFER_H
#define DTMF_BUFFER_H

#include "typedefs.h"

#include "neteq_defines.h"

/* Include this code only if ATEVENT (DTMF) is defined in */
#ifdef NETEQ_ATEVENT_DECODE

#define MAX_DTMF_QUEUE_SIZE 4 

typedef struct dtmf_inst_t_
{
    WebRtc_Word16 MaxPLCtime;
    WebRtc_Word16 CurrentPLCtime;
    WebRtc_Word16 EventQueue[MAX_DTMF_QUEUE_SIZE];
    WebRtc_Word16 EventQueueVolume[MAX_DTMF_QUEUE_SIZE];
    WebRtc_Word16 EventQueueEnded[MAX_DTMF_QUEUE_SIZE];
    WebRtc_UWord32 EventQueueStartTime[MAX_DTMF_QUEUE_SIZE];
    WebRtc_UWord32 EventQueueEndTime[MAX_DTMF_QUEUE_SIZE];
    WebRtc_Word16 EventBufferSize;
    WebRtc_Word16 framelen;
} dtmf_inst_t;

/****************************************************************************
 * WebRtcNetEQ_DtmfDecoderInit(...)
 *
 * This function initializes a DTMF instance.
 *
 * Input:
 *      - DTMF_decinst_t    : DTMF instance
 *      - fs                : The sample rate used for the DTMF
 *      - MaxPLCtime        : Maximum length for a PLC before zeros should be inserted
 *
 * Return value             :  0 - Ok
 *                            -1 - Error
 */

WebRtc_Word16 WebRtcNetEQ_DtmfDecoderInit(dtmf_inst_t *DTMFdec_inst, WebRtc_UWord16 fs,
                                          WebRtc_Word16 MaxPLCtime);

/****************************************************************************
 * WebRtcNetEQ_DtmfInsertEvent(...)
 *
 * This function decodes a packet with DTMF frames.
 *
 * Input:
 *      - DTMFdec_inst      : DTMF instance
 *      - encoded           : Encoded DTMF frame(s)
 *      - len               : Bytes in encoded vector
 *
 *
 * Return value             :  0 - Ok
 *                            -1 - Error
 */

WebRtc_Word16 WebRtcNetEQ_DtmfInsertEvent(dtmf_inst_t *DTMFdec_inst,
                                          const WebRtc_Word16 *encoded, WebRtc_Word16 len,
                                          WebRtc_UWord32 timeStamp);

/****************************************************************************
 * WebRtcNetEQ_DtmfDecode(...)
 *
 * This function decodes a packet with DTMF frame(s). Output will be the
 * event that should be played for next 10 ms. 
 *
 * Input:
 *      - DTMFdec_inst      : DTMF instance
 *      - currTimeStamp     : The current playout timestamp
 *
 * Output:
 *      - event             : Event number to be played
 *      - volume            : Event volume to be played
 *
 * Return value             : >0 - There is a event to be played
 *                             0 - No event to be played
 *                            -1 - Error
 */

WebRtc_Word16 WebRtcNetEQ_DtmfDecode(dtmf_inst_t *DTMFdec_inst, WebRtc_Word16 *event,
                                     WebRtc_Word16 *volume, WebRtc_UWord32 currTimeStamp);

#endif    /* NETEQ_ATEVENT_DECODE */

#endif    /* DTMF_BUFFER_H */

