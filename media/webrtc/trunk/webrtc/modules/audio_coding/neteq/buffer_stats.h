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
 * Calculates and stores the packet buffer statistics.
 */

#ifndef BUFFER_STATS_H
#define BUFFER_STATS_H

#include "automode.h"
#include "webrtc_neteq.h" /* to define enum WebRtcNetEQPlayoutMode */

/* NetEQ related decisions */
#define BUFSTATS_DO_NORMAL					0
#define BUFSTATS_DO_ACCELERATE				1
#define BUFSTATS_DO_MERGE					2
#define BUFSTATS_DO_EXPAND					3
#define BUFSTAT_REINIT						4
#define BUFSTATS_DO_RFC3389CNG_PACKET		5
#define BUFSTATS_DO_RFC3389CNG_NOPACKET		6
#define BUFSTATS_DO_INTERNAL_CNG_NOPACKET	7
#define BUFSTATS_DO_PREEMPTIVE_EXPAND		8
#define BUFSTAT_REINIT_DECODER              9
#define BUFSTATS_DO_DTMF_ONLY               10
/* Decisions related to when NetEQ is switched off (or in FAX mode) */
#define BUFSTATS_DO_ALTERNATIVE_PLC				   11
#define BUFSTATS_DO_ALTERNATIVE_PLC_INC_TS		   12
#define BUFSTATS_DO_AUDIO_REPETITION			   13
#define BUFSTATS_DO_AUDIO_REPETITION_INC_TS		   14

/* Reinit decoder states after this number of expands (upon arrival of new packet) */
#define REINIT_AFTER_EXPANDS 100

/* Wait no longer than this number of RecOut calls before using an "early" packet */
#define MAX_WAIT_FOR_PACKET 10

/* CNG modes */
#define CNG_OFF 0
#define CNG_RFC3389_ON 1
#define CNG_INTERNAL_ON 2

typedef struct
{

    /* store statistical data here */
    int16_t w16_cngOn; /* remember if CNG is interrupted by other event (e.g. DTMF) */
    int16_t w16_noExpand;
    int32_t uw32_CNGplayedTS;

    /* VQmon data */
    uint16_t avgDelayMsQ8;
    int16_t maxDelayMs;

    AutomodeInst_t Automode_inst;

} BufstatsInst_t;

/****************************************************************************
 * WebRtcNetEQ_BufstatsDecision()
 *
 * Gives a decision about what action that is currently desired 
 *
 *
 *	Input:
 *		inst:			    The bufstat instance
 *		cur_size:		    Current buffer size in ms in Q3 domain
 *		targetTS:		    The desired timestamp to start playout from
 *		availableTS:	    The closest future value available in buffer
 *		noPacket		    1 if no packet is available, makes availableTS undefined
 *		prevPlayMode	    mode of last NetEq playout
 *		timestampsPerCall	number of timestamp for 10ms
 *
 *	Output:
 *		Returns:		    A decision, as defined above (see top of file)
 *
 */

uint16_t WebRtcNetEQ_BufstatsDecision(BufstatsInst_t *inst, int16_t frameSize,
                                      int32_t cur_size, uint32_t targetTS,
                                      uint32_t availableTS, int noPacket,
                                      int cngPacket, int prevPlayMode,
                                      enum WebRtcNetEQPlayoutMode playoutMode,
                                      int timestampsPerCall, int NoOfExpandCalls,
                                      int16_t fs_mult,
                                      int16_t lastModeBGNonly, int playDtmf);

#endif
