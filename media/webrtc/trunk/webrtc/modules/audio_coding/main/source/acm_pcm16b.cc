/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/source/acm_pcm16b.h"

#include "webrtc/modules/audio_coding/main/source/acm_codec_database.h"
#include "webrtc/modules/audio_coding/main/source/acm_common_defs.h"
#include "webrtc/modules/audio_coding/main/source/acm_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq_help_macros.h"
#include "webrtc/system_wrappers/interface/trace.h"

#ifdef WEBRTC_CODEC_PCM16
#include "webrtc/modules/audio_coding/codecs/pcm16b/include/pcm16b.h"
#endif

namespace webrtc {

#ifndef WEBRTC_CODEC_PCM16

ACMPCM16B::ACMPCM16B(WebRtc_Word16 /* codec_id */) {
  return;
}

ACMPCM16B::~ACMPCM16B() {
  return;
}

WebRtc_Word16 ACMPCM16B::InternalEncode(
    WebRtc_UWord8* /* bitstream */,
    WebRtc_Word16* /* bitstream_len_byte */) {
  return -1;
}

WebRtc_Word16 ACMPCM16B::DecodeSafe(WebRtc_UWord8* /* bitstream */,
                                    WebRtc_Word16 /* bitstream_len_byte */,
                                    WebRtc_Word16* /* audio */,
                                    WebRtc_Word16* /* audio_samples */,
                                    WebRtc_Word8* /* speech_type */) {
  return -1;
}

WebRtc_Word16 ACMPCM16B::InternalInitEncoder(
    WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

WebRtc_Word16 ACMPCM16B::InternalInitDecoder(
    WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

WebRtc_Word32 ACMPCM16B::CodecDef(WebRtcNetEQ_CodecDef& /* codec_def */,
                                  const CodecInst& /* codec_inst */) {
  return -1;
}

ACMGenericCodec* ACMPCM16B::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMPCM16B::InternalCreateEncoder() {
  return -1;
}

WebRtc_Word16 ACMPCM16B::InternalCreateDecoder() {
  return -1;
}

void ACMPCM16B::InternalDestructEncoderInst(void* /* ptr_inst */) {
  return;
}

void ACMPCM16B::DestructEncoderSafe() {
  return;
}

void ACMPCM16B::DestructDecoderSafe() {
  return;
}

void ACMPCM16B::SplitStereoPacket(uint8_t* /*payload*/,
                                  int32_t* /*payload_length*/) {
}

#else     //===================== Actual Implementation =======================
ACMPCM16B::ACMPCM16B(WebRtc_Word16 codec_id) {
  codec_id_ = codec_id;
  sampling_freq_hz_ = ACMCodecDB::CodecFreq(codec_id_);
}

ACMPCM16B::~ACMPCM16B() {
  return;
}

WebRtc_Word16 ACMPCM16B::InternalEncode(WebRtc_UWord8* bitstream,
                                        WebRtc_Word16* bitstream_len_byte) {
  *bitstream_len_byte = WebRtcPcm16b_Encode(&in_audio_[in_audio_ix_read_],
                                            frame_len_smpl_ * num_channels_,
                                            bitstream);
  // Increment the read index to tell the caller that how far
  // we have gone forward in reading the audio buffer.
  in_audio_ix_read_ += frame_len_smpl_ * num_channels_;
  return *bitstream_len_byte;
}

WebRtc_Word16 ACMPCM16B::DecodeSafe(WebRtc_UWord8* /* bitstream */,
                                    WebRtc_Word16 /* bitstream_len_byte */,
                                    WebRtc_Word16* /* audio */,
                                    WebRtc_Word16* /* audio_samples */,
                                    WebRtc_Word8* /* speech_type */) {
  return 0;
}

WebRtc_Word16 ACMPCM16B::InternalInitEncoder(
    WebRtcACMCodecParams* /* codec_params */) {
  // This codec does not need initialization, PCM has no instance.
  return 0;
}

WebRtc_Word16 ACMPCM16B::InternalInitDecoder(
    WebRtcACMCodecParams* /* codec_params */) {
  // This codec does not need initialization, PCM has no instance.
  return 0;
}

WebRtc_Word32 ACMPCM16B::CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                                  const CodecInst& codec_inst) {
  // Fill up the structure by calling "SET_CODEC_PAR" & "SET_PCMU_FUNCTION".
  // Then call NetEQ to add the codec to it's database.
  if (codec_inst.channels == 1) {
    switch (sampling_freq_hz_) {
      case 8000: {
        SET_CODEC_PAR(codec_def, kDecoderPCM16B, codec_inst.pltype, NULL, 8000);
        SET_PCM16B_FUNCTIONS(codec_def);
        break;
      }
      case 16000: {
        SET_CODEC_PAR(codec_def, kDecoderPCM16Bwb, codec_inst.pltype, NULL,
                      16000);
        SET_PCM16B_WB_FUNCTIONS(codec_def);
        break;
      }
      case 32000: {
        SET_CODEC_PAR(codec_def, kDecoderPCM16Bswb32kHz, codec_inst.pltype,
                      NULL, 32000);
        SET_PCM16B_SWB32_FUNCTIONS(codec_def);
        break;
      }
      default: {
        return -1;
      }
    }
  } else {
    switch (sampling_freq_hz_) {
      case 8000: {
        SET_CODEC_PAR(codec_def, kDecoderPCM16B_2ch, codec_inst.pltype, NULL,
                      8000);
        SET_PCM16B_FUNCTIONS(codec_def);
        break;
      }
      case 16000: {
        SET_CODEC_PAR(codec_def, kDecoderPCM16Bwb_2ch, codec_inst.pltype,
                      NULL, 16000);
        SET_PCM16B_WB_FUNCTIONS(codec_def);
        break;
      }
      case 32000: {
        SET_CODEC_PAR(codec_def, kDecoderPCM16Bswb32kHz_2ch, codec_inst.pltype,
                      NULL, 32000);
        SET_PCM16B_SWB32_FUNCTIONS(codec_def);
        break;
      }
      default: {
        return -1;
      }
    }
  }
  return 0;
}

ACMGenericCodec* ACMPCM16B::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMPCM16B::InternalCreateEncoder() {
  // PCM has no instance.
  return 0;
}

WebRtc_Word16 ACMPCM16B::InternalCreateDecoder() {
  // PCM has no instance.
  return 0;
}

void ACMPCM16B::InternalDestructEncoderInst(void* /* ptr_inst */) {
  // PCM has no instance.
  return;
}

void ACMPCM16B::DestructEncoderSafe() {
  // PCM has no instance.
  encoder_exist_ = false;
  encoder_initialized_ = false;
  return;
}

void ACMPCM16B::DestructDecoderSafe() {
  // PCM has no instance.
  decoder_exist_ = false;
  decoder_initialized_ = false;
  return;
}

// Split the stereo packet and place left and right channel after each other
// in the payload vector.
void ACMPCM16B::SplitStereoPacket(uint8_t* payload, int32_t* payload_length) {
  uint8_t right_byte_msb;
  uint8_t right_byte_lsb;

  // Check for valid inputs.
  assert(payload != NULL);
  assert(*payload_length > 0);

  // Move two bytes representing right channel each loop, and place it at the
  // end of the bytestream vector. After looping the data is reordered to:
  // l1 l2 l3 l4 ... l(N-1) lN r1 r2 r3 r4 ... r(N-1) r(N),
  // where N is the total number of samples.

  for (int i = 0; i < *payload_length / 2; i += 2) {
    right_byte_msb = payload[i + 2];
    right_byte_lsb = payload[i + 3];
    memmove(&payload[i + 2], &payload[i + 4], *payload_length - i - 4);
    payload[*payload_length - 2] = right_byte_msb;
    payload[*payload_length - 1] = right_byte_lsb;
  }
}
#endif

}  // namespace webrtc
