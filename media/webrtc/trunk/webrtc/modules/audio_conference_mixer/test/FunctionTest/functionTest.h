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

#include "module_common_types.h"
#include "level_indicator.h"
#include "list_wrapper.h"
#include "map_wrapper.h"
#include "audio_conference_mixer.h"
#include "audio_conference_mixer_defines.h"
#include "tick_util.h"

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
        const WebRtc_Word32 samples);

    bool EnableAutomaticVAD(
        bool enable,
        int mode);

    bool SetVAD(
        bool vad);
private:
    bool GetVAD(
        WebRtc_Word16* buffer,
        WebRtc_UWord8 bufferLengthInSamples,
        bool& vad);

    Frequency       _frequency;
    WebRtc_UWord8     _sampleSize;

    WebRtc_UWord32 _timeStamp;

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
        const WebRtc_UWord32 id,
        ParticipantType participantType,
        const WebRtc_Word32 startPosition,
        char* outputPath);
    ~MixerParticipant();

    WebRtc_Word32 GetAudioFrame(
        const WebRtc_Word32 id,
        AudioFrame& audioFrame);

    WebRtc_Word32 MixedAudioFrame(
        const AudioFrame& audioFrame);

    WebRtc_Word32 GetParticipantType(
        ParticipantType& participantType);
private:
    MixerParticipant(
        const WebRtc_UWord32 id,
        ParticipantType participantType);

    bool InitializeFileReader(
        const WebRtc_Word32 startPositionInSamples);

    bool InitializeFileWriter(
        char* outputPath);

    WebRtc_UWord32 _id;
    ParticipantType _participantType;

    FileReader _fileReader;
    FileWriter _fileWriter;
};

class StatusReceiver : public AudioMixerStatusReceiver
{
public:
    StatusReceiver(
        const WebRtc_Word32 id);
    ~StatusReceiver();

    void MixedParticipants(
        const WebRtc_Word32 id,
        const ParticipantStatistics* participantStatistics,
        const WebRtc_UWord32 size);

    void VADPositiveParticipants(
        const WebRtc_Word32 id,
        const ParticipantStatistics* participantStatistics,
        const WebRtc_UWord32 size);

    void MixedAudioLevel(
        const WebRtc_Word32 id,
        const WebRtc_UWord32 level);

    void PrintMixedParticipants();

    void PrintVadPositiveParticipants();

    void PrintMixedAudioLevel();
private:
    WebRtc_Word32 _id;

    ParticipantStatistics*  _mixedParticipants;
    WebRtc_UWord32                _mixedParticipantsAmount;
    WebRtc_UWord32                _mixedParticipantsSize;

    ParticipantStatistics*  _vadPositiveParticipants;
    WebRtc_UWord32                _vadPositiveParticipantsAmount;
    WebRtc_UWord32                _vadPositiveParticipantsSize;

    WebRtc_UWord32 _mixedAudioLevel;
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
        const WebRtc_Word32 startPosition);

    bool DeleteParticipant(
        const WebRtc_UWord32 id);

    bool StartMixing(
        const WebRtc_UWord32 mixedParticipants = AudioConferenceMixer::kDefaultAmountOfMixedParticipants);

    bool StopMixing();

    void NewMixedAudio(
        const WebRtc_Word32 id,
        const AudioFrame& generalAudioFrame,
        const AudioFrame** uniqueAudioFrames,
        const WebRtc_UWord32 size);

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
        const WebRtc_UWord32 id);

    bool StopMixingParticipant(
        const WebRtc_UWord32 id);

    bool GetFreeItemIds(
        WebRtc_UWord32& itemId);

    void AddFreeItemIds(
        const WebRtc_UWord32 itemId);

    void ClearAllItemIds();

    webrtc::ThreadWrapper*  _processThread;
    unsigned int _threadId;

    // Performance hooks
    enum{WARNING_COUNTER = 100};

    bool _firstProcessCall;
    TickTime _previousTime;             // Tick time of previous process
    const WebRtc_Word64  _periodicityInTicks; // Periodicity

    webrtc::EventWrapper*  _synchronizationEvent;

    ListWrapper        _freeItemIds;
    WebRtc_UWord32    _itemIdCounter;

    MapWrapper _mixerParticipants;

    static WebRtc_Word32 _mixerWrapperIdCounter;
    WebRtc_Word32 _mixerWrappererId;
    char _instanceOutputPath[128];

    webrtc::Trace* _trace;
    AudioConferenceMixer* _mixer;

    StatusReceiver _statusReceiver;

    FileWriter _generalAudioWriter;
};

bool
LoopedFileRead(
    WebRtc_Word16* buffer,
    WebRtc_UWord32 bufferSizeInSamples,
    WebRtc_UWord32 samplesToRead,
    FILE* file);

void
GenerateRandomPosition(
    WebRtc_Word32& startPosition);

#endif // WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_TEST_FUNCTIONTEST_FUNCTIONTEST_H_
