/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_OPUS_INTERFACE_OPUS_INTERFACE_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_OPUS_INTERFACE_OPUS_INTERFACE_H_

#include "typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque wrapper types for the codec state.
typedef struct WebRtcOpusEncInst OpusEncInst;
typedef struct WebRtcOpusDecInst OpusDecInst;

int16_t WebRtcOpus_EncoderCreate(OpusEncInst** inst, int32_t channels);
int16_t WebRtcOpus_EncoderFree(OpusEncInst* inst);

/****************************************************************************
 * WebRtcOpus_Encode(...)
 *
 * This function encodes audio as a series of Opus frames and inserts
 * it into a packet. Input buffer can be any length.
 *
 * Input:
 *      - inst                  : Encoder context
 *      - audio_in              : Input speech data buffer
 *      - samples               : Samples in audio_in
 *      - length_encoded_buffer : Output buffer size
 *
 * Output:
 *      - encoded               : Output compressed data buffer
 *
 * Return value                 : >0 - Length (in bytes) of coded data
 *                                -1 - Error
 */
int16_t WebRtcOpus_Encode(OpusEncInst* inst, int16_t* audio_in, int16_t samples,
                          int16_t length_encoded_buffer, uint8_t* encoded);

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
int16_t WebRtcOpus_SetBitRate(OpusEncInst* inst, int32_t rate);

int16_t WebRtcOpus_DecoderCreate(OpusDecInst** inst, int channels);
int16_t WebRtcOpus_DecoderFree(OpusDecInst* inst);

/****************************************************************************
 * WebRtcOpus_DecoderChannels(...)
 *
 * This function returns the number of channels created for Opus decoder.
 */
int WebRtcOpus_DecoderChannels(OpusDecInst* inst);

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
int16_t WebRtcOpus_DecoderInitSlave(OpusDecInst* inst);

/****************************************************************************
 * WebRtcOpus_Decode(...)
 *
 * This function decodes an Opus packet into one or more audio frames at the
 * ACM interface's sampling rate (32 kHz).
 *
 * Input:
 *      - inst               : Decoder context
 *      - encoded            : Encoded data
 *      - encoded_bytes      : Bytes in encoded vector
 *
 * Output:
 *      - decoded            : The decoded vector
 *      - audio_type         : 1 normal, 2 CNG (for Opus it should
 *                             always return 1 since we're not using Opus's
 *                             built-in DTX/CNG scheme)
 *
 * Return value              : >0 - Samples in decoded vector
 *                             -1 - Error
 */
int16_t WebRtcOpus_Decode(OpusDecInst* inst, int16_t* encoded,
                          int16_t encoded_bytes, int16_t* decoded,
                          int16_t* audio_type);
int16_t WebRtcOpus_DecodeSlave(OpusDecInst* inst, int16_t* encoded,
                               int16_t encoded_bytes, int16_t* decoded,
                               int16_t* audio_type);
/****************************************************************************
 * WebRtcOpus_DecodePlc(...)
 *
 * This function precesses PLC for opus frame(s).
 * Input:
 *        - inst                  : Decoder context
 *        - number_of_lost_frames : Number of PLC frames to produce
 *
 * Output:
 *        - decoded               : The decoded vector
 *
 * Return value                   : >0 - number of samples in decoded PLC vector
 *                                  -1 - Error
 */
int16_t WebRtcOpus_DecodePlc(OpusDecInst* inst, int16_t* decoded,
                             int16_t number_of_lost_frames);

/****************************************************************************
 * WebRtcOpus_DurationEst(...)
 *
 * This function calculates the duration of an opus packet.
 * Input:
 *        - inst                 : Decoder context
 *        - payload              : Encoded data pointer
 *        - payload_length_bytes : Bytes of encoded data
 *
 * Return value                  : The duration of the packet, in samples.
 */
int WebRtcOpus_DurationEst(OpusDecInst* inst,
                           const uint8_t* payload,
                           int payload_length_bytes);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_OPUS_INTERFACE_OPUS_INTERFACE_H_
