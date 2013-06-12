/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/source/acm_opus.h"

#include "webrtc/modules/audio_coding/main/source/acm_codec_database.h"
#include "webrtc/modules/audio_coding/main/source/acm_common_defs.h"
#include "webrtc/modules/audio_coding/main/source/acm_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq_help_macros.h"
#include "webrtc/system_wrappers/interface/trace.h"

#ifdef WEBRTC_CODEC_OPUS
#include "webrtc/modules/audio_coding/codecs/opus/interface/opus_interface.h"
#endif

namespace webrtc {

#ifndef WEBRTC_CODEC_OPUS

ACMOpus::ACMOpus(int16_t /* codec_id */)
    : encoder_inst_ptr_(NULL),
      decoder_inst_ptr_(NULL),
      sample_freq_(0),
      bitrate_(0),
      channels_(1) {
  return;
}

ACMOpus::~ACMOpus() {
  return;
}

int16_t ACMOpus::InternalEncode(uint8_t* /* bitstream */,
                                int16_t* /* bitstream_len_byte */) {
  return -1;
}

int16_t ACMOpus::DecodeSafe(uint8_t* /* bitstream */,
                            int16_t /* bitstream_len_byte */,
                            int16_t* /* audio */,
                            int16_t* /* audio_samples */,
                            int8_t* /* speech_type */) {
  return -1;
}

int16_t ACMOpus::InternalInitEncoder(WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

int16_t ACMOpus::InternalInitDecoder(WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

int32_t ACMOpus::CodecDef(WebRtcNetEQ_CodecDef& /* codec_def */,
                          const CodecInst& /* codec_inst */) {
  return -1;
}

ACMGenericCodec* ACMOpus::CreateInstance(void) {
  return NULL;
}

int16_t ACMOpus::InternalCreateEncoder() {
  return -1;
}

void ACMOpus::DestructEncoderSafe() {
  return;
}

int16_t ACMOpus::InternalCreateDecoder() {
  return -1;
}

void ACMOpus::DestructDecoderSafe() {
  return;
}

void ACMOpus::InternalDestructEncoderInst(void* /* ptr_inst */) {
  return;
}

int16_t ACMOpus::SetBitRateSafe(const int32_t /*rate*/) {
  return -1;
}

bool ACMOpus::IsTrueStereoCodec() {
  return true;
}

void ACMOpus::SplitStereoPacket(uint8_t* /*payload*/,
                                int32_t* /*payload_length*/) {}

#else  //===================== Actual Implementation =======================

ACMOpus::ACMOpus(int16_t codec_id)
    : encoder_inst_ptr_(NULL),
      decoder_inst_ptr_(NULL),
      sample_freq_(32000),  // Default sampling frequency.
      bitrate_(20000),  // Default bit-rate.
      channels_(1) {  // Default mono
  codec_id_ = codec_id;
  // Opus has internal DTX, but we dont use it for now.
  has_internal_dtx_ = false;

  if (codec_id_ != ACMCodecDB::kOpus) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "Wrong codec id for Opus.");
    sample_freq_ = -1;
    bitrate_ = -1;
  }
  return;
}

ACMOpus::~ACMOpus() {
  if (encoder_inst_ptr_ != NULL) {
    WebRtcOpus_EncoderFree(encoder_inst_ptr_);
    encoder_inst_ptr_ = NULL;
  }
  if (decoder_inst_ptr_ != NULL) {
    WebRtcOpus_DecoderFree(decoder_inst_ptr_);
    decoder_inst_ptr_ = NULL;
  }
  return;
}

int16_t ACMOpus::InternalEncode(uint8_t* bitstream,
                                int16_t* bitstream_len_byte) {
  // Call Encoder.
  *bitstream_len_byte = WebRtcOpus_Encode(encoder_inst_ptr_,
                                          &in_audio_[in_audio_ix_read_],
                                          frame_len_smpl_,
                                          MAX_PAYLOAD_SIZE_BYTE, bitstream);
  // Check for error reported from encoder.
  if (*bitstream_len_byte < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "InternalEncode: Encode error for Opus");
    *bitstream_len_byte = 0;
    return -1;
  }

  // Increment the read index. This tells the caller how far
  // we have gone forward in reading the audio buffer.
  in_audio_ix_read_ += frame_len_smpl_ * channels_;

  return *bitstream_len_byte;
}

int16_t ACMOpus::DecodeSafe(uint8_t* bitstream, int16_t bitstream_len_byte,
                            int16_t* audio, int16_t* audio_samples,
                            int8_t* speech_type) {
  return 0;
}

int16_t ACMOpus::InternalInitEncoder(WebRtcACMCodecParams* codec_params) {
  int16_t ret;
  if (encoder_inst_ptr_ != NULL) {
    WebRtcOpus_EncoderFree(encoder_inst_ptr_);
    encoder_inst_ptr_ = NULL;
  }
  ret = WebRtcOpus_EncoderCreate(&encoder_inst_ptr_,
                                 codec_params->codec_inst.channels);
  // Store number of channels.
  channels_ = codec_params->codec_inst.channels;

  if (ret < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "Encoder creation failed for Opus");
    return ret;
  }
  ret = WebRtcOpus_SetBitRate(encoder_inst_ptr_,
                              codec_params->codec_inst.rate);
  if (ret < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "Setting initial bitrate failed for Opus");
    return ret;
  }

  // Store bitrate.
  bitrate_ = codec_params->codec_inst.rate;

  return 0;
}

int16_t ACMOpus::InternalInitDecoder(WebRtcACMCodecParams* codec_params) {
  if (decoder_inst_ptr_ == NULL) {
    if (WebRtcOpus_DecoderCreate(&decoder_inst_ptr_,
                                 codec_params->codec_inst.channels) < 0) {
      return -1;
    }
  }

  // Number of channels in decoder should match the number in |codec_params|.
  assert(codec_params->codec_inst.channels ==
      WebRtcOpus_DecoderChannels(decoder_inst_ptr_));

  if (WebRtcOpus_DecoderInit(decoder_inst_ptr_) < 0) {
    return -1;
  }
  if (WebRtcOpus_DecoderInitSlave(decoder_inst_ptr_) < 0) {
    return -1;
  }
  return 0;
}

int32_t ACMOpus::CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                          const CodecInst& codec_inst) {
  if (!decoder_initialized_) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "CodeDef: Decoder uninitialized for Opus");
    return -1;
  }

  // Fill up the structure by calling "SET_CODEC_PAR" & "SET_OPUS_FUNCTION."
  // Then call NetEQ to add the codec to its database.
  // TODO(tlegrand): Decoder is registered in NetEQ as a 32 kHz decoder, which
  // is true until we have a full 48 kHz system, and remove the downsampling
  // in the Opus decoder wrapper.
  SET_CODEC_PAR(codec_def, kDecoderOpus, codec_inst.pltype,
                decoder_inst_ptr_, 32000);

  // If this is the master of NetEQ, regular decoder will be added, otherwise
  // the slave decoder will be used.
  if (is_master_) {
    SET_OPUS_FUNCTIONS(codec_def);
  } else {
    SET_OPUSSLAVE_FUNCTIONS(codec_def);
  }

  return 0;
}

ACMGenericCodec* ACMOpus::CreateInstance(void) {
  return NULL;
}

int16_t ACMOpus::InternalCreateEncoder() {
  // Real encoder will be created in InternalInitEncoder.
  return 0;
}

void ACMOpus::DestructEncoderSafe() {
  if (encoder_inst_ptr_) {
    WebRtcOpus_EncoderFree(encoder_inst_ptr_);
    encoder_inst_ptr_ = NULL;
  }
}

int16_t ACMOpus::InternalCreateDecoder() {
  // Real decoder will be created in InternalInitDecoder
  return 0;
}

void ACMOpus::DestructDecoderSafe() {
  decoder_initialized_ = false;
  if (decoder_inst_ptr_) {
    WebRtcOpus_DecoderFree(decoder_inst_ptr_);
    decoder_inst_ptr_ = NULL;
  }
}

void ACMOpus::InternalDestructEncoderInst(void* ptr_inst) {
  if (ptr_inst != NULL) {
    WebRtcOpus_EncoderFree((OpusEncInst*) ptr_inst);
  }
  return;
}

int16_t ACMOpus::SetBitRateSafe(const int32_t rate) {
  if (rate < 6000 || rate > 510000) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "SetBitRateSafe: Invalid rate Opus");
    return -1;
  }

  bitrate_ = rate;

  // Ask the encoder for the new rate.
  if (WebRtcOpus_SetBitRate(encoder_inst_ptr_, bitrate_) >= 0) {
    encoder_params_.codec_inst.rate = bitrate_;
    return 0;
  }

  return -1;
}

bool ACMOpus::IsTrueStereoCodec() {
  return true;
}

// Copy the stereo packet so that NetEq will insert into both master and slave.
void ACMOpus::SplitStereoPacket(uint8_t* payload, int32_t* payload_length) {
  // Check for valid inputs.
  assert(payload != NULL);
  assert(*payload_length > 0);

  // Duplicate the payload.
  memcpy(&payload[*payload_length], &payload[0],
         sizeof(uint8_t) * (*payload_length));
  // Double the size of the packet.
  *payload_length *= 2;
}

#endif  // WEBRTC_CODEC_OPUS

}  // namespace webrtc
