/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "acm_amr.h"
#include "acm_common_defs.h"
#include "acm_neteq.h"
#include "audio_coding_module_typedefs.h"
#include "rw_lock_wrapper.h"
#include "trace.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_help_macros.h"

#ifdef WEBRTC_CODEC_AMR
// NOTE! GSM AMR is not included in the open-source package. The following
// interface file is needed:
//
// /modules/audio_coding/codecs/amr/main/interface/amr_interface.h
//
// The API in the header file should match the one below.
//
// int16_t WebRtcAmr_CreateEnc(AMR_encinst_t_** encInst);
// int16_t WebRtcAmr_CreateDec(AMR_decinst_t_** decInst);
// int16_t WebRtcAmr_FreeEnc(AMR_encinst_t_* encInst);
// int16_t WebRtcAmr_FreeDec(AMR_decinst_t_* decInst);
// int16_t WebRtcAmr_Encode(AMR_encinst_t_* encInst,
//                          int16_t* input,
//                          int16_t len,
//                          int16_t*output,
//                          int16_t mode);
//  int16_t WebRtcAmr_EncoderInit(AMR_encinst_t_* encInst,
//                               int16_t dtxMode);
// int16_t WebRtcAmr_EncodeBitmode(AMR_encinst_t_* encInst,
//                                 int format);
// int16_t WebRtcAmr_Decode(AMR_decinst_t_* decInst);
// int16_t WebRtcAmr_DecodePlc(AMR_decinst_t_* decInst);
// int16_t WebRtcAmr_DecoderInit(AMR_decinst_t_* decInst);
// int16_t WebRtcAmr_DecodeBitmode(AMR_decinst_t_* decInst,
//                                 int format);
#include "amr_interface.h"
#endif

namespace webrtc {

#ifndef WEBRTC_CODEC_AMR
ACMAMR::ACMAMR(WebRtc_Word16 /* codecID */)
    : _encoderInstPtr(NULL),
      _decoderInstPtr(NULL),
      _encodingMode(-1),  // Invalid value.
      _encodingRate(0),  // Invalid value.
      _encoderPackingFormat(AMRBandwidthEfficient),
      _decoderPackingFormat(AMRBandwidthEfficient) {
  return;
}

ACMAMR::~ACMAMR() {
  return;
}

WebRtc_Word16 ACMAMR::InternalEncode(WebRtc_UWord8* /* bitStream */,
                                     WebRtc_Word16* /* bitStreamLenByte */) {
  return -1;
}

WebRtc_Word16 ACMAMR::DecodeSafe(WebRtc_UWord8* /* bitStream */,
                                 WebRtc_Word16 /* bitStreamLenByte */,
                                 WebRtc_Word16* /* audio */,
                                 WebRtc_Word16* /* audioSamples */,
                                 WebRtc_Word8* /* speechType */) {
  return -1;
}

WebRtc_Word16 ACMAMR::EnableDTX() {
  return -1;
}

WebRtc_Word16 ACMAMR::DisableDTX() {
  return -1;
}

WebRtc_Word16 ACMAMR::InternalInitEncoder(
    WebRtcACMCodecParams* /* codecParams */) {
  return -1;
}

WebRtc_Word16 ACMAMR::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */) {
  return -1;
}

WebRtc_Word32 ACMAMR::CodecDef(WebRtcNetEQ_CodecDef& /* codecDef */,
                               const CodecInst& /* codecInst */) {
  return -1;
}

ACMGenericCodec* ACMAMR::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMAMR::InternalCreateEncoder() {
  return -1;
}

void ACMAMR::DestructEncoderSafe() {
  return;
}

WebRtc_Word16 ACMAMR::InternalCreateDecoder() {
  return -1;
}

void ACMAMR::DestructDecoderSafe() {
  return;
}

WebRtc_Word16 ACMAMR::SetBitRateSafe(const WebRtc_Word32 /* rate */) {
  return -1;
}

void ACMAMR::InternalDestructEncoderInst(void* /* ptrInst */) {
  return;
}

WebRtc_Word16 ACMAMR::SetAMREncoderPackingFormat(
    ACMAMRPackingFormat /* packingFormat */) {
  return -1;
}

ACMAMRPackingFormat ACMAMR::AMREncoderPackingFormat() const {
  return AMRUndefined;
}

WebRtc_Word16 ACMAMR::SetAMRDecoderPackingFormat(
    ACMAMRPackingFormat /* packingFormat */) {
  return -1;
}

ACMAMRPackingFormat ACMAMR::AMRDecoderPackingFormat() const {
  return AMRUndefined;
}

#else     //===================== Actual Implementation =======================

#define WEBRTC_AMR_MR475  0
#define WEBRTC_AMR_MR515  1
#define WEBRTC_AMR_MR59   2
#define WEBRTC_AMR_MR67   3
#define WEBRTC_AMR_MR74   4
#define WEBRTC_AMR_MR795  5
#define WEBRTC_AMR_MR102  6
#define WEBRTC_AMR_MR122  7

ACMAMR::ACMAMR(WebRtc_Word16 codecID)
    : _encoderInstPtr(NULL),
      _decoderInstPtr(NULL),
      _encodingMode(-1), // invalid value
      _encodingRate(0) { // invalid value
  _codecID = codecID;
  _hasInternalDTX = true;
  _encoderPackingFormat = AMRBandwidthEfficient;
  _decoderPackingFormat = AMRBandwidthEfficient;
  return;
}

ACMAMR::~ACMAMR() {
  if (_encoderInstPtr != NULL) {
    WebRtcAmr_FreeEnc(_encoderInstPtr);
    _encoderInstPtr = NULL;
  }
  if (_decoderInstPtr != NULL) {
    WebRtcAmr_FreeDec(_decoderInstPtr);
    _decoderInstPtr = NULL;
  }
  return;
}

WebRtc_Word16 ACMAMR::InternalEncode(WebRtc_UWord8* bitStream,
                                     WebRtc_Word16* bitStreamLenByte) {
  WebRtc_Word16 vadDecision = 1;
  // sanity check, if the rate is set correctly. we might skip this
  // sanity check. if rate is not set correctly, initialization flag
  // should be false and should not be here.
  if ((_encodingMode < WEBRTC_AMR_MR475) ||
      (_encodingMode > WEBRTC_AMR_MR122)) {
    *bitStreamLenByte = 0;
    return -1;
  }
  *bitStreamLenByte = WebRtcAmr_Encode(_encoderInstPtr,
                                       &_inAudio[_inAudioIxRead],
                                       _frameLenSmpl,
                                       (WebRtc_Word16*) bitStream,
                                       _encodingMode);

  // Update VAD, if internal DTX is used
  if (_hasInternalDTX && _dtxEnabled) {
    if (*bitStreamLenByte <= (7 * _frameLenSmpl / 160)) {
      vadDecision = 0;
    }
    for (WebRtc_Word16 n = 0; n < MAX_FRAME_SIZE_10MSEC; n++) {
      _vadLabel[n] = vadDecision;
    }
  }
  // increment the read index
  _inAudioIxRead += _frameLenSmpl;
  return *bitStreamLenByte;
}

WebRtc_Word16 ACMAMR::DecodeSafe(WebRtc_UWord8* /* bitStream */,
                                 WebRtc_Word16 /* bitStreamLenByte */,
                                 WebRtc_Word16* /* audio */,
                                 WebRtc_Word16* /* audioSamples */,
                                 WebRtc_Word8* /* speechType */) {
  return 0;
}

WebRtc_Word16 ACMAMR::EnableDTX() {
  if (_dtxEnabled) {
    return 0;
  } else if (_encoderExist) { // check if encoder exist
    // enable DTX
    if (WebRtcAmr_EncoderInit(_encoderInstPtr, 1) < 0) {
      return -1;
    }
    _dtxEnabled = true;
    return 0;
  } else {
    return -1;
  }
}

WebRtc_Word16 ACMAMR::DisableDTX() {
  if (!_dtxEnabled) {
    return 0;
  } else if (_encoderExist) { // check if encoder exist
    // disable DTX
    if (WebRtcAmr_EncoderInit(_encoderInstPtr, 0) < 0) {
      return -1;
    }
    _dtxEnabled = false;
    return 0;
  } else {
    // encoder doesn't exists, therefore disabling is harmless
    return 0;
  }
}

WebRtc_Word16 ACMAMR::InternalInitEncoder(WebRtcACMCodecParams* codecParams) {
  WebRtc_Word16 status = SetBitRateSafe((codecParams->codecInstant).rate);
  status += (WebRtcAmr_EncoderInit(
      _encoderInstPtr, ((codecParams->enableDTX) ? 1 : 0)) < 0) ? -1 : 0;
  status += (WebRtcAmr_EncodeBitmode(
      _encoderInstPtr, _encoderPackingFormat) < 0) ? -1 : 0;
  return (status < 0) ? -1 : 0;
}

WebRtc_Word16 ACMAMR::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */) {
  WebRtc_Word16 status =
      ((WebRtcAmr_DecoderInit(_decoderInstPtr) < 0) ? -1 : 0);
  status += WebRtcAmr_DecodeBitmode(_decoderInstPtr, _decoderPackingFormat);
  return (status < 0) ? -1 : 0;
}

WebRtc_Word32 ACMAMR::CodecDef(WebRtcNetEQ_CodecDef& codecDef,
                               const CodecInst& codecInst) {
  if (!_decoderInitialized) {
    // Todo:
    // log error
    return -1;
  }
  // Fill up the structure by calling
  // "SET_CODEC_PAR" & "SET_AMR_FUNCTION."
  // Then call NetEQ to add the codec to it's
  // database.
  SET_CODEC_PAR((codecDef), kDecoderAMR, codecInst.pltype, _decoderInstPtr,
                8000);
  SET_AMR_FUNCTIONS((codecDef));
  return 0;
}

ACMGenericCodec* ACMAMR::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMAMR::InternalCreateEncoder() {
  return WebRtcAmr_CreateEnc(&_encoderInstPtr);
}

void ACMAMR::DestructEncoderSafe() {
  if (_encoderInstPtr != NULL) {
    WebRtcAmr_FreeEnc(_encoderInstPtr);
    _encoderInstPtr = NULL;
  }
  // there is no encoder set the following
  _encoderExist = false;
  _encoderInitialized = false;
  _encodingMode = -1; // invalid value
  _encodingRate = 0; // invalid value
}

WebRtc_Word16 ACMAMR::InternalCreateDecoder() {
  return WebRtcAmr_CreateDec(&_decoderInstPtr);
}

void ACMAMR::DestructDecoderSafe() {
  if (_decoderInstPtr != NULL) {
    WebRtcAmr_FreeDec(_decoderInstPtr);
    _decoderInstPtr = NULL;
  }
  // there is no encoder instance set the followings
  _decoderExist = false;
  _decoderInitialized = false;
}

WebRtc_Word16 ACMAMR::SetBitRateSafe(const WebRtc_Word32 rate) {
  switch (rate) {
    case 4750: {
      _encodingMode = WEBRTC_AMR_MR475;
      _encodingRate = 4750;
      break;
    }
    case 5150: {
      _encodingMode = WEBRTC_AMR_MR515;
      _encodingRate = 5150;
      break;
    }
    case 5900: {
      _encodingMode = WEBRTC_AMR_MR59;
      _encodingRate = 5900;
      break;
    }
    case 6700: {
      _encodingMode = WEBRTC_AMR_MR67;
      _encodingRate = 6700;
      break;
    }
    case 7400: {
      _encodingMode = WEBRTC_AMR_MR74;
      _encodingRate = 7400;
      break;
    }
    case 7950: {
      _encodingMode = WEBRTC_AMR_MR795;
      _encodingRate = 7950;
      break;
    }
    case 10200: {
      _encodingMode = WEBRTC_AMR_MR102;
      _encodingRate = 10200;
      break;
    }
    case 12200: {
      _encodingMode = WEBRTC_AMR_MR122;
      _encodingRate = 12200;
      break;
    }
    default: {
      return -1;
    }
  }
  return 0;
}

void ACMAMR::InternalDestructEncoderInst(void* ptrInst) {
  // Free the memory where ptrInst is pointing to
  if (ptrInst != NULL) {
    WebRtcAmr_FreeEnc(reinterpret_cast<AMR_encinst_t_*>(ptrInst));
  }
  return;
}

WebRtc_Word16 ACMAMR::SetAMREncoderPackingFormat(
    ACMAMRPackingFormat packingFormat) {
  if ((packingFormat != AMRBandwidthEfficient) &&
      (packingFormat != AMROctetAlligned) &&
      (packingFormat != AMRFileStorage)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "Invalid AMR Encoder packing-format.");
    return -1;
  } else {
    if (WebRtcAmr_EncodeBitmode(_encoderInstPtr, packingFormat) < 0) {
      return -1;
    } else {
      _encoderPackingFormat = packingFormat;
      return 0;
    }
  }
}

ACMAMRPackingFormat ACMAMR::AMREncoderPackingFormat() const {
  return _encoderPackingFormat;
}

WebRtc_Word16 ACMAMR::SetAMRDecoderPackingFormat(
    ACMAMRPackingFormat packingFormat) {
  if ((packingFormat != AMRBandwidthEfficient) &&
      (packingFormat != AMROctetAlligned) &&
      (packingFormat != AMRFileStorage)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "Invalid AMR decoder packing-format.");
    return -1;
  } else {
    if (WebRtcAmr_DecodeBitmode(_decoderInstPtr, packingFormat) < 0) {
      return -1;
    } else {
      _decoderPackingFormat = packingFormat;
      return 0;
    }
  }
}

ACMAMRPackingFormat ACMAMR::AMRDecoderPackingFormat() const {
  return _decoderPackingFormat;
}

#endif

}
