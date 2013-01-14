/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_BASE_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_BASE_IMPL_H

#include "voe_base.h"

#include "module_common_types.h"
#include "shared_data.h"

namespace webrtc
{

class ProcessThread;

class VoEBaseImpl: public VoEBase,
                   public AudioTransport,
                   public AudioDeviceObserver
{
public:
    virtual int RegisterVoiceEngineObserver(VoiceEngineObserver& observer);

    virtual int DeRegisterVoiceEngineObserver();

    virtual int Init(AudioDeviceModule* external_adm = NULL);

    virtual int Terminate();

    virtual int MaxNumOfChannels();

    virtual int CreateChannel();

    virtual int DeleteChannel(int channel);

    virtual int SetLocalReceiver(int channel, int port,
                                 int RTCPport = kVoEDefault,
                                 const char ipAddr[64] = NULL,
                                 const char multiCastAddr[64] = NULL);

    virtual int GetLocalReceiver(int channel, int& port, int& RTCPport,
                                 char ipAddr[64]);

    virtual int SetSendDestination(int channel, int port,
                                   const char ipAddr[64],
                                   int sourcePort = kVoEDefault,
                                   int RTCPport = kVoEDefault);

    virtual int GetSendDestination(int channel,
                                   int& port,
                                   char ipAddr[64],
                                   int& sourcePort,
                                   int& RTCPport);

    virtual int StartReceive(int channel);

    virtual int StartPlayout(int channel);

    virtual int StartSend(int channel);

    virtual int StopReceive(int channel);

    virtual int StopPlayout(int channel);

    virtual int StopSend(int channel);

    virtual int SetNetEQPlayoutMode(int channel, NetEqModes mode);

    virtual int GetNetEQPlayoutMode(int channel, NetEqModes& mode);

    virtual int SetNetEQBGNMode(int channel, NetEqBgnModes mode);

    virtual int GetNetEQBGNMode(int channel, NetEqBgnModes& mode);


    virtual int SetOnHoldStatus(int channel,
                                bool enable,
                                OnHoldModes mode = kHoldSendAndPlay);

    virtual int GetOnHoldStatus(int channel, bool& enabled, OnHoldModes& mode);

    virtual int GetVersion(char version[1024]);

    virtual int LastError();

    // AudioTransport
    virtual WebRtc_Word32
        RecordedDataIsAvailable(const void* audioSamples,
                                const WebRtc_UWord32 nSamples,
                                const WebRtc_UWord8 nBytesPerSample,
                                const WebRtc_UWord8 nChannels,
                                const WebRtc_UWord32 samplesPerSec,
                                const WebRtc_UWord32 totalDelayMS,
                                const WebRtc_Word32 clockDrift,
                                const WebRtc_UWord32 currentMicLevel,
                                WebRtc_UWord32& newMicLevel);

    virtual WebRtc_Word32 NeedMorePlayData(const WebRtc_UWord32 nSamples,
                                           const WebRtc_UWord8 nBytesPerSample,
                                           const WebRtc_UWord8 nChannels,
                                           const WebRtc_UWord32 samplesPerSec,
                                           void* audioSamples,
                                           WebRtc_UWord32& nSamplesOut);

    // AudioDeviceObserver
    virtual void OnErrorIsReported(const ErrorCode error);
    virtual void OnWarningIsReported(const WarningCode warning);

protected:
    VoEBaseImpl(voe::SharedData* shared);
    virtual ~VoEBaseImpl();

private:
    WebRtc_Word32 StartPlayout();
    WebRtc_Word32 StopPlayout();
    WebRtc_Word32 StartSend();
    WebRtc_Word32 StopSend();
    WebRtc_Word32 TerminateInternal();

    WebRtc_Word32 AddBuildInfo(char* str) const;
    WebRtc_Word32 AddVoEVersion(char* str) const;
#ifdef WEBRTC_EXTERNAL_TRANSPORT
    WebRtc_Word32 AddExternalTransportBuild(char* str) const;
#endif
#ifdef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
    WebRtc_Word32 AddExternalRecAndPlayoutBuild(char* str) const;
#endif
    VoiceEngineObserver* _voiceEngineObserverPtr;
    CriticalSectionWrapper& _callbackCritSect;

    bool _voiceEngineObserver;
    WebRtc_UWord32 _oldVoEMicLevel;
    WebRtc_UWord32 _oldMicLevel;
    AudioFrame _audioFrame;
    voe::SharedData* _shared;

};

} // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_BASE_IMPL_H
