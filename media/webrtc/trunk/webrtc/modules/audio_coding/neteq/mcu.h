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
 * MCU struct and functions related to the MCU side operations.
 */

#ifndef MCU_H
#define MCU_H

#include "typedefs.h"

#include "codec_db.h"
#include "rtcp.h"
#include "packet_buffer.h"
#include "buffer_stats.h"
#include "neteq_statistics.h"

#ifdef NETEQ_ATEVENT_DECODE
#include "dtmf_buffer.h"
#endif

#define MAX_ONE_DESC 5 /* cannot do more than this many consecutive one-descriptor decodings */
#define MAX_LOSS_REPORT_PERIOD 60   /* number of seconds between auto-reset */

enum TsScaling
{
    kTSnoScaling = 0,
    kTSscalingTwo,
    kTSscalingTwoThirds,
    kTSscalingFourThirds
};

enum { kLenWaitingTimes = 100 };

typedef struct
{

    int16_t current_Codec;
    int16_t current_Payload;
    uint32_t timeStamp; /* Next timestamp that should be played */
    int16_t millisecondsPerCall;
    uint16_t timestampsPerCall; /* Output chunk size */
    uint16_t fs;
    uint32_t ssrc; /* Current ssrc */
    int16_t new_codec;
    int16_t first_packet;

    /* MCU/DSP Communication layer */
    int16_t *pw16_readAddress;
    int16_t *pw16_writeAddress;
    void *main_inst;

    CodecDbInst_t codec_DB_inst; /* Information about all the codecs, i.e. which
     functions to use and which codpoints that
     have been assigned */
    SplitInfo_t PayloadSplit_inst; /* Information about how the current codec
     payload should be splitted */
    WebRtcNetEQ_RTCP_t RTCP_inst; /* RTCP statistics */
    PacketBuf_t PacketBuffer_inst; /* The packet buffer */
    BufstatsInst_t BufferStat_inst; /* Statistics that are used to make decision
     for what the DSP should perform */
#ifdef NETEQ_ATEVENT_DECODE
    dtmf_inst_t DTMF_inst;
#endif
    int NoOfExpandCalls;
    int16_t AVT_PlayoutOn;
    enum WebRtcNetEQPlayoutMode NetEqPlayoutMode;

    int16_t one_desc; /* Number of times running on one desc */

    uint32_t lostTS; /* Number of timestamps lost */
    uint32_t lastReportTS; /* Timestamp elapsed since last report was given */

    int waiting_times[kLenWaitingTimes];  /* Waiting time statistics storage. */
    int len_waiting_times;
    int next_waiting_time_index;

    uint32_t externalTS;
    uint32_t internalTS;
    int16_t TSscalingInitialized;
    enum TsScaling scalingFactor;

    /* AV-sync enabled. In AV-sync NetEq screens packets for specific sync
     * packets. Sync packets are not decoded by a decoder but generate all-zero
     * signal with the same number of samples as previously decoded payload.
     * Also in AV-sync mode the sample-size of a sync payload is reported as
     * previous frame-size. */
    int av_sync;

#ifdef NETEQ_STEREO
    int usingStereo;
#endif

    /* The sequence number of the latest decoded RTP payload. */
    int decoded_packet_sequence_number;
    uint32_t decoded_packet_timestamp;
} MCUInst_t;

/****************************************************************************
 * WebRtcNetEQ_McuReset(...)
 *
 * Reset the MCU instance.
 *
 * Input:
 *      - inst          : MCU instance
 *
 * Return value         :  0 - Ok
 *                        <0 - Error
 */
int WebRtcNetEQ_McuReset(MCUInst_t *inst);

/****************************************************************************
 * WebRtcNetEQ_ResetMcuInCallStats(...)
 *
 * Reset MCU-side statistics variables for the in-call statistics.
 *
 * Input:
 *      - inst          : MCU instance
 *
 * Return value         :  0 - Ok
 *                        <0 - Error
 */
int WebRtcNetEQ_ResetMcuInCallStats(MCUInst_t *inst);

/****************************************************************************
 * WebRtcNetEQ_ResetWaitingTimeStats(...)
 *
 * Reset waiting-time statistics.
 *
 * Input:
 *      - inst          : MCU instance.
 *
 * Return value         : n/a
 */
void WebRtcNetEQ_ResetWaitingTimeStats(MCUInst_t *inst);

/****************************************************************************
 * WebRtcNetEQ_LogWaitingTime(...)
 *
 * Log waiting-time to the statistics.
 *
 * Input:
 *      - inst          : MCU instance.
 *      - waiting_time  : Waiting time in "RecOut calls" (i.e., 1 call = 10 ms).
 *
 * Return value         : n/a
 */
void WebRtcNetEQ_StoreWaitingTime(MCUInst_t *inst, int waiting_time);

/****************************************************************************
 * WebRtcNetEQ_ResetMcuJitterStat(...)
 *
 * Reset MCU-side statistics variables for the post-call statistics.
 *
 * Input:
 *      - inst          : MCU instance
 *
 * Return value         :  0 - Ok
 *                        <0 - Error
 */
int WebRtcNetEQ_ResetMcuJitterStat(MCUInst_t *inst);

/****************************************************************************
 * WebRtcNetEQ_McuAddressInit(...)
 *
 * Initializes MCU with read address and write address.
 *
 * Input:
 *      - inst              : MCU instance
 *      - Data2McuAddress   : Pointer to MCU address
 *      - Data2DspAddress   : Pointer to DSP address
 *      - main_inst         : Pointer to NetEQ main instance
 *
 * Return value         :  0 - Ok
 *                        <0 - Error
 */
int WebRtcNetEQ_McuAddressInit(MCUInst_t *inst, void * Data2McuAddress,
                               void * Data2DspAddress, void *main_inst);

/****************************************************************************
 * WebRtcNetEQ_McuSetFs(...)
 *
 * Initializes MCU with read address and write address.
 *
 * Input:
 *      - inst          : MCU instance
 *      - fs_hz         : Sample rate in Hz -- 8000, 16000, 32000, (48000)
 *
 * Return value         :  0 - Ok
 *                        <0 - Error
 */
int WebRtcNetEQ_McuSetFs(MCUInst_t *inst, uint16_t fs_hz);

/****************************************************************************
 * WebRtcNetEQ_SignalMcu(...)
 *
 * Signal the MCU that data is available and ask for a RecOut decision.
 *
 * Input:
 *      - inst          : MCU instance
 *      - av_sync       : 1 if NetEQ is in AV-sync mode, otherwise 0.
 *
 * Return value         :  0 - Ok
 *                        <0 - Error
 */
int WebRtcNetEQ_SignalMcu(MCUInst_t *inst);

/****************************************************************************
 * WebRtcNetEQ_RecInInternal(...)
 *
 * This function inserts a packet into the jitter buffer.
 *
 * Input:
 *		- MCU_inst		: MCU instance
 *		- RTPpacket	    : The RTP packet, parsed into NetEQ's internal RTP struct
 *		- uw32_timeRec	: Time stamp for the arrival of the packet (not RTP timestamp)
 *
 * Return value			:  0 - Ok
 *						  -1 - Error
 */

int WebRtcNetEQ_RecInInternal(MCUInst_t *MCU_inst, RTPPacket_t *RTPpacket,
                              uint32_t uw32_timeRec);

/****************************************************************************
 * WebRtcNetEQ_RecInInternal(...)
 *
 * Split the packet according to split_inst and inserts the parts into
 * Buffer_inst.
 *
 * Input:
 *      - MCU_inst      : MCU instance
 *      - RTPpacket     : The RTP packet, parsed into NetEQ's internal RTP struct
 *      - uw32_timeRec  : Time stamp for the arrival of the packet (not RTP timestamp)
 *      - av_sync       : indicates if AV-sync is enabled, 1 enabled,
 *                        0 disabled.
 *
 * Return value         :  0 - Ok
 *                        -1 - Error
 */
int WebRtcNetEQ_SplitAndInsertPayload(RTPPacket_t* packet,
                                      PacketBuf_t* Buffer_inst,
                                      SplitInfo_t* split_inst,
                                      int16_t* flushed,
                                      int av_sync);

/****************************************************************************
 * WebRtcNetEQ_GetTimestampScaling(...)
 *
 * Update information about timestamp scaling for a payload type
 * in MCU_inst->scalingFactor.
 *
 * Input:
 *      - MCU_inst          : MCU instance
 *      - rtpPayloadType    : RTP payload number
 *
 * Return value             :  0 - Ok
 *                            -1 - Error
 */

int WebRtcNetEQ_GetTimestampScaling(MCUInst_t *MCU_inst, int rtpPayloadType);

/****************************************************************************
 * WebRtcNetEQ_ScaleTimestampExternalToInternal(...)
 *
 * Convert from external to internal timestamp using current scaling info.
 *
 * Input:
 *      - MCU_inst      : MCU instance
 *      - externalTS    : External timestamp
 *
 * Return value         : Internal timestamp
 */

uint32_t WebRtcNetEQ_ScaleTimestampExternalToInternal(const MCUInst_t *MCU_inst,
                                                      uint32_t externalTS);

/****************************************************************************
 * WebRtcNetEQ_ScaleTimestampInternalToExternal(...)
 *
 * Convert from external to internal timestamp using current scaling info.
 *
 * Input:
 *      - MCU_inst      : MCU instance
 *      - externalTS    : Internal timestamp
 *
 * Return value         : External timestamp
 */

uint32_t WebRtcNetEQ_ScaleTimestampInternalToExternal(const MCUInst_t *MCU_inst,
                                                      uint32_t internalTS);
#endif
