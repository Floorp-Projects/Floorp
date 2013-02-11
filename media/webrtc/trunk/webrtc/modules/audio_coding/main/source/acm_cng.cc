/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/source/acm_cng.h"

#include "webrtc/modules/audio_coding/codecs/cng/include/webrtc_cng.h"
#include "webrtc/modules/audio_coding/main/source/acm_codec_database.h"
#include "webrtc/modules/audio_coding/main/source/acm_common_defs.h"
#include "webrtc/modules/audio_coding/main/source/acm_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq_help_macros.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

ACMCNG::ACMCNG(WebRtc_Word16 codec_id) {
  encoder_inst_ptr_ = NULL;
  decoder_inst_ptr_ = NULL;
  codec_id_ = codec_id;
  samp_freq_hz_ = ACMCodecDB::CodecFreq(codec_id_);
  return;
}

ACMCNG::~ACMCNG() {
  if (encoder_inst_ptr_ != NULL) {
    WebRtcCng_FreeEnc(encoder_inst_ptr_);
    encoder_inst_ptr_ = NULL;
  }
  if (decoder_inst_ptr_ != NULL) {
    WebRtcCng_FreeDec(decoder_inst_ptr_);
    decoder_inst_ptr_ = NULL;
  }
  return;
}

// CNG is not like a regular encoder, this function
// should not be called normally
// instead the following function is called from inside
// ACMGenericCodec::ProcessFrameVADDTX
WebRtc_Word16 ACMCNG::InternalEncode(WebRtc_UWord8* /* bitstream */,
                                     WebRtc_Word16* /* bitstream_len_byte */) {
  return -1;
}

WebRtc_Word16 ACMCNG::DecodeSafe(WebRtc_UWord8* /* bitstream */,
                                 WebRtc_Word16 /* bitstream_len_byte */,
                                 WebRtc_Word16* /* audio */,
                                 WebRtc_Word16* /* audio_samples */,
                                 WebRtc_Word8* /* speech_type */) {
  return 0;
}

// CNG is not like a regular encoder,
// this function should not be called normally
// instead the following function is called from inside
// ACMGenericCodec::ProcessFrameVADDTX
WebRtc_Word16 ACMCNG::InternalInitEncoder(
    WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

WebRtc_Word16 ACMCNG::InternalInitDecoder(
    WebRtcACMCodecParams* /* codec_params */) {
  return WebRtcCng_InitDec(decoder_inst_ptr_);
}

WebRtc_Word32 ACMCNG::CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                               const CodecInst& codec_inst) {
  if (!decoder_initialized_) {
    // TODO(tlegrand): log error
    return -1;
  }
  // Fill up the structure by calling
  // "SET_CODEC_PAR" & "SET_CNG_FUNCTION."
  // Then return the structure back to NetEQ to add the codec to it's
  // database.

  if (samp_freq_hz_ == 8000 || samp_freq_hz_ == 16000 ||
      samp_freq_hz_ == 32000 || samp_freq_hz_ == 48000) {
    SET_CODEC_PAR((codec_def), kDecoderCNG, codec_inst.pltype,
                  decoder_inst_ptr_, samp_freq_hz_);
    SET_CNG_FUNCTIONS((codec_def));
    return 0;
  } else {
    return -1;
  }
}

ACMGenericCodec* ACMCNG::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMCNG::InternalCreateEncoder() {
  if (WebRtcCng_CreateEnc(&encoder_inst_ptr_) < 0) {
    encoder_inst_ptr_ = NULL;
    return -1;
  } else {
    return 0;
  }
}

void ACMCNG::DestructEncoderSafe() {
  if (encoder_inst_ptr_ != NULL) {
    WebRtcCng_FreeEnc(encoder_inst_ptr_);
    encoder_inst_ptr_ = NULL;
  }
  encoder_exist_ = false;
  encoder_initialized_ = false;
}

WebRtc_Word16 ACMCNG::InternalCreateDecoder() {
  if (WebRtcCng_CreateDec(&decoder_inst_ptr_) < 0) {
    decoder_inst_ptr_ = NULL;
    return -1;
  } else {
    return 0;
  }
}

void ACMCNG::DestructDecoderSafe() {
  if (decoder_inst_ptr_ != NULL) {
    WebRtcCng_FreeDec(decoder_inst_ptr_);
    decoder_inst_ptr_ = NULL;
  }
  decoder_exist_ = false;
  decoder_initialized_ = false;
}

void ACMCNG::InternalDestructEncoderInst(void* ptr_inst) {
  if (ptr_inst != NULL) {
    WebRtcCng_FreeEnc(static_cast<CNG_enc_inst*>(ptr_inst));
  }
  return;
}

}  // namespace webrtc
