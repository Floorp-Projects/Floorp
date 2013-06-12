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

    virtual int Init(AudioDeviceModule* external_adm = NULL,
                     AudioProcessing* audioproc = NULL);
    virtual AudioProcessing* audio_processing() {
      return _shared->audio_processing();
    }

    virtual int Terminate();

    virtual int MaxNumOfChannels();

    virtual int CreateChannel();

    virtual int DeleteChannel(int channel);

    virtual int StartReceive(int channel);

    virtual int StartPlayout(int channel);

    virtual int StartSend(int channel);

    virtual int StopReceive(int channel);

    virtual int StopPlayout(int channel);

    virtual int StopSend(int channel);

    virtual int SetNetEQPlayoutMode(int channel, NetEqModes mode);

    virtual int GetNetEQPlayoutMode(int channel, NetEqModes& mode);

    virtual int SetOnHoldStatus(int channel,
                                bool enable,
                                OnHoldModes mode = kHoldSendAndPlay);

    virtual int GetOnHoldStatus(int channel, bool& enabled, OnHoldModes& mode);

    virtual int GetVersion(char version[1024]);

    virtual int LastError();

    // AudioTransport
    virtual int32_t
        RecordedDataIsAvailable(const void* audioSamples,
                                const uint32_t nSamples,
                                const uint8_t nBytesPerSample,
                                const uint8_t nChannels,
                                const uint32_t samplesPerSec,
                                const uint32_t totalDelayMS,
                                const int32_t clockDrift,
                                const uint32_t currentMicLevel,
                                uint32_t& newMicLevel);

    virtual int32_t NeedMorePlayData(const uint32_t nSamples,
                                     const uint8_t nBytesPerSample,
                                     const uint8_t nChannels,
                                     const uint32_t samplesPerSec,
                                     void* audioSamples,
                                     uint32_t& nSamplesOut);

    // AudioDeviceObserver
    virtual void OnErrorIsReported(const ErrorCode error);
    virtual void OnWarningIsReported(const WarningCode warning);

protected:
    VoEBaseImpl(voe::SharedData* shared);
    virtual ~VoEBaseImpl();

private:
    int32_t StartPlayout();
    int32_t StopPlayout();
    int32_t StartSend();
    int32_t StopSend();
    int32_t TerminateInternal();

    int32_t AddBuildInfo(char* str) const;
    int32_t AddVoEVersion(char* str) const;
#ifdef WEBRTC_EXTERNAL_TRANSPORT
    int32_t AddExternalTransportBuild(char* str) const;
#endif
#ifdef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
    int32_t AddExternalRecAndPlayoutBuild(char* str) const;
#endif
    VoiceEngineObserver* _voiceEngineObserverPtr;
    CriticalSectionWrapper& _callbackCritSect;

    bool _voiceEngineObserver;
    uint32_t _oldVoEMicLevel;
    uint32_t _oldMicLevel;
    AudioFrame _audioFrame;
    voe::SharedData* _shared;

};

} // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_BASE_IMPL_H
