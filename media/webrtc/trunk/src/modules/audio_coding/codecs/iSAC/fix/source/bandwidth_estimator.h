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
 * bandwidth_estimator.h
 *
 * This header file contains the API for the Bandwidth Estimator
 * designed for iSAC.
 *
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_BANDWIDTH_ESTIMATOR_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_BANDWIDTH_ESTIMATOR_H_

#include "structs.h"


/****************************************************************************
 * WebRtcIsacfix_InitBandwidthEstimator(...)
 *
 * This function initializes the struct for the bandwidth estimator
 *
 * Input/Output:
 *      - bwest_str        : Struct containing bandwidth information.
 *
 * Return value            : 0
 */

WebRtc_Word32 WebRtcIsacfix_InitBandwidthEstimator(BwEstimatorstr *bwest_str);


/****************************************************************************
 * WebRtcIsacfix_UpdateUplinkBwImpl(...)
 *
 * This function updates bottle neck rate received from other side in payload
 * and calculates a new bottle neck to send to the other side.
 *
 * Input/Output:
 *      - bweStr           : struct containing bandwidth information.
 *      - rtpNumber        : value from RTP packet, from NetEq
 *      - frameSize        : length of signal frame in ms, from iSAC decoder
 *      - sendTime         : value in RTP header giving send time in samples
 *      - arrivalTime      : value given by timeGetTime() time of arrival in
 *                           samples of packet from NetEq
 *      - pksize           : size of packet in bytes, from NetEq
 *      - Index            : integer (range 0...23) indicating bottle neck &
 *                           jitter as estimated by other side
 *
 * Return value            : 0 if everything went fine,
 *                           -1 otherwise
 */

WebRtc_Word32 WebRtcIsacfix_UpdateUplinkBwImpl(BwEstimatorstr            *bwest_str,
                                               const WebRtc_UWord16        rtp_number,
                                               const WebRtc_Word16         frameSize,
                                               const WebRtc_UWord32    send_ts,
                                               const WebRtc_UWord32        arr_ts,
                                               const WebRtc_Word16         pksize,
                                               const WebRtc_UWord16        Index);

/* Update receiving estimates. Used when we only receive BWE index, no iSAC data packet. */
WebRtc_Word16 WebRtcIsacfix_UpdateUplinkBwRec(BwEstimatorstr *bwest_str,
                                              const WebRtc_Word16 Index);

/****************************************************************************
 * WebRtcIsacfix_GetDownlinkBwIndexImpl(...)
 *
 * This function calculates and returns the bandwidth/jitter estimation code
 * (integer 0...23) to put in the sending iSAC payload.
 *
 * Input:
 *      - bweStr       : BWE struct
 *
 * Return:
 *      bandwith and jitter index (0..23)
 */
WebRtc_UWord16 WebRtcIsacfix_GetDownlinkBwIndexImpl(BwEstimatorstr *bwest_str);

/* Returns the bandwidth estimation (in bps) */
WebRtc_UWord16 WebRtcIsacfix_GetDownlinkBandwidth(const BwEstimatorstr *bwest_str);

/* Returns the bandwidth that iSAC should send with in bps */
WebRtc_Word16 WebRtcIsacfix_GetUplinkBandwidth(const BwEstimatorstr *bwest_str);

/* Returns the max delay (in ms) */
WebRtc_Word16 WebRtcIsacfix_GetDownlinkMaxDelay(const BwEstimatorstr *bwest_str);

/* Returns the max delay value from the other side in ms */
WebRtc_Word16 WebRtcIsacfix_GetUplinkMaxDelay(const BwEstimatorstr *bwest_str);

/*
 * update amount of data in bottle neck buffer and burst handling
 * returns minimum payload size (bytes)
 */
WebRtc_UWord16 WebRtcIsacfix_GetMinBytes(RateModel *State,
                                         WebRtc_Word16 StreamSize,     /* bytes in bitstream */
                                         const WebRtc_Word16 FrameLen,    /* ms per frame */
                                         const WebRtc_Word16 BottleNeck,        /* bottle neck rate; excl headers (bps) */
                                         const WebRtc_Word16 DelayBuildUp);     /* max delay from bottle neck buffering (ms) */

/*
 * update long-term average bitrate and amount of data in buffer
 */
void WebRtcIsacfix_UpdateRateModel(RateModel *State,
                                   WebRtc_Word16 StreamSize,    /* bytes in bitstream */
                                   const WebRtc_Word16 FrameSamples,  /* samples per frame */
                                   const WebRtc_Word16 BottleNeck);       /* bottle neck rate; excl headers (bps) */


void WebRtcIsacfix_InitRateModel(RateModel *State);

/* Returns the new framelength value (input argument: bottle_neck) */
WebRtc_Word16 WebRtcIsacfix_GetNewFrameLength(WebRtc_Word16 bottle_neck, WebRtc_Word16 current_framelength);

/* Returns the new SNR value (input argument: bottle_neck) */
//returns snr in Q10
WebRtc_Word16 WebRtcIsacfix_GetSnr(WebRtc_Word16 bottle_neck, WebRtc_Word16 framesamples);


#endif /*  WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_BANDWIDTH_ESTIMATOR_H_ */
