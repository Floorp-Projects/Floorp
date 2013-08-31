/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_TEST_FUNCTIONTEST_FUNCTIONTEST_H_
#define WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_TEST_FUNCTIONTEST_FUNCTIONTEST_H_

#include "webrtc/modules/audio_conference_mixer/interface/audio_conference_mixer.h"
#include "webrtc/modules/audio_conference_mixer/interface/audio_conference_mixer_defines.h"
#include "webrtc/modules/audio_conference_mixer/source/level_indicator.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/list_wrapper.h"
#include "webrtc/system_wrappers/interface/map_wrapper.h"
#include "webrtc/system_wrappers/interface/tick_util.h"

namespace webrtc {
class EventWrapper;
class ThreadWrapper;
class Trace;
}
struct WebRtcVadInst;

class FileWriter
{
public:
    FileWriter();
    ~FileWriter();

    bool SetFileName(
        const char* fileName);

    bool WriteToFile(
        const AudioFrame& audioFrame);
private:
    FILE* _file;
};

class FileReader
{
public:
    enum {kProcessPeriodicityInMs = 10};
    enum Frequency
    {
        kNbInHz          = 8000,
        kWbInHz          = 16000,
        kDefaultFrequency = kWbInHz
    };

    FileReader();
    ~FileReader();

    bool SetFileName(
        const char* fileName);

    bool ReadFromFile(
        AudioFrame& audioFrame);

    bool FastForwardFile(
        const int32_t samples);

    bool EnableAutomaticVAD(
        bool enable,
        int mode);

    bool SetVAD(
        bool vad);
private:
    bool GetVAD(
        int16_t* buffer,
        uint8_t bufferLengthInSamples,
        bool& vad);

    Frequency       _frequency;
    uint8_t     _sampleSize;

    uint32_t _timeStamp;

    FILE* _file;

    WebRtcVadInst* _vadInstr;
    bool  _automaticVad;
    bool  _vad;

    LevelIndicator _volumeCalculator;
};

class MixerParticipant : public MixerParticipant
{
public:
    enum ParticipantType
    {
        VIP             = 0,
        REGULAR         = 1,
        MIXED_ANONYMOUS = 2,
        RANDOM          = 3
    };

    static MixerParticipant* CreateParticipant(
        const uint32_t id,
        ParticipantType participantType,
        const int32_t startPosition,
        char* outputPath);
    ~MixerParticipant();

    int32_t GetAudioFrame(
        const int32_t id,
        AudioFrame& audioFrame);

    int32_t MixedAudioFrame(
        const AudioFrame& audioFrame);

    int32_t GetParticipantType(
        ParticipantType& participantType);
private:
    MixerParticipant(
        const uint32_t id,
        ParticipantType participantType);

    bool InitializeFileReader(
        const int32_t startPositionInSamples);

    bool InitializeFileWriter(
        char* outputPath);

    uint32_t _id;
    ParticipantType _participantType;

    FileReader _fileReader;
    FileWriter _fileWriter;
};

class StatusReceiver : public AudioMixerStatusReceiver
{
public:
    StatusReceiver(
        const int32_t id);
    ~StatusReceiver();

    void MixedParticipants(
        const int32_t id,
        const ParticipantStatistics* participantStatistics,
        const uint32_t size);

    void VADPositiveParticipants(
        const int32_t id,
        const ParticipantStatistics* participantStatistics,
        const uint32_t size);

    void MixedAudioLevel(
        const int32_t id,
        const uint32_t level);

    void PrintMixedParticipants();

    void PrintVadPositiveParticipants();

    void PrintMixedAudioLevel();
private:
    int32_t _id;

    ParticipantStatistics*  _mixedParticipants;
    uint32_t                _mixedParticipantsAmount;
    uint32_t                _mixedParticipantsSize;

    ParticipantStatistics*  _vadPositiveParticipants;
    uint32_t                _vadPositiveParticipantsAmount;
    uint32_t                _vadPositiveParticipantsSize;

    uint32_t _mixedAudioLevel;
};

class MixerWrapper : public AudioMixerOutputReceiver
{
public:
    static MixerWrapper* CreateMixerWrapper();
    ~MixerWrapper();

    bool SetMixFrequency(
        const AudioConferenceMixer::Frequency frequency);

    bool CreateParticipant(
        MixerParticipant::ParticipantType participantType);

    bool CreateParticipant(
        MixerParticipant::ParticipantType participantType,
        const int32_t startPosition);

    bool DeleteParticipant(
        const uint32_t id);

    bool StartMixing(
        const uint32_t mixedParticipants = AudioConferenceMixer::kDefaultAmountOfMixedParticipants);

    bool StopMixing();

    void NewMixedAudio(
        const int32_t id,
        const AudioFrame& generalAudioFrame,
        const AudioFrame** uniqueAudioFrames,
        const uint32_t size);

    bool GetParticipantList(
        ListWrapper& participants);

    void PrintStatus();
private:
    MixerWrapper();

    bool InitializeFileWriter();

    static bool Process(
        void* instance);

    bool Process();

    bool StartMixingParticipant(
        const uint32_t id);

    bool StopMixingParticipant(
        const uint32_t id);

    bool GetFreeItemIds(
        uint32_t& itemId);

    void AddFreeItemIds(
        const uint32_t itemId);

    void ClearAllItemIds();

    webrtc::ThreadWrapper*  _processThread;
    unsigned int _threadId;

    // Performance hooks
    enum{WARNING_COUNTER = 100};

    bool _firstProcessCall;
    TickTime _previousTime;             // Tick time of previous process
    const int64_t  _periodicityInTicks; // Periodicity

    webrtc::EventWrapper*  _synchronizationEvent;

    ListWrapper        _freeItemIds;
    uint32_t    _itemIdCounter;

    MapWrapper _mixerParticipants;

    static int32_t _mixerWrapperIdCounter;
    int32_t _mixerWrappererId;
    char _instanceOutputPath[128];

    webrtc::Trace* _trace;
    AudioConferenceMixer* _mixer;

    StatusReceiver _statusReceiver;

    FileWriter _generalAudioWriter;
};

bool
LoopedFileRead(
    int16_t* buffer,
    uint32_t bufferSizeInSamples,
    uint32_t samplesToRead,
    FILE* file);

void
GenerateRandomPosition(
    int32_t& startPosition);

#endif // WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_TEST_FUNCTIONTEST_FUNCTIONTEST_H_
