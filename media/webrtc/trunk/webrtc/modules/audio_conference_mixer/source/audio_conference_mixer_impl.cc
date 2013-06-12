/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio_conference_mixer_defines.h"
#include "audio_conference_mixer_impl.h"
#include "audio_frame_manipulator.h"
#include "audio_processing.h"
#include "critical_section_wrapper.h"
#include "map_wrapper.h"
#include "modules/utility/interface/audio_frame_operations.h"
#include "trace.h"

namespace webrtc {
namespace {

// Mix |frame| into |mixed_frame|, with saturation protection and upmixing.
// These effects are applied to |frame| itself prior to mixing. Assumes that
// |mixed_frame| always has at least as many channels as |frame|. Supports
// stereo at most.
//
// TODO(andrew): consider not modifying |frame| here.
void MixFrames(AudioFrame* mixed_frame, AudioFrame* frame) {
  assert(mixed_frame->num_channels_ >= frame->num_channels_);
  // Divide by two to avoid saturation in the mixing.
  *frame >>= 1;
  if (mixed_frame->num_channels_ > frame->num_channels_) {
    // We only support mono-to-stereo.
    assert(mixed_frame->num_channels_ == 2 &&
           frame->num_channels_ == 1);
    AudioFrameOperations::MonoToStereo(frame);
  }

  *mixed_frame += *frame;
}

// Return the max number of channels from a |list| composed of AudioFrames.
int MaxNumChannels(const ListWrapper& list) {
  ListItem* item = list.First();
  int max_num_channels = 1;
  while (item) {
    AudioFrame* frame = static_cast<AudioFrame*>(item->GetItem());
    max_num_channels = std::max(max_num_channels, frame->num_channels_);
    item = list.Next(item);
  }
  return max_num_channels;
}

void SetParticipantStatistics(ParticipantStatistics* stats,
                              const AudioFrame& frame)
{
    stats->participant = frame.id_;
    stats->level = 0;  // TODO(andrew): to what should this be set?
}

}  // namespace

MixerParticipant::MixerParticipant()
    : _mixHistory(new MixHistory())
{
}

MixerParticipant::~MixerParticipant()
{
    delete _mixHistory;
}

int32_t MixerParticipant::IsMixed(bool& mixed) const
{
    return _mixHistory->IsMixed(mixed);
}

MixHistory::MixHistory()
    : _isMixed(0)
{
}

MixHistory::~MixHistory()
{
}

int32_t MixHistory::IsMixed(bool& mixed) const
{
    mixed = _isMixed;
    return 0;
}

int32_t MixHistory::WasMixed(bool& wasMixed) const
{
    // Was mixed is the same as is mixed depending on perspective. This function
    // is for the perspective of AudioConferenceMixerImpl.
    return IsMixed(wasMixed);
}

int32_t MixHistory::SetIsMixed(const bool mixed)
{
    _isMixed = mixed;
    return 0;
}

void MixHistory::ResetMixedStatus()
{
    _isMixed = false;
}

AudioConferenceMixer* AudioConferenceMixer::Create(int id)
{
    AudioConferenceMixerImpl* mixer = new AudioConferenceMixerImpl(id);
    if(!mixer->Init())
    {
        delete mixer;
        return NULL;
    }
    return mixer;
}

AudioConferenceMixerImpl::AudioConferenceMixerImpl(int id)
    : _scratchParticipantsToMixAmount(0),
      _scratchMixedParticipants(),
      _scratchVadPositiveParticipantsAmount(0),
      _scratchVadPositiveParticipants(),
      _crit(NULL),
      _cbCrit(NULL),
      _id(id),
      _minimumMixingFreq(kLowestPossible),
      _mixReceiver(NULL),
      _mixerStatusCallback(NULL),
      _amountOf10MsBetweenCallbacks(1),
      _amountOf10MsUntilNextCallback(0),
      _mixerStatusCb(false),
      _outputFrequency(kDefaultFrequency),
      _sampleSize(0),
      _audioFramePool(NULL),
      _participantList(),
      _additionalParticipantList(),
      _numMixedParticipants(0),
      _timeStamp(0),
      _timeScheduler(kProcessPeriodicityInMs),
      _mixedAudioLevel(),
      _processCalls(0),
      _limiter(NULL)
{}

bool AudioConferenceMixerImpl::Init()
{
    _crit.reset(CriticalSectionWrapper::CreateCriticalSection());
    if (_crit.get() == NULL)
        return false;

    _cbCrit.reset(CriticalSectionWrapper::CreateCriticalSection());
    if(_cbCrit.get() == NULL)
        return false;

    _limiter.reset(AudioProcessing::Create(_id));
    if(_limiter.get() == NULL)
        return false;

    MemoryPool<AudioFrame>::CreateMemoryPool(_audioFramePool,
                                             DEFAULT_AUDIO_FRAME_POOLSIZE);
    if(_audioFramePool == NULL)
        return false;

    if(SetOutputFrequency(kDefaultFrequency) == -1)
        return false;

    // Assume mono.
    if (!SetNumLimiterChannels(1))
        return false;

    if(_limiter->gain_control()->set_mode(GainControl::kFixedDigital) !=
        _limiter->kNoError)
        return false;

    // We smoothly limit the mixed frame to -7 dbFS. -6 would correspond to the
    // divide-by-2 but -7 is used instead to give a bit of headroom since the
    // AGC is not a hard limiter.
    if(_limiter->gain_control()->set_target_level_dbfs(7) != _limiter->kNoError)
        return false;

    if(_limiter->gain_control()->set_compression_gain_db(0)
        != _limiter->kNoError)
        return false;

    if(_limiter->gain_control()->enable_limiter(true) != _limiter->kNoError)
        return false;

    if(_limiter->gain_control()->Enable(true) != _limiter->kNoError)
        return false;

    return true;
}

AudioConferenceMixerImpl::~AudioConferenceMixerImpl()
{
    MemoryPool<AudioFrame>::DeleteMemoryPool(_audioFramePool);
    assert(_audioFramePool == NULL);
}

int32_t AudioConferenceMixerImpl::ChangeUniqueId(const int32_t id)
{
    _id = id;
    return 0;
}

// Process should be called every kProcessPeriodicityInMs ms
int32_t AudioConferenceMixerImpl::TimeUntilNextProcess()
{
    int32_t timeUntilNextProcess = 0;
    CriticalSectionScoped cs(_crit.get());
    if(_timeScheduler.TimeToNextUpdate(timeUntilNextProcess) != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioMixerServer, _id,
                     "failed in TimeToNextUpdate() call");
        // Sanity check
        assert(false);
        return -1;
    }
    return timeUntilNextProcess;
}

int32_t AudioConferenceMixerImpl::Process()
{
    uint32_t remainingParticipantsAllowedToMix =
        kMaximumAmountOfMixedParticipants;
    {
        CriticalSectionScoped cs(_crit.get());
        assert(_processCalls == 0);
        _processCalls++;

        // Let the scheduler know that we are running one iteration.
        _timeScheduler.UpdateScheduler();
    }

    ListWrapper mixList;
    ListWrapper rampOutList;
    ListWrapper additionalFramesList;
    MapWrapper mixedParticipantsMap;
    {
        CriticalSectionScoped cs(_cbCrit.get());

        int32_t lowFreq = GetLowestMixingFrequency();
        // SILK can run in 12 kHz and 24 kHz. These frequencies are not
        // supported so use the closest higher frequency to not lose any
        // information.
        // TODO(henrike): this is probably more appropriate to do in
        //                GetLowestMixingFrequency().
        if (lowFreq == 12000)
        {
            lowFreq = 16000;
        } else if (lowFreq == 24000) {
            lowFreq = 32000;
        }
        if(lowFreq <= 0)
        {
            CriticalSectionScoped cs(_crit.get());
            _processCalls--;
            return 0;
        } else  {
            switch(lowFreq)
            {
            case 8000:
                if(OutputFrequency() != kNbInHz)
                {
                    SetOutputFrequency(kNbInHz);
                }
                break;
            case 16000:
                if(OutputFrequency() != kWbInHz)
                {
                    SetOutputFrequency(kWbInHz);
                }
                break;
            case 32000:
                if(OutputFrequency() != kSwbInHz)
                {
                    SetOutputFrequency(kSwbInHz);
                }
                break;
            case 48000:
                if(OutputFrequency() != kFbInHz)
                {
                    SetOutputFrequency(kFbInHz);
                }
                break;
            default:
                assert(false);

                CriticalSectionScoped cs(_crit.get());
                _processCalls--;
                return -1;
            }
        }

        UpdateToMix(mixList, rampOutList, mixedParticipantsMap,
                    remainingParticipantsAllowedToMix);

        GetAdditionalAudio(additionalFramesList);
        UpdateMixedStatus(mixedParticipantsMap);
        _scratchParticipantsToMixAmount = mixedParticipantsMap.Size();
    }

    // Clear mixedParticipantsMap to avoid memory leak warning.
    // Please note that the mixedParticipantsMap doesn't own any dynamically
    // allocated memory.
    while(mixedParticipantsMap.Erase(mixedParticipantsMap.First()) == 0) {}

    // Get an AudioFrame for mixing from the memory pool.
    AudioFrame* mixedAudio = NULL;
    if(_audioFramePool->PopMemory(mixedAudio) == -1)
    {
        WEBRTC_TRACE(kTraceMemory, kTraceAudioMixerServer, _id,
                     "failed PopMemory() call");
        assert(false);
        return -1;
    }

    bool timeForMixerCallback = false;
    int retval = 0;
    int32_t audioLevel = 0;
    {
        CriticalSectionScoped cs(_crit.get());

        // TODO(henrike): it might be better to decide the number of channels
        //                with an API instead of dynamically.

        // Find the max channels over all mixing lists.
        const int num_mixed_channels = std::max(MaxNumChannels(mixList),
            std::max(MaxNumChannels(additionalFramesList),
                     MaxNumChannels(rampOutList)));

        if (!SetNumLimiterChannels(num_mixed_channels))
            retval = -1;

        mixedAudio->UpdateFrame(-1, _timeStamp, NULL, 0, _outputFrequency,
                                AudioFrame::kNormalSpeech,
                                AudioFrame::kVadPassive, num_mixed_channels);

        _timeStamp += _sampleSize;

        MixFromList(*mixedAudio, mixList);
        MixAnonomouslyFromList(*mixedAudio, additionalFramesList);
        MixAnonomouslyFromList(*mixedAudio, rampOutList);

        if(mixedAudio->samples_per_channel_ == 0)
        {
            // Nothing was mixed, set the audio samples to silence.
            mixedAudio->samples_per_channel_ = _sampleSize;
            mixedAudio->Mute();
        }
        else
        {
            // Only call the limiter if we have something to mix.
            if(!LimitMixedAudio(*mixedAudio))
                retval = -1;
        }

        _mixedAudioLevel.ComputeLevel(mixedAudio->data_,_sampleSize);
        audioLevel = _mixedAudioLevel.GetLevel();

        if(_mixerStatusCb)
        {
            _scratchVadPositiveParticipantsAmount = 0;
            UpdateVADPositiveParticipants(mixList);
            if(_amountOf10MsUntilNextCallback-- == 0)
            {
                _amountOf10MsUntilNextCallback = _amountOf10MsBetweenCallbacks;
                timeForMixerCallback = true;
            }
        }
    }

    {
        CriticalSectionScoped cs(_cbCrit.get());
        if(_mixReceiver != NULL)
        {
            const AudioFrame** dummy = NULL;
            _mixReceiver->NewMixedAudio(
                _id,
                *mixedAudio,
                dummy,
                0);
        }

        if((_mixerStatusCallback != NULL) &&
            timeForMixerCallback)
        {
            _mixerStatusCallback->MixedParticipants(
                _id,
                _scratchMixedParticipants,
                _scratchParticipantsToMixAmount);

            _mixerStatusCallback->VADPositiveParticipants(
                _id,
                _scratchVadPositiveParticipants,
                _scratchVadPositiveParticipantsAmount);
            _mixerStatusCallback->MixedAudioLevel(_id,audioLevel);
        }
    }

    // Reclaim all outstanding memory.
    _audioFramePool->PushMemory(mixedAudio);
    ClearAudioFrameList(mixList);
    ClearAudioFrameList(rampOutList);
    ClearAudioFrameList(additionalFramesList);
    {
        CriticalSectionScoped cs(_crit.get());
        _processCalls--;
    }
    return retval;
}

int32_t AudioConferenceMixerImpl::RegisterMixedStreamCallback(
    AudioMixerOutputReceiver& mixReceiver)
{
    CriticalSectionScoped cs(_cbCrit.get());
    if(_mixReceiver != NULL)
    {
        return -1;
    }
    _mixReceiver = &mixReceiver;
    return 0;
}

int32_t AudioConferenceMixerImpl::UnRegisterMixedStreamCallback()
{
    CriticalSectionScoped cs(_cbCrit.get());
    if(_mixReceiver == NULL)
    {
        return -1;
    }
    _mixReceiver = NULL;
    return 0;
}

int32_t AudioConferenceMixerImpl::SetOutputFrequency(
    const Frequency frequency)
{
    CriticalSectionScoped cs(_crit.get());
    const int error = _limiter->set_sample_rate_hz(frequency);
    if(error != _limiter->kNoError)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioMixerServer, _id,
                     "Error from AudioProcessing: %d", error);
        return -1;
    }

    _outputFrequency = frequency;
    _sampleSize = (_outputFrequency*kProcessPeriodicityInMs) / 1000;

    return 0;
}

AudioConferenceMixer::Frequency
AudioConferenceMixerImpl::OutputFrequency() const
{
    CriticalSectionScoped cs(_crit.get());
    return _outputFrequency;
}

bool AudioConferenceMixerImpl::SetNumLimiterChannels(int numChannels)
{
    if(_limiter->num_input_channels() != numChannels)
    {
        const int error = _limiter->set_num_channels(numChannels,
                                                     numChannels);
        if(error != _limiter->kNoError)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioMixerServer, _id,
                         "Error from AudioProcessing: %d", error);
            assert(false);
            return false;
        }
    }

    return true;
}

int32_t AudioConferenceMixerImpl::RegisterMixerStatusCallback(
    AudioMixerStatusReceiver& mixerStatusCallback,
    const uint32_t amountOf10MsBetweenCallbacks)
{
    if(amountOf10MsBetweenCallbacks == 0)
    {
        WEBRTC_TRACE(
            kTraceWarning,
            kTraceAudioMixerServer,
            _id,
            "amountOf10MsBetweenCallbacks(%d) needs to be larger than 0");
        return -1;
    }
    {
        CriticalSectionScoped cs(_cbCrit.get());
        if(_mixerStatusCallback != NULL)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioMixerServer, _id,
                         "Mixer status callback already registered");
            return -1;
        }
        _mixerStatusCallback = &mixerStatusCallback;
    }
    {
        CriticalSectionScoped cs(_crit.get());
        _amountOf10MsBetweenCallbacks  = amountOf10MsBetweenCallbacks;
        _amountOf10MsUntilNextCallback = 0;
        _mixerStatusCb                 = true;
    }
    return 0;
}

int32_t AudioConferenceMixerImpl::UnRegisterMixerStatusCallback()
{
    {
        CriticalSectionScoped cs(_crit.get());
        if(!_mixerStatusCb)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioMixerServer, _id,
                         "Mixer status callback not registered");
            return -1;
        }
        _mixerStatusCb = false;
    }
    {
        CriticalSectionScoped cs(_cbCrit.get());
        _mixerStatusCallback = NULL;
    }
    return 0;
}

int32_t AudioConferenceMixerImpl::SetMixabilityStatus(
    MixerParticipant& participant,
    const bool mixable)
{
    if (!mixable)
    {
        // Anonymous participants are in a separate list. Make sure that the
        // participant is in the _participantList if it is being mixed.
        SetAnonymousMixabilityStatus(participant, false);
    }
    uint32_t numMixedParticipants;
    {
        CriticalSectionScoped cs(_cbCrit.get());
        const bool isMixed =
            IsParticipantInList(participant,_participantList);
        // API must be called with a new state.
        if(!(mixable ^ isMixed))
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioMixerServer, _id,
                         "Mixable is aready %s",
                         isMixed ? "ON" : "off");
            return -1;
        }
        bool success = false;
        if(mixable)
        {
            success = AddParticipantToList(participant,_participantList);
        }
        else
        {
            success = RemoveParticipantFromList(participant,_participantList);
        }
        if(!success)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioMixerServer, _id,
                         "failed to %s participant",
                         mixable ? "add" : "remove");
            assert(false);
            return -1;
        }

        int numMixedNonAnonymous = _participantList.GetSize();
        if (numMixedNonAnonymous > kMaximumAmountOfMixedParticipants)
        {
            numMixedNonAnonymous = kMaximumAmountOfMixedParticipants;
        }
        numMixedParticipants = numMixedNonAnonymous +
                               _additionalParticipantList.GetSize();
    }
    // A MixerParticipant was added or removed. Make sure the scratch
    // buffer is updated if necessary.
    // Note: The scratch buffer may only be updated in Process().
    CriticalSectionScoped cs(_crit.get());
    _numMixedParticipants = numMixedParticipants;
    return 0;
}

int32_t AudioConferenceMixerImpl::MixabilityStatus(
    MixerParticipant& participant,
    bool& mixable)
{
    CriticalSectionScoped cs(_cbCrit.get());
    mixable = IsParticipantInList(participant, _participantList);
    return 0;
}

int32_t AudioConferenceMixerImpl::SetAnonymousMixabilityStatus(
    MixerParticipant& participant, const bool anonymous)
{
    CriticalSectionScoped cs(_cbCrit.get());
    if(IsParticipantInList(participant, _additionalParticipantList))
    {
        if(anonymous)
        {
            return 0;
        }
        if(!RemoveParticipantFromList(participant, _additionalParticipantList))
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioMixerServer, _id,
                         "unable to remove participant from anonymous list");
            assert(false);
            return -1;
        }
        return AddParticipantToList(participant, _participantList) ? 0 : -1;
    }
    if(!anonymous)
    {
        return 0;
    }
    const bool mixable = RemoveParticipantFromList(participant,
                                                   _participantList);
    if(!mixable)
    {
        WEBRTC_TRACE(
            kTraceWarning,
            kTraceAudioMixerServer,
            _id,
            "participant must be registered before turning it into anonymous");
        // Setting anonymous status is only possible if MixerParticipant is
        // already registered.
        return -1;
    }
    return AddParticipantToList(participant, _additionalParticipantList) ?
        0 : -1;
}

int32_t AudioConferenceMixerImpl::AnonymousMixabilityStatus(
    MixerParticipant& participant, bool& mixable)
{
    CriticalSectionScoped cs(_cbCrit.get());
    mixable = IsParticipantInList(participant,
                                  _additionalParticipantList);
    return 0;
}

int32_t AudioConferenceMixerImpl::SetMinimumMixingFrequency(
    Frequency freq)
{
    // Make sure that only allowed sampling frequencies are used. Use closest
    // higher sampling frequency to avoid losing information.
    if (static_cast<int>(freq) == 12000)
    {
         freq = kWbInHz;
    } else if (static_cast<int>(freq) == 24000) {
        freq = kSwbInHz;
    }

    if((freq == kNbInHz) || (freq == kWbInHz) || (freq == kSwbInHz) ||
       (freq == kLowestPossible))
    {
        _minimumMixingFreq=freq;
        return 0;
    }
    else
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioMixerServer, _id,
                     "SetMinimumMixingFrequency incorrect frequency: %i",freq);
        assert(false);
        return -1;
    }
}

// Check all AudioFrames that are to be mixed. The highest sampling frequency
// found is the lowest that can be used without losing information.
int32_t AudioConferenceMixerImpl::GetLowestMixingFrequency()
{
    const int participantListFrequency =
        GetLowestMixingFrequencyFromList(_participantList);
    const int anonymousListFrequency =
        GetLowestMixingFrequencyFromList(_additionalParticipantList);
    const int highestFreq =
        (participantListFrequency > anonymousListFrequency) ?
            participantListFrequency : anonymousListFrequency;
    // Check if the user specified a lowest mixing frequency.
    if(_minimumMixingFreq != kLowestPossible)
    {
        if(_minimumMixingFreq > highestFreq)
        {
            return _minimumMixingFreq;
        }
    }
    return highestFreq;
}

int32_t AudioConferenceMixerImpl::GetLowestMixingFrequencyFromList(
    ListWrapper& mixList)
{
    int32_t highestFreq = 8000;
    ListItem* item = mixList.First();
    while(item)
    {
        MixerParticipant* participant =
            static_cast<MixerParticipant*>(item->GetItem());
        const int32_t neededFrequency = participant->NeededFrequency(_id);
        if(neededFrequency > highestFreq)
        {
            highestFreq = neededFrequency;
        }
        item = mixList.Next(item);
    }
    return highestFreq;
}

void AudioConferenceMixerImpl::UpdateToMix(
    ListWrapper& mixList,
    ListWrapper& rampOutList,
    MapWrapper& mixParticipantList,
    uint32_t& maxAudioFrameCounter)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "UpdateToMix(mixList,rampOutList,mixParticipantList,%d)",
                 maxAudioFrameCounter);
    const uint32_t mixListStartSize = mixList.GetSize();
    ListWrapper activeList; // Elements are AudioFrames
    // Struct needed by the passive lists to keep track of which AudioFrame
    // belongs to which MixerParticipant.
    struct ParticipantFramePair
    {
        MixerParticipant* participant;
        AudioFrame* audioFrame;
    };
    ListWrapper passiveWasNotMixedList; // Elements are MixerParticipant
    ListWrapper passiveWasMixedList;    // Elements are MixerParticipant
    ListItem* item = _participantList.First();
    while(item)
    {
        // Stop keeping track of passive participants if there are already
        // enough participants available (they wont be mixed anyway).
        bool mustAddToPassiveList = (maxAudioFrameCounter >
                                    (activeList.GetSize() +
                                     passiveWasMixedList.GetSize() +
                                     passiveWasNotMixedList.GetSize()));

        MixerParticipant* participant = static_cast<MixerParticipant*>(
            item->GetItem());
        bool wasMixed = false;
        participant->_mixHistory->WasMixed(wasMixed);
        AudioFrame* audioFrame = NULL;
        if(_audioFramePool->PopMemory(audioFrame) == -1)
        {
            WEBRTC_TRACE(kTraceMemory, kTraceAudioMixerServer, _id,
                         "failed PopMemory() call");
            assert(false);
            return;
        }
        audioFrame->sample_rate_hz_ = _outputFrequency;

        if(participant->GetAudioFrame(_id,*audioFrame) != 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioMixerServer, _id,
                         "failed to GetAudioFrame() from participant");
            _audioFramePool->PushMemory(audioFrame);
            item = _participantList.Next(item);
            continue;
        }
        // TODO(henrike): this assert triggers in some test cases where SRTP is
        // used which prevents NetEQ from making a VAD. Temporarily disable this
        // assert until the problem is fixed on a higher level.
        // assert(audioFrame->vad_activity_ != AudioFrame::kVadUnknown);
        if (audioFrame->vad_activity_ == AudioFrame::kVadUnknown)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioMixerServer, _id,
                         "invalid VAD state from participant");
        }

        if(audioFrame->vad_activity_ == AudioFrame::kVadActive)
        {
            if(!wasMixed)
            {
                RampIn(*audioFrame);
            }

            if(activeList.GetSize() >= maxAudioFrameCounter)
            {
                // There are already more active participants than should be
                // mixed. Only keep the ones with the highest energy.
                ListItem* replaceItem = NULL;
                CalculateEnergy(*audioFrame);
                uint32_t lowestEnergy = audioFrame->energy_;

                ListItem* activeItem = activeList.First();
                while(activeItem)
                {
                    AudioFrame* replaceFrame = static_cast<AudioFrame*>(
                        activeItem->GetItem());
                    CalculateEnergy(*replaceFrame);
                    if(replaceFrame->energy_ < lowestEnergy)
                    {
                        replaceItem = activeItem;
                        lowestEnergy = replaceFrame->energy_;
                    }
                    activeItem = activeList.Next(activeItem);
                }
                if(replaceItem != NULL)
                {
                    AudioFrame* replaceFrame = static_cast<AudioFrame*>(
                        replaceItem->GetItem());

                    bool replaceWasMixed = false;
                    MapItem* replaceParticipant = mixParticipantList.Find(
                        replaceFrame->id_);
                    // When a frame is pushed to |activeList| it is also pushed
                    // to mixParticipantList with the frame's id. This means
                    // that the Find call above should never fail.
                    if(replaceParticipant == NULL)
                    {
                        assert(false);
                    } else {
                        static_cast<MixerParticipant*>(
                            replaceParticipant->GetItem())->_mixHistory->
                            WasMixed(replaceWasMixed);

                        mixParticipantList.Erase(replaceFrame->id_);
                        activeList.Erase(replaceItem);

                        activeList.PushFront(static_cast<void*>(audioFrame));
                        mixParticipantList.Insert(
                            audioFrame->id_,
                            static_cast<void*>(participant));
                        assert(mixParticipantList.Size() <=
                               kMaximumAmountOfMixedParticipants);

                        if(replaceWasMixed)
                        {
                            RampOut(*replaceFrame);
                            rampOutList.PushBack(
                                static_cast<void*>(replaceFrame));
                            assert(rampOutList.GetSize() <=
                                   kMaximumAmountOfMixedParticipants);
                        } else {
                            _audioFramePool->PushMemory(replaceFrame);
                        }
                    }
                } else {
                    if(wasMixed)
                    {
                        RampOut(*audioFrame);
                        rampOutList.PushBack(static_cast<void*>(audioFrame));
                        assert(rampOutList.GetSize() <=
                               kMaximumAmountOfMixedParticipants);
                    } else {
                        _audioFramePool->PushMemory(audioFrame);
                    }
                }
            } else {
                activeList.PushFront(static_cast<void*>(audioFrame));
                mixParticipantList.Insert(audioFrame->id_,
                                          static_cast<void*>(participant));
                assert(mixParticipantList.Size() <=
                       kMaximumAmountOfMixedParticipants);
            }
        } else {
            if(wasMixed)
            {
                ParticipantFramePair* pair = new ParticipantFramePair;
                pair->audioFrame  = audioFrame;
                pair->participant = participant;
                passiveWasMixedList.PushBack(static_cast<void*>(pair));
            } else if(mustAddToPassiveList) {
                RampIn(*audioFrame);
                ParticipantFramePair* pair = new ParticipantFramePair;
                pair->audioFrame  = audioFrame;
                pair->participant = participant;
                passiveWasNotMixedList.PushBack(static_cast<void*>(pair));
            } else {
                _audioFramePool->PushMemory(audioFrame);
            }
        }
        item = _participantList.Next(item);
    }
    assert(activeList.GetSize() <= maxAudioFrameCounter);
    // At this point it is known which participants should be mixed. Transfer
    // this information to this functions output parameters.
    while(!activeList.Empty())
    {
        ListItem* mixItem = activeList.First();
        mixList.PushBack(mixItem->GetItem());
        activeList.Erase(mixItem);
    }
    // Always mix a constant number of AudioFrames. If there aren't enough
    // active participants mix passive ones. Starting with those that was mixed
    // last iteration.
    while(!passiveWasMixedList.Empty())
    {
        ListItem* mixItem = passiveWasMixedList.First();
        ParticipantFramePair* pair = static_cast<ParticipantFramePair*>(
            mixItem->GetItem());
        if(mixList.GetSize() <  maxAudioFrameCounter + mixListStartSize)
        {
            mixList.PushBack(pair->audioFrame);
            mixParticipantList.Insert(pair->audioFrame->id_,
                                      static_cast<void*>(pair->participant));
            assert(mixParticipantList.Size() <=
                   kMaximumAmountOfMixedParticipants);
        }
        else
        {
            _audioFramePool->PushMemory(pair->audioFrame);
        }
        delete pair;
        passiveWasMixedList.Erase(mixItem);
    }
    // And finally the ones that have not been mixed for a while.
    while(!passiveWasNotMixedList.Empty())
    {
        ListItem* mixItem = passiveWasNotMixedList.First();
        ParticipantFramePair* pair = static_cast<ParticipantFramePair*>(
            mixItem->GetItem());
        if(mixList.GetSize() <  maxAudioFrameCounter + mixListStartSize)
        {
            mixList.PushBack(pair->audioFrame);
            mixParticipantList.Insert(pair->audioFrame->id_,
                                      static_cast<void*>(pair->participant));
            assert(mixParticipantList.Size() <=
                   kMaximumAmountOfMixedParticipants);
        }
        else
        {
            _audioFramePool->PushMemory(pair->audioFrame);
        }
        delete pair;
        passiveWasNotMixedList.Erase(mixItem);
    }
    assert(maxAudioFrameCounter + mixListStartSize >= mixList.GetSize());
    maxAudioFrameCounter += mixListStartSize - mixList.GetSize();
}

void AudioConferenceMixerImpl::GetAdditionalAudio(
    ListWrapper& additionalFramesList)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "GetAdditionalAudio(additionalFramesList)");
    ListItem* item = _additionalParticipantList.First();
    while(item)
    {
        // The GetAudioFrame() callback may remove the current item. Store the
        // next item just in case that happens.
        ListItem* nextItem = _additionalParticipantList.Next(item);

        MixerParticipant* participant = static_cast<MixerParticipant*>(
            item->GetItem());
        AudioFrame* audioFrame = NULL;
        if(_audioFramePool->PopMemory(audioFrame) == -1)
        {
            WEBRTC_TRACE(kTraceMemory, kTraceAudioMixerServer, _id,
                         "failed PopMemory() call");
            assert(false);
            return;
        }
        audioFrame->sample_rate_hz_ = _outputFrequency;
        if(participant->GetAudioFrame(_id, *audioFrame) != 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioMixerServer, _id,
                         "failed to GetAudioFrame() from participant");
            _audioFramePool->PushMemory(audioFrame);
            item = nextItem;
            continue;
        }
        if(audioFrame->samples_per_channel_ == 0)
        {
            // Empty frame. Don't use it.
            _audioFramePool->PushMemory(audioFrame);
            item = nextItem;
            continue;
        }
        additionalFramesList.PushBack(static_cast<void*>(audioFrame));
        item = nextItem;
    }
}

void AudioConferenceMixerImpl::UpdateMixedStatus(
    MapWrapper& mixedParticipantsMap)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "UpdateMixedStatus(mixedParticipantsMap)");
    assert(mixedParticipantsMap.Size() <= kMaximumAmountOfMixedParticipants);

    // Loop through all participants. If they are in the mix map they
    // were mixed.
    ListItem* participantItem = _participantList.First();
    while(participantItem != NULL)
    {
        bool isMixed = false;
        MixerParticipant* participant =
            static_cast<MixerParticipant*>(participantItem->GetItem());

        MapItem* mixedItem = mixedParticipantsMap.First();
        while(mixedItem)
        {
            if(participant == mixedItem->GetItem())
            {
                isMixed = true;
                break;
            }
            mixedItem = mixedParticipantsMap.Next(mixedItem);
        }
        participant->_mixHistory->SetIsMixed(isMixed);
        participantItem = _participantList.Next(participantItem);
    }
}

void AudioConferenceMixerImpl::ClearAudioFrameList(ListWrapper& audioFrameList)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "ClearAudioFrameList(audioFrameList)");
    ListItem* item = audioFrameList.First();
    while(item)
    {
        AudioFrame* audioFrame = static_cast<AudioFrame*>(item->GetItem());
        _audioFramePool->PushMemory(audioFrame);
        audioFrameList.Erase(item);
        item = audioFrameList.First();
    }
}

void AudioConferenceMixerImpl::UpdateVADPositiveParticipants(
    ListWrapper& mixList)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "UpdateVADPositiveParticipants(mixList)");

    ListItem* item = mixList.First();
    while(item != NULL)
    {
        AudioFrame* audioFrame = static_cast<AudioFrame*>(item->GetItem());
        CalculateEnergy(*audioFrame);
        if(audioFrame->vad_activity_ == AudioFrame::kVadActive)
        {
            _scratchVadPositiveParticipants[
                _scratchVadPositiveParticipantsAmount].participant =
                audioFrame->id_;
            // TODO(andrew): to what should this be set?
            _scratchVadPositiveParticipants[
                _scratchVadPositiveParticipantsAmount].level = 0;
            _scratchVadPositiveParticipantsAmount++;
        }
        item = mixList.Next(item);
    }
}

bool AudioConferenceMixerImpl::IsParticipantInList(
    MixerParticipant& participant,
    ListWrapper& participantList)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "IsParticipantInList(participant,participantList)");
    ListItem* item = participantList.First();
    while(item != NULL)
    {
        MixerParticipant* rhsParticipant =
            static_cast<MixerParticipant*>(item->GetItem());
        if(&participant == rhsParticipant)
        {
            return true;
        }
        item = participantList.Next(item);
    }
    return false;
}

bool AudioConferenceMixerImpl::AddParticipantToList(
    MixerParticipant& participant,
    ListWrapper& participantList)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "AddParticipantToList(participant, participantList)");
    if(participantList.PushBack(static_cast<void*>(&participant)) == -1)
    {
        return false;
    }
    // Make sure that the mixed status is correct for new MixerParticipant.
    participant._mixHistory->ResetMixedStatus();
    return true;
}

bool AudioConferenceMixerImpl::RemoveParticipantFromList(
    MixerParticipant& participant,
    ListWrapper& participantList)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "RemoveParticipantFromList(participant, participantList)");
    ListItem* item = participantList.First();
    while(item)
    {
        if(item->GetItem() == &participant)
        {
            participantList.Erase(item);
            // Participant is no longer mixed, reset to default.
            participant._mixHistory->ResetMixedStatus();
            return true;
        }
        item = participantList.Next(item);
    }
    return false;
}

int32_t AudioConferenceMixerImpl::MixFromList(
    AudioFrame& mixedAudio,
    const ListWrapper& audioFrameList)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "MixFromList(mixedAudio, audioFrameList)");
    uint32_t position = 0;
    ListItem* item = audioFrameList.First();
    if(item == NULL)
    {
        return 0;
    }

    if(_numMixedParticipants == 1)
    {
        // No mixing required here; skip the saturation protection.
        AudioFrame* audioFrame = static_cast<AudioFrame*>(item->GetItem());
        mixedAudio.CopyFrom(*audioFrame);
        SetParticipantStatistics(&_scratchMixedParticipants[position],
                                 *audioFrame);
        return 0;
    }

    while(item != NULL)
    {
        if(position >= kMaximumAmountOfMixedParticipants)
        {
            WEBRTC_TRACE(
                kTraceMemory,
                kTraceAudioMixerServer,
                _id,
                "Trying to mix more than max amount of mixed participants:%d!",
                kMaximumAmountOfMixedParticipants);
            // Assert and avoid crash
            assert(false);
            position = 0;
        }
        AudioFrame* audioFrame = static_cast<AudioFrame*>(item->GetItem());
        MixFrames(&mixedAudio, audioFrame);

        SetParticipantStatistics(&_scratchMixedParticipants[position],
                                 *audioFrame);

        position++;
        item = audioFrameList.Next(item);
    }

    return 0;
}

// TODO(andrew): consolidate this function with MixFromList.
int32_t AudioConferenceMixerImpl::MixAnonomouslyFromList(
    AudioFrame& mixedAudio,
    const ListWrapper& audioFrameList)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "MixAnonomouslyFromList(mixedAudio, audioFrameList)");
    ListItem* item = audioFrameList.First();
    if(item == NULL)
        return 0;

    if(_numMixedParticipants == 1)
    {
        // No mixing required here; skip the saturation protection.
        AudioFrame* audioFrame = static_cast<AudioFrame*>(item->GetItem());
        mixedAudio.CopyFrom(*audioFrame);
        return 0;
    }

    while(item != NULL)
    {
        AudioFrame* audioFrame = static_cast<AudioFrame*>(item->GetItem());
        MixFrames(&mixedAudio, audioFrame);
        item = audioFrameList.Next(item);
    }
    return 0;
}

bool AudioConferenceMixerImpl::LimitMixedAudio(AudioFrame& mixedAudio)
{
    if(_numMixedParticipants == 1)
    {
        return true;
    }

    // Smoothly limit the mixed frame.
    const int error = _limiter->ProcessStream(&mixedAudio);

    // And now we can safely restore the level. This procedure results in
    // some loss of resolution, deemed acceptable.
    //
    // It's possible to apply the gain in the AGC (with a target level of 0 dbFS
    // and compression gain of 6 dB). However, in the transition frame when this
    // is enabled (moving from one to two participants) it has the potential to
    // create discontinuities in the mixed frame.
    //
    // Instead we double the frame (with addition since left-shifting a
    // negative value is undefined).
    mixedAudio += mixedAudio;

    if(error != _limiter->kNoError)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioMixerServer, _id,
                     "Error from AudioProcessing: %d", error);
        assert(false);
        return false;
    }
    return true;
}
} // namespace webrtc
