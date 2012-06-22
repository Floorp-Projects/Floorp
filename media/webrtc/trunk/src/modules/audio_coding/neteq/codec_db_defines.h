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
typedef WebRtc_Word16 (*FuncDecode)(void* state, WebRtc_Word16* encoded, WebRtc_Word16 len,
                                    WebRtc_Word16* decoded, WebRtc_Word16* speechType);

/*
 * Pointer to PLC function.
 */
typedef WebRtc_Word16 (*FuncDecodePLC)(void* state, WebRtc_Word16* decodec,
                                       WebRtc_Word16 frames);

/*
 * Pointer to decoder init function.
 */
typedef WebRtc_Word16 (*FuncDecodeInit)(void* state);

/*
 * Pointer to add late packet function.
 */
typedef WebRtc_Word16
                (*FuncAddLatePkt)(void* state, WebRtc_Word16* encoded, WebRtc_Word16 len);

/*
 * Pointer to get MD infofunction.
 */
typedef WebRtc_Word16 (*FuncGetMDinfo)(void* state);

/*
 * Pointer to pitch info function.
 * Return 0 for unvoiced, -1 if pitch not availiable.
 */
typedef WebRtc_Word16 (*FuncGetPitchInfo)(void* state, WebRtc_Word16* encoded,
                                          WebRtc_Word16* length);

/*
 *  Pointer to the update bandwidth estimate function
 */
typedef WebRtc_Word16 (*FuncUpdBWEst)(void* state, const WebRtc_UWord16 *encoded,
                                      WebRtc_Word32 packet_size,
                                      WebRtc_UWord16 rtp_seq_number, WebRtc_UWord32 send_ts,
                                      WebRtc_UWord32 arr_ts);

/*
 *  Pointer to error code function
 */
typedef WebRtc_Word16 (*FuncGetErrorCode)(void* state);

typedef struct CodecFuncInst_t_
{

    FuncDecode funcDecode;
    FuncDecode funcDecodeRCU;
    FuncDecodePLC funcDecodePLC;
    FuncDecodeInit funcDecodeInit;
    FuncAddLatePkt funcAddLatePkt;
    FuncGetMDinfo funcGetMDinfo;
    FuncUpdBWEst funcUpdBWEst; /* Currently in use for the ISAC family (without LC) only*/
    FuncGetErrorCode funcGetErrorCode;
    void * codec_state;
    WebRtc_UWord16 codec_fs;
    WebRtc_UWord32 timeStamp;

} CodecFuncInst_t;

#endif

