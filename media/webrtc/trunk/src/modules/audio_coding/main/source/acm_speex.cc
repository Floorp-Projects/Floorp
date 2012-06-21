/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "acm_speex.h"
#include "acm_codec_database.h"
#include "acm_common_defs.h"
#include "acm_neteq.h"
#include "trace.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_help_macros.h"

#ifdef WEBRTC_CODEC_SPEEX
    // NOTE! Speex is not included in the open-source package. The following
    // interface file is needed:
    //
    // /modules/audio_coding/codecs/speex/main/interface/speex_interface.h
    //
    // The API in the header file should match the one below.
    //
    // int16_t WebRtcSpeex_CreateEnc(SPEEX_encinst_t **SPEEXenc_inst,
    //                               int32_t fs);
    // int16_t WebRtcSpeex_FreeEnc(SPEEX_encinst_t *SPEEXenc_inst);
    // int16_t WebRtcSpeex_CreateDec(SPEEX_decinst_t **SPEEXdec_inst,
    //                               int32_t fs,
    //                               int16_t enh_enabled);
    // int16_t WebRtcSpeex_FreeDec(SPEEX_decinst_t *SPEEXdec_inst);
    // int16_t WebRtcSpeex_Encode(SPEEX_encinst_t *SPEEXenc_inst,
    //                            int16_t *speechIn,
    //                            int32_t rate);
    // int16_t WebRtcSpeex_EncoderInit(SPEEX_encinst_t *SPEEXenc_inst,
    //                                 int16_t vbr, int16_t complexity,
    //                                 int16_t vad_enable);
    // int16_t WebRtcSpeex_GetBitstream(SPEEX_encinst_t *SPEEXenc_inst,
    //                                  int16_t *encoded);
    // int16_t WebRtcSpeex_DecodePlc(SPEEX_decinst_t *SPEEXdec_inst,
    //                               int16_t *decoded, int16_t noOfLostFrames);
    // int16_t WebRtcSpeex_Decode(SPEEX_decinst_t *SPEEXdec_inst,
    //                            int16_t *encoded, int16_t len,
    //                            int16_t *decoded, int16_t *speechType);
    // int16_t WebRtcSpeex_DecoderInit(SPEEX_decinst_t *SPEEXdec_inst);
    // void WebRtcSpeex_Version(char *versionStr, short len);
    #include "speex_interface.h"
#endif

namespace webrtc {

#ifndef WEBRTC_CODEC_SPEEX
ACMSPEEX::ACMSPEEX(WebRtc_Word16 /* codecID*/)
    : _encoderInstPtr(NULL),
      _decoderInstPtr(NULL),
      _complMode(0),
      _vbrEnabled(false),
      _encodingRate(-1),
      _samplingFrequency(-1),
      _samplesIn20MsAudio(-1) {
  return;
}

ACMSPEEX::~ACMSPEEX()
{
    return;
}

WebRtc_Word16
ACMSPEEX::InternalEncode(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16* /* bitStreamLenByte */)
{
    return -1;
}

WebRtc_Word16
ACMSPEEX::DecodeSafe(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16  /* bitStreamLenByte */,
    WebRtc_Word16* /* audio            */,
    WebRtc_Word16* /* audioSamples     */,
    WebRtc_Word8*  /* speechType       */)
{
    return -1;
}

WebRtc_Word16
ACMSPEEX::EnableDTX()
{
    return -1;
}

WebRtc_Word16
ACMSPEEX::DisableDTX()
{
    return -1;
}

WebRtc_Word16
ACMSPEEX::InternalInitEncoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    return -1;
}

WebRtc_Word16
ACMSPEEX::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    return -1;
}

WebRtc_Word32
ACMSPEEX::CodecDef(
    WebRtcNetEQ_CodecDef& /* codecDef  */,
    const CodecInst&      /* codecInst */)
{
    return -1;
}

ACMGenericCodec*
ACMSPEEX::CreateInstance(void)
{
    return NULL;
}

WebRtc_Word16
ACMSPEEX::InternalCreateEncoder()
{
    return -1;
}

void
ACMSPEEX::DestructEncoderSafe()
{
    return;
}


WebRtc_Word16
ACMSPEEX::InternalCreateDecoder()
{
    return -1;
}

void
ACMSPEEX::DestructDecoderSafe()
{
    return;
}

WebRtc_Word16
ACMSPEEX::SetBitRateSafe(
    const WebRtc_Word32 /* rate */)
{
    return -1;
}

void
ACMSPEEX::InternalDestructEncoderInst(
    void* /* ptrInst */)
{
    return;
}

WebRtc_Word16
ACMSPEEX::UnregisterFromNetEqSafe(
    ACMNetEQ*     /* netEq       */,
    WebRtc_Word16 /* payloadType */)
{
    return -1;
}

#ifdef UNUSEDSPEEX
WebRtc_Word16
ACMSPEEX::EnableVBR()
{
    return -1;
}

WebRtc_Word16
ACMSPEEX::DisableVBR()
{
    return -1;
}

WebRtc_Word16
ACMSPEEX::SetComplMode(
    WebRtc_Word16 mode)
{
    return -1;
}
#endif

#else     //===================== Actual Implementation =======================

ACMSPEEX::ACMSPEEX(WebRtc_Word16 codecID):
_encoderInstPtr(NULL),
_decoderInstPtr(NULL)
{
    _codecID = codecID;

    // Set sampling frequency, frame size and rate Speex
    if(_codecID == ACMCodecDB::kSPEEX8)
    {
        _samplingFrequency = 8000;
        _samplesIn20MsAudio = 160;
        _encodingRate = 11000;
    }
    else if(_codecID == ACMCodecDB::kSPEEX16)
    {
        _samplingFrequency = 16000;
        _samplesIn20MsAudio = 320;
        _encodingRate = 22000;
    }
    else
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "Wrong codec id for Speex.");

        _samplingFrequency = -1;
        _samplesIn20MsAudio = -1;
        _encodingRate = -1;
    }

    _hasInternalDTX = true;
    _dtxEnabled = false;
    _vbrEnabled = false;
    _complMode =  3; // default complexity value

    return;
}

ACMSPEEX::~ACMSPEEX()
{
    if(_encoderInstPtr != NULL)
    {
        WebRtcSpeex_FreeEnc(_encoderInstPtr);
        _encoderInstPtr = NULL;
    }
    if(_decoderInstPtr != NULL)
    {
        WebRtcSpeex_FreeDec(_decoderInstPtr);
        _decoderInstPtr = NULL;
    }
    return;
}

WebRtc_Word16
ACMSPEEX::InternalEncode(
    WebRtc_UWord8* bitStream,
    WebRtc_Word16* bitStreamLenByte)
{
    WebRtc_Word16 status;
    WebRtc_Word16 numEncodedSamples = 0;
    WebRtc_Word16 n = 0;

    while( numEncodedSamples < _frameLenSmpl)
    {
        status = WebRtcSpeex_Encode(_encoderInstPtr, &_inAudio[_inAudioIxRead],
            _encodingRate);

        // increment the read index this tell the caller that how far
        // we have gone forward in reading the audio buffer
        _inAudioIxRead += _samplesIn20MsAudio;
        numEncodedSamples += _samplesIn20MsAudio;

        if(status < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                "Error in Speex encoder");
            return status;
        }

        // Update VAD, if internal DTX is used
        if(_hasInternalDTX && _dtxEnabled)
        {
            _vadLabel[n++] = status;
            _vadLabel[n++] = status;
        }

        if(status == 0)
        {
            // This frame is detected as inactive. We need send whatever
            // encoded so far.
            *bitStreamLenByte = WebRtcSpeex_GetBitstream(_encoderInstPtr,
                (WebRtc_Word16*)bitStream);

            return *bitStreamLenByte;
        }
    }

    *bitStreamLenByte = WebRtcSpeex_GetBitstream(_encoderInstPtr,
        (WebRtc_Word16*)bitStream);
    return *bitStreamLenByte;
}

WebRtc_Word16
ACMSPEEX::DecodeSafe(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16  /* bitStreamLenByte */,
    WebRtc_Word16* /* audio            */,
    WebRtc_Word16* /* audioSamples     */,
    WebRtc_Word8*  /* speechType       */)
{
    return 0;
}

WebRtc_Word16
ACMSPEEX::EnableDTX()
{
    if(_dtxEnabled)
    {
        return 0;
    }
    else if(_encoderExist)  // check if encoder exist
    {
        // enable DTX
        if(WebRtcSpeex_EncoderInit(_encoderInstPtr, (_vbrEnabled ? 1:0), _complMode, 1) < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "Cannot enable DTX for Speex");
            return -1;
        }
        _dtxEnabled = true;
        return 0;
    }
    else
    {
        return -1;
    }

    return 0;
}

WebRtc_Word16
ACMSPEEX::DisableDTX()
{
    if(!_dtxEnabled)
    {
        return 0;
    }
    else if(_encoderExist)  // check if encoder exist
    {
        // disable DTX
        if(WebRtcSpeex_EncoderInit(_encoderInstPtr, (_vbrEnabled ? 1:0), _complMode, 0) < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "Cannot disable DTX for Speex");
            return -1;
        }
        _dtxEnabled = false;
        return 0;
    }
    else
    {
        // encoder doesn't exists, therefore disabling is harmless
        return 0;
    }

    return 0;
}

WebRtc_Word16
ACMSPEEX::InternalInitEncoder(
    WebRtcACMCodecParams* codecParams)
{
    // sanity check
    if (_encoderInstPtr == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
        "Cannot initialize Speex encoder, instance does not exist");
        return -1;
    }

    WebRtc_Word16 status = SetBitRateSafe((codecParams->codecInstant).rate);
    status += (WebRtcSpeex_EncoderInit(_encoderInstPtr, _vbrEnabled, _complMode, ((codecParams->enableDTX)? 1:0)) < 0)? -1:0;

    if (status >= 0) {
        return 0;
    } else {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
        "Error in initialization of Speex encoder");
        return -1;
    }
}

WebRtc_Word16
ACMSPEEX::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    WebRtc_Word16 status;

    // sanity check
    if (_decoderInstPtr == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
        "Cannot initialize Speex decoder, instance does not exist");
        return -1;
    }
    status = ((WebRtcSpeex_DecoderInit(_decoderInstPtr) < 0)? -1:0);

    if (status >= 0) {
        return 0;
    } else {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
        "Error in initialization of Speex decoder");
        return -1;
    }
}

WebRtc_Word32
ACMSPEEX::CodecDef(
    WebRtcNetEQ_CodecDef& codecDef,
    const CodecInst&      codecInst)
{
    if (!_decoderInitialized)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
        "Error, Speex decoder is not initialized");
        return -1;
    }

    // Fill up the structure by calling
    // "SET_CODEC_PAR" & "SET_SPEEX_FUNCTION."
    // Then call NetEQ to add the codec to it's
    // database.

    switch(_samplingFrequency)
    {
    case 8000:
        {
            SET_CODEC_PAR((codecDef), kDecoderSPEEX_8, codecInst.pltype,
                _decoderInstPtr, 8000);
            break;
        }
    case 16000:
        {
            SET_CODEC_PAR((codecDef), kDecoderSPEEX_16, codecInst.pltype,
                _decoderInstPtr, 16000);
            break;
        }
    default:
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "Unsupported sampling frequency for Speex");

            return -1;
        }
    }

    SET_SPEEX_FUNCTIONS((codecDef));
    return 0;
}

ACMGenericCodec*
ACMSPEEX::CreateInstance(void)
{
    return NULL;
}

WebRtc_Word16
ACMSPEEX::InternalCreateEncoder()
{
    return WebRtcSpeex_CreateEnc(&_encoderInstPtr, _samplingFrequency);
}

void
ACMSPEEX::DestructEncoderSafe()
{
    if(_encoderInstPtr != NULL)
    {
        WebRtcSpeex_FreeEnc(_encoderInstPtr);
        _encoderInstPtr = NULL;
    }
    // there is no encoder set the following
    _encoderExist = false;
    _encoderInitialized = false;
    _encodingRate = 0;
}


WebRtc_Word16
ACMSPEEX::InternalCreateDecoder()
{
    return WebRtcSpeex_CreateDec(&_decoderInstPtr, _samplingFrequency, 1);
}

void
ACMSPEEX::DestructDecoderSafe()
{
    if(_decoderInstPtr != NULL)
    {
        WebRtcSpeex_FreeDec(_decoderInstPtr);
        _decoderInstPtr = NULL;
    }
    // there is no encoder instance set the followings
    _decoderExist = false;
    _decoderInitialized = false;
}

WebRtc_Word16
ACMSPEEX::SetBitRateSafe(
    const WebRtc_Word32 rate)
{
    // Check if changed rate
    if (rate == _encodingRate) {
        return 0;
    } else if (rate > 2000) {
        _encodingRate = rate;
        _encoderParams.codecInstant.rate = rate;
    } else {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
        "Unsupported encoding rate for Speex");

        return -1;
    }

    return 0;
}


void
ACMSPEEX::InternalDestructEncoderInst(
    void* ptrInst)
{
    if(ptrInst != NULL)
    {
        WebRtcSpeex_FreeEnc((SPEEX_encinst_t_*)ptrInst);
    }
    return;
}


WebRtc_Word16
ACMSPEEX::UnregisterFromNetEqSafe(
    ACMNetEQ*     netEq,
    WebRtc_Word16 payloadType)
{
    if(payloadType != _decoderParams.codecInstant.pltype)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "Cannot unregister codec %s given payload-type %d does not match \
the stored payload type",
            _decoderParams.codecInstant.plname,
            payloadType,
            _decoderParams.codecInstant.pltype);
        return -1;
    }


    switch(_samplingFrequency)
    {
    case 8000:
        {
            return netEq->RemoveCodec(kDecoderSPEEX_8);
        }
    case 16000:
        {
            return netEq->RemoveCodec(kDecoderSPEEX_16);
        }
    default:
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "Could not unregister Speex from NetEQ. Sampling frequency doesn't match");
            return -1;
        }
    }
}


#ifdef UNUSEDSPEEX

// This API is currently not in use. If requested to be able to enable/disable VBR
// an ACM API need to be added.
WebRtc_Word16
ACMSPEEX::EnableVBR()
{
    if(_vbrEnabled)
    {
        return 0;
    }
    else if(_encoderExist)  // check if encoder exist
    {
        // enable Variable Bit Rate (VBR)
        if(WebRtcSpeex_EncoderInit(_encoderInstPtr, 1, _complMode, (_dtxEnabled? 1:0)) < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "Cannot enable VBR mode for Speex");

            return -1;
        }
        _vbrEnabled = true;
        return 0;
    }
    else
    {
        return -1;
    }
}


// This API is currently not in use. If requested to be able to enable/disable VBR
// an ACM API need to be added.
WebRtc_Word16
ACMSPEEX::DisableVBR()
{
    if(!_vbrEnabled)
    {
        return 0;
    }
    else if(_encoderExist)  // check if encoder exist
    {
        // disable DTX
        if(WebRtcSpeex_EncoderInit(_encoderInstPtr, 0, _complMode, (_dtxEnabled? 1:0)) < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "Cannot disable DTX for Speex");

            return -1;
        }
        _vbrEnabled = false;
        return 0;
    }
    else
    {
        // encoder doesn't exists, therefore disabling is harmless
        return 0;
    }
}

// This API is currently not in use. If requested to be able to set complexity
// an ACM API need to be added.
WebRtc_Word16
ACMSPEEX::SetComplMode(
    WebRtc_Word16 mode)
{
    // Check if new mode
    if(mode == _complMode)
    {
        return 0;
    }
    else if(_encoderExist)  // check if encoder exist
    {
        // Set new mode
        if(WebRtcSpeex_EncoderInit(_encoderInstPtr, 0, mode, (_dtxEnabled? 1:0)) < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "Error in complexity mode for Speex");
            return -1;
        }
        _complMode = mode;
        return 0;
    }
    else
    {
        // encoder doesn't exists, therefore disabling is harmless
        return 0;
    }
}

#endif

#endif

} // namespace webrtc
