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
 * Some definitions related to the codec database.
 */

#ifndef CODEC_DB_DEFINES_H
#define CODEC_DB_DEFINES_H

#include "typedefs.h"

#define NUM_CODECS 47 /* probably too large with the limited set of supported codecs*/
#define NUM_TOTAL_CODECS	kDecoderReservedEnd

/*
 * Pointer to decoder function.
 */
typedef int16_t (*FuncDecode)(void* state, int16_t* encoded, int16_t len,
                                    int16_t* decoded, int16_t* speechType);

/*
 * Pointer to PLC function.
 */
typedef int16_t (*FuncDecodePLC)(void* state, int16_t* decodec,
                                       int16_t frames);

/*
 * Pointer to decoder init function.
 */
typedef int16_t (*FuncDecodeInit)(void* state);

/*
 * Pointer to add late packet function.
 */
typedef int16_t
                (*FuncAddLatePkt)(void* state, int16_t* encoded, int16_t len);

/*
 * Pointer to get MD infofunction.
 */
typedef int16_t (*FuncGetMDinfo)(void* state);

/*
 * Pointer to pitch info function.
 * Return 0 for unvoiced, -1 if pitch not availiable.
 */
typedef int16_t (*FuncGetPitchInfo)(void* state, int16_t* encoded,
                                          int16_t* length);

/*
 *  Pointer to the update bandwidth estimate function
 */
typedef int16_t (*FuncUpdBWEst)(void* state, const uint16_t *encoded,
                                      int32_t packet_size,
                                      uint16_t rtp_seq_number, uint32_t send_ts,
                                      uint32_t arr_ts);

/*
 *  Pointer to the frame size estimate function.
 *  Returns the estimated number of samples in the packet.
 */
typedef int (*FuncDurationEst)(void* state, const uint8_t* payload,
                               int payload_length_bytes);

/*
 *  Pointer to error code function
 */
typedef int16_t (*FuncGetErrorCode)(void* state);

typedef struct CodecFuncInst_t_
{

    FuncDecode funcDecode;
    FuncDecode funcDecodeRCU;
    FuncDecodePLC funcDecodePLC;
    FuncDecodeInit funcDecodeInit;
    FuncAddLatePkt funcAddLatePkt;
    FuncGetMDinfo funcGetMDinfo;
    FuncUpdBWEst funcUpdBWEst; /* Currently in use for the ISAC family (without LC) only*/
    FuncDurationEst funcDurationEst;
    FuncGetErrorCode funcGetErrorCode;
    void * codec_state;
    uint16_t codec_fs;
    uint32_t timeStamp;

} CodecFuncInst_t;

#endif

