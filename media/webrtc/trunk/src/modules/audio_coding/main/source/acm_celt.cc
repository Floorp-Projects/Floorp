/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "acm_common_defs.h"
#include "acm_neteq.h"
#include "acm_celt.h"
#include "trace.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_help_macros.h"
// TODO(tlegrand): Add full paths.

#ifdef WEBRTC_CODEC_CELT
// NOTE! Celt is not included in the open-source package. Modify this file or
// your codec API to match the function call and name of used Celt API file.
#include "celt_interface.h"
#endif

namespace webrtc {

#ifndef WEBRTC_CODEC_CELT

ACMCELT::ACMCELT(int16_t /* codecID */)
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

int16_t ACMCELT::InternalEncode(uint8_t* /* bitStream */,
                                int16_t* /* bitStreamLenByte */) {
  return -1;
}

int16_t ACMCELT::DecodeSafe(uint8_t* /* bitStream */,
                            int16_t /* bitStreamLenByte */,
                            int16_t* /* audio */,
                            int16_t* /* audioSamples */,
                            WebRtc_Word8* /* speechType */) {
  return -1;
}

int16_t ACMCELT::InternalInitEncoder(WebRtcACMCodecParams* /* codecParams */) {
  return -1;
}

int16_t ACMCELT::InternalInitDecoder(WebRtcACMCodecParams* /* codecParams */) {
  return -1;
}

int32_t ACMCELT::CodecDef(WebRtcNetEQ_CodecDef& /* codecDef  */,
                          const CodecInst& /* codecInst */) {
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

void ACMCELT::InternalDestructEncoderInst(void* /* ptrInst */) {
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

ACMCELT::ACMCELT(int16_t codecID)
    : enc_inst_ptr_(NULL),
      dec_inst_ptr_(NULL),
      sampling_freq_(32000),  // Default sampling frequency.
      bitrate_(64000),  // Default rate.
      channels_(1),  // Default send mono.
      dec_channels_(1) {  // Default receive mono.
  // TODO(tlegrand): remove later when ACMGenericCodec has a new constructor.
  _codecID = codecID;

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

int16_t ACMCELT::InternalEncode(uint8_t* bitStream, int16_t* bitStreamLenByte) {
  *bitStreamLenByte = 0;

  // Call Encoder.
  *bitStreamLenByte = WebRtcCelt_Encode(enc_inst_ptr_,
                                        &_inAudio[_inAudioIxRead],
                                        bitStream);

  // Increment the read index this tell the caller that how far
  // we have gone forward in reading the audio buffer.
  _inAudioIxRead += _frameLenSmpl * channels_;

  if (*bitStreamLenByte < 0) {
    // Error reported from the encoder.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "InternalEncode: Encode error for Celt");
    *bitStreamLenByte = 0;
    return -1;
  }

  return *bitStreamLenByte;
}

int16_t ACMCELT::DecodeSafe(uint8_t* /* bitStream */,
                            int16_t /* bitStreamLenByte */,
                            int16_t* /* audio */,
                            int16_t* /* audioSamples */,
                            WebRtc_Word8* /* speechType */) {
  return 0;
}

int16_t ACMCELT::InternalInitEncoder(WebRtcACMCodecParams* codecParams) {
  // Set bitrate and check that it is within the valid range.
  int16_t status = SetBitRateSafe((codecParams->codecInstant).rate);
  if (status < 0) {
    return -1;
  }

  // If number of channels changed we need to re-create memory.
  if (codecParams->codecInstant.channels != channels_) {
    WebRtcCelt_FreeEnc(enc_inst_ptr_);
    enc_inst_ptr_ = NULL;
    // Store new number of channels.
    channels_ = codecParams->codecInstant.channels;
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

int16_t ACMCELT::InternalInitDecoder(WebRtcACMCodecParams* codecParams) {
  // If number of channels changed we need to re-create memory.
  if (codecParams->codecInstant.channels != dec_channels_) {
    WebRtcCelt_FreeDec(dec_inst_ptr_);
    dec_inst_ptr_ = NULL;
    // Store new number of channels.
    dec_channels_ = codecParams->codecInstant.channels;
    if (WebRtcCelt_CreateDec(&dec_inst_ptr_, dec_channels_) < 0) {
       return -1;
    }
  }

  // Initiate decoder, both master and slave parts.
  if (WebRtcCelt_DecoderInit(dec_inst_ptr_) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "InternalInitDecoder: init decoder failed for Celt.");
    return -1;
  }
  if (WebRtcCelt_DecoderInitSlave(dec_inst_ptr_) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "InternalInitDecoder: init decoder failed for Celt.");
    return -1;
  }
  return 0;
}

int32_t ACMCELT::CodecDef(WebRtcNetEQ_CodecDef& codecDef,
                          const CodecInst& codecInst) {
  if (!_decoderInitialized) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "CodecDef: Decoder uninitialized for Celt");
    return -1;
  }

  // Fill up the structure by calling
  // "SET_CODEC_PAR" and "SET_CELT_FUNCTIONS" or "SET_CELTSLAVE_FUNCTIONS".
  // Then call NetEQ to add the codec to it's
  // database.
  if (codecInst.channels == 1) {
    SET_CODEC_PAR(codecDef, kDecoderCELT_32, codecInst.pltype, dec_inst_ptr_,
                  32000);
  } else {
    SET_CODEC_PAR(codecDef, kDecoderCELT_32_2ch, codecInst.pltype,
                  dec_inst_ptr_, 32000);
  }

  // If this is the master of NetEQ, regular decoder will be added, otherwise
  // the slave decoder will be used.
  if (_isMaster) {
    SET_CELT_FUNCTIONS(codecDef);
  } else {
    SET_CELTSLAVE_FUNCTIONS(codecDef);
  }
  return 0;
}

ACMGenericCodec* ACMCELT::CreateInstance(void) {
  return NULL;
}

int16_t ACMCELT::InternalCreateEncoder() {
  if (WebRtcCelt_CreateEnc(&enc_inst_ptr_, _noChannels) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "InternalCreateEncoder: create encoder failed for Celt");
    return -1;
  }
  channels_ = _noChannels;
  return 0;
}

void ACMCELT::DestructEncoderSafe() {
  _encoderExist = false;
  _encoderInitialized = false;
  if (enc_inst_ptr_ != NULL) {
    WebRtcCelt_FreeEnc(enc_inst_ptr_);
    enc_inst_ptr_ = NULL;
  }
}

int16_t ACMCELT::InternalCreateDecoder() {
  if (WebRtcCelt_CreateDec(&dec_inst_ptr_, dec_channels_) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "InternalCreateDecoder: create decoder failed for Celt");
    return -1;
  }

  return 0;
}

void ACMCELT::DestructDecoderSafe() {
  _decoderExist = false;
  _decoderInitialized = false;
  if (dec_inst_ptr_ != NULL) {
    WebRtcCelt_FreeDec(dec_inst_ptr_);
    dec_inst_ptr_ = NULL;
  }
}

void ACMCELT::InternalDestructEncoderInst(void* ptrInst) {
  if (ptrInst != NULL) {
    WebRtcCelt_FreeEnc(static_cast<CELT_encinst_t*>(ptrInst));
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
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                   "SetBitRateSafe: Failed to initiate Celt with rate %d",
                   rate);
      return -1;
    }
  } else {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
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
