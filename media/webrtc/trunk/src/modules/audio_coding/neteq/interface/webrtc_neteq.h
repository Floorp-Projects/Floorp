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
    kDecoderILBC,
    kDecoderISAC,
    kDecoderISACswb,
    kDecoderPCM16B,
    kDecoderPCM16Bwb,
    kDecoderPCM16Bswb32kHz,
    kDecoderPCM16Bswb48kHz,
    kDecoderG722,
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
    kDecoderSPEEX_8,
    kDecoderSPEEX_16,
    kDecoderCELT_32,
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

typedef WebRtc_Word16 (*WebRtcNetEQ_FuncDecode)(void* state, WebRtc_Word16* encoded,
                                                WebRtc_Word16 len, WebRtc_Word16* decoded,
                                                WebRtc_Word16* speechType);
typedef WebRtc_Word16 (*WebRtcNetEQ_FuncDecodePLC)(void* state, WebRtc_Word16* decoded,
                                                   WebRtc_Word16 frames);
typedef WebRtc_Word16 (*WebRtcNetEQ_FuncDecodeInit)(void* state);
typedef WebRtc_Word16 (*WebRtcNetEQ_FuncAddLatePkt)(void* state, WebRtc_Word16* encoded,
                                                    WebRtc_Word16 len);
typedef WebRtc_Word16 (*WebRtcNetEQ_FuncGetMDinfo)(void* state);
typedef WebRtc_Word16 (*WebRtcNetEQ_FuncGetPitchInfo)(void* state, WebRtc_Word16* encoded,
                                                      WebRtc_Word16* length);
typedef WebRtc_Word16 (*WebRtcNetEQ_FuncUpdBWEst)(void* state, const WebRtc_UWord16 *encoded,
                                                  WebRtc_Word32 packet_size,
                                                  WebRtc_UWord16 rtp_seq_number,
                                                  WebRtc_UWord32 send_ts,
                                                  WebRtc_UWord32 arr_ts);
typedef WebRtc_Word16 (*WebRtcNetEQ_FuncGetErrorCode)(void* state);

/**********************************************************
 * Structures
 */

typedef struct
{
    enum WebRtcNetEQDecoder codec;
    WebRtc_Word16 payloadType;
    WebRtcNetEQ_FuncDecode funcDecode;
    WebRtcNetEQ_FuncDecode funcDecodeRCU;
    WebRtcNetEQ_FuncDecodePLC funcDecodePLC;
    WebRtcNetEQ_FuncDecodeInit funcDecodeInit;
    WebRtcNetEQ_FuncAddLatePkt funcAddLatePkt;
    WebRtcNetEQ_FuncGetMDinfo funcGetMDinfo;
    WebRtcNetEQ_FuncGetPitchInfo funcGetPitch;
    WebRtcNetEQ_FuncUpdBWEst funcUpdBWEst;
    WebRtcNetEQ_FuncGetErrorCode funcGetErrorCode;
    void* codec_state;
    WebRtc_UWord16 codec_fs;
} WebRtcNetEQ_CodecDef;

typedef struct
{
    WebRtc_UWord16 fraction_lost;
    WebRtc_UWord32 cum_lost;
    WebRtc_UWord32 ext_max;
    WebRtc_UWord32 jitter;
} WebRtcNetEQ_RTCPStat;

/**********************************************************
 * NETEQ Functions
 */

/* Info functions */

#define WEBRTC_NETEQ_MAX_ERROR_NAME 40
int WebRtcNetEQ_GetVersion(char *version);
int WebRtcNetEQ_GetErrorCode(void *inst);
int WebRtcNetEQ_GetErrorName(int errorCode, char *errorName, int maxStrLen);

/* Instance memory assign functions */

int WebRtcNetEQ_AssignSize(int *sizeinbytes);
int WebRtcNetEQ_Assign(void **inst, void *NETEQ_inst_Addr);
int WebRtcNetEQ_GetRecommendedBufferSize(void *inst, const enum WebRtcNetEQDecoder *codec,
                                         int noOfCodecs, enum WebRtcNetEQNetworkType nwType,
                                         int *MaxNoOfPackets, int *sizeinbytes);
int WebRtcNetEQ_AssignBuffer(void *inst, int MaxNoOfPackets, void *NETEQ_Buffer_Addr,
                             int sizeinbytes);

/* Init functions */

int WebRtcNetEQ_Init(void *inst, WebRtc_UWord16 fs);
int WebRtcNetEQ_SetAVTPlayout(void *inst, int PlayoutAVTon);
int WebRtcNetEQ_SetExtraDelay(void *inst, int DelayInMs);
int WebRtcNetEQ_SetPlayoutMode(void *inst, enum WebRtcNetEQPlayoutMode playoutMode);
int WebRtcNetEQ_SetBGNMode(void *inst, enum WebRtcNetEQBGNMode bgnMode);
int WebRtcNetEQ_GetBGNMode(const void *inst, enum WebRtcNetEQBGNMode *bgnMode);

/* Codec Database functions */

int WebRtcNetEQ_CodecDbReset(void *inst);
int WebRtcNetEQ_CodecDbAdd(void *inst, WebRtcNetEQ_CodecDef *codecInst);
int WebRtcNetEQ_CodecDbRemove(void *inst, enum WebRtcNetEQDecoder codec);
int WebRtcNetEQ_CodecDbGetSizeInfo(void *inst, WebRtc_Word16 *UsedEntries,
                                   WebRtc_Word16 *MaxEntries);
int WebRtcNetEQ_CodecDbGetCodecInfo(void *inst, WebRtc_Word16 Entry,
                                    enum WebRtcNetEQDecoder *codec);

/* Real-time functions */

int WebRtcNetEQ_RecIn(void *inst, WebRtc_Word16 *p_w16datagramstart, WebRtc_Word16 w16_RTPlen,
                      WebRtc_UWord32 uw32_timeRec);
int WebRtcNetEQ_RecOut(void *inst, WebRtc_Word16 *pw16_outData, WebRtc_Word16 *pw16_len);
int WebRtcNetEQ_GetRTCPStats(void *inst, WebRtcNetEQ_RTCPStat *RTCP_inst);
int WebRtcNetEQ_GetRTCPStatsNoReset(void *inst, WebRtcNetEQ_RTCPStat *RTCP_inst);
int WebRtcNetEQ_GetSpeechTimeStamp(void *inst, WebRtc_UWord32 *timestamp);
int WebRtcNetEQ_GetSpeechOutputType(void *inst, enum WebRtcNetEQOutputType *outputType);

/* VQmon related functions */
int WebRtcNetEQ_VQmonRecOutStatistics(void *inst, WebRtc_UWord16 *validVoiceDurationMs,
                                      WebRtc_UWord16 *concealedVoiceDurationMs,
                                      WebRtc_UWord8 *concealedVoiceFlags);
int WebRtcNetEQ_VQmonGetConfiguration(void *inst, WebRtc_UWord16 *absMaxDelayMs,
                                      WebRtc_UWord8 *adaptationRate);
int WebRtcNetEQ_VQmonGetRxStatistics(void *inst, WebRtc_UWord16 *avgDelayMs,
                                     WebRtc_UWord16 *maxDelayMs);

#ifdef __cplusplus
}
#endif

#endif
