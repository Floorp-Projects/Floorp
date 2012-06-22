/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "acm_common_defs.h"
#include "acm_neteq.h"
#include "acm_opus.h"
#include "trace.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_help_macros.h"

#ifdef WEBRTC_CODEC_OPUS
    // NOTE! Opus is not included in the open-source package. Modify this file or your codec
    // API to match the function call and name of used Opus API file.
    // #include "opus_interface.h"
#endif

namespace webrtc
{

#ifndef WEBRTC_CODEC_OPUS

ACMOPUS::ACMOPUS(WebRtc_Word16 /* codecID */)
    : _encoderInstPtr(NULL),
      _decoderInstPtr(NULL),
      _mySampFreq(0),
      _myRate(0),
      _opusMode(0),
      _flagVBR(0) {
  return;
}


ACMOPUS::~ACMOPUS()
{
    return;
}


WebRtc_Word16
ACMOPUS::InternalEncode(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16* /* bitStreamLenByte */)
{
    return -1;
}


WebRtc_Word16
ACMOPUS::DecodeSafe(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16  /* bitStreamLenByte */,
    WebRtc_Word16* /* audio            */,
    WebRtc_Word16* /* audioSamples     */,
    WebRtc_Word8*  /* speechType       */)
{
    return -1;
}


WebRtc_Word16
ACMOPUS::InternalInitEncoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    return -1;
}


WebRtc_Word16
ACMOPUS::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    return -1;
}


WebRtc_Word32
ACMOPUS::CodecDef(
    WebRtcNetEQ_CodecDef& /* codecDef  */,
    const CodecInst&      /* codecInst */)
{
    return -1;
}


ACMGenericCodec*
ACMOPUS::CreateInstance(void)
{
    return NULL;
}


WebRtc_Word16
ACMOPUS::InternalCreateEncoder()
{
    return -1;
}


void
ACMOPUS::DestructEncoderSafe()
{
    return;
}


WebRtc_Word16
ACMOPUS::InternalCreateDecoder()
{
    return -1;
}


void
ACMOPUS::DestructDecoderSafe()
{
    return;
}


void
ACMOPUS::InternalDestructEncoderInst(
    void* /* ptrInst */)
{
    return;
}


WebRtc_Word16
ACMOPUS::UnregisterFromNetEqSafe(
    ACMNetEQ*     /* netEq       */,
    WebRtc_Word16 /* payloadType */)
{
    return -1;
}

WebRtc_Word16
ACMOPUS::SetBitRateSafe(
    const WebRtc_Word32 /*rate*/ )
{
    return -1;
}

#else     //===================== Actual Implementation =======================

// Remove when integrating a real Opus wrapper
extern WebRtc_Word16 WebRtcOpus_CreateEnc(OPUS_inst_t_** inst, WebRtc_Word16 samplFreq);
extern WebRtc_Word16 WebRtcOpus_CreateDec(OPUS_inst_t_** inst, WebRtc_Word16 samplFreq);
extern WebRtc_Word16 WebRtcOpus_FreeEnc(OPUS_inst_t_* inst);
extern WebRtc_Word16 WebRtcOpus_FreeDec(OPUS_inst_t_* inst);
extern WebRtc_Word16 WebRtcOpus_Encode(OPUS_inst_t_* encInst,
                                       WebRtc_Word16* input,
                                       WebRtc_Word16* output,
                                       WebRtc_Word16 len,
                                       WebRtc_Word16 byteLen);
extern WebRtc_Word16 WebRtcOpus_EncoderInit(OPUS_inst_t_* encInst,
                                            WebRtc_Word16 samplFreq,
                                            WebRtc_Word16 mode,
                                            WebRtc_Word16 vbrFlag);
extern WebRtc_Word16 WebRtcOpus_Decode(OPUS_inst_t_* decInst);
extern WebRtc_Word16 WebRtcOpus_DecodeBwe(OPUS_inst_t_* decInst, WebRtc_Word16* input);
extern WebRtc_Word16 WebRtcOpus_DecodePlc(OPUS_inst_t_* decInst);
extern WebRtc_Word16 WebRtcOpus_DecoderInit(OPUS_inst_t_* decInst);

ACMOPUS::ACMOPUS(WebRtc_Word16 codecID)
    : _encoderInstPtr(NULL),
      _decoderInstPtr(NULL),
      _mySampFreq(48000),  // Default sampling frequency.
      _myRate(50000),  // Default rate.
      _opusMode(1),  // Default mode is the hybrid mode.
      _flagVBR(0) {  // Default VBR off.
  _codecID = codecID;

  // Current implementation doesn't have DTX. That might change.
  _hasInternalDTX = false;

  return;
}

ACMOPUS::~ACMOPUS()
{
    if(_encoderInstPtr != NULL)
    {
        WebRtcOpus_FreeEnc(_encoderInstPtr);
        _encoderInstPtr = NULL;
    }
    if(_decoderInstPtr != NULL)
    {
        WebRtcOpus_FreeDec(_decoderInstPtr);
        _decoderInstPtr = NULL;
    }
    return;
}


WebRtc_Word16
ACMOPUS::InternalEncode(
    WebRtc_UWord8* bitStream,
    WebRtc_Word16* bitStreamLenByte)
{
    WebRtc_Word16 noEncodedSamples = 0;
    WebRtc_Word16 tmpLenByte = 0;
    *bitStreamLenByte = 0;

    WebRtc_Word16 byteLengthFrame = 0;

    // Derive what byte-length is requested
    byteLengthFrame = _myRate*_frameLenSmpl/(8*_mySampFreq);

    // Call Encoder
    *bitStreamLenByte = WebRtcOpus_Encode(_encoderInstPtr, &_inAudio[_inAudioIxRead],
           (WebRtc_Word16*)bitStream, _frameLenSmpl, byteLengthFrame);

    // increment the read index this tell the caller that how far
    // we have gone forward in reading the audio buffer
    _inAudioIxRead += _frameLenSmpl;

    // sanity check
    if(*bitStreamLenByte < 0)
    {
        // error has happened
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "InternalEncode: Encode error for Opus");
            *bitStreamLenByte = 0;
            return -1;
    }

    return *bitStreamLenByte;
}



WebRtc_Word16
ACMOPUS::DecodeSafe(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16  /* bitStreamLenByte */,
    WebRtc_Word16* /* audio            */,
    WebRtc_Word16* /* audioSamples     */,
    WebRtc_Word8*  /* speechType       */)
{
    return 0;
}


WebRtc_Word16
ACMOPUS::InternalInitEncoder(
    WebRtcACMCodecParams* codecParams)
{
    //set the bit rate and initialize
    _myRate = codecParams->codecInstant.rate;
    return SetBitRateSafe( (WebRtc_UWord32)_myRate);
}


WebRtc_Word16
ACMOPUS::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    if (WebRtcOpus_DecoderInit(_decoderInstPtr) < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                "InternalInitDecoder: init decoder failed for Opus");
        return -1;
    }
    return 0;
}


WebRtc_Word32
ACMOPUS::CodecDef(
    WebRtcNetEQ_CodecDef& codecDef,
    const CodecInst&      codecInst)
{
    if (!_decoderInitialized)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "CodeDef: Decoder uninitialized for Opus");
        return -1;
    }

    // Fill up the structure by calling
    // "SET_CODEC_PAR" & "SET_G729_FUNCTION."
    // Then call NetEQ to add the codec to it's
    // database.
    SET_CODEC_PAR((codecDef), kDecoderOpus, codecInst.pltype,
        _decoderInstPtr, 16000);
    SET_OPUS_FUNCTIONS((codecDef));
    return 0;
}


ACMGenericCodec*
ACMOPUS::CreateInstance(void)
{
    return NULL;
}


WebRtc_Word16
ACMOPUS::InternalCreateEncoder()
{
    if (WebRtcOpus_CreateEnc(&_encoderInstPtr, _mySampFreq) < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "InternalCreateEncoder: create encoder failed for Opus");
        return -1;
    }
    return 0;
}


void
ACMOPUS::DestructEncoderSafe()
{
    _encoderExist = false;
    _encoderInitialized = false;
    if(_encoderInstPtr != NULL)
    {
        WebRtcOpus_FreeEnc(_encoderInstPtr);
        _encoderInstPtr = NULL;
    }
}


WebRtc_Word16
ACMOPUS::InternalCreateDecoder()
{
   if (WebRtcOpus_CreateDec(&_decoderInstPtr, _mySampFreq) < 0)
   {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
        "InternalCreateDecoder: create decoder failed for Opus");
       return -1;
   }
   return 0;
}


void
ACMOPUS::DestructDecoderSafe()
{
    _decoderExist = false;
    _decoderInitialized = false;
    if(_decoderInstPtr != NULL)
    {
        WebRtcOpus_FreeDec(_decoderInstPtr);
        _decoderInstPtr = NULL;
    }
}


void
ACMOPUS::InternalDestructEncoderInst(
    void* ptrInst)
{
    if(ptrInst != NULL)
    {
        WebRtcOpus_FreeEnc((OPUS_inst_t*)ptrInst);
    }
    return;
}


WebRtc_Word16
ACMOPUS::UnregisterFromNetEqSafe(
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
    return netEq->RemoveCodec(kDecoderOpus);
}

WebRtc_Word16
ACMOPUS::SetBitRateSafe(
    const WebRtc_Word32 rate)
{
    //allowed rates: {8000, 12000, 14000, 16000, 18000, 20000,
    //                22000, 24000, 26000, 28000, 30000, 32000};
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
                "SetBitRateSafe: Invalid rate Opus");
            return -1;
        }
    }

    // Re-init with new rate
    if (WebRtcOpus_EncoderInit(_encoderInstPtr, _mySampFreq, _opusMode, _flagVBR) >= 0)
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
