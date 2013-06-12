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
 * Interface for the codec database.
 */

#ifndef CODEC_DB_H
#define CODEC_DB_H

#include "typedefs.h"

#include "webrtc_neteq.h"
#include "codec_db_defines.h"
#include "neteq_defines.h"

#if defined(NETEQ_48KHZ_WIDEBAND)
    #define NUM_CNG_CODECS 4
#elif defined(NETEQ_32KHZ_WIDEBAND)
    #define NUM_CNG_CODECS 3
#elif defined(NETEQ_WIDEBAND)
    #define NUM_CNG_CODECS 2
#else
    #define NUM_CNG_CODECS 1
#endif

typedef struct
{

    int16_t position[NUM_TOTAL_CODECS];
    int16_t nrOfCodecs;

    int16_t payloadType[NUM_CODECS];
    FuncDecode funcDecode[NUM_CODECS];
    FuncDecode funcDecodeRCU[NUM_CODECS];
    FuncDecodePLC funcDecodePLC[NUM_CODECS];
    FuncDecodeInit funcDecodeInit[NUM_CODECS];
    FuncAddLatePkt funcAddLatePkt[NUM_CODECS];
    FuncGetMDinfo funcGetMDinfo[NUM_CODECS];
    FuncGetPitchInfo funcGetPitch[NUM_CODECS];
    FuncUpdBWEst funcUpdBWEst[NUM_CODECS];
    FuncDurationEst funcDurationEst[NUM_CODECS];
    FuncGetErrorCode funcGetErrorCode[NUM_CODECS];
    void * codec_state[NUM_CODECS];
    uint16_t codec_fs[NUM_CODECS];
    int16_t CNGpayloadType[NUM_CNG_CODECS];

} CodecDbInst_t;

#define NO_SPLIT -1 /* codec payload cannot be split */

typedef struct
{
    int16_t deltaBytes;
    int16_t deltaTime;
} SplitInfo_t;

/*
 * Resets the codec database.
 */
int WebRtcNetEQ_DbReset(CodecDbInst_t *inst);

/*
 * Adds a new codec to the database.
 */
int WebRtcNetEQ_DbAdd(CodecDbInst_t *inst, enum WebRtcNetEQDecoder codec,
                      int16_t payloadType, FuncDecode funcDecode,
                      FuncDecode funcDecodeRCU, FuncDecodePLC funcDecodePLC,
                      FuncDecodeInit funcDecodeInit, FuncAddLatePkt funcAddLatePkt,
                      FuncGetMDinfo funcGetMDinfo, FuncGetPitchInfo funcGetPitch,
                      FuncUpdBWEst funcUpdBWEst, FuncDurationEst funcDurationEst,
                      FuncGetErrorCode funcGetErrorCode, void* codec_state,
                      uint16_t codec_fs);

/*
 * Removes a codec from the database.
 */
int WebRtcNetEQ_DbRemove(CodecDbInst_t *inst, enum WebRtcNetEQDecoder codec);

/*
 * Get the decoder function pointers for a codec.
 */
int WebRtcNetEQ_DbGetPtrs(CodecDbInst_t *inst, enum WebRtcNetEQDecoder,
                          CodecFuncInst_t *ptr_inst);

/*
 * Returns payload number given a codec identifier.
 */

int WebRtcNetEQ_DbGetPayload(CodecDbInst_t *inst, enum WebRtcNetEQDecoder codecID);

/*
 * Returns codec identifier given a payload number.
 */

int WebRtcNetEQ_DbGetCodec(const CodecDbInst_t *inst, int payloadType);

/*
 * Extracts the Payload Split information of the codec with the specified payloadType.
 */

int WebRtcNetEQ_DbGetSplitInfo(SplitInfo_t *inst, enum WebRtcNetEQDecoder codecID,
                               int codedsize);

/*
 * Returns 1 if codec is multiple description type, 0 otherwise.
 */
int WebRtcNetEQ_DbIsMDCodec(enum WebRtcNetEQDecoder codecID);

/*
 * Returns 1 if payload type is registered as a CNG codec, 0 otherwise.
 */
int WebRtcNetEQ_DbIsCNGPayload(const CodecDbInst_t *inst, int payloadType);

/*
 * Return the sample rate for the codec with the given payload type, 0 if error.
 */
uint16_t WebRtcNetEQ_DbGetSampleRate(CodecDbInst_t *inst, int payloadType);

#endif

