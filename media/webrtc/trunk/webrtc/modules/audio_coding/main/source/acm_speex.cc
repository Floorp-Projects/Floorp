/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/source/acm_speex.h"

#include "webrtc/modules/audio_coding/main/source/acm_codec_database.h"
#include "webrtc/modules/audio_coding/main/source/acm_common_defs.h"
#include "webrtc/modules/audio_coding/main/source/acm_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq_help_macros.h"
#include "webrtc/system_wrappers/interface/trace.h"

#ifdef WEBRTC_CODEC_SPEEX
// NOTE! Speex is not included in the open-source package. Modify this file or
// your codec API to match the function calls and names of used Speex API file.
#include "speex_interface.h"
#endif

namespace webrtc {

#ifndef WEBRTC_CODEC_SPEEX
ACMSPEEX::ACMSPEEX(int16_t /* codec_id */)
    : encoder_inst_ptr_(NULL),
      decoder_inst_ptr_(NULL),
      compl_mode_(0),
      vbr_enabled_(false),
      encoding_rate_(-1),
      sampling_frequency_(-1),
      samples_in_20ms_audio_(-1) {
  return;
}

ACMSPEEX::~ACMSPEEX() {
  return;
}

int16_t ACMSPEEX::InternalEncode(
    uint8_t* /* bitstream */,
    int16_t* /* bitstream_len_byte */) {
  return -1;
}

int16_t ACMSPEEX::DecodeSafe(uint8_t* /* bitstream */,
                             int16_t /* bitstream_len_byte */,
                             int16_t* /* audio */,
                             int16_t* /* audio_samples */,
                             int8_t* /* speech_type */) {
  return -1;
}

int16_t ACMSPEEX::EnableDTX() {
  return -1;
}

int16_t ACMSPEEX::DisableDTX() {
  return -1;
}

int16_t ACMSPEEX::InternalInitEncoder(
    WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

int16_t ACMSPEEX::InternalInitDecoder(
    WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

int32_t ACMSPEEX::CodecDef(WebRtcNetEQ_CodecDef& /* codec_def */,
                           const CodecInst& /* codec_inst */) {
  return -1;
}

ACMGenericCodec* ACMSPEEX::CreateInstance(void) {
  return NULL;
}

int16_t ACMSPEEX::InternalCreateEncoder() {
  return -1;
}

void ACMSPEEX::DestructEncoderSafe() {
  return;
}

int16_t ACMSPEEX::InternalCreateDecoder() {
  return -1;
}

void ACMSPEEX::DestructDecoderSafe() {
  return;
}

int16_t ACMSPEEX::SetBitRateSafe(const int32_t /* rate */) {
  return -1;
}

void ACMSPEEX::InternalDestructEncoderInst(void* /* ptr_inst */) {
  return;
}

#ifdef UNUSEDSPEEX
int16_t ACMSPEEX::EnableVBR() {
  return -1;
}

int16_t ACMSPEEX::DisableVBR() {
  return -1;
}

int16_t ACMSPEEX::SetComplMode(int16_t mode) {
  return -1;
}
#endif

#else  //===================== Actual Implementation =======================

ACMSPEEX::ACMSPEEX(int16_t codec_id)
    : encoder_inst_ptr_(NULL),
      decoder_inst_ptr_(NULL) {
  codec_id_ = codec_id;

  // Set sampling frequency, frame size and rate Speex
  if (codec_id_ == ACMCodecDB::kSPEEX8) {
    sampling_frequency_ = 8000;
    samples_in_20ms_audio_ = 160;
    encoding_rate_ = 11000;
  } else if (codec_id_ == ACMCodecDB::kSPEEX16) {
    sampling_frequency_ = 16000;
    samples_in_20ms_audio_ = 320;
    encoding_rate_ = 22000;
  } else {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "Wrong codec id for Speex.");

    sampling_frequency_ = -1;
    samples_in_20ms_audio_ = -1;
    encoding_rate_ = -1;
  }

  has_internal_dtx_ = true;
  dtx_enabled_ = false;
  vbr_enabled_ = false;
  compl_mode_ = 3;  // default complexity value

  return;
}

ACMSPEEX::~ACMSPEEX() {
  if (encoder_inst_ptr_ != NULL) {
    WebRtcSpeex_FreeEnc(encoder_inst_ptr_);
    encoder_inst_ptr_ = NULL;
  }
  if (decoder_inst_ptr_ != NULL) {
    WebRtcSpeex_FreeDec(decoder_inst_ptr_);
    decoder_inst_ptr_ = NULL;
  }
  return;
}

int16_t ACMSPEEX::InternalEncode(uint8_t* bitstream,
                                 int16_t* bitstream_len_byte) {
  int16_t status;
  int16_t num_encoded_samples = 0;
  int16_t n = 0;

  while (num_encoded_samples < frame_len_smpl_) {
    status = WebRtcSpeex_Encode(encoder_inst_ptr_,
                                &in_audio_[in_audio_ix_read_], encoding_rate_);

    // increment the read index this tell the caller that how far
    // we have gone forward in reading the audio buffer
    in_audio_ix_read_ += samples_in_20ms_audio_;
    num_encoded_samples += samples_in_20ms_audio_;

    if (status < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                   "Error in Speex encoder");
      return status;
    }

    // Update VAD, if internal DTX is used
    if (has_internal_dtx_ && dtx_enabled_) {
      vad_label_[n++] = status;
      vad_label_[n++] = status;
    }

    if (status == 0) {
      // This frame is detected as inactive. We need send whatever
      // encoded so far.
      *bitstream_len_byte = WebRtcSpeex_GetBitstream(encoder_inst_ptr_,
                                                     (int16_t*)bitstream);
      return *bitstream_len_byte;
    }
  }

  *bitstream_len_byte = WebRtcSpeex_GetBitstream(encoder_inst_ptr_,
                                                 (int16_t*)bitstream);
  return *bitstream_len_byte;
}

int16_t ACMSPEEX::DecodeSafe(uint8_t* /* bitstream */,
                             int16_t /* bitstream_len_byte */,
                             int16_t* /* audio */,
                             int16_t* /* audio_samples */,
                             int8_t* /* speech_type */) {
  return 0;
}

int16_t ACMSPEEX::EnableDTX() {
  if (dtx_enabled_) {
    return 0;
  } else if (encoder_exist_) {  // check if encoder exist
    // enable DTX
    if (WebRtcSpeex_EncoderInit(encoder_inst_ptr_, (vbr_enabled_ ? 1 : 0),
                                compl_mode_, 1) < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                   "Cannot enable DTX for Speex");
      return -1;
    }
    dtx_enabled_ = true;
    return 0;
  } else {
    return -1;
  }

  return 0;
}

int16_t ACMSPEEX::DisableDTX() {
  if (!dtx_enabled_) {
    return 0;
  } else if (encoder_exist_) {  // check if encoder exist
    // disable DTX
    if (WebRtcSpeex_EncoderInit(encoder_inst_ptr_, (vbr_enabled_ ? 1 : 0),
                                compl_mode_, 0) < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                   "Cannot disable DTX for Speex");
      return -1;
    }
    dtx_enabled_ = false;
    return 0;
  } else {
    // encoder doesn't exists, therefore disabling is harmless
    return 0;
  }

  return 0;
}

int16_t ACMSPEEX::InternalInitEncoder(
    WebRtcACMCodecParams* codec_params) {
  // sanity check
  if (encoder_inst_ptr_ == NULL) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "Cannot initialize Speex encoder, instance does not exist");
    return -1;
  }

  int16_t status = SetBitRateSafe((codec_params->codecInstant).rate);
  status +=
      (WebRtcSpeex_EncoderInit(encoder_inst_ptr_, vbr_enabled_, compl_mode_,
                               ((codec_params->enable_dtx) ? 1 : 0)) < 0) ?
                                   -1 : 0;

  if (status >= 0) {
    return 0;
  } else {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "Error in initialization of Speex encoder");
    return -1;
  }
}

int16_t ACMSPEEX::InternalInitDecoder(
    WebRtcACMCodecParams* /* codec_params */) {
  int16_t status;

  // sanity check
  if (decoder_inst_ptr_ == NULL) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "Cannot initialize Speex decoder, instance does not exist");
    return -1;
  }
  status = ((WebRtcSpeex_DecoderInit(decoder_inst_ptr_) < 0) ? -1 : 0);

  if (status >= 0) {
    return 0;
  } else {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "Error in initialization of Speex decoder");
    return -1;
  }
}

int32_t ACMSPEEX::CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                           const CodecInst& codec_inst) {
  if (!decoder_initialized_) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "Error, Speex decoder is not initialized");
    return -1;
  }

  // Fill up the structure by calling
  // "SET_CODEC_PAR" & "SET_SPEEX_FUNCTION."
  // Then call NetEQ to add the codec to its
  // database.

  switch (sampling_frequency_) {
    case 8000: {
      SET_CODEC_PAR((codec_def), kDecoderSPEEX_8, codec_inst.pltype,
                    decoder_inst_ptr_, 8000);
      break;
    }
    case 16000: {
      SET_CODEC_PAR((codec_def), kDecoderSPEEX_16, codec_inst.pltype,
                    decoder_inst_ptr_, 16000);
      break;
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                   "Unsupported sampling frequency for Speex");

      return -1;
    }
  }

  SET_SPEEX_FUNCTIONS((codec_def));
  return 0;
}

ACMGenericCodec* ACMSPEEX::CreateInstance(void) {
  return NULL;
}

int16_t ACMSPEEX::InternalCreateEncoder() {
  return WebRtcSpeex_CreateEnc(&encoder_inst_ptr_, sampling_frequency_);
}

void ACMSPEEX::DestructEncoderSafe() {
  if (encoder_inst_ptr_ != NULL) {
    WebRtcSpeex_FreeEnc(encoder_inst_ptr_);
    encoder_inst_ptr_ = NULL;
  }
  // there is no encoder set the following
  encoder_exist_ = false;
  encoder_initialized_ = false;
  encoding_rate_ = 0;
}

int16_t ACMSPEEX::InternalCreateDecoder() {
  return WebRtcSpeex_CreateDec(&decoder_inst_ptr_, sampling_frequency_, 1);
}

void ACMSPEEX::DestructDecoderSafe() {
  if (decoder_inst_ptr_ != NULL) {
    WebRtcSpeex_FreeDec(decoder_inst_ptr_);
    decoder_inst_ptr_ = NULL;
  }
  // there is no encoder instance set the followings
  decoder_exist_ = false;
  decoder_initialized_ = false;
}

int16_t ACMSPEEX::SetBitRateSafe(const int32_t rate) {
  // Check if changed rate
  if (rate == encoding_rate_) {
    return 0;
  } else if (rate > 2000) {
    encoding_rate_ = rate;
    encoder_params_.codecInstant.rate = rate;
  } else {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                 "Unsupported encoding rate for Speex");

    return -1;
  }

  return 0;
}

void ACMSPEEX::InternalDestructEncoderInst(void* ptr_inst) {
  if (ptr_inst != NULL) {
    WebRtcSpeex_FreeEnc((SPEEX_encinst_t_*) ptr_inst);
  }
  return;
}

#ifdef UNUSEDSPEEX

// This API is currently not in use. If requested to be able to enable/disable
// VBR an ACM API need to be added.
int16_t ACMSPEEX::EnableVBR() {
  if (vbr_enabled_) {
    return 0;
  } else if (encoder_exist_) {  // check if encoder exist
    // enable Variable Bit Rate (VBR)
    if (WebRtcSpeex_EncoderInit(encoder_inst_ptr_, 1, compl_mode_,
                                (dtx_enabled_ ? 1 : 0)) < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                   "Cannot enable VBR mode for Speex");

      return -1;
    }
    vbr_enabled_ = true;
    return 0;
  } else {
    return -1;
  }
}

// This API is currently not in use. If requested to be able to enable/disable
// VBR an ACM API need to be added.
int16_t ACMSPEEX::DisableVBR() {
  if (!vbr_enabled_) {
    return 0;
  } else if (encoder_exist_) {  // check if encoder exist
    // disable DTX
    if (WebRtcSpeex_EncoderInit(encoder_inst_ptr_, 0, compl_mode_,
                                (dtx_enabled_ ? 1 : 0)) < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                   "Cannot disable DTX for Speex");

      return -1;
    }
    vbr_enabled_ = false;
    return 0;
  } else {
    // encoder doesn't exists, therefore disabling is harmless
    return 0;
  }
}

// This API is currently not in use. If requested to be able to set complexity
// an ACM API need to be added.
int16_t ACMSPEEX::SetComplMode(int16_t mode) {
  // Check if new mode
  if (mode == compl_mode_) {
    return 0;
  } else if (encoder_exist_) {  // check if encoder exist
    // Set new mode
    if (WebRtcSpeex_EncoderInit(encoder_inst_ptr_, 0, mode,
                                (dtx_enabled_ ? 1 : 0)) < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, unique_id_,
                   "Error in complexity mode for Speex");
      return -1;
    }
    compl_mode_ = mode;
    return 0;
  } else {
    // encoder doesn't exists, therefore disabling is harmless
    return 0;
  }
}

#endif

#endif

}  // namespace webrtc
