/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "acm_g7221c.h"
#include "acm_codec_database.h"
#include "acm_common_defs.h"
#include "acm_neteq.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_help_macros.h"
#include "trace.h"

#ifdef WEBRTC_CODEC_G722_1C
// NOTE! G.722.1C is not included in the open-source package. The following
// interface file is needed:
//
// /modules/audio_coding/codecs/g7221c/main/interface/g7221c_interface.h
//
// The API in the header file should match the one below.
//
// int16_t WebRtcG7221C_CreateEnc24(G722_1C_24_encinst_t_** encInst);
// int16_t WebRtcG7221C_CreateEnc32(G722_1C_32_encinst_t_** encInst);
// int16_t WebRtcG7221C_CreateEnc48(G722_1C_48_encinst_t_** encInst);
// int16_t WebRtcG7221C_CreateDec24(G722_1C_24_decinst_t_** decInst);
// int16_t WebRtcG7221C_CreateDec32(G722_1C_32_decinst_t_** decInst);
// int16_t WebRtcG7221C_CreateDec48(G722_1C_48_decinst_t_** decInst);
//
// int16_t WebRtcG7221C_FreeEnc24(G722_1C_24_encinst_t_** encInst);
// int16_t WebRtcG7221C_FreeEnc32(G722_1C_32_encinst_t_** encInst);
// int16_t WebRtcG7221C_FreeEnc48(G722_1C_48_encinst_t_** encInst);
// int16_t WebRtcG7221C_FreeDec24(G722_1C_24_decinst_t_** decInst);
// int16_t WebRtcG7221C_FreeDec32(G722_1C_32_decinst_t_** decInst);
// int16_t WebRtcG7221C_FreeDec48(G722_1C_48_decinst_t_** decInst);
//
// int16_t WebRtcG7221C_EncoderInit24(G722_1C_24_encinst_t_* encInst);
// int16_t WebRtcG7221C_EncoderInit32(G722_1C_32_encinst_t_* encInst);
// int16_t WebRtcG7221C_EncoderInit48(G722_1C_48_encinst_t_* encInst);
// int16_t WebRtcG7221C_DecoderInit24(G722_1C_24_decinst_t_* decInst);
// int16_t WebRtcG7221C_DecoderInit32(G722_1C_32_decinst_t_* decInst);
// int16_t WebRtcG7221C_DecoderInit48(G722_1C_48_decinst_t_* decInst);
//
// int16_t WebRtcG7221C_Encode24(G722_1C_24_encinst_t_* encInst,
//                               int16_t* input,
//                               int16_t len,
//                               int16_t* output);
// int16_t WebRtcG7221C_Encode32(G722_1C_32_encinst_t_* encInst,
//                               int16_t* input,
//                               int16_t len,
//                               int16_t* output);
// int16_t WebRtcG7221C_Encode48(G722_1C_48_encinst_t_* encInst,
//                               int16_t* input,
//                               int16_t len,
//                               int16_t* output);
//
// int16_t WebRtcG7221C_Decode24(G722_1C_24_decinst_t_* decInst,
//                               int16_t* bitstream,
//                               int16_t len,
//                               int16_t* output);
// int16_t WebRtcG7221C_Decode32(G722_1C_32_decinst_t_* decInst,
//                               int16_t* bitstream,
//                               int16_t len,
//                               int16_t* output);
// int16_t WebRtcG7221C_Decode48(G722_1C_48_decinst_t_* decInst,
//                               int16_t* bitstream,
//                               int16_t len,
//                               int16_t* output);
//
// int16_t WebRtcG7221C_DecodePlc24(G722_1C_24_decinst_t_* decInst,
//                                  int16_t* output,
//                                  int16_t nrLostFrames);
// int16_t WebRtcG7221C_DecodePlc32(G722_1C_32_decinst_t_* decInst,
//                                  int16_t* output,
//                                  int16_t nrLostFrames);
// int16_t WebRtcG7221C_DecodePlc48(G722_1C_48_decinst_t_* decInst,
//                                  int16_t* output,
//                                  int16_t nrLostFrames);
#include "g7221c_interface.h"
#endif

namespace webrtc {

#ifndef WEBRTC_CODEC_G722_1C

ACMG722_1C::ACMG722_1C(WebRtc_Word16 /* codecID */)
    : _operationalRate(-1),
      _encoderInstPtr(NULL),
      _encoderInstPtrRight(NULL),
      _decoderInstPtr(NULL),
      _encoderInst24Ptr(NULL),
      _encoderInst24PtrR(NULL),
      _encoderInst32Ptr(NULL),
      _encoderInst32PtrR(NULL),
      _encoderInst48Ptr(NULL),
      _encoderInst48PtrR(NULL),
      _decoderInst24Ptr(NULL),
      _decoderInst32Ptr(NULL),
      _decoderInst48Ptr(NULL) {
  return;
}

ACMG722_1C::~ACMG722_1C() {
  return;
}

WebRtc_Word16 ACMG722_1C::InternalEncode(
    WebRtc_UWord8* /* bitStream */,
    WebRtc_Word16* /* bitStreamLenByte */) {
  return -1;
}

WebRtc_Word16 ACMG722_1C::DecodeSafe(WebRtc_UWord8* /* bitStream */,
    WebRtc_Word16 /* bitStreamLenByte */,
    WebRtc_Word16* /* audio */,
    WebRtc_Word16* /* audioSamples */,
    WebRtc_Word8* /* speechType */) {
  return -1;
}

WebRtc_Word16 ACMG722_1C::InternalInitEncoder(
    WebRtcACMCodecParams* /* codecParams */) {
  return -1;
}

WebRtc_Word16 ACMG722_1C::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */) {
  return -1;
}

WebRtc_Word32 ACMG722_1C::CodecDef(WebRtcNetEQ_CodecDef& /* codecDef */,
    const CodecInst& /* codecInst */) {
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

void ACMG722_1C::InternalDestructEncoderInst(void* /* ptrInst */) {
  return;
}

#else     //===================== Actual Implementation =======================
ACMG722_1C::ACMG722_1C(WebRtc_Word16 codecID) :
  _encoderInstPtr(NULL), _encoderInstPtrRight(NULL), _decoderInstPtr(NULL),
      _encoderInst24Ptr(NULL), _encoderInst24PtrR(NULL), _encoderInst32Ptr(NULL),
      _encoderInst32PtrR(NULL), _encoderInst48Ptr(NULL), _encoderInst48PtrR(NULL),
      _decoderInst24Ptr(NULL), _decoderInst32Ptr(NULL), _decoderInst48Ptr(NULL) {
  _codecID = codecID;
  if (_codecID == ACMCodecDB::kG722_1C_24) {
    _operationalRate = 24000;
  } else if (_codecID == ACMCodecDB::kG722_1C_32) {
    _operationalRate = 32000;
  } else if (_codecID == ACMCodecDB::kG722_1C_48) {
    _operationalRate = 48000;
  } else {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "Wrong codec id for G722_1c.");
    _operationalRate = -1;
  }
  return;
}

ACMG722_1C::~ACMG722_1C() {
  if (_encoderInstPtr != NULL) {
    delete _encoderInstPtr;
    _encoderInstPtr = NULL;
  }
  if (_encoderInstPtrRight != NULL) {
    delete _encoderInstPtrRight;
    _encoderInstPtrRight = NULL;
  }
  if (_decoderInstPtr != NULL) {
    delete _decoderInstPtr;
    _decoderInstPtr = NULL;
  }

  switch (_operationalRate) {
    case 24000: {
      _encoderInst24Ptr = NULL;
      _encoderInst24PtrR = NULL;
      _decoderInst24Ptr = NULL;
      break;
    }
    case 32000: {
      _encoderInst32Ptr = NULL;
      _encoderInst32PtrR = NULL;
      _decoderInst32Ptr = NULL;
      break;
    }
    case 48000: {
      _encoderInst48Ptr = NULL;
      _encoderInst48PtrR = NULL;
      _decoderInst48Ptr = NULL;
      break;
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                   "Wrong rate for G722_1c.");
      break;
    }
  }
  return;
}

WebRtc_Word16 ACMG722_1C::InternalEncode(WebRtc_UWord8* bitStream,
                                         WebRtc_Word16* bitStreamLenByte) {
  WebRtc_Word16 leftChannel[640];
  WebRtc_Word16 rightChannel[640];
  WebRtc_Word16 lenInBytes;
  WebRtc_Word16 outB[240];

  // If stereo, split input signal in left and right channel before encoding
  if (_noChannels == 2) {
    for (int i = 0, j = 0; i < _frameLenSmpl * 2; i += 2, j++) {
      leftChannel[j] = _inAudio[_inAudioIxRead + i];
      rightChannel[j] = _inAudio[_inAudioIxRead + i + 1];
    }
  } else {
    memcpy(leftChannel, &_inAudio[_inAudioIxRead], 640);
  }

  switch (_operationalRate) {
    case 24000: {
      lenInBytes = WebRtcG7221C_Encode24(_encoderInst24Ptr, leftChannel, 640,
                                         &outB[0]);
      if (_noChannels == 2) {
        lenInBytes += WebRtcG7221C_Encode24(_encoderInst24PtrR, rightChannel,
                                            640, &outB[lenInBytes / 2]);
      }
      break;
    }
    case 32000: {
      lenInBytes = WebRtcG7221C_Encode32(_encoderInst32Ptr, leftChannel, 640,
                                         &outB[0]);
      if (_noChannels == 2) {
        lenInBytes += WebRtcG7221C_Encode32(_encoderInst32PtrR, rightChannel,
                                            640, &outB[lenInBytes / 2]);
      }
      break;
    }
    case 48000: {
      lenInBytes = WebRtcG7221C_Encode48(_encoderInst48Ptr, leftChannel, 640,
                                         &outB[0]);
      if (_noChannels == 2) {
        lenInBytes += WebRtcG7221C_Encode48(_encoderInst48PtrR, rightChannel,
                                            640, &outB[lenInBytes / 2]);
      }
      break;
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                   "InternalEncode: Wrong rate for G722_1c.");
      return -1;
    }
  }

  memcpy(bitStream, outB, lenInBytes);
  *bitStreamLenByte = lenInBytes;

  // increment the read index this tell the caller that how far
  // we have gone forward in reading the audio buffer
  _inAudioIxRead += 640 * _noChannels;

  return *bitStreamLenByte;
}

WebRtc_Word16 ACMG722_1C::DecodeSafe(WebRtc_UWord8* /* bitStream */,
                                     WebRtc_Word16 /* bitStreamLenByte */,
                                     WebRtc_Word16* /* audio */,
                                     WebRtc_Word16* /* audioSamples */,
                                     WebRtc_Word8* /* speechType */) {
  return 0;
}

WebRtc_Word16 ACMG722_1C::InternalInitEncoder(
    WebRtcACMCodecParams* codecParams) {
  WebRtc_Word16 ret;

  switch (_operationalRate) {
    case 24000: {
      ret = WebRtcG7221C_EncoderInit24(_encoderInst24PtrR);
      if (ret < 0) {
        return ret;
      }
      return WebRtcG7221C_EncoderInit24(_encoderInst24Ptr);
    }
    case 32000: {
      ret = WebRtcG7221C_EncoderInit32(_encoderInst32PtrR);
      if (ret < 0) {
        return ret;
      }
      return WebRtcG7221C_EncoderInit32(_encoderInst32Ptr);
    }
    case 48000: {
      ret = WebRtcG7221C_EncoderInit48(_encoderInst48PtrR);
      if (ret < 0) {
        return ret;
      }
      return WebRtcG7221C_EncoderInit48(_encoderInst48Ptr);
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                   "InternalInitEncode: Wrong rate for G722_1c.");
      return -1;
    }
  }
}

WebRtc_Word16 ACMG722_1C::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */) {
  switch (_operationalRate) {
    case 24000: {
      return WebRtcG7221C_DecoderInit24(_decoderInst24Ptr);
    }
    case 32000: {
      return WebRtcG7221C_DecoderInit32(_decoderInst32Ptr);
    }
    case 48000: {
      return WebRtcG7221C_DecoderInit48(_decoderInst48Ptr);
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                   "InternalInitDecoder: Wrong rate for G722_1c.");
      return -1;
    }
  }
}

WebRtc_Word32 ACMG722_1C::CodecDef(WebRtcNetEQ_CodecDef& codecDef,
                                   const CodecInst& codecInst) {

  if (!_decoderInitialized) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
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
  switch (_operationalRate) {
    case 24000: {
      SET_CODEC_PAR((codecDef), kDecoderG722_1C_24, codecInst.pltype,
          _decoderInst24Ptr, 32000);
      SET_G722_1C_24_FUNCTIONS((codecDef));
      break;
    }
    case 32000: {
      SET_CODEC_PAR((codecDef), kDecoderG722_1C_32, codecInst.pltype,
          _decoderInst32Ptr, 32000);
      SET_G722_1C_32_FUNCTIONS((codecDef));
      break;
    }
    case 48000: {
      SET_CODEC_PAR((codecDef), kDecoderG722_1C_32, codecInst.pltype,
          _decoderInst48Ptr, 32000);
      SET_G722_1C_48_FUNCTIONS((codecDef));
      break;
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
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
  if ((_encoderInstPtr == NULL) || (_encoderInstPtrRight == NULL)) {
    return -1;
  }
  switch (_operationalRate) {
    case 24000: {
      WebRtcG7221C_CreateEnc24(&_encoderInst24Ptr);
      WebRtcG7221C_CreateEnc24(&_encoderInst24PtrR);
      break;
    }
    case 32000: {
      WebRtcG7221C_CreateEnc32(&_encoderInst32Ptr);
      WebRtcG7221C_CreateEnc32(&_encoderInst32PtrR);
      break;
    }
    case 48000: {
      WebRtcG7221C_CreateEnc48(&_encoderInst48Ptr);
      WebRtcG7221C_CreateEnc48(&_encoderInst48PtrR);
      break;
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                   "InternalCreateEncoder: Wrong rate for G722_1c.");
      return -1;
    }
  }
  return 0;
}

void ACMG722_1C::DestructEncoderSafe() {
  _encoderExist = false;
  _encoderInitialized = false;
  if (_encoderInstPtr != NULL) {
    delete _encoderInstPtr;
    _encoderInstPtr = NULL;
  }
  if (_encoderInstPtrRight != NULL) {
    delete _encoderInstPtrRight;
    _encoderInstPtrRight = NULL;
  }
  _encoderInst24Ptr = NULL;
  _encoderInst32Ptr = NULL;
  _encoderInst48Ptr = NULL;
}

WebRtc_Word16 ACMG722_1C::InternalCreateDecoder() {
  if (_decoderInstPtr == NULL) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "InternalCreateEncoder: cannot create decoder");
    return -1;
  }
  switch (_operationalRate) {
    case 24000: {
      WebRtcG7221C_CreateDec24(&_decoderInst24Ptr);
      break;
    }
    case 32000: {
      WebRtcG7221C_CreateDec32(&_decoderInst32Ptr);
      break;
    }
    case 48000: {
      WebRtcG7221C_CreateDec48(&_decoderInst48Ptr);
      break;
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                   "InternalCreateEncoder: Wrong rate for G722_1c.");
      return -1;
    }
  }
  return 0;
}

void ACMG722_1C::DestructDecoderSafe() {
  _decoderExist = false;
  _decoderInitialized = false;
  if (_decoderInstPtr != NULL) {
    delete _decoderInstPtr;
    _decoderInstPtr = NULL;
  }
  _decoderInst24Ptr = NULL;
  _decoderInst32Ptr = NULL;
  _decoderInst48Ptr = NULL;
}

void ACMG722_1C::InternalDestructEncoderInst(void* ptrInst) {
  if (ptrInst != NULL) {
    delete ptrInst;
  }
  return;
}

#endif

} // namespace webrtc
