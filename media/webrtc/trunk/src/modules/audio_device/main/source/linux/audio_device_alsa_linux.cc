/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cassert>

#include "audio_device_utility.h"
#include "audio_device_alsa_linux.h"
#include "audio_device_config.h"

#include "event_wrapper.h"
#include "trace.h"
#include "thread_wrapper.h"


webrtc_adm_linux_alsa::AlsaSymbolTable AlsaSymbolTable;

// Accesses ALSA functions through our late-binding symbol table instead of
// directly. This way we don't have to link to libasound, which means our binary
// will work on systems that don't have it.
#define LATE(sym) \
  LATESYM_GET(webrtc_adm_linux_alsa::AlsaSymbolTable, &AlsaSymbolTable, sym)

// Redefine these here to be able to do late-binding
#undef snd_ctl_card_info_alloca
#define snd_ctl_card_info_alloca(ptr) \
        do { *ptr = (snd_ctl_card_info_t *) \
            __builtin_alloca (LATE(snd_ctl_card_info_sizeof)()); \
            memset(*ptr, 0, LATE(snd_ctl_card_info_sizeof)()); } while (0)

#undef snd_pcm_info_alloca
#define snd_pcm_info_alloca(pInfo) \
       do { *pInfo = (snd_pcm_info_t *) \
       __builtin_alloca (LATE(snd_pcm_info_sizeof)()); \
       memset(*pInfo, 0, LATE(snd_pcm_info_sizeof)()); } while (0)

// snd_lib_error_handler_t
void WebrtcAlsaErrorHandler(const char *file,
                          int line,
                          const char *function,
                          int err,
                          const char *fmt,...){};

namespace webrtc
{
static const unsigned int ALSA_PLAYOUT_FREQ = 48000;
static const unsigned int ALSA_PLAYOUT_CH = 2;
static const unsigned int ALSA_PLAYOUT_LATENCY = 40*1000; // in us
static const unsigned int ALSA_CAPTURE_FREQ = 48000;
static const unsigned int ALSA_CAPTURE_CH = 2;
static const unsigned int ALSA_CAPTURE_LATENCY = 40*1000; // in us
static const unsigned int ALSA_PLAYOUT_WAIT_TIMEOUT = 5; // in ms
static const unsigned int ALSA_CAPTURE_WAIT_TIMEOUT = 5; // in ms

#define FUNC_GET_NUM_OF_DEVICE 0
#define FUNC_GET_DEVICE_NAME 1
#define FUNC_GET_DEVICE_NAME_FOR_AN_ENUM 2

AudioDeviceLinuxALSA::AudioDeviceLinuxALSA(const WebRtc_Word32 id) :
    _ptrAudioBuffer(NULL),
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _ptrThreadRec(NULL),
    _ptrThreadPlay(NULL),
    _recThreadID(0),
    _playThreadID(0),
    _id(id),
    _mixerManager(id),
    _inputDeviceIndex(0),
    _outputDeviceIndex(0),
    _inputDeviceIsSpecified(false),
    _outputDeviceIsSpecified(false),
    _handleRecord(NULL),
    _handlePlayout(NULL),
    _recordingBuffersizeInFrame(0),
    _recordingPeriodSizeInFrame(0),
    _playoutBufferSizeInFrame(0),
    _playoutPeriodSizeInFrame(0),
    _recordingBufferSizeIn10MS(0),
    _playoutBufferSizeIn10MS(0),
    _recordingFramesIn10MS(0),
    _playoutFramesIn10MS(0),
    _recordingFreq(ALSA_CAPTURE_FREQ),
    _playoutFreq(ALSA_PLAYOUT_FREQ),
    _recChannels(ALSA_CAPTURE_CH),
    _playChannels(ALSA_PLAYOUT_CH),
    _recordingBuffer(NULL),
    _playoutBuffer(NULL),
    _recordingFramesLeft(0),
    _playoutFramesLeft(0),
    _playbackBufferSize(0),
    _playBufType(AudioDeviceModule::kFixedBufferSize),
    _initialized(false),
    _recording(false),
    _playing(false),
    _recIsInitialized(false),
    _playIsInitialized(false),
    _AGC(false),
    _recordingDelay(0),
    _playoutDelay(0),
    _writeErrors(0),
    _playWarning(0),
    _playError(0),
    _recWarning(0),
    _recError(0),
    _playBufDelay(80),
    _playBufDelayFixed(80)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, id,
                 "%s created", __FUNCTION__);
}

// ----------------------------------------------------------------------------
//  AudioDeviceLinuxALSA - dtor
// ----------------------------------------------------------------------------

AudioDeviceLinuxALSA::~AudioDeviceLinuxALSA()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id,
                 "%s destroyed", __FUNCTION__);
    
    Terminate();

    // Clean up the recording buffer and playout buffer.
    if (_recordingBuffer)
    {
        delete [] _recordingBuffer;
        _recordingBuffer = NULL;
    }
    if (_playoutBuffer)
    {
        delete [] _playoutBuffer;
        _playoutBuffer = NULL;
    }
    delete &_critSect;
}

void AudioDeviceLinuxALSA::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer)
{

    CriticalSectionScoped lock(&_critSect);

    _ptrAudioBuffer = audioBuffer;

    // Inform the AudioBuffer about default settings for this implementation.
    // Set all values to zero here since the actual settings will be done by
    // InitPlayout and InitRecording later.
    _ptrAudioBuffer->SetRecordingSampleRate(0);
    _ptrAudioBuffer->SetPlayoutSampleRate(0);
    _ptrAudioBuffer->SetRecordingChannels(0);
    _ptrAudioBuffer->SetPlayoutChannels(0);
}

WebRtc_Word32 AudioDeviceLinuxALSA::ActiveAudioLayer(
    AudioDeviceModule::AudioLayer& audioLayer) const
{
    audioLayer = AudioDeviceModule::kLinuxAlsaAudio;
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::Init()
{

    CriticalSectionScoped lock(&_critSect);

    // Load libasound
    if (!AlsaSymbolTable.Load())
    {
        // Alsa is not installed on
        // this system
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                   "  failed to load symbol table");
        return -1;
    }

    if (_initialized)
    {
        return 0;
    }

    _playWarning = 0;
    _playError = 0;
    _recWarning = 0;
    _recError = 0;

    _initialized = true;

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::Terminate()
{

    if (!_initialized)
    {
        return 0;
    }

    CriticalSectionScoped lock(&_critSect);

    _mixerManager.Close();

    // RECORDING
    if (_ptrThreadRec)
    {
        ThreadWrapper* tmpThread = _ptrThreadRec;
        _ptrThreadRec = NULL;
        _critSect.Leave();

        tmpThread->SetNotAlive();

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

    // PLAYOUT
    if (_ptrThreadPlay)
    {
        ThreadWrapper* tmpThread = _ptrThreadPlay;
        _ptrThreadPlay = NULL;
        _critSect.Leave();

        tmpThread->SetNotAlive();

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

    _initialized = false;
    _outputDeviceIsSpecified = false;
    _inputDeviceIsSpecified = false;

    return 0;
}

bool AudioDeviceLinuxALSA::Initialized() const
{
    return (_initialized);
}

WebRtc_Word32 AudioDeviceLinuxALSA::SpeakerIsAvailable(bool& available)
{

    bool wasInitialized = _mixerManager.SpeakerIsInitialized();

    // Make an attempt to open up the
    // output mixer corresponding to the currently selected output device.
    //
    if (!wasInitialized && InitSpeaker() == -1)
    {
        available = false;
        return 0;
    }

    // Given that InitSpeaker was successful, we know that a valid speaker
    // exists
    available = true;

    // Close the initialized output mixer
    //
    if (!wasInitialized)
    {
        _mixerManager.CloseSpeaker();
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::InitSpeaker()
{

    CriticalSectionScoped lock(&_critSect);

    if (_playing)
    {
        return -1;
    }

    char devName[kAdmMaxDeviceNameSize] = {0};
    GetDevicesInfo(2, true, _outputDeviceIndex, devName, kAdmMaxDeviceNameSize);
    return _mixerManager.OpenSpeaker(devName);
}

WebRtc_Word32 AudioDeviceLinuxALSA::MicrophoneIsAvailable(bool& available)
{

    bool wasInitialized = _mixerManager.MicrophoneIsInitialized();

    // Make an attempt to open up the
    // input mixer corresponding to the currently selected output device.
    //
    if (!wasInitialized && InitMicrophone() == -1)
    {
        available = false;
        return 0;
    }

    // Given that InitMicrophone was successful, we know that a valid
    // microphone exists
    available = true;

    // Close the initialized input mixer
    //
    if (!wasInitialized)
    {
        _mixerManager.CloseMicrophone();
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::InitMicrophone()
{

    CriticalSectionScoped lock(&_critSect);

    if (_recording)
    {
        return -1;
    }

    char devName[kAdmMaxDeviceNameSize] = {0};
    GetDevicesInfo(2, false, _inputDeviceIndex, devName, kAdmMaxDeviceNameSize);
    return _mixerManager.OpenMicrophone(devName);
}

bool AudioDeviceLinuxALSA::SpeakerIsInitialized() const
{
    return (_mixerManager.SpeakerIsInitialized());
}

bool AudioDeviceLinuxALSA::MicrophoneIsInitialized() const
{
    return (_mixerManager.MicrophoneIsInitialized());
}

WebRtc_Word32 AudioDeviceLinuxALSA::SpeakerVolumeIsAvailable(bool& available)
{

    bool wasInitialized = _mixerManager.SpeakerIsInitialized();

    // Make an attempt to open up the
    // output mixer corresponding to the currently selected output device.
    if (!wasInitialized && InitSpeaker() == -1)
    {
        // If we end up here it means that the selected speaker has no volume
        // control.
        available = false;
        return 0;
    }

    // Given that InitSpeaker was successful, we know that a volume control
    // exists
    available = true;

    // Close the initialized output mixer
    if (!wasInitialized)
    {
        _mixerManager.CloseSpeaker();
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetSpeakerVolume(WebRtc_UWord32 volume)
{

    return (_mixerManager.SetSpeakerVolume(volume));
}

WebRtc_Word32 AudioDeviceLinuxALSA::SpeakerVolume(WebRtc_UWord32& volume) const
{

    WebRtc_UWord32 level(0);

    if (_mixerManager.SpeakerVolume(level) == -1)
    {
        return -1;
    }

    volume = level;
    
    return 0;
}


WebRtc_Word32 AudioDeviceLinuxALSA::SetWaveOutVolume(WebRtc_UWord16 volumeLeft,
                                                     WebRtc_UWord16 volumeRight)
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

WebRtc_Word32 AudioDeviceLinuxALSA::WaveOutVolume(
    WebRtc_UWord16& /*volumeLeft*/,
    WebRtc_UWord16& /*volumeRight*/) const
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

WebRtc_Word32 AudioDeviceLinuxALSA::MaxSpeakerVolume(
    WebRtc_UWord32& maxVolume) const
{

    WebRtc_UWord32 maxVol(0);

    if (_mixerManager.MaxSpeakerVolume(maxVol) == -1)
    {
        return -1;
    }

    maxVolume = maxVol;
    
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::MinSpeakerVolume(
    WebRtc_UWord32& minVolume) const
{

    WebRtc_UWord32 minVol(0);

    if (_mixerManager.MinSpeakerVolume(minVol) == -1)
    {
        return -1;
    }

    minVolume = minVol;
    
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SpeakerVolumeStepSize(
    WebRtc_UWord16& stepSize) const
{

    WebRtc_UWord16 delta(0); 
     
    if (_mixerManager.SpeakerVolumeStepSize(delta) == -1)
    {
        return -1;
    }

    stepSize = delta;

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SpeakerMuteIsAvailable(bool& available)
{

    bool isAvailable(false);
    bool wasInitialized = _mixerManager.SpeakerIsInitialized();

    // Make an attempt to open up the
    // output mixer corresponding to the currently selected output device.
    //
    if (!wasInitialized && InitSpeaker() == -1)
    {
        // If we end up here it means that the selected speaker has no volume
        // control, hence it is safe to state that there is no mute control
        // already at this stage.
        available = false;
        return 0;
    }

    // Check if the selected speaker has a mute control
    _mixerManager.SpeakerMuteIsAvailable(isAvailable);

    available = isAvailable;

    // Close the initialized output mixer
    if (!wasInitialized)
    {
        _mixerManager.CloseSpeaker();
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetSpeakerMute(bool enable)
{
    return (_mixerManager.SetSpeakerMute(enable));
}

WebRtc_Word32 AudioDeviceLinuxALSA::SpeakerMute(bool& enabled) const
{

    bool muted(0); 
        
    if (_mixerManager.SpeakerMute(muted) == -1)
    {
        return -1;
    }

    enabled = muted;
    
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::MicrophoneMuteIsAvailable(bool& available)
{

    bool isAvailable(false);
    bool wasInitialized = _mixerManager.MicrophoneIsInitialized();

    // Make an attempt to open up the
    // input mixer corresponding to the currently selected input device.
    //
    if (!wasInitialized && InitMicrophone() == -1)
    {
        // If we end up here it means that the selected microphone has no volume
        // control, hence it is safe to state that there is no mute control
        // already at this stage.
        available = false;
        return 0;
    }

    // Check if the selected microphone has a mute control
    //
    _mixerManager.MicrophoneMuteIsAvailable(isAvailable);
    available = isAvailable;

    // Close the initialized input mixer
    //
    if (!wasInitialized)
    {
        _mixerManager.CloseMicrophone();
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetMicrophoneMute(bool enable)
{
    return (_mixerManager.SetMicrophoneMute(enable));
}

// ----------------------------------------------------------------------------
//  MicrophoneMute
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceLinuxALSA::MicrophoneMute(bool& enabled) const
{

    bool muted(0); 
        
    if (_mixerManager.MicrophoneMute(muted) == -1)
    {
        return -1;
    }

    enabled = muted;
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::MicrophoneBoostIsAvailable(bool& available)
{
    
    bool isAvailable(false);
    bool wasInitialized = _mixerManager.MicrophoneIsInitialized();

    // Enumerate all avaliable microphone and make an attempt to open up the
    // input mixer corresponding to the currently selected input device.
    //
    if (!wasInitialized && InitMicrophone() == -1)
    {
        // If we end up here it means that the selected microphone has no volume
        // control, hence it is safe to state that there is no boost control
        // already at this stage.
        available = false;
        return 0;
    }

    // Check if the selected microphone has a boost control
    _mixerManager.MicrophoneBoostIsAvailable(isAvailable);
    available = isAvailable;

    // Close the initialized input mixer
    if (!wasInitialized)
    {
        _mixerManager.CloseMicrophone();
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetMicrophoneBoost(bool enable)
{

    return (_mixerManager.SetMicrophoneBoost(enable));
}

WebRtc_Word32 AudioDeviceLinuxALSA::MicrophoneBoost(bool& enabled) const
{

    bool onOff(0); 
        
    if (_mixerManager.MicrophoneBoost(onOff) == -1)
    {
        return -1;
    }

    enabled = onOff;
    
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::StereoRecordingIsAvailable(bool& available)
{

    CriticalSectionScoped lock(&_critSect);

    // If we already have initialized in stereo it's obviously available
    if (_recIsInitialized && (2 == _recChannels))
    {
        available = true;
        return 0;
    }

    // Save rec states and the number of rec channels
    bool recIsInitialized = _recIsInitialized;
    bool recording = _recording;
    int recChannels = _recChannels;

    available = false;
    
    // Stop/uninitialize recording if initialized (and possibly started)
    if (_recIsInitialized)
    {
        StopRecording();
    }

    // Try init in stereo;
    _recChannels = 2;
    if (InitRecording() == 0)
    {
        available = true;
    }

    // Stop/uninitialize recording
    StopRecording();

    // Recover previous states
    _recChannels = recChannels;
    if (recIsInitialized)
    {
        InitRecording();
    }
    if (recording)
    {
        StartRecording();
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetStereoRecording(bool enable)
{

    if (enable)
        _recChannels = 2;
    else
        _recChannels = 1;

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::StereoRecording(bool& enabled) const
{

    if (_recChannels == 2)
        enabled = true;
    else
        enabled = false;

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::StereoPlayoutIsAvailable(bool& available)
{

    CriticalSectionScoped lock(&_critSect);

    // If we already have initialized in stereo it's obviously available
    if (_playIsInitialized && (2 == _playChannels))
    {
        available = true;
        return 0;
    }

    // Save rec states and the number of rec channels
    bool playIsInitialized = _playIsInitialized;
    bool playing = _playing;
    int playChannels = _playChannels;

    available = false;
    
    // Stop/uninitialize recording if initialized (and possibly started)
    if (_playIsInitialized)
    {
        StopPlayout();
    }

    // Try init in stereo;
    _playChannels = 2;
    if (InitPlayout() == 0)
    {
        available = true;
    }

    // Stop/uninitialize recording
    StopPlayout();

    // Recover previous states
    _playChannels = playChannels;
    if (playIsInitialized)
    {
        InitPlayout();
    }
    if (playing)
    {
        StartPlayout();
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetStereoPlayout(bool enable)
{

    if (enable)
        _playChannels = 2;
    else
        _playChannels = 1;

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::StereoPlayout(bool& enabled) const
{

    if (_playChannels == 2)
        enabled = true;
    else
        enabled = false;

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetAGC(bool enable)
{

    _AGC = enable;

    return 0;
}

bool AudioDeviceLinuxALSA::AGC() const
{

    return _AGC;
}

WebRtc_Word32 AudioDeviceLinuxALSA::MicrophoneVolumeIsAvailable(bool& available)
{

    bool wasInitialized = _mixerManager.MicrophoneIsInitialized();

    // Make an attempt to open up the
    // input mixer corresponding to the currently selected output device.
    if (!wasInitialized && InitMicrophone() == -1)
    {
        // If we end up here it means that the selected microphone has no volume
        // control.
        available = false;
        return 0;
    }

    // Given that InitMicrophone was successful, we know that a volume control
    // exists
    available = true;

    // Close the initialized input mixer
    if (!wasInitialized)
    {
        _mixerManager.CloseMicrophone();
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetMicrophoneVolume(WebRtc_UWord32 volume)
{

    return (_mixerManager.SetMicrophoneVolume(volume));
 
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::MicrophoneVolume(WebRtc_UWord32& volume) const
{

    WebRtc_UWord32 level(0);

    if (_mixerManager.MicrophoneVolume(level) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  failed to retrive current microphone level");
        return -1;
    }

    volume = level;
    
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::MaxMicrophoneVolume(
    WebRtc_UWord32& maxVolume) const
{

    WebRtc_UWord32 maxVol(0);

    if (_mixerManager.MaxMicrophoneVolume(maxVol) == -1)
    {
        return -1;
    }

    maxVolume = maxVol;

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::MinMicrophoneVolume(
    WebRtc_UWord32& minVolume) const
{

    WebRtc_UWord32 minVol(0);

    if (_mixerManager.MinMicrophoneVolume(minVol) == -1)
    {
        return -1;
    }

    minVolume = minVol;

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::MicrophoneVolumeStepSize(
    WebRtc_UWord16& stepSize) const
{

    WebRtc_UWord16 delta(0); 
        
    if (_mixerManager.MicrophoneVolumeStepSize(delta) == -1)
    {
        return -1;
    }

    stepSize = delta;

    return 0;
}

WebRtc_Word16 AudioDeviceLinuxALSA::PlayoutDevices()
{

    return (WebRtc_Word16)GetDevicesInfo(0, true);
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetPlayoutDevice(WebRtc_UWord16 index)
{

    if (_playIsInitialized)
    {
        return -1;
    }

    WebRtc_UWord32 nDevices = GetDevicesInfo(0, true);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "  number of availiable audio output devices is %u", nDevices);

    if (index > (nDevices-1))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  device index is out of range [0,%u]", (nDevices-1));
        return -1;
    }

    _outputDeviceIndex = index;
    _outputDeviceIsSpecified = true;

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetPlayoutDevice(
    AudioDeviceModule::WindowsDeviceType /*device*/)
{
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "WindowsDeviceType not supported");
    return -1;
}

WebRtc_Word32 AudioDeviceLinuxALSA::PlayoutDeviceName(
    WebRtc_UWord16 index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize])
{

    const WebRtc_UWord16 nDevices(PlayoutDevices());

    if ((index > (nDevices-1)) || (name == NULL))
    {
        return -1;
    }

    memset(name, 0, kAdmMaxDeviceNameSize);

    if (guid != NULL)
    {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    return GetDevicesInfo(1, true, index, name, kAdmMaxDeviceNameSize);
}

WebRtc_Word32 AudioDeviceLinuxALSA::RecordingDeviceName(
    WebRtc_UWord16 index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize])
{

    const WebRtc_UWord16 nDevices(RecordingDevices());

    if ((index > (nDevices-1)) || (name == NULL))
    {
        return -1;
    }

    memset(name, 0, kAdmMaxDeviceNameSize);

    if (guid != NULL)
    {
        memset(guid, 0, kAdmMaxGuidSize);
    }
    
    return GetDevicesInfo(1, false, index, name, kAdmMaxDeviceNameSize);
}

WebRtc_Word16 AudioDeviceLinuxALSA::RecordingDevices()
{

    return (WebRtc_Word16)GetDevicesInfo(0, false);
}

WebRtc_Word32 AudioDeviceLinuxALSA::SetRecordingDevice(WebRtc_UWord16 index)
{

    if (_recIsInitialized)
    {
        return -1;
    }

    WebRtc_UWord32 nDevices = GetDevicesInfo(0, false);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "  number of availiable audio input devices is %u", nDevices);

    if (index > (nDevices-1))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  device index is out of range [0,%u]", (nDevices-1));
        return -1;
    }

    _inputDeviceIndex = index;
    _inputDeviceIsSpecified = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice II (II)
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceLinuxALSA::SetRecordingDevice(
    AudioDeviceModule::WindowsDeviceType /*device*/)
{
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "WindowsDeviceType not supported");
    return -1;
}

WebRtc_Word32 AudioDeviceLinuxALSA::PlayoutIsAvailable(bool& available)
{
    
    available = false;

    // Try to initialize the playout side with mono
    // Assumes that user set num channels after calling this function
    _playChannels = 1;
    WebRtc_Word32 res = InitPlayout();

    // Cancel effect of initialization
    StopPlayout();

    if (res != -1)
    {
        available = true;
    }
    else
    {
        // It may be possible to play out in stereo
        res = StereoPlayoutIsAvailable(available);
        if (available)
        {
            // Then set channels to 2 so InitPlayout doesn't fail
            _playChannels = 2;
        }
    }
    
    return res;
}

WebRtc_Word32 AudioDeviceLinuxALSA::RecordingIsAvailable(bool& available)
{
    
    available = false;

    // Try to initialize the recording side with mono
    // Assumes that user set num channels after calling this function
    _recChannels = 1;
    WebRtc_Word32 res = InitRecording();

    // Cancel effect of initialization
    StopRecording();

    if (res != -1)
    {
        available = true;
    }
    else
    {
        // It may be possible to record in stereo
        res = StereoRecordingIsAvailable(available);
        if (available)
        {
            // Then set channels to 2 so InitPlayout doesn't fail
            _recChannels = 2;
        }
    }
    
    return res;
}

WebRtc_Word32 AudioDeviceLinuxALSA::InitPlayout()
{

    int errVal = 0;

    CriticalSectionScoped lock(&_critSect);
    if (_playing)
    {
        return -1;
    }

    if (!_outputDeviceIsSpecified)
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

    // Start by closing any existing wave-output devices
    //
    if (_handlePlayout != NULL)
    {
        LATE(snd_pcm_close)(_handlePlayout);
        _handlePlayout = NULL;
        _playIsInitialized = false;
        if (errVal < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  Error closing current playout sound device, error:"
                         " %s", LATE(snd_strerror)(errVal));
        }
    }

    // Open PCM device for playout
    char deviceName[kAdmMaxDeviceNameSize] = {0};
    GetDevicesInfo(2, true, _outputDeviceIndex, deviceName,
                   kAdmMaxDeviceNameSize);

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "  InitPlayout open (%s)", deviceName);

    errVal = LATE(snd_pcm_open)
                 (&_handlePlayout,
                  deviceName,
                  SND_PCM_STREAM_PLAYBACK,
                  SND_PCM_NONBLOCK);

    if (errVal == -EBUSY) // Device busy - try some more!
    {
        for (int i=0; i < 5; i++)
        {
            sleep(1);
            errVal = LATE(snd_pcm_open)
                         (&_handlePlayout,
                          deviceName,
                          SND_PCM_STREAM_PLAYBACK,
                          SND_PCM_NONBLOCK);
            if (errVal == 0)
            {
                break;
            }
        }
    }
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     unable to open playback device: %s (%d)",
                     LATE(snd_strerror)(errVal),
                     errVal);
        _handlePlayout = NULL;
        return -1;
    }

    _playoutFramesIn10MS = _playoutFreq/100;
    if ((errVal = LATE(snd_pcm_set_params)( _handlePlayout,
#if defined(WEBRTC_BIG_ENDIAN)
        SND_PCM_FORMAT_S16_BE,
#else
        SND_PCM_FORMAT_S16_LE, //format
#endif
        SND_PCM_ACCESS_RW_INTERLEAVED, //access
        _playChannels, //channels
        _playoutFreq, //rate
        1, //soft_resample
        ALSA_PLAYOUT_LATENCY //40*1000 //latency required overall latency in us
    )) < 0)
    {   /* 0.5sec */
        _playoutFramesIn10MS = 0;
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     unable to set playback device: %s (%d)",
                     LATE(snd_strerror)(errVal),
                     errVal);
        ErrorRecovery(errVal, _handlePlayout);
        errVal = LATE(snd_pcm_close)(_handlePlayout);
        _handlePlayout = NULL;
        return -1;
    }

    errVal = LATE(snd_pcm_get_params)(_handlePlayout,
        &_playoutBufferSizeInFrame, &_playoutPeriodSizeInFrame);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "    snd_pcm_get_params %s",
                     LATE(snd_strerror)(errVal),
                     errVal);
        _playoutBufferSizeInFrame = 0;
        _playoutPeriodSizeInFrame = 0;
    }
    else {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "    playout snd_pcm_get_params "
                     "buffer_size:%d period_size :%d",
                     _playoutBufferSizeInFrame, _playoutPeriodSizeInFrame);
    }

    if (_ptrAudioBuffer)
    {
        // Update webrtc audio buffer with the selected parameters
        _ptrAudioBuffer->SetPlayoutSampleRate(_playoutFreq);
        _ptrAudioBuffer->SetPlayoutChannels(_playChannels);
    }

    // Set play buffer size
    _playoutBufferSizeIn10MS = LATE(snd_pcm_frames_to_bytes)(
        _handlePlayout, _playoutFramesIn10MS);

    // Init varaibles used for play
    _playWarning = 0;
    _playError = 0;

    if (_handlePlayout != NULL)
    {
        _playIsInitialized = true;
        return 0;
    }
    else
    {
        return -1;
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::InitRecording()
{

    int errVal = 0;

    CriticalSectionScoped lock(&_critSect);

    if (_recording)
    {
        return -1;
    }

    if (!_inputDeviceIsSpecified)
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

    // Start by closing any existing pcm-input devices
    //
    if (_handleRecord != NULL)
    {
        int errVal = LATE(snd_pcm_close)(_handleRecord);
        _handleRecord = NULL;
        _recIsInitialized = false;
        if (errVal < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "     Error closing current recording sound device,"
                         " error: %s",
                         LATE(snd_strerror)(errVal));
        }
    }

    // Open PCM device for recording
    // The corresponding settings for playout are made after the record settings
    char deviceName[kAdmMaxDeviceNameSize] = {0};
    GetDevicesInfo(2, false, _inputDeviceIndex, deviceName,
                   kAdmMaxDeviceNameSize);

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "InitRecording open (%s)", deviceName);
    errVal = LATE(snd_pcm_open)
                 (&_handleRecord,
                  deviceName,
                  SND_PCM_STREAM_CAPTURE,
                  SND_PCM_NONBLOCK);

    // Available modes: 0 = blocking, SND_PCM_NONBLOCK, SND_PCM_ASYNC
    if (errVal == -EBUSY) // Device busy - try some more!
    {
        for (int i=0; i < 5; i++)
        {
            sleep(1);
            errVal = LATE(snd_pcm_open)
                         (&_handleRecord,
                          deviceName,
                          SND_PCM_STREAM_CAPTURE,
                          SND_PCM_NONBLOCK);
            if (errVal == 0)
            {
                break;
            }
        }
    }
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "    unable to open record device: %s",
                     LATE(snd_strerror)(errVal));
        _handleRecord = NULL;
        return -1;
    }

    _recordingFramesIn10MS = _recordingFreq/100;
    if ((errVal = LATE(snd_pcm_set_params)(_handleRecord,
#if defined(WEBRTC_BIG_ENDIAN)
        SND_PCM_FORMAT_S16_BE, //format
#else
        SND_PCM_FORMAT_S16_LE, //format
#endif
        SND_PCM_ACCESS_RW_INTERLEAVED, //access
        _recChannels, //channels
        _recordingFreq, //rate
        1, //soft_resample
        ALSA_CAPTURE_LATENCY //latency in us
    )) < 0)
    {
         // Fall back to another mode then.
         if (_recChannels == 1)
           _recChannels = 2;
         else
           _recChannels = 1;

         if ((errVal = LATE(snd_pcm_set_params)(_handleRecord,
#if defined(WEBRTC_BIG_ENDIAN)
             SND_PCM_FORMAT_S16_BE, //format
#else
             SND_PCM_FORMAT_S16_LE, //format
#endif
             SND_PCM_ACCESS_RW_INTERLEAVED, //access
             _recChannels, //channels
             _recordingFreq, //rate
             1, //soft_resample
             ALSA_CAPTURE_LATENCY //latency in us
         )) < 0)
         {
             _recordingFramesIn10MS = 0;
             WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                          "    unable to set record settings: %s (%d)",
                          LATE(snd_strerror)(errVal), errVal);
             ErrorRecovery(errVal, _handleRecord);
             errVal = LATE(snd_pcm_close)(_handleRecord);
             _handleRecord = NULL;
             return -1;
         }
    }

    errVal = LATE(snd_pcm_get_params)(_handleRecord,
        &_recordingBuffersizeInFrame, &_recordingPeriodSizeInFrame);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "    snd_pcm_get_params %s",
                     LATE(snd_strerror)(errVal), errVal);
        _recordingBuffersizeInFrame = 0;
        _recordingPeriodSizeInFrame = 0;
    }
    else {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "    capture snd_pcm_get_params "
                     "buffer_size:%d period_size:%d",
                     _recordingBuffersizeInFrame, _recordingPeriodSizeInFrame);
    }

    if (_ptrAudioBuffer)
    {
        // Update webrtc audio buffer with the selected parameters
        _ptrAudioBuffer->SetRecordingSampleRate(_recordingFreq);
        _ptrAudioBuffer->SetRecordingChannels(_recChannels);
    }

    // Set rec buffer size and create buffer
    _recordingBufferSizeIn10MS = LATE(snd_pcm_frames_to_bytes)(
        _handleRecord, _recordingFramesIn10MS);

    if (_handleRecord != NULL)
    {
        // Mark recording side as initialized
        _recIsInitialized = true;
        return 0;
    }
    else
    {
        return -1;
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::StartRecording()
{

    if (!_recIsInitialized)
    {
        return -1;
    }

    if (_recording)
    {
        return 0;
    }

    _recording = true;

    int errVal = 0;
    _recordingFramesLeft = _recordingFramesIn10MS;

    // Make sure we only create the buffer once.
    if (!_recordingBuffer)
        _recordingBuffer = new WebRtc_Word8[_recordingBufferSizeIn10MS];
    if (!_recordingBuffer)
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "   failed to alloc recording buffer");
        _recording = false;
        return -1;
    }
    // RECORDING
    const char* threadName = "webrtc_audio_module_capture_thread";
    _ptrThreadRec = ThreadWrapper::CreateThread(RecThreadFunc,
                                                this,
                                                kRealtimePriority,
                                                threadName);
    if (_ptrThreadRec == NULL)
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to create the rec audio thread");
        _recording = false;
        delete [] _recordingBuffer;
        _recordingBuffer = NULL;
        return -1;
    }

    unsigned int threadID(0);
    if (!_ptrThreadRec->Start(threadID))
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to start the rec audio thread");
        _recording = false;
        delete _ptrThreadRec;
        _ptrThreadRec = NULL;
        delete [] _recordingBuffer;
        _recordingBuffer = NULL;
        return -1;
    }
    _recThreadID = threadID;

    errVal = LATE(snd_pcm_prepare)(_handleRecord);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     capture snd_pcm_prepare failed (%s)\n",
                     LATE(snd_strerror)(errVal));
        // just log error
        // if snd_pcm_open fails will return -1
    }

    errVal = LATE(snd_pcm_start)(_handleRecord);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     capture snd_pcm_start err: %s",
                     LATE(snd_strerror)(errVal));
        errVal = LATE(snd_pcm_start)(_handleRecord);
        if (errVal < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "     capture snd_pcm_start 2nd try err: %s",
                         LATE(snd_strerror)(errVal));
            StopRecording();
            return -1;
        }
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::StopRecording()
{

    {
      CriticalSectionScoped lock(&_critSect);

      if (!_recIsInitialized)
      {
          return 0;
      }

      if (_handleRecord == NULL)
      {
          return -1;
      }

      // Make sure we don't start recording (it's asynchronous).
      _recIsInitialized = false;
      _recording = false;
    }

    if (_ptrThreadRec && !_ptrThreadRec->Stop())
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "    failed to stop the rec audio thread");
        return -1;
    }
    else {
        delete _ptrThreadRec;
        _ptrThreadRec = NULL;
    }

    CriticalSectionScoped lock(&_critSect);
    _recordingFramesLeft = 0;
    if (_recordingBuffer)
    {
        delete [] _recordingBuffer;
        _recordingBuffer = NULL;
    }

    // Stop and close pcm recording device.
    int errVal = LATE(snd_pcm_drop)(_handleRecord);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     Error stop recording: %s",
                     LATE(snd_strerror)(errVal));
        return -1;
    }

    errVal = LATE(snd_pcm_close)(_handleRecord);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "     Error closing record sound device, error: %s",
                     LATE(snd_strerror)(errVal));
        return -1;
    }

    // Check if we have muted and unmute if so.
    bool muteEnabled = false;
    MicrophoneMute(muteEnabled);
    if (muteEnabled)
    {
        SetMicrophoneMute(false);
    }

    // set the pcm input handle to NULL
    _handleRecord = NULL;
    return 0;
}

bool AudioDeviceLinuxALSA::RecordingIsInitialized() const
{
    return (_recIsInitialized);
}

bool AudioDeviceLinuxALSA::Recording() const
{
    return (_recording);
}

bool AudioDeviceLinuxALSA::PlayoutIsInitialized() const
{
    return (_playIsInitialized);
}

WebRtc_Word32 AudioDeviceLinuxALSA::StartPlayout()
{
    if (!_playIsInitialized)
    {
        return -1;
    }

    if (_playing)
    {
        return 0;
    }

    _playing = true;

    _playoutFramesLeft = 0;
    if (!_playoutBuffer)
        _playoutBuffer = new WebRtc_Word8[_playoutBufferSizeIn10MS];
    if (!_playoutBuffer)
    {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                   "    failed to alloc playout buf");
      _playing = false;
      return -1;
    }

    // PLAYOUT
    const char* threadName = "webrtc_audio_module_play_thread";
    _ptrThreadPlay =  ThreadWrapper::CreateThread(PlayThreadFunc,
                                                  this,
                                                  kRealtimePriority,
                                                  threadName);
    if (_ptrThreadPlay == NULL)
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "    failed to create the play audio thread");
        _playing = false;
        delete [] _playoutBuffer;
        _playoutBuffer = NULL;
        return -1;
    }

    unsigned int threadID(0);
    if (!_ptrThreadPlay->Start(threadID))
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to start the play audio thread");
        _playing = false;
        delete _ptrThreadPlay;
        _ptrThreadPlay = NULL;
        delete [] _playoutBuffer;
        _playoutBuffer = NULL;
        return -1;
    }
    _playThreadID = threadID;

    int errVal = LATE(snd_pcm_prepare)(_handlePlayout);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "     playout snd_pcm_prepare failed (%s)\n",
                     LATE(snd_strerror)(errVal));
        // just log error
        // if snd_pcm_open fails will return -1
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::StopPlayout()
{

    {
        CriticalSectionScoped lock(&_critSect);

        if (!_playIsInitialized)
        {
            return 0;
        }

        if (_handlePlayout == NULL)
        {
            return -1;
        }

        _playing = false;
    }

    // stop playout thread first
    if (_ptrThreadPlay && !_ptrThreadPlay->Stop())
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to stop the play audio thread");
        return -1;
    }
    else {
        delete _ptrThreadPlay;
        _ptrThreadPlay = NULL;
    }

    CriticalSectionScoped lock(&_critSect);

    _playoutFramesLeft = 0;
    delete [] _playoutBuffer;
    _playoutBuffer = NULL;

    // stop and close pcm playout device
    int errVal = LATE(snd_pcm_drop)(_handlePlayout);
    if (errVal < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "    Error stop playing: %s",
                     LATE(snd_strerror)(errVal));
    }

    errVal = LATE(snd_pcm_close)(_handlePlayout);
     if (errVal < 0)
         WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                      "    Error closing playout sound device, error: %s",
                      LATE(snd_strerror)(errVal));

     // set the pcm input handle to NULL
     _playIsInitialized = false;
     _handlePlayout = NULL;
     WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                  "  handle_playout is now set to NULL");

     return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::PlayoutDelay(WebRtc_UWord16& delayMS) const
{
    delayMS = (WebRtc_UWord16)_playoutDelay * 1000 / _playoutFreq;
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::RecordingDelay(WebRtc_UWord16& delayMS) const
{
    // Adding 10ms adjusted value to the record delay due to 10ms buffering.
    delayMS = (WebRtc_UWord16)(10 + _recordingDelay * 1000 / _recordingFreq);
    return 0;
}

bool AudioDeviceLinuxALSA::Playing() const
{
    return (_playing);
}
// ----------------------------------------------------------------------------
//  SetPlayoutBuffer
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceLinuxALSA::SetPlayoutBuffer(
    const AudioDeviceModule::BufferType type,
    WebRtc_UWord16 sizeMS)
{
    _playBufType = type;
    if (type == AudioDeviceModule::kFixedBufferSize)
    {
        _playBufDelayFixed = sizeMS;
    }
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::PlayoutBuffer(
    AudioDeviceModule::BufferType& type,
    WebRtc_UWord16& sizeMS) const
{
    type = _playBufType;
    if (type == AudioDeviceModule::kFixedBufferSize)
    {
        sizeMS = _playBufDelayFixed; 
    }
    else
    {
        sizeMS = _playBufDelay; 
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::CPULoad(WebRtc_UWord16& load) const
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
               "  API call not supported on this platform");
    return -1;
}

bool AudioDeviceLinuxALSA::PlayoutWarning() const
{
    return (_playWarning > 0);
}

bool AudioDeviceLinuxALSA::PlayoutError() const
{
    return (_playError > 0);
}

bool AudioDeviceLinuxALSA::RecordingWarning() const
{
    return (_recWarning > 0);
}

bool AudioDeviceLinuxALSA::RecordingError() const
{
    return (_recError > 0);
}

void AudioDeviceLinuxALSA::ClearPlayoutWarning()
{
    _playWarning = 0;
}

void AudioDeviceLinuxALSA::ClearPlayoutError()
{
    _playError = 0;
}

void AudioDeviceLinuxALSA::ClearRecordingWarning()
{
    _recWarning = 0;
}

void AudioDeviceLinuxALSA::ClearRecordingError()
{
    _recError = 0;
}

// ============================================================================
//                                 Private Methods
// ============================================================================

WebRtc_Word32 AudioDeviceLinuxALSA::GetDevicesInfo(
    const WebRtc_Word32 function,
    const bool playback,
    const WebRtc_Word32 enumDeviceNo,
    char* enumDeviceName,
    const WebRtc_Word32 ednLen) const
{
    
    // Device enumeration based on libjingle implementation
    // by Tristan Schmelcher at Google Inc.

    const char *type = playback ? "Output" : "Input";
    // dmix and dsnoop are only for playback and capture, respectively, but ALSA
    // stupidly includes them in both lists.
    const char *ignorePrefix = playback ? "dsnoop:" : "dmix:" ;
    // (ALSA lists many more "devices" of questionable interest, but we show them
    // just in case the weird devices may actually be desirable for some
    // users/systems.)

    int err;
    int enumCount(0);
    bool keepSearching(true);

    void **hints;
    err = LATE(snd_device_name_hint)(-1,     // All cards
                                     "pcm",  // Only PCM devices
                                     &hints);
    if (err != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "GetDevicesInfo - device name hint error: %s",
                     LATE(snd_strerror)(err));
        return -1;
    }

    enumCount++; // default is 0
    if (function == FUNC_GET_DEVICE_NAME && enumDeviceNo == 0)
    {
        strcpy(enumDeviceName, "default");
        return 0;
    }
    if (function == FUNC_GET_DEVICE_NAME_FOR_AN_ENUM && enumDeviceNo == 0)
    {
        strcpy(enumDeviceName, "default");
        return 0;
    }

    for (void **list = hints; *list != NULL; ++list)
    {
        char *actualType = LATE(snd_device_name_get_hint)(*list, "IOID");
        if (actualType)
        {   // NULL means it's both.
            bool wrongType = (strcmp(actualType, type) != 0);
            free(actualType);
            if (wrongType)
            {
                // Wrong type of device (i.e., input vs. output).
                continue;
            }
        }

        char *name = LATE(snd_device_name_get_hint)(*list, "NAME");
        if (!name)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "Device has no name");
            // Skip it.
            continue;
        }

        // Now check if we actually want to show this device.
        if (strcmp(name, "default") != 0 &&
            strcmp(name, "null") != 0 &&
            strcmp(name, "pulse") != 0 &&
            strncmp(name, ignorePrefix, strlen(ignorePrefix)) != 0)
        {
            // Yes, we do.
            char *desc = LATE(snd_device_name_get_hint)(*list, "DESC");
            if (!desc)
            {
                // Virtual devices don't necessarily have descriptions.
                // Use their names instead
                desc = name;
            }

            if (FUNC_GET_NUM_OF_DEVICE == function)
            {
                WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                             "    Enum device %d - %s", enumCount, name);

            }
            if ((FUNC_GET_DEVICE_NAME == function) &&
                (enumDeviceNo == enumCount))
            {

                // We have found the enum device, copy the name to buffer
                strncpy(enumDeviceName, desc, ednLen);
                enumDeviceName[ednLen-1] = '\0';
                keepSearching = false;
                // replace '\n' with '-'
                char * pret = strchr(enumDeviceName, '\n'/*0xa*/); //LF
                if (pret)
                    *pret = '-';
            }
            if ((FUNC_GET_DEVICE_NAME_FOR_AN_ENUM == function) &&
                (enumDeviceNo == enumCount))
            {
                // We have found the enum device, copy the name to buffer
                strncpy(enumDeviceName, name, ednLen);
                enumDeviceName[ednLen-1] = '\0';
                keepSearching = false;
            }
            if (keepSearching)
            {
                ++enumCount;
            }

            if (desc != name)
            {
                free(desc);
            }
        }

        free(name);

        if (!keepSearching)
        {
            break;
        }
    }

    err = LATE(snd_device_name_free_hint)(hints);
    if (err != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "GetDevicesInfo - device name free hint error: %s",
                     LATE(snd_strerror)(err));
        // Continue and return true anyways, since we did get the whole list.
    }

    if (FUNC_GET_NUM_OF_DEVICE == function)
    {
        if (enumCount == 1) // only default?
            enumCount = 0;
        return enumCount; // Normal return point for function 0
    }

    if (keepSearching)
    {
        // If we get here for function 1 and 2, we didn't find the specified
        // enum device
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "GetDevicesInfo - Could not find device name or numbers");
        return -1;
    }

    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::InputSanityCheckAfterUnlockedPeriod() const
{
    if (_handleRecord == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  input state has been modified during unlocked period");
        return -1;
    }
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::OutputSanityCheckAfterUnlockedPeriod() const
{
    if (_handlePlayout == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  output state has been modified during unlocked period");
        return -1;
    }
    return 0;
}

WebRtc_Word32 AudioDeviceLinuxALSA::ErrorRecovery(WebRtc_Word32 error,
                                                  snd_pcm_t* deviceHandle)
{
    int st = LATE(snd_pcm_state)(deviceHandle);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
               "Trying to recover from error: %s (%d) (state %d)",
               (LATE(snd_pcm_stream)(deviceHandle) == SND_PCM_STREAM_CAPTURE) ?
                   "capture" : "playout", LATE(snd_strerror)(error), error, st);

    // It is recommended to use snd_pcm_recover for all errors. If that function
    // cannot handle the error, the input error code will be returned, otherwise
    // 0 is returned. From snd_pcm_recover API doc: "This functions handles
    // -EINTR (4) (interrupted system call), -EPIPE (32) (playout overrun or
    // capture underrun) and -ESTRPIPE (86) (stream is suspended) error codes
    // trying to prepare given stream for next I/O."

    /** Open */
    //    SND_PCM_STATE_OPEN = 0,
    /** Setup installed */
    //    SND_PCM_STATE_SETUP,
    /** Ready to start */
    //    SND_PCM_STATE_PREPARED,
    /** Running */
    //    SND_PCM_STATE_RUNNING,
    /** Stopped: underrun (playback) or overrun (capture) detected */
    //    SND_PCM_STATE_XRUN,= 4
    /** Draining: running (playback) or stopped (capture) */
    //    SND_PCM_STATE_DRAINING,
    /** Paused */
    //    SND_PCM_STATE_PAUSED,
    /** Hardware is suspended */
    //    SND_PCM_STATE_SUSPENDED,
    //  ** Hardware is disconnected */
    //    SND_PCM_STATE_DISCONNECTED,
    //    SND_PCM_STATE_LAST = SND_PCM_STATE_DISCONNECTED

    // snd_pcm_recover isn't available in older alsa, e.g. on the FC4 machine
    // in Sthlm lab.

    int res = LATE(snd_pcm_recover)(deviceHandle, error, 1);
    if (0 == res)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                   "    Recovery - snd_pcm_recover OK");

        if ((error == -EPIPE || error == -ESTRPIPE) && // Buf underrun/overrun.
            _recording &&
            LATE(snd_pcm_stream)(deviceHandle) == SND_PCM_STREAM_CAPTURE)
        {
            // For capture streams we also have to repeat the explicit start()
            // to get data flowing again.
            int err = LATE(snd_pcm_start)(deviceHandle);
            if (err != 0)
            {
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "  Recovery - snd_pcm_start error: %u", err);
                return -1;
            }
        }

        if ((error == -EPIPE || error == -ESTRPIPE) &&  // Buf underrun/overrun.
            _playing &&
            LATE(snd_pcm_stream)(deviceHandle) == SND_PCM_STREAM_PLAYBACK)
        {
            // For capture streams we also have to repeat the explicit start() to get
            // data flowing again.
            int err = LATE(snd_pcm_start)(deviceHandle);
            if (err != 0)
            {
              WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                       "    Recovery - snd_pcm_start error: %s",
                       LATE(snd_strerror)(err));
              return -1;
            }
        }

        return -EPIPE == error ? 1 : 0;
    }
    else {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Terriable, it shouldn't happen");
    }

    return res;
}

// ============================================================================
//                                  Thread Methods
// ============================================================================

bool AudioDeviceLinuxALSA::PlayThreadFunc(void* pThis)
{
    return (static_cast<AudioDeviceLinuxALSA*>(pThis)->PlayThreadProcess());
}

bool AudioDeviceLinuxALSA::RecThreadFunc(void* pThis)
{
    return (static_cast<AudioDeviceLinuxALSA*>(pThis)->RecThreadProcess());
}

bool AudioDeviceLinuxALSA::PlayThreadProcess()
{
    if(!_playing)
        return false;

    int err;
    snd_pcm_sframes_t frames;
    snd_pcm_sframes_t avail_frames;

    Lock();
    //return a positive number of frames ready otherwise a negative error code
    avail_frames = LATE(snd_pcm_avail_update)(_handlePlayout);
    if (avail_frames < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                   "playout snd_pcm_avail_update error: %s",
                   LATE(snd_strerror)(avail_frames));
        ErrorRecovery(avail_frames, _handlePlayout);
        UnLock();
        return true;
    }
    else if (avail_frames == 0)
    {
        UnLock();

        //maximum tixe in milliseconds to wait, a negative value means infinity
        err = LATE(snd_pcm_wait)(_handlePlayout, 2);
        if (err == 0)
        { //timeout occured
            WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id,
                         "playout snd_pcm_wait timeout");
        }

        return true;
    }

    if (_playoutFramesLeft <= 0)
    {
        UnLock();
        _ptrAudioBuffer->RequestPlayoutData(_playoutFramesIn10MS);
        Lock();

        _playoutFramesLeft = _ptrAudioBuffer->GetPlayoutData(_playoutBuffer);
        assert(_playoutFramesLeft == _playoutFramesIn10MS);
    }

    if (static_cast<WebRtc_UWord32>(avail_frames) > _playoutFramesLeft)
        avail_frames = _playoutFramesLeft;

    int size = LATE(snd_pcm_frames_to_bytes)(_handlePlayout,
        _playoutFramesLeft);
    frames = LATE(snd_pcm_writei)(
        _handlePlayout,
        &_playoutBuffer[_playoutBufferSizeIn10MS - size],
        avail_frames);

    if (frames < 0)
    {
        WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id,
                     "playout snd_pcm_avail_update error: %s",
                     LATE(snd_strerror)(frames));
        _playoutFramesLeft = 0;
        ErrorRecovery(frames, _handlePlayout);
        UnLock();
        return true;
    }
    else {
        assert(frames==avail_frames);
        _playoutFramesLeft -= frames;
    }

    UnLock();
    return true;
}

bool AudioDeviceLinuxALSA::RecThreadProcess()
{
    if (!_recording)
        return false;

    int err;
    snd_pcm_sframes_t frames;
    snd_pcm_sframes_t avail_frames;
    WebRtc_Word8 buffer[_recordingBufferSizeIn10MS];

    Lock();

    //return a positive number of frames ready otherwise a negative error code
    avail_frames = LATE(snd_pcm_avail_update)(_handleRecord);
    if (avail_frames < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "capture snd_pcm_avail_update error: %s",
                     LATE(snd_strerror)(avail_frames));
        ErrorRecovery(avail_frames, _handleRecord);
        UnLock();
        return true;
    }
    else if (avail_frames == 0)
    { // no frame is available now
        UnLock();

        //maximum time in milliseconds to wait, a negative value means infinity
        err = LATE(snd_pcm_wait)(_handleRecord,
            ALSA_CAPTURE_WAIT_TIMEOUT);
        if (err == 0) //timeout occured
            WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id,
                         "caputre snd_pcm_wait timeout");

        return true;
    }

    if (static_cast<WebRtc_UWord32>(avail_frames) > _recordingFramesLeft)
        avail_frames = _recordingFramesLeft;

    frames = LATE(snd_pcm_readi)(_handleRecord,
        buffer, avail_frames); // frames to be written
    if (frames < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "caputre snd_pcm_readi error: %s",
                     LATE(snd_strerror)(frames));
        ErrorRecovery(frames, _handleRecord);
        UnLock();
        return true;
    }
    else if (frames > 0)
    {
        assert(frames == avail_frames);

        int left_size = LATE(snd_pcm_frames_to_bytes)(_handleRecord,
            _recordingFramesLeft);
        int size = LATE(snd_pcm_frames_to_bytes)(_handleRecord, frames);

        memcpy(&_recordingBuffer[_recordingBufferSizeIn10MS - left_size],
               buffer, size);
        _recordingFramesLeft -= frames;

        if (!_recordingFramesLeft)
        { // buf is full
            _recordingFramesLeft = _recordingFramesIn10MS;

            // store the recorded buffer (no action will be taken if the
            // #recorded samples is not a full buffer)
            _ptrAudioBuffer->SetRecordedBuffer(_recordingBuffer,
                                               _recordingFramesIn10MS);

            WebRtc_UWord32 currentMicLevel = 0;
            WebRtc_UWord32 newMicLevel = 0;

            if (AGC())
            {
                // store current mic level in the audio buffer if AGC is enabled
                if (MicrophoneVolume(currentMicLevel) == 0)
                {
                    if (currentMicLevel == 0xffffffff)
                        currentMicLevel = 100;
                    // this call does not affect the actual microphone volume
                    _ptrAudioBuffer->SetCurrentMicLevel(currentMicLevel);
                }
            }

            // calculate delay
            _playoutDelay = 0;
            _recordingDelay = 0;
            if (_handlePlayout)
            {
                err = LATE(snd_pcm_delay)(_handlePlayout,
                    &_playoutDelay); // returned delay in frames
                if (err < 0)
                {
                    // TODO(xians): Shall we call ErrorRecovery() here?
                    _playoutDelay = 0;
                    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                                 "playout snd_pcm_delay: %s",
                                 LATE(snd_strerror)(err));
                }
            }

            err = LATE(snd_pcm_delay)(_handleRecord,
                &_recordingDelay); // returned delay in frames
            if (err < 0)
            {
                // TODO(xians): Shall we call ErrorRecovery() here?
                _recordingDelay = 0;
                WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                             "caputre snd_pcm_delay: %s",
                             LATE(snd_strerror)(err));
            }

           // TODO(xians): Shall we add 10ms buffer delay to the record delay?
            _ptrAudioBuffer->SetVQEData(
                _playoutDelay * 1000 / _playoutFreq,
                _recordingDelay * 1000 / _recordingFreq, 0);

            // Deliver recorded samples at specified sample rate, mic level etc.
            // to the observer using callback.
            UnLock();
            _ptrAudioBuffer->DeliverRecordedData();
            Lock();

            if (AGC())
            {
                newMicLevel = _ptrAudioBuffer->NewMicLevel();
                if (newMicLevel != 0)
                {
                    // The VQE will only deliver non-zero microphone levels when a
                    // change is needed. Set this new mic level (received from the
                    // observer as return value in the callback).
                    if (SetMicrophoneVolume(newMicLevel) == -1)
                        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                                     "  the required modification of the "
                                     "microphone volume failed");
                }
            }
        }
    }

    UnLock();
    return true;
}

}  // namespace webrtc
