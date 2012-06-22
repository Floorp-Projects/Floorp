/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <string.h>

#include "acm_codec_database.h"
#include "acm_common_defs.h"
#include "acm_generic_codec.h"
#include "acm_neteq.h"
#include "trace.h"
#include "webrtc_vad.h"
#include "webrtc_cng.h"

namespace webrtc
{

// Enum for CNG
enum
{
    kMaxPLCParamsCNG = WEBRTC_CNG_MAX_LPC_ORDER,
    kNewCNGNumPLCParams = 8
};

#define ACM_SID_INTERVAL_MSEC 100

// We set some of the variables to invalid values as a check point
// if a proper initialization has happened. Another approach is
// to initialize to a default codec that we are sure is always included.
ACMGenericCodec::ACMGenericCodec()
    : _inAudioIxWrite(0),
      _inAudioIxRead(0),
      _inTimestampIxWrite(0),
      _inAudio(NULL),
      _inTimestamp(NULL),
      _frameLenSmpl(-1),  // invalid value
      _noChannels(1),
      _codecID(-1),  // invalid value
      _noMissedSamples(0),
      _encoderExist(false),
      _decoderExist(false),
      _encoderInitialized(false),
      _decoderInitialized(false),
      _registeredInNetEq(false),
      _hasInternalDTX(false),
      _ptrVADInst(NULL),
      _vadEnabled(false),
      _vadMode(VADNormal),
      _dtxEnabled(false),
      _ptrDTXInst(NULL),
      _numLPCParams(kNewCNGNumPLCParams),
      _sentCNPrevious(false),
      _isMaster(true),
      _netEqDecodeLock(NULL),
      _codecWrapperLock(*RWLockWrapper::CreateRWLock()),
      _lastEncodedTimestamp(0),
      _lastTimestamp(0xD87F3F9F),
      _isAudioBuffFresh(true),
      _uniqueID(0) {
  // Initialize VAD vector.
  for (int i = 0; i < MAX_FRAME_SIZE_10MSEC; i++) {
    _vadLabel[i] = 0;
  }

  // Nullify memory for encoder and decoder, and set payload type to an
  // invalid value.
  memset(&_encoderParams, 0, sizeof(WebRtcACMCodecParams));
  _encoderParams.codecInstant.pltype = -1;
  memset(&_decoderParams, 0, sizeof(WebRtcACMCodecParams));
  _decoderParams.codecInstant.pltype = -1;
}

ACMGenericCodec::~ACMGenericCodec()
{
    // Check all the members which are pointers and
    // if they are not NULL delete/free them.

    if(_ptrVADInst != NULL)
    {
        WebRtcVad_Free(_ptrVADInst);
        _ptrVADInst = NULL;
    }

    if (_inAudio != NULL)
    {
        delete [] _inAudio;
        _inAudio = NULL;
    }

    if (_inTimestamp != NULL)
    {
        delete [] _inTimestamp;
        _inTimestamp = NULL;
    }
    if(_ptrDTXInst != NULL)
    {
        WebRtcCng_FreeEnc(_ptrDTXInst);
        _ptrDTXInst = NULL;
    }
    delete &_codecWrapperLock;
}

WebRtc_Word32
ACMGenericCodec::Add10MsData(
    const WebRtc_UWord32 timestamp,
    const WebRtc_Word16* data,
    const WebRtc_UWord16 lengthSmpl,
    const WebRtc_UWord8  audioChannel)
{
    WriteLockScoped wl(_codecWrapperLock);
    return Add10MsDataSafe(timestamp, data, lengthSmpl, audioChannel);
}

WebRtc_Word32
ACMGenericCodec::Add10MsDataSafe(
    const WebRtc_UWord32 timestamp,
    const WebRtc_Word16* data,
    const WebRtc_UWord16 lengthSmpl,
    const WebRtc_UWord8  audioChannel)
{
    // The codec expects to get data in correct sampling rate.
    // get the sampling frequency of the codec
    WebRtc_UWord16 plFreqHz;

    if(EncoderSampFreq(plFreqHz) < 0)
    {
        // _codecID is not correct, perhaps the codec is not initialized yet.
        return -1;
    }

    // Sanity check, if the length of the input corresponds to 10 ms.
    if((plFreqHz / 100) != lengthSmpl)
    {
        // This is not 10 ms of audio, given the sampling frequency of the
        // codec
        return -1;
    }
    if(_lastTimestamp == timestamp)
    {
        // Same timestamp as the last time, overwrite.
        if((_inAudioIxWrite >= lengthSmpl) && (_inTimestampIxWrite > 0))
        {
            _inAudioIxWrite -= lengthSmpl;
            _inTimestampIxWrite--;
            WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, _uniqueID,
                "Adding 10ms with previous timestamp, \
overwriting the previous 10ms");
        }
        else
        {
            WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, _uniqueID,
                "Adding 10ms with previous timestamp, this will sound bad");
        }
    }

    _lastTimestamp = timestamp;

    if ((_inAudioIxWrite + lengthSmpl*audioChannel) > AUDIO_BUFFER_SIZE_W16)
    {
        // Get the number of samples to be overwritten
        WebRtc_Word16 missedSamples = _inAudioIxWrite + lengthSmpl*audioChannel -
            AUDIO_BUFFER_SIZE_W16;

        // Move the data (overwite the old data)
        memmove(_inAudio, _inAudio + missedSamples,
            (AUDIO_BUFFER_SIZE_W16 - lengthSmpl*audioChannel)*sizeof(WebRtc_Word16));
        // Copy the new data
        memcpy(_inAudio + (AUDIO_BUFFER_SIZE_W16 - lengthSmpl*audioChannel), data,
            lengthSmpl*audioChannel * sizeof(WebRtc_Word16));

        // Get the number of 10 ms blocks which are overwritten
        WebRtc_Word16 missed10MsecBlocks =
            (WebRtc_Word16)((missedSamples/audioChannel * 100) / plFreqHz);

        // Move the timestamps
        memmove(_inTimestamp, _inTimestamp + missed10MsecBlocks,
            (_inTimestampIxWrite - missed10MsecBlocks) * sizeof(WebRtc_UWord32));
        _inTimestampIxWrite -= missed10MsecBlocks;
        _inTimestamp[_inTimestampIxWrite] = timestamp;
        _inTimestampIxWrite++;

        // Buffer is full
        _inAudioIxWrite = AUDIO_BUFFER_SIZE_W16;
        IncreaseNoMissedSamples(missedSamples);
        _isAudioBuffFresh = false;
        return -missedSamples;
    }
    memcpy(_inAudio + _inAudioIxWrite, data, lengthSmpl*audioChannel * sizeof(WebRtc_Word16));
    _inAudioIxWrite += lengthSmpl*audioChannel;

    assert(_inTimestampIxWrite < TIMESTAMP_BUFFER_SIZE_W32);
    assert(_inTimestampIxWrite >= 0);

    _inTimestamp[_inTimestampIxWrite] = timestamp;
    _inTimestampIxWrite++;
    _isAudioBuffFresh = false;
    return 0;
}

WebRtc_Word16
ACMGenericCodec::Encode(
    WebRtc_UWord8*         bitStream,
    WebRtc_Word16*         bitStreamLenByte,
    WebRtc_UWord32*        timeStamp,
    WebRtcACMEncodingType* encodingType)
{
    WriteLockScoped lockCodec(_codecWrapperLock);
    ReadLockScoped lockNetEq(*_netEqDecodeLock);
    return EncodeSafe(bitStream, bitStreamLenByte,
        timeStamp, encodingType);
}


WebRtc_Word16
ACMGenericCodec::EncodeSafe(
    WebRtc_UWord8*         bitStream,
    WebRtc_Word16*         bitStreamLenByte,
    WebRtc_UWord32*        timeStamp,
    WebRtcACMEncodingType* encodingType)
{
    // Do we have enough data to encode?
    // we wait until we have a full frame to encode.
    if(_inAudioIxWrite < _frameLenSmpl*_noChannels)
    {
        // There is not enough audio
        *timeStamp = 0;
        *bitStreamLenByte = 0;
        // Doesn't really matter what this parameter set to
        *encodingType = kNoEncoding;
        return 0;
    }

    // Not all codecs accept the whole frame to be pushed into
    // encoder at once.
    const WebRtc_Word16 myBasicCodingBlockSmpl =
        ACMCodecDB::BasicCodingBlock(_codecID);
    if((myBasicCodingBlockSmpl < 0) ||
        (!_encoderInitialized) ||
        (!_encoderExist))
    {
        // This should not happen
        *timeStamp = 0;
        *bitStreamLenByte = 0;
        *encodingType = kNoEncoding;
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "EncodeSafe: error, basic coding sample block is negative");
        return -1;
    }

    // This makes the internal encoder read from the begining of the buffer
    _inAudioIxRead = 0;
    *timeStamp = _inTimestamp[0];

    // Process the audio through VAD the function doesn't set _vadLabels.
    // If VAD is disabled all labels are set to ONE (active)
    WebRtc_Word16 status = 0;
    WebRtc_Word16 dtxProcessedSamples = 0;

    status = ProcessFrameVADDTX(bitStream, bitStreamLenByte,
        &dtxProcessedSamples);

    if(status < 0)
    {
        *timeStamp = 0;
        *bitStreamLenByte = 0;
        *encodingType = kNoEncoding;
    }
    else
    {
        if(dtxProcessedSamples > 0)
        {
            // Dtx have processed some samples may or may not a bit-stream
            // is generated we should not do any encoding (normally there
            // will be not enough data)

            // Setting the following makes that the move of audio data
            // and timestamps happen correctly
            _inAudioIxRead = dtxProcessedSamples;
            // This will let the owner of ACMGenericCodec to know that the
            // generated bit-stream is DTX to use correct payload type
            WebRtc_UWord16 sampFreqHz;
            EncoderSampFreq(sampFreqHz);
            if (sampFreqHz == 8000) {
                *encodingType = kPassiveDTXNB;
            } else if (sampFreqHz == 16000) {
                *encodingType = kPassiveDTXWB;
            } else if (sampFreqHz == 32000) {
                *encodingType = kPassiveDTXSWB;
            } else {
                status = -1;
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                    "EncodeSafe: Wrong sampling frequency for DTX.");
            }

            // Transport empty frame if we have an empty bitstream
            if ((*bitStreamLenByte == 0)
                && (_sentCNPrevious || ((_inAudioIxWrite - _inAudioIxRead) <= 0))
                )
            {
                // Makes sure we transmit an empty frame
                *bitStreamLenByte = 1;
                *encodingType = kNoEncoding;
            }
            _sentCNPrevious = true;
        }
        else
        {
            _sentCNPrevious = false;
            // This will let the caller of the method to know if the frame is
            // Active or non-Active The caller of the method knows that the
            // stream is encoded by codec and can use the info for callbacks,
            // if any registered.
            if(myBasicCodingBlockSmpl == 0)
            {
                // This codec can handle all allowed frame sizes as basic
                // coding block
                status = InternalEncode(bitStream, bitStreamLenByte);

                if(status < 0)
                {
                    // TODO:
                    // Maybe reseting the encoder to be fresh for the next
                    // frame
                    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                        "EncodeSafe: error in internalEncode");
                    *bitStreamLenByte = 0;
                    *encodingType = kNoEncoding;
                }
            }
            else
            {
                // A basic-coding-block for this codec is defined so we loop
                // over the audio with the steps of the basic-coding-block.
                // It is not necessary that in each itteration
                WebRtc_Word16 tmpBitStreamLenByte;

                // Reset the variables which will be increamented in the loop
                *bitStreamLenByte = 0;
                bool done = false;
                while(!done)
                {
                    status = InternalEncode(&bitStream[*bitStreamLenByte],
                        &tmpBitStreamLenByte);
                    *bitStreamLenByte += tmpBitStreamLenByte;

                    // Guard Against errors and too large payloads
                    if((status < 0) ||
                        (*bitStreamLenByte > MAX_PAYLOAD_SIZE_BYTE))
                    {
                        // Error has happened if we are in the middle of a full
                        // frame we have to exit. Before exiting, whatever bits
                        // are in the buffer are probably corruptred. Anyways
                        // we ignore them.
                        *bitStreamLenByte = 0;
                        *encodingType = kNoEncoding;
                        // We might have come here because of the second
                        // condition.
                        status = -1;
                         WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding,
                            _uniqueID, "EncodeSafe: error in InternalEncode");
                        // break from the loop
                        break;
                    }

                    done = _inAudioIxRead >= _frameLenSmpl;
                }
            }
            if(status >= 0)
            {
                *encodingType = (_vadLabel[0] == 1)?
                kActiveNormalEncoded:kPassiveNormalEncoded;
                // Transport empty frame if we have an empty bitsteram
                if ((*bitStreamLenByte == 0) && ((_inAudioIxWrite - _inAudioIxRead) <= 0))
                {
                    // Makes sure we transmit an empty frame
                    *bitStreamLenByte = 1;
                    *encodingType = kNoEncoding;
                }
            }
        }
    }

    // Move the timestampe buffer according to the number of 10 ms blocks
    // which are read.
    WebRtc_UWord16 sampFreqHz;
    EncoderSampFreq(sampFreqHz);

    WebRtc_Word16 num10MsecBlocks =
            (WebRtc_Word16)((_inAudioIxRead/_noChannels * 100) / sampFreqHz);
    if(_inTimestampIxWrite > num10MsecBlocks)
    {
        memmove(_inTimestamp, _inTimestamp + num10MsecBlocks,
            (_inTimestampIxWrite - num10MsecBlocks) * sizeof(WebRtc_Word32));
    }
    _inTimestampIxWrite -= num10MsecBlocks;

    // We have to move the audio that is not encoded to the beginning
    // of the buffer and accordingly adjust the read and write indices.
    if(_inAudioIxRead < _inAudioIxWrite)
    {
        memmove(_inAudio, &_inAudio[_inAudioIxRead],
            (_inAudioIxWrite - _inAudioIxRead)*sizeof(WebRtc_Word16));
    }

    _inAudioIxWrite -= _inAudioIxRead;

    _inAudioIxRead = 0;
    _lastEncodedTimestamp = *timeStamp;
    return (status < 0) ? (-1):(*bitStreamLenByte);
}

WebRtc_Word16
ACMGenericCodec::Decode(
    WebRtc_UWord8* bitStream,
    WebRtc_Word16  bitStreamLenByte,
    WebRtc_Word16* audio,
    WebRtc_Word16* audioSamples,
    WebRtc_Word8*  speechType)
{
    WriteLockScoped wl(_codecWrapperLock);
    return DecodeSafe(bitStream, bitStreamLenByte, audio,
        audioSamples, speechType);
}

bool
ACMGenericCodec::EncoderInitialized()
{
    ReadLockScoped rl(_codecWrapperLock);
    return _encoderInitialized;
}

bool
ACMGenericCodec::DecoderInitialized()
{
    ReadLockScoped rl(_codecWrapperLock);
    return _decoderInitialized;
}


WebRtc_Word32
ACMGenericCodec::RegisterInNetEq(
    ACMNetEQ*   netEq,
    const CodecInst& codecInst)
{
    WebRtcNetEQ_CodecDef codecDef;
    WriteLockScoped wl(_codecWrapperLock);

    if(CodecDef(codecDef, codecInst) < 0)
    {
        // Failed to register
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "RegisterInNetEq: error, failed to register");
        _registeredInNetEq = false;
        return -1;
    }
    else
    {
        if(netEq->AddCodec(&codecDef, _isMaster) < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                "RegisterInNetEq: error, failed to add codec");
            _registeredInNetEq = false;
            return -1;
        }
        // Registered
        _registeredInNetEq = true;
        return 0;
    }
}

WebRtc_Word16
ACMGenericCodec::EncoderParams(
    WebRtcACMCodecParams* encParams)
{
    ReadLockScoped rl(_codecWrapperLock);
    return EncoderParamsSafe(encParams);
}

WebRtc_Word16
ACMGenericCodec::EncoderParamsSafe(
    WebRtcACMCodecParams* encParams)
{
    // Codec parameters are valid only if the encoder is initialized
    if(_encoderInitialized)
    {
        WebRtc_Word32 currentRate;
        memcpy(encParams, &_encoderParams, sizeof(WebRtcACMCodecParams));
        currentRate = encParams->codecInstant.rate;
        CurrentRate(currentRate);
        encParams->codecInstant.rate = currentRate;
        return 0;
    }
    else
    {
        encParams->codecInstant.plname[0] = '\0';
        encParams->codecInstant.pltype    = -1;
        encParams->codecInstant.pacsize   = 0;
        encParams->codecInstant.rate      = 0;
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "EncoderParamsSafe: error, encoder not initialized");
        return -1;
    }
}

bool
ACMGenericCodec::DecoderParams(
    WebRtcACMCodecParams* decParams,
    const WebRtc_UWord8   payloadType)
{
    ReadLockScoped rl(_codecWrapperLock);
    return DecoderParamsSafe(decParams, payloadType);
}

bool
ACMGenericCodec::DecoderParamsSafe(
    WebRtcACMCodecParams* decParams,
    const WebRtc_UWord8   payloadType)
{
    // Decoder parameters are valid only if decoder is initialized
    if(_decoderInitialized)
    {
        if(payloadType == _decoderParams.codecInstant.pltype)
        {
            memcpy(decParams, &_decoderParams, sizeof(WebRtcACMCodecParams));
            return true;
        }
    }

    decParams->codecInstant.plname[0] = '\0';
    decParams->codecInstant.pltype    = -1;
    decParams->codecInstant.pacsize   = 0;
    decParams->codecInstant.rate      = 0;
    return false;
}

WebRtc_Word16
ACMGenericCodec::ResetEncoder()
{
    WriteLockScoped lockCodec(_codecWrapperLock);
    ReadLockScoped lockNetEq(*_netEqDecodeLock);
    return ResetEncoderSafe();
}

WebRtc_Word16
ACMGenericCodec::ResetEncoderSafe()
{
    if(!_encoderExist || !_encoderInitialized)
    {
        // We don't reset if doesn't exists or not initialized yet
        return 0;
    }

    _inAudioIxWrite     = 0;
    _inAudioIxRead      = 0;
    _inTimestampIxWrite = 0;
    _noMissedSamples    = 0;
    _isAudioBuffFresh   = true;
    memset(_inAudio, 0, AUDIO_BUFFER_SIZE_W16 * sizeof(WebRtc_Word16));
    memset(_inTimestamp, 0, TIMESTAMP_BUFFER_SIZE_W32 * sizeof(WebRtc_Word32));

    // Store DTX/VAD params
    bool enableVAD = _vadEnabled;
    bool enableDTX = _dtxEnabled;
    ACMVADMode mode = _vadMode;

    // Reset the encoder
    if(InternalResetEncoder() < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "ResetEncoderSafe: error in reset encoder");
        return -1;
    }

    // Disable DTX & VAD this deletes the states
    // we like to have fresh start
    DisableDTX();
    DisableVAD();

    // Set DTX/VAD
    return SetVADSafe(enableDTX, enableVAD, mode);
}

WebRtc_Word16
ACMGenericCodec::InternalResetEncoder()
{
    // For most of the codecs it is sufficient to
    // call their internal initialization.
    // There are some exceptions.
    // ----
    // For iSAC we don't want to lose BWE history,
    // so for iSAC we have to over-write this function.
    // ----
    return InternalInitEncoder(&_encoderParams);
}

WebRtc_Word16
ACMGenericCodec::InitEncoder(
    WebRtcACMCodecParams* codecParams,
    bool                  forceInitialization)
{
    WriteLockScoped lockCodec(_codecWrapperLock);
    ReadLockScoped lockNetEq(*_netEqDecodeLock);
    return InitEncoderSafe(codecParams, forceInitialization);
}

WebRtc_Word16
ACMGenericCodec::InitEncoderSafe(
    WebRtcACMCodecParams* codecParams,
    bool                  forceInitialization)
{
    // Check if we got a valid set of parameters
    int mirrorID;
    int codecNumber =
        ACMCodecDB::CodecNumber(&(codecParams->codecInstant), &mirrorID);

    if(codecNumber < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "InitEncoderSafe: error, codec number negative");
        return -1;
    }
    // Check if the parameters are for this codec
    if((_codecID >= 0) && (_codecID != codecNumber) && (_codecID != mirrorID))
    {
        // The current codec is not the same as the one given by codecParams
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "InitEncoderSafe: current codec is not the same as the one given by codecParams");
        return -1;
    }

    if(!CanChangeEncodingParam(codecParams->codecInstant))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "InitEncoderSafe: cannot change encoding parameters");
        return -1;
    }

    if(_encoderInitialized && !forceInitialization)
    {
        // The encoder is already initialized
        return 0;
    }
    WebRtc_Word16 status;
    if(!_encoderExist)
    {
        _encoderInitialized = false;
        status = CreateEncoder();
        if(status < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "InitEncoderSafe: cannot create encoder");
            return -1;
        }
        else
        {
            _encoderExist = true;
        }
    }
    _frameLenSmpl = (codecParams->codecInstant).pacsize;
    _noChannels = codecParams->codecInstant.channels;
    status = InternalInitEncoder(codecParams);
    if(status < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "InitEncoderSafe: error in init encoder");
        _encoderInitialized = false;
        return -1;
    }
    else
    {
        memcpy(&_encoderParams, codecParams, sizeof(WebRtcACMCodecParams));
        _encoderInitialized = true;
        if(_inAudio == NULL)
        {
            _inAudio = new WebRtc_Word16[AUDIO_BUFFER_SIZE_W16];
            if(_inAudio == NULL)
            {
                return -1;
            }
            memset(_inAudio, 0, AUDIO_BUFFER_SIZE_W16 * sizeof(WebRtc_Word16));
        }
        if(_inTimestamp == NULL)
        {
            _inTimestamp = new WebRtc_UWord32[TIMESTAMP_BUFFER_SIZE_W32];
            if(_inTimestamp == NULL)
            {
                return -1;
            }
            memset(_inTimestamp, 0, sizeof(WebRtc_UWord32) *
                TIMESTAMP_BUFFER_SIZE_W32);
        }
        _isAudioBuffFresh = true;
    }
    status = SetVADSafe(codecParams->enableDTX, codecParams->enableVAD,
        codecParams->vadMode);

    return status;
}

bool
ACMGenericCodec::CanChangeEncodingParam(
    CodecInst& /*codecInst*/)
{
    return true;
}

WebRtc_Word16
ACMGenericCodec::InitDecoder(
    WebRtcACMCodecParams* codecParams,
    bool                  forceInitialization)
{
    WriteLockScoped lockCodc(_codecWrapperLock);
    WriteLockScoped lockNetEq(*_netEqDecodeLock);
    return InitDecoderSafe(codecParams, forceInitialization);
}

WebRtc_Word16
ACMGenericCodec::InitDecoderSafe(
    WebRtcACMCodecParams* codecParams,
    bool                  forceInitialization)
{
    int mirrorID;
    // Check if we got a valid set of parameters
    int codecNumber =
        ACMCodecDB::ReceiverCodecNumber(&codecParams->codecInstant, &mirrorID);

    if(codecNumber < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                    "InitDecoderSafe: error, invalid codec number");
        return -1;
    }
    // Check if the parameters are for this codec
    if((_codecID >= 0) && (_codecID != codecNumber) && (_codecID != mirrorID))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                    "InitDecoderSafe: current codec is not the same as the one given "
                    "by codecParams");
        // The current codec is not the same as the one given by codecParams
        return -1;
    }


    if(_decoderInitialized && !forceInitialization)
    {
        // The encoder is already initialized
        return 0;
    }

    WebRtc_Word16 status;
    if(!_decoderExist)
    {
        _decoderInitialized = false;
        status = CreateDecoder();
        if(status < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                    "InitDecoderSafe: cannot create decoder");
            return -1;
        }
        else
        {
            _decoderExist = true;
        }
    }

    status = InternalInitDecoder(codecParams);
    if(status < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                "InitDecoderSafe: cannot init decoder");
        _decoderInitialized = false;
        return -1;
    }
    else
    {
        // Store the parameters
        SaveDecoderParamSafe(codecParams);
        _decoderInitialized = true;
    }
    return 0;
}

WebRtc_Word16
ACMGenericCodec::ResetDecoder(WebRtc_Word16 payloadType)
{
    WriteLockScoped lockCodec(_codecWrapperLock);
    WriteLockScoped lockNetEq(*_netEqDecodeLock);
    return ResetDecoderSafe(payloadType);
}

WebRtc_Word16
ACMGenericCodec::ResetDecoderSafe(WebRtc_Word16 payloadType)
{
    WebRtcACMCodecParams decoderParams;
    if(!_decoderExist || !_decoderInitialized)
    {
        return 0;
    }
    // Initialization of the decoder should work for all
    // the codec. If there is a codec that has to keep
    // some states then we need to define a virtual and
    // overwrite in that codec
    DecoderParamsSafe(&decoderParams, (WebRtc_UWord8) payloadType);
    return InternalInitDecoder(&decoderParams);
}

void
ACMGenericCodec::ResetNoMissedSamples()
{
    WriteLockScoped cs(_codecWrapperLock);
    _noMissedSamples = 0;
}

void
ACMGenericCodec::IncreaseNoMissedSamples(
    const WebRtc_Word16 noSamples)
{
    _noMissedSamples += noSamples;
}

// Get the number of missed samples, this can be public
WebRtc_UWord32
ACMGenericCodec::NoMissedSamples() const
{
    ReadLockScoped cs(_codecWrapperLock);
    return _noMissedSamples;
}
void
ACMGenericCodec::DestructEncoder()
{
    WriteLockScoped wl(_codecWrapperLock);

    // Disable VAD and delete the instance
    if(_ptrVADInst != NULL)
    {
        WebRtcVad_Free(_ptrVADInst);
        _ptrVADInst = NULL;
    }
    _vadEnabled = false;
    _vadMode = VADNormal;

    //Disable DTX and delete the instance
    _dtxEnabled = false;
    if(_ptrDTXInst != NULL)
    {
        WebRtcCng_FreeEnc(_ptrDTXInst);
        _ptrDTXInst = NULL;
    }
    _numLPCParams = kNewCNGNumPLCParams;

    DestructEncoderSafe();
}

void
ACMGenericCodec::DestructDecoder()
{
    WriteLockScoped wl(_codecWrapperLock);
    _decoderParams.codecInstant.pltype = -1;
    DestructDecoderSafe();
}

WebRtc_Word16
ACMGenericCodec::SetBitRate(
    const WebRtc_Word32 bitRateBPS)
{
    WriteLockScoped wl(_codecWrapperLock);
    return SetBitRateSafe(bitRateBPS);
}

WebRtc_Word16
ACMGenericCodec::SetBitRateSafe(
    const WebRtc_Word32 bitRateBPS)
{
    // If the codec can change the bit-rate this function
    // should be overwritten, otherewise the only acceptable
    // value is the one that is in database.
    CodecInst codecParams;
    if(ACMCodecDB::Codec(_codecID, &codecParams) < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "SetBitRateSafe: error in ACMCodecDB::Codec");
        return -1;
    }
    if(codecParams.rate != bitRateBPS)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "SetBitRateSafe: rate value is not acceptable");
        return -1;
    }
    else
    {
        return 0;
    }
}

WebRtc_Word32
ACMGenericCodec::GetEstimatedBandwidth()
{
    WriteLockScoped wl(_codecWrapperLock);
    return GetEstimatedBandwidthSafe();
}

WebRtc_Word32
ACMGenericCodec::GetEstimatedBandwidthSafe()
{
    // All codecs but iSAC will return -1
    return -1;
}

WebRtc_Word32
ACMGenericCodec::SetEstimatedBandwidth(
    WebRtc_Word32 estimatedBandwidth)
{
    WriteLockScoped wl(_codecWrapperLock);
    return SetEstimatedBandwidthSafe(estimatedBandwidth);
}

WebRtc_Word32
ACMGenericCodec::SetEstimatedBandwidthSafe(
    WebRtc_Word32 /*estimatedBandwidth*/)
{
    // All codecs but iSAC will return -1
    return -1;
}

WebRtc_Word32
ACMGenericCodec::GetRedPayload(
    WebRtc_UWord8* redPayload,
    WebRtc_Word16* payloadBytes)
{
    WriteLockScoped wl(_codecWrapperLock);
    return GetRedPayloadSafe(redPayload, payloadBytes);
}

WebRtc_Word32
ACMGenericCodec::GetRedPayloadSafe(
    WebRtc_UWord8* /* redPayload   */,
    WebRtc_Word16* /* payloadBytes */)
{
    return -1; // Do nothing by default
}

WebRtc_Word16
ACMGenericCodec::CreateEncoder()
{
    WebRtc_Word16 status = 0;
    if(!_encoderExist)
    {
        status = InternalCreateEncoder();
        // We just created the codec and obviously it is not initialized
        _encoderInitialized = false;
    }

    if(status < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "CreateEncoder: error in internal create encoder");
        _encoderExist = false;
    }
    else
    {
        _encoderExist = true;
    }
    return status;
}

WebRtc_Word16
ACMGenericCodec::CreateDecoder()
{
    WebRtc_Word16 status = 0;
    if(!_decoderExist)
    {
        status = InternalCreateDecoder();
        // Decoder just created and obviously it is not initialized
        _decoderInitialized = false;
    }

    if(status < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "CreateDecoder: error in internal create decoder");
        _decoderExist = false;
    }
    else
    {
        _decoderExist = true;
    }
    return status;
}


void ACMGenericCodec::DestructEncoderInst(void* ptrInst)
{
    if(ptrInst != NULL)
    {
        WriteLockScoped lockCodec(_codecWrapperLock);
        ReadLockScoped lockNetEq(*_netEqDecodeLock);
        InternalDestructEncoderInst(ptrInst);
    }
}


WebRtc_Word16
ACMGenericCodec::AudioBuffer(
    WebRtcACMAudioBuff& audioBuff)
{
    ReadLockScoped cs(_codecWrapperLock);
    memcpy(audioBuff.inAudio, _inAudio,
        AUDIO_BUFFER_SIZE_W16 * sizeof(WebRtc_Word16));
    audioBuff.inAudioIxRead = _inAudioIxRead;
    audioBuff.inAudioIxWrite = _inAudioIxWrite;
    memcpy(audioBuff.inTimestamp, _inTimestamp,
        TIMESTAMP_BUFFER_SIZE_W32*sizeof(WebRtc_UWord32));
    audioBuff.inTimestampIxWrite = _inTimestampIxWrite;
    audioBuff.lastTimestamp = _lastTimestamp;
    return 0;
}


WebRtc_Word16
ACMGenericCodec::SetAudioBuffer(
    WebRtcACMAudioBuff& audioBuff)
{
    WriteLockScoped cs(_codecWrapperLock);
    memcpy(_inAudio, audioBuff.inAudio,
        AUDIO_BUFFER_SIZE_W16 * sizeof(WebRtc_Word16));
    _inAudioIxRead = audioBuff.inAudioIxRead;
    _inAudioIxWrite = audioBuff.inAudioIxWrite;
    memcpy(_inTimestamp, audioBuff.inTimestamp,
        TIMESTAMP_BUFFER_SIZE_W32*sizeof(WebRtc_UWord32));
    _inTimestampIxWrite = audioBuff.inTimestampIxWrite;
    _lastTimestamp = audioBuff.lastTimestamp;
    _isAudioBuffFresh = false;
    return 0;
}


WebRtc_UWord32
ACMGenericCodec::LastEncodedTimestamp() const
{
    ReadLockScoped cs(_codecWrapperLock);
    return _lastEncodedTimestamp;
}


WebRtc_UWord32
ACMGenericCodec::EarliestTimestamp() const
{
    ReadLockScoped cs(_codecWrapperLock);
    return _inTimestamp[0];
}


WebRtc_Word16
ACMGenericCodec::SetVAD(
    const bool       enableDTX,
    const bool       enableVAD,
    const ACMVADMode mode)
{
    WriteLockScoped cs(_codecWrapperLock);
    return SetVADSafe(enableDTX, enableVAD, mode);
}


WebRtc_Word16
ACMGenericCodec::SetVADSafe(
    const bool       enableDTX,
    const bool       enableVAD,
    const ACMVADMode mode)
{
    if(enableDTX)
    {
        // Make G729 AnnexB a special case
        if (!STR_CASE_CMP(_encoderParams.codecInstant.plname, "G729") && !_hasInternalDTX)
        {
            if (ACMGenericCodec::EnableDTX() < 0)
            {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                "SetVADSafe: error in enable DTX");
                return -1;
            }
        }
        else
        {
            if(EnableDTX() < 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                    "SetVADSafe: error in enable DTX");
                return -1;
            }
        }

        if(_hasInternalDTX)
        {
            // Codec has internal DTX, practically we don't need WebRtc VAD,
            // however, we let the user to turn it on if they need call-backs
            // on silence. Store VAD mode for future even if VAD is off.
            _vadMode = mode;
            return (enableVAD)? EnableVAD(mode):DisableVAD();
        }
        else
        {
            // Codec does not have internal DTX so enabling DTX requires an
            // active VAD. 'enableDTX == true' overwrites VAD status.
            if(EnableVAD(mode) < 0)
            {
                // If we cannot create VAD we have to disable DTX
                if(!_vadEnabled)
                {
                    DisableDTX();
                }
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                    "SetVADSafe: error in enable VAD");
                return -1;
            }

            // Return '1', to let the caller know VAD was turned on, even if the
            // function was called with VAD='false'
            if (enableVAD == false) {
                return 1;
            } else {
                return 0;
            }
        }
    }
    else
    {
        // Make G729 AnnexB a special case
        if (!STR_CASE_CMP(_encoderParams.codecInstant.plname, "G729") && !_hasInternalDTX)
        {
            ACMGenericCodec::DisableDTX();
        }
        else
        {
            DisableDTX();
        }
        return (enableVAD)? EnableVAD(mode):DisableVAD();
    }
}

WebRtc_Word16
ACMGenericCodec::EnableDTX()
{
    if(_hasInternalDTX)
    {
        // We should not be here if we have internal DTX
        // this function should be overwritten by the derived
        // class in this case
        return -1;
    }
    if(!_dtxEnabled)
    {
        if(WebRtcCng_CreateEnc(&_ptrDTXInst) < 0)
        {
            _ptrDTXInst = NULL;
            return -1;
        }
        WebRtc_UWord16 freqHz;
        EncoderSampFreq(freqHz);
        if(WebRtcCng_InitEnc(_ptrDTXInst, (WebRtc_Word16)freqHz,
            ACM_SID_INTERVAL_MSEC, _numLPCParams) < 0)
        {
            // Couldn't initialize, has to return -1, and free the memory
            WebRtcCng_FreeEnc(_ptrDTXInst);
            _ptrDTXInst = NULL;
            return -1;
        }
        _dtxEnabled = true;
    }
    return 0;
}

WebRtc_Word16
ACMGenericCodec::DisableDTX()
{
    if(_hasInternalDTX)
    {
        // We should not be here if we have internal DTX
        // this function should be overwritten by the derived
        // class in this case
        return -1;
    }
    if(_ptrDTXInst != NULL)
    {
        WebRtcCng_FreeEnc(_ptrDTXInst);
        _ptrDTXInst = NULL;
    }
    _dtxEnabled = false;
    return 0;
}

WebRtc_Word16
ACMGenericCodec::EnableVAD(
    ACMVADMode mode)
{
    if((mode < VADNormal) || (mode > VADVeryAggr))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "EnableVAD: error in VAD mode range");
        return -1;
    }

    if(!_vadEnabled)
    {
        if(WebRtcVad_Create(&_ptrVADInst) < 0)
        {
            _ptrVADInst = NULL;
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                "EnableVAD: error in create VAD");
            return -1;
        }
        if(WebRtcVad_Init(_ptrVADInst) < 0)
        {
            WebRtcVad_Free(_ptrVADInst);
            _ptrVADInst = NULL;
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                "EnableVAD: error in init VAD");
            return -1;
        }
    }

    // Set the vad mode to the given value
    if(WebRtcVad_set_mode(_ptrVADInst, mode) < 0)
    {
        // We failed to set the mode and we have to return -1. If
        // we already have a working VAD (_vadEnabled == true) then
        // we leave it to work. otherwise, the following will be
        // executed.
        if(!_vadEnabled)
        {
            // We just created the instance but cannot set the mode
            // we have to free the memomry.
            WebRtcVad_Free(_ptrVADInst);
            _ptrVADInst = NULL;
        }
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, _uniqueID,
            "EnableVAD: failed to set the VAD mode");
        return -1;
    }
    _vadMode = mode;
    _vadEnabled = true;
    return 0;
}

WebRtc_Word16
ACMGenericCodec::DisableVAD()
{
    if(_ptrVADInst != NULL)
    {
        WebRtcVad_Free(_ptrVADInst);
        _ptrVADInst = NULL;
    }
    _vadEnabled = false;
    return 0;
}

WebRtc_Word32
ACMGenericCodec::ReplaceInternalDTX(
    const bool replaceInternalDTX)
{
    WriteLockScoped cs(_codecWrapperLock);
    return ReplaceInternalDTXSafe(replaceInternalDTX);
}

WebRtc_Word32
ACMGenericCodec::ReplaceInternalDTXSafe(
    const bool /* replaceInternalDTX */)
{
    return -1;
}

WebRtc_Word32
ACMGenericCodec::IsInternalDTXReplaced(
    bool* internalDTXReplaced)
{
    WriteLockScoped cs(_codecWrapperLock);
    return IsInternalDTXReplacedSafe(internalDTXReplaced);
}

WebRtc_Word32
ACMGenericCodec::IsInternalDTXReplacedSafe(
    bool* internalDTXReplaced)
{
    *internalDTXReplaced = false;
    return 0;
}

WebRtc_Word16
ACMGenericCodec::ProcessFrameVADDTX(
    WebRtc_UWord8* bitStream,
    WebRtc_Word16* bitStreamLenByte,
    WebRtc_Word16* samplesProcessed)
{
    if(!_vadEnabled)
    {
        // VAD not enabled, set all vadLable[] to 1 (speech detected)
        for(WebRtc_Word16 n = 0; n < MAX_FRAME_SIZE_10MSEC; n++)
        {
            _vadLabel[n] = 1;
        }
        *samplesProcessed = 0;
        return 0;
    }
    WebRtc_UWord16 freqHz;
    EncoderSampFreq(freqHz);

    // Calculate number of samples in 10 ms blocks, and number ms in one frame
    WebRtc_Word16 samplesIn10Msec = (WebRtc_Word16)(freqHz / 100);
    WebRtc_Word32 frameLenMsec = (((WebRtc_Word32)_frameLenSmpl * 1000) / freqHz);
    WebRtc_Word16 status;

    // Vector for storing maximum 30 ms of mono audio at 32 kHz
    WebRtc_Word16 audio[960];

    // Calculate number of VAD-blocks to process, and number of samples in each block.
    int noSamplesToProcess[2];
    if (frameLenMsec == 40)
    {
        // 20 ms in each VAD block
        noSamplesToProcess[0] = noSamplesToProcess[1] = 2*samplesIn10Msec;
    }
    else
    {
        // For 10-30 ms framesizes, second VAD block will be size zero ms,
        // for 50 and 60 ms first VAD block will be 30 ms.
        noSamplesToProcess[0] = (frameLenMsec > 30)? 3*samplesIn10Msec : _frameLenSmpl;
        noSamplesToProcess[1] = _frameLenSmpl-noSamplesToProcess[0];
    }

    int offSet = 0;
    int loops = (noSamplesToProcess[1]>0) ? 2 : 1;
    for (int i=0; i<loops; i++) {
        // If stereo, calculate mean of the two channels
        if(_noChannels == 2) {
            for (int j=0; j<noSamplesToProcess[i]; j++) {
                audio[j] = (_inAudio[(offSet+j)*2]+_inAudio[(offSet+j)*2+1])/2;
        }
        offSet = noSamplesToProcess[0];
        } else {
            // Mono, copy data from _inAudio to continue work on
            memcpy(audio, _inAudio, sizeof(WebRtc_Word16)*noSamplesToProcess[i]);
        }

        // Call VAD
        status = WebRtcVad_Process(_ptrVADInst, (WebRtc_Word16)freqHz,
            audio, noSamplesToProcess[i]);

        _vadLabel[i] = status;

        if(status < 0)
        {
            // This will force that the data be removed from the buffer
            *samplesProcessed += noSamplesToProcess[i];
            return -1;
        }

        // If VAD decision non-active, update DTX. NOTE! We only do this if the first part of
        // a frame gets the VAD decision "inactive". Otherwise DTX might say it is time to
        // transmit SID frame, but we will encode the whole frame, because the first part is
        // active.
        *samplesProcessed = 0;
        if((status == 0) && (i==0) && _dtxEnabled && !_hasInternalDTX)
        {
            WebRtc_Word16 bitStreamLen;
            WebRtc_Word16 num10MsecFrames = noSamplesToProcess[i] / samplesIn10Msec;
            *bitStreamLenByte = 0;
            for(WebRtc_Word16 n = 0; n < num10MsecFrames; n++)
            {
                // This block is (passive) && (vad enabled)
                status = WebRtcCng_Encode(_ptrDTXInst, &audio[n*samplesIn10Msec],
                    samplesIn10Msec, bitStream, &bitStreamLen, 0);
                if (status < 0) {
                    return -1;
                }

                *samplesProcessed += samplesIn10Msec*_noChannels;

                // bitStreamLen will only be > 0 once per 100 ms
                *bitStreamLenByte += bitStreamLen;
            }


            // Check if all samples got processed by the DTX
            if(*samplesProcessed != noSamplesToProcess[i]*_noChannels) {
                // Set to zero since something went wrong. Shouldn't happen.
                *samplesProcessed = 0;
            }
        }

        if(*samplesProcessed > 0)
        {
            // The block contains inactive speech, and is processed by DTX.
            // Discontinue running VAD.
            break;
        }
    }

    return status;
}

WebRtc_Word16
ACMGenericCodec::SamplesLeftToEncode()
{
    ReadLockScoped rl(_codecWrapperLock);
    return (_frameLenSmpl <= _inAudioIxWrite)?
        0:(_frameLenSmpl - _inAudioIxWrite);
}

WebRtc_Word32
ACMGenericCodec::UnregisterFromNetEq(
    ACMNetEQ*     netEq,
    WebRtc_Word16 payloadType)
{
    WriteLockScoped wl(_codecWrapperLock);
    if(!_registeredInNetEq)
    {
        return 0;
    }
    if(UnregisterFromNetEqSafe(netEq, payloadType) < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "UnregisterFromNetEq: error, cannot unregister from NetEq");
        _registeredInNetEq = true;
        return -1;
    }
    else
    {
        _registeredInNetEq = false;
        return 0;
    }
}

void
ACMGenericCodec::SetUniqueID(
    const WebRtc_UWord32 id)
{
    _uniqueID = id;
}

bool
ACMGenericCodec::IsAudioBufferFresh() const
{
    ReadLockScoped rl(_codecWrapperLock);
    return _isAudioBuffFresh;
}

// This function is replaced by codec specific functions for some codecs
WebRtc_Word16
ACMGenericCodec::EncoderSampFreq(WebRtc_UWord16& sampFreqHz)
{
    WebRtc_Word32 f;
    f = ACMCodecDB::CodecFreq(_codecID);
    if(f < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                     "EncoderSampFreq: codec frequency is negative");
        return -1;
    }
    else
    {
        sampFreqHz = (WebRtc_UWord16)f;
        return 0;
    }
}


WebRtc_Word32
ACMGenericCodec::ConfigISACBandwidthEstimator(
    const WebRtc_UWord8  /* initFrameSizeMsec */,
    const WebRtc_UWord16 /* initRateBitPerSec */,
    const bool           /* enforceFrameSize  */)
{
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, _uniqueID,
        "The send-codec is not iSAC, failed to config iSAC bandwidth estimator.");
    return -1;
}

WebRtc_Word32
ACMGenericCodec::SetISACMaxRate(
    const WebRtc_UWord32 /* maxRateBitPerSec */)
{
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, _uniqueID,
        "The send-codec is not iSAC, failed to set iSAC max rate.");
    return -1;
}

WebRtc_Word32
ACMGenericCodec::SetISACMaxPayloadSize(
    const WebRtc_UWord16 /* maxPayloadLenBytes */)
{
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, _uniqueID,
        "The send-codec is not iSAC, failed to set iSAC max payload-size.");
    return -1;
}


void
ACMGenericCodec::SaveDecoderParam(
    const WebRtcACMCodecParams* codecParams)
{
    WriteLockScoped wl(_codecWrapperLock);
    SaveDecoderParamSafe(codecParams);
}


void
ACMGenericCodec::SaveDecoderParamSafe(
    const WebRtcACMCodecParams* codecParams)
{
    memcpy(&_decoderParams, codecParams, sizeof(WebRtcACMCodecParams));
}

WebRtc_Word16
ACMGenericCodec::UpdateEncoderSampFreq(
    WebRtc_UWord16 /* encoderSampFreqHz */)
{
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
        "It is asked for a change in smapling frequency while the \
current send-codec supports only one sampling rate.");
    return -1;
}


void
ACMGenericCodec::SetIsMaster(
    bool isMaster)
{
    WriteLockScoped wl(_codecWrapperLock);
    _isMaster = isMaster;
}



WebRtc_Word16
ACMGenericCodec::REDPayloadISAC(
        const WebRtc_Word32  /* isacRate        */,
        const WebRtc_Word16  /* isacBwEstimate  */,
        WebRtc_UWord8*       /* payload         */,
        WebRtc_Word16*       /* payloadLenBytes */)
{
   WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
       "Error: REDPayloadISAC is an iSAC specific function");
    return -1;
}

} // namespace webrtc
