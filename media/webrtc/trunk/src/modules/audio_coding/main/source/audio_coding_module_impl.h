/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_AUDIO_CODING_MODULE_IMPL_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_AUDIO_CODING_MODULE_IMPL_H_

#include "acm_codec_database.h"
#include "acm_neteq.h"
#include "acm_resampler.h"
#include "common_types.h"
#include "engine_configurations.h"

namespace webrtc {

class ACMDTMFDetection;
class ACMGenericCodec;
class CriticalSectionWrapper;
class RWLockWrapper;

//#define TIMED_LOGGING

#ifdef TIMED_LOGGING
    #include "../test/timedtrace.h"
#endif

#ifdef ACM_QA_TEST
#   include <stdio.h>
#endif

class AudioCodingModuleImpl : public AudioCodingModule
{
public:
    // constructor
    AudioCodingModuleImpl(
        const WebRtc_Word32 id);

    // destructor
    ~AudioCodingModuleImpl();

    // get version information for ACM and all components
    WebRtc_Word32 Version(
        char*   version,
        WebRtc_UWord32& remainingBufferInBytes,
        WebRtc_UWord32& position) const;

    // change the unique identifier of this object
    virtual WebRtc_Word32 ChangeUniqueId(
        const WebRtc_Word32 id);

    // returns the number of milliseconds until the module want
    // a worker thread to call Process
    WebRtc_Word32 TimeUntilNextProcess();

    // Process any pending tasks such as timeouts
    WebRtc_Word32 Process();

    // used in conference to go to and from active encoding, hence
    // in and out of mix
    WebRtc_Word32 SetMode(
        const bool passive);



    /////////////////////////////////////////
    //   Sender
    //

    // initialize send codec
    WebRtc_Word32 InitializeSender();

    // reset send codec
    WebRtc_Word32 ResetEncoder();

    // can be called multiple times for Codec, CNG, RED
    WebRtc_Word32 RegisterSendCodec(
        const CodecInst& sendCodec);

    // get current send codec
    WebRtc_Word32 SendCodec(
        CodecInst& currentSendCodec) const;

    // get current send freq
    WebRtc_Word32 SendFrequency() const;

    // Get encode bitrate
    // Adaptive rate codecs return their current encode target rate, while other codecs
    // return there longterm avarage or their fixed rate.
    WebRtc_Word32 SendBitrate() const;

    // set available bandwidth, inform the encoder about the
    // estimated bandwidth received from the remote party
    virtual WebRtc_Word32 SetReceivedEstimatedBandwidth(
        const WebRtc_Word32 bw);

    // register a transport callback which will be
    // called to deliver the encoded buffers
    WebRtc_Word32 RegisterTransportCallback(
        AudioPacketizationCallback* transport);

    // Used by the module to deliver messages to the codec module/application
    // AVT(DTMF)
    WebRtc_Word32 RegisterIncomingMessagesCallback(
        AudioCodingFeedback* incomingMessagesCallback,
        const ACMCountries cpt);

     // Add 10MS of raw (PCM) audio data to the encoder
    WebRtc_Word32 Add10MsData(
        const AudioFrame& audioFrame);

    // set background noise mode for NetEQ, on, off or fade
    WebRtc_Word32 SetBackgroundNoiseMode(
        const ACMBackgroundNoiseMode mode);

    // get current background noise mode
    WebRtc_Word32 BackgroundNoiseMode(
        ACMBackgroundNoiseMode& mode);

    /////////////////////////////////////////
    // (FEC) Forward Error Correction
    //

    // configure FEC status i.e on/off
    WebRtc_Word32 SetFECStatus(
        const bool enable);

    // Get FEC status
    bool FECStatus() const;

    /////////////////////////////////////////
    //   (VAD) Voice Activity Detection
    //   and
    //   (CNG) Comfort Noise Generation
    //

    WebRtc_Word32 SetVAD(
        const bool             enableDTX = true,
        const bool             enableVAD = false,
        const ACMVADMode vadMode   = VADNormal);

    WebRtc_Word32 VAD(
        bool&             dtxEnabled,
        bool&             vadEnabled,
        ACMVADMode& vadMode) const;

    WebRtc_Word32 RegisterVADCallback(
        ACMVADCallback* vadCallback);

    // Get VAD aggressiveness on the incoming stream
    ACMVADMode ReceiveVADMode() const;

    // Configure VAD aggressiveness on the incoming stream
    WebRtc_Word16 SetReceiveVADMode(
        const ACMVADMode mode);


    /////////////////////////////////////////
    //   Receiver
    //

    // initialize receiver, resets codec database etc
    WebRtc_Word32 InitializeReceiver();

    // reset the decoder state
    WebRtc_Word32 ResetDecoder();

    // get current receive freq
    WebRtc_Word32 ReceiveFrequency() const;

    // get current playout freq
    WebRtc_Word32 PlayoutFrequency() const;

    // register possible reveive codecs, can be called multiple times,
    // for codecs, CNG, DTMF, RED
    WebRtc_Word32 RegisterReceiveCodec(
        const CodecInst& receiveCodec);

    // get current received codec
    WebRtc_Word32 ReceiveCodec(
        CodecInst& currentReceiveCodec) const;

    // incoming packet from network parsed and ready for decode
    WebRtc_Word32 IncomingPacket(
        const WebRtc_UWord8*   incomingPayload,
        const WebRtc_Word32    payloadLength,
        const WebRtcRTPHeader& rtpInfo);

    // Incoming payloads, without rtp-info, the rtp-info will be created in ACM.
    // One usage for this API is when pre-encoded files are pushed in ACM.
    WebRtc_Word32 IncomingPayload(
        const WebRtc_UWord8* incomingPayload,
        const WebRtc_Word32  payloadLength,
        const WebRtc_UWord8  payloadType,
        const WebRtc_UWord32 timestamp = 0);

    // Minimum playout dealy (Used for lip-sync)
    WebRtc_Word32 SetMinimumPlayoutDelay(
        const WebRtc_Word32 timeMs);

    // configure Dtmf playout status i.e on/off playout the incoming outband Dtmf tone
    WebRtc_Word32 SetDtmfPlayoutStatus(
        const bool enable);

    // Get Dtmf playout status
    bool DtmfPlayoutStatus() const;

    // Estimate the Bandwidth based on the incoming stream
    // This is also done in the RTP module
    // need this for one way audio where the RTCP send the BW estimate
    WebRtc_Word32 DecoderEstimatedBandwidth() const;

    // Set playout mode voice, fax
    WebRtc_Word32 SetPlayoutMode(
        const AudioPlayoutMode mode);

    // Get playout mode voice, fax
    AudioPlayoutMode PlayoutMode() const;

    // Get playout timestamp
    WebRtc_Word32 PlayoutTimestamp(
        WebRtc_UWord32& timestamp);

    // Get 10 milliseconds of raw audio data to play out
    // automatic resample to the requested frequency if > 0
    WebRtc_Word32 PlayoutData10Ms(
        const WebRtc_Word32   desiredFreqHz,
        AudioFrame            &audioFrame);


    /////////////////////////////////////////
    //   Statistics
    //

    WebRtc_Word32  NetworkStatistics(
        ACMNetworkStatistics& statistics) const;

    void DestructEncoderInst(void* ptrInst);

    WebRtc_Word16 AudioBuffer(WebRtcACMAudioBuff& audioBuff);

    // GET RED payload for iSAC. The method id called
    // when 'this' ACM is default ACM.
    WebRtc_Word32 REDPayloadISAC(
        const WebRtc_Word32  isacRate,
        const WebRtc_Word16  isacBwEstimate,
        WebRtc_UWord8*       payload,
        WebRtc_Word16*       payloadLenByte);

    WebRtc_Word16 SetAudioBuffer(WebRtcACMAudioBuff& audioBuff);

    WebRtc_UWord32 EarliestTimestamp() const;

    WebRtc_Word32 LastEncodedTimestamp(WebRtc_UWord32& timestamp) const;

    WebRtc_Word32 ReplaceInternalDTXWithWebRtc(
        const bool useWebRtcDTX);

    WebRtc_Word32 IsInternalDTXReplacedWithWebRtc(
        bool& usesWebRtcDTX);

    WebRtc_Word32 SetISACMaxRate(
        const WebRtc_UWord32 rateBitPerSec);

    WebRtc_Word32 SetISACMaxPayloadSize(
        const WebRtc_UWord16 payloadLenBytes);

    WebRtc_Word32 ConfigISACBandwidthEstimator(
        const WebRtc_UWord8  initFrameSizeMsec,
        const WebRtc_UWord16 initRateBitPerSec,
        const bool           enforceFrameSize = false);

    WebRtc_Word32 UnregisterReceiveCodec(
        const WebRtc_Word16 payloadType);

protected:
    void UnregisterSendCodec();

    WebRtc_Word32 UnregisterReceiveCodecSafe(
        const WebRtc_Word16 codecID);

    ACMGenericCodec* CreateCodec(
        const CodecInst& codec);

    WebRtc_Word16 DecoderParamByPlType(
        const WebRtc_UWord8    payloadType,
        WebRtcACMCodecParams&  codecParams) const;

    WebRtc_Word16 DecoderListIDByPlName(
        const char*  payloadName,
        const WebRtc_UWord16 sampFreqHz = 0) const;

    WebRtc_Word32 InitializeReceiverSafe();

    bool HaveValidEncoder(const char* callerName) const;

    WebRtc_Word32 RegisterRecCodecMSSafe(
        const CodecInst& receiveCodec,
        WebRtc_Word16         codecId,
        WebRtc_Word16         mirrorId,
        ACMNetEQ::JB          jitterBuffer);

private:
    AudioPacketizationCallback*    _packetizationCallback;
    WebRtc_Word32                  _id;
    WebRtc_UWord32                 _lastTimestamp;
    WebRtc_UWord32                 _lastInTimestamp;
    CodecInst                      _sendCodecInst;
    uint8_t                        _cng_nb_pltype;
    uint8_t                        _cng_wb_pltype;
    uint8_t                        _cng_swb_pltype;
    uint8_t                        _red_pltype;
    bool                           _cng_reg_receiver;
    bool                           _vadEnabled;
    bool                           _dtxEnabled;
    ACMVADMode                     _vadMode;
    ACMGenericCodec*               _codecs[ACMCodecDB::kMaxNumCodecs];
    ACMGenericCodec*               _slaveCodecs[ACMCodecDB::kMaxNumCodecs];
    WebRtc_Word16                  _mirrorCodecIdx[ACMCodecDB::kMaxNumCodecs];
    bool                           _stereoReceive[ACMCodecDB::kMaxNumCodecs];
    bool                           _stereoReceiveRegistered;
    bool                           _stereoSend;
    int                            _prev_received_channel;
    int                            _expected_channels;
    WebRtc_Word32                  _currentSendCodecIdx;
    int                            _current_receive_codec_idx;
    bool                           _sendCodecRegistered;
    ACMResampler                   _inputResampler;
    ACMResampler                   _outputResampler;
    ACMNetEQ                       _netEq;
    CriticalSectionWrapper*        _acmCritSect;
    ACMVADCallback*                _vadCallback;
    WebRtc_UWord8                  _lastRecvAudioCodecPlType;

    // RED/FEC
    bool                           _isFirstRED;
    bool                           _fecEnabled;
    WebRtc_UWord8*                 _redBuffer;
    RTPFragmentationHeader*        _fragmentation;
    WebRtc_UWord32                 _lastFECTimestamp;
    // if no RED is registered as receive codec this
    // will have an invalid value.
    WebRtc_UWord8                  _receiveREDPayloadType;

    // This is to keep track of CN instances where we can send DTMFs
    WebRtc_UWord8                  _previousPayloadType;

    // This keeps track of payload types associated with _codecs[].
    // We define it as signed variable and initialize with -1 to indicate
    // unused elements.
    WebRtc_Word16                  _registeredPlTypes[ACMCodecDB::kMaxNumCodecs];

    // Used when payloads are pushed into ACM without any RTP info
    // One example is when pre-encoded bit-stream is pushed from
    // a file.
    WebRtcRTPHeader*               _dummyRTPHeader;
    WebRtc_UWord16                 _recvPlFrameSizeSmpls;

    bool                           _receiverInitialized;
    ACMDTMFDetection*              _dtmfDetector;

    AudioCodingFeedback*           _dtmfCallback;
    WebRtc_Word16                  _lastDetectedTone;
    CriticalSectionWrapper*        _callbackCritSect;
#ifdef TIMED_LOGGING
    TimedTrace                     _trace;
#endif

    AudioFrame                     _audioFrame;

#ifdef ACM_QA_TEST
    FILE* _outgoingPL;
    FILE* _incomingPL;
#endif

};

} // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_AUDIO_CODING_MODULE_IMPL_H_
