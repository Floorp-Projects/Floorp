/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_AUDIO_CONFERENCE_MIXER_IMPL_H_
#define WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_AUDIO_CONFERENCE_MIXER_IMPL_H_

#include <list>
#include <map>

#include "webrtc/engine_configurations.h"
#include "webrtc/modules/audio_conference_mixer/interface/audio_conference_mixer.h"
#include "webrtc/modules/audio_conference_mixer/source/level_indicator.h"
#include "webrtc/modules/audio_conference_mixer/source/memory_pool.h"
#include "webrtc/modules/audio_conference_mixer/source/time_scheduler.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {
class AudioProcessing;
class CriticalSectionWrapper;

typedef std::list<AudioFrame*> AudioFrameList;
typedef std::list<MixerParticipant*> MixerParticipantList;

// Cheshire cat implementation of MixerParticipant's non virtual functions.
class MixHistory
{
public:
    MixHistory();
    ~MixHistory();

    // MixerParticipant function
    int32_t IsMixed(bool& mixed) const;

    // Sets wasMixed to true if the participant was mixed previous mix
    // iteration.
    int32_t WasMixed(bool& wasMixed) const;

    // Updates the mixed status.
    int32_t SetIsMixed(const bool mixed);

    void ResetMixedStatus();
private:
    bool _isMixed;
};

class AudioConferenceMixerImpl : public AudioConferenceMixer
{
public:
    // AudioProcessing only accepts 10 ms frames.
    enum {kProcessPeriodicityInMs = 10};

    AudioConferenceMixerImpl(int id);
    ~AudioConferenceMixerImpl();

    // Must be called after ctor.
    bool Init();

    // Module functions
    virtual int32_t ChangeUniqueId(const int32_t id) OVERRIDE;
    virtual int32_t TimeUntilNextProcess() OVERRIDE;
    virtual int32_t Process() OVERRIDE;

    // AudioConferenceMixer functions
    virtual int32_t RegisterMixedStreamCallback(
        AudioMixerOutputReceiver& mixReceiver) OVERRIDE;
    virtual int32_t UnRegisterMixedStreamCallback() OVERRIDE;
    virtual int32_t RegisterMixerStatusCallback(
        AudioMixerStatusReceiver& mixerStatusCallback,
        const uint32_t amountOf10MsBetweenCallbacks) OVERRIDE;
    virtual int32_t UnRegisterMixerStatusCallback() OVERRIDE;
    virtual int32_t SetMixabilityStatus(MixerParticipant& participant,
                                        bool mixable) OVERRIDE;
    virtual int32_t MixabilityStatus(MixerParticipant& participant,
                                     bool& mixable) OVERRIDE;
    virtual int32_t SetMinimumMixingFrequency(Frequency freq) OVERRIDE;
    virtual int32_t SetAnonymousMixabilityStatus(
        MixerParticipant& participant, const bool mixable) OVERRIDE;
    virtual int32_t AnonymousMixabilityStatus(
        MixerParticipant& participant, bool& mixable) OVERRIDE;
private:
    enum{DEFAULT_AUDIO_FRAME_POOLSIZE = 50};

    // Set/get mix frequency
    int32_t SetOutputFrequency(const Frequency frequency);
    Frequency OutputFrequency() const;

    // Fills mixList with the AudioFrames pointers that should be used when
    // mixing. Fills mixParticipantList with ParticipantStatistics for the
    // participants who's AudioFrames are inside mixList.
    // maxAudioFrameCounter both input and output specifies how many more
    // AudioFrames that are allowed to be mixed.
    // rampOutList contain AudioFrames corresponding to an audio stream that
    // used to be mixed but shouldn't be mixed any longer. These AudioFrames
    // should be ramped out over this AudioFrame to avoid audio discontinuities.
    void UpdateToMix(
        AudioFrameList* mixList,
        AudioFrameList* rampOutList,
        std::map<int, MixerParticipant*>* mixParticipantList,
        size_t& maxAudioFrameCounter);

    // Return the lowest mixing frequency that can be used without having to
    // downsample any audio.
    int32_t GetLowestMixingFrequency();
    int32_t GetLowestMixingFrequencyFromList(MixerParticipantList* mixList);

    // Return the AudioFrames that should be mixed anonymously.
    void GetAdditionalAudio(AudioFrameList* additionalFramesList);

    // Update the MixHistory of all MixerParticipants. mixedParticipantsList
    // should contain a map of MixerParticipants that have been mixed.
    void UpdateMixedStatus(
        std::map<int, MixerParticipant*>& mixedParticipantsList);

    // Clears audioFrameList and reclaims all memory associated with it.
    void ClearAudioFrameList(AudioFrameList* audioFrameList);

    // Update the list of MixerParticipants who have a positive VAD. mixList
    // should be a list of AudioFrames
    void UpdateVADPositiveParticipants(
        AudioFrameList* mixList);

    // This function returns true if it finds the MixerParticipant in the
    // specified list of MixerParticipants.
    bool IsParticipantInList(
        MixerParticipant& participant,
        MixerParticipantList* participantList) const;

    // Add/remove the MixerParticipant to the specified
    // MixerParticipant list.
    bool AddParticipantToList(
        MixerParticipant& participant,
        MixerParticipantList* participantList);
    bool RemoveParticipantFromList(
        MixerParticipant& removeParticipant,
        MixerParticipantList* participantList);

    // Mix the AudioFrames stored in audioFrameList into mixedAudio.
    int32_t MixFromList(
        AudioFrame& mixedAudio,
        const AudioFrameList* audioFrameList);
    // Mix the AudioFrames stored in audioFrameList into mixedAudio. No
    // record will be kept of this mix (e.g. the corresponding MixerParticipants
    // will not be marked as IsMixed()
    int32_t MixAnonomouslyFromList(AudioFrame& mixedAudio,
                                   const AudioFrameList* audioFrameList);

    bool LimitMixedAudio(AudioFrame& mixedAudio);

    // Scratch memory
    // Note that the scratch memory may only be touched in the scope of
    // Process().
    size_t         _scratchParticipantsToMixAmount;
    ParticipantStatistics  _scratchMixedParticipants[
        kMaximumAmountOfMixedParticipants];
    uint32_t         _scratchVadPositiveParticipantsAmount;
    ParticipantStatistics  _scratchVadPositiveParticipants[
        kMaximumAmountOfMixedParticipants];

    scoped_ptr<CriticalSectionWrapper> _crit;
    scoped_ptr<CriticalSectionWrapper> _cbCrit;

    int32_t _id;

    Frequency _minimumMixingFreq;

    // Mix result callback
    AudioMixerOutputReceiver* _mixReceiver;

    AudioMixerStatusReceiver* _mixerStatusCallback;
    uint32_t _amountOf10MsBetweenCallbacks;
    uint32_t _amountOf10MsUntilNextCallback;
    bool _mixerStatusCb;

    // The current sample frequency and sample size when mixing.
    Frequency _outputFrequency;
    uint16_t _sampleSize;

    // Memory pool to avoid allocating/deallocating AudioFrames
    MemoryPool<AudioFrame>* _audioFramePool;

    // List of all participants. Note all lists are disjunct
    MixerParticipantList _participantList;              // May be mixed.
    // Always mixed, anonomously.
    MixerParticipantList _additionalParticipantList;

    size_t _numMixedParticipants;
    // Determines if we will use a limiter for clipping protection during
    // mixing.
    bool use_limiter_;

    uint32_t _timeStamp;

    // Metronome class.
    TimeScheduler _timeScheduler;

    // Smooth level indicator.
    LevelIndicator _mixedAudioLevel;

    // Counter keeping track of concurrent calls to process.
    // Note: should never be higher than 1 or lower than 0.
    int16_t _processCalls;

    // Used for inhibiting saturation in mixing.
    scoped_ptr<AudioProcessing> _limiter;
};
}  // namespace webrtc

#endif // WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_AUDIO_CONFERENCE_MIXER_IMPL_H_
