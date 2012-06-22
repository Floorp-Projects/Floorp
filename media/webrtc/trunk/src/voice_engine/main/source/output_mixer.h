/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_OUTPUT_MIXER_H
#define WEBRTC_VOICE_ENGINE_OUTPUT_MIXER_H

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
    static WebRtc_Word32 Create(OutputMixer*& mixer,
                                const WebRtc_UWord32 instanceId);

    static void Destroy(OutputMixer*& mixer);

    WebRtc_Word32 SetEngineInformation(Statistics& engineStatistics);

    WebRtc_Word32 SetAudioProcessingModule(
        AudioProcessing* audioProcessingModule);

    // VoEExternalMedia
    int RegisterExternalMediaProcessing(
        VoEMediaProcess& proccess_object);

    int DeRegisterExternalMediaProcessing();

    // VoEDtmf
    int PlayDtmfTone(WebRtc_UWord8 eventCode,
                     int lengthMs,
                     int attenuationDb);

    int StartPlayingDtmfTone(WebRtc_UWord8 eventCode,
                             int attenuationDb);

    int StopPlayingDtmfTone();

    WebRtc_Word32 MixActiveChannels();

    WebRtc_Word32 DoOperationsOnCombinedSignal();

    WebRtc_Word32 SetMixabilityStatus(MixerParticipant& participant,
                                      const bool mixable);

    WebRtc_Word32 SetAnonymousMixabilityStatus(MixerParticipant& participant,
                                               const bool mixable);

    WebRtc_Word32 GetMixedAudio(const WebRtc_Word32 desiredFreqHz,
                                const WebRtc_UWord8 channels,
                                AudioFrame& audioFrame);

    // VoEVolumeControl
    int GetSpeechOutputLevel(WebRtc_UWord32& level);

    int GetSpeechOutputLevelFullRange(WebRtc_UWord32& level);

    int SetOutputVolumePan(float left, float right);

    int GetOutputVolumePan(float& left, float& right);

    // VoEFile
    int StartRecordingPlayout(const char* fileName,
                              const CodecInst* codecInst);

    int StartRecordingPlayout(OutStream* stream,
                              const CodecInst* codecInst);
    int StopRecordingPlayout();

    virtual ~OutputMixer();

public:	// from AudioMixerOutputReceiver
    virtual void NewMixedAudio(
        const WebRtc_Word32 id,
        const AudioFrame& generalAudioFrame,
        const AudioFrame** uniqueAudioFrames,
        const WebRtc_UWord32 size);

public:  // from AudioMixerStatusReceiver
    virtual void MixedParticipants(
        const WebRtc_Word32 id,
        const ParticipantStatistics* participantStatistics,
        const WebRtc_UWord32 size);

    virtual void VADPositiveParticipants(
        const WebRtc_Word32 id,
        const ParticipantStatistics* participantStatistics,
        const WebRtc_UWord32 size);

    virtual void MixedAudioLevel(const WebRtc_Word32  id,
                                 const WebRtc_UWord32 level);

public: // For file recording
    void PlayNotification(const WebRtc_Word32 id,
                          const WebRtc_UWord32 durationMs);

    void RecordNotification(const WebRtc_Word32 id,
                            const WebRtc_UWord32 durationMs);

    void PlayFileEnded(const WebRtc_Word32 id);
    void RecordFileEnded(const WebRtc_Word32 id);

private:
    OutputMixer(const WebRtc_UWord32 instanceId);
    int APMAnalyzeReverseStream();
    int InsertInbandDtmfTone();

private:  // uses
    Statistics* _engineStatisticsPtr;
    AudioProcessing* _audioProcessingModulePtr;

private:  // owns
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

#endif // VOICE_ENGINE_OUTPUT_MIXER_H
