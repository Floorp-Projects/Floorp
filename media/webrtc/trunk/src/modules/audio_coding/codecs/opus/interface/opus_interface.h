/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_OPUS_INTERFACE_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_OPUS_INTERFACE_H_

#include <opus.h>

#include "typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

// opaque wrapper types for the codec state
typedef struct WebRTCOpusEncInst OpusEncInst;
typedef struct WebRTCOpusDecInst OpusDecInst;

int16_t WebRtcOpus_EncoderCreate(OpusEncInst **inst, int32_t channels);
int16_t WebRtcOpus_EncoderFree(OpusEncInst *inst);

/****************************************************************************
 * WebRtcOpus_Encode(...)
 *
 * This function encodes audio as a series of Opus frames and inserts
 * it into a packet. Input buffer can be any length.
 *
 * Input:
 *      - inst               : Encoder context
 *      - audioIn            : Input speech data buffer
 *      - encoded            : Output compressed data buffer
 *      - encodedLenByte     : Output buffer size
 *      - len                : Samples in speechIn
 *
 * Output:
 *      - coded              : The encoded audio packet data buffer
 *
 * Return value              : >0 - Length (in bytes) of coded data
 *                             -1 - Error
 */
int16_t WebRtcOpus_Encode(OpusEncInst   *inst,
                          int16_t       *audioIn,
                          uint8_t       *encoded,
                          int16_t        encodedLenByte,
                          int16_t        len);

/****************************************************************************
 * WebRtcOpus_SetBitRate(...)
 *
 * This function adjusts the target bitrate of the encoder.
 *
 * Input:
 *      - inst               : Encoder context
 *      - rate               : New target bitrate
 *
 * Return value              :  0 - Success
 *                             -1 - Error
 */
int16_t WebRtcOpus_SetBitRate(OpusEncInst *inst, int32_t rate);

int16_t WebRtcOpus_DecoderCreate(OpusDecInst **inst,
                                 int32_t       fs,
                                 int32_t       channels);
int16_t WebRtcOpus_DecoderFree(OpusDecInst *inst);

/****************************************************************************
 * WebRtcOpus_DecoderInit(...)
 *
 * This function resets state of the decoder.
 *
 * Input:
 *      - inst               : Decoder context
 *
 * Return value              :  0 - Success
 *                             -1 - Error
 */
int16_t WebRtcOpus_DecoderInit(OpusDecInst* inst);

/****************************************************************************
 * WebRtcOpus_DecodeNative(...)
 *
 * This function decodes an Opus packet into one or more audio frames at the
 * native Opus sampling rate (48 kHz).
 *
 * Input:
 *      - inst               : Decoder context
 *      - encoded            : Encoded data
 *      - len                : Bytes in encoded vector
 *
 * Output:
 *      - decoded            : The decoded vector
 *      - audioType          : 1 normal, 2 CNG (for Opus it should
 *                             always return 1 since we're not using Opus's
 *                             built-in DTX/CNG scheme)
 *
 * Return value              : >0 - Samples in decoded vector
 *                             -1 - Error
 */
int16_t WebRtcOpus_DecodeNative(OpusDecInst *inst,
                                int16_t     *encoded,
                                int16_t      len,
                                int16_t     *decoded,
                                int16_t     *audioType);

/****************************************************************************
 * WebRtcOpus_Decode(...)
 *
 * This function decodes an Opus packet into one or more audio frames at the
 * ACM interface's sampling rate (32 kHz).
 *
 * Input:
 *      - inst               : Decoder context
 *      - encoded            : Encoded data
 *      - len                : Bytes in encoded vector
 *
 * Output:
 *      - decoded            : The decoded vector
 *      - audioType          : 1 normal, 2 CNG (for Opus it should
 *                             always return 1 since we're not using Opus's
 *                             built-in DTX/CNG scheme)
 *
 * Return value              : >0 - Samples in decoded vector
 *                             -1 - Error
 */
int16_t WebRtcOpus_Decode(OpusDecInst   *inst,
                          int16_t       *encoded,
                          int16_t        len,
                          int16_t       *decoded,
                          int16_t       *audioType);

/****************************************************************************
 * WebRtcOpus_DecodePlc(...)
 *
 * This function precesses PLC for opus frame(s).
 * Input:
 *        - inst              : Decoder context
 *        - noOfLostFrames    : Number of PLC frames to produce
 *
 * Output:
 *        - decoded           : The decoded vector
 *
 * Return value               : >0 - number of samples in decoded PLC vector
 *                              -1 - Error
 */
int16_t WebRtcOpus_DecodePlc(OpusDecInst   *inst,
                             int16_t       *decoded,
                             int16_t        noOfLostFrames);

/****************************************************************************
 * WebRtcOpus_Version(...)
 *
 * This function queries the version string of the Opus codec implementation.
 * Input:
 *        - lenBytes          : The size of the allocated space
 *                              in bytes where the version number
 *                              is to be written in string format.
 *
 * Output:
 *        - version           : Pointer to a buffer where the version number
 *                              should be written.
 *
 * Return value               : >0 - number of samples in decoded PLC vector
 *                              -1 - Error
 */
int16_t WebRtcOpus_Version(char *version, int16_t lenBytes);

#ifdef __cplusplus
} // extern "C"
#endif

#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_OPUS_INTERFACE_H_
