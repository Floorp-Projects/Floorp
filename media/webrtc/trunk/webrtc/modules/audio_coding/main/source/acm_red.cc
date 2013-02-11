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

ACMRED::ACMRED(WebRtc_Word16 codec_id) {
  codec_id_ = codec_id;
}

ACMRED::~ACMRED() {
  return;
}

WebRtc_Word16 ACMRED::InternalEncode(WebRtc_UWord8* /* bitstream */,
                                     WebRtc_Word16* /* bitstream_len_byte */) {
  // RED is never used as an encoder
  // RED has no instance
  return 0;
}

WebRtc_Word16 ACMRED::DecodeSafe(WebRtc_UWord8* /* bitstream */,
                                 WebRtc_Word16 /* bitstream_len_byte */,
                                 WebRtc_Word16* /* audio */,
                                 WebRtc_Word16* /* audio_samples */,
                                 WebRtc_Word8* /* speech_type */) {
  return 0;
}

WebRtc_Word16 ACMRED::InternalInitEncoder(
    WebRtcACMCodecParams* /* codec_params */) {
  // This codec does not need initialization,
  // RED has no instance
  return 0;
}

WebRtc_Word16 ACMRED::InternalInitDecoder(
    WebRtcACMCodecParams* /* codec_params */) {
  // This codec does not need initialization,
  // RED has no instance
  return 0;
}

WebRtc_Word32 ACMRED::CodecDef(WebRtcNetEQ_CodecDef& codec_def,
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

WebRtc_Word16 ACMRED::InternalCreateEncoder() {
  // RED has no instance
  return 0;
}

WebRtc_Word16 ACMRED::InternalCreateDecoder() {
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
