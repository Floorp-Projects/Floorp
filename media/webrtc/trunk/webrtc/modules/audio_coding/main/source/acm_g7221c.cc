/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/source/acm_g7221c.h"

#include "webrtc/modules/audio_coding/main/source/acm_codec_database.h"
#include "webrtc/modules/audio_coding/main/source/acm_common_defs.h"
#include "webrtc/modules/audio_coding/main/source/acm_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq_help_macros.h"
#include "webrtc/system_wrappers/interface/trace.h"

#ifdef WEBRTC_CODEC_G722_1C
// NOTE! G.722.1C is not included in the open-source package. The following
// interface file is needed:
//
// /modules/audio_coding/codecs/g7221c/main/interface/g7221c_interface.h
//
// The API in the header file should match the one below.
//

// int16_t WebRtcG7221C_CreateEnc24(G722_1C_24_encinst_t_** enc_inst);
// int16_t WebRtcG7221C_CreateEnc32(G722_1C_32_encinst_t_** enc_inst);
// int16_t WebRtcG7221C_CreateEnc48(G722_1C_48_encinst_t_** enc_inst);
// int16_t WebRtcG7221C_CreateDec24(G722_1C_24_decinst_t_** dec_inst);
// int16_t WebRtcG7221C_CreateDec32(G722_1C_32_decinst_t_** dec_inst);
// int16_t WebRtcG7221C_CreateDec48(G722_1C_48_decinst_t_** dec_inst);
//
// int16_t WebRtcG7221C_FreeEnc24(G722_1C_24_encinst_t_** enc_inst);
// int16_t WebRtcG7221C_FreeEnc32(G722_1C_32_encinst_t_** enc_inst);
// int16_t WebRtcG7221C_FreeEnc48(G722_1C_48_encinst_t_** enc_inst);
// int16_t WebRtcG7221C_FreeDec24(G722_1C_24_decinst_t_** dec_inst);
// int16_t WebRtcG7221C_FreeDec32(G722_1C_32_decinst_t_** dec_inst);
// int16_t WebRtcG7221C_FreeDec48(G722_1C_48_decinst_t_** dec_inst);
//
// int16_t WebRtcG7221C_EncoderInit24(G722_1C_24_encinst_t_* enc_inst);
// int16_t WebRtcG7221C_EncoderInit32(G722_1C_32_encinst_t_* enc_inst);
// int16_t WebRtcG7221C_EncoderInit48(G722_1C_48_encinst_t_* enc_inst);
// int16_t WebRtcG7221C_DecoderInit24(G722_1C_24_decinst_t_* dec_inst);
// int16_t WebRtcG7221C_DecoderInit32(G722_1C_32_decinst_t_* dec_inst);
// int16_t WebRtcG7221C_DecoderInit48(G722_1C_48_decinst_t_* dec_inst);
//
// int16_t WebRtcG7221C_Encode24(G722_1C_24_encinst_t_* enc_inst,
//                               int16_t* input,
//                               int16_t len,
//                               int16_t* output);
// int16_t WebRtcG7221C_Encode32(G722_1C_32_encinst_t_* enc_inst,
//                               int16_t* input,
//                               int16_t len,
//                               int16_t* output);
// int16_t WebRtcG7221C_Encode48(G722_1C_48_encinst_t_* enc_inst,
//                               int16_t* input,
//                               int16_t len,
//                               int16_t* output);
//
// int16_t WebRtcG7221C_Decode24(G722_1C_24_decinst_t_* dec_inst,
//                               int16_t* bitstream,
//                               int16_t len,
//                               int16_t* output);
// int16_t WebRtcG7221C_Decode32(G722_1C_32_decinst_t_* dec_inst,
//                               int16_t* bitstream,
//                               int16_t len,
//                               int16_t* output);
// int16_t WebRtcG7221C_Decode48(G722_1C_48_decinst_t_* dec_inst,
//                               int16_t* bitstream,
//                               int16_t len,
//                               int16_t* output);
//
// int16_t WebRtcG7221C_DecodePlc24(G722_1C_24_decinst_t_* dec_inst,
//                                  int16_t* output,
//                                  int16_t nr_lost_frames);
// int16_t WebRtcG7221C_DecodePlc32(G722_1C_32_decinst_t_* dec_inst,
//                                  int16_t* output,
//                                  int16_t nr_lost_frames);
// int16_t WebRtcG7221C_DecodePlc48(G722_1C_48_decinst_t_* dec_inst,
//                                  int16_t* output,
//                                  int16_t nr_lost_frames);
#include "g7221c_interface.h"
#endif

namespace webrtc {

#ifndef WEBRTC_CODEC_G722_1C

ACMG722_1C::ACMG722_1C(WebRtc_Word16 /* codec_id */)
    : operational_rate_(-1),
      encoder_inst_ptr_(NULL),
      encoder_inst_ptr_right_(NULL),
      decoder_inst_ptr_(NULL),
      encoder_inst24_ptr_(NULL),
      encoder_inst24_ptr_right_(NULL),
      encoder_inst32_ptr_(NULL),
      encoder_inst32_ptr_right_(NULL),
      encoder_inst48_ptr_(NULL),
      encoder_inst48_ptr_right_(NULL),
      decoder_inst24_ptr_(NULL),
      decoder_inst32_ptr_(NULL),
      decoder_inst48_ptr_(NULL) {
  return;
}

ACMG722_1C::~ACMG722_1C() {
  return;
}

WebRtc_Word16 ACMG722_1C::InternalEncode(
    WebRtc_UWord8* /* bitstream */,
    WebRtc_Word16* /* bitstream_len_byte */) {
  return -1;
}

WebRtc_Word16 ACMG722_1C::DecodeSafe(WebRtc_UWord8* /* bitstream */,
                                     WebRtc_Word16 /* bitstream_len_byte */,
                                     WebRtc_Word16* /* audio */,
                                     WebRtc_Word16* /* audio_samples */,
                                     WebRtc_Word8* /* speech_type */) {
  return -1;
}

WebRtc_Word16 ACMG722_1C::InternalInitEncoder(
    WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

WebRtc_Word16 ACMG722_1C::InternalInitDecoder(
    WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

WebRtc_Word32 ACMG722_1C::CodecDef(WebRtcNetEQ_CodecDef& /* codec_def */,
                                   const CodecInst& /* codec_inst */) {
  return -1;
}

ACMGenericCodec* ACMG722_1C::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMG722_1C::InternalCreateEncoder() {
  return -1;
}

void ACMG722_1C::DestructEncoderSafe() {
  return;
}

WebRtc_Word16 ACMG722_1C::InternalCreateDecoder() {
  return -1;
}

void ACMG722_1C::DestructDecoderSafe() {
  return;
}

void ACMG722_1C::InternalDestructEncoderInst(void* /* ptr_inst */) {
  return;
}

#else     //===================== Actual Implementation =======================
ACMG722_1C::ACMG722_1C(WebRtc_Word16 codec_id)
    : encoder_inst_ptr_(NULL),
      encoder_inst_ptr_right_(NULL),
      decoder_inst_ptr_(NULL),
      encoder_inst24_ptr_(NULL),
      encoder_inst24_ptr_right_(NULL),
      encoder_inst32_ptr_(NULL),
      encoder_inst32_ptr_right_(NULL),
      encoder_inst48_ptr_(NULL),
      encoder_inst48_ptr_right_(NULL),
      decoder_inst24_ptr_(NULL),
      decoder_inst32_ptr_(NULL),
      decoder_inst48_ptr_(NULL) {
  codec_id_ = codec_id;
  if (codec_id_ == ACMCodecDB::kG722_1C_24) {
    operational_rate_ = 24000;
  } else if (codec_id_ == ACMCodecDB::kG722_1C_32) {
    operational_rate_ = 32000;
  } else if (codec_id_ == ACMCodecDB::kG722_1C_48) {
    operational_rate_ = 48000;
  } else {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "Wrong codec id for G722_1c.");
    operational_rate_ = -1;
  }
  return;
}

ACMG722_1C::~ACMG722_1C() {
  if (encoder_inst_ptr_ != NULL) {
    delete encoder_inst_ptr_;
    encoder_inst_ptr_ = NULL;
  }
  if (encoder_inst_ptr_right_ != NULL) {
    delete encoder_inst_ptr_right_;
    encoder_inst_ptr_right_ = NULL;
  }
  if (decoder_inst_ptr_ != NULL) {
    delete decoder_inst_ptr_;
    decoder_inst_ptr_ = NULL;
  }

  switch (operational_rate_) {
    case 24000: {
      encoder_inst24_ptr_ = NULL;
      encoder_inst24_ptr_right_ = NULL;
      decoder_inst24_ptr_ = NULL;
      break;
    }
    case 32000: {
      encoder_inst32_ptr_ = NULL;
      encoder_inst32_ptr_right_ = NULL;
      decoder_inst32_ptr_ = NULL;
      break;
    }
    case 48000: {
      encoder_inst48_ptr_ = NULL;
      encoder_inst48_ptr_right_ = NULL;
      decoder_inst48_ptr_ = NULL;
      break;
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                   "Wrong rate for G722_1c.");
      break;
    }
  }
  return;
}

WebRtc_Word16 ACMG722_1C::InternalEncode(WebRtc_UWord8* bitstream,
                                         WebRtc_Word16* bitstream_len_byte) {
  WebRtc_Word16 left_channel[640];
  WebRtc_Word16 right_channel[640];
  WebRtc_Word16 len_in_bytes;
  WebRtc_Word16 out_bits[240];

  // If stereo, split input signal in left and right channel before encoding
  if (num_channels_ == 2) {
    for (int i = 0, j = 0; i < frame_len_smpl_ * 2; i += 2, j++) {
      left_channel[j] = in_audio_[in_audio_ix_read_ + i];
      right_channel[j] = in_audio_[in_audio_ix_read_ + i + 1];
    }
  } else {
    memcpy(left_channel, &in_audio_[in_audio_ix_read_], 640);
  }

  switch (operational_rate_) {
    case 24000: {
      len_in_bytes = WebRtcG7221C_Encode24(encoder_inst24_ptr_, left_channel,
                                           640, &out_bits[0]);
      if (num_channels_ == 2) {
        len_in_bytes += WebRtcG7221C_Encode24(encoder_inst24_ptr_right_,
                                              right_channel, 640,
                                              &out_bits[len_in_bytes / 2]);
      }
      break;
    }
    case 32000: {
      len_in_bytes = WebRtcG7221C_Encode32(encoder_inst32_ptr_, left_channel,
                                           640, &out_bits[0]);
      if (num_channels_ == 2) {
        len_in_bytes += WebRtcG7221C_Encode32(encoder_inst32_ptr_right_,
                                              right_channel, 640,
                                              &out_bits[len_in_bytes / 2]);
      }
      break;
    }
    case 48000: {
      len_in_bytes = WebRtcG7221C_Encode48(encoder_inst48_ptr_, left_channel,
                                           640, &out_bits[0]);
      if (num_channels_ == 2) {
        len_in_bytes += WebRtcG7221C_Encode48(encoder_inst48_ptr_right_,
                                              right_channel, 640,
                                              &out_bits[len_in_bytes / 2]);
      }
      break;
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                   "InternalEncode: Wrong rate for G722_1c.");
      return -1;
    }
  }

  memcpy(bitstream, out_bits, len_in_bytes);
  *bitstream_len_byte = len_in_bytes;

  // increment the read index this tell the caller that how far
  // we have gone forward in reading the audio buffer
  in_audio_ix_read_ += 640 * num_channels_;

  return *bitstream_len_byte;
}

WebRtc_Word16 ACMG722_1C::DecodeSafe(WebRtc_UWord8* /* bitstream */,
                                     WebRtc_Word16 /* bitstream_len_byte */,
                                     WebRtc_Word16* /* audio */,
                                     WebRtc_Word16* /* audio_samples */,
                                     WebRtc_Word8* /* speech_type */) {
  return 0;
}

WebRtc_Word16 ACMG722_1C::InternalInitEncoder(
    WebRtcACMCodecParams* codec_params) {
  WebRtc_Word16 ret;

  switch (operational_rate_) {
    case 24000: {
      ret = WebRtcG7221C_EncoderInit24(encoder_inst24_ptr_right_);
      if (ret < 0) {
        return ret;
      }
      return WebRtcG7221C_EncoderInit24(encoder_inst24_ptr_);
    }
    case 32000: {
      ret = WebRtcG7221C_EncoderInit32(encoder_inst32_ptr_right_);
      if (ret < 0) {
        return ret;
      }
      return WebRtcG7221C_EncoderInit32(encoder_inst32_ptr_);
    }
    case 48000: {
      ret = WebRtcG7221C_EncoderInit48(encoder_inst48_ptr_right_);
      if (ret < 0) {
        return ret;
      }
      return WebRtcG7221C_EncoderInit48(encoder_inst48_ptr_);
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                   "InternalInitEncode: Wrong rate for G722_1c.");
      return -1;
    }
  }
}

WebRtc_Word16 ACMG722_1C::InternalInitDecoder(
    WebRtcACMCodecParams* /* codec_params */) {
  switch (operational_rate_) {
    case 24000: {
      return WebRtcG7221C_DecoderInit24(decoder_inst24_ptr_);
    }
    case 32000: {
      return WebRtcG7221C_DecoderInit32(decoder_inst32_ptr_);
    }
    case 48000: {
      return WebRtcG7221C_DecoderInit48(decoder_inst48_ptr_);
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                   "InternalInitDecoder: Wrong rate for G722_1c.");
      return -1;
    }
  }
}

WebRtc_Word32 ACMG722_1C::CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                                   const CodecInst& codec_inst) {
  if (!decoder_initialized_) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "CodeDef: decoder not initialized for G722_1c");
    return -1;
  }
  // NetEq has an array of pointers to WebRtcNetEQ_CodecDef.
  // get an entry of that array (neteq wrapper will allocate memory)
  // by calling "netEq->CodecDef", where "NETEQ_CODEC_G722_1_XX" would
  // be the index of the entry.
  // Fill up the given structure by calling
  // "SET_CODEC_PAR" & "SET_G722_1_XX_FUNCTION."
  // Then return the structure back to NetEQ to add the codec to it's
  // database.
  switch (operational_rate_) {
    case 24000: {
      SET_CODEC_PAR((codec_def), kDecoderG722_1C_24, codec_inst.pltype,
                    decoder_inst24_ptr_, 32000);
      SET_G722_1C_24_FUNCTIONS((codec_def));
      break;
    }
    case 32000: {
      SET_CODEC_PAR((codec_def), kDecoderG722_1C_32, codec_inst.pltype,
                    decoder_inst32_ptr_, 32000);
      SET_G722_1C_32_FUNCTIONS((codec_def));
      break;
    }
    case 48000: {
      SET_CODEC_PAR((codec_def), kDecoderG722_1C_32, codec_inst.pltype,
                    decoder_inst48_ptr_, 32000);
      SET_G722_1C_48_FUNCTIONS((codec_def));
      break;
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                   "CodeDef: Wrong rate for G722_1c.");
      return -1;
    }
  }
  return 0;
}

ACMGenericCodec*
ACMG722_1C::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMG722_1C::InternalCreateEncoder() {
  if ((encoder_inst_ptr_ == NULL) || (encoder_inst_ptr_right_ == NULL)) {
    return -1;
  }
  switch (operational_rate_) {
    case 24000: {
      WebRtcG7221C_CreateEnc24(&encoder_inst24_ptr_);
      WebRtcG7221C_CreateEnc24(&encoder_inst24_ptr_right_);
      break;
    }
    case 32000: {
      WebRtcG7221C_CreateEnc32(&encoder_inst32_ptr_);
      WebRtcG7221C_CreateEnc32(&encoder_inst32_ptr_right_);
      break;
    }
    case 48000: {
      WebRtcG7221C_CreateEnc48(&encoder_inst48_ptr_);
      WebRtcG7221C_CreateEnc48(&encoder_inst48_ptr_right_);
      break;
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                   "InternalCreateEncoder: Wrong rate for G722_1c.");
      return -1;
    }
  }
  return 0;
}

void ACMG722_1C::DestructEncoderSafe() {
  encoder_exist_ = false;
  encoder_initialized_ = false;
  if (encoder_inst_ptr_ != NULL) {
    delete encoder_inst_ptr_;
    encoder_inst_ptr_ = NULL;
  }
  if (encoder_inst_ptr_right_ != NULL) {
    delete encoder_inst_ptr_right_;
    encoder_inst_ptr_right_ = NULL;
  }
  encoder_inst24_ptr_ = NULL;
  encoder_inst32_ptr_ = NULL;
  encoder_inst48_ptr_ = NULL;
}

WebRtc_Word16 ACMG722_1C::InternalCreateDecoder() {
  if (decoder_inst_ptr_ == NULL) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "InternalCreateEncoder: cannot create decoder");
    return -1;
  }
  switch (operational_rate_) {
    case 24000: {
      WebRtcG7221C_CreateDec24(&decoder_inst24_ptr_);
      break;
    }
    case 32000: {
      WebRtcG7221C_CreateDec32(&decoder_inst32_ptr_);
      break;
    }
    case 48000: {
      WebRtcG7221C_CreateDec48(&decoder_inst48_ptr_);
      break;
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                   "InternalCreateEncoder: Wrong rate for G722_1c.");
      return -1;
    }
  }
  return 0;
}

void ACMG722_1C::DestructDecoderSafe() {
  decoder_exist_ = false;
  decoder_initialized_ = false;
  if (decoder_inst_ptr_ != NULL) {
    delete decoder_inst_ptr_;
    decoder_inst_ptr_ = NULL;
  }
  decoder_inst24_ptr_ = NULL;
  decoder_inst32_ptr_ = NULL;
  decoder_inst48_ptr_ = NULL;
}

void ACMG722_1C::InternalDestructEncoderInst(void* ptr_inst) {
  if (ptr_inst != NULL) {
    delete ptr_inst;
  }
  return;
}

#endif

}  // namespace webrtc
