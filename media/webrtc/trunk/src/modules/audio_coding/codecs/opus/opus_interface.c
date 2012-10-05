/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "opus_interface.h"

#include <stdlib.h>
#include <string.h>

#include <opus.h>

#include "neteq_defines.h"
#include "signal_processing_library.h"
#include "resample_by_2_internal.h"

/* We always produce 20ms frames. */
#define WEBRTC_OPUS_MAX_ENCODE_FRAME_SIZE_MS (20)

/* The format allows up to 120ms frames. Since we
 * don't control the other side, we must allow
 * for packets that large. NetEq is currently
 * limited to 60 ms on the receive side.
 */
#define WEBRTC_OPUS_MAX_DECODE_FRAME_SIZE_MS (120)

/* Sample count is 48 kHz * FRAME_SIZE_MS. */
#define WEBRTC_OPUS_MAX_FRAME_SIZE (48*WEBRTC_OPUS_MAX_DECODE_FRAME_SIZE_MS)

struct WebRTCOpusEncInst{
  int32_t state_32_64[8];
  int32_t state_64_48[8];
  OpusEncoder *encoder;
};

int16_t WebRtcOpus_EncoderCreate(OpusEncInst **inst,
                                 int32_t       channels)
{
  OpusEncInst *state;
  state = (OpusEncInst*)calloc(1, sizeof(*state));
  if(state) {
    int error;
    state->encoder = opus_encoder_create(48000, channels,
                          OPUS_APPLICATION_VOIP, &error);
      if (error == OPUS_OK && state->encoder != NULL) {
        *inst = state;
        return 0;
      }
      free(state);
  }
  return -1;
}

int16_t WebRtcOpus_EncoderFree(OpusEncInst *inst)
{
  opus_encoder_destroy(inst->encoder);
  free(inst);
  return 0;
}

int16_t WebRtcOpus_EncodeNative(OpusEncInst *inst,
                                int16_t     *audioIn,
                                uint8_t     *encoded,
                                int16_t      encodedLenByte,
                                int16_t      len)
{
  opus_int16 *audio = (opus_int16 *)audioIn;
  unsigned char *coded = encoded;

  int res = opus_encode(inst->encoder, audio, len, coded, encodedLenByte);

  if (res > 0) {
    return res;
  }

  return -1;
}

int16_t WebRtcOpus_Encode(OpusEncInst *inst,
                          int16_t     *audioIn,
                          uint8_t     *encoded,
                          int16_t      encodedLenByte,
                          int16_t      len)
{
  int16_t buffer16[48*WEBRTC_OPUS_MAX_ENCODE_FRAME_SIZE_MS];
  int32_t buffer32[64*WEBRTC_OPUS_MAX_ENCODE_FRAME_SIZE_MS+7];
  int i;

  if (len > 32*WEBRTC_OPUS_MAX_ENCODE_FRAME_SIZE_MS) {
    return -1;
  }
  /* Resample 32 kHz to 64 kHz. */
  for(i = 0; i < 7; i++) {
    buffer32[i] = inst->state_64_48[i];
  }
  WebRtcSpl_UpBy2ShortToInt(audioIn, len, buffer32+7, inst->state_32_64);
  /* Resample 64 kHz to 48 kHz. */
  for(i = 0; i < 7; i++) {
      inst->state_64_48[i] = buffer32[2*len+i];
  }
  len >>= 1;
  WebRtcSpl_Resample32khzTo24khz(buffer32, buffer32, len);
  len *= 3;
  WebRtcSpl_VectorBitShiftW32ToW16(buffer16, len, buffer32, 15);

  return WebRtcOpus_EncodeNative(inst, buffer16,
                                 encoded, encodedLenByte, len);
}

int16_t WebRtcOpus_SetBitRate(OpusEncInst *inst, int32_t rate)
{
  return opus_encoder_ctl(inst->encoder, OPUS_SET_BITRATE(rate));
}


struct WebRTCOpusDecInst{
  int16_t state_48_32[8];
  OpusDecoder *decoder;
};

int16_t WebRtcOpus_DecoderCreate(OpusDecInst **inst,
                                 int32_t       fs,
                                 int32_t       channels)
{
  OpusDecInst *state;
  state = (OpusDecInst*)calloc(1 ,sizeof(*state));
  if(state) {
    int error;
    state->decoder = opus_decoder_create(48000, channels, &error);
    if (error == OPUS_OK && state->decoder != NULL) {
      *inst = state;
      return 0;
    }
    free(state);
  }
  return -1;
}

int16_t WebRtcOpus_DecoderFree(OpusDecInst *inst)
{
    opus_decoder_destroy(inst->decoder);
    free(inst);
    return 0;
}

int16_t WebRtcOpus_DecoderInit(OpusDecInst* inst)
{
  int error = opus_decoder_ctl(inst->decoder, OPUS_RESET_STATE);
  if(error == OPUS_OK) {
    memset(inst->state_48_32,0,sizeof(inst->state_48_32));
    return 0;
  }
  return -1;
}

int16_t WebRtcOpus_DecodeNative(OpusDecInst *inst,
                                int16_t     *encoded,
                                int16_t      len,
                                int16_t     *decoded,
                                int16_t     *audioType)
{
  unsigned char *coded = (unsigned char *)encoded;
  opus_int16 *audio = (opus_int16 *)decoded;

  int res = opus_decode(inst->decoder, coded, len, audio,
                        WEBRTC_OPUS_MAX_FRAME_SIZE, 0);
  /* TODO: set to DTX for zero-length packets? */
  *audioType = 0;

  if(res > 0) {
    return res;
  }
  return -1;
}


int16_t WebRtcOpus_Decode(OpusDecInst *inst,
                          int16_t     *encoded,
                          int16_t      len,
                          int16_t     *decoded,
                          int16_t     *audioType)
{
  /* Enough for 120 ms (the largest Opus packet size) of mono audio at 48 kHz.
   * This will need to be enlarged for stereo decoding. */
  int16_t buffer16[WEBRTC_OPUS_MAX_FRAME_SIZE];
  int32_t buffer32[WEBRTC_OPUS_MAX_FRAME_SIZE+7];
  int decodedSamples;
  int blocks;
  int16_t outputSamples;
  int i;

  /* Decode to a temporary buffer. */
  decodedSamples =
      WebRtcOpus_DecodeNative(inst, encoded, len, buffer16, audioType);
  if (decodedSamples < 0) {
    return -1;
  }
  /* Resample from 48 kHz to 32 kHz. */
  for (i = 0; i < 7; i++) {
    buffer32[i] = inst->state_48_32[i];
  }
  for (i = 0; i < decodedSamples; i++) {
    buffer32[7+i] = buffer16[i];
  }
  for (i = 0; i < 7; i++) {
    inst->state_48_32[i] = (int16_t)buffer32[decodedSamples+i];
  }
  blocks = decodedSamples/3;
  WebRtcSpl_Resample48khzTo32khz(buffer32, buffer32, blocks);
  outputSamples = (int16_t)(blocks*2);
  WebRtcSpl_VectorBitShiftW32ToW16(decoded, outputSamples, buffer32, 15);

  return outputSamples;
}


int16_t WebRtcOpus_DecodePlc(OpusDecInst *inst,
                             int16_t     *decoded,
                             int16_t      noOfLostFrames)
{
  /* TODO: We can pass NULL to opus_decode to activate packet
   * loss concealment, but I don't know how many samples
   * noOfLostFrames corresponds to. */
  return -1;
}

int16_t WebRtcOpus_Version(char *version, int16_t lenBytes)
{
  const char *opus = opus_get_version_string();

  if (opus == NULL || version == NULL || lenBytes < 0) {
    return -1;
  }

  strncpy(version, opus, lenBytes);
  version[lenBytes-1] = '\0';

  return 0;
}
