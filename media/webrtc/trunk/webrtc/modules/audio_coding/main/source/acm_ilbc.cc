/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/audio_coding/main/source/acm_ilbc.h"

#include "webrtc/modules/audio_coding/main/source/acm_common_defs.h"
#include "webrtc/modules/audio_coding/main/source/acm_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq_help_macros.h"
#include "webrtc/system_wrappers/interface/trace.h"

#ifdef WEBRTC_CODEC_ILBC
#include "webrtc/modules/audio_coding/codecs/ilbc/interface/ilbc.h"
#endif

namespace webrtc {

#ifndef WEBRTC_CODEC_ILBC

ACMILBC::ACMILBC(WebRtc_Word16 /* codec_id */)
    : encoder_inst_ptr_(NULL),
      decoder_inst_ptr_(NULL) {
  return;
}

ACMILBC::~ACMILBC() {
  return;
}

WebRtc_Word16 ACMILBC::InternalEncode(
    WebRtc_UWord8* /* bitstream */,
    WebRtc_Word16* /* bitstream_len_byte */) {
  return -1;
}

WebRtc_Word16 ACMILBC::DecodeSafe(WebRtc_UWord8* /* bitstream */,
                                  WebRtc_Word16 /* bitstream_len_byte */,
                                  WebRtc_Word16* /* audio */,
                                  WebRtc_Word16* /* audio_samples */,
                                  WebRtc_Word8* /* speech_type */) {
  return -1;
}

WebRtc_Word16 ACMILBC::InternalInitEncoder(
    WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

WebRtc_Word16 ACMILBC::InternalInitDecoder(
    WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

WebRtc_Word32 ACMILBC::CodecDef(WebRtcNetEQ_CodecDef& /* codec_def  */,
                                const CodecInst& /* codec_inst */) {
  return -1;
}

ACMGenericCodec* ACMILBC::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMILBC::InternalCreateEncoder() {
  return -1;
}

void ACMILBC::DestructEncoderSafe() {
  return;
}

WebRtc_Word16 ACMILBC::InternalCreateDecoder() {
  return -1;
}

void ACMILBC::DestructDecoderSafe() {
  return;
}

void ACMILBC::InternalDestructEncoderInst(void* /* ptr_inst */) {
  return;
}

WebRtc_Word16 ACMILBC::SetBitRateSafe(const WebRtc_Word32 /* rate */) {
  return -1;
}

#else  //===================== Actual Implementation =======================

ACMILBC::ACMILBC(WebRtc_Word16 codec_id)
    : encoder_inst_ptr_(NULL),
      decoder_inst_ptr_(NULL) {
  codec_id_ = codec_id;
  return;
}

ACMILBC::~ACMILBC() {
  if (encoder_inst_ptr_ != NULL) {
    WebRtcIlbcfix_EncoderFree(encoder_inst_ptr_);
    encoder_inst_ptr_ = NULL;
  }
  if (decoder_inst_ptr_ != NULL) {
    WebRtcIlbcfix_DecoderFree(decoder_inst_ptr_);
    decoder_inst_ptr_ = NULL;
  }
  return;
}

WebRtc_Word16 ACMILBC::InternalEncode(WebRtc_UWord8* bitstream,
                                      WebRtc_Word16* bitstream_len_byte) {
  *bitstream_len_byte = WebRtcIlbcfix_Encode(encoder_inst_ptr_,
                                             &in_audio_[in_audio_ix_read_],
                                             frame_len_smpl_,
                                             (WebRtc_Word16*)bitstream);
  if (*bitstream_len_byte < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "InternalEncode: error in encode for ILBC");
    return -1;
  }
  // increment the read index this tell the caller that how far
  // we have gone forward in reading the audio buffer
  in_audio_ix_read_ += frame_len_smpl_;
  return *bitstream_len_byte;
}

WebRtc_Word16 ACMILBC::DecodeSafe(WebRtc_UWord8* /* bitstream */,
                                  WebRtc_Word16 /* bitstream_len_byte */,
                                  WebRtc_Word16* /* audio */,
                                  WebRtc_Word16* /* audio_samples */,
                                  WebRtc_Word8* /* speech_type */) {
  return 0;
}

WebRtc_Word16 ACMILBC::InternalInitEncoder(WebRtcACMCodecParams* codec_params) {
  // initialize with a correct processing block length
  if ((160 == (codec_params->codec_inst).pacsize) ||
      (320 == (codec_params->codec_inst).pacsize)) {
    // processing block of 20ms
    return WebRtcIlbcfix_EncoderInit(encoder_inst_ptr_, 20);
  } else if ((240 == (codec_params->codec_inst).pacsize) ||
      (480 == (codec_params->codec_inst).pacsize)) {
    // processing block of 30ms
    return WebRtcIlbcfix_EncoderInit(encoder_inst_ptr_, 30);
  } else {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "InternalInitEncoder: invalid processing block");
    return -1;
  }
}

WebRtc_Word16 ACMILBC::InternalInitDecoder(WebRtcACMCodecParams* codec_params) {
  // initialize with a correct processing block length
  if ((160 == (codec_params->codec_inst).pacsize) ||
      (320 == (codec_params->codec_inst).pacsize)) {
    // processing block of 20ms
    return WebRtcIlbcfix_DecoderInit(decoder_inst_ptr_, 20);
  } else if ((240 == (codec_params->codec_inst).pacsize) ||
      (480 == (codec_params->codec_inst).pacsize)) {
    // processing block of 30ms
    return WebRtcIlbcfix_DecoderInit(decoder_inst_ptr_, 30);
  } else {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "InternalInitDecoder: invalid processing block");
    return -1;
  }
}

WebRtc_Word32 ACMILBC::CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                                const CodecInst& codec_inst) {
  if (!decoder_initialized_) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "CodeDef: decoder not initialized for ILBC");
    return -1;
  }
  // Fill up the structure by calling
  // "SET_CODEC_PAR" & "SET_ILBC_FUNCTION."
  // Then return the structure back to NetEQ to add the codec to it's
  // database.
  SET_CODEC_PAR((codec_def), kDecoderILBC, codec_inst.pltype, decoder_inst_ptr_,
                8000);
  SET_ILBC_FUNCTIONS((codec_def));
  return 0;
}

ACMGenericCodec* ACMILBC::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMILBC::InternalCreateEncoder() {
  if (WebRtcIlbcfix_EncoderCreate(&encoder_inst_ptr_) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "InternalCreateEncoder: cannot create instance for ILBC "
                 "encoder");
    return -1;
  }
  return 0;
}

void ACMILBC::DestructEncoderSafe() {
  encoder_initialized_ = false;
  encoder_exist_ = false;
  if (encoder_inst_ptr_ != NULL) {
    WebRtcIlbcfix_EncoderFree(encoder_inst_ptr_);
    encoder_inst_ptr_ = NULL;
  }
}

WebRtc_Word16 ACMILBC::InternalCreateDecoder() {
  if (WebRtcIlbcfix_DecoderCreate(&decoder_inst_ptr_) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "InternalCreateDecoder: cannot create instance for ILBC "
                 "decoder");
    return -1;
  }
  return 0;
}

void ACMILBC::DestructDecoderSafe() {
  decoder_initialized_ = false;
  decoder_exist_ = false;
  if (decoder_inst_ptr_ != NULL) {
    WebRtcIlbcfix_DecoderFree(decoder_inst_ptr_);
    decoder_inst_ptr_ = NULL;
  }
}

void ACMILBC::InternalDestructEncoderInst(void* ptr_inst) {
  if (ptr_inst != NULL) {
    WebRtcIlbcfix_EncoderFree((iLBC_encinst_t_*) ptr_inst);
  }
  return;
}

WebRtc_Word16 ACMILBC::SetBitRateSafe(const WebRtc_Word32 rate) {
  // Check that rate is valid. No need to store the value
  if (rate == 13300) {
    WebRtcIlbcfix_EncoderInit(encoder_inst_ptr_, 30);
  } else if (rate == 15200) {
    WebRtcIlbcfix_EncoderInit(encoder_inst_ptr_, 20);
  } else {
    return -1;
  }
  encoder_params_.codec_inst.rate = rate;

  return 0;
}

#endif

}  // namespace webrtc
