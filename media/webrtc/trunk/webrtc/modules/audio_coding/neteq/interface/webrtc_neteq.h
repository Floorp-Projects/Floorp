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
 * This is the main API for NetEQ. Helper macros are located in webrtc_neteq_help_macros.h,
 * while some internal API functions are found in webrtc_neteq_internal.h.
 */

#include "typedefs.h"

#ifndef WEBRTC_NETEQ_H
#define WEBRTC_NETEQ_H

#ifdef __cplusplus 
extern "C"
{
#endif

/**********************************************************
 * Definitions
 */

enum WebRtcNetEQDecoder
{
    kDecoderReservedStart,
    kDecoderPCMu,
    kDecoderPCMa,
    kDecoderPCMu_2ch,
    kDecoderPCMa_2ch,
    kDecoderILBC,
    kDecoderISAC,
    kDecoderISACswb,
    kDecoderISACfb,
    kDecoderPCM16B,
    kDecoderPCM16Bwb,
    kDecoderPCM16Bswb32kHz,
    kDecoderPCM16Bswb48kHz,
    kDecoderPCM16B_2ch,
    kDecoderPCM16Bwb_2ch,
    kDecoderPCM16Bswb32kHz_2ch,
    kDecoderG722,
    kDecoderG722_2ch,
    kDecoderRED,
    kDecoderAVT,
    kDecoderCNG,
    kDecoderArbitrary,
    kDecoderG729,
    kDecoderG729_1,
    kDecoderG726_16,
    kDecoderG726_24,
    kDecoderG726_32,
    kDecoderG726_40,
    kDecoderG722_1_16,
    kDecoderG722_1_24,
    kDecoderG722_1_32,
    kDecoderG722_1C_24,
    kDecoderG722_1C_32,
    kDecoderG722_1C_48,
    kDecoderOpus,
    kDecoderSPEEX_8,
    kDecoderSPEEX_16,
    kDecoderCELT_32,
    kDecoderCELT_32_2ch,
    kDecoderGSMFR,
    kDecoderAMR,
    kDecoderAMRWB,
    kDecoderReservedEnd
};

enum WebRtcNetEQNetworkType
{
    kUDPNormal,
    kUDPVideoSync,
    kTCPNormal,
    kTCPLargeJitter,
    kTCPXLargeJitter
};

enum WebRtcNetEQOutputType
{
    kOutputNormal,
    kOutputPLC,
    kOutputCNG,
    kOutputPLCtoCNG,
    kOutputVADPassive
};

enum WebRtcNetEQPlayoutMode
{
    kPlayoutOn, kPlayoutOff, kPlayoutFax, kPlayoutStreaming
};

/* Available modes for background noise (inserted after long expands) */
enum WebRtcNetEQBGNMode
{
    kBGNOn, /* default "normal" behavior with eternal noise */
    kBGNFade, /* noise fades to zero after some time */
    kBGNOff
/* background noise is always zero */
};

/*************************************************
 * Definitions of decoder calls and the default 
 * API function calls for each codec
 */

typedef int16_t (*WebRtcNetEQ_FuncDecode)(void* state, int16_t* encoded,
                                                int16_t len, int16_t* decoded,
                                                int16_t* speechType);
typedef int16_t (*WebRtcNetEQ_FuncDecodePLC)(void* state, int16_t* decoded,
                                                   int16_t frames);
typedef int16_t (*WebRtcNetEQ_FuncDecodeInit)(void* state);
typedef int16_t (*WebRtcNetEQ_FuncAddLatePkt)(void* state, int16_t* encoded,
                                                    int16_t len);
typedef int16_t (*WebRtcNetEQ_FuncGetMDinfo)(void* state);
typedef int16_t (*WebRtcNetEQ_FuncGetPitchInfo)(void* state, int16_t* encoded,
                                                      int16_t* length);
typedef int16_t (*WebRtcNetEQ_FuncUpdBWEst)(void* state, const uint16_t *encoded,
                                                  int32_t packet_size,
                                                  uint16_t rtp_seq_number,
                                                  uint32_t send_ts,
                                                  uint32_t arr_ts);
typedef int (*WebRtcNetEQ_FuncDurationEst)(void* state, const uint8_t* payload,
                                           int payload_length_bytes);
typedef int16_t (*WebRtcNetEQ_FuncGetErrorCode)(void* state);

/**********************************************************
 * Structures
 */

typedef struct
{
    enum WebRtcNetEQDecoder codec;
    int16_t payloadType;
    WebRtcNetEQ_FuncDecode funcDecode;
    WebRtcNetEQ_FuncDecode funcDecodeRCU;
    WebRtcNetEQ_FuncDecodePLC funcDecodePLC;
    WebRtcNetEQ_FuncDecodeInit funcDecodeInit;
    WebRtcNetEQ_FuncAddLatePkt funcAddLatePkt;
    WebRtcNetEQ_FuncGetMDinfo funcGetMDinfo;
    WebRtcNetEQ_FuncGetPitchInfo funcGetPitch;
    WebRtcNetEQ_FuncUpdBWEst funcUpdBWEst;
    WebRtcNetEQ_FuncDurationEst funcDurationEst;
    WebRtcNetEQ_FuncGetErrorCode funcGetErrorCode;
    void* codec_state;
    uint16_t codec_fs;
} WebRtcNetEQ_CodecDef;

typedef struct
{
    uint16_t fraction_lost;
    uint32_t cum_lost;
    uint32_t ext_max;
    uint32_t jitter;
} WebRtcNetEQ_RTCPStat;

/**********************************************************
 * NETEQ Functions
 */

/* Info functions */

#define WEBRTC_NETEQ_MAX_ERROR_NAME 40
int WebRtcNetEQ_GetErrorCode(void *inst);
int WebRtcNetEQ_GetErrorName(int errorCode, char *errorName, int maxStrLen);

/* Instance memory assign functions */

int WebRtcNetEQ_AssignSize(int *sizeinbytes);
int WebRtcNetEQ_Assign(void **inst, void *NETEQ_inst_Addr);
int WebRtcNetEQ_GetRecommendedBufferSize(void *inst, const enum WebRtcNetEQDecoder *codec,
                                         int noOfCodecs, enum WebRtcNetEQNetworkType nwType,
                                         int *MaxNoOfPackets, int *sizeinbytes,
                                         int* per_packet_overhead_bytes);
int WebRtcNetEQ_AssignBuffer(void *inst, int MaxNoOfPackets, void *NETEQ_Buffer_Addr,
                             int sizeinbytes);

/* Init functions */

int WebRtcNetEQ_Init(void *inst, uint16_t fs);
int WebRtcNetEQ_SetAVTPlayout(void *inst, int PlayoutAVTon);
int WebRtcNetEQ_SetExtraDelay(void *inst, int DelayInMs);
int WebRtcNetEQ_SetPlayoutMode(void *inst, enum WebRtcNetEQPlayoutMode playoutMode);
int WebRtcNetEQ_SetBGNMode(void *inst, enum WebRtcNetEQBGNMode bgnMode);
int WebRtcNetEQ_GetBGNMode(const void *inst, enum WebRtcNetEQBGNMode *bgnMode);

/* Codec Database functions */

int WebRtcNetEQ_CodecDbReset(void *inst);
int WebRtcNetEQ_CodecDbAdd(void *inst, WebRtcNetEQ_CodecDef *codecInst);
int WebRtcNetEQ_CodecDbRemove(void *inst, enum WebRtcNetEQDecoder codec);
int WebRtcNetEQ_CodecDbGetSizeInfo(void *inst, int16_t *UsedEntries,
                                   int16_t *MaxEntries);
int WebRtcNetEQ_CodecDbGetCodecInfo(void *inst, int16_t Entry,
                                    enum WebRtcNetEQDecoder *codec);

/* Real-time functions */

int WebRtcNetEQ_RecIn(void *inst, int16_t *p_w16datagramstart, int16_t w16_RTPlen,
                      uint32_t uw32_timeRec);
int WebRtcNetEQ_RecOut(void *inst, int16_t *pw16_outData, int16_t *pw16_len);
int WebRtcNetEQ_GetRTCPStats(void *inst, WebRtcNetEQ_RTCPStat *RTCP_inst);
int WebRtcNetEQ_GetRTCPStatsNoReset(void *inst, WebRtcNetEQ_RTCPStat *RTCP_inst);
int WebRtcNetEQ_GetSpeechTimeStamp(void *inst, uint32_t *timestamp);
int WebRtcNetEQ_GetSpeechOutputType(void *inst, enum WebRtcNetEQOutputType *outputType);

/* VQmon related functions */
int WebRtcNetEQ_VQmonRecOutStatistics(void *inst, uint16_t *validVoiceDurationMs,
                                      uint16_t *concealedVoiceDurationMs,
                                      uint8_t *concealedVoiceFlags);
int WebRtcNetEQ_VQmonGetConfiguration(void *inst, uint16_t *absMaxDelayMs,
                                      uint8_t *adaptationRate);
int WebRtcNetEQ_VQmonGetRxStatistics(void *inst, uint16_t *avgDelayMs,
                                     uint16_t *maxDelayMs);

#ifdef __cplusplus
}
#endif

#endif
