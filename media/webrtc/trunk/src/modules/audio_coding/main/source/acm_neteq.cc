/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <algorithm>  // sort
#include <stdlib.h>  // malloc
#include <vector>

#include "acm_neteq.h"
#include "common_types.h"
#include "critical_section_wrapper.h"
#include "rw_lock_wrapper.h"
#include "signal_processing_library.h"
#include "tick_util.h"
#include "trace.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_internal.h"

namespace webrtc
{

#define RTP_HEADER_SIZE 12
#define NETEQ_INIT_FREQ 8000
#define NETEQ_INIT_FREQ_KHZ (NETEQ_INIT_FREQ/1000)
#define NETEQ_ERR_MSG_LEN_BYTE (WEBRTC_NETEQ_MAX_ERROR_NAME + 1)


ACMNetEQ::ACMNetEQ()
:
_id(0),
_currentSampFreqKHz(NETEQ_INIT_FREQ_KHZ),
_avtPlayout(false),
_playoutMode(voice),
_netEqCritSect(CriticalSectionWrapper::CreateCriticalSection()),
_vadStatus(false),
_vadMode(VADNormal),
_decodeLock(RWLockWrapper::CreateRWLock()),
_numSlaves(0),
_receivedStereo(false),
_masterSlaveInfo(NULL),
_previousAudioActivity(AudioFrame::kVadUnknown),
_extraDelay(0),
_callbackCritSect(CriticalSectionWrapper::CreateCriticalSection())
{
    for(int n = 0; n < MAX_NUM_SLAVE_NETEQ + 1; n++)
    {
        _isInitialized[n]     = false;
        _ptrVADInst[n]        = NULL;
        _inst[n]              = NULL;
        _instMem[n]           = NULL;
        _netEqPacketBuffer[n] = NULL;
    }
}

ACMNetEQ::~ACMNetEQ() {
  {
    CriticalSectionScoped lock(_netEqCritSect);
    RemoveNetEQSafe(0);  // Master.
    RemoveSlavesSafe();
  }
  if (_netEqCritSect != NULL) {
    delete _netEqCritSect;
  }

  if (_decodeLock != NULL) {
    delete _decodeLock;
  }

  if (_callbackCritSect != NULL) {
    delete _callbackCritSect;
  }
}

WebRtc_Word32
ACMNetEQ::Init()
{
    CriticalSectionScoped lock(_netEqCritSect);

    for(WebRtc_Word16 idx = 0; idx < _numSlaves + 1; idx++)
    {
        if(InitByIdxSafe(idx) < 0)
        {
            return -1;
        }
        // delete VAD instance and start fresh if required.
        if(_ptrVADInst[idx] != NULL)
        {
            WebRtcVad_Free(_ptrVADInst[idx]);
            _ptrVADInst[idx] = NULL;
        }
        if(_vadStatus)
        {
            // Has to enable VAD
            if(EnableVADByIdxSafe(idx) < 0)
            {
                // Failed to enable VAD.
                // Delete VAD instance, if it is created
                if(_ptrVADInst[idx] != NULL)
                {
                    WebRtcVad_Free(_ptrVADInst[idx]);
                    _ptrVADInst[idx] = NULL;
                }
                // We are at initialization of NetEq, if failed to
                // enable VAD, we delete the NetEq instance.
                if (_instMem[idx] != NULL) {
                    free(_instMem[idx]);
                    _instMem[idx] = NULL;
                    _inst[idx] = NULL;
                }
                _isInitialized[idx] = false;
                return -1;
            }
        }
        _isInitialized[idx] = true;
    }
    if (EnableVAD() == -1)
    {
        return -1;
    }
    return 0;
}

WebRtc_Word16
ACMNetEQ::InitByIdxSafe(
    const WebRtc_Word16 idx)
{
    int memorySizeBytes;
    if (WebRtcNetEQ_AssignSize(&memorySizeBytes) != 0)
    {
        LogError("AssignSize", idx);
        return -1;
    }

    if(_instMem[idx] != NULL)
    {
        free(_instMem[idx]);
        _instMem[idx] = NULL;
    }
    _instMem[idx] = malloc(memorySizeBytes);
    if (_instMem[idx] == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "InitByIdxSafe: NetEq Initialization error: could not allocate memory for NetEq");
        _isInitialized[idx] = false;
        return -1;
    }
    if (WebRtcNetEQ_Assign(&_inst[idx], _instMem[idx]) != 0)
    {
        if (_instMem[idx] != NULL) {
            free(_instMem[idx]);
            _instMem[idx] = NULL;
        }
        LogError("Assign", idx);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "InitByIdxSafe: NetEq Initialization error: could not Assign");
        _isInitialized[idx] = false;
        return -1;
    }
    if (WebRtcNetEQ_Init(_inst[idx], NETEQ_INIT_FREQ) != 0)
    {
        if (_instMem[idx] != NULL) {
            free(_instMem[idx]);
            _instMem[idx] = NULL;
        }
        LogError("Init", idx);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "InitByIdxSafe: NetEq Initialization error: could not initialize NetEq");
        _isInitialized[idx] = false;
        return -1;
    }
    _isInitialized[idx] = true;
    return 0;
}

WebRtc_Word16
ACMNetEQ::EnableVADByIdxSafe(
    const WebRtc_Word16 idx)
{
    if(_ptrVADInst[idx] == NULL)
    {
        if(WebRtcVad_Create(&_ptrVADInst[idx]) < 0)
        {
            _ptrVADInst[idx] = NULL;
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "EnableVADByIdxSafe: NetEq Initialization error: could not create VAD");
            return -1;
        }
    }

    if(WebRtcNetEQ_SetVADInstance(_inst[idx], _ptrVADInst[idx],
        (WebRtcNetEQ_VADInitFunction)    WebRtcVad_Init,
        (WebRtcNetEQ_VADSetmodeFunction) WebRtcVad_set_mode,
        (WebRtcNetEQ_VADFunction)        WebRtcVad_Process) < 0)
    {
       LogError("setVADinstance", idx);
       WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
           "EnableVADByIdxSafe: NetEq Initialization error: could not set VAD instance");
        return -1;
    }

    if(WebRtcNetEQ_SetVADMode(_inst[idx], _vadMode) < 0)
    {
        LogError("setVADmode", idx);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "EnableVADByIdxSafe: NetEq Initialization error: could not set VAD mode");
        return -1;
    }
    return 0;
}




WebRtc_Word32
ACMNetEQ::AllocatePacketBuffer(
    const WebRtcNetEQDecoder* usedCodecs,
    WebRtc_Word16     noOfCodecs)
{
    // Due to WebRtcNetEQ_GetRecommendedBufferSize
    // the following has to be int otherwise we will have compiler error
    // if not casted

    CriticalSectionScoped lock(_netEqCritSect);
    for(WebRtc_Word16 idx = 0; idx < _numSlaves + 1; idx++)
    {
        if(AllocatePacketBufferByIdxSafe(usedCodecs, noOfCodecs, idx) < 0)
        {
            return -1;
        }
    }
    return 0;
}

WebRtc_Word16
ACMNetEQ::AllocatePacketBufferByIdxSafe(
    const WebRtcNetEQDecoder*    usedCodecs,
    WebRtc_Word16       noOfCodecs,
    const WebRtc_Word16 idx)
{
    int maxNoPackets;
    int bufferSizeInBytes;

    if(!_isInitialized[idx])
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "AllocatePacketBufferByIdxSafe: NetEq is not initialized.");
        return -1;
    }
    if (WebRtcNetEQ_GetRecommendedBufferSize(_inst[idx], usedCodecs, noOfCodecs,
        kTCPLargeJitter , &maxNoPackets, &bufferSizeInBytes)
        != 0)
    {
        LogError("GetRecommendedBufferSize", idx);
        return -1;
    }
    if(_netEqPacketBuffer[idx] != NULL)
    {
        free(_netEqPacketBuffer[idx]);
        _netEqPacketBuffer[idx] = NULL;
    }

    _netEqPacketBuffer[idx] = (WebRtc_Word16 *)malloc(bufferSizeInBytes);
    if (_netEqPacketBuffer[idx] == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "AllocatePacketBufferByIdxSafe: NetEq Initialization error: could not allocate "
            "memory for NetEq Packet Buffer");
        return -1;

    }
    if (WebRtcNetEQ_AssignBuffer(_inst[idx], maxNoPackets, _netEqPacketBuffer[idx],
        bufferSizeInBytes) != 0)
    {
        if (_netEqPacketBuffer[idx] != NULL) {
            free(_netEqPacketBuffer[idx]);
            _netEqPacketBuffer[idx] = NULL;
        }
        LogError("AssignBuffer", idx);
        return -1;
    }
    return 0;
}




WebRtc_Word32
ACMNetEQ::SetExtraDelay(
    const WebRtc_Word32 delayInMS)
{
    CriticalSectionScoped lock(_netEqCritSect);

    for(WebRtc_Word16 idx = 0; idx < _numSlaves + 1; idx++)
    {
        if(!_isInitialized[idx])
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "SetExtraDelay: NetEq is not initialized.");
            return -1;
        }
        if(WebRtcNetEQ_SetExtraDelay(_inst[idx], delayInMS) < 0)
        {
            LogError("SetExtraDelay", idx);
            return -1;
        }
    }
    _extraDelay = delayInMS;
    return 0;
}


WebRtc_Word32
ACMNetEQ::SetAVTPlayout(
    const bool enable)
{
    CriticalSectionScoped lock(_netEqCritSect);
    if (_avtPlayout != enable)
    {
        for(WebRtc_Word16 idx = 0; idx < _numSlaves + 1; idx++)
        {
            if(!_isInitialized[idx])
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                    "SetAVTPlayout: NetEq is not initialized.");
                return -1;
            }
            if(WebRtcNetEQ_SetAVTPlayout(_inst[idx], (enable) ? 1 : 0) < 0)
            {
                LogError("SetAVTPlayout", idx);
                return -1;
            }
        }
    }
    _avtPlayout = enable;
    return 0;
}


bool
ACMNetEQ::AVTPlayout() const
{
    CriticalSectionScoped lock(_netEqCritSect);
    return _avtPlayout;
}

WebRtc_Word32
ACMNetEQ::CurrentSampFreqHz() const
{
    CriticalSectionScoped lock(_netEqCritSect);
    if(!_isInitialized[0])
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "CurrentSampFreqHz: NetEq is not initialized.");
        return -1;
    }
    return (WebRtc_Word32)(1000*_currentSampFreqKHz);
}


WebRtc_Word32
ACMNetEQ::SetPlayoutMode(
    const AudioPlayoutMode mode)
{
    CriticalSectionScoped lock(_netEqCritSect);
    if(_playoutMode != mode)
    {
        for(WebRtc_Word16 idx = 0; idx < _numSlaves + 1; idx++)
        {
            if(!_isInitialized[idx])
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                    "SetPlayoutMode: NetEq is not initialized.");
                return -1;
            }

            enum WebRtcNetEQPlayoutMode playoutMode = kPlayoutOff;
            switch(mode)
            {
            case voice:
                playoutMode = kPlayoutOn;
                break;
            case fax:
                playoutMode = kPlayoutFax;
                break;
            case streaming:
                playoutMode = kPlayoutStreaming;
                break;
            }
            if(WebRtcNetEQ_SetPlayoutMode(_inst[idx], playoutMode) < 0)
            {
                LogError("SetPlayoutMode", idx);
                return -1;
            }
        }
        _playoutMode = mode;
    }

    return 0;
}

AudioPlayoutMode
ACMNetEQ::PlayoutMode() const
{
    CriticalSectionScoped lock(_netEqCritSect);
    return _playoutMode;
}


WebRtc_Word32
ACMNetEQ::NetworkStatistics(
    ACMNetworkStatistics* statistics) const
{
    WebRtcNetEQ_NetworkStatistics stats;
    CriticalSectionScoped lock(_netEqCritSect);
    if(!_isInitialized[0])
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "NetworkStatistics: NetEq is not initialized.");
        return -1;
    }
    if(WebRtcNetEQ_GetNetworkStatistics(_inst[0], &stats) == 0)
    {
        statistics->currentAccelerateRate = stats.currentAccelerateRate;
        statistics->currentBufferSize = stats.currentBufferSize;
        statistics->jitterPeaksFound = (stats.jitterPeaksFound > 0);
        statistics->currentDiscardRate = stats.currentDiscardRate;
        statistics->currentExpandRate = stats.currentExpandRate;
        statistics->currentPacketLossRate = stats.currentPacketLossRate;
        statistics->currentPreemptiveRate = stats.currentPreemptiveRate;
        statistics->preferredBufferSize = stats.preferredBufferSize;
        statistics->clockDriftPPM = stats.clockDriftPPM;
    }
    else
    {
        LogError("getNetworkStatistics", 0);
        return -1;
    }
    const int kArrayLen = 100;
    int waiting_times[kArrayLen];
    int waiting_times_len = WebRtcNetEQ_GetRawFrameWaitingTimes(
        _inst[0], kArrayLen, waiting_times);
    if (waiting_times_len > 0)
    {
        std::vector<int> waiting_times_vec(waiting_times,
                                           waiting_times + waiting_times_len);
        std::sort(waiting_times_vec.begin(), waiting_times_vec.end());
        size_t size = waiting_times_vec.size();
        assert(size == static_cast<size_t>(waiting_times_len));
        if (size % 2 == 0)
        {
            statistics->medianWaitingTimeMs =
                (waiting_times_vec[size / 2 - 1] +
                    waiting_times_vec[size / 2]) / 2;
        }
        else
        {
            statistics->medianWaitingTimeMs = waiting_times_vec[size / 2];
        }
        statistics->minWaitingTimeMs = waiting_times_vec.front();
        statistics->maxWaitingTimeMs = waiting_times_vec.back();
        double sum = 0;
        for (size_t i = 0; i < size; ++i) {
          sum += waiting_times_vec[i];
        }
        statistics->meanWaitingTimeMs = static_cast<int>(sum / size);
    }
    else if (waiting_times_len == 0)
    {
        statistics->meanWaitingTimeMs = -1;
        statistics->medianWaitingTimeMs = -1;
        statistics->minWaitingTimeMs = -1;
        statistics->maxWaitingTimeMs = -1;
    }
    else
    {
        LogError("getRawFrameWaitingTimes", 0);
        return -1;
    }
    return 0;
}

WebRtc_Word32
ACMNetEQ::RecIn(
    const WebRtc_UWord8*   incomingPayload,
    const WebRtc_Word32    payloadLength,
    const WebRtcRTPHeader& rtpInfo)
{
    WebRtc_Word16 payload_length = static_cast<WebRtc_Word16>(payloadLength);

    // translate to NetEq struct
    WebRtcNetEQ_RTPInfo netEqRTPInfo;
    netEqRTPInfo.payloadType = rtpInfo.header.payloadType;
    netEqRTPInfo.sequenceNumber = rtpInfo.header.sequenceNumber;
    netEqRTPInfo.timeStamp = rtpInfo.header.timestamp;
    netEqRTPInfo.SSRC = rtpInfo.header.ssrc;
    netEqRTPInfo.markerBit = rtpInfo.header.markerBit;

    CriticalSectionScoped lock(_netEqCritSect);
    // Down-cast the time to (32-6)-bit since we only care about
    // the least significant bits. (32-6) bits cover 2^(32-6) = 67108864 ms.
    // we masked 6 most significant bits of 32-bit so we don't loose resolution
    // when do the following multiplication.
    const WebRtc_UWord32 nowInMs = static_cast<WebRtc_UWord32>(
        TickTime::MillisecondTimestamp() & 0x03ffffff);
    WebRtc_UWord32 recvTimestamp = static_cast<WebRtc_UWord32>
        (_currentSampFreqKHz * nowInMs);

    int status;
    // In case of stereo payload, first half of the data should be pushed into
    // master, and the second half into slave.
    if (rtpInfo.type.Audio.channel == 2) {
      payload_length = payload_length / 2;
    }

    // Check that master is initialized.
    if(!_isInitialized[0])
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "RecIn: NetEq is not initialized.");
        return -1;
    }
    // PUSH into Master
    status = WebRtcNetEQ_RecInRTPStruct(_inst[0], &netEqRTPInfo,
             incomingPayload, payload_length, recvTimestamp);
    if(status < 0)
    {
        LogError("RecInRTPStruct", 0);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                     "RecIn: NetEq, error in pushing in Master");
        return -1;
    }

    // If the received stream is stereo, insert second half of paket into slave.
    if(rtpInfo.type.Audio.channel == 2)
    {
        if(!_isInitialized[1])
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "RecIn: NetEq is not initialized.");
            return -1;
        }
        // PUSH into Slave
        status = WebRtcNetEQ_RecInRTPStruct(_inst[1], &netEqRTPInfo,
                                            &incomingPayload[payload_length],
                                            payload_length,
                                            recvTimestamp);
        if(status < 0)
        {
            LogError("RecInRTPStruct", 1);
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "RecIn: NetEq, error in pushing in Slave");
            return -1;
        }
    }

    return 0;
}

WebRtc_Word32
ACMNetEQ::RecOut(
    AudioFrame& audioFrame)
{
    enum WebRtcNetEQOutputType type;
    WebRtc_Word16 payloadLenSample;
    enum WebRtcNetEQOutputType typeMaster;
    enum WebRtcNetEQOutputType typeSlave;

    WebRtc_Word16 payloadLenSampleSlave;

    CriticalSectionScoped lockNetEq(_netEqCritSect);

    if(!_receivedStereo)
    {
        if(!_isInitialized[0])
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "RecOut: NetEq is not initialized.");
            return -1;
        }
        {
            WriteLockScoped lockCodec(*_decodeLock);
            if(WebRtcNetEQ_RecOut(_inst[0], &(audioFrame.data_[0]),
                &payloadLenSample) != 0)
            {
                LogError("RecOut", 0);
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id, 
                    "RecOut: NetEq, error in pulling out for mono case");

                // Check for errors that can be recovered from:
                // RECOUT_ERROR_SAMPLEUNDERRUN = 2003
                int errorCode = WebRtcNetEQ_GetErrorCode(_inst[0]);
                if(errorCode != 2003)
                {
                    // Cannot recover; return an error
                    return -1;
                }
            }
        }
        WebRtcNetEQ_GetSpeechOutputType(_inst[0], &type);
        audioFrame.num_channels_ = 1;
    }
    else
    {
        if(!_isInitialized[0] || !_isInitialized[1])
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "RecOut: NetEq is not initialized.");
            return -1;
        }
        WebRtc_Word16 payloadMaster[480];
        WebRtc_Word16 payloadSlave[480];
        {
            WriteLockScoped lockCodec(*_decodeLock);
            if(WebRtcNetEQ_RecOutMasterSlave(_inst[0], payloadMaster,
                &payloadLenSample, _masterSlaveInfo, 1) != 0)
            {
                LogError("RecOutMasterSlave", 0);
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                    "RecOut: NetEq, error in pulling out for master");

                // Check for errors that can be recovered from:
                // RECOUT_ERROR_SAMPLEUNDERRUN = 2003
                int errorCode = WebRtcNetEQ_GetErrorCode(_inst[0]);
                if(errorCode != 2003)
                {
                    // Cannot recover; return an error
                    return -1;
                }
            }
            if(WebRtcNetEQ_RecOutMasterSlave(_inst[1], payloadSlave,
                &payloadLenSampleSlave, _masterSlaveInfo, 0) != 0)
            {
                LogError("RecOutMasterSlave", 1);
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                    "RecOut: NetEq, error in pulling out for slave");

                // Check for errors that can be recovered from:
                // RECOUT_ERROR_SAMPLEUNDERRUN = 2003
                int errorCode = WebRtcNetEQ_GetErrorCode(_inst[1]);
                if(errorCode != 2003)
                {
                    // Cannot recover; return an error
                    return -1;
                }
            }
        }

        if(payloadLenSample != payloadLenSampleSlave)
        {
            WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, _id,
                "RecOut: mismatch between the lenght of the decoded \
audio by Master (%d samples) and Slave (%d samples).",
            payloadLenSample, payloadLenSampleSlave);
            if(payloadLenSample > payloadLenSampleSlave)
            {
                memset(&payloadSlave[payloadLenSampleSlave], 0,
                    (payloadLenSample - payloadLenSampleSlave) * sizeof(WebRtc_Word16));
            }
        }

        for(WebRtc_Word16 n = 0; n < payloadLenSample; n++)
        {
            audioFrame.data_[n<<1]     = payloadMaster[n];
            audioFrame.data_[(n<<1)+1] = payloadSlave[n];
        }
        audioFrame.num_channels_ = 2;

        WebRtcNetEQ_GetSpeechOutputType(_inst[0], &typeMaster);
        WebRtcNetEQ_GetSpeechOutputType(_inst[1], &typeSlave);
        if((typeMaster == kOutputNormal) ||
            (typeSlave == kOutputNormal))
        {
            type = kOutputNormal;
        }
        else
        {
            type = typeMaster;
        }
    }

    audioFrame.samples_per_channel_ = static_cast<WebRtc_UWord16>(payloadLenSample);
    // NetEq always returns 10 ms of audio.
    _currentSampFreqKHz = static_cast<float>(audioFrame.samples_per_channel_) / 10.0f;
    audioFrame.sample_rate_hz_ = audioFrame.samples_per_channel_ * 100;
    if(_vadStatus)
    {
        if(type == kOutputVADPassive)
        {
            audioFrame.vad_activity_ = AudioFrame::kVadPassive;
            audioFrame.speech_type_ = AudioFrame::kNormalSpeech;
        }
        else if(type == kOutputNormal)
        {
            audioFrame.vad_activity_ = AudioFrame::kVadActive;
            audioFrame.speech_type_ = AudioFrame::kNormalSpeech;
        }
        else if(type == kOutputPLC)
        {
            audioFrame.vad_activity_ = _previousAudioActivity;
            audioFrame.speech_type_  = AudioFrame::kPLC;
        }
        else if(type == kOutputCNG)
        {
            audioFrame.vad_activity_ = AudioFrame::kVadPassive;
            audioFrame.speech_type_  = AudioFrame::kCNG;
        }
        else
        {
            audioFrame.vad_activity_ = AudioFrame::kVadPassive;
            audioFrame.speech_type_  = AudioFrame::kPLCCNG;
        }
    }
    else
    {
        // Always return kVadUnknown when receive VAD is inactive
        audioFrame.vad_activity_ = AudioFrame::kVadUnknown;

        if(type == kOutputNormal)
        {
            audioFrame.speech_type_  = AudioFrame::kNormalSpeech;
        }
        else if(type == kOutputPLC)
        {
            audioFrame.speech_type_  = AudioFrame::kPLC;
        }
        else if(type == kOutputPLCtoCNG)
        {
            audioFrame.speech_type_  = AudioFrame::kPLCCNG;
        }
        else if(type == kOutputCNG)
        {
            audioFrame.speech_type_  = AudioFrame::kCNG;
        }
        else
        {
            // type is kOutputVADPassive which
            // we don't expect to get if _vadStatus is false
            WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, _id,
                "RecOut: NetEq returned kVadPassive while _vadStatus is false.");
            audioFrame.vad_activity_ = AudioFrame::kVadUnknown;
            audioFrame.speech_type_  = AudioFrame::kNormalSpeech;
        }
    }
    _previousAudioActivity = audioFrame.vad_activity_;

    return 0;
}

// When ACMGenericCodec has set the codec specific parameters in codecDef
// it calls AddCodec() to add the new codec to the NetEQ database.
WebRtc_Word32
ACMNetEQ::AddCodec(
    WebRtcNetEQ_CodecDef* codecDef,
    bool                  toMaster)
{
    if (codecDef == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "ACMNetEQ::AddCodec: error, codecDef is NULL");
        return -1;
    }
    CriticalSectionScoped lock(_netEqCritSect);

    WebRtc_Word16 idx;
    if(toMaster)
    {
        idx = 0;
    }
    else
    {
        idx = 1;
    }

    if(!_isInitialized[idx])
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "ACMNetEQ::AddCodec: NetEq is not initialized.");
        return -1;
    }
    if(WebRtcNetEQ_CodecDbAdd(_inst[idx], codecDef) < 0)
    {
        LogError("CodecDB_Add", idx);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "ACMNetEQ::AddCodec: NetEq, error in adding codec");
        return -1;
    }
    else
    {
        return 0;
    }
}

// Creates a Word16 RTP packet out of a Word8 payload and an rtp info struct.
// Must be byte order safe.
void
ACMNetEQ::RTPPack(
    WebRtc_Word16*         rtpPacket,
    const WebRtc_Word8*    payload,
    const WebRtc_Word32    payloadLengthW8,
    const WebRtcRTPHeader& rtpInfo)
{
    WebRtc_Word32 idx = 0;
    WEBRTC_SPL_SET_BYTE(rtpPacket, (WebRtc_Word8)0x80, idx);
    idx++;

    WEBRTC_SPL_SET_BYTE(rtpPacket, rtpInfo.header.payloadType, idx);
    idx++;

    WEBRTC_SPL_SET_BYTE(rtpPacket, WEBRTC_SPL_GET_BYTE(
        &(rtpInfo.header.sequenceNumber), 1), idx);
    idx++;

    WEBRTC_SPL_SET_BYTE(rtpPacket, WEBRTC_SPL_GET_BYTE(
        &(rtpInfo.header.sequenceNumber), 0), idx);
    idx++;

    WEBRTC_SPL_SET_BYTE(rtpPacket, WEBRTC_SPL_GET_BYTE(
        &(rtpInfo.header.timestamp), 3), idx);
    idx++;

    WEBRTC_SPL_SET_BYTE(rtpPacket, WEBRTC_SPL_GET_BYTE(
        &(rtpInfo.header.timestamp), 2), idx);
    idx++;

    WEBRTC_SPL_SET_BYTE(rtpPacket, WEBRTC_SPL_GET_BYTE(
        &(rtpInfo.header.timestamp), 1), idx);
    idx++;

    WEBRTC_SPL_SET_BYTE(rtpPacket, WEBRTC_SPL_GET_BYTE(
        &(rtpInfo.header.timestamp), 0), idx);
    idx++;

    WEBRTC_SPL_SET_BYTE(rtpPacket, WEBRTC_SPL_GET_BYTE(
        &(rtpInfo.header.ssrc), 3), idx);
    idx++;

    WEBRTC_SPL_SET_BYTE(rtpPacket, WEBRTC_SPL_GET_BYTE(
        &(rtpInfo.header.ssrc), 2), idx);
    idx++;

    WEBRTC_SPL_SET_BYTE(rtpPacket, WEBRTC_SPL_GET_BYTE(
        &(rtpInfo.header.ssrc), 1), idx);
    idx++;

    WEBRTC_SPL_SET_BYTE(rtpPacket, WEBRTC_SPL_GET_BYTE(
        &(rtpInfo.header.ssrc), 0), idx);
    idx++;

    for (WebRtc_Word16 i=0; i < payloadLengthW8; i++)
    {
        WEBRTC_SPL_SET_BYTE(rtpPacket, payload[i], idx);
        idx++;
    }
    if (payloadLengthW8 & 1)
    {
        // Our 16 bits buffer is one byte too large, set that
        // last byte to zero.
        WEBRTC_SPL_SET_BYTE(rtpPacket, 0x0, idx);
    }
}

WebRtc_Word16
ACMNetEQ::EnableVAD()
{
    CriticalSectionScoped lock(_netEqCritSect);
    if (_vadStatus)
    {
        return 0;
    }
    for(WebRtc_Word16 idx = 0; idx < _numSlaves + 1; idx++)
    {
        if(!_isInitialized[idx])
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "SetVADStatus: NetEq is not initialized.");
            return -1;
        }
        // VAD was off and we have to turn it on
        if(EnableVADByIdxSafe(idx) < 0)
        {
            return -1;
        }

        // Set previous VAD status to PASSIVE
        _previousAudioActivity = AudioFrame::kVadPassive;
    }
    _vadStatus = true;
    return 0;
}


ACMVADMode
ACMNetEQ::VADMode() const
{
    CriticalSectionScoped lock(_netEqCritSect);
    return _vadMode;
}


WebRtc_Word16
ACMNetEQ::SetVADMode(
    const ACMVADMode mode)
{
    CriticalSectionScoped lock(_netEqCritSect);
    if((mode < VADNormal) || (mode > VADVeryAggr))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "SetVADMode: NetEq error: could not set VAD mode, mode is not supported");
        return -1;
    }
    else
    {
        for(WebRtc_Word16 idx = 0; idx < _numSlaves + 1; idx++)
        {
            if(!_isInitialized[idx])
            {
                WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                    "SetVADMode: NetEq is not initialized.");
                return -1;
            }
            if(WebRtcNetEQ_SetVADMode(_inst[idx], mode) < 0)
            {
                LogError("SetVADmode", idx);
                return -1;
            }
        }
        _vadMode = mode;
        return 0;
    }
}


WebRtc_Word32
ACMNetEQ::FlushBuffers()
{
    CriticalSectionScoped lock(_netEqCritSect);
    for(WebRtc_Word16 idx = 0; idx < _numSlaves + 1; idx++)
    {
        if(!_isInitialized[idx])
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "FlushBuffers: NetEq is not initialized.");
            return -1;
        }
        if(WebRtcNetEQ_FlushBuffers(_inst[idx]) < 0)
        {
            LogError("FlushBuffers", idx);
            return -1;
        }
    }
    return 0;
}

WebRtc_Word16
ACMNetEQ::RemoveCodec(
    WebRtcNetEQDecoder codecIdx,
    bool               isStereo)
{
    // sanity check
    if((codecIdx <= kDecoderReservedStart) ||
        (codecIdx >= kDecoderReservedEnd))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "RemoveCodec: NetEq error: could not Remove Codec, codec index out of range");
        return -1;
    }
    CriticalSectionScoped lock(_netEqCritSect);
    if(!_isInitialized[0])
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "RemoveCodec: NetEq is not initialized.");
        return -1;
    }

    if(WebRtcNetEQ_CodecDbRemove(_inst[0], codecIdx) < 0)
    {
        LogError("CodecDB_Remove", 0);
        return -1;
    }

    if(isStereo)
    {
        if(WebRtcNetEQ_CodecDbRemove(_inst[1], codecIdx) < 0)
        {
            LogError("CodecDB_Remove", 1);
            return -1;
        }
    }

    return 0;
}

WebRtc_Word16
ACMNetEQ::SetBackgroundNoiseMode(
    const ACMBackgroundNoiseMode mode)
{
    CriticalSectionScoped lock(_netEqCritSect);
    for(WebRtc_Word16 idx = 0; idx < _numSlaves + 1; idx++)
    {
        if(!_isInitialized[idx])
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "SetBackgroundNoiseMode: NetEq is not initialized.");
            return -1;
        }
        if(WebRtcNetEQ_SetBGNMode(_inst[idx], (WebRtcNetEQBGNMode)mode) < 0)
        {
            LogError("SetBGNMode", idx);
            return -1;
        }
    }
    return 0;
}

WebRtc_Word16
ACMNetEQ::BackgroundNoiseMode(
    ACMBackgroundNoiseMode& mode)
{
    WebRtcNetEQBGNMode myMode;
    CriticalSectionScoped lock(_netEqCritSect);
    if(!_isInitialized[0])
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
            "BackgroundNoiseMode: NetEq is not initialized.");
        return -1;
    }
    if(WebRtcNetEQ_GetBGNMode(_inst[0], &myMode) < 0)
    {
        LogError("WebRtcNetEQ_GetBGNMode", 0);
        return -1;
    }
    else
    {
        mode = (ACMBackgroundNoiseMode)myMode;
    }
    return 0;
}

void 
ACMNetEQ::SetUniqueId(
    WebRtc_Word32 id)
{
    CriticalSectionScoped lock(_netEqCritSect);
    _id = id;
}


void
ACMNetEQ::LogError(
    const char* neteqFuncName,
    const WebRtc_Word16 idx) const
{
    char errorName[NETEQ_ERR_MSG_LEN_BYTE];
    char myFuncName[50];
    int neteqErrorCode = WebRtcNetEQ_GetErrorCode(_inst[idx]);
    WebRtcNetEQ_GetErrorName(neteqErrorCode, errorName, NETEQ_ERR_MSG_LEN_BYTE - 1);
    strncpy(myFuncName, neteqFuncName, 49);
    errorName[NETEQ_ERR_MSG_LEN_BYTE - 1] = '\0';
    myFuncName[49] = '\0';
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
        "NetEq-%d Error in function %s, error-code: %d, error-string: %s",
        idx,
        myFuncName,
        neteqErrorCode,
        errorName);
}


WebRtc_Word32
ACMNetEQ::PlayoutTimestamp(
    WebRtc_UWord32& timestamp)
{
    CriticalSectionScoped lock(_netEqCritSect);
    if(WebRtcNetEQ_GetSpeechTimeStamp(_inst[0], &timestamp) < 0)
    {
        LogError("GetSpeechTimeStamp", 0);
        return -1;
    }
    else
    {
        return 0;
    }
}

void ACMNetEQ::RemoveSlaves() {
  CriticalSectionScoped lock(_netEqCritSect);
  RemoveSlavesSafe();
}

void ACMNetEQ::RemoveSlavesSafe() {
  for (int i = 1; i < _numSlaves + 1; i++) {
    RemoveNetEQSafe(i);
  }

  if (_masterSlaveInfo != NULL) {
    free(_masterSlaveInfo);
    _masterSlaveInfo = NULL;
  }
  _numSlaves = 0;
}

void ACMNetEQ::RemoveNetEQSafe(int index) {
  if (_instMem[index] != NULL) {
    free(_instMem[index]);
    _instMem[index] = NULL;
  }
  if (_netEqPacketBuffer[index] != NULL) {
    free(_netEqPacketBuffer[index]);
    _netEqPacketBuffer[index] = NULL;
  }
  if (_ptrVADInst[index] != NULL) {
    WebRtcVad_Free(_ptrVADInst[index]);
    _ptrVADInst[index] = NULL;
  }
}

WebRtc_Word16
ACMNetEQ::AddSlave(
    const WebRtcNetEQDecoder* usedCodecs,
    WebRtc_Word16       noOfCodecs)
{
    CriticalSectionScoped lock(_netEqCritSect);
    const WebRtc_Word16 slaveIdx = 1;
    if(_numSlaves < 1)
    {
        // initialize the receiver, this also sets up VAD.
        if(InitByIdxSafe(slaveIdx) < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "AddSlave: AddSlave Failed, Could not Initialize");
            return -1;
        }

        // Allocate buffer.
        if(AllocatePacketBufferByIdxSafe(usedCodecs, noOfCodecs, slaveIdx) < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "AddSlave: AddSlave Failed, Could not Allocate Packet Buffer");
            return -1;
        }

        if(_masterSlaveInfo != NULL)
        {
            free(_masterSlaveInfo);
            _masterSlaveInfo = NULL;
        }
        int msInfoSize = WebRtcNetEQ_GetMasterSlaveInfoSize();
        _masterSlaveInfo = malloc(msInfoSize);

        if(_masterSlaveInfo == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "AddSlave: AddSlave Failed, Could not Allocate memory for Master-Slave Info");
            return -1;
        }

        // We accept this as initialized NetEQ, the rest is to synchronize
        // Slave with Master.
        _numSlaves = 1;
        _isInitialized[slaveIdx] = true;

        // Set Slave delay as all other instances.
        if(WebRtcNetEQ_SetExtraDelay(_inst[slaveIdx], _extraDelay) < 0)
        {
            LogError("SetExtraDelay", slaveIdx);
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "AddSlave: AddSlave Failed, Could not set delay");
            return -1;
        }

        // Set AVT
        if(WebRtcNetEQ_SetAVTPlayout(_inst[slaveIdx], (_avtPlayout) ? 1 : 0) < 0)
        {
            LogError("SetAVTPlayout", slaveIdx);
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "AddSlave: AddSlave Failed, Could not set AVT playout.");
            return -1;
        }

        // Set Background Noise
        WebRtcNetEQBGNMode currentMode;
        if(WebRtcNetEQ_GetBGNMode(_inst[0], &currentMode) < 0)
        {
            LogError("GetBGNMode", 0);
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "AAddSlave: AddSlave Failed, Could not Get BGN form Master.");
            return -1;
        }

        if(WebRtcNetEQ_SetBGNMode(_inst[slaveIdx], (WebRtcNetEQBGNMode)currentMode) < 0)
        {
            LogError("SetBGNMode", slaveIdx);
             WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "AddSlave: AddSlave Failed, Could not set BGN mode.");
           return -1;
        }

        enum WebRtcNetEQPlayoutMode playoutMode = kPlayoutOff;
        switch(_playoutMode)
        {
        case voice:
            playoutMode = kPlayoutOn;
            break;
        case fax:
            playoutMode = kPlayoutFax;
            break;
        case streaming:
            playoutMode = kPlayoutStreaming;
            break;
        }
        if(WebRtcNetEQ_SetPlayoutMode(_inst[slaveIdx], playoutMode) < 0)
        {
            LogError("SetPlayoutMode", 1);
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _id,
                "AddSlave: AddSlave Failed, Could not Set Playout Mode.");
            return -1;
        }
    }

    return 0;
}

void
ACMNetEQ::SetReceivedStereo(
    bool receivedStereo)
{
    CriticalSectionScoped lock(_netEqCritSect);
    _receivedStereo = receivedStereo;
}

WebRtc_UWord8
ACMNetEQ::NumSlaves()
{
    CriticalSectionScoped lock(_netEqCritSect);
    return _numSlaves;
}

} // namespace webrtc
