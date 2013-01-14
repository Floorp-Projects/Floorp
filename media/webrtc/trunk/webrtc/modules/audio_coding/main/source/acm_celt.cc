/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/source/acm_celt.h"

#include "webrtc/modules/audio_coding/main/source/acm_common_defs.h"
#include "webrtc/modules/audio_coding/main/source/acm_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq_help_macros.h"
#include "webrtc/system_wrappers/interface/trace.h"

#ifdef WEBRTC_CODEC_CELT
// NOTE! Celt is not included in the open-source package. Modify this file or
// your codec API to match the function call and name of used Celt API file.
#include "celt_interface.h"
#endif

namespace webrtc {

#ifndef WEBRTC_CODEC_CELT

ACMCELT::ACMCELT(int16_t /* codec_id */)
    : enc_inst_ptr_(NULL),
      dec_inst_ptr_(NULL),
      sampling_freq_(0),
      bitrate_(0),
      channels_(1),
      dec_channels_(1) {
  return;
}

ACMCELT::~ACMCELT() {
  return;
}

int16_t ACMCELT::InternalEncode(uint8_t* /* bitstream */,
                                int16_t* /* bitstream_len_byte */) {
  return -1;
}

int16_t ACMCELT::DecodeSafe(uint8_t* /* bitstream */,
                            int16_t /* bitstream_len_byte */,
                            int16_t* /* audio */,
                            int16_t* /* audio_samples */,
                            WebRtc_Word8* /* speech_type */) {
  return -1;
}

int16_t ACMCELT::InternalInitEncoder(WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

int16_t ACMCELT::InternalInitDecoder(WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

int32_t ACMCELT::CodecDef(WebRtcNetEQ_CodecDef& /* codec_def  */,
                          const CodecInst& /* codec_inst */) {
  return -1;
}

ACMGenericCodec* ACMCELT::CreateInstance(void) {
  return NULL;
}

int16_t ACMCELT::InternalCreateEncoder() {
  return -1;
}

void ACMCELT::DestructEncoderSafe() {
  return;
}

int16_t ACMCELT::InternalCreateDecoder() {
  return -1;
}

void ACMCELT::DestructDecoderSafe() {
  return;
}

void ACMCELT::InternalDestructEncoderInst(void* /* ptr_inst */) {
  return;
}

bool ACMCELT::IsTrueStereoCodec() {
  return true;
}

int16_t ACMCELT::SetBitRateSafe(const int32_t /*rate*/) {
  return -1;
}

void ACMCELT::SplitStereoPacket(uint8_t* /*payload*/,
                                int32_t* /*payload_length*/) {}

#else  //===================== Actual Implementation =======================

ACMCELT::ACMCELT(int16_t codec_id)
    : enc_inst_ptr_(NULL),
      dec_inst_ptr_(NULL),
      sampling_freq_(32000),  // Default sampling frequency.
      bitrate_(64000),  // Default rate.
      channels_(1),  // Default send mono.
      dec_channels_(1) {  // Default receive mono.
  // TODO(tlegrand): remove later when ACMGenericCodec has a new constructor.
  codec_id_ = codec_id;

  return;
}

ACMCELT::~ACMCELT() {
  if (enc_inst_ptr_ != NULL) {
    WebRtcCelt_FreeEnc(enc_inst_ptr_);
    enc_inst_ptr_ = NULL;
  }
  if (dec_inst_ptr_ != NULL) {
    WebRtcCelt_FreeDec(dec_inst_ptr_);
    dec_inst_ptr_ = NULL;
  }
  return;
}

int16_t ACMCELT::InternalEncode(uint8_t* bitstream,
                                int16_t* bitstream_len_byte) {
  *bitstream_len_byte = 0;

  // Call Encoder.
  *bitstream_len_byte = WebRtcCelt_Encode(enc_inst_ptr_,
                                          &in_audio_[in_audio_ix_read_],
                                          bitstream);

  // Increment the read index this tell the caller that how far
  // we have gone forward in reading the audio buffer.
  in_audio_ix_read_ += frame_len_smpl_ * channels_;

  if (*bitstream_len_byte < 0) {
    // Error reported from the encoder.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "InternalEncode: Encode error for Celt");
    *bitstream_len_byte = 0;
    return -1;
  }

  return *bitstream_len_byte;
}

int16_t ACMCELT::DecodeSafe(uint8_t* /* bitstream */,
                            int16_t /* bitstream_len_byte */,
                            int16_t* /* audio */,
                            int16_t* /* audio_samples */,
                            WebRtc_Word8* /* speech_type */) {
  return 0;
}

int16_t ACMCELT::InternalInitEncoder(WebRtcACMCodecParams* codec_params) {
  // Set bitrate and check that it is within the valid range.
  int16_t status = SetBitRateSafe((codec_params->codec_inst).rate);
  if (status < 0) {
    return -1;
  }

  // If number of channels changed we need to re-create memory.
  if (codec_params->codec_inst.channels != channels_) {
    WebRtcCelt_FreeEnc(enc_inst_ptr_);
    enc_inst_ptr_ = NULL;
    // Store new number of channels.
    channels_ = codec_params->codec_inst.channels;
    if (WebRtcCelt_CreateEnc(&enc_inst_ptr_, channels_) < 0) {
      return -1;
    }
  }

  // Initiate encoder.
  if (WebRtcCelt_EncoderInit(enc_inst_ptr_, channels_, bitrate_) >= 0) {
    return 0;
  } else {
    return -1;
  }
}

int16_t ACMCELT::InternalInitDecoder(WebRtcACMCodecParams* codec_params) {
  // If number of channels changed we need to re-create memory.
  if (codec_params->codec_inst.channels != dec_channels_) {
    WebRtcCelt_FreeDec(dec_inst_ptr_);
    dec_inst_ptr_ = NULL;
    // Store new number of channels.
    dec_channels_ = codec_params->codec_inst.channels;
    if (WebRtcCelt_CreateDec(&dec_inst_ptr_, dec_channels_) < 0) {
      return -1;
    }
  }

  // Initiate decoder, both master and slave parts.
  if (WebRtcCelt_DecoderInit(dec_inst_ptr_) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "InternalInitDecoder: init decoder failed for Celt.");
    return -1;
  }
  if (WebRtcCelt_DecoderInitSlave(dec_inst_ptr_) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "InternalInitDecoder: init decoder failed for Celt.");
    return -1;
  }
  return 0;
}

int32_t ACMCELT::CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                          const CodecInst& codec_inst) {
  if (!decoder_initialized_) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "CodecDef: Decoder uninitialized for Celt");
    return -1;
  }

  // Fill up the structure by calling
  // "SET_CODEC_PAR" and "SET_CELT_FUNCTIONS" or "SET_CELTSLAVE_FUNCTIONS".
  // Then call NetEQ to add the codec to it's
  // database.
  if (codec_inst.channels == 1) {
    SET_CODEC_PAR(codec_def, kDecoderCELT_32, codec_inst.pltype, dec_inst_ptr_,
                  32000);
  } else {
    SET_CODEC_PAR(codec_def, kDecoderCELT_32_2ch, codec_inst.pltype,
                  dec_inst_ptr_, 32000);
  }

  // If this is the master of NetEQ, regular decoder will be added, otherwise
  // the slave decoder will be used.
  if (is_master_) {
    SET_CELT_FUNCTIONS(codec_def);
  } else {
    SET_CELTSLAVE_FUNCTIONS(codec_def);
  }
  return 0;
}

ACMGenericCodec* ACMCELT::CreateInstance(void) {
  return NULL;
}

int16_t ACMCELT::InternalCreateEncoder() {
  if (WebRtcCelt_CreateEnc(&enc_inst_ptr_, num_channels_) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "InternalCreateEncoder: create encoder failed for Celt");
    return -1;
  }
  channels_ = num_channels_;
  return 0;
}

void ACMCELT::DestructEncoderSafe() {
  encoder_exist_ = false;
  encoder_initialized_ = false;
  if (enc_inst_ptr_ != NULL) {
    WebRtcCelt_FreeEnc(enc_inst_ptr_);
    enc_inst_ptr_ = NULL;
  }
}

int16_t ACMCELT::InternalCreateDecoder() {
  if (WebRtcCelt_CreateDec(&dec_inst_ptr_, dec_channels_) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "InternalCreateDecoder: create decoder failed for Celt");
    return -1;
  }

  return 0;
}

void ACMCELT::DestructDecoderSafe() {
  decoder_exist_ = false;
  decoder_initialized_ = false;
  if (dec_inst_ptr_ != NULL) {
    WebRtcCelt_FreeDec(dec_inst_ptr_);
    dec_inst_ptr_ = NULL;
  }
}

void ACMCELT::InternalDestructEncoderInst(void* ptr_inst) {
  if (ptr_inst != NULL) {
    WebRtcCelt_FreeEnc(static_cast<CELT_encinst_t*>(ptr_inst));
  }
  return;
}

bool ACMCELT::IsTrueStereoCodec() {
  return true;
}

int16_t ACMCELT::SetBitRateSafe(const int32_t rate) {
  // Check that rate is in the valid range.
  if ((rate >= 48000) && (rate <= 128000)) {
    // Store new rate.
    bitrate_ = rate;

    // Initiate encoder with new rate.
    if (WebRtcCelt_EncoderInit(enc_inst_ptr_, channels_, bitrate_) >= 0) {
      return 0;
    } else {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                   "SetBitRateSafe: Failed to initiate Celt with rate %d",
                   rate);
      return -1;
    }
  } else {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "SetBitRateSafe: Invalid rate Celt, %d", rate);
    return -1;
  }
}

// Copy the stereo packet so that NetEq will insert into both master and slave.
void ACMCELT::SplitStereoPacket(uint8_t* payload, int32_t* payload_length) {
  // Check for valid inputs.
  assert(payload != NULL);
  assert(*payload_length > 0);

  // Duplicate the payload.
  memcpy(&payload[*payload_length], &payload[0],
         sizeof(uint8_t) * (*payload_length));
  // Double the size of the packet.
  *payload_length *= 2;
}

#endif

}  // namespace webrtc
