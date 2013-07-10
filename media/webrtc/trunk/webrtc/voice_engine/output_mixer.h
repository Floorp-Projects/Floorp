/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_OUTPUT_MIXER_H_
#define WEBRTC_VOICE_ENGINE_OUTPUT_MIXER_H_

#include "audio_conference_mixer.h"
#include "audio_conference_mixer_defines.h"
#include "common_types.h"
#include "dtmf_inband.h"
#include "file_recorder.h"
#include "level_indicator.h"
#include "resampler.h"
#include "voice_engine_defines.h"

namespace webrtc {

class AudioProcessing;
class CriticalSectionWrapper;
class FileWrapper;
class VoEMediaProcess;

namespace voe {

class Statistics;

class OutputMixer : public AudioMixerOutputReceiver,
                    public AudioMixerStatusReceiver,
                    public FileCallback
{
public:
    static int32_t Create(OutputMixer*& mixer, const uint32_t instanceId);

    static void Destroy(OutputMixer*& mixer);

    int32_t SetEngineInformation(Statistics& engineStatistics);

    int32_t SetAudioProcessingModule(
        AudioProcessing* audioProcessingModule);

    // VoEExternalMedia
    int RegisterExternalMediaProcessing(
        VoEMediaProcess& proccess_object);

    int DeRegisterExternalMediaProcessing();

    // VoEDtmf
    int PlayDtmfTone(uint8_t eventCode, int lengthMs, int attenuationDb);

    int StartPlayingDtmfTone(uint8_t eventCode, int attenuationDb);

    int StopPlayingDtmfTone();

    int32_t MixActiveChannels();

    int32_t DoOperationsOnCombinedSignal();

    int32_t SetMixabilityStatus(MixerParticipant& participant,
                                const bool mixable);

    int32_t SetAnonymousMixabilityStatus(MixerParticipant& participant,
                                         const bool mixable);

    int GetMixedAudio(int sample_rate_hz, int num_channels,
                      AudioFrame* audioFrame);

    // VoEVolumeControl
    int GetSpeechOutputLevel(uint32_t& level);

    int GetSpeechOutputLevelFullRange(uint32_t& level);

    int SetOutputVolumePan(float left, float right);

    int GetOutputVolumePan(float& left, float& right);

    // VoEFile
    int StartRecordingPlayout(const char* fileName,
                              const CodecInst* codecInst);

    int StartRecordingPlayout(OutStream* stream,
                              const CodecInst* codecInst);
    int StopRecordingPlayout();

    virtual ~OutputMixer();

    // from AudioMixerOutputReceiver
    virtual void NewMixedAudio(
        const int32_t id,
        const AudioFrame& generalAudioFrame,
        const AudioFrame** uniqueAudioFrames,
        const uint32_t size);

    // from AudioMixerStatusReceiver
    virtual void MixedParticipants(
        const int32_t id,
        const ParticipantStatistics* participantStatistics,
        const uint32_t size);

    virtual void VADPositiveParticipants(
        const int32_t id,
        const ParticipantStatistics* participantStatistics,
        const uint32_t size);

    virtual void MixedAudioLevel(const int32_t id, const uint32_t level);

    // For file recording
    void PlayNotification(const int32_t id, const uint32_t durationMs);

    void RecordNotification(const int32_t id, const uint32_t durationMs);

    void PlayFileEnded(const int32_t id);
    void RecordFileEnded(const int32_t id);

private:
    OutputMixer(const uint32_t instanceId);
    void APMAnalyzeReverseStream();
    int InsertInbandDtmfTone();

    // uses
    Statistics* _engineStatisticsPtr;
    AudioProcessing* _audioProcessingModulePtr;

    // owns
    CriticalSectionWrapper& _callbackCritSect;
    // protect the _outputFileRecorderPtr and _outputFileRecording
    CriticalSectionWrapper& _fileCritSect;
    AudioConferenceMixer& _mixerModule;
    AudioFrame _audioFrame;
    Resampler _resampler;        // converts mixed audio to fit ADM format
    Resampler _apmResampler;    // converts mixed audio to fit APM rate
    AudioLevel _audioLevel;    // measures audio level for the combined signal
    DtmfInband _dtmfGenerator;
    int _instanceId;
    VoEMediaProcess* _externalMediaCallbackPtr;
    bool _externalMedia;
    float _panLeft;
    float _panRight;
    int _mixingFrequencyHz;
    FileRecorder* _outputFileRecorderPtr;
    bool _outputFileRecording;
};

}  //  namespace voe

}  //  namespace werbtc

#endif  // VOICE_ENGINE_OUTPUT_MIXER_H_
