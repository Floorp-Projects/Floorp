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

#include "audio_conference_mixer.h"
#include "engine_configurations.h"
#include "level_indicator.h"
#include "list_wrapper.h"
#include "memory_pool.h"
#include "module_common_types.h"
#include "scoped_ptr.h"
#include "time_scheduler.h"

namespace webrtc {
class AudioProcessing;
class CriticalSectionWrapper;

// Cheshire cat implementation of MixerParticipant's non virtual functions.
class MixHistory
{
public:
    MixHistory();
    ~MixHistory();

    // MixerParticipant function
    WebRtc_Word32 IsMixed(bool& mixed) const;

    // Sets wasMixed to true if the participant was mixed previous mix
    // iteration.
    WebRtc_Word32 WasMixed(bool& wasMixed) const;

    // Updates the mixed status.
    WebRtc_Word32 SetIsMixed(const bool mixed);

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
    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);
    virtual WebRtc_Word32 TimeUntilNextProcess();
    virtual WebRtc_Word32 Process();

    // AudioConferenceMixer functions
    virtual WebRtc_Word32 RegisterMixedStreamCallback(
        AudioMixerOutputReceiver& mixReceiver);
    virtual WebRtc_Word32 UnRegisterMixedStreamCallback();
    virtual WebRtc_Word32 RegisterMixerStatusCallback(
        AudioMixerStatusReceiver& mixerStatusCallback,
        const WebRtc_UWord32 amountOf10MsBetweenCallbacks);
    virtual WebRtc_Word32 UnRegisterMixerStatusCallback();
    virtual WebRtc_Word32 SetMixabilityStatus(MixerParticipant& participant,
                                              const bool mixable);
    virtual WebRtc_Word32 MixabilityStatus(MixerParticipant& participant,
                                           bool& mixable);
    virtual WebRtc_Word32 SetMinimumMixingFrequency(Frequency freq);
    virtual WebRtc_Word32 SetAnonymousMixabilityStatus(
        MixerParticipant& participant, const bool mixable);
    virtual WebRtc_Word32 AnonymousMixabilityStatus(
        MixerParticipant& participant, bool& mixable);
private:
    enum{DEFAULT_AUDIO_FRAME_POOLSIZE = 50};

    // Set/get mix frequency
    WebRtc_Word32 SetOutputFrequency(const Frequency frequency);
    Frequency OutputFrequency() const;

    // Must be called whenever an audio frame indicates the number of channels
    // has changed.
    bool SetNumLimiterChannels(int numChannels);

    // Fills mixList with the AudioFrames pointers that should be used when
    // mixing. Fills mixParticipantList with ParticipantStatistics for the
    // participants who's AudioFrames are inside mixList.
    // maxAudioFrameCounter both input and output specifies how many more
    // AudioFrames that are allowed to be mixed.
    // rampOutList contain AudioFrames corresponding to an audio stream that
    // used to be mixed but shouldn't be mixed any longer. These AudioFrames
    // should be ramped out over this AudioFrame to avoid audio discontinuities.
    void UpdateToMix(ListWrapper& mixList, ListWrapper& rampOutList,
                     MapWrapper& mixParticipantList,
                     WebRtc_UWord32& maxAudioFrameCounter);

    // Return the lowest mixing frequency that can be used without having to
    // downsample any audio.
    WebRtc_Word32 GetLowestMixingFrequency();
    WebRtc_Word32 GetLowestMixingFrequencyFromList(ListWrapper& mixList);

    // Return the AudioFrames that should be mixed anonymously.
    void GetAdditionalAudio(ListWrapper& additionalFramesList);

    // Update the MixHistory of all MixerParticipants. mixedParticipantsList
    // should contain a map of MixerParticipants that have been mixed.
    void UpdateMixedStatus(MapWrapper& mixedParticipantsList);

    // Clears audioFrameList and reclaims all memory associated with it.
    void ClearAudioFrameList(ListWrapper& audioFrameList);

    // Update the list of MixerParticipants who have a positive VAD. mixList
    // should be a list of AudioFrames
    void UpdateVADPositiveParticipants(
        ListWrapper& mixList);

    // This function returns true if it finds the MixerParticipant in the
    // specified list of MixerParticipants.
    bool IsParticipantInList(
        MixerParticipant& participant,
        ListWrapper& participantList);

    // Add/remove the MixerParticipant to the specified
    // MixerParticipant list.
    bool AddParticipantToList(
        MixerParticipant& participant,
        ListWrapper& participantList);
    bool RemoveParticipantFromList(
        MixerParticipant& removeParticipant,
        ListWrapper& participantList);

    // Mix the AudioFrames stored in audioFrameList into mixedAudio.
    WebRtc_Word32 MixFromList(
        AudioFrame& mixedAudio,
        const ListWrapper& audioFrameList);
    // Mix the AudioFrames stored in audioFrameList into mixedAudio. No
    // record will be kept of this mix (e.g. the corresponding MixerParticipants
    // will not be marked as IsMixed()
    WebRtc_Word32 MixAnonomouslyFromList(AudioFrame& mixedAudio,
                                         const ListWrapper& audioFrameList);

    bool LimitMixedAudio(AudioFrame& mixedAudio);

    // Scratch memory
    // Note that the scratch memory may only be touched in the scope of
    // Process().
    WebRtc_UWord32         _scratchParticipantsToMixAmount;
    ParticipantStatistics  _scratchMixedParticipants[
        kMaximumAmountOfMixedParticipants];
    WebRtc_UWord32         _scratchVadPositiveParticipantsAmount;
    ParticipantStatistics  _scratchVadPositiveParticipants[
        kMaximumAmountOfMixedParticipants];

    scoped_ptr<CriticalSectionWrapper> _crit;
    scoped_ptr<CriticalSectionWrapper> _cbCrit;

    WebRtc_Word32 _id;

    Frequency _minimumMixingFreq;

    // Mix result callback
    AudioMixerOutputReceiver* _mixReceiver;

    AudioMixerStatusReceiver* _mixerStatusCallback;
    WebRtc_UWord32            _amountOf10MsBetweenCallbacks;
    WebRtc_UWord32            _amountOf10MsUntilNextCallback;
    bool                      _mixerStatusCb;

    // The current sample frequency and sample size when mixing.
    Frequency _outputFrequency;
    WebRtc_UWord16 _sampleSize;

    // Memory pool to avoid allocating/deallocating AudioFrames
    MemoryPool<AudioFrame>* _audioFramePool;

    // List of all participants. Note all lists are disjunct
    ListWrapper _participantList;              // May be mixed.
    ListWrapper _additionalParticipantList;    // Always mixed, anonomously.

    WebRtc_UWord32 _numMixedParticipants;

    WebRtc_UWord32 _timeStamp;

    // Metronome class.
    TimeScheduler _timeScheduler;

    // Smooth level indicator.
    LevelIndicator _mixedAudioLevel;

    // Counter keeping track of concurrent calls to process.
    // Note: should never be higher than 1 or lower than 0.
    WebRtc_Word16 _processCalls;

    // Used for inhibiting saturation in mixing.
    scoped_ptr<AudioProcessing> _limiter;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_AUDIO_CONFERENCE_MIXER_IMPL_H_
