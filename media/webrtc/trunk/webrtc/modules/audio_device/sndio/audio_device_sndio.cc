/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <string.h>

#include "webrtc/modules/audio_device/audio_device_config.h"
#include "webrtc/modules/audio_device/audio_device_utility.h"
#include "webrtc/modules/audio_device/sndio/audio_device_sndio.h"

#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/system_wrappers/interface/trace.h"

#include "Latency.h"

#define LOG_FIRST_CAPTURE(x) LogTime(AsyncLatencyLogger::AudioCaptureBase, \
                                     reinterpret_cast<uint64_t>(x), 0)
#define LOG_CAPTURE_FRAMES(x, frames) LogLatency(AsyncLatencyLogger::AudioCapture, \
                                                 reinterpret_cast<uint64_t>(x), frames)
extern "C"
{
    static void playOnmove(void *arg, int delta)
    {
        static_cast<webrtc::AudioDeviceSndio *>(arg)->_playDelay -= delta;
    }

    static void recOnmove(void *arg, int delta)
    {
        static_cast<webrtc::AudioDeviceSndio *>(arg)->_recDelay += delta;
    }
}

namespace webrtc
{
AudioDeviceSndio::AudioDeviceSndio(const int32_t id) :
    _ptrAudioBuffer(NULL),
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _id(id),
    _recordingBuffer(NULL),
    _playoutBuffer(NULL),
    _playBufType(AudioDeviceModule::kFixedBufferSize),
    _initialized(false),
    _playHandle(NULL),
    _recHandle(NULL),
    _recording(false),
    _playing(false),
    _AGC(false),
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

AudioDeviceSndio::~AudioDeviceSndio()
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

void AudioDeviceSndio::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer)
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

int32_t AudioDeviceSndio::ActiveAudioLayer(
    AudioDeviceModule::AudioLayer& audioLayer) const
{
    audioLayer = AudioDeviceModule::kSndioAudio;
    return 0;
}

int32_t AudioDeviceSndio::Init()
{

    CriticalSectionScoped lock(&_critSect);

    if (_initialized)
    {
        return 0;
    }
    _playError = 0;
    _recError = 0;
    _initialized = true;
    return 0;
}

int32_t AudioDeviceSndio::Terminate()
{
    if (!_initialized)
    {
        return 0;
    }

    CriticalSectionScoped lock(&_critSect);

    if (_playing)
    {
        StopPlayout();
    }

    if (_recording)
    {
        StopRecording();
    }

    if (_playHandle)
    {
        sio_close(_playHandle);
        delete [] _playoutBuffer;
        _playHandle = NULL;
    }

    if (_recHandle)
    {
        sio_close(_recHandle);
        delete [] _recordingBuffer;
        _recHandle = NULL;
    }

    _initialized = false;
    return 0;
}

bool AudioDeviceSndio::Initialized() const
{
    return (_initialized);
}

int32_t AudioDeviceSndio::InitSpeaker()
{
    return 0;
}


int32_t AudioDeviceSndio::InitMicrophone()
{
    return 0;
}

bool AudioDeviceSndio::SpeakerIsInitialized() const
{
    return true;
}

bool AudioDeviceSndio::MicrophoneIsInitialized() const
{
    return true;
}

int32_t AudioDeviceSndio::SpeakerVolumeIsAvailable(bool& available)
{
    available = true;
    return 0;
}

int32_t AudioDeviceSndio::SetSpeakerVolume(uint32_t volume)
{

    CriticalSectionScoped lock(&_critSect);

    // XXX: call sio_onvol()
    sio_setvol(_playHandle, volume);
    return 0;
}

int32_t AudioDeviceSndio::SpeakerVolume(uint32_t& volume) const
{

    CriticalSectionScoped lock(&_critSect);

    // XXX: copy value reported by sio_onvol() call-back
    volume = 0;
    return 0;
}

int32_t AudioDeviceSndio::SetWaveOutVolume(uint16_t volumeLeft,
                                           uint16_t volumeRight)
{
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceSndio::WaveOutVolume(
    uint16_t& /*volumeLeft*/,
    uint16_t& /*volumeRight*/) const
{
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceSndio::MaxSpeakerVolume(
    uint32_t& maxVolume) const
{
    maxVolume = SIO_MAXVOL;
    return 0;
}

int32_t AudioDeviceSndio::MinSpeakerVolume(
    uint32_t& minVolume) const
{
    minVolume = 0;
    return 0;
}

int32_t AudioDeviceSndio::SpeakerVolumeStepSize(
    uint16_t& stepSize) const
{
    stepSize = 1;
    return 0;
}

int32_t AudioDeviceSndio::SpeakerMuteIsAvailable(bool& available)
{
    available = false;
    return 0;
}

int32_t AudioDeviceSndio::SetSpeakerMute(bool enable)
{
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceSndio::SpeakerMute(bool& enabled) const
{
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceSndio::MicrophoneMuteIsAvailable(bool& available)
{
    available = false;
    return 0;
}

int32_t AudioDeviceSndio::SetMicrophoneMute(bool enable)
{
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceSndio::MicrophoneMute(bool& enabled) const
{
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceSndio::MicrophoneBoostIsAvailable(bool& available)
{
    available = false;
    return 0;
}

int32_t AudioDeviceSndio::SetMicrophoneBoost(bool enable)
{
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceSndio::MicrophoneBoost(bool& enabled) const
{
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceSndio::StereoRecordingIsAvailable(bool& available)
{
    available = true;
    return 0;
}

int32_t AudioDeviceSndio::SetStereoRecording(bool enable)
{

    if (enable)
    {
        _recChannels = 2;
    } else {
        _recChannels = 1;
    }
    return 0;
}

int32_t AudioDeviceSndio::StereoRecording(bool& enabled) const
{

    if (_recChannels == 2)
    {
        enabled = true;
    } else {
        enabled = false;
    }
    return 0;
}

int32_t AudioDeviceSndio::StereoPlayoutIsAvailable(bool& available)
{
    available = true;
    return 0;
}

int32_t AudioDeviceSndio::SetStereoPlayout(bool enable)
{

    if (enable)
    {
        _playChannels = 2;
    } else {
        _playChannels = 1;
    }
    return 0;
}

int32_t AudioDeviceSndio::StereoPlayout(bool& enabled) const
{
    if (_playChannels == 2)
    {
        enabled = true;
    } else {
        enabled = false;
    }
    return 0;
}

int32_t AudioDeviceSndio::SetAGC(bool enable)
{
    _AGC = enable;

    return 0;
}

bool AudioDeviceSndio::AGC() const
{
    return _AGC;
}

int32_t AudioDeviceSndio::MicrophoneVolumeIsAvailable(bool& available)
{
    available = false;
    return 0;
}

int32_t AudioDeviceSndio::SetMicrophoneVolume(uint32_t volume)
{
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceSndio::MicrophoneVolume(uint32_t& volume) const
{
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceSndio::MaxMicrophoneVolume(
    uint32_t& maxVolume) const
{
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceSndio::MinMicrophoneVolume(
    uint32_t& minVolume) const
{
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceSndio::MicrophoneVolumeStepSize(
    uint16_t& stepSize) const
{
    return -1;
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
}

int16_t AudioDeviceSndio::PlayoutDevices()
{
    return 1;
}

int32_t AudioDeviceSndio::SetPlayoutDevice(uint16_t index)
{
    if (index != 0) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  device index != 0");
        return -1;
    }
    return 0;
}

int32_t AudioDeviceSndio::SetPlayoutDevice(
    AudioDeviceModule::WindowsDeviceType /*device*/)
{
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "WindowsDeviceType not supported");
    return -1;
}

int32_t AudioDeviceSndio::PlayoutDeviceName(
    uint16_t index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize])
{

    if (index != 0 || (name == NULL))
    {
        return -1;
    }

    strlcpy(name, SIO_DEVANY, kAdmMaxDeviceNameSize);

    if (guid != NULL)
    {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    return 0;
}

int32_t AudioDeviceSndio::RecordingDeviceName(
    uint16_t index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize])
{

    if (index != 0 || (name == NULL))
    {
        return -1;
    }

    strlcpy(name, SIO_DEVANY, kAdmMaxDeviceNameSize);

    if (guid != NULL)
    {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    return 0;
}

int16_t AudioDeviceSndio::RecordingDevices()
{
    return 1;
}

int32_t AudioDeviceSndio::SetRecordingDevice(uint16_t index)
{
    if (index != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  device index != 0");
        return -1;
    }
    return 0;
}

int32_t AudioDeviceSndio::SetRecordingDevice(
    AudioDeviceModule::WindowsDeviceType /*device*/)
{
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "WindowsDeviceType not supported");
    return -1;
}

int32_t AudioDeviceSndio::PlayoutIsAvailable(bool& available)
{
    struct sio_hdl *hdl;

    hdl = sio_open(SIO_DEVANY, SIO_PLAY, 0);
    if (hdl == NULL)
    {
        available = false;
    } else {
        sio_close(hdl);
        available = true;
    }
    return 0;
}

int32_t AudioDeviceSndio::RecordingIsAvailable(bool& available)
{
    struct sio_hdl *hdl;

    hdl = sio_open(SIO_DEVANY, SIO_REC, 0);
    if (hdl == NULL)
    {
        available = false;
    } else {
        sio_close(hdl);
        available = true;
    }
    return 0;
}

int32_t AudioDeviceSndio::InitPlayout()
{
    CriticalSectionScoped lock(&_critSect);
    if (_playing) {
        return -1;
    }

    if (_playHandle != NULL)
    {
        return 0;
    }

    _playHandle = sio_open(SIO_DEVANY, SIO_PLAY, 0);
    if (_playHandle == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Error opening play device");
        return -1;
    }

    sio_initpar(&_playParams);
    _playParams.rate = 48000;
    _playParams.pchan = 2;
    _playParams.bits = 16;
    _playParams.sig = 1;
    _playParams.le = SIO_LE_NATIVE;
    _playParams.appbufsz = _playParams.rate * 40 / 1000;

    if (!sio_setpar(_playHandle, &_playParams))
    {
       WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                    "  Error setting _playParams");
        sio_close(_playHandle);
        _playHandle = NULL;
        return -1;
    }
    if (!sio_getpar(_playHandle, &_playParams))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Error getting _playParams");
        sio_close(_playHandle);
        _playHandle = NULL;
        return -1;
    }

    _playFrames = _playParams.rate / 100;
    _playoutBuffer = new int8_t[_playFrames *
                                _playParams.bps *
                                _playParams.pchan];
    if (_playoutBuffer == NULL)
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "   failed to alloc play buffer");
        sio_close(_playHandle);
        _playHandle = NULL;
        return -1;
    }

    if (_ptrAudioBuffer)
    {
        // Update webrtc audio buffer with the selected parameters
        _ptrAudioBuffer->SetPlayoutSampleRate(_playParams.rate);
        _ptrAudioBuffer->SetPlayoutChannels(_playParams.pchan);
    }

    _playDelay = 0;
    sio_onmove(_playHandle, playOnmove, this);
    return 0;
}

int32_t AudioDeviceSndio::InitRecording()
{
    CriticalSectionScoped lock(&_critSect);

    if (_recording)
    {
        return -1;
    }

    if (_recHandle != NULL)
    {
        return 0;
    }

    _recHandle = sio_open(SIO_DEVANY, SIO_REC, 0);
    if (_recHandle == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Error opening rec device");
        return -1;
    }

    sio_initpar(&_recParams);
    _recParams.rate = 48000;
    _recParams.rchan = 2;
    _recParams.bits = 16;
    _recParams.sig = 1;
    _recParams.le = SIO_LE_NATIVE;
    _recParams.appbufsz = _recParams.rate * 40 / 1000;

    if (!sio_setpar(_recHandle, &_recParams))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Error setting _recParams");
        sio_close(_recHandle);
        _recHandle = NULL;
        return -1;
    }
    if (!sio_getpar(_recHandle, &_recParams))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Error getting _recParams");
        sio_close(_recHandle);
        _recHandle = NULL;
        return -1;
    }

    _recFrames = _recParams.rate / 100;
    _recordingBuffer = new int8_t[_recFrames *
                                  _recParams.bps *
                                  _recParams.rchan];
    if (_recordingBuffer == NULL)
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "   failed to alloc rec buffer");
        sio_close(_recHandle);
        _recHandle = NULL;
        return -1;
    }

    if (_ptrAudioBuffer)
    {
        _ptrAudioBuffer->SetRecordingSampleRate(_recParams.rate);
        _ptrAudioBuffer->SetRecordingChannels(_recParams.rchan);
    }

    _recDelay = 0;
    sio_onmove(_recHandle, recOnmove, this);
    return 0;
}

int32_t AudioDeviceSndio::StartRecording()
{
    const char* threadName = "webrtc_audio_module_capture_thread";

    if (_recHandle == NULL)
    {
        return -1;
    }

    if (_recording)
    {
        return 0;
    }

    _ptrThreadRec = ThreadWrapper::CreateThread(RecThreadFunc,
                                                this,
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

    _playDelay = 0;

    if (!sio_start(_recHandle))
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  couldn't start recording");
        _ptrThreadRec.reset();
        delete [] _recordingBuffer;
        _recordingBuffer = NULL;
        return -1;
    }

    if (!_ptrThreadRec->Start())
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to start the rec audio thread");
        _recording = false;
        sio_stop(_recHandle);
        _ptrThreadRec.reset();
        delete [] _recordingBuffer;
        _recordingBuffer = NULL;
        return -1;
    }
    _recording = true;
    return 0;
}

int32_t AudioDeviceSndio::StopRecording()
{

    {
      CriticalSectionScoped lock(&_critSect);

      if (_recHandle == NULL)
      {
          return 0;
      }

      _recording = false;
    }

    if (_ptrThreadRec)
    {
        _ptrThreadRec->Stop();
        _ptrThreadRec.reset();
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "    failed to stop the rec audio thread");
        return -1;
    }

    sio_stop(_recHandle);

    return 0;
}

bool AudioDeviceSndio::RecordingIsInitialized() const
{
    return (_recHandle != NULL);
}

bool AudioDeviceSndio::Recording() const
{
    return (_recording);
}

bool AudioDeviceSndio::PlayoutIsInitialized() const
{
    return (_playHandle != NULL);
}

int32_t AudioDeviceSndio::StartPlayout()
{
    const char* threadName = "webrtc_audio_module_play_thread";

    if (_playHandle == NULL)
    {
        return -1;
    }

    if (_playing)
    {
        return 0;
    }

    _ptrThreadPlay =  ThreadWrapper::CreateThread(PlayThreadFunc,
                                                  this,
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

    if (!sio_start(_playHandle)) {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "    failed to start audio playback");
        _ptrThreadPlay.reset();
        delete [] _playoutBuffer;
        _playoutBuffer = NULL;
        return -1;
    }

    if (!_ptrThreadPlay->Start())
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to start the play audio thread");
        sio_stop(_playHandle);
        _ptrThreadPlay.reset();
        _ptrThreadPlay = NULL;
        delete [] _playoutBuffer;
        _playoutBuffer = NULL;
        return -1;
    }
    _playing = true;
    return 0;
}

int32_t AudioDeviceSndio::StopPlayout()
{

    {
      CriticalSectionScoped lock(&_critSect);
      if (_playHandle == NULL)
      {
          return 0;
      }
      _playing = false;
    }

    if (_ptrThreadPlay)
    {
        _ptrThreadPlay->Stop();
        _ptrThreadPlay.reset();
    }

    sio_stop(_playHandle);

    CriticalSectionScoped lock(&_critSect);

    delete [] _playoutBuffer;
    _playoutBuffer = NULL;
    return 0;
}

int32_t AudioDeviceSndio::PlayoutDelay(uint16_t& delayMS) const
{
    CriticalSectionScoped lock(&_critSect);
    delayMS = (uint16_t) (_playDelay * 1000) / _playParams.rate;
    return 0;
}

int32_t AudioDeviceSndio::RecordingDelay(uint16_t& delayMS) const
{
    CriticalSectionScoped lock(&_critSect);
    delayMS = (uint16_t) (_recDelay * 1000) / _recParams.rate;
    return 0;
}

bool AudioDeviceSndio::Playing() const
{
    return (_playing);
}
// ----------------------------------------------------------------------------
//  SetPlayoutBuffer
// ----------------------------------------------------------------------------

int32_t AudioDeviceSndio::SetPlayoutBuffer(
    const AudioDeviceModule::BufferType type,
    uint16_t sizeMS)
{
    _playBufType = type;
    if (type == AudioDeviceModule::kFixedBufferSize)
    {
        _playBufDelayFixed = sizeMS;
    }
    return 0;
}

int32_t AudioDeviceSndio::PlayoutBuffer(
    AudioDeviceModule::BufferType& type,
    uint16_t& sizeMS) const
{
    type = _playBufType;
    if (type == AudioDeviceModule::kFixedBufferSize)
    {
        sizeMS = _playBufDelayFixed;
    } else {
        sizeMS = _playBufDelay;
    }
    return 0;
}

int32_t AudioDeviceSndio::CPULoad(uint16_t& load) const
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
               "  API call not supported on this platform");
    return -1;
}

bool AudioDeviceSndio::PlayoutWarning() const
{
    CriticalSectionScoped lock(&_critSect);
    return (_playWarning > 0);
}

bool AudioDeviceSndio::PlayoutError() const
{
    CriticalSectionScoped lock(&_critSect);
    return (_playError > 0);
}

bool AudioDeviceSndio::RecordingWarning() const
{
    CriticalSectionScoped lock(&_critSect);
    return (_recWarning > 0);
}

bool AudioDeviceSndio::RecordingError() const
{
    CriticalSectionScoped lock(&_critSect);
    return (_recError > 0);
}

void AudioDeviceSndio::ClearPlayoutWarning()
{
    CriticalSectionScoped lock(&_critSect);
    _playWarning = 0;
}

void AudioDeviceSndio::ClearPlayoutError()
{
    CriticalSectionScoped lock(&_critSect);
    _playError = 0;
}

void AudioDeviceSndio::ClearRecordingWarning()
{
    CriticalSectionScoped lock(&_critSect);
    _recWarning = 0;
}

void AudioDeviceSndio::ClearRecordingError()
{
    CriticalSectionScoped lock(&_critSect);
    _recError = 0;
}

bool AudioDeviceSndio::PlayThreadFunc(void* pThis)
{
    return (static_cast<AudioDeviceSndio*>(pThis)->PlayThreadProcess());
}

bool AudioDeviceSndio::RecThreadFunc(void* pThis)
{
    return (static_cast<AudioDeviceSndio*>(pThis)->RecThreadProcess());
}

bool AudioDeviceSndio::PlayThreadProcess()
{
    int n;
    bool res;

    res = true;
    CriticalSectionScoped lock(&_critSect);
    // XXX: update volume here
    _ptrAudioBuffer->RequestPlayoutData(_playFrames);
    n = _ptrAudioBuffer->GetPlayoutData(_playoutBuffer);
    sio_write(_playHandle, _playoutBuffer, n *
             _playParams.bps *
             _playParams.pchan);
    if (sio_eof(_playHandle))
    {
        res = false;
        _playError = 1;
    }
    _playDelay += n;
    _critSect.Leave();
    return res;
}

bool AudioDeviceSndio::RecThreadProcess()
{
    int n, todo;
    int8_t *data;
    bool res;

    res = true;
    CriticalSectionScoped lock(&_critSect);
    data = _recordingBuffer;
    todo = _recFrames * _recParams.bps * _recParams.rchan;
    while (todo > 0)
    {
        n = sio_read(_recHandle, data, todo);
        if (n == 0)
        {
             WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                          "sio_read failed, device disconnected?");
            res = false;
            _recError = 1;
            break;
        }
        data += n;
        todo -= n;
    }
    if (res)
    {
        _ptrAudioBuffer->SetVQEData(
            _playDelay * 1000 / _playParams.rate,
            _recDelay * 1000 / _recParams.rate, 0);
        _ptrAudioBuffer->SetRecordedBuffer(_recordingBuffer, _recFrames);
        _ptrAudioBuffer->DeliverRecordedData();
    }
    _recDelay -= _recFrames;
    return res;
}
} // namespace "webrtc"
