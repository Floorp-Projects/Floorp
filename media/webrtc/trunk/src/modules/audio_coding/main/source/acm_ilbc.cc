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
#include "acm_ilbc.h"
#include "acm_neteq.h"
#include "trace.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_help_macros.h"

#ifdef WEBRTC_CODEC_ILBC
    #include "ilbc.h"
#endif

namespace webrtc
{

#ifndef WEBRTC_CODEC_ILBC

ACMILBC::ACMILBC(WebRtc_Word16 /* codecID */)
    : _encoderInstPtr(NULL),
      _decoderInstPtr(NULL) {
  return;
}


ACMILBC::~ACMILBC()
{
    return;
}


WebRtc_Word16
ACMILBC::InternalEncode(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16* /* bitStreamLenByte */)
{
    return -1;
}


WebRtc_Word16
ACMILBC::DecodeSafe(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16  /* bitStreamLenByte */,
    WebRtc_Word16* /* audio            */,
    WebRtc_Word16* /* audioSamples     */,
    WebRtc_Word8*  /* speechType       */)
{
    return -1;
}


WebRtc_Word16
ACMILBC::InternalInitEncoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    return -1;
}


WebRtc_Word16
ACMILBC::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    return -1;
}


WebRtc_Word32
ACMILBC::CodecDef(
    WebRtcNetEQ_CodecDef& /* codecDef  */,
    const CodecInst&      /* codecInst */)
{
    return -1;
}


ACMGenericCodec*
ACMILBC::CreateInstance(void)
{
    return NULL;
}


WebRtc_Word16
ACMILBC::InternalCreateEncoder()
{
    return -1;
}


void
ACMILBC::DestructEncoderSafe()
{
    return;
}


WebRtc_Word16
ACMILBC::InternalCreateDecoder()
{
    return -1;
}


void
ACMILBC::DestructDecoderSafe()
{
    return;
}


void
ACMILBC::InternalDestructEncoderInst(
    void* /* ptrInst */)
{
    return;
}

WebRtc_Word16
ACMILBC::SetBitRateSafe(const WebRtc_Word32 /* rate */)
{
    return -1;
}


WebRtc_Word16
ACMILBC::UnregisterFromNetEqSafe(
    ACMNetEQ*     /* netEq       */,
    WebRtc_Word16 /* payloadType */)
{
    return -1;
}


#else     //===================== Actual Implementation =======================


ACMILBC::ACMILBC(
    WebRtc_Word16 codecID):
_encoderInstPtr(NULL),
_decoderInstPtr(NULL)
{
    _codecID = codecID;
    return;
}


ACMILBC::~ACMILBC()
{
    if(_encoderInstPtr != NULL)
    {
        WebRtcIlbcfix_EncoderFree(_encoderInstPtr);
        _encoderInstPtr = NULL;
    }
    if(_decoderInstPtr != NULL)
    {
        WebRtcIlbcfix_DecoderFree(_decoderInstPtr);
        _decoderInstPtr = NULL;
    }
    return;
}


WebRtc_Word16
ACMILBC::InternalEncode(
    WebRtc_UWord8* bitStream,
    WebRtc_Word16* bitStreamLenByte)
{
    *bitStreamLenByte = WebRtcIlbcfix_Encode(_encoderInstPtr,
        &_inAudio[_inAudioIxRead], _frameLenSmpl, (WebRtc_Word16*)bitStream);
    if (*bitStreamLenByte < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "InternalEncode: error in encode for ILBC");
        return -1;
    }
    // increment the read index this tell the caller that how far
    // we have gone forward in reading the audio buffer
    _inAudioIxRead += _frameLenSmpl;
    return *bitStreamLenByte;
}


WebRtc_Word16
ACMILBC::DecodeSafe(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16  /* bitStreamLenByte */,
    WebRtc_Word16* /* audio            */,
    WebRtc_Word16* /* audioSamples     */,
    WebRtc_Word8*  /* speechType       */)
{
    return 0;
}


WebRtc_Word16
ACMILBC::InternalInitEncoder(
    WebRtcACMCodecParams* codecParams)
{
    // initialize with a correct processing block length
    if((160 == (codecParams->codecInstant).pacsize) ||
        (320 == (codecParams->codecInstant).pacsize))
    {
        // processing block of 20ms
        return WebRtcIlbcfix_EncoderInit(_encoderInstPtr, 20);
    }
    else if((240 == (codecParams->codecInstant).pacsize) ||
        (480 == (codecParams->codecInstant).pacsize))
    {
        // processing block of 30ms
        return WebRtcIlbcfix_EncoderInit(_encoderInstPtr, 30);
    }
    else
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "InternalInitEncoder: invalid processing block");
        return -1;
    }
}


WebRtc_Word16
ACMILBC::InternalInitDecoder(
    WebRtcACMCodecParams* codecParams)
{
    // initialize with a correct processing block length
    if((160 == (codecParams->codecInstant).pacsize) ||
        (320 == (codecParams->codecInstant).pacsize))
    {
        // processing block of 20ms
        return WebRtcIlbcfix_DecoderInit(_decoderInstPtr, 20);
    }
    else if((240 == (codecParams->codecInstant).pacsize) ||
        (480 == (codecParams->codecInstant).pacsize))
    {
        // processing block of 30ms
        return WebRtcIlbcfix_DecoderInit(_decoderInstPtr, 30);
    }
    else
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "InternalInitDecoder: invalid processing block");
        return -1;
    }
}


WebRtc_Word32
ACMILBC::CodecDef(
    WebRtcNetEQ_CodecDef& codecDef,
    const CodecInst&      codecInst)
{
    if (!_decoderInitialized)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "CodeDef: decoder not initialized for ILBC");
        return -1;
    }
    // Fill up the structure by calling
    // "SET_CODEC_PAR" & "SET_ILBC_FUNCTION."
    // Then return the structure back to NetEQ to add the codec to it's
    // database.
    SET_CODEC_PAR((codecDef), kDecoderILBC, codecInst.pltype,
        _decoderInstPtr, 8000);
    SET_ILBC_FUNCTIONS((codecDef));
    return 0;
}


ACMGenericCodec*
ACMILBC::CreateInstance(void)
{
    return NULL;
}


WebRtc_Word16
ACMILBC::InternalCreateEncoder()
{
    if (WebRtcIlbcfix_EncoderCreate(&_encoderInstPtr) < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "InternalCreateEncoder: cannot create instance for ILBC encoder");
        return -1;
    }
    return 0;
}


void
ACMILBC::DestructEncoderSafe()
{
    _encoderInitialized = false;
    _encoderExist = false;
    if(_encoderInstPtr != NULL)
    {
        WebRtcIlbcfix_EncoderFree(_encoderInstPtr);
        _encoderInstPtr = NULL;
    }
}


WebRtc_Word16
ACMILBC::InternalCreateDecoder()
{
    if (WebRtcIlbcfix_DecoderCreate(&_decoderInstPtr) < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "InternalCreateDecoder: cannot create instance for ILBC decoder");
        return -1;
    }
    return 0;
}


void
ACMILBC::DestructDecoderSafe()
{
    _decoderInitialized = false;
    _decoderExist = false;
    if(_decoderInstPtr != NULL)
    {
        WebRtcIlbcfix_DecoderFree(_decoderInstPtr);
        _decoderInstPtr = NULL;
    }
}


void
ACMILBC::InternalDestructEncoderInst(
    void* ptrInst)
{
    if(ptrInst != NULL)
    {
        WebRtcIlbcfix_EncoderFree((iLBC_encinst_t_*)ptrInst);
    }
    return;
}

WebRtc_Word16
ACMILBC::SetBitRateSafe(const WebRtc_Word32 rate)
{
    // Check that rate is valid. No need to store the value
    if (rate == 13300)
    {
        WebRtcIlbcfix_EncoderInit(_encoderInstPtr, 30);
    }
    else if (rate == 15200)
    {
        WebRtcIlbcfix_EncoderInit(_encoderInstPtr, 20);
    }
    else
    {
        return -1;
    }
    _encoderParams.codecInstant.rate = rate;

    return 0;
}

WebRtc_Word16
ACMILBC::UnregisterFromNetEqSafe(
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
    return netEq->RemoveCodec(kDecoderILBC);
}

#endif

} // namespace webrtc
