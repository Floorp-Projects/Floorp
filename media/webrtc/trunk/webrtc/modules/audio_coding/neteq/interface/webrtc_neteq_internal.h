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
 * This file contains the internal API functions.
 */

#include "typedefs.h"

#ifndef WEBRTC_NETEQ_INTERNAL_H
#define WEBRTC_NETEQ_INTERNAL_H

#ifdef __cplusplus 
extern "C"
{
#endif

typedef struct
{
    uint8_t payloadType;
    uint16_t sequenceNumber;
    uint32_t timeStamp;
    uint32_t SSRC;
    uint8_t markerBit;
} WebRtcNetEQ_RTPInfo;

/****************************************************************************
 * WebRtcNetEQ_RecInRTPStruct(...)
 *
 * Alternative RecIn function, used when the RTP data has already been
 * parsed into an RTP info struct (WebRtcNetEQ_RTPInfo).
 *
 * Input:
 *		- inst	            : NetEQ instance
 *		- rtpInfo		    : Pointer to RTP info
 *		- payloadPtr        : Pointer to the RTP payload (first byte after header)
 *      - payloadLenBytes   : Length (in bytes) of the payload in payloadPtr
 *      - timeRec           : Receive time (in timestamps of the used codec)
 *
 * Return value			    :  0 - Ok
 *                            -1 - Error
 */
int WebRtcNetEQ_RecInRTPStruct(void *inst, WebRtcNetEQ_RTPInfo *rtpInfo,
                               const uint8_t *payloadPtr, int16_t payloadLenBytes,
                               uint32_t timeRec);

/****************************************************************************
 * WebRtcNetEQ_GetMasterSlaveInfoSize(...)
 *
 * Get size in bytes for master/slave struct msInfo used in 
 * WebRtcNetEQ_RecOutMasterSlave.
 *
 * Return value			    :  Struct size in bytes
 * 
 */

int WebRtcNetEQ_GetMasterSlaveInfoSize();

/****************************************************************************
 * WebRtcNetEQ_RecOutMasterSlave(...)
 *
 * RecOut function for running several NetEQ instances in master/slave mode.
 * One master can be used to control several slaves. 
 * The MasterSlaveInfo struct must be allocated outside NetEQ.
 * Use function WebRtcNetEQ_GetMasterSlaveInfoSize to get the size needed.
 *
 * Input:
 *      - inst          : NetEQ instance
 *      - isMaster      : Non-zero indicates that this is the master channel
 *      - msInfo        : (slave only) Information from master
 *
 * Output:
 *		- inst	        : Updated NetEQ instance
 *      - pw16_outData  : Pointer to vector where output should be written
 *      - pw16_len      : Pointer to variable where output length is returned
 *      - msInfo        : (master only) Information to slave(s)
 *
 * Return value			:  0 - Ok
 *						  -1 - Error
 */

int WebRtcNetEQ_RecOutMasterSlave(void *inst, int16_t *pw16_outData,
                                  int16_t *pw16_len, void *msInfo,
                                  int16_t isMaster);

typedef struct
{
    uint16_t currentBufferSize;         /* Current jitter buffer size in ms. */
    uint16_t preferredBufferSize;       /* Preferred buffer size in ms. */
    uint16_t jitterPeaksFound;          /* 1 if adding extra delay due to peaky
                                         * jitter; 0 otherwise. */
    uint16_t currentPacketLossRate;     /* Loss rate (network + late) (Q14). */
    uint16_t currentDiscardRate;        /* Late loss rate (Q14). */
    uint16_t currentExpandRate;         /* Fraction (of original stream) of
                                         * synthesized speech inserted through
                                         * expansion (in Q14). */
    uint16_t currentPreemptiveRate;     /* Fraction of data inserted through
                                         * pre-emptive expansion (in Q14). */
    uint16_t currentAccelerateRate;     /* Fraction of data removed through
                                         * acceleration (in Q14). */
    int32_t clockDriftPPM;              /* Average clock-drift in parts-per-
                                         * million (positive or negative). */
    int addedSamples;                   /* Number of zero samples added in off
                                         * mode */
} WebRtcNetEQ_NetworkStatistics;

/*
 * Get the "in-call" statistics from NetEQ.
 * The statistics are reset after the query.
 */
int WebRtcNetEQ_GetNetworkStatistics(void *inst, WebRtcNetEQ_NetworkStatistics *stats);


typedef struct {
  /* Samples removed from background noise only segments. */
  int accelerate_bgn_samples;

  /* Samples removed from normal audio segments. */
  int accelerate_normal_samples;

  /* Number of samples synthesized during background noise only segments. */
  int expand_bgn_sampels;

  /* Number of samples synthesized during normal audio segments. */
  int expand_normal_samples;

  /* Number of samples synthesized during background noise only segments,
   * in preemptive mode. */
  int preemptive_expand_bgn_samples;

  /* Number of samples synthesized during normal audio segments, in preemptive
   * mode. */
  int preemptive_expand_normal_samples;

  /* Number of samples synthesized during background noise only segments,
   * while merging. */
  int merge_expand_bgn_samples;

  /* Number of samples synthesized during normal audio segments, while
   * merging. */
  int merge_expand_normal_samples;
} WebRtcNetEQ_ProcessingActivity;

/*
 * Get the processing activities from NetEQ.
 * The statistics are reset after the query.
 * This API is meant to obtain processing activities in high granularity,
 * e.g. per RecOut() call.
 */
void WebRtcNetEQ_GetProcessingActivity(void* inst,
                                       WebRtcNetEQ_ProcessingActivity* stat);

/*
 * Get the raw waiting times for decoded frames. The function writes the last
 * recorded waiting times (from frame arrival to frame decoding) to the memory
 * pointed to by waitingTimeMs. The number of elements written is in the return
 * value. No more than maxLength elements are written. Statistics are reset on
 * each query.
 */
int WebRtcNetEQ_GetRawFrameWaitingTimes(void *inst,
                                        int max_length,
                                        int* waiting_times_ms);

/***********************************************/
/* Functions for post-decode VAD functionality */
/***********************************************/

/* NetEQ must be compiled with the flag NETEQ_VAD enabled for these functions to work. */

/*
 * VAD function pointer types
 *
 * These function pointers match the definitions of webrtc VAD functions WebRtcVad_Init,
 * WebRtcVad_set_mode and WebRtcVad_Process, respectively, all found in webrtc_vad.h.
 */
typedef int (*WebRtcNetEQ_VADInitFunction)(void *VAD_inst);
typedef int (*WebRtcNetEQ_VADSetmodeFunction)(void *VAD_inst, int mode);
typedef int (*WebRtcNetEQ_VADFunction)(void *VAD_inst, int fs,
    int16_t *frame, int frameLen);

/****************************************************************************
 * WebRtcNetEQ_SetVADInstance(...)
 *
 * Provide a pointer to an allocated VAD instance. If function is never 
 * called or it is called with NULL pointer as VAD_inst, the post-decode
 * VAD functionality is disabled. Also provide pointers to init, setmode
 * and VAD functions. These are typically pointers to WebRtcVad_Init,
 * WebRtcVad_set_mode and WebRtcVad_Process, respectively, all found in the
 * interface file webrtc_vad.h.
 *
 * Input:
 *      - NetEQ_inst        : NetEQ instance
 *		- VADinst		    : VAD instance
 *		- initFunction	    : Pointer to VAD init function
 *		- setmodeFunction   : Pointer to VAD setmode function
 *		- VADfunction	    : Pointer to VAD function
 *
 * Output:
 *		- NetEQ_inst	    : Updated NetEQ instance
 *
 * Return value			    :  0 - Ok
 *						      -1 - Error
 */

int WebRtcNetEQ_SetVADInstance(void *NetEQ_inst, void *VAD_inst,
                               WebRtcNetEQ_VADInitFunction initFunction,
                               WebRtcNetEQ_VADSetmodeFunction setmodeFunction,
                               WebRtcNetEQ_VADFunction VADFunction);

/****************************************************************************
 * WebRtcNetEQ_SetVADMode(...)
 *
 * Pass an aggressiveness mode parameter to the post-decode VAD instance.
 * If this function is never called, mode 0 (quality mode) is used as default.
 *
 * Input:
 *      - inst          : NetEQ instance
 *		- mode  		: mode parameter (same range as WebRtc VAD mode)
 *
 * Output:
 *		- inst	        : Updated NetEQ instance
 *
 * Return value			:  0 - Ok
 *						  -1 - Error
 */

int WebRtcNetEQ_SetVADMode(void *NetEQ_inst, int mode);

/****************************************************************************
 * WebRtcNetEQ_RecOutNoDecode(...)
 *
 * Special RecOut that does not do any decoding.
 *
 * Input:
 *      - inst          : NetEQ instance
 *
 * Output:
 *		- inst	        : Updated NetEQ instance
 *      - pw16_outData  : Pointer to vector where output should be written
 *      - pw16_len      : Pointer to variable where output length is returned
 *
 * Return value			:  0 - Ok
 *						  -1 - Error
 */

int WebRtcNetEQ_RecOutNoDecode(void *inst, int16_t *pw16_outData,
                               int16_t *pw16_len);

/****************************************************************************
 * WebRtcNetEQ_FlushBuffers(...)
 *
 * Flush packet and speech buffers. Does not reset codec database or 
 * jitter statistics.
 *
 * Input:
 *      - inst          : NetEQ instance
 *
 * Output:
 *		- inst	        : Updated NetEQ instance
 *
 * Return value			:  0 - Ok
 *						  -1 - Error
 */

int WebRtcNetEQ_FlushBuffers(void *inst);

/*****************************************************************************
 * void WebRtcNetEq_EnableAVSync(...)
 *
 * Enable AV-sync. If Enabled, NetEq will screen for sync payloads. For
 * each sync payload a silence frame is generated.
 *
 * Input:
 *    - inst          : NetEQ instance
 *    - enable        : non-zero to enable, otherwise disabled.
 *
 * Output:
 *    - inst          : Updated NetEQ instance
 *
 */

void WebRtcNetEQ_EnableAVSync(void* inst, int enable);

/****************************************************************************
 * WebRtcNetEQ_RecInSyncRTP(...)
 *
 * Insert a sync packet with the given RTP specification.
 *
 * Input:
 *    - inst              : NetEQ instance
 *    - rtpInfo           : Pointer to RTP info
 *    - receive_timestamp : Receive time (in timestamps of the used codec)
 *
 * Output:
 *    - inst              : Updated NetEQ instance
 *
 * Return value           : if succeeded it returns the number of bytes pushed
 *                          in, otherwise returns -1.
 */

int WebRtcNetEQ_RecInSyncRTP(void* inst,
                             WebRtcNetEQ_RTPInfo* rtp_info,
                             uint32_t receive_timestamp);

#ifdef __cplusplus
}
#endif

#endif
