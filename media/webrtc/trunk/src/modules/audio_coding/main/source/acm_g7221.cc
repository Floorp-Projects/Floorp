/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "acm_g7221.h"
#include "acm_codec_database.h"
#include "acm_common_defs.h"
#include "acm_neteq.h"
#include "trace.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_help_macros.h"

#ifdef WEBRTC_CODEC_G722_1
// NOTE! G.722.1 is not included in the open-source package. The following
// interface file is needed:
//
// /modules/audio_coding/codecs/g7221/main/interface/g7221_interface.h
//
// The API in the header file should match the one below.
//
// int16_t WebRtcG7221_CreateEnc16(G722_1_16_encinst_t_** encInst);
// int16_t WebRtcG7221_CreateEnc24(G722_1_24_encinst_t_** encInst);
// int16_t WebRtcG7221_CreateEnc32(G722_1_32_encinst_t_** encInst);
// int16_t WebRtcG7221_CreateDec16(G722_1_16_decinst_t_** decInst);
// int16_t WebRtcG7221_CreateDec24(G722_1_24_decinst_t_** decInst);
// int16_t WebRtcG7221_CreateDec32(G722_1_32_decinst_t_** decInst);
//
// int16_t WebRtcG7221_FreeEnc16(G722_1_16_encinst_t_** encInst);
// int16_t WebRtcG7221_FreeEnc24(G722_1_24_encinst_t_** encInst);
// int16_t WebRtcG7221_FreeEnc32(G722_1_32_encinst_t_** encInst);
// int16_t WebRtcG7221_FreeDec16(G722_1_16_decinst_t_** decInst);
// int16_t WebRtcG7221_FreeDec24(G722_1_24_decinst_t_** decInst);
// int16_t WebRtcG7221_FreeDec32(G722_1_32_decinst_t_** decInst);
//
// int16_t WebRtcG7221_EncoderInit16(G722_1_16_encinst_t_* encInst);
// int16_t WebRtcG7221_EncoderInit24(G722_1_24_encinst_t_* encInst);
// int16_t WebRtcG7221_EncoderInit32(G722_1_32_encinst_t_* encInst);
// int16_t WebRtcG7221_DecoderInit16(G722_1_16_decinst_t_* decInst);
// int16_t WebRtcG7221_DecoderInit24(G722_1_24_decinst_t_* decInst);
// int16_t WebRtcG7221_DecoderInit32(G722_1_32_decinst_t_* decInst);
//
// int16_t WebRtcG7221_Encode16(G722_1_16_encinst_t_* encInst,
//                              int16_t* input,
//                              int16_t len,
//                              int16_t* output);
// int16_t WebRtcG7221_Encode24(G722_1_24_encinst_t_* encInst,
//                              int16_t* input,
//                              int16_t len,
//                              int16_t* output);
// int16_t WebRtcG7221_Encode32(G722_1_32_encinst_t_* encInst,
//                              int16_t* input,
//                              int16_t len,
//                              int16_t* output);
//
// int16_t WebRtcG7221_Decode16(G722_1_16_decinst_t_* decInst,
//                              int16_t* bitstream,
//                              int16_t len,
//                              int16_t* output);
// int16_t WebRtcG7221_Decode24(G722_1_24_decinst_t_* decInst,
//                              int16_t* bitstream,
//                              int16_t len,
//                              int16_t* output);
// int16_t WebRtcG7221_Decode32(G722_1_32_decinst_t_* decInst,
//                              int16_t* bitstream,
//                              int16_t len,
//                              int16_t* output);
//
// int16_t WebRtcG7221_DecodePlc16(G722_1_16_decinst_t_* decInst,
//                                 int16_t* output,
//                                 int16_t nrLostFrames);
// int16_t WebRtcG7221_DecodePlc24(G722_1_24_decinst_t_* decInst,
//                                 int16_t* output,
//                                 int16_t nrLostFrames);
// int16_t WebRtcG7221_DecodePlc32(G722_1_32_decinst_t_* decInst,
//                                 int16_t* output,
//                                 int16_t nrLostFrames);
// void WebRtcG7221_Version(char *versionStr, short len);
#include "g7221_interface.h"
#endif

namespace webrtc {

#ifndef WEBRTC_CODEC_G722_1

ACMG722_1::ACMG722_1(WebRtc_Word16 /* codecID */)
    : _operationalRate(-1),
      _encoderInstPtr(NULL),
      _encoderInstPtrRight(NULL),
      _decoderInstPtr(NULL),
      _encoderInst16Ptr(NULL),
      _encoderInst16PtrR(NULL),
      _encoderInst24Ptr(NULL),
      _encoderInst24PtrR(NULL),
      _encoderInst32Ptr(NULL),
      _encoderInst32PtrR(NULL),
      _decoderInst16Ptr(NULL),
      _decoderInst24Ptr(NULL),
      _decoderInst32Ptr(NULL) {
  return;
}

ACMG722_1::~ACMG722_1() {
  return;
}

WebRtc_Word16 ACMG722_1::InternalEncode(WebRtc_UWord8* /* bitStream */,
                                        WebRtc_Word16* /* bitStreamLenByte */) {
  return -1;
}

WebRtc_Word16 ACMG722_1::DecodeSafe(WebRtc_UWord8* /* bitStream  */,
                                    WebRtc_Word16 /* bitStreamLenByte */,
                                    WebRtc_Word16* /* audio */,
                                    WebRtc_Word16* /* audioSamples */,
                                    WebRtc_Word8* /* speechType */) {
  return -1;
}

WebRtc_Word16 ACMG722_1::InternalInitEncoder(
    WebRtcACMCodecParams* /* codecParams */) {
  return -1;
}

WebRtc_Word16 ACMG722_1::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */) {
  return -1;
}

WebRtc_Word32 ACMG722_1::CodecDef(WebRtcNetEQ_CodecDef& /* codecDef  */,
                                  const CodecInst& /* codecInst */) {
  return -1;
}

ACMGenericCodec* ACMG722_1::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMG722_1::InternalCreateEncoder() {
  return -1;
}

void ACMG722_1::DestructEncoderSafe() {
  return;
}

WebRtc_Word16 ACMG722_1::InternalCreateDecoder() {
  return -1;
}

void ACMG722_1::DestructDecoderSafe() {
  return;
}

void ACMG722_1::InternalDestructEncoderInst(void* /* ptrInst */) {
  return;
}

WebRtc_Word16 ACMG722_1::UnregisterFromNetEqSafe(
    ACMNetEQ* /* netEq */,  WebRtc_Word16 /* payloadType */) {
  return -1;
}

#else     //===================== Actual Implementation =======================
ACMG722_1::ACMG722_1(
    WebRtc_Word16 codecID):
    _encoderInstPtr(NULL),
    _encoderInstPtrRight(NULL),
    _decoderInstPtr(NULL),
    _encoderInst16Ptr(NULL),
    _encoderInst16PtrR(NULL),
    _encoderInst24Ptr(NULL),
    _encoderInst24PtrR(NULL),
    _encoderInst32Ptr(NULL),
    _encoderInst32PtrR(NULL),
    _decoderInst16Ptr(NULL),
    _decoderInst24Ptr(NULL),
    _decoderInst32Ptr(NULL) {
  _codecID = codecID;
  if (_codecID == ACMCodecDB::kG722_1_16) {
    _operationalRate = 16000;
  } else if (_codecID == ACMCodecDB::kG722_1_24) {
    _operationalRate = 24000;
  } else if (_codecID == ACMCodecDB::kG722_1_32) {
    _operationalRate = 32000;
  } else {
    _operationalRate = -1;
  }
  return;
}

ACMG722_1::~ACMG722_1() {
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
    case 16000: {
      _encoderInst16Ptr = NULL;
      _encoderInst16PtrR = NULL;
      _decoderInst16Ptr = NULL;
      break;
    }
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
    default: {
      break;
    }
  }
  return;
}

WebRtc_Word16 ACMG722_1::InternalEncode(WebRtc_UWord8* bitStream,
                                        WebRtc_Word16* bitStreamLenByte) {
  WebRtc_Word16 leftChannel[320];
  WebRtc_Word16 rightChannel[320];
  WebRtc_Word16 lenInBytes;
  WebRtc_Word16 outB[160];

  // If stereo, split input signal in left and right channel before encoding
  if (_noChannels == 2) {
    for (int i = 0, j = 0; i < _frameLenSmpl * 2; i += 2, j++) {
      leftChannel[j] = _inAudio[_inAudioIxRead + i];
      rightChannel[j] = _inAudio[_inAudioIxRead + i + 1];
    }
  } else {
    memcpy(leftChannel, &_inAudio[_inAudioIxRead], 320);
  }

  switch (_operationalRate) {
    case 16000: {
      Inst lenInBytes = WebRtcG7221_Encode16(_encoderInst16Ptr, leftChannel,
                                             320, &outB[0]);
      if (_noChannels == 2) {
        lenInBytes += WebRtcG7221_Encode16(_encoderInst16PtrR, rightChannel,
                                           320, &outB[lenInBytes / 2]);
      }
      break;
    }
    case 24000: {
      lenInBytes = WebRtcG7221_Encode24(_encoderInst24Ptr, leftChannel, 320,
                                        &outB[0]);
      if (_noChannels == 2) {
        lenInBytes += WebRtcG7221_Encode24(_encoderInst24PtrR, rightChannel,
                                           320, &outB[lenInBytes / 2]);
      }
      break;
    }
    case 32000: {
      lenInBytes = WebRtcG7221_Encode32(_encoderInst32Ptr, leftChannel, 320,
                                        &outB[0]);
      if (_noChannels == 2) {
        lenInBytes += WebRtcG7221_Encode32(_encoderInst32PtrR, rightChannel,
                                           320, &outB[lenInBytes / 2]);
      }
      break;
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                   "InternalInitEncode: Wrong rate for G722_1.");
      return -1;
    }
  }
  memcpy(bitStream, outB, lenInBytes);
  *bitStreamLenByte = lenInBytes;

  // increment the read index this tell the caller that how far
  // we have gone forward in reading the audio buffer
  _inAudioIxRead += 320 * _noChannels;
  return *bitStreamLenByte;
}

WebRtc_Word16 ACMG722_1::DecodeSafe(WebRtc_UWord8* /* bitStream */,
                                    WebRtc_Word16 /* bitStreamLenByte */,
                                    WebRtc_Word16* /* audio */,
                                    WebRtc_Word16* /* audioSamples */,
                                    WebRtc_Word8* /* speechType */) {
  return 0;
}

WebRtc_Word16 ACMG722_1::InternalInitEncoder(
    WebRtcACMCodecParams* codecParams) {
  WebRtc_Word16 ret;

  switch (_operationalRate) {
    case 16000: {
      ret = WebRtcG7221_EncoderInit16(_encoderInst16PtrR);
      if (ret < 0) {
        return ret;
      }
      return WebRtcG7221_EncoderInit16(_encoderInst16Ptr);
    }
    case 24000: {
      ret = WebRtcG7221_EncoderInit24(_encoderInst24PtrR);
      if (ret < 0) {
        return ret;
      }
      return WebRtcG7221_EncoderInit24(_encoderInst24Ptr);
    }
    case 32000: {
      ret = WebRtcG7221_EncoderInit32(_encoderInst32PtrR);
      if (ret < 0) {
        return ret;
      }
      return WebRtcG7221_EncoderInit32(_encoderInst32Ptr);
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError,Inst webrtc::kTraceAudioCoding,
                   _uniqueID, "InternalInitEncoder: Wrong rate for G722_1.");
      return -1;
    }
  }
}

WebRtc_Word16 ACMG722_1::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */) {
  switch (_operationalRate) {
    case 16000: {
      return WebRtcG7221_DecoderInit16(_decoderInst16Ptr);
    }
    case 24000: {
      return WebRtcG7221_DecoderInit24(_decoderInst24Ptr);
    }
    case 32000: {
      return WebRtcG7221_DecoderInit32(_decoderInst32Ptr);
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                   "InternalInitDecoder: Wrong rate for G722_1.");
      return -1;
    }
  }
}

WebRtc_Word32 ACMG722_1::CodecDef(WebRtcNetEQ_CodecDef& codecDef,
                                  const CodecInst& codecInst) {
  if (!_decoderInitialized) {
    // Todo:
    // log error
    return -1;
  }
  // NetEq has an array of pointers to WebRtcNetEQ_CodecDef.
  // Get an entry of that array (neteq wrapper will allocate memory)
  // by calling "netEq->CodecDef", where "NETEQ_CODEC_G722_1_XX" would
  // be the index of the entry.
  // Fill up the given structure by calling
  // "SET_CODEC_PAR" & "SET_G722_1_XX_FUNCTION."
  // Then return the structure back to NetEQ to add the codec to it's
  // database.
  switch (_operationalRate) {
    case 16000: {
      SET_CODEC_PAR((codecDef), kDecoderG722_1_16, codecInst.pltype,
          _decoderInst16Ptr, 16000);
      SET_G722_1_16_FUNCTIONS((codecDef));
      break;
    }
    case 24000: {
      SET_CODEC_PAR((codecDef), kDecoderG722_1_24, codecInst.pltype,
          _decoderInst24Ptr, 16000);
      SET_G722_1_24_FUNCTIONS((codecDef));
      break;
    }
    case 32000: {
      SET_CODEC_PAR((codecDef), kDecoderG722_1_32, codecInst.pltype,
          _decoderInst32Ptr, 16000);
      SET_G722_1_32_FUNCTIONS((codecDef));
      break;
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                   "CodecDef: Wrong rate for G722_1.");
      return -1;
    }
  }
  return 0;
}

ACMGenericCodec* ACMG722_1::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMG722_1::InternalCreateEncoder() {
  if ((_encoderInstPtr == NULL) || (_encoderInstPtrRight == NULL)) {
    return -1;
  }
  switch (_operationalRate) {
    case 16000: {
      WebRtcG7221_CreateEnc16(&_encoderInst16Ptr);
      WebRtcG7221_CreateEnc16(&_encoderInst16PtrR);
      break;
    }
    case 24000: {
      WebRtcG7221_CreateEnc24(&_encoderInst24Ptr);
      WebRtcG7221_CreateEnc24(&_encoderInst24PtrR);
      break;
    }
    case 32000: {
      WebRtcG7221_CreateEnc32(&_encoderInst32Ptr);
      WebRtcG7221_CreateEnc32(&_encoderInst32PtrR);
      break;
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                   "InternalCreateEncoder: Wrong rate for G722_1.");
      return -1;
    }
  }
  return 0;
}

void ACMG722_1::DestructEncoderSafe() {
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
  _encoderInst16Ptr = NULL;
  _encoderInst24Ptr = NULL;
  _encoderInst32Ptr = NULL;
}

WebRtc_Word16 ACMG722_1::InternalCreateDecoder() {
  if (_decoderInstPtr == NULL) {
    return -1;
  }
  switch (_operationalRate) {
    case 16000: {
      WebRtcG7221_CreateDec16(&_decoderInst16Ptr);
      break;
    }
    case 24000: {
      WebRtcG7221_CreateDec24(&_decoderInst24Ptr);
      break;
    }
    case 32000: {
      WebRtcG7221_CreateDec32(&_decoderInst32Ptr);
      break;
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                   "InternalCreateDecoder: Wrong rate for G722_1.");
      return -1;
    }
  }
  return 0;
}

void ACMG722_1::DestructDecoderSafe() {
  _decoderExist = false;
  _decoderInitialized = false;
  if (_decoderInstPtr != NULL) {
    delete _decoderInstPtr;
    _decoderInstPtr = NULL;
  }
  _decoderInst16Ptr = NULL;
  _decoderInst24Ptr = NULL;
  _decoderInst32Ptr = NULL;
}

void ACMG722_1::InternalDestructEncoderInst(void* ptrInst) {
  if (ptrInst != NULL) {
    delete ptrInst;
  }
  return;
}

WebRtc_Word16 ACMG722_1::UnregisterFromNetEqSafe(ACMNetEQ* netEq,
                                                 WebRtc_Word16 payloadType) {
  if (payloadType != _decoderParams.codecInstant.pltype) {
    WEBRTC_TRACE(webrtc::kTraceError,
                 webrtc::kTraceAudioCoding,
                 _uniqueID,
                 "Cannot unregister codec %s given payload-type %d does not "
                 "match the stored payload type",
                 _decoderParams.codecInstant.plname, payloadType,
                 _decoderParams.codecInstant.pltype);
    return -1;
  }
  switch (_operationalRate) {
    case 16000: {
      return netEq->RemoveCodec(kDecoderG722_1_16);
    }
    case 24000: {
      return netEq->RemoveCodec(kDecoderG722_1_24);
    }
    case 32000: {
      return netEq->RemoveCodec(kDecoderG722_1_32);
    }
    default: {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                   "UnregisterFromNetEqSafe: Wrong rate for G722_1.");
      return -1;
    }
  }
}

#endif

} // namespace webrtc
