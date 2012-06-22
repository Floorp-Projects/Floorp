/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio_device_dummy.h"

#include <string.h>

#include "trace.h"
#include "thread_wrapper.h"
#include "event_wrapper.h"

// Enable to record playout data
//#define RECORD_PLAYOUT 1

namespace webrtc {

const WebRtc_UWord32 REC_TIMER_PERIOD_MS = 10;
const WebRtc_UWord32 PLAY_TIMER_PERIOD_MS = 10;

// ============================================================================
//                            Construction & Destruction
// ============================================================================

// ----------------------------------------------------------------------------
//  AudioDeviceDummy() - ctor
// ----------------------------------------------------------------------------

AudioDeviceDummy::AudioDeviceDummy(const WebRtc_Word32 id) :
	  _ptrAudioBuffer(NULL),
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _id(id),
    _timeEventRec(*EventWrapper::Create()),
    _timeEventPlay(*EventWrapper::Create()),
    _recStartEvent(*EventWrapper::Create()),
    _playStartEvent(*EventWrapper::Create()),
    _ptrThreadRec(NULL),
    _ptrThreadPlay(NULL),
    _recThreadID(0),
    _playThreadID(0),
    _initialized(false),
    _recording(false),
    _playing(false),
    _recIsInitialized(false),
    _playIsInitialized(false),
    _speakerIsInitialized(false),
    _microphoneIsInitialized(false),
    _playDataFile(NULL)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, id, "%s created", __FUNCTION__);

    memset(_recBuffer, 0, sizeof(_recBuffer));
    WebRtc_Word16* tmp = (WebRtc_Word16*)_recBuffer;

    // Saw tooth -16000 to 16000, 100 Hz @ fs = 16 kHz
//    for(int i=0; i<160; ++i)
//    {
//        tmp[i] = i*200-16000;
//    }

    // Rough sinus 2 kHz @ fs = 16 kHz
    for(int i=0; i<20; ++i)
    {
      tmp[i*8] = 0;
      tmp[i*8+1] = -5000;
      tmp[i*8+2] = -16000;
      tmp[i*8+3] = -5000;
      tmp[i*8+4] = 0;
      tmp[i*8+5] = 5000;
      tmp[i*8+6] = 16000;
      tmp[i*8+7] = 5000;
    }
  
#ifdef RECORD_PLAYOUT
    _playDataFile = fopen("webrtc_VoiceEngine_playout.pcm", "wb");
    if (!_playDataFile)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                   "  Could not open file for writing playout data");
    }
#endif
}

// ----------------------------------------------------------------------------
//  AudioDeviceDummy() - dtor
// ----------------------------------------------------------------------------

AudioDeviceDummy::~AudioDeviceDummy()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s destroyed", __FUNCTION__);

    Terminate();

    _ptrAudioBuffer = NULL;

    delete &_recStartEvent;
    delete &_playStartEvent;
    delete &_timeEventRec;
    delete &_timeEventPlay;
    delete &_critSect;

    if (_playDataFile)
    {
        fclose(_playDataFile);
    }
}

// ============================================================================
//                                     API
// ============================================================================

// ----------------------------------------------------------------------------
//  AttachAudioBuffer
// ----------------------------------------------------------------------------

void AudioDeviceDummy::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer)
{

    _ptrAudioBuffer = audioBuffer;

    // Inform the AudioBuffer about default settings for this implementation.
    _ptrAudioBuffer->SetRecordingSampleRate(16000);
    _ptrAudioBuffer->SetPlayoutSampleRate(16000);
    _ptrAudioBuffer->SetRecordingChannels(1);
    _ptrAudioBuffer->SetPlayoutChannels(1);
}

// ----------------------------------------------------------------------------
//  ActiveAudioLayer
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::ActiveAudioLayer(
    AudioDeviceModule::AudioLayer& audioLayer) const
{
    audioLayer = AudioDeviceModule::kDummyAudio;
    return 0;
}

// ----------------------------------------------------------------------------
//  Init
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::Init()
{

    CriticalSectionScoped lock(&_critSect);

    if (_initialized)
    {
        return 0;
    }

    const bool periodic(true);
    unsigned int threadID(0);
    char threadName[64] = {0};

    // RECORDING
    strncpy(threadName, "webrtc_audio_module_rec_thread", 63);
    _ptrThreadRec = ThreadWrapper::CreateThread(
        RecThreadFunc, this, kRealtimePriority, threadName);
    if (_ptrThreadRec == NULL)
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to create the rec audio thread");
        return -1;
    }

    if (!_ptrThreadRec->Start(threadID))
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to start the rec audio thread");
        delete _ptrThreadRec;
        _ptrThreadRec = NULL;
        return -1;
    }
    _recThreadID = threadID;

    if (!_timeEventRec.StartTimer(periodic, REC_TIMER_PERIOD_MS))
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to start the rec timer event");
        if (_ptrThreadRec->Stop())
        {
            delete _ptrThreadRec;
            _ptrThreadRec = NULL;
        }
        else
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  unable to stop the activated rec thread");
        }
        return -1;
    }

    // PLAYOUT
    strncpy(threadName, "webrtc_audio_module_play_thread", 63);
    _ptrThreadPlay = ThreadWrapper::CreateThread(
        PlayThreadFunc, this, kRealtimePriority, threadName);
    if (_ptrThreadPlay == NULL)
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to create the play audio thread");
        return -1;
    }

    threadID = 0;
    if (!_ptrThreadPlay->Start(threadID))
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to start the play audio thread");
        delete _ptrThreadPlay;
        _ptrThreadPlay = NULL;
        return -1;
    }
    _playThreadID = threadID;

    if (!_timeEventPlay.StartTimer(periodic, PLAY_TIMER_PERIOD_MS))
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to start the play timer event");
        if (_ptrThreadPlay->Stop())
        {
            delete _ptrThreadPlay;
            _ptrThreadPlay = NULL;
        }
        else
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  unable to stop the activated play thread");
        }
        return -1;
    }

    _initialized = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  Terminate
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::Terminate()
{

    CriticalSectionScoped lock(&_critSect);

    if (!_initialized)
    {
        return 0;
    }

    // RECORDING
    if (_ptrThreadRec)
    {
        ThreadWrapper* tmpThread = _ptrThreadRec;
        _ptrThreadRec = NULL;
        _critSect.Leave();

        tmpThread->SetNotAlive();
        _timeEventRec.Set();

        if (tmpThread->Stop())
        {
            delete tmpThread;
        }
        else
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  failed to close down the rec audio thread");
        }

        _critSect.Enter();
    }

    _timeEventRec.StopTimer();

    // PLAYOUT
    if (_ptrThreadPlay)
    {
        ThreadWrapper* tmpThread = _ptrThreadPlay;
        _ptrThreadPlay = NULL;
        _critSect.Leave();

        tmpThread->SetNotAlive();
        _timeEventPlay.Set();

        if (tmpThread->Stop())
        {
            delete tmpThread;
        }
        else
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  failed to close down the play audio thread");
        }

        _critSect.Enter();
    }

    _timeEventPlay.StopTimer();

    _initialized = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  Initialized
// ----------------------------------------------------------------------------

bool AudioDeviceDummy::Initialized() const
{
    return (_initialized);
}

// ----------------------------------------------------------------------------
//  SpeakerIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SpeakerIsAvailable(bool& available)
{

    CriticalSectionScoped lock(&_critSect);

    available = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  InitSpeaker
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::InitSpeaker()
{

    CriticalSectionScoped lock(&_critSect);

    if (_playing)
    {
        return -1;
    }

	_speakerIsInitialized = true;

	return 0;
}

// ----------------------------------------------------------------------------
//  MicrophoneIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::MicrophoneIsAvailable(bool& available)
{

    CriticalSectionScoped lock(&_critSect);

    available = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  InitMicrophone
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::InitMicrophone()
{

    CriticalSectionScoped lock(&_critSect);

    if (_recording)
    {
        return -1;
    }

    _microphoneIsInitialized = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  SpeakerIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceDummy::SpeakerIsInitialized() const
{

    return (_speakerIsInitialized);
}

// ----------------------------------------------------------------------------
//  MicrophoneIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceDummy::MicrophoneIsInitialized() const
{

    return (_microphoneIsInitialized);
}

// ----------------------------------------------------------------------------
//  SpeakerVolumeIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SpeakerVolumeIsAvailable(bool& available)
{

    CriticalSectionScoped lock(&_critSect);

    available = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetSpeakerVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SetSpeakerVolume(WebRtc_UWord32 volume)
{

	return -1;
}

// ----------------------------------------------------------------------------
//  SpeakerVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SpeakerVolume(WebRtc_UWord32& volume) const
{

    return -1;
}

// ----------------------------------------------------------------------------
//  SetWaveOutVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SetWaveOutVolume(WebRtc_UWord16 volumeLeft, WebRtc_UWord16 volumeRight)
{

    return -1;
}

// ----------------------------------------------------------------------------
//  WaveOutVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::WaveOutVolume(WebRtc_UWord16& volumeLeft, WebRtc_UWord16& volumeRight) const
{

    return -1;
}

// ----------------------------------------------------------------------------
//  MaxSpeakerVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::MaxSpeakerVolume(WebRtc_UWord32& maxVolume) const
{

    return -1;
}

// ----------------------------------------------------------------------------
//  MinSpeakerVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::MinSpeakerVolume(WebRtc_UWord32& minVolume) const
{

    return -1;
}

// ----------------------------------------------------------------------------
//  SpeakerVolumeStepSize
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SpeakerVolumeStepSize(WebRtc_UWord16& stepSize) const
{
	
    return -1;
}

// ----------------------------------------------------------------------------
//  SpeakerMuteIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SpeakerMuteIsAvailable(bool& available)
{

    CriticalSectionScoped lock(&_critSect);

    available = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetSpeakerMute
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SetSpeakerMute(bool enable)
{

    return -1;
}

// ----------------------------------------------------------------------------
//  SpeakerMute
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SpeakerMute(bool& enabled) const
{

    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneMuteIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::MicrophoneMuteIsAvailable(bool& available)
{

    CriticalSectionScoped lock(&_critSect);

    available = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneMute
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SetMicrophoneMute(bool enable)
{

    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneMute
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::MicrophoneMute(bool& enabled) const
{

    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneBoostIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::MicrophoneBoostIsAvailable(bool& available)
{

    available = false;
    return 0;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneBoost
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SetMicrophoneBoost(bool enable)
{

    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneBoost
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::MicrophoneBoost(bool& enabled) const
{

    return -1;
}

// ----------------------------------------------------------------------------
//  StereoRecordingIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::StereoRecordingIsAvailable(bool& available)
{

    available = false;
    return 0;
}

// ----------------------------------------------------------------------------
//  SetStereoRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SetStereoRecording(bool enable)
{

    CriticalSectionScoped lock(&_critSect);

    if (enable)
    {
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  StereoRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::StereoRecording(bool& enabled) const
{

    enabled = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  StereoPlayoutIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::StereoPlayoutIsAvailable(bool& available)
{

    available = false;
    return 0;
}

// ----------------------------------------------------------------------------
//  SetStereoPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SetStereoPlayout(bool enable)
{

    CriticalSectionScoped lock(&_critSect);

    if (enable)
    {
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  StereoPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::StereoPlayout(bool& enabled) const
{

    enabled = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetAGC
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SetAGC(bool enable)
{

    return -1;
}

// ----------------------------------------------------------------------------
//  AGC
// ----------------------------------------------------------------------------

bool AudioDeviceDummy::AGC() const
{
    // WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    return false;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolumeIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::MicrophoneVolumeIsAvailable(bool& available)
{

    CriticalSectionScoped lock(&_critSect);

    available = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SetMicrophoneVolume(WebRtc_UWord32 volume)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id,
                 "AudioDeviceDummy::SetMicrophoneVolume(volume=%u)", volume);

    CriticalSectionScoped lock(&_critSect);

    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::MicrophoneVolume(WebRtc_UWord32& volume) const
{
    // WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    return -1;
}

// ----------------------------------------------------------------------------
//  MaxMicrophoneVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::MaxMicrophoneVolume(
    WebRtc_UWord32& maxVolume) const
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    return -1;
}

// ----------------------------------------------------------------------------
//  MinMicrophoneVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::MinMicrophoneVolume(
    WebRtc_UWord32& minVolume) const
{

    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolumeStepSize
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::MicrophoneVolumeStepSize(
    WebRtc_UWord16& stepSize) const
{

    return -1;
}

// ----------------------------------------------------------------------------
//  PlayoutDevices
// ----------------------------------------------------------------------------

WebRtc_Word16 AudioDeviceDummy::PlayoutDevices()
{

    CriticalSectionScoped lock(&_critSect);

    return 1;
}

// ----------------------------------------------------------------------------
//  SetPlayoutDevice I (II)
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SetPlayoutDevice(WebRtc_UWord16 index)
{

    if (_playIsInitialized)
    {
        return -1;
    }

    if (index != 0)
    {
      return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  SetPlayoutDevice II (II)
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SetPlayoutDevice(
    AudioDeviceModule::WindowsDeviceType device)
{
	return -1;
}

// ----------------------------------------------------------------------------
//  PlayoutDeviceName
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::PlayoutDeviceName(
    WebRtc_UWord16 index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize])
{

    if (index != 0)
    {
        return -1;
    }

    memset(name, 0, kAdmMaxDeviceNameSize);

    if (guid != NULL)
    {
      memset(guid, 0, kAdmMaxGuidSize);
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingDeviceName
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::RecordingDeviceName(
    WebRtc_UWord16 index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize])
{

    if (index != 0)
    {
        return -1;
    }

    memset(name, 0, kAdmMaxDeviceNameSize);

    if (guid != NULL)
    {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingDevices
// ----------------------------------------------------------------------------

WebRtc_Word16 AudioDeviceDummy::RecordingDevices()
{

    CriticalSectionScoped lock(&_critSect);

    return 1;
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice I (II)
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SetRecordingDevice(WebRtc_UWord16 index)
{

    if (_recIsInitialized)
    {
        return -1;
    }

    if (index != 0 )
    {
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice II (II)
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SetRecordingDevice(
    AudioDeviceModule::WindowsDeviceType device)
{
    return -1;
}

// ----------------------------------------------------------------------------
//  PlayoutIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::PlayoutIsAvailable(bool& available)
{

    available = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::RecordingIsAvailable(bool& available)
{

    available = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  InitPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::InitPlayout()
{

    CriticalSectionScoped lock(&_critSect);

    if (_playing)
    {
        return -1;
    }

    if (_playIsInitialized)
    {
        return 0;
    }

    // Initialize the speaker (devices might have been added or removed)
    if (InitSpeaker() == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  InitSpeaker() failed");
    }

    _playIsInitialized = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  InitRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::InitRecording()
{

    CriticalSectionScoped lock(&_critSect);

    if (_recording)
    {
        return -1;
    }

    if (_recIsInitialized)
    {
        return 0;
    }

    // Initialize the microphone (devices might have been added or removed)
    if (InitMicrophone() == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  InitMicrophone() failed");
    }

    _recIsInitialized = true;

    return 0;

}

// ----------------------------------------------------------------------------
//  StartRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::StartRecording()
{

    CriticalSectionScoped lock(&_critSect);

    if (!_recIsInitialized)
    {
        return -1;
    }

    if (_recording)
    {
        return 0;
    }

    _recording = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  StopRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::StopRecording()
{

    CriticalSectionScoped lock(&_critSect);

    if (!_recIsInitialized)
    {
        return 0;
    }

    _recIsInitialized = false;
    _recording = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceDummy::RecordingIsInitialized() const
{
    return (_recIsInitialized);
}

// ----------------------------------------------------------------------------
//  Recording
// ----------------------------------------------------------------------------

bool AudioDeviceDummy::Recording() const
{
    return (_recording);
}

// ----------------------------------------------------------------------------
//  PlayoutIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceDummy::PlayoutIsInitialized() const
{

    return (_playIsInitialized);
}

// ----------------------------------------------------------------------------
//  StartPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::StartPlayout()
{

    CriticalSectionScoped lock(&_critSect);

    if (!_playIsInitialized)
    {
        return -1;
    }

    if (_playing)
    {
        return 0;
    }

    _playing = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  StopPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::StopPlayout()
{

    if (!_playIsInitialized)
    {
        return 0;
    }

    _playIsInitialized = false;
    _playing = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutDelay
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::PlayoutDelay(WebRtc_UWord16& delayMS) const
{
    CriticalSectionScoped lock(&_critSect);
    delayMS = 0;
    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingDelay
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::RecordingDelay(WebRtc_UWord16& delayMS) const
{
    CriticalSectionScoped lock(&_critSect);
    delayMS = 0;
    return 0;
}

// ----------------------------------------------------------------------------
//  Playing
// ----------------------------------------------------------------------------

bool AudioDeviceDummy::Playing() const
{
    return (_playing);
}
// ----------------------------------------------------------------------------
//  SetPlayoutBuffer
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::SetPlayoutBuffer(
    const AudioDeviceModule::BufferType type, WebRtc_UWord16 sizeMS)
{

    CriticalSectionScoped lock(&_critSect);

    // Just ignore

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutBuffer
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::PlayoutBuffer(
    AudioDeviceModule::BufferType& type, WebRtc_UWord16& sizeMS) const
{
    CriticalSectionScoped lock(&_critSect);

    type = AudioDeviceModule::kAdaptiveBufferSize;
    sizeMS = 0;

    return 0;
}

// ----------------------------------------------------------------------------
//  CPULoad
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceDummy::CPULoad(WebRtc_UWord16& load) const
{

    load = 0;

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutWarning
// ----------------------------------------------------------------------------

bool AudioDeviceDummy::PlayoutWarning() const
{
    return false;
}

// ----------------------------------------------------------------------------
//  PlayoutError
// ----------------------------------------------------------------------------

bool AudioDeviceDummy::PlayoutError() const
{
    return false;
}

// ----------------------------------------------------------------------------
//  RecordingWarning
// ----------------------------------------------------------------------------

bool AudioDeviceDummy::RecordingWarning() const
{
    return false;
}

// ----------------------------------------------------------------------------
//  RecordingError
// ----------------------------------------------------------------------------

bool AudioDeviceDummy::RecordingError() const
{
    return false;
}

// ----------------------------------------------------------------------------
//  ClearPlayoutWarning
// ----------------------------------------------------------------------------

void AudioDeviceDummy::ClearPlayoutWarning()
{
}

// ----------------------------------------------------------------------------
//  ClearPlayoutError
// ----------------------------------------------------------------------------

void AudioDeviceDummy::ClearPlayoutError()
{
}

// ----------------------------------------------------------------------------
//  ClearRecordingWarning
// ----------------------------------------------------------------------------

void AudioDeviceDummy::ClearRecordingWarning()
{
}

// ----------------------------------------------------------------------------
//  ClearRecordingError
// ----------------------------------------------------------------------------

void AudioDeviceDummy::ClearRecordingError()
{
}

// ============================================================================
//                                  Thread Methods
// ============================================================================

// ----------------------------------------------------------------------------
//  PlayThreadFunc
// ----------------------------------------------------------------------------

bool AudioDeviceDummy::PlayThreadFunc(void* pThis)
{
    return (static_cast<AudioDeviceDummy*>(pThis)->PlayThreadProcess());
}

// ----------------------------------------------------------------------------
//  RecThreadFunc
// ----------------------------------------------------------------------------

bool AudioDeviceDummy::RecThreadFunc(void* pThis)
{
    return (static_cast<AudioDeviceDummy*>(pThis)->RecThreadProcess());
}

// ----------------------------------------------------------------------------
//  PlayThreadProcess
// ----------------------------------------------------------------------------

bool AudioDeviceDummy::PlayThreadProcess()
{
    switch (_timeEventPlay.Wait(1000))
    {
    case kEventSignaled:
        break;
    case kEventError:
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                   "EventWrapper::Wait() failed => restarting timer");
        _timeEventPlay.StopTimer();
        _timeEventPlay.StartTimer(true, PLAY_TIMER_PERIOD_MS);
        return true;
    case kEventTimeout:
        return true;
    }

    Lock();

    if(_playing)
    {
        WebRtc_Word8 playBuffer[2*160];

        UnLock();
        WebRtc_Word32 nSamples = (WebRtc_Word32)_ptrAudioBuffer->RequestPlayoutData(160);
        Lock();

        if (!_playing)
        {
            UnLock();
            return true;
        }

        nSamples = _ptrAudioBuffer->GetPlayoutData(playBuffer);
        if (nSamples != 160)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                "  invalid number of output samples(%d)", nSamples);
        }

        if (_playDataFile)
        {
            int wr = fwrite(playBuffer, 2, 160, _playDataFile);
            if (wr != 160)
            {
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                           "  Could not write playout data to file (%d) ferror = %d",
                           wr, ferror(_playDataFile));
            }
        }
    }

    UnLock();
    return true;
}

// ----------------------------------------------------------------------------
//  RecThreadProcess
// ----------------------------------------------------------------------------

bool AudioDeviceDummy::RecThreadProcess()
{
    switch (_timeEventRec.Wait(1000))
    {
    case kEventSignaled:
        break;
    case kEventError:
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                   "EventWrapper::Wait() failed => restarting timer");
        _timeEventRec.StopTimer();
        _timeEventRec.StartTimer(true, REC_TIMER_PERIOD_MS);
        return true;
    case kEventTimeout:
        return true;
    }

    Lock();

    if (_recording)
    {
        // store the recorded buffer
        _ptrAudioBuffer->SetRecordedBuffer(_recBuffer, 160);

        // store vqe delay values
        _ptrAudioBuffer->SetVQEData(0, 0, 0);

        // deliver recorded samples at specified sample rate, mic level etc. to the observer using callback
        UnLock();
        _ptrAudioBuffer->DeliverRecordedData();
    }
    else
    {
        UnLock();
    }

    return true;
}

}  // namespace webrtc
