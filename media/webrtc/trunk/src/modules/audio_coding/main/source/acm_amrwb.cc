/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "acm_amrwb.h"
#include "acm_common_defs.h"
#include "acm_neteq.h"
#include "audio_coding_module_typedefs.h"
#include "rw_lock_wrapper.h"
#include "trace.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_help_macros.h"

#ifdef WEBRTC_CODEC_AMRWB
// NOTE! GSM AMR-wb is not included in the open-source package. The
// following interface file is needed:
//
// /modules/audio_coding/codecs/amrwb/main/interface/amrwb_interface.h
//
// The API in the header file should match the one below.
//
// int16_t WebRtcAmrWb_CreateEnc(AMRWB_encinst_t_** encInst);
// int16_t WebRtcAmrWb_CreateDec(AMRWB_decinst_t_** decInst);
// int16_t WebRtcAmrWb_FreeEnc(AMRWB_encinst_t_* encInst);
// int16_t WebRtcAmrWb_FreeDec(AMRWB_decinst_t_* decInst);
// int16_t WebRtcAmrWb_Encode(AMRWB_encinst_t_* encInst, int16_t* input,
//                            int16_t len, int16_t* output, int16_t mode);
// int16_t WebRtcAmrWb_EncoderInit(AMRWB_encinst_t_* encInst,
//                                 int16_t dtxMode);
// int16_t WebRtcAmrWb_EncodeBitmode(AMRWB_encinst_t_* encInst,
//                                    int format);
// int16_t WebRtcAmrWb_Decode(AMRWB_decinst_t_* decInst);
// int16_t WebRtcAmrWb_DecodePlc(AMRWB_decinst_t_* decInst);
// int16_t WebRtcAmrWb_DecoderInit(AMRWB_decinst_t_* decInst);
// int16_t WebRtcAmrWb_DecodeBitmode(AMRWB_decinst_t_* decInst,
//                                   int format);
// void WebRtcAmrWb_Version(char *versionStr, short len);
#include "amrwb_interface.h"
#endif

namespace webrtc {

#ifndef WEBRTC_CODEC_AMRWB
ACMAMRwb::ACMAMRwb(WebRtc_Word16 /* codecID*/)
    : _encoderInstPtr(NULL),
      _decoderInstPtr(NULL),
      _encodingMode(-1),  // invalid value
      _encodingRate(0),  // invalid value
      _encoderPackingFormat(AMRBandwidthEfficient),
      _decoderPackingFormat(AMRBandwidthEfficient) {
  return;
}

ACMAMRwb::~ACMAMRwb() {
  return;
}

WebRtc_Word16 ACMAMRwb::InternalEncode(WebRtc_UWord8* /* bitStream */,
                                       WebRtc_Word16* /* bitStreamLenByte */) {
  return -1;
}

WebRtc_Word16 ACMAMRwb::DecodeSafe(WebRtc_UWord8* /* bitStream */,
                                   WebRtc_Word16 /* bitStreamLenByte */,
                                   WebRtc_Word16* /* audio */,
                                   WebRtc_Word16* /* audioSamples */,
                                   WebRtc_Word8* /* speechType */) {
  return -1;
}

WebRtc_Word16 ACMAMRwb::EnableDTX() {
  return -1;
}

WebRtc_Word16 ACMAMRwb::DisableDTX() {
  return -1;
}

WebRtc_Word16 ACMAMRwb::InternalInitEncoder(
    WebRtcACMCodecParams* /* codecParams */) {
  return -1;
}

WebRtc_Word16 ACMAMRwb::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */) {
  return -1;
}

WebRtc_Word32 ACMAMRwb::CodecDef(WebRtcNetEQ_CodecDef& /* codecDef */,
                                 const CodecInst& /* codecInst */) {
  return -1;
}

ACMGenericCodec*
ACMAMRwb::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMAMRwb::InternalCreateEncoder() {
  return -1;
}

void ACMAMRwb::DestructEncoderSafe() {
  return;
}

WebRtc_Word16 ACMAMRwb::InternalCreateDecoder() {
  return -1;
}

void ACMAMRwb::DestructDecoderSafe() {
  return;
}

WebRtc_Word16 ACMAMRwb::SetBitRateSafe(const WebRtc_Word32 /* rate */) {
  return -1;
}

void ACMAMRwb::InternalDestructEncoderInst(void* /* ptrInst */) {
  return;
}

WebRtc_Word16 ACMAMRwb::SetAMRwbEncoderPackingFormat(
    ACMAMRPackingFormat /* packingFormat */) {
  return -1;
}

ACMAMRPackingFormat ACMAMRwb::AMRwbEncoderPackingFormat() const {
  return AMRUndefined;
}

WebRtc_Word16 ACMAMRwb::SetAMRwbDecoderPackingFormat(
    ACMAMRPackingFormat /* packingFormat */) {
  return -1;
}

ACMAMRPackingFormat ACMAMRwb::AMRwbDecoderPackingFormat() const {
  return AMRUndefined;
}

WebRtc_Word16 ACMAMRwb::UnregisterFromNetEqSafe(
    ACMNetEQ* /* netEq */,
    WebRtc_Word16 /* payloadType */) {
  return -1;
}

#else     //===================== Actual Implementation =======================

#define AMRWB_MODE_7k       0
#define AMRWB_MODE_9k       1
#define AMRWB_MODE_12k      2
#define AMRWB_MODE_14k      3
#define AMRWB_MODE_16k      4
#define AMRWB_MODE_18k      5
#define AMRWB_MODE_20k      6
#define AMRWB_MODE_23k      7
#define AMRWB_MODE_24k      8

ACMAMRwb::ACMAMRwb(WebRtc_Word16 codecID)
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

ACMAMRwb::~ACMAMRwb() {
  if (_encoderInstPtr != NULL) {
    WebRtcAmrWb_FreeEnc(_encoderInstPtr);
    _encoderInstPtr = NULL;
  }
  if (_decoderInstPtr != NULL) {
    WebRtcAmrWb_FreeDec(_decoderInstPtr);
    _decoderInstPtr = NULL;
  }
  return;
}

WebRtc_Word16 ACMAMRwb::InternalEncode(WebRtc_UWord8* bitStream,
                                       WebRtc_Word16* bitStreamLenByte) {
  WebRtc_Word16 vadDecision = 1;
  // sanity check, if the rate is set correctly. we might skip this
  // sanity check. if rate is not set correctly, initialization flag
  // should be false and should not be here.
  if ((_encodingMode < AMRWB_MODE_7k) || (_encodingMode > AMRWB_MODE_24k)) {
    *bitStreamLenByte = 0;
    return -1;
  }
  *bitStreamLenByte = WebRtcAmrWb_Encode(_encoderInstPtr,
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
  // increment the read index this tell the caller that how far
  // we have gone forward in reading the audio buffer
  _inAudioIxRead += _frameLenSmpl;
  return *bitStreamLenByte;
}

WebRtc_Word16 ACMAMRwb::DecodeSafe(WebRtc_UWord8* /* bitStream */,
                                   WebRtc_Word16 /* bitStreamLenByte */,
                                   WebRtc_Word16* /* audio */,
                                   WebRtc_Word16* /* audioSamples */,
                                   WebRtc_Word8* /* speechType */) {
  return 0;
}

WebRtc_Word16 ACMAMRwb::EnableDTX() {
  if (_dtxEnabled) {
    return 0;
  } else if (_encoderExist) { // check if encoder exist
    // enable DTX
    if (WebRtcAmrWb_EncoderInit(_encoderInstPtr, 1) < 0) {
      return -1;
    }
    _dtxEnabled = true;
    return 0;
  } else {
    return -1;
  }
}

WebRtc_Word16 ACMAMRwb::DisableDTX() {
  if (!_dtxEnabled) {
    return 0;
  } else if (_encoderExist) { // check if encoder exist
    // disable DTX
    if (WebRtcAmrWb_EncoderInit(_encoderInstPtr, 0) < 0) {
      return -1;
    }
    _dtxEnabled = false;
    return 0;
  } else {
    // encoder doesn't exists, therefore disabling is harmless
    return 0;
  }
}

WebRtc_Word16 ACMAMRwb::InternalInitEncoder(WebRtcACMCodecParams* codecParams) {
  // sanity check
  if (_encoderInstPtr == NULL) {
    return -1;
  }

  WebRtc_Word16 status = SetBitRateSafe((codecParams->codecInstant).rate);
  status += (WebRtcAmrWb_EncoderInit(
      _encoderInstPtr, ((codecParams->enableDTX) ? 1 : 0)) < 0) ? -1 : 0;
  status += (WebRtcAmrWb_EncodeBitmode(
      _encoderInstPtr, _encoderPackingFormat) < 0) ? -1 : 0;
  return (status < 0) ? -1 : 0;
}

WebRtc_Word16 ACMAMRwb::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */) {
  WebRtc_Word16 status = WebRtcAmrWb_DecodeBitmode(_decoderInstPtr,
                                                   _decoderPackingFormat);
  status += ((WebRtcAmrWb_DecoderInit(_decoderInstPtr) < 0) ? -1 : 0);
  return (status < 0) ? -1 : 0;
}

WebRtc_Word32 ACMAMRwb::CodecDef(WebRtcNetEQ_CodecDef& codecDef,
                                 const CodecInst& codecInst) {
  if (!_decoderInitialized) {
    return -1;
  }

  // Fill up the structure by calling
  // "SET_CODEC_PAR" & "SET_AMRWB_FUNCTION."
  // Then call NetEQ to add the codec to it's
  // database.
  SET_CODEC_PAR((codecDef), kDecoderAMRWB, codecInst.pltype, _decoderInstPtr,
                16000);
  SET_AMRWB_FUNCTIONS((codecDef));
  return 0;
}

ACMGenericCodec* ACMAMRwb::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMAMRwb::InternalCreateEncoder() {
  return WebRtcAmrWb_CreateEnc(&_encoderInstPtr);
}

void ACMAMRwb::DestructEncoderSafe() {
  if (_encoderInstPtr != NULL) {
    WebRtcAmrWb_FreeEnc(_encoderInstPtr);
    _encoderInstPtr = NULL;
  }
  // there is no encoder set the following
  _encoderExist = false;
  _encoderInitialized = false;
  _encodingMode = -1; // invalid value
  _encodingRate = 0;
}

WebRtc_Word16 ACMAMRwb::InternalCreateDecoder() {
  return WebRtcAmrWb_CreateDec(&_decoderInstPtr);
}

void ACMAMRwb::DestructDecoderSafe() {
  if (_decoderInstPtr != NULL) {
    WebRtcAmrWb_FreeDec(_decoderInstPtr);
    _decoderInstPtr = NULL;
  }
  // there is no encoder instance set the followings
  _decoderExist = false;
  _decoderInitialized = false;
}

WebRtc_Word16 ACMAMRwb::SetBitRateSafe(const WebRtc_Word32 rate) {
  switch (rate) {
    case 7000: {
      _encodingMode = AMRWB_MODE_7k;
      _encodingRate = 7000;
      break;
    }
    case 9000: {
      _encodingMode = AMRWB_MODE_9k;
      _encodingRate = 9000;
      break;
    }
    case 12000: {
      _encodingMode = AMRWB_MODE_12k;
      _encodingRate = 12000;
      break;
    }
    case 14000: {
      _encodingMode = AMRWB_MODE_14k;
      _encodingRate = 14000;
      break;
    }
    case 16000: {
      _encodingMode = AMRWB_MODE_16k;
      _encodingRate = 16000;
      break;
    }
    case 18000: {
      _encodingMode = AMRWB_MODE_18k;
      _encodingRate = 18000;
      break;
    }
    case 20000: {
      _encodingMode = AMRWB_MODE_20k;
      _encodingRate = 20000;
      break;
    }
    case 23000: {
      _encodingMode = AMRWB_MODE_23k;
      _encodingRate = 23000;
      break;
    }
    case 24000: {
      _encodingMode = AMRWB_MODE_24k;
      _encodingRate = 24000;
      break;
    }
    default: {
      return -1;
    }
  }
  return 0;
}

void ACMAMRwb::InternalDestructEncoderInst(void* ptrInst) {
  if (ptrInst != NULL) {
    WebRtcAmrWb_FreeEnc(static_cast<AMRWB_encinst_t_*>(ptrInst));
  }
  return;
}

WebRtc_Word16 ACMAMRwb::SetAMRwbEncoderPackingFormat(
    ACMAMRPackingFormat packingFormat) {
  if ((packingFormat != AMRBandwidthEfficient) &&
      (packingFormat != AMROctetAlligned) &&
      (packingFormat != AMRFileStorage)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "Invalid AMRwb encoder packing-format.");
    return -1;
  } else {
    if (WebRtcAmrWb_EncodeBitmode(_encoderInstPtr, packingFormat) < 0) {
      return -1;
    } else {
      _encoderPackingFormat = packingFormat;
      return 0;
    }
  }
}

ACMAMRPackingFormat ACMAMRwb::AMRwbEncoderPackingFormat() const {
  return _encoderPackingFormat;
}

WebRtc_Word16 ACMAMRwb::SetAMRwbDecoderPackingFormat(
    ACMAMRPackingFormat packingFormat) {
  if ((packingFormat != AMRBandwidthEfficient) &&
      (packingFormat != AMROctetAlligned) &&
      (packingFormat != AMRFileStorage)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "Invalid AMRwb decoder packing-format.");
    return -1;
  } else {
    if (WebRtcAmrWb_DecodeBitmode(_decoderInstPtr, packingFormat) < 0) {
      return -1;
    } else {
      _decoderPackingFormat = packingFormat;
      return 0;
    }
  }
}

ACMAMRPackingFormat ACMAMRwb::AMRwbDecoderPackingFormat() const {
  return _decoderPackingFormat;
}

WebRtc_Word16 ACMAMRwb::UnregisterFromNetEqSafe(ACMNetEQ* netEq,
                                                WebRtc_Word16 payloadType) {
  if (payloadType != _decoderParams.codecInstant.pltype) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "Cannot unregister codec %s given payload-type %d does not"
                 "match the stored payload type",
                 _decoderParams.codecInstant.plname, payloadType,
                 _decoderParams.codecInstant.pltype);
    return -1;
  }
  return netEq->RemoveCodec(kDecoderAMRWB);
}
#endif

} // namespace webrtc
