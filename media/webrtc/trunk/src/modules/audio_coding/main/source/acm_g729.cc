/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "acm_g729.h"
#include "acm_common_defs.h"
#include "acm_neteq.h"
#include "trace.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_help_macros.h"

#ifdef WEBRTC_CODEC_G729
    // NOTE! G.729 is not included in the open-source package. The following
    // interface file is needed:
    //
    // /modules/audio_coding/codecs/g729/main/interface/g729_interface.h
    //
    // The API in the header file should match the one below.
    //
    // int16_t WebRtcG729_CreateEnc(G729_encinst_t_** inst);
    // int16_t WebRtcG729_CreateDec(G729_decinst_t_** inst);
    // int16_t WebRtcG729_FreeEnc(G729_encinst_t_* inst);
    // int16_t WebRtcG729_FreeDec(G729_decinst_t_* inst);
    // int16_t WebRtcG729_Encode(G729_encinst_t_* encInst, int16_t* input,
    //                                       int16_t len, int16_t* output);
    // int16_t WebRtcG729_EncoderInit(G729_encinst_t_* encInst, int16_t mode);
    // int16_t WebRtcG729_Decode(G729_decinst_t_* decInst);
    // int16_t WebRtcG729_DecodeBwe(G729_decinst_t_* decInst, int16_t* input);
    // int16_t WebRtcG729_DecodePlc(G729_decinst_t_* decInst);
    // int16_t WebRtcG729_DecoderInit(G729_decinst_t_* decInst);
    // void WebRtcG729_Version(char *versionStr, short len);
    #include "g729_interface.h"
#endif

namespace webrtc {

#ifndef WEBRTC_CODEC_G729

ACMG729::ACMG729(WebRtc_Word16 /* codecID */)
    : _encoderInstPtr(NULL),
      _decoderInstPtr(NULL) {
  return;
}


ACMG729::~ACMG729()
{
    return;
}


WebRtc_Word16
ACMG729::InternalEncode(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16* /* bitStreamLenByte */)
{
    return -1;
}


WebRtc_Word16
ACMG729::EnableDTX()
{
    return -1;
}


WebRtc_Word16
ACMG729::DisableDTX()
{
    return -1;
}

WebRtc_Word32
ACMG729::ReplaceInternalDTXSafe(
    const bool /*replaceInternalDTX*/)
{
    return -1;
}

WebRtc_Word32
ACMG729::IsInternalDTXReplacedSafe(
    bool* /* internalDTXReplaced */)
{
    return -1;
}


WebRtc_Word16
ACMG729::DecodeSafe(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16  /* bitStreamLenByte */,
    WebRtc_Word16* /* audio            */,
    WebRtc_Word16* /* audioSamples     */,
    WebRtc_Word8*  /* speechType       */)
{
    return -1;
}


WebRtc_Word16
ACMG729::InternalInitEncoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    return -1;
}


WebRtc_Word16
ACMG729::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    return -1;
}


WebRtc_Word32
ACMG729::CodecDef(
    WebRtcNetEQ_CodecDef& /* codecDef  */,
    const CodecInst&      /* codecInst */)
{
    return -1;
}


ACMGenericCodec*
ACMG729::CreateInstance(void)
{
    return NULL;
}


WebRtc_Word16
ACMG729::InternalCreateEncoder()
{
    return -1;
}


void
ACMG729::DestructEncoderSafe()
{
    return;
}


WebRtc_Word16
ACMG729::InternalCreateDecoder()
{
    return -1;
}


void
ACMG729::DestructDecoderSafe()
{
    return;
}


void
ACMG729::InternalDestructEncoderInst(
    void* /* ptrInst */)
{
    return;
}


WebRtc_Word16
ACMG729::UnregisterFromNetEqSafe(
    ACMNetEQ*     /* netEq       */,
    WebRtc_Word16 /* payloadType */)
{
    return -1;
}


#else     //===================== Actual Implementation =======================

ACMG729::ACMG729(
    WebRtc_Word16 codecID):
_encoderInstPtr(NULL),
_decoderInstPtr(NULL)
{
    _codecID = codecID;
    _hasInternalDTX = true;
    return;
}


ACMG729::~ACMG729()
{
    if(_encoderInstPtr != NULL)
    {
        // Delete encoder memory
        WebRtcG729_FreeEnc(_encoderInstPtr);
        _encoderInstPtr = NULL;
    }
    if(_decoderInstPtr != NULL)
    {
        // Delete decoder memory
        WebRtcG729_FreeDec(_decoderInstPtr);
        _decoderInstPtr = NULL;
    }
    return;
}


WebRtc_Word16
ACMG729::InternalEncode(
    WebRtc_UWord8* bitStream,
    WebRtc_Word16* bitStreamLenByte)
{
    // Initialize before entering the loop
    WebRtc_Word16 noEncodedSamples = 0;
    WebRtc_Word16 tmpLenByte = 0;
    WebRtc_Word16 vadDecision = 0;
    *bitStreamLenByte = 0;
    while(noEncodedSamples < _frameLenSmpl)
    {
        // Call G.729 encoder with pointer to encoder memory, input
        // audio, number of samples and bitsream
        tmpLenByte = WebRtcG729_Encode(_encoderInstPtr,
            &_inAudio[_inAudioIxRead], 80,
            (WebRtc_Word16*)(&(bitStream[*bitStreamLenByte])));

        // increment the read index this tell the caller that how far
        // we have gone forward in reading the audio buffer
        _inAudioIxRead += 80;

        // sanity check
        if(tmpLenByte < 0)
        {
            // error has happened
            *bitStreamLenByte = 0;
            return -1;
        }

        // increment number of written bytes
        *bitStreamLenByte += tmpLenByte;
        switch(tmpLenByte)
        {
        case 0:
            {
                if(0 == noEncodedSamples)
                {
                    // this is the first 10 ms in this packet and there is
                    // no data generated, perhaps DTX is enabled and the
                    // codec is not generating any bit-stream for this 10 ms.
                    // we do not continue encoding this frame.
                    return 0;
                }
                break;
            }
        case 2:
            {
                // check if G.729 internal DTX is enabled
                if(_hasInternalDTX && _dtxEnabled)
                {
                    vadDecision = 0;
                    for(WebRtc_Word16 n = 0; n < MAX_FRAME_SIZE_10MSEC; n++)
                    {
                        _vadLabel[n] = vadDecision;
                    }
                }
                // we got a SID and have to send out this packet no matter
                // how much audio we have encoded
                return *bitStreamLenByte;
            }
        case 10:
            {
                vadDecision = 1;
                // this is a valid length just continue encoding
                break;
            }
        default:
            {
                return -1;
            }
        }

        // update number of encoded samples
        noEncodedSamples += 80;
    }

    // update VAD decision vector
    if(_hasInternalDTX && !vadDecision && _dtxEnabled)
    {
        for(WebRtc_Word16 n = 0; n < MAX_FRAME_SIZE_10MSEC; n++)
        {
            _vadLabel[n] = vadDecision;
        }
    }

    // done encoding, return number of encoded bytes
    return *bitStreamLenByte;
}


WebRtc_Word16
ACMG729::EnableDTX()
{
    if(_dtxEnabled)
    {
        // DTX already enabled, do nothing
        return 0;
    }
    else if(_encoderExist)
    {
        // Re-init the G.729 encoder to turn on DTX
        if(WebRtcG729_EncoderInit(_encoderInstPtr, 1) < 0)
        {
            return -1;
        }
        _dtxEnabled = true;
        return 0;
    }
    else
    {
        return -1;
    }
}


WebRtc_Word16
ACMG729::DisableDTX()
{
    if(!_dtxEnabled)
    {
        // DTX already dissabled, do nothing
        return 0;
    }
    else if(_encoderExist)
    {
        // Re-init the G.729 decoder to turn off DTX
        if(WebRtcG729_EncoderInit(_encoderInstPtr, 0) < 0)
        {
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
}


WebRtc_Word32
ACMG729::ReplaceInternalDTXSafe(
    const bool replaceInternalDTX)
{
    // This function is used to dissable the G.729 built in DTX and use an
    // external instead.

    if(replaceInternalDTX == _hasInternalDTX)
    {
        // Make sure we keep the DTX/VAD setting if possible
        bool oldEnableDTX = _dtxEnabled;
        bool oldEnableVAD = _vadEnabled;
        ACMVADMode oldMode = _vadMode;
        if (replaceInternalDTX)
        {
            // Disable internal DTX before enabling external DTX
            DisableDTX();
        }
        else
        {
            // Disable external DTX before enabling internal
            ACMGenericCodec::DisableDTX();
        }
        _hasInternalDTX = !replaceInternalDTX;
        WebRtc_Word16 status = SetVADSafe(oldEnableDTX, oldEnableVAD, oldMode);
        // Check if VAD status has changed from inactive to active, or if error was
        // reported
        if (status == 1) {
            _vadEnabled = true;
            return status;
        } else if (status < 0) {
            _hasInternalDTX = replaceInternalDTX;
            return -1;
        }
    }
    return 0;
}


WebRtc_Word32
ACMG729::IsInternalDTXReplacedSafe(
    bool* internalDTXReplaced)
{
    // Get status of wether DTX is replaced or not
    *internalDTXReplaced = !_hasInternalDTX;
    return 0;
}


WebRtc_Word16
ACMG729::DecodeSafe(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16  /* bitStreamLenByte */,
    WebRtc_Word16* /* audio            */,
    WebRtc_Word16* /* audioSamples     */,
    WebRtc_Word8*  /* speechType       */)
{
    // This function is not used. G.729 decoder is called from inside NetEQ
    return 0;
}


WebRtc_Word16
ACMG729::InternalInitEncoder(
    WebRtcACMCodecParams* codecParams)
{
    // Init G.729 encoder
    return WebRtcG729_EncoderInit(_encoderInstPtr,
        ((codecParams->enableDTX)? 1:0));
}


WebRtc_Word16
ACMG729::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    // Init G.729 decoder
    return WebRtcG729_DecoderInit(_decoderInstPtr);
}


WebRtc_Word32
ACMG729::CodecDef(
    WebRtcNetEQ_CodecDef& codecDef,
    const CodecInst&      codecInst)
{
    if (!_decoderInitialized)
    {
        // Todo:
        // log error
        return -1;
    }

    // Fill up the structure by calling
    // "SET_CODEC_PAR" & "SET_G729_FUNCTION."
    // Then call NetEQ to add the codec to it's
    // database.
    SET_CODEC_PAR((codecDef), kDecoderG729, codecInst.pltype,
        _decoderInstPtr, 8000);
    SET_G729_FUNCTIONS((codecDef));
    return 0;
}


ACMGenericCodec*
ACMG729::CreateInstance(void)
{
    // Function not used
    return NULL;
}


WebRtc_Word16
ACMG729::InternalCreateEncoder()
{
    // Create encoder memory
    return WebRtcG729_CreateEnc(&_encoderInstPtr);
}


void
ACMG729::DestructEncoderSafe()
{
    // Free encoder memory
    _encoderExist = false;
    _encoderInitialized = false;
    if(_encoderInstPtr != NULL)
    {
        WebRtcG729_FreeEnc(_encoderInstPtr);
        _encoderInstPtr = NULL;
    }
}


WebRtc_Word16
ACMG729::InternalCreateDecoder()
{
    // Create decoder memory
    return WebRtcG729_CreateDec(&_decoderInstPtr);
}


void
ACMG729::DestructDecoderSafe()
{
    // Free decoder memory
    _decoderExist = false;
    _decoderInitialized = false;
    if(_decoderInstPtr != NULL)
    {
        WebRtcG729_FreeDec(_decoderInstPtr);
        _decoderInstPtr = NULL;
    }
}


void
ACMG729::InternalDestructEncoderInst(
    void* ptrInst)
{
    if(ptrInst != NULL)
    {
        WebRtcG729_FreeEnc((G729_encinst_t_*)ptrInst);
    }
    return;
}


WebRtc_Word16
ACMG729::UnregisterFromNetEqSafe(
    ACMNetEQ*     netEq,
    WebRtc_Word16 payloadType)
{
    // Remove codec from the NetEQ database
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
    return netEq->RemoveCodec(kDecoderG729);
}

#endif

} // namespace webrtc
