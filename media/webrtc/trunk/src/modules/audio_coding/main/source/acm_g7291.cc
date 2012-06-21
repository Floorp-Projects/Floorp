/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "acm_g7291.h"
#include "acm_common_defs.h"
#include "acm_neteq.h"
#include "trace.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_help_macros.h"

#ifdef WEBRTC_CODEC_G729_1
    // NOTE! G.729.1 is not included in the open-source package. The following
    // interface file is needed:
    //
    // /modules/audio_coding/codecs/g7291/main/interface/g7291_interface.h
    //
    // The API in the header file should match the one below.
    //
    // int16_t WebRtcG7291_Create(G729_1_inst_t_** inst);
    // int16_t WebRtcG7291_Free(G729_1_inst_t_* inst);
    // int16_t WebRtcG7291_Encode(G729_1_inst_t_* encInst, int16_t* input,
    //                            int16_t* output, int16_t myRate,
    //                            int16_t nrFrames);
    // int16_t WebRtcG7291_EncoderInit(G729_1_inst_t_* encInst, int16_t myRate,
    //                                 int16_t flag8kHz, int16_t flagG729mode);
    // int16_t WebRtcG7291_Decode(G729_1_inst_t_* decInst);
    // int16_t WebRtcG7291_DecodeBwe(G729_1_inst_t_* decInst, int16_t* input);
    // int16_t WebRtcG7291_DecodePlc(G729_1_inst_t_* decInst);
    // int16_t WebRtcG7291_DecoderInit(G729_1_inst_t_* decInst);
    // void WebRtcG7291_Version(char *versionStr, short len);
    #include "g7291_interface.h"
#endif

namespace webrtc {

#ifndef WEBRTC_CODEC_G729_1

ACMG729_1::ACMG729_1( WebRtc_Word16 /* codecID */)
    : _encoderInstPtr(NULL),
      _decoderInstPtr(NULL),
      _myRate(32000),
      _flag8kHz(0),
      _flagG729mode(0) {
  return;
}


ACMG729_1::~ACMG729_1()
{
    return;
}


WebRtc_Word16
ACMG729_1::InternalEncode(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16* /* bitStreamLenByte */)
{
    return -1;
}


WebRtc_Word16
ACMG729_1::DecodeSafe(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16  /* bitStreamLenByte */,
    WebRtc_Word16* /* audio            */,
    WebRtc_Word16* /* audioSamples     */,
    WebRtc_Word8*  /* speechType       */)
{
    return -1;
}


WebRtc_Word16
ACMG729_1::InternalInitEncoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    return -1;
}


WebRtc_Word16
ACMG729_1::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    return -1;
}


WebRtc_Word32
ACMG729_1::CodecDef(
    WebRtcNetEQ_CodecDef& /* codecDef  */,
    const CodecInst&      /* codecInst */)
{
    return -1;
}


ACMGenericCodec*
ACMG729_1::CreateInstance(void)
{
    return NULL;
}


WebRtc_Word16
ACMG729_1::InternalCreateEncoder()
{
    return -1;
}


void
ACMG729_1::DestructEncoderSafe()
{
    return;
}


WebRtc_Word16
ACMG729_1::InternalCreateDecoder()
{
    return -1;
}


void
ACMG729_1::DestructDecoderSafe()
{
    return;
}


void
ACMG729_1::InternalDestructEncoderInst(
    void* /* ptrInst */)
{
    return;
}


WebRtc_Word16
ACMG729_1::UnregisterFromNetEqSafe(
    ACMNetEQ*     /* netEq       */,
    WebRtc_Word16 /* payloadType */)
{
    return -1;
}

WebRtc_Word16
ACMG729_1::SetBitRateSafe(
    const WebRtc_Word32 /*rate*/ )
{
  return -1;
}

#else     //===================== Actual Implementation =======================

struct G729_1_inst_t_;

ACMG729_1::ACMG729_1(WebRtc_Word16 codecID)
    : _encoderInstPtr(NULL),
      _decoderInstPtr(NULL),
      _myRate(32000),  // Default rate.
      _flag8kHz(0),
      _flagG729mode(0) {
  // TODO(tlegrand): We should add codecID as a input variable to the
  // constructor of ACMGenericCodec.
  _codecID = codecID;
  return;
}

ACMG729_1::~ACMG729_1()
{
    if(_encoderInstPtr != NULL)
    {
        WebRtcG7291_Free(_encoderInstPtr);
        _encoderInstPtr = NULL;
    }
    if(_decoderInstPtr != NULL)
    {
        WebRtcG7291_Free(_decoderInstPtr);
        _decoderInstPtr = NULL;
    }
    return;
}


WebRtc_Word16
ACMG729_1::InternalEncode(
    WebRtc_UWord8* bitStream,
    WebRtc_Word16* bitStreamLenByte)
{

    // Initialize before entering the loop 
    WebRtc_Word16 noEncodedSamples = 0;
    *bitStreamLenByte = 0;

  WebRtc_Word16 byteLengthFrame = 0;

    // Derive number of 20ms frames per encoded packet.
  // [1,2,3] <=> [20,40,60]ms <=> [320,640,960] samples
    WebRtc_Word16 n20msFrames = (_frameLenSmpl / 320);
    // Byte length for the frame. +1 is for rate information.
    byteLengthFrame = _myRate/(8*50) * n20msFrames + (1 - _flagG729mode);

    // The following might be revised if we have G729.1 Annex C (support for DTX);
    do
    {
        *bitStreamLenByte = WebRtcG7291_Encode(_encoderInstPtr, &_inAudio[_inAudioIxRead],
       (WebRtc_Word16*)bitStream, _myRate, n20msFrames);

        // increment the read index this tell the caller that how far
        // we have gone forward in reading the audio buffer
    _inAudioIxRead += 160;

        // sanity check
        if(*bitStreamLenByte < 0)
        {
      // error has happened
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
        "InternalEncode: Encode error for G729_1");
            *bitStreamLenByte = 0;
            return -1;
        }

    noEncodedSamples += 160;
    } while(*bitStreamLenByte == 0);


    // This criteria will change if we have Annex C.
    if(*bitStreamLenByte != byteLengthFrame)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "InternalEncode: Encode error for G729_1");
        *bitStreamLenByte = 0;
        return -1;
    }


    if(noEncodedSamples != _frameLenSmpl)
    {
        *bitStreamLenByte = 0;
        return -1;
    }

    return *bitStreamLenByte;
}


WebRtc_Word16
ACMG729_1::DecodeSafe(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16  /* bitStreamLenByte */,
    WebRtc_Word16* /* audio            */,
    WebRtc_Word16* /* audioSamples     */,
    WebRtc_Word8*  /* speechType       */)
{
    return 0;
}


WebRtc_Word16
ACMG729_1::InternalInitEncoder(
    WebRtcACMCodecParams* codecParams)
{
  //set the bit rate and initialize
  _myRate = codecParams->codecInstant.rate;
    return SetBitRateSafe( (WebRtc_UWord32)_myRate);
}


WebRtc_Word16
ACMG729_1::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    if (WebRtcG7291_DecoderInit(_decoderInstPtr) < 0)
  {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
        "InternalInitDecoder: init decoder failed for G729_1");
    return -1;
  }
  return 0;
}


WebRtc_Word32
ACMG729_1::CodecDef(
    WebRtcNetEQ_CodecDef& codecDef,
    const CodecInst&      codecInst)
{
    if (!_decoderInitialized)
    {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
      "CodeDef: Decoder uninitialized for G729_1");
      return -1;
    }

    // Fill up the structure by calling
    // "SET_CODEC_PAR" & "SET_G729_FUNCTION."
    // Then call NetEQ to add the codec to it's
    // database.
    SET_CODEC_PAR((codecDef), kDecoderG729_1, codecInst.pltype,
        _decoderInstPtr, 16000);
    SET_G729_1_FUNCTIONS((codecDef));
    return 0;
}


ACMGenericCodec*
ACMG729_1::CreateInstance(void)
{
    return NULL;
}


WebRtc_Word16
ACMG729_1::InternalCreateEncoder()
{
    if (WebRtcG7291_Create(&_encoderInstPtr) < 0)
  {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
      "InternalCreateEncoder: create encoder failed for G729_1");
    return -1;
  }
  return 0;
}


void
ACMG729_1::DestructEncoderSafe()
{
    _encoderExist = false;
    _encoderInitialized = false;
    if(_encoderInstPtr != NULL)
    {
        WebRtcG7291_Free(_encoderInstPtr);
        _encoderInstPtr = NULL;
    }
}


WebRtc_Word16
ACMG729_1::InternalCreateDecoder()
{
   if (WebRtcG7291_Create(&_decoderInstPtr) < 0)
   {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
    "InternalCreateDecoder: create decoder failed for G729_1");
     return -1;
   }
   return 0;
}


void
ACMG729_1::DestructDecoderSafe()
{
    _decoderExist = false;
    _decoderInitialized = false;
    if(_decoderInstPtr != NULL)
    {
        WebRtcG7291_Free(_decoderInstPtr);
        _decoderInstPtr = NULL;
    }
}


void
ACMG729_1::InternalDestructEncoderInst(
    void* ptrInst)
{
    if(ptrInst != NULL)
    {
        //WebRtcG7291_Free((G729_1_inst_t*)ptrInst);
    }
    return;
}


WebRtc_Word16
ACMG729_1::UnregisterFromNetEqSafe(
    ACMNetEQ*     netEq,
    WebRtc_Word16 payloadType)
{
    if(payloadType != _decoderParams.codecInstant.pltype)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
      "Cannot unregister codec: given payload-type does not match \
the stored payload type",
            _decoderParams.codecInstant.plname,
            payloadType,
            _decoderParams.codecInstant.pltype);
        return -1;
    }
    return netEq->RemoveCodec(kDecoderG729_1);
}

WebRtc_Word16
ACMG729_1::SetBitRateSafe(
    const WebRtc_Word32 rate)
{
    //allowed rates: { 8000, 12000, 14000, 16000, 18000, 20000,
    //                22000, 24000, 26000, 28000, 30000, 32000};
    // TODO(tlegrand): This check exists in one other place two. Should be
    // possible to reuse code.
    switch(rate)
    {
    case 8000:
        {
            _myRate = 8000;
            break;
        }
  case 12000:
        {
            _myRate = 12000;
            break;
        }
  case 14000:
        {
            _myRate = 14000;
            break;
        }
  case 16000:
        {
            _myRate = 16000;
            break;
        }
  case 18000:
        {
            _myRate = 18000;
            break;
        }
  case 20000:
        {
            _myRate = 20000;
            break;
        }
  case 22000:
        {
            _myRate = 22000;
            break;
        }
  case 24000:
        {
            _myRate = 24000;
            break;
        }
  case 26000:
        {
            _myRate = 26000;
            break;
        }
  case 28000:
        {
            _myRate = 28000;
            break;
        }
  case 30000:
        {
            _myRate = 30000;
            break;
        }
  case 32000:
        {
            _myRate = 32000;
            break;
        }
    default:
        {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
        "SetBitRateSafe: Invalid rate G729_1");
            return -1;
        }
    }

    // Re-init with new rate
    if (WebRtcG7291_EncoderInit(_encoderInstPtr, _myRate, _flag8kHz, _flagG729mode) >= 0)
    {
        _encoderParams.codecInstant.rate = _myRate;
        return 0;
    }
    else
    {
        return -1;
    }
}


#endif

} // namespace webrtc
