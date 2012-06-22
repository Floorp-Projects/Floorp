/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "acm_codec_database.h"
#include "acm_common_defs.h"
#include "acm_dtmf_detection.h"
#include "acm_generic_codec.h"
#include "acm_resampler.h"
#include "audio_coding_module_impl.h"
#include "critical_section_wrapper.h"
#include "engine_configurations.h"
#include "rw_lock_wrapper.h"
#include "trace.h"

#include <assert.h>
#include <stdlib.h>

#ifdef ACM_QA_TEST
#   include <stdio.h>
#endif

#ifdef TIMED_LOGGING
    char message[500];
    #include "../test/timedtrace.h"
    #include <string.h>
    #define LOGWITHTIME(logString)                \
                sprintf(message, logString, _id); \
                _trace.TimedLogg(message);
#else
    #define LOGWITHTIME(logString)
#endif

namespace webrtc
{

enum {
    kACMToneEnd = 999
};

// Maximum number of bytes in one packet (PCM16B, 20 ms packets, stereo)
enum {
    kMaxPacketSize = 2560
};

AudioCodingModuleImpl::AudioCodingModuleImpl(
    const WebRtc_Word32 id):
    _packetizationCallback(NULL),
    _id(id),
    _lastTimestamp(0),
    _lastInTimestamp(0),
    _cng_nb_pltype(255),
    _cng_wb_pltype(255),
    _cng_swb_pltype(255),
    _red_pltype(255),
    _cng_reg_receiver(false),
    _vadEnabled(false),
    _dtxEnabled(false),
    _vadMode(VADNormal),
    _stereoReceiveRegistered(false),
    _stereoSend(false),
    _prev_received_channel(0),
    _expected_channels(1),
    _currentSendCodecIdx(-1),  // invalid value
    _current_receive_codec_idx(-1),  // invalid value
    _sendCodecRegistered(false),
    _acmCritSect(CriticalSectionWrapper::CreateCriticalSection()),
    _vadCallback(NULL),
    _lastRecvAudioCodecPlType(255),
    _isFirstRED(true),
    _fecEnabled(false),
    _fragmentation(NULL),
    _lastFECTimestamp(0),
    _receiveREDPayloadType(255),  // invalid value
    _previousPayloadType(255),
    _dummyRTPHeader(NULL),
    _recvPlFrameSizeSmpls(0),
    _receiverInitialized(false),
    _dtmfDetector(NULL),
    _dtmfCallback(NULL),
    _lastDetectedTone(kACMToneEnd),
    _callbackCritSect(CriticalSectionWrapper::CreateCriticalSection())
{
    _lastTimestamp = 0xD87F3F9F;
    _lastInTimestamp = 0xD87F3F9F;

    // Nullify send codec memory, set payload type and set codec name to
    // invalid values.
    memset(&_sendCodecInst, 0, sizeof(CodecInst));
    strncpy(_sendCodecInst.plname, "noCodecRegistered", 31);
    _sendCodecInst.pltype = -1;

    for (int i = 0; i < ACMCodecDB::kMaxNumCodecs; i++)
    {
        _codecs[i]            = NULL;
        _registeredPlTypes[i] = -1;
        _stereoReceive[i]     = false;
        _slaveCodecs[i]       = NULL;
        _mirrorCodecIdx[i]    = -1;
    }

    _netEq.SetUniqueId(_id);

    // Allocate memory for RED
    _redBuffer = new WebRtc_UWord8[MAX_PAYLOAD_SIZE_BYTE];
    _fragmentation = new RTPFragmentationHeader;
    _fragmentation->fragmentationVectorSize = 2;
    _fragmentation->fragmentationOffset = new WebRtc_UWord32[2];
    _fragmentation->fragmentationLength = new WebRtc_UWord32[2];
    _fragmentation->fragmentationTimeDiff = new WebRtc_UWord16[2];
    _fragmentation->fragmentationPlType = new WebRtc_UWord8[2];

    // Register the default payload type for RED and for
    // CNG for the three frequencies 8, 16 and 32 kHz
    for (int i = (ACMCodecDB::kNumCodecs - 1); i>=0; i--)
    {
        if((STR_CASE_CMP(ACMCodecDB::database_[i].plname, "red") == 0))
        {
          _red_pltype = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
        }
        else if ((STR_CASE_CMP(ACMCodecDB::database_[i].plname, "CN") == 0))
        {
            if (ACMCodecDB::database_[i].plfreq == 8000)
            {
              _cng_nb_pltype =
                  static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
            }
            else if (ACMCodecDB::database_[i].plfreq == 16000)
            {
                _cng_wb_pltype =
                    static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
            } else if (ACMCodecDB::database_[i].plfreq == 32000)
            {
                _cng_swb_pltype =
                    static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
            }
        }
    }

    if(InitializeReceiverSafe() < 0 )
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "Cannot initialize reciever");
    }
#ifdef TIMED_LOGGING
    _trace.SetUp("TimedLogg.txt");
#endif

#ifdef ACM_QA_TEST
    char fileName[500];
    sprintf(fileName, "ACM_QA_incomingPL_%03d_%d%d%d%d%d%d.dat",
        _id,
        rand() % 10,
        rand() % 10,
        rand() % 10,
        rand() % 10,
        rand() % 10,
        rand() % 10);

    _incomingPL = fopen(fileName, "wb");

    sprintf(fileName, "ACM_QA_outgoingPL_%03d_%d%d%d%d%d%d.dat",
        _id,
        rand() % 10,
        rand() % 10,
        rand() % 10,
        rand() % 10,
        rand() % 10,
        rand() % 10);
    _outgoingPL = fopen(fileName, "wb");
#endif

    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceAudioCoding, id, "Created");
}

AudioCodingModuleImpl::~AudioCodingModuleImpl()
{
    {
        CriticalSectionScoped lock(*_acmCritSect);
        _currentSendCodecIdx = -1;

        for (int i=0; i < ACMCodecDB::kMaxNumCodecs; i++)
        {
            if (_codecs[i] != NULL)
            {
                // True stereo codecs share the same memory for master and
                // slave, so slave codec need to be nullified here, since the
                // memory will be deleted.
                if(_slaveCodecs[i] == _codecs[i]) {
                    _slaveCodecs[i] = NULL;
                }

                // Mirror index holds the address of the codec memory.
                assert(_mirrorCodecIdx[i] > -1);
                if(_codecs[_mirrorCodecIdx[i]] != NULL)
                {
                    delete _codecs[_mirrorCodecIdx[i]];
                    _codecs[_mirrorCodecIdx[i]] = NULL;
                }

                _codecs[i] = NULL;
            }

            if(_slaveCodecs[i] != NULL)
            {
                // Delete memory for stereo usage of mono codecs.
                assert(_mirrorCodecIdx[i] > -1);
                if(_slaveCodecs[_mirrorCodecIdx[i]] != NULL)
                {
                    delete _slaveCodecs[_mirrorCodecIdx[i]];
                    _slaveCodecs[_mirrorCodecIdx[i]] =  NULL;
                }
                _slaveCodecs[i] = NULL;
            }
        }

        if(_dtmfDetector != NULL)
        {
            delete _dtmfDetector;
            _dtmfDetector = NULL;
        }
        if(_dummyRTPHeader != NULL)
        {
            delete _dummyRTPHeader;
            _dummyRTPHeader = NULL;
        }
        if(_redBuffer != NULL)
        {
            delete [] _redBuffer;
            _redBuffer = NULL;
        }
        if(_fragmentation != NULL)
        {
            // Only need to delete fragmentation header, it will clean
            // up it's own memory
            delete _fragmentation;
            _fragmentation = NULL;
        }
    }

#ifdef ACM_QA_TEST
        if(_incomingPL != NULL)
        {
            fclose(_incomingPL);
        }

        if(_outgoingPL != NULL)
        {
            fclose(_outgoingPL);
        }
#endif

    delete _callbackCritSect;
    _callbackCritSect = NULL;

    delete _acmCritSect;
    _acmCritSect = NULL;
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceAudioCoding, _id, "Destroyed");
}

WebRtc_Word32
AudioCodingModuleImpl::ChangeUniqueId(
    const WebRtc_Word32 id)
{
    {
        CriticalSectionScoped lock(*_acmCritSect);
        _id = id;
#ifdef ACM_QA_TEST
        if(_incomingPL != NULL)
        {
            fclose(_incomingPL);
        }

        if(_outgoingPL != NULL)
        {
            fclose(_outgoingPL);
        }

        char fileName[500];
        sprintf(fileName, "ACM_QA_incomingPL_%03d_%d%d%d%d%d%d.dat",
            _id,
            rand() % 10,
            rand() % 10,
            rand() % 10,
            rand() % 10,
            rand() % 10,
            rand() % 10);

        _incomingPL = fopen(fileName, "wb");

        sprintf(fileName, "ACM_QA_outgoingPL_%03d_%d%d%d%d%d%d.dat",
            _id,
            rand() % 10,
            rand() % 10,
            rand() % 10,
            rand() % 10,
            rand() % 10,
            rand() % 10);
        _outgoingPL = fopen(fileName, "wb");
#endif

        for (int i = 0; i < ACMCodecDB::kMaxNumCodecs; i++)
        {
            if(_codecs[i] != NULL)
            {
                _codecs[i]->SetUniqueID(id);
            }
        }
    }

    _netEq.SetUniqueId(_id);
    return 0;
}

// returns the number of milliseconds until the module want a
// worker thread to call Process
WebRtc_Word32
AudioCodingModuleImpl::TimeUntilNextProcess()
{
    CriticalSectionScoped lock(*_acmCritSect);

    if(!HaveValidEncoder("TimeUntilNextProcess"))
    {
        return -1;
    }
    return _codecs[_currentSendCodecIdx]->SamplesLeftToEncode() /
        (_sendCodecInst.plfreq / 1000);
}

// Process any pending tasks such as timeouts
WebRtc_Word32
AudioCodingModuleImpl::Process()
{
    WebRtc_UWord8 bitStream[2 * MAX_PAYLOAD_SIZE_BYTE]; // Make room for 1 RED payload
    WebRtc_Word16 lengthBytes = 2 * MAX_PAYLOAD_SIZE_BYTE;
    WebRtc_Word16 redLengthBytes = lengthBytes;
    WebRtc_UWord32 rtpTimestamp;
    WebRtc_Word16 status;
    WebRtcACMEncodingType encodingType;
    FrameType frameType = kAudioFrameSpeech;
    WebRtc_UWord8 currentPayloadType = 0;
    bool hasDataToSend = false;
    bool fecActive = false;

    // keep the scope of the ACM critical section limited
    {
        CriticalSectionScoped lock(*_acmCritSect);
        if(!HaveValidEncoder("Process"))
        {
            return -1;
        }

        status = _codecs[_currentSendCodecIdx]->Encode(bitStream, &lengthBytes,
                 &rtpTimestamp, &encodingType);
        if (status < 0) // Encode failed
        {
            // logging error
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "Process(): Encoding Failed");
            lengthBytes = 0;
            return -1;
        }
        else if(status == 0)
        {
            // Not enough data
            return 0;
        }
        else
        {
            switch(encodingType)
            {
            case kNoEncoding:
                {
                    currentPayloadType = _previousPayloadType;
                    frameType = kFrameEmpty;
                    lengthBytes = 0;
                    break;
                }
            case kActiveNormalEncoded:
            case kPassiveNormalEncoded:
                {
                    currentPayloadType = (WebRtc_UWord8)_sendCodecInst.pltype;
                    frameType = kAudioFrameSpeech;
                    break;
                }
            case kPassiveDTXNB:
                {
                    currentPayloadType =  _cng_nb_pltype;
                    frameType = kAudioFrameCN;
                    _isFirstRED = true;
                    break;
                }
            case kPassiveDTXWB:
                {
                    currentPayloadType =  _cng_wb_pltype;
                    frameType = kAudioFrameCN;
                    _isFirstRED = true;
                    break;
                }
            case kPassiveDTXSWB:
                {
                    currentPayloadType =  _cng_swb_pltype;
                    frameType = kAudioFrameCN;
                    _isFirstRED = true;
                    break;
                }
            }
            hasDataToSend = true;
            _previousPayloadType = currentPayloadType;

            // Redundancy encode is done here,
            // the two bitstreams packetized into
            // one RTP packet and the fragmentation points
            // are set.
            // Only apply RED on speech data.
            if((_fecEnabled) &&
                ((encodingType == kActiveNormalEncoded) ||
                 (encodingType == kPassiveNormalEncoded)))
            {
                // FEC is enabled within this scope.
                //
                // Note that, a special solution exists for iSAC since it is the only codec for
                // which getRedPayload has a non-empty implementation.
                //
                // Summary of the FEC scheme below (use iSAC as example):
                //
                //  1st (_firstRED is true) encoded iSAC frame (primary #1) =>
                //      - call getRedPayload() and store redundancy for packet #1 in second
                //        fragment of RED buffer (old data)
                //      - drop the primary iSAC frame
                //      - don't call SendData
                //  2nd (_firstRED is false) encoded iSAC frame (primary #2) =>
                //      - store primary #2 in 1st fragment of RED buffer and send the combined
                //        packet
                //      - the transmitted packet contains primary #2 (new) and reduncancy for
                //        packet #1 (old)
                //      - call getRedPayload() and store redundancy for packet #2 in second
                //        fragment of RED buffer
                //
                //  ...
                //
                //  Nth encoded iSAC frame (primary #N) =>
                //      - store primary #N in 1st fragment of RED buffer and send the combined
                //        packet
                //      - the transmitted packet contains primary #N (new) and reduncancy for
                //        packet #(N-1) (old)
                //      - call getRedPayload() and store redundancy for packet #N in second
                //        fragment of RED buffer
                //
                //  For all other codecs, getRedPayload does nothing and returns -1 =>
                //  redundant data is only a copy.
                //
                //  First combined packet contains : #2 (new) and #1 (old)
                //  Second combined packet contains: #3 (new) and #2 (old)
                //  Third combined packet contains : #4 (new) and #3 (old)
                //
                //  Hence, even if every second packet is dropped, perfect reconstruction is
                //  possible.
                fecActive = true;

                hasDataToSend = false;
                if(!_isFirstRED)    // skip this part for the first packet in a RED session
                {
                    // Rearrange bitStream such that FEC packets are included.
                    // Replace bitStream now that we have stored current bitStream.
                    memcpy(bitStream + _fragmentation->fragmentationOffset[1], _redBuffer,
                        _fragmentation->fragmentationLength[1]);
                    // Update the fragmentation time difference vector
                    WebRtc_UWord16 timeSinceLastTimestamp =
                            WebRtc_UWord16(rtpTimestamp - _lastFECTimestamp);

                    // Update fragmentation vectors
                    _fragmentation->fragmentationPlType[1] =
                            _fragmentation->fragmentationPlType[0];
                    _fragmentation->fragmentationTimeDiff[1] = timeSinceLastTimestamp;
                    hasDataToSend = true;
                }

                // Insert new packet length.
                _fragmentation->fragmentationLength[0] = lengthBytes;

                // Insert new packet payload type.
                _fragmentation->fragmentationPlType[0] = currentPayloadType;
                _lastFECTimestamp = rtpTimestamp;

                // can be modified by the GetRedPayload() call if iSAC is utilized
                redLengthBytes = lengthBytes;
                // A fragmentation header is provided => packetization according to RFC 2198
                // (RTP Payload for Redundant Audio Data) will be used.
                // First fragment is the current data (new).
                // Second fragment is the previous data (old).
                lengthBytes =
                    static_cast<WebRtc_Word16> (_fragmentation->fragmentationLength[0] +
                            _fragmentation->fragmentationLength[1]);

                // Get, and store, redundant data from the encoder based on the recently
                // encoded frame.
                // NOTE - only iSAC contains an implementation; all other codecs does nothing
                // and returns -1.
                if (_codecs[_currentSendCodecIdx]->GetRedPayload(_redBuffer,
                        &redLengthBytes) == -1)
                {
                    // The codec was not iSAC => use current encoder output as redundant data
                    // instead (trivial FEC scheme)
                    memcpy(_redBuffer, bitStream, redLengthBytes);
                }

                _isFirstRED = false;
                // Update payload type with RED payload type
                currentPayloadType = _red_pltype;
            }
        }
    }

    if(hasDataToSend)
    {
        CriticalSectionScoped lock(*_callbackCritSect);
#ifdef ACM_QA_TEST
        if(_outgoingPL != NULL)
        {
            fwrite(&rtpTimestamp,       sizeof(WebRtc_UWord32), 1, _outgoingPL);
            fwrite(&currentPayloadType, sizeof(WebRtc_UWord8),  1, _outgoingPL);
            fwrite(&lengthBytes,        sizeof(WebRtc_Word16),  1, _outgoingPL);
        }
#endif

        if(_packetizationCallback != NULL)
        {
            if (fecActive) {
                _packetizationCallback->SendData(frameType, currentPayloadType,
                    rtpTimestamp, bitStream, lengthBytes, _fragmentation);
            } else {
                _packetizationCallback->SendData(frameType, currentPayloadType,
                    rtpTimestamp, bitStream, lengthBytes, NULL);
            }
        }

        // This is for test
        if(_vadCallback != NULL)
        {
            _vadCallback->InFrameType(((WebRtc_Word16)encodingType));
        }
    }
    if (fecActive) {
        _fragmentation->fragmentationLength[1] = redLengthBytes;
    }
    return lengthBytes;
}




/////////////////////////////////////////
//   Sender
//

// Initialize send codec
WebRtc_Word32
AudioCodingModuleImpl::InitializeSender()
{
    CriticalSectionScoped lock(*_acmCritSect);

    _sendCodecRegistered = false;
    _currentSendCodecIdx = -1; // invalid value

    _sendCodecInst.plname[0] = '\0';

    for(int codecCntr = 0; codecCntr < ACMCodecDB::kMaxNumCodecs; codecCntr++)
    {
        if(_codecs[codecCntr] != NULL)
        {
            _codecs[codecCntr]->DestructEncoder();
        }
    }
    // Initialize FEC/RED
    _isFirstRED = true;
    if(_fecEnabled)
    {
        if(_redBuffer != NULL)
        {
            memset(_redBuffer, 0, MAX_PAYLOAD_SIZE_BYTE);
        }
        if(_fragmentation != NULL)
        {
            _fragmentation->fragmentationVectorSize = 2;
            _fragmentation->fragmentationOffset[0] = 0;
            _fragmentation->fragmentationOffset[0] = MAX_PAYLOAD_SIZE_BYTE;
            memset(_fragmentation->fragmentationLength, 0, sizeof(WebRtc_UWord32) * 2);
            memset(_fragmentation->fragmentationTimeDiff, 0, sizeof(WebRtc_UWord16) * 2);
            memset(_fragmentation->fragmentationPlType, 0, sizeof(WebRtc_UWord8) * 2);
        }
    }

    return 0;
}

WebRtc_Word32
AudioCodingModuleImpl::ResetEncoder()
{
    CriticalSectionScoped lock(*_acmCritSect);
    if(!HaveValidEncoder("ResetEncoder"))
    {
        return -1;
    }
    return _codecs[_currentSendCodecIdx]->ResetEncoder();
}

void
AudioCodingModuleImpl::UnregisterSendCodec()
{
    CriticalSectionScoped lock(*_acmCritSect);
    _sendCodecRegistered = false;
    _currentSendCodecIdx = -1;    // invalid value

    return;
}

ACMGenericCodec*
AudioCodingModuleImpl::CreateCodec(
    const CodecInst& codec)
{
    ACMGenericCodec* myCodec = NULL;

    myCodec = ACMCodecDB::CreateCodecInstance(&codec);
    if(myCodec == NULL)
    {
        // Error, could not create the codec

        // logging error
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                    "ACMCodecDB::CreateCodecInstance() failed in \
CreateCodec()");
        return myCodec;
    }
    myCodec->SetUniqueID(_id);
    myCodec->SetNetEqDecodeLock(_netEq.DecodeLock());

    return myCodec;
}

// can be called multiple times for Codec, CNG, RED
WebRtc_Word32
AudioCodingModuleImpl::RegisterSendCodec(
    const CodecInst& sendCodec)
{
    if((sendCodec.channels != 1) && (sendCodec.channels != 2))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "Registering Send codec failed due to wrong number of channels, %d. Only\
mono codecs are supported, i.e. channels=1.", sendCodec.channels);
        return -1;
    }

    char errMsg[500];
    int mirrorId;
    int codecID = ACMCodecDB::CodecNumber(&sendCodec, &mirrorId, errMsg,
                                          sizeof(errMsg));
    CriticalSectionScoped lock(*_acmCritSect);

    // Check for reported errors from function CodecNumber()
    if(codecID < 0)
    {
        if(!_sendCodecRegistered)
        {
            // This values has to be NULL if there is no codec registered
            _currentSendCodecIdx = -1;  // invalid value
        }
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id, errMsg);
        // Failed to register Send Codec
        return -1;
    }

    // telephone-event cannot be a send codec
    if(!STR_CASE_CMP(sendCodec.plname, "telephone-event"))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "telephone-event cannot be registered as send codec");
        return -1;
    }

    // RED can be registered with other payload type. If not registered a default
    // payload type is used.
    if(!STR_CASE_CMP(sendCodec.plname, "red"))
    {
        // TODO(tlegrand): Remove this check. Already taken care of in
        // ACMCodecDB::CodecNumber().
        // Check if the payload-type is valid
        if(!ACMCodecDB::ValidPayloadType(sendCodec.pltype))
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "Invalid payload-type %d for %s.",
                sendCodec.pltype, sendCodec.plname);
            return -1;
        }
        // Set RED payload type
        _red_pltype = static_cast<uint8_t>(sendCodec.pltype);
        return 0;
    }

    // CNG can be registered with other payload type. If not registered the
    // default payload types from codec database will be used.
    if(!STR_CASE_CMP(sendCodec.plname, "CN"))
    {
        // CNG is registered
        switch(sendCodec.plfreq)
        {
        case 8000:
            {
                _cng_nb_pltype = static_cast<uint8_t>(sendCodec.pltype);
                break;
            }
        case 16000:
            {
                _cng_wb_pltype = static_cast<uint8_t>(sendCodec.pltype);
                break;
            }
        case 32000:
            {
                _cng_swb_pltype = static_cast<uint8_t>(sendCodec.pltype);
                break;
            }
        default :
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                    "RegisterSendCodec() failed, invalid frequency for CNG registeration");
                return -1;
            }
        }

        return 0;
    }

    // TODO(tlegrand): Remove this check. Already taken care of in
    // ACMCodecDB::CodecNumber().
    // Check if the payload-type is valid
    if(!ACMCodecDB::ValidPayloadType(sendCodec.pltype))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "Invalid payload-type %d for %s.",
                sendCodec.pltype, sendCodec.plname);
        return -1;
    }

    // Check if codec supports the number of channels
    if(ACMCodecDB::codec_settings_[codecID].channel_support < sendCodec.channels)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "%d number of channels not supportedn for %s.",
                sendCodec.channels, sendCodec.plname);
        return -1;
    }

    // Set Stereo
    if (sendCodec.channels == 2)
    {
        _stereoSend = true;
    }

    // check if the codec is already registered as send codec
    bool oldCodecFamily;
    if(_sendCodecRegistered)
    {
        int sendCodecMirrorID;
        int sendCodecID =
                ACMCodecDB::CodecNumber(&_sendCodecInst, &sendCodecMirrorID);
        assert(sendCodecID >= 0);
        oldCodecFamily = (sendCodecID == codecID) || (mirrorId == sendCodecMirrorID);
    }
    else
    {
        oldCodecFamily = false;
    }

    // If new codec, register
    if (!oldCodecFamily)
    {
        if(_codecs[mirrorId] == NULL)
        {

            _codecs[mirrorId] = CreateCodec(sendCodec);
            if(_codecs[mirrorId] == NULL)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                    "Cannot Create the codec");
                return -1;
            }
            _mirrorCodecIdx[mirrorId] = mirrorId;
        }

        if(mirrorId != codecID)
        {
            _codecs[codecID] = _codecs[mirrorId];
            _mirrorCodecIdx[codecID] = mirrorId;
        }

        ACMGenericCodec* tmpCodecPtr = _codecs[codecID];
        WebRtc_Word16 status;
        WebRtcACMCodecParams codecParams;

        memcpy(&(codecParams.codecInstant), &sendCodec,
            sizeof(CodecInst));
        codecParams.enableVAD = _vadEnabled;
        codecParams.enableDTX = _dtxEnabled;
        codecParams.vadMode   = _vadMode;
        // force initialization
        status = tmpCodecPtr->InitEncoder(&codecParams, true);

        // Check if VAD was turned on, or if error is reported
        if (status == 1) {
            _vadEnabled = true;
        } else if (status < 0)
        {
            // could not initialize the encoder

            // Check if already have a registered codec
            // Depending on that different messages are logged
            if(!_sendCodecRegistered)
            {
                _currentSendCodecIdx = -1;     // invalid value
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                    "Cannot Initialize the encoder No Encoder is registered");
            }
            else
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                    "Cannot Initialize the encoder, continue encoding \
with the previously registered codec");
            }
            return -1;
        }

        // Everything is fine so we can replace the previous codec
        // with this one
        if(_sendCodecRegistered)
        {
            // If we change codec we start fresh with FEC.
            // This is not strictly required by the standard.
            _isFirstRED = true;

            if(tmpCodecPtr->SetVAD(_dtxEnabled, _vadEnabled, _vadMode) < 0){
                // SetVAD failed
                _vadEnabled = false;
                _dtxEnabled = false;
            }

        }

        _currentSendCodecIdx = codecID;
        _sendCodecRegistered = true;
        memcpy(&_sendCodecInst, &sendCodec, sizeof(CodecInst));
        _previousPayloadType = _sendCodecInst.pltype;
        return 0;
    }
    else
    {
        // If codec is the same as already registers check if any parameters
        // has changed compared to the current values.
        // If any parameter is valid then apply it and record.
        bool forceInit = false;

        if(mirrorId != codecID)
        {
            _codecs[codecID] = _codecs[mirrorId];
            _mirrorCodecIdx[codecID] = mirrorId;
        }

        // check the payload-type
        if(sendCodec.pltype != _sendCodecInst.pltype)
        {
            // At this point check if the given payload type is valid.
            // Record it later when the sampling frequency is changed
            // successfully.
            if(!ACMCodecDB::ValidPayloadType(sendCodec.pltype))
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                    "Out of range payload type");
                return -1;
            }

        }

        // If there is a codec that ONE instance of codec supports multiple
        // sampling frequencies, then we need to take care of it here.
        // one such a codec is iSAC. Both WB and SWB are encoded and decoded
        // with one iSAC instance. Therefore, we need to update the encoder
        // frequency if required.
        if(_sendCodecInst.plfreq != sendCodec.plfreq)
        {
            forceInit = true;

            // if sampling frequency is changed we have to start fresh with RED.
            _isFirstRED = true;
        }

        // If packet size or number of channels has changed, we need to
        // re-initialize the encoder.
        if(_sendCodecInst.pacsize != sendCodec.pacsize)
        {
            forceInit = true;
        }
        if(_sendCodecInst.channels != sendCodec.channels)
        {
            forceInit = true;
        }

        if(forceInit)
        {
            WebRtcACMCodecParams codecParams;

            memcpy(&(codecParams.codecInstant), &sendCodec,
                sizeof(CodecInst));
            codecParams.enableVAD = _vadEnabled;
            codecParams.enableDTX = _dtxEnabled;
            codecParams.vadMode   = _vadMode;

            // force initialization
            if(_codecs[_currentSendCodecIdx]->InitEncoder(&codecParams, true) < 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                    "Could not change the codec packet-size.");
                return -1;
            }

            _sendCodecInst.plfreq = sendCodec.plfreq;
            _sendCodecInst.pacsize = sendCodec.pacsize;
            _sendCodecInst.channels = sendCodec.channels;
        }

        // If the change of sampling frequency has been successful then
        // we store the payload-type.
        _sendCodecInst.pltype = sendCodec.pltype;

        // check if a change in Rate is required
        if(sendCodec.rate != _sendCodecInst.rate)
        {
            if(_codecs[codecID]->SetBitRate(sendCodec.rate) < 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                    "Could not change the codec rate.");
                return -1;
            }
            _sendCodecInst.rate = sendCodec.rate;
        }
        _previousPayloadType = _sendCodecInst.pltype;

        return 0;
    }
}

// get current send codec
WebRtc_Word32
AudioCodingModuleImpl::SendCodec(
    CodecInst& currentSendCodec) const
{
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, _id,
        "SendCodec()");
    CriticalSectionScoped lock(*_acmCritSect);

    if(!_sendCodecRegistered)
    {
        WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, _id,
            "SendCodec Failed, no codec is registered");

        return -1;
    }
    WebRtcACMCodecParams encoderParam;
    _codecs[_currentSendCodecIdx]->EncoderParams(&encoderParam);
    encoderParam.codecInstant.pltype = _sendCodecInst.pltype;
    memcpy(&currentSendCodec, &(encoderParam.codecInstant),
        sizeof(CodecInst));

    return 0;
}

// get current send freq
WebRtc_Word32
AudioCodingModuleImpl::SendFrequency() const
{
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, _id,
        "SendFrequency()");
    CriticalSectionScoped lock(*_acmCritSect);

    if(!_sendCodecRegistered)
    {
        WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, _id,
            "SendFrequency Failed, no codec is registered");

        return -1;
    }

    return _sendCodecInst.plfreq;
}

// Get encode bitrate
// Adaptive rate codecs return their current encode target rate, while other codecs
// return there longterm avarage or their fixed rate.
WebRtc_Word32
AudioCodingModuleImpl::SendBitrate() const
{
    CriticalSectionScoped lock(*_acmCritSect);

    if(!_sendCodecRegistered)
    {
        WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, _id,
            "SendBitrate Failed, no codec is registered");

        return -1;
    }

    WebRtcACMCodecParams encoderParam;
    _codecs[_currentSendCodecIdx]->EncoderParams(&encoderParam);

    return encoderParam.codecInstant.rate;
}

// set available bandwidth, inform the encoder about the estimated bandwidth
// received from the remote party
WebRtc_Word32
AudioCodingModuleImpl::SetReceivedEstimatedBandwidth(
    const WebRtc_Word32  bw )
{
    return _codecs[_currentSendCodecIdx]->SetEstimatedBandwidth(bw);
}

// register a transport callback wich will be called to deliver
// the encoded buffers
WebRtc_Word32
AudioCodingModuleImpl::RegisterTransportCallback(
    AudioPacketizationCallback* transport)
{
    CriticalSectionScoped lock(*_callbackCritSect);
    _packetizationCallback = transport;
    return 0;
}

// Used by the module to deliver messages to the codec module/appliation
// AVT(DTMF)
WebRtc_Word32
AudioCodingModuleImpl::RegisterIncomingMessagesCallback(
#ifndef WEBRTC_DTMF_DETECTION
    AudioCodingFeedback* /* incomingMessagesCallback */,
    const ACMCountries   /* cpt                      */)
{
    return -1;
#else
    AudioCodingFeedback* incomingMessagesCallback,
    const ACMCountries   cpt)
{
    WebRtc_Word16 status = 0;

    // Enter the critical section for callback
    {
        CriticalSectionScoped lock(*_callbackCritSect);
        _dtmfCallback = incomingMessagesCallback;
    }
    // enter the ACM critical section to set up the DTMF class.
    {
        CriticalSectionScoped lock(*_acmCritSect);
        // Check if the call is to disable or enable the callback
        if(incomingMessagesCallback == NULL)
        {
            // callback is disabled, delete DTMF-detector class
            if(_dtmfDetector != NULL)
            {
                delete _dtmfDetector;
                _dtmfDetector = NULL;
            }
            status = 0;
        }
        else
        {
            status = 0;
            if(_dtmfDetector == NULL)
            {
                _dtmfDetector = new(ACMDTMFDetection);
                if(_dtmfDetector == NULL)
                {
                    status = -1;
                }
            }
            if(status >= 0)
            {
                status = _dtmfDetector->Enable(cpt);
                if(status < 0)
                {
                    // failed to initialize if DTMF-detection was not enabled before,
                    // delete the class, and set the callback to NULL and return -1.
                    delete _dtmfDetector;
                    _dtmfDetector = NULL;
                }
            }
        }
    }
    // check if we failed in setting up the DTMF-detector class
    if((status < 0))
    {
        // we failed, we cannot have the callback
        CriticalSectionScoped lock(*_callbackCritSect);
        _dtmfCallback = NULL;
    }

    return status;
#endif
}


// Add 10MS of raw (PCM) audio data to the encoder
WebRtc_Word32
AudioCodingModuleImpl::Add10MsData(
    const AudioFrame& audioFrame)
{
    // Do we have a codec registered?
    CriticalSectionScoped lock(*_acmCritSect);
    if(!HaveValidEncoder("Add10MsData"))
    {
        return -1;
    }

    if(audioFrame._payloadDataLengthInSamples == 0)
    {
        assert(false);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "Cannot Add 10 ms audio, payload length is zero");
        return -1;
    }
    // Allow for 8, 16, 32 and 48kHz input audio
    if((audioFrame._frequencyInHz  != 8000)  &&
        (audioFrame._frequencyInHz != 16000) &&
        (audioFrame._frequencyInHz != 32000) &&
        (audioFrame._frequencyInHz != 48000))
    {
        assert(false);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "Cannot Add 10 ms audio, input frequency not valid");
        return -1;
    }


    // If the length and frequency matches. We currently just support raw PCM
    if((audioFrame._frequencyInHz/ 100) !=
        audioFrame._payloadDataLengthInSamples)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "Cannot Add 10 ms audio, input frequency and length doesn't \
match");
        return -1;
    }

    // Calculate the timestamp that should be pushed to codec.
    // This might be different from the timestamp of the frame
    // due to re-sampling
    bool resamplingRequired =
        ((WebRtc_Word32)audioFrame._frequencyInHz != _sendCodecInst.plfreq);

    // If number of channels in audio doesn't match codec mode, we need
    // either mono-to-stereo or stereo-to-mono conversion.
    WebRtc_Word16 audio[WEBRTC_10MS_PCM_AUDIO];
    int audio_channels = _sendCodecInst.channels;
    if (audioFrame._audioChannel != _sendCodecInst.channels) {
      if (_sendCodecInst.channels == 2) {
        // Do mono-to-stereo conversion by copying each sample.
        for (int k = 0; k < audioFrame._payloadDataLengthInSamples; k++) {
          audio[k * 2] = audioFrame._payloadData[k];
          audio[(k * 2) + 1] = audioFrame._payloadData[k];
        }
      } else if (_sendCodecInst.channels == 1) {
        // Do stereo-to-mono conversion by creating the average of the stereo
        // samples.
        for (int k = 0; k < audioFrame._payloadDataLengthInSamples; k++) {
          audio[k] = (audioFrame._payloadData[k * 2] +
              audioFrame._payloadData[(k * 2) + 1]) >> 1;
        }
      }
    } else {
      // Copy payload data for future use.
      size_t length = static_cast<size_t>(
          audioFrame._payloadDataLengthInSamples * audio_channels);
      memcpy(audio, audioFrame._payloadData, length * sizeof(WebRtc_UWord16));
    }

    WebRtc_UWord32 currentTimestamp;
    WebRtc_Word32 status;
    // if it is required, we have to do a resampling.
    if(resamplingRequired)
    {
        WebRtc_Word16 resampledAudio[WEBRTC_10MS_PCM_AUDIO];
        WebRtc_Word32 sendPlFreq = _sendCodecInst.plfreq;
        WebRtc_UWord32 diffInputTimestamp;
        WebRtc_Word16 newLengthSmpl;

        // calculate the timestamp of this frame
        if(_lastInTimestamp > audioFrame._timeStamp)
        {
            // a wrap around has happened
            diffInputTimestamp = ((WebRtc_UWord32)0xFFFFFFFF - _lastInTimestamp)
                + audioFrame._timeStamp;
        }
        else
        {
            diffInputTimestamp = audioFrame._timeStamp - _lastInTimestamp;
        }
        currentTimestamp = _lastTimestamp + (WebRtc_UWord32)(diffInputTimestamp *
            ((double)_sendCodecInst.plfreq / (double)audioFrame._frequencyInHz));

         newLengthSmpl = _inputResampler.Resample10Msec(
             audio, audioFrame._frequencyInHz, resampledAudio, sendPlFreq,
             audio_channels);

        if(newLengthSmpl < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "Cannot add 10 ms audio, resmapling failed");
            return -1;
        }
        status = _codecs[_currentSendCodecIdx]->Add10MsData(currentTimestamp,
            resampledAudio, newLengthSmpl, audio_channels);
    }
    else
    {
        currentTimestamp = audioFrame._timeStamp;

        status = _codecs[_currentSendCodecIdx]->Add10MsData(currentTimestamp,
            audio, audioFrame._payloadDataLengthInSamples,
            audio_channels);
    }
    _lastInTimestamp = audioFrame._timeStamp;
    _lastTimestamp = currentTimestamp;
    return status;
}

/////////////////////////////////////////
//   (FEC) Forward Error Correction
//

bool
AudioCodingModuleImpl::FECStatus() const
{
    CriticalSectionScoped lock(*_acmCritSect);
    return _fecEnabled;
}

// configure FEC status i.e on/off
WebRtc_Word32
AudioCodingModuleImpl::SetFECStatus(
#ifdef WEBRTC_CODEC_RED
    const bool enableFEC)
{
    CriticalSectionScoped lock(*_acmCritSect);

    if (_fecEnabled != enableFEC)
    {
        // Reset the RED buffer
        memset(_redBuffer, 0, MAX_PAYLOAD_SIZE_BYTE);

        // Reset fragmentation buffers
        _fragmentation->fragmentationVectorSize = 2;
        _fragmentation->fragmentationOffset[0] = 0;
        _fragmentation->fragmentationOffset[1] = MAX_PAYLOAD_SIZE_BYTE;
        memset(_fragmentation->fragmentationLength, 0, sizeof(WebRtc_UWord32) * 2);
        memset(_fragmentation->fragmentationTimeDiff, 0, sizeof(WebRtc_UWord16) * 2);
        memset(_fragmentation->fragmentationPlType, 0, sizeof(WebRtc_UWord8) * 2);

        // set _fecEnabled
        _fecEnabled = enableFEC;
    }
    _isFirstRED = true; // Make sure we restart FEC
    return 0;
#else
    const bool /* enableFEC */)
{
    _fecEnabled = false;
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, _id,
               "  WEBRTC_CODEC_RED is undefined => _fecEnabled = %d", _fecEnabled);
    return -1;
#endif
}


/////////////////////////////////////////
//   (VAD) Voice Activity Detection
//

WebRtc_Word32
AudioCodingModuleImpl::SetVAD(
    const bool       enableDTX,
    const bool       enableVAD,
    const ACMVADMode vadMode)
{
    CriticalSectionScoped lock(*_acmCritSect);

    // sanity check of the mode
    if((vadMode != VADNormal)      &&
       (vadMode != VADLowBitrate) &&
       (vadMode != VADAggr)       &&
       (vadMode != VADVeryAggr))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "Invalid VAD Mode %d, no change is made to VAD/DTX status",
            (int)vadMode);
        return -1;
    }

    // If a send codec is registered, set VAD/DTX for the codec
    if(HaveValidEncoder("SetVAD")) {
        WebRtc_Word16 status =
                _codecs[_currentSendCodecIdx]->SetVAD(enableDTX, enableVAD, vadMode);
        if(status == 1) {
            // Vad was enabled;
            _vadEnabled = true;
            _dtxEnabled = enableDTX;
            _vadMode = vadMode;

            return 0;
        } else if (status < 0) {
            // SetVAD failed
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "SetVAD failed");

            _vadEnabled = false;
            _dtxEnabled = false;

            return -1;
        }
    }

    _vadEnabled = enableVAD;
    _dtxEnabled = enableDTX;
    _vadMode = vadMode;

    return 0;
}

WebRtc_Word32
AudioCodingModuleImpl::VAD(
    bool&       dtxEnabled,
    bool&       vadEnabled,
    ACMVADMode& vadMode) const
{
    CriticalSectionScoped lock(*_acmCritSect);

    dtxEnabled = _dtxEnabled;
    vadEnabled = _vadEnabled;
    vadMode = _vadMode;

    return 0;
}

/////////////////////////////////////////
//   Receiver
//

WebRtc_Word32
AudioCodingModuleImpl::InitializeReceiver()
{
    CriticalSectionScoped lock(*_acmCritSect);
    return InitializeReceiverSafe();
}

// Initialize receiver, resets codec database etc
WebRtc_Word32
AudioCodingModuleImpl::InitializeReceiverSafe()
{
    // If the receiver is already initialized then we
    // also like to destruct decoders if any exist. After a call
    // to this function, we should have a clean start-up.
    if(_receiverInitialized)
    {
        for(int codecCntr = 0; codecCntr < ACMCodecDB::kNumCodecs; codecCntr++)
        {
            if(UnregisterReceiveCodecSafe(codecCntr) < 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                    "InitializeReceiver() failed, Could not unregister codec");
                return -1;
            }
        }
    }
    if (_netEq.Init() != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "InitializeReceiver() failed, Could not initialize NetEQ");
        return -1;
    }
    _netEq.SetUniqueId(_id);
    if (_netEq.AllocatePacketBuffer(ACMCodecDB::NetEQDecoders(),
        ACMCodecDB::kNumCodecs) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "NetEQ cannot allocatePacket Buffer");
        return -1;
    }

    // Register RED and CN
    int regInNeteq = 0;
    for (int i = (ACMCodecDB::kNumCodecs - 1); i>-1; i--) {
        if((STR_CASE_CMP(ACMCodecDB::database_[i].plname, "red") == 0)) {
            regInNeteq = 1;
        } else if ((STR_CASE_CMP(ACMCodecDB::database_[i].plname, "CN") == 0)) {
            regInNeteq = 1;
        }

        if (regInNeteq == 1) {
           if(RegisterRecCodecMSSafe(ACMCodecDB::database_[i], i, i,
                ACMNetEQ::masterJB) < 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                    "Cannot register master codec.");
                return -1;
            }
            _registeredPlTypes[i] = ACMCodecDB::database_[i].pltype;
            regInNeteq = 0;
        }
    }
    _cng_reg_receiver = true;

    _receiverInitialized = true;
    return 0;
}

// Reset the decoder state
WebRtc_Word32
AudioCodingModuleImpl::ResetDecoder()
{
    CriticalSectionScoped lock(*_acmCritSect);

    for(int codecCntr = 0; codecCntr < ACMCodecDB::kMaxNumCodecs; codecCntr++)
    {
        if((_codecs[codecCntr] != NULL) && (_registeredPlTypes[codecCntr] != -1))
        {
            if(_codecs[codecCntr]->ResetDecoder(_registeredPlTypes[codecCntr]) < 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                    "ResetDecoder failed:");
                return -1;
            }
        }
    }
    return _netEq.FlushBuffers();
}

// get current receive freq
WebRtc_Word32
AudioCodingModuleImpl::ReceiveFrequency() const
{
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, _id,
        "ReceiveFrequency()");
    WebRtcACMCodecParams codecParams;

    CriticalSectionScoped lock(*_acmCritSect);
    if(DecoderParamByPlType(_lastRecvAudioCodecPlType, codecParams) < 0)
    {
        return _netEq.CurrentSampFreqHz();
    }
    else
    {
        return codecParams.codecInstant.plfreq;
    }
}

// get current playout freq
WebRtc_Word32
AudioCodingModuleImpl::PlayoutFrequency() const
{
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, _id,
        "PlayoutFrequency()");

    CriticalSectionScoped lock(*_acmCritSect);

    return _netEq.CurrentSampFreqHz();
}


// register possible reveive codecs, can be called multiple times,
// for codecs, CNG (NB, WB and SWB), DTMF, RED
WebRtc_Word32
AudioCodingModuleImpl::RegisterReceiveCodec(
    const CodecInst& receiveCodec)
{
    CriticalSectionScoped lock(*_acmCritSect);

    if(receiveCodec.channels > 2)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "More than 2 audio channel is not supported.");
        return -1;
    }

    int mirrorId;
    int codecId = ACMCodecDB::ReceiverCodecNumber(&receiveCodec, &mirrorId);

    if(codecId < 0 || codecId >= ACMCodecDB::kNumCodecs)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "Wrong codec params to be registered as receive codec");
        return -1;
    }
    // Check if the payload-type is valid.
    if(!ACMCodecDB::ValidPayloadType(receiveCodec.pltype))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "Invalid payload-type %d for %s.",
                receiveCodec.pltype, receiveCodec.plname);
        return -1;
    }

    if(!_receiverInitialized)
    {
        if(InitializeReceiverSafe() < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "Cannot initialize reciver, so failed registering a codec.");
            return -1;
        }
    }

    // If codec already registered, unregister. Except for CN where we only
    // unregister if payload type is changing.
    if ((_registeredPlTypes[codecId] == receiveCodec.pltype) &&
        (STR_CASE_CMP(receiveCodec.plname, "CN") == 0)) {
      // Codec already registered as receiver with this payload type. Nothing
      // to be done.
      return 0;
    } else if (_registeredPlTypes[codecId] != -1) {
        if(UnregisterReceiveCodecSafe(codecId) < 0) {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "Cannot register master codec.");
            return -1;
        }
    }

    if(RegisterRecCodecMSSafe(receiveCodec, codecId, mirrorId,
        ACMNetEQ::masterJB) < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "Cannot register master codec.");
        return -1;
    }

    // If CN is being registered in master, and we have at least one stereo
    // codec registered in receiver, register CN in slave.
    if (STR_CASE_CMP(receiveCodec.plname, "CN") == 0) {
        _cng_reg_receiver = true;
        if (_stereoReceiveRegistered) {
            // At least one of the registered receivers is stereo, so go
            // a head and add CN to the slave.
            if(RegisterRecCodecMSSafe(receiveCodec, codecId, mirrorId,
                                      ACMNetEQ::slaveJB) < 0) {
               WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding,
                            _id, "Cannot register slave codec.");
               return -1;
            }
            _stereoReceive[codecId] = true;
            _registeredPlTypes[codecId] = receiveCodec.pltype;
            return 0;
        }
    }

    // If receive stereo, make sure we have two instances of NetEQ, one for each
    // channel. Mark CN and RED as stereo.
    if(receiveCodec.channels == 2)
    {
        if(_netEq.NumSlaves() < 1)
        {
            if(_netEq.AddSlave(ACMCodecDB::NetEQDecoders(),
                   ACMCodecDB::kNumCodecs) < 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding,
                             _id, "Cannot Add Slave jitter buffer to NetEQ.");
                return -1;
            }
        }

        // If this is the first time a stereo codec is registered, RED and CN
        // should be register in slave, if they are registered in master.
        if (!_stereoReceiveRegistered) {
            bool reg_in_neteq = false;
            for (int i = (ACMCodecDB::kNumCodecs - 1); i > -1; i--) {
                if (STR_CASE_CMP(ACMCodecDB::database_[i].plname, "RED") == 0) {
                    if (_registeredPlTypes[i] != -1) {
                        // Mark RED as stereo, since we have registered at least
                        // one stereo receive codec.
                        _stereoReceive[i] = true;
                        reg_in_neteq = true;
                    }
                } else if (STR_CASE_CMP(ACMCodecDB::database_[i].plname, "CN")
                    == 0) {
                    if (_cng_reg_receiver) {
                        // Mark CN as stereo, since we have registered at least
                        // one stereo receive codec.
                        _stereoReceive[i] = true;
                        reg_in_neteq = true;
                    }
                }

                if (reg_in_neteq) {
                    CodecInst tmp_codec;
                    memcpy(&tmp_codec, &ACMCodecDB::database_[i],
                           sizeof(CodecInst));
                    tmp_codec.pltype = _registeredPlTypes[i];
                    // Register RED of CN in slave, with the same payload type
                    // as in master.
                    if(RegisterRecCodecMSSafe(tmp_codec, i, i,
                                              ACMNetEQ::slaveJB) < 0) {
                        WEBRTC_TRACE(webrtc::kTraceError,
                                     webrtc::kTraceAudioCoding, _id,
                                     "Cannot register slave codec.");
                        return -1;
                    }
                    reg_in_neteq = false;
                }
            }
        }

        if(RegisterRecCodecMSSafe(receiveCodec, codecId, mirrorId,
            ACMNetEQ::slaveJB) < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "Cannot register slave codec.");
            return -1;
        }

        // Last received payload type equal the current one, but was marked
        // as mono. Reset to avoid problems.
        if((_stereoReceive[codecId] == false) &&
            (_lastRecvAudioCodecPlType == receiveCodec.pltype))
        {
            _lastRecvAudioCodecPlType = -1;
        }
        _stereoReceive[codecId] = true;
        _stereoReceiveRegistered = true;
    }
    else
    {
        _stereoReceive[codecId] = false;
    }

    _registeredPlTypes[codecId] = receiveCodec.pltype;

    if(!STR_CASE_CMP(receiveCodec.plname, "RED"))
    {
        _receiveREDPayloadType = receiveCodec.pltype;
    }
    return 0;
}



WebRtc_Word32
AudioCodingModuleImpl::RegisterRecCodecMSSafe(
    const CodecInst& receiveCodec,
    WebRtc_Word16    codecId,
    WebRtc_Word16    mirrorId,
    ACMNetEQ::JB     jitterBuffer)
{
    ACMGenericCodec** codecArray;
    if(jitterBuffer == ACMNetEQ::masterJB)
    {
        codecArray = &_codecs[0];
    }
    else if(jitterBuffer == ACMNetEQ::slaveJB)
    {
        codecArray = &_slaveCodecs[0];
        if (_codecs[codecId]->IsTrueStereoCodec()) {
          // True stereo codecs need to use the same codec memory
          // for both master and slave.
          _slaveCodecs[mirrorId] = _codecs[mirrorId];
          _mirrorCodecIdx[mirrorId] = mirrorId;
        }
    }
    else
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "RegisterReceiveCodecMSSafe failed, jitterBuffer is neither master or slave ");
        return -1;
    }

    if (codecArray[mirrorId] == NULL)
    {
        codecArray[mirrorId] = CreateCodec(receiveCodec);
        if(codecArray[mirrorId] == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "Cannot create codec to register as receive codec");
            return -1;
        }
        _mirrorCodecIdx[mirrorId] = mirrorId;
    }
    if(mirrorId != codecId)
    {
        codecArray[codecId] = codecArray[mirrorId];
        _mirrorCodecIdx[codecId] = mirrorId;
    }

    codecArray[codecId]->SetIsMaster(jitterBuffer == ACMNetEQ::masterJB);

    WebRtc_Word16 status = 0;
    bool registerInNetEq = true;
    WebRtcACMCodecParams codecParams;
    memcpy(&(codecParams.codecInstant), &receiveCodec,
        sizeof(CodecInst));
    codecParams.enableVAD = false;
    codecParams.enableDTX = false;
    codecParams.vadMode   = VADNormal;
    if (!codecArray[codecId]->DecoderInitialized())
    {
        // force initialization
        status = codecArray[codecId]->InitDecoder(&codecParams, true);
        if(status < 0)
        {
            // could not initialize the decoder we don't want to
            // continue if we could not initialize properly.
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "could not initialize the receive codec, codec not registered");

            return -1;
        }
    }
    else if(mirrorId != codecId)
    {
        // Currently this only happens for iSAC.
        // we have to store the decoder parameters

        codecArray[codecId]->SaveDecoderParam(&codecParams);
    }
    if (registerInNetEq)
    {
        if(codecArray[codecId]->RegisterInNetEq(&_netEq, receiveCodec)
            != 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "Receive codec could not be registered in NetEQ");

            return -1;
        }
        // Guaranty that the same payload-type that is
        // registered in NetEQ is stored in the codec.
        codecArray[codecId]->SaveDecoderParam(&codecParams);
    }

    return status;
}



// Get current received codec
WebRtc_Word32
AudioCodingModuleImpl::ReceiveCodec(
    CodecInst& currentReceiveCodec) const
{
    WebRtcACMCodecParams decoderParam;
    CriticalSectionScoped lock(*_acmCritSect);

    for(int decCntr = 0; decCntr < ACMCodecDB::kMaxNumCodecs; decCntr++)
    {
        if(_codecs[decCntr] != NULL)
        {
            if(_codecs[decCntr]->DecoderInitialized())
            {
                if(_codecs[decCntr]->DecoderParams(&decoderParam,
                    _lastRecvAudioCodecPlType))
                {
                    memcpy(&currentReceiveCodec, &decoderParam.codecInstant,
                        sizeof(CodecInst));
                    return 0;
                }
            }
        }
    }

    // if we are here then we haven't found any codec
    // set codec pltype to -1 to indicate that the structure
    // is invalid and return -1.
    currentReceiveCodec.pltype = -1;
    return -1;
}

// Incoming packet from network parsed and ready for decode
WebRtc_Word32
AudioCodingModuleImpl::IncomingPacket(
    const WebRtc_UWord8*   incomingPayload,
    const WebRtc_Word32    payloadLength,
    const WebRtcRTPHeader& rtpInfo)
{
    WebRtcRTPHeader rtp_header;

    memcpy(&rtp_header, &rtpInfo, sizeof(WebRtcRTPHeader));

    if (payloadLength < 0)
    {
        // Log error
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "IncomingPacket() Error, payload-length cannot be negative");
        return -1;
    }
    {
        // store the payload Type. this will be used to retrieve "received codec"
        // and "received frequency."
        CriticalSectionScoped lock(*_acmCritSect);
#ifdef ACM_QA_TEST
        if(_incomingPL != NULL)
        {
            fwrite(&rtpInfo.header.timestamp,   sizeof(WebRtc_UWord32), 1, _incomingPL);
            fwrite(&rtpInfo.header.payloadType, sizeof(WebRtc_UWord8),  1, _incomingPL);
            fwrite(&payloadLength,              sizeof(WebRtc_Word16),  1, _incomingPL);
        }
#endif

        WebRtc_UWord8 myPayloadType;

        // Check if this is an RED payload
        if(rtpInfo.header.payloadType == _receiveREDPayloadType)
        {
            // get the primary payload-type.
            myPayloadType = incomingPayload[0] & 0x7F;
        }
        else
        {
            myPayloadType = rtpInfo.header.payloadType;
        }

        // If payload is audio, check if received payload is different from
        // previous.
        if(!rtpInfo.type.Audio.isCNG)
        {
            // This is Audio not CNG

            if(myPayloadType != _lastRecvAudioCodecPlType)
            {
                // We detect a change in payload type. It is necessary for iSAC
                // we are going to use ONE iSAC instance for decoding both WB and
                // SWB payloads. If payload is changed there might be a need to reset
                // sampling rate of decoder. depending what we have received "now".
                for(int i = 0; i < ACMCodecDB::kMaxNumCodecs; i++)
                {
                    if(_registeredPlTypes[i] == myPayloadType)
                    {
                        if(_codecs[i] == NULL)
                        {
                            // we found a payload type but the corresponding
                            // codec is NULL this should not happen
                            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                                "IncomingPacket() Error, payload type found but corresponding "
                                "codec is NULL");
                            return -1;
                        }
                        _codecs[i]->UpdateDecoderSampFreq(i);
                        _netEq.SetReceivedStereo(_stereoReceive[i]);
                        _current_receive_codec_idx = i;

                        // If we have a change in expected number of channels,
                        // flush packet buffers in NetEQ.
                        if ((_stereoReceive[i] && (_expected_channels == 1)) ||
                            (!_stereoReceive[i] && (_expected_channels == 2))) {
                          _netEq.FlushBuffers();
                          _codecs[i]->ResetDecoder(myPayloadType);
                        }

                        // Store number of channels we expect to receive for the
                        // current payload type.
                        if (_stereoReceive[i]) {
                          _expected_channels = 2;
                        } else {
                          _expected_channels = 1;
                        }

                        // Reset previous received channel
                        _prev_received_channel = 0;

                        break;
                    }
                }
            }
            _lastRecvAudioCodecPlType = myPayloadType;
        }
    }

    // Split the payload for stereo packets, so that first half of payload
    // vector holds left channel, and second half holds right channel.
    if (_expected_channels == 2) {
      // Create a new vector for the payload, maximum payload size.
      WebRtc_Word32 length = payloadLength;
      WebRtc_UWord8 payload[kMaxPacketSize];
      assert(payloadLength <= kMaxPacketSize);
      memcpy(payload, incomingPayload, payloadLength);
      _codecs[_current_receive_codec_idx]->SplitStereoPacket(payload, &length);
      rtp_header.type.Audio.channel = 2;
      // Insert packet into NetEQ.
      return _netEq.RecIn(payload, length, rtp_header);
    } else {
      return _netEq.RecIn(incomingPayload, payloadLength, rtp_header);
    }
}

// Minimum playout delay (Used for lip-sync)
WebRtc_Word32
AudioCodingModuleImpl::SetMinimumPlayoutDelay(
    const WebRtc_Word32 timeMs)
{
    if((timeMs < 0) || (timeMs > 1000))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "Delay must be in the range of 0-1000 milliseconds.");
        return -1;
    }
    return _netEq.SetExtraDelay(timeMs);
}

// Get Dtmf playout status
bool
AudioCodingModuleImpl::DtmfPlayoutStatus() const
{
#ifndef WEBRTC_CODEC_AVT
    return false;
#else
    return _netEq.AVTPlayout();
#endif
}

// configure Dtmf playout status i.e on/off
// playout the incoming outband Dtmf tone
WebRtc_Word32
AudioCodingModuleImpl::SetDtmfPlayoutStatus(
#ifndef WEBRTC_CODEC_AVT
    const bool /* enable */)
{
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, _id,
        "SetDtmfPlayoutStatus() failed: AVT is not supported.");
    return -1;
#else
    const bool enable)
{
    return _netEq.SetAVTPlayout(enable);
#endif
}

// Estimate the Bandwidth based on the incoming stream
// This is also done in the RTP module
// need this for one way audio where the RTCP send the BW estimate
WebRtc_Word32
AudioCodingModuleImpl::DecoderEstimatedBandwidth() const
{
    CodecInst codecInst;
    WebRtc_Word16 codecID = -1;
    int plTypWB;
    int plTypSWB;

    // Get iSAC settings
    for(int codecCntr = 0; codecCntr < ACMCodecDB::kNumCodecs; codecCntr++)
    {
        // Store codec settings for codec number "codeCntr" in the output struct
        ACMCodecDB::Codec(codecCntr, &codecInst);

        if(!STR_CASE_CMP(codecInst.plname, "isac"))
        {
            codecID = 1;
            plTypWB = codecInst.pltype;

            ACMCodecDB::Codec(codecCntr+1, &codecInst);
            plTypSWB = codecInst.pltype;

            break;
        }
    }

    if(codecID < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "DecoderEstimatedBandwidth failed");
        return -1;
    }

    if ((_lastRecvAudioCodecPlType == plTypWB) || (_lastRecvAudioCodecPlType == plTypSWB))
    {
        return _codecs[codecID]->GetEstimatedBandwidth();
    } else {
        return -1;
    }
}

// Set playout mode for: voice, fax, or streaming
WebRtc_Word32
AudioCodingModuleImpl::SetPlayoutMode(
    const AudioPlayoutMode mode)
{
    if((mode  != voice) &&
        (mode != fax)   &&
        (mode != streaming))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "Invalid playout mode.");
        return -1;
    }
    return _netEq.SetPlayoutMode(mode);
}

// Get playout mode voice, fax
AudioPlayoutMode
AudioCodingModuleImpl::PlayoutMode() const
{
    return _netEq.PlayoutMode();
}


// Get 10 milliseconds of raw audio data to play out
// automatic resample to the requested frequency
WebRtc_Word32
AudioCodingModuleImpl::PlayoutData10Ms(
    const WebRtc_Word32 desiredFreqHz,
    AudioFrame&         audioFrame)
{
    bool stereoMode;

     // recOut always returns 10 ms
    if (_netEq.RecOut(_audioFrame) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "PlayoutData failed, RecOut Failed");
        return -1;
    }

    audioFrame._audioChannel = _audioFrame._audioChannel;
    audioFrame._vadActivity  = _audioFrame._vadActivity;
    audioFrame._speechType   = _audioFrame._speechType;

    stereoMode =  (_audioFrame._audioChannel > 1);
    //For stereo playout:
    // Master and Slave samples are interleaved starting with Master

    const WebRtc_UWord16 recvFreq = static_cast<WebRtc_UWord16>(_audioFrame._frequencyInHz);
    bool toneDetected = false;
    WebRtc_Word16 lastDetectedTone;
    WebRtc_Word16 tone;

     // limit the scope of ACM Critical section
    // perhaps we don't need to have output resampler in
    // critical section, it is supposed to be called in this
    // function and no where else. However, it won't degrade complexity
    {
        CriticalSectionScoped lock(*_acmCritSect);

        if ((recvFreq != desiredFreqHz) && (desiredFreqHz != -1))
        {
            // resample payloadData
            WebRtc_Word16 tmpLen = _outputResampler.Resample10Msec(
                _audioFrame._payloadData, recvFreq, audioFrame._payloadData, desiredFreqHz,
                _audioFrame._audioChannel);

            if(tmpLen < 0)
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                    "PlayoutData failed, resampler failed");
                return -1;
            }

            //Set the payload data length from the resampler
            audioFrame._payloadDataLengthInSamples = (WebRtc_UWord16)tmpLen;
            // set the ssampling frequency
            audioFrame._frequencyInHz = desiredFreqHz;
        }
        else
        {
            memcpy(audioFrame._payloadData, _audioFrame._payloadData,
              _audioFrame._payloadDataLengthInSamples * audioFrame._audioChannel
              * sizeof(WebRtc_Word16));
            // set the payload length
            audioFrame._payloadDataLengthInSamples = _audioFrame._payloadDataLengthInSamples;
            // set the sampling frequency
            audioFrame._frequencyInHz = recvFreq;
        }

        //Tone detection done for master channel
        if(_dtmfDetector != NULL)
        {
            // Dtmf Detection
            if(audioFrame._frequencyInHz == 8000)
            {
                // use audioFrame._payloadData then Dtmf detector doesn't
                // need resampling
                if(!stereoMode)
                {
                    _dtmfDetector->Detect(audioFrame._payloadData,
                        audioFrame._payloadDataLengthInSamples,
                        audioFrame._frequencyInHz, toneDetected, tone);
                }
                else
                {
                    // we are in 8 kHz so the master channel needs only 80 samples
                    WebRtc_Word16 masterChannel[80];
                    for(int n = 0; n < 80; n++)
                    {
                        masterChannel[n] = audioFrame._payloadData[n<<1];
                    }
                    _dtmfDetector->Detect(masterChannel,
                        audioFrame._payloadDataLengthInSamples,
                        audioFrame._frequencyInHz, toneDetected, tone);
                }
            }
            else
            {
                // Do the detection on the audio that we got from NetEQ (_audioFrame).
                if(!stereoMode)
                {
                    _dtmfDetector->Detect(_audioFrame._payloadData,
                        _audioFrame._payloadDataLengthInSamples, recvFreq,
                        toneDetected, tone);
                }
                else
                {
                    WebRtc_Word16 masterChannel[WEBRTC_10MS_PCM_AUDIO];
                    for(int n = 0; n < _audioFrame._payloadDataLengthInSamples; n++)
                    {
                        masterChannel[n] = _audioFrame._payloadData[n<<1];
                    }
                    _dtmfDetector->Detect(masterChannel,
                        _audioFrame._payloadDataLengthInSamples, recvFreq,
                        toneDetected, tone);
                }
            }
        }

        // we want to do this while we are in _acmCritSect
        // doesn't really need to initialize the following
        // variable but Linux complains if we don't
        lastDetectedTone = kACMToneEnd;
        if(toneDetected)
        {
            lastDetectedTone = _lastDetectedTone;
            _lastDetectedTone = tone;
        }
    }

    if(toneDetected)
    {
        // we will deal with callback here, so enter callback critical
        // section
        CriticalSectionScoped lock(*_callbackCritSect);

        if(_dtmfCallback != NULL)
        {
            if(tone != kACMToneEnd)
            {
                // just a tone
                _dtmfCallback->IncomingDtmf((WebRtc_UWord8)tone, false);
            }
            else if((tone == kACMToneEnd) &&
                (lastDetectedTone != kACMToneEnd))
            {
                // The tone is "END" and the previously detected tone is
                // not "END," so call fir an end.
                _dtmfCallback->IncomingDtmf((WebRtc_UWord8)lastDetectedTone,
                    true);
            }
        }
    }

    audioFrame._id = _id;
    audioFrame._volume = -1;
    audioFrame._energy = -1;
    audioFrame._timeStamp = 0;

    return 0;
}

/////////////////////////////////////////
//   (CNG) Comfort Noise Generation
//   Generate comfort noise when receiving DTX packets
//

// Get VAD aggressiveness on the incoming stream
ACMVADMode
AudioCodingModuleImpl::ReceiveVADMode() const
{
    return _netEq.VADMode();
}

// Configure VAD aggressiveness on the incoming stream
WebRtc_Word16
AudioCodingModuleImpl::SetReceiveVADMode(
    const ACMVADMode mode)
{
    return _netEq.SetVADMode(mode);
}

/////////////////////////////////////////
//   statistics
//

WebRtc_Word32
AudioCodingModuleImpl::NetworkStatistics(
    ACMNetworkStatistics& statistics) const
{
    WebRtc_Word32 status;
    status = _netEq.NetworkStatistics(&statistics);
    return status;
}

void
AudioCodingModuleImpl::DestructEncoderInst(
    void* ptrInst)
{
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, _id,
        "DestructEncoderInst()");
    if(!HaveValidEncoder("DestructEncoderInst"))
    {
        return;
    }

    _codecs[_currentSendCodecIdx]->DestructEncoderInst(ptrInst);
}

WebRtc_Word16
AudioCodingModuleImpl::AudioBuffer(
    WebRtcACMAudioBuff& audioBuff)
{
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, _id,
        "AudioBuffer()");
    if(!HaveValidEncoder("AudioBuffer"))
    {
        return -1;
    }

    audioBuff.lastInTimestamp = _lastInTimestamp;
    return _codecs[_currentSendCodecIdx]->AudioBuffer(audioBuff);
}

WebRtc_Word16
AudioCodingModuleImpl::SetAudioBuffer(
    WebRtcACMAudioBuff& audioBuff)
{
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, _id,
        "SetAudioBuffer()");
    if(!HaveValidEncoder("SetAudioBuffer"))
    {
        return -1;
    }

    return _codecs[_currentSendCodecIdx]->SetAudioBuffer(audioBuff);
}


WebRtc_UWord32
AudioCodingModuleImpl::EarliestTimestamp() const
{
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, _id,
        "EarliestTimestamp()");
    if(!HaveValidEncoder("EarliestTimestamp"))
    {
        return -1;
    }

    return _codecs[_currentSendCodecIdx]->EarliestTimestamp();
}

WebRtc_Word32
AudioCodingModuleImpl::RegisterVADCallback(
    ACMVADCallback* vadCallback)
{
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, _id,
        "RegisterVADCallback()");
    CriticalSectionScoped lock(*_callbackCritSect);
    _vadCallback = vadCallback;
    return 0;
}

// TODO(tlegrand): Modify this function to work for stereo, and add tests.
WebRtc_Word32
AudioCodingModuleImpl::IncomingPayload(
    const WebRtc_UWord8* incomingPayload,
    const WebRtc_Word32  payloadLength,
    const WebRtc_UWord8  payloadType,
    const WebRtc_UWord32 timestamp)
{
    if (payloadLength < 0)
    {
        // Log error in trace file.
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "IncomingPacket() Error, payload-length cannot be negative");
        return -1;
    }

    if(_dummyRTPHeader == NULL)
    {
        // This is the first time that we are using _dummyRTPHeader
        // so we have to create it.
        WebRtcACMCodecParams codecParams;
        _dummyRTPHeader = new WebRtcRTPHeader;
        if (_dummyRTPHeader == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "IncomingPacket() Error, out of memory");
            return -1;
        }
        _dummyRTPHeader->header.payloadType = payloadType;
        // Don't matter in this case
        _dummyRTPHeader->header.ssrc = 0;
        _dummyRTPHeader->header.markerBit = false;
        // start with random numbers
        _dummyRTPHeader->header.sequenceNumber = rand();
        _dummyRTPHeader->header.timestamp = (((WebRtc_UWord32)rand()) << 16) +
            (WebRtc_UWord32)rand();
        _dummyRTPHeader->type.Audio.channel = 1;

        if(DecoderParamByPlType(payloadType, codecParams) < 0)
        {
            // we didn't find a codec with the given payload.
            // something is wrong we exit, but we delete _dummyRTPHeader
            // and set it to NULL to start clean next time
            delete _dummyRTPHeader;
            _dummyRTPHeader = NULL;
            return -1;
        }
        _recvPlFrameSizeSmpls = codecParams.codecInstant.pacsize;
    }

    if(payloadType != _dummyRTPHeader->header.payloadType)
    {
        // payload type has changed since the last time we might need to
        // update the frame-size
        WebRtcACMCodecParams codecParams;
        if(DecoderParamByPlType(payloadType, codecParams) < 0)
        {
            // we didn't find a codec with the given payload.
            // something is wrong we exit
            return -1;
        }
        _recvPlFrameSizeSmpls = codecParams.codecInstant.pacsize;
        _dummyRTPHeader->header.payloadType = payloadType;
    }

    if(timestamp > 0)
    {
        _dummyRTPHeader->header.timestamp = timestamp;
    }

    // store the payload Type. this will be used to retrieve "received codec"
    // and "received frequency."
    _lastRecvAudioCodecPlType = payloadType;

    // Insert in NetEQ
    if(_netEq.RecIn(incomingPayload, payloadLength, (*_dummyRTPHeader)) < 0)
    {
        return -1;
    }

    // get ready for the next payload
    _dummyRTPHeader->header.sequenceNumber++;
    _dummyRTPHeader->header.timestamp += _recvPlFrameSizeSmpls;
    return 0;
}

WebRtc_Word16
AudioCodingModuleImpl::DecoderParamByPlType(
    const WebRtc_UWord8    payloadType,
    WebRtcACMCodecParams&  codecParams) const
{
    CriticalSectionScoped lock(*_acmCritSect);
    for(WebRtc_Word16 codecCntr = 0; codecCntr < ACMCodecDB::kMaxNumCodecs; codecCntr++)
    {
        if(_codecs[codecCntr] != NULL)
        {
            if(_codecs[codecCntr]->DecoderInitialized())
            {
                if(_codecs[codecCntr]->DecoderParams(&codecParams,
                    payloadType))
                {
                    return 0;
                }
            }
        }
    }
    // if we are here it means that we could not find a
    // codec with that payload type. reset the values to
    // not acceptable values and return -1;
    codecParams.codecInstant.plname[0] = '\0';
    codecParams.codecInstant.pacsize   = 0;
    codecParams.codecInstant.rate      = 0;
    codecParams.codecInstant.pltype    = -1;
    return -1;
}



WebRtc_Word16
AudioCodingModuleImpl::DecoderListIDByPlName(
    const char*  payloadName,
    const WebRtc_UWord16 sampFreqHz) const
{
    WebRtcACMCodecParams codecParams;
    CriticalSectionScoped lock(*_acmCritSect);
    for(WebRtc_Word16 codecCntr = 0; codecCntr < ACMCodecDB::kMaxNumCodecs; codecCntr++)
    {
        if((_codecs[codecCntr] != NULL))
        {
            if(_codecs[codecCntr]->DecoderInitialized())
            {
                assert(_registeredPlTypes[codecCntr] >= 0);
                assert(_registeredPlTypes[codecCntr] <= 255);
                _codecs[codecCntr]->DecoderParams(&codecParams,
                    (WebRtc_UWord8)_registeredPlTypes[codecCntr]);
                if(!STR_CASE_CMP(codecParams.codecInstant.plname, payloadName))
                {
                    // Check if the given sampling frequency matches.
                    // A zero sampling frequency means we matching the names
                    // is sufficient and we don't need to check for the
                    // frequencies.
                    // Currently it is only iSAC which has one name but two
                    // sampling frequencies.
                    if((sampFreqHz == 0) ||
                        (codecParams.codecInstant.plfreq == sampFreqHz))
                    {
                        return codecCntr;
                    }
                }
            }
        }
    }
    // if we are here it means that we could not find a
    // codec with that payload type. return -1;
    return -1;
}

WebRtc_Word32
AudioCodingModuleImpl::LastEncodedTimestamp(WebRtc_UWord32& timestamp) const
{
    CriticalSectionScoped lock(*_acmCritSect);
    if(!HaveValidEncoder("LastEncodedTimestamp"))
    {
        return -1;
    }
    timestamp = _codecs[_currentSendCodecIdx]->LastEncodedTimestamp();
    return 0;
}

WebRtc_Word32
AudioCodingModuleImpl::ReplaceInternalDTXWithWebRtc(bool useWebRtcDTX)
{
    CriticalSectionScoped lock(*_acmCritSect);

    if(!HaveValidEncoder("ReplaceInternalDTXWithWebRtc"))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "Cannot replace codec internal DTX when no send codec is registered.");
        return -1;
    }

    WebRtc_Word32 res = _codecs[_currentSendCodecIdx]->ReplaceInternalDTX(useWebRtcDTX);
    // Check if VAD is turned on, or if there is any error
    if(res == 1)
    {
        _vadEnabled = true;
    } else if(res < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "Failed to set ReplaceInternalDTXWithWebRtc(%d)", useWebRtcDTX);
        return res;
    }

    return 0;
}

WebRtc_Word32
AudioCodingModuleImpl::IsInternalDTXReplacedWithWebRtc(bool& usesWebRtcDTX)
{
    CriticalSectionScoped lock(*_acmCritSect);

    if(!HaveValidEncoder("IsInternalDTXReplacedWithWebRtc"))
    {
        return -1;
    }
    if(_codecs[_currentSendCodecIdx]->IsInternalDTXReplaced(&usesWebRtcDTX) < 0)
    {
        return -1;
    }
    return 0;
}


WebRtc_Word32
AudioCodingModuleImpl::SetISACMaxRate(
    const WebRtc_UWord32 maxRateBitPerSec)
{
    CriticalSectionScoped lock(*_acmCritSect);

    if(!HaveValidEncoder("SetISACMaxRate"))
    {
        return -1;
    }

    return _codecs[_currentSendCodecIdx]->SetISACMaxRate(maxRateBitPerSec);
}


WebRtc_Word32
AudioCodingModuleImpl::SetISACMaxPayloadSize(
    const WebRtc_UWord16 maxPayloadLenBytes)
{
    CriticalSectionScoped lock(*_acmCritSect);

    if(!HaveValidEncoder("SetISACMaxPayloadSize"))
    {
        return -1;
    }

    return _codecs[_currentSendCodecIdx]->SetISACMaxPayloadSize(maxPayloadLenBytes);
}

WebRtc_Word32
AudioCodingModuleImpl::ConfigISACBandwidthEstimator(
    const WebRtc_UWord8  initFrameSizeMsec,
    const WebRtc_UWord16 initRateBitPerSec,
    const bool           enforceFrameSize)
{
    CriticalSectionScoped lock(*_acmCritSect);

    if(!HaveValidEncoder("ConfigISACBandwidthEstimator"))
    {
        return -1;
    }

    return _codecs[_currentSendCodecIdx]->ConfigISACBandwidthEstimator(
        initFrameSizeMsec, initRateBitPerSec, enforceFrameSize);
}

WebRtc_Word32
AudioCodingModuleImpl::SetBackgroundNoiseMode(
    const ACMBackgroundNoiseMode mode)
{
    if((mode < On) ||
        (mode > Off))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "The specified background noise is out of range.\n");
        return -1;
    }
    return _netEq.SetBackgroundNoiseMode(mode);
}

WebRtc_Word32
AudioCodingModuleImpl::BackgroundNoiseMode(
    ACMBackgroundNoiseMode& mode)
{
    return _netEq.BackgroundNoiseMode(mode);
}

WebRtc_Word32
AudioCodingModuleImpl::PlayoutTimestamp(
    WebRtc_UWord32& timestamp)
{
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, _id,
        "PlayoutTimestamp()");
    return _netEq.PlayoutTimestamp(timestamp);
}

bool
AudioCodingModuleImpl::HaveValidEncoder(
    const char* callerName) const
{
    if((!_sendCodecRegistered) ||
        (_currentSendCodecIdx < 0) ||
        (_currentSendCodecIdx >= ACMCodecDB::kNumCodecs))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "%s failed: No send codec is registered.", callerName);
        return false;
    }
    if((_currentSendCodecIdx < 0) ||
        (_currentSendCodecIdx >= ACMCodecDB::kNumCodecs))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "%s failed: Send codec index out of range.", callerName);
        return false;
    }
    if(_codecs[_currentSendCodecIdx] == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "%s failed: Send codec is NULL pointer.", callerName);
        return false;
    }
    return true;
}

WebRtc_Word32
AudioCodingModuleImpl::UnregisterReceiveCodec(
    const WebRtc_Word16 payloadType)
{
    CriticalSectionScoped lock(*_acmCritSect);
    WebRtc_Word16 codecID;

    // Search through the list of registered payload types
    for (codecID = 0; codecID < ACMCodecDB::kMaxNumCodecs; codecID++)
    {
        if (_registeredPlTypes[codecID] == payloadType)
        {
            // we have found the codecID registered with the payload type
            break;
        }
    }

    if(codecID >= ACMCodecDB::kNumCodecs)
    {
        // payload type was not registered. No need to unregister
        return 0;
    }

    // Unregister the codec with the given payload type
    return UnregisterReceiveCodecSafe(codecID);
}

WebRtc_Word32
AudioCodingModuleImpl::UnregisterReceiveCodecSafe(
    const WebRtc_Word16 codecID)
{
    const WebRtcNetEQDecoder *neteqDecoder = ACMCodecDB::NetEQDecoders();
    WebRtc_Word16 mirrorID = ACMCodecDB::MirrorID(codecID);
    bool stereo_receiver = false;

    if(_codecs[codecID] != NULL)
    {
        if(_registeredPlTypes[codecID] != -1)
        {
            // Store stereo information for future use.
            stereo_receiver = _stereoReceive[codecID];

            // Before deleting the decoder instance unregister from NetEQ.
            if(_netEq.RemoveCodec(neteqDecoder[codecID], _stereoReceive[codecID]) < 0)
            {
                CodecInst codecInst;
                ACMCodecDB::Codec(codecID, &codecInst);
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                    "Unregistering %s-%d from NetEQ failed.",
                    codecInst.plname, codecInst.plfreq);
                return -1;
            }

            // CN is a special case for NetEQ, all three sampling frequencies
            // are unregistered if one is deleted
            if(STR_CASE_CMP(ACMCodecDB::database_[codecID].plname, "CN") == 0)
            {
                // Search codecs nearby in the database to unregister all CN.
                // TODO(tlegrand): do this search in a safe way. We can search
                // outside the database.
                for (int i=-2; i<3; i++)
                {
                    if (STR_CASE_CMP(ACMCodecDB::database_[codecID+i].plname, "CN") == 0)
                    {
                        if(_stereoReceive[codecID+i])
                        {
                            _stereoReceive[codecID+i] = false;
                        }
                        _registeredPlTypes[codecID+i] = -1;
                    }
                }
                _cng_reg_receiver = false;
            } else
            {
                if(codecID == mirrorID)
                {
                    _codecs[codecID]->DestructDecoder();
                    if(_stereoReceive[codecID])
                    {
                        _slaveCodecs[codecID]->DestructDecoder();
                        _stereoReceive[codecID] = false;

                    }
                }
            }

            // Check if this is the last registered stereo receive codec.
            if (stereo_receiver) {
                bool no_stereo = true;

                for (int i = 0; i < ACMCodecDB::kNumCodecs; i++) {
                    if (_stereoReceive[i]) {
                        // We still have stereo codecs registered.
                        no_stereo = false;
                       break;
                    }
                }

                // If we don't have any stereo codecs left, change status.
                if (no_stereo) {
                  _stereoReceiveRegistered = false;
                }
            }
        }
    }

    if(_registeredPlTypes[codecID] == _receiveREDPayloadType)
    {
        // RED is going to be unregistered.
        // set the following to an invalid value.
        _receiveREDPayloadType = 255;
    }
    _registeredPlTypes[codecID] = -1;

    return 0;
}

WebRtc_Word32
AudioCodingModuleImpl::REDPayloadISAC(
    const WebRtc_Word32  isacRate,
    const WebRtc_Word16  isacBwEstimate,
    WebRtc_UWord8*       payload,
    WebRtc_Word16*       payloadLenByte)
{
   if(!HaveValidEncoder("EncodeData"))
   {
       return -1;
   }
   WebRtc_Word16 status;

   status = _codecs[_currentSendCodecIdx]->REDPayloadISAC(isacRate, isacBwEstimate,
       payload, payloadLenByte);

   return status;
}

} // namespace webrtc
