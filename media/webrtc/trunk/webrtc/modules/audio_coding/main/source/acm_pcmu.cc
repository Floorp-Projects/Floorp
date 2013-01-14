/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/source/acm_pcmu.h"

#include "webrtc/modules/audio_coding/codecs/g711/include/g711_interface.h"
#include "webrtc/modules/audio_coding/main/source/acm_common_defs.h"
#include "webrtc/modules/audio_coding/main/source/acm_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq_help_macros.h"
#include "webrtc/system_wrappers/interface/trace.h"

// Codec interface

namespace webrtc {

ACMPCMU::ACMPCMU(WebRtc_Word16 codec_id) {
  codec_id_ = codec_id;
}

ACMPCMU::~ACMPCMU() {
  return;
}

WebRtc_Word16 ACMPCMU::InternalEncode(WebRtc_UWord8* bitstream,
                                      WebRtc_Word16* bitstream_len_byte) {
  *bitstream_len_byte = WebRtcG711_EncodeU(NULL, &in_audio_[in_audio_ix_read_],
                                           frame_len_smpl_ * num_channels_,
                                           (WebRtc_Word16*)bitstream);
  // Increment the read index this tell the caller that how far
  // we have gone forward in reading the audio buffer.
  in_audio_ix_read_ += frame_len_smpl_ * num_channels_;
  return *bitstream_len_byte;
}

WebRtc_Word16 ACMPCMU::DecodeSafe(WebRtc_UWord8* /* bitstream */,
                                  WebRtc_Word16 /* bitstream_len_byte */,
                                  WebRtc_Word16* /* audio */,
                                  WebRtc_Word16* /* audio_samples */,
                                  WebRtc_Word8* /* speech_type */) {
  return 0;
}

WebRtc_Word16 ACMPCMU::InternalInitEncoder(
    WebRtcACMCodecParams* /* codec_params */) {
  // This codec does not need initialization, PCM has no instance.
  return 0;
}

WebRtc_Word16 ACMPCMU::InternalInitDecoder(
    WebRtcACMCodecParams* /* codec_params */) {
  // This codec does not need initialization, PCM has no instance.
  return 0;
}

WebRtc_Word32 ACMPCMU::CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                                const CodecInst& codec_inst) {
  // Fill up the structure by calling
  // "SET_CODEC_PAR" & "SET_PCMU_FUNCTION."
  // Then call NetEQ to add the codec to it's database.
  if (codec_inst.channels == 1) {
    // Mono mode.
    SET_CODEC_PAR(codec_def, kDecoderPCMu, codec_inst.pltype, NULL, 8000);
  } else {
    // Stereo mode.
    SET_CODEC_PAR(codec_def, kDecoderPCMu_2ch, codec_inst.pltype, NULL, 8000);
  }
  SET_PCMU_FUNCTIONS(codec_def);
  return 0;
}

ACMGenericCodec* ACMPCMU::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMPCMU::InternalCreateEncoder() {
  // PCM has no instance.
  return 0;
}

WebRtc_Word16 ACMPCMU::InternalCreateDecoder() {
  // PCM has no instance.
  return 0;
}

void ACMPCMU::InternalDestructEncoderInst(void* /* ptr_inst */) {
  // PCM has no instance.
  return;
}

void ACMPCMU::DestructEncoderSafe() {
  // PCM has no instance.
  encoder_exist_ = false;
  encoder_initialized_ = false;
  return;
}

void ACMPCMU::DestructDecoderSafe() {
  // PCM has no instance.
  decoder_initialized_ = false;
  decoder_exist_ = false;
  return;
}

// Split the stereo packet and place left and right channel after each other
// in the payload vector.
void ACMPCMU::SplitStereoPacket(uint8_t* payload, int32_t* payload_length) {
  uint8_t right_byte;

  // Check for valid inputs.
  assert(payload != NULL);
  assert(*payload_length > 0);

  // Move one bytes representing right channel each loop, and place it at the
  // end of the bytestream vector. After looping the data is reordered to:
  // l1 l2 l3 l4 ... l(N-1) lN r1 r2 r3 r4 ... r(N-1) r(N),
  // where N is the total number of samples.
  for (int i = 0; i < *payload_length / 2; i++) {
    right_byte = payload[i + 1];
    memmove(&payload[i + 1], &payload[i + 2], *payload_length - i - 2);
    payload[*payload_length - 1] = right_byte;
  }
}

}  // namespace webrtc
