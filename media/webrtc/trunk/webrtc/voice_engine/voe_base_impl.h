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

#include "webrtc/voice_engine/include/voe_base.h"

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/voice_engine/shared_data.h"

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
                                uint32_t nSamples,
                                uint8_t nBytesPerSample,
                                uint8_t nChannels,
                                uint32_t samplesPerSec,
                                uint32_t totalDelayMS,
                                int32_t clockDrift,
                                uint32_t currentMicLevel,
                                bool keyPressed,
                                uint32_t& newMicLevel);

    virtual int32_t NeedMorePlayData(uint32_t nSamples,
                                     uint8_t nBytesPerSample,
                                     uint8_t nChannels,
                                     uint32_t samplesPerSec,
                                     void* audioSamples,
                                     uint32_t& nSamplesOut);

    virtual int OnDataAvailable(const int voe_channels[],
                                int number_of_voe_channels,
                                const int16_t* audio_data,
                                int sample_rate,
                                int number_of_channels,
                                int number_of_frames,
                                int audio_delay_milliseconds,
                                int current_volume,
                                bool key_pressed,
                                bool need_audio_processing);

    // AudioDeviceObserver
    virtual void OnErrorIsReported(ErrorCode error);
    virtual void OnWarningIsReported(WarningCode warning);

protected:
    VoEBaseImpl(voe::SharedData* shared);
    virtual ~VoEBaseImpl();

private:
    int32_t StartPlayout();
    int32_t StopPlayout();
    int32_t StartSend();
    int32_t StopSend();
    int32_t TerminateInternal();

    // Helper function to process the recorded data with AudioProcessing Module,
    // demultiplex the data to specific voe channels, encode and send to the
    // network. When |number_of_VoE_channels| is 0, it will demultiplex the
    // data to all the existing VoE channels.
    // It returns new AGC microphone volume or 0 if no volume changes
    // should be done.
    int ProcessRecordedDataWithAPM(const int voe_channels[],
                                   int number_of_voe_channels,
                                   const void* audio_data,
                                   uint32_t sample_rate,
                                   uint8_t number_of_channels,
                                   uint32_t number_of_frames,
                                   uint32_t audio_delay_milliseconds,
                                   int32_t clock_drift,
                                   uint32_t current_volume,
                                   bool key_pressed);

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

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_BASE_IMPL_H
