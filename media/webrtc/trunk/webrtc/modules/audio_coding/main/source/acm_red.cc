/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/source/acm_red.h"

#include "webrtc/modules/audio_coding/main/source/acm_common_defs.h"
#include "webrtc/modules/audio_coding/main/source/acm_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq_help_macros.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

ACMRED::ACMRED(int16_t codec_id) {
  codec_id_ = codec_id;
}

ACMRED::~ACMRED() {
  return;
}

int16_t ACMRED::InternalEncode(uint8_t* /* bitstream */,
                               int16_t* /* bitstream_len_byte */) {
  // RED is never used as an encoder
  // RED has no instance
  return 0;
}

int16_t ACMRED::DecodeSafe(uint8_t* /* bitstream */,
                           int16_t /* bitstream_len_byte */,
                           int16_t* /* audio */,
                           int16_t* /* audio_samples */,
                           int8_t* /* speech_type */) {
  return 0;
}

int16_t ACMRED::InternalInitEncoder(
    WebRtcACMCodecParams* /* codec_params */) {
  // This codec does not need initialization,
  // RED has no instance
  return 0;
}

int16_t ACMRED::InternalInitDecoder(
    WebRtcACMCodecParams* /* codec_params */) {
  // This codec does not need initialization,
  // RED has no instance
  return 0;
}

int32_t ACMRED::CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                         const CodecInst& codec_inst) {
  if (!decoder_initialized_) {
    // Todo:
    // log error
    return -1;
  }

  // Fill up the structure by calling
  // "SET_CODEC_PAR" & "SET_PCMU_FUNCTION."
  // Then call NetEQ to add the codec to it's
  // database.
  SET_CODEC_PAR((codec_def), kDecoderRED, codec_inst.pltype, NULL, 8000);
  SET_RED_FUNCTIONS((codec_def));
  return 0;
}

ACMGenericCodec* ACMRED::CreateInstance(void) {
  return NULL;
}

int16_t ACMRED::InternalCreateEncoder() {
  // RED has no instance
  return 0;
}

int16_t ACMRED::InternalCreateDecoder() {
  // RED has no instance
  return 0;
}

void ACMRED::InternalDestructEncoderInst(void* /* ptr_inst */) {
  // RED has no instance
  return;
}

void ACMRED::DestructEncoderSafe() {
  // RED has no instance
  return;
}

void ACMRED::DestructDecoderSafe() {
  // RED has no instance
  return;
}

}  // namespace webrtc
