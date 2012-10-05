/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <time.h>
#include <sys/time.h>

#include "audio_device_utility.h"
#include "audio_device_android_opensles.h"
#include "audio_device_config.h"

#include "trace.h"
#include "thread_wrapper.h"
#include "event_wrapper.h"

#ifdef WEBRTC_ANDROID_DEBUG
#include <android/log.h>
#define WEBRTC_TRACE(a,b,c,...)  __android_log_print(                  \
           ANDROID_LOG_DEBUG, "WebRTC ADM OpenSLES", __VA_ARGS__)
#endif

namespace webrtc {

// ============================================================================
//                            Construction & Destruction
// ============================================================================

// ----------------------------------------------------------------------------
//  AudioDeviceAndroidOpenSLES - ctor
// ----------------------------------------------------------------------------

AudioDeviceAndroidOpenSLES::AudioDeviceAndroidOpenSLES(const WebRtc_Word32 id) :
    _ptrAudioBuffer(NULL),
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _id(id),
    _slEngineObject(NULL),
    _slPlayer(NULL),
    _slEngine(NULL),
    _slPlayerPlay(NULL),
    _slOutputMixObject(NULL),
    _slSpeakerVolume(NULL),
    _slRecorder(NULL),
    _slRecorderRecord(NULL),
    _slAudioIODeviceCapabilities(NULL),
    _slRecorderSimpleBufferQueue(NULL),
    _slMicVolume(NULL),
    _micDeviceId(0),
    _recQueueSeq(0),
    _timeEventRec(*EventWrapper::Create()),
    _ptrThreadRec(NULL),
    _recThreadID(0),
    _playQueueSeq(0),
    _recCurrentSeq(0),
    _recBufferTotalSize(0),
    _recordingDeviceIsSpecified(false),
    _playoutDeviceIsSpecified(false),
    _initialized(false),
    _recording(false),
    _playing(false),
    _recIsInitialized(false),
    _playIsInitialized(false),
    _micIsInitialized(false),
    _speakerIsInitialized(false),
    _playWarning(0),
    _playError(0),
    _recWarning(0),
    _recError(0),
    _playoutDelay(0),
    _recordingDelay(0),
    _AGC(false),
    _adbSampleRate(0),
    _samplingRateIn(SL_SAMPLINGRATE_16),
    _samplingRateOut(SL_SAMPLINGRATE_16),
    _maxSpeakerVolume(0),
    _minSpeakerVolume(0),
    _loudSpeakerOn(false) {
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, id, "%s created",
                 __FUNCTION__);
    memset(_playQueueBuffer, 0, sizeof(_playQueueBuffer));
    memset(_recQueueBuffer, 0, sizeof(_recQueueBuffer));
    memset(_recBuffer, 0, sizeof(_recBuffer));
    memset(_recLength, 0, sizeof(_recLength));
    memset(_recSeqNumber, 0, sizeof(_recSeqNumber));
}

AudioDeviceAndroidOpenSLES::~AudioDeviceAndroidOpenSLES() {
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s destroyed",
                 __FUNCTION__);

    Terminate();

    delete &_timeEventRec;
    delete &_critSect;
}

// ============================================================================
//                                     API
// ============================================================================

void AudioDeviceAndroidOpenSLES::AttachAudioBuffer(
    AudioDeviceBuffer* audioBuffer) {

    CriticalSectionScoped lock(&_critSect);

    _ptrAudioBuffer = audioBuffer;

    // inform the AudioBuffer about default settings for this implementation
    _ptrAudioBuffer->SetRecordingSampleRate(N_REC_SAMPLES_PER_SEC);
    _ptrAudioBuffer->SetPlayoutSampleRate(N_PLAY_SAMPLES_PER_SEC);
    _ptrAudioBuffer->SetRecordingChannels(N_REC_CHANNELS);
    _ptrAudioBuffer->SetPlayoutChannels(N_PLAY_CHANNELS);
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::ActiveAudioLayer(
    AudioDeviceModule::AudioLayer& audioLayer) const {

    audioLayer = AudioDeviceModule::kPlatformDefaultAudio;

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::Init() {

    CriticalSectionScoped lock(&_critSect);

    if (_initialized) {
        return 0;
    }

    _playWarning = 0;
    _playError = 0;
    _recWarning = 0;
    _recError = 0;

    SLEngineOption EngineOption[] = {
      { (SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE },
    };
    WebRtc_Word32 res = slCreateEngine(&_slEngineObject, 1, EngineOption, 0,
                                       NULL, NULL);
    //WebRtc_Word32 res = slCreateEngine( &_slEngineObject, 0, NULL, 0, NULL,
    //    NULL);
    if (res != SL_RESULT_SUCCESS) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to create SL Engine Object");
        return -1;
    }
    /* Realizing the SL Engine in synchronous mode. */
    if ((*_slEngineObject)->Realize(_slEngineObject, SL_BOOLEAN_FALSE)
            != SL_RESULT_SUCCESS) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to Realize SL Engine");
        return -1;
    }

    if ((*_slEngineObject)->GetInterface(_slEngineObject, SL_IID_ENGINE,
                                         (void*) &_slEngine)
            != SL_RESULT_SUCCESS) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to get SL Engine interface");
        return -1;
    }

    // Check the sample rate to be used for playback and recording
    if (InitSampleRate() != 0) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "%s: Failed to init samplerate", __FUNCTION__);
        return -1;
    }

    // Set the audio device buffer sampling rate, we assume we get the same
    // for play and record
    if (_ptrAudioBuffer->SetRecordingSampleRate(_adbSampleRate) < 0) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Could not set audio device buffer recording "
                         "sampling rate (%d)", _adbSampleRate);
    }
    if (_ptrAudioBuffer->SetPlayoutSampleRate(_adbSampleRate) < 0) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Could not set audio device buffer playout sampling "
                         "rate (%d)", _adbSampleRate);
    }

    _initialized = true;

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::Terminate() {

    CriticalSectionScoped lock(&_critSect);

    if (!_initialized) {
        return 0;
    }

    // RECORDING
    StopRecording();

    _micIsInitialized = false;
    _recordingDeviceIsSpecified = false;

    // PLAYOUT
    StopPlayout();

    if (_slEngineObject != NULL) {
        (*_slEngineObject)->Destroy(_slEngineObject);
        _slEngineObject = NULL;
        _slEngine = NULL;
    }

    _initialized = false;

    return 0;
}

bool AudioDeviceAndroidOpenSLES::Initialized() const {

    return (_initialized);
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SpeakerIsAvailable(bool& available) {

    // We always assume it's available
    available = true;

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::InitSpeaker() {

    CriticalSectionScoped lock(&_critSect);

    if (_playing) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  Playout already started");
        return -1;
    }

    if (!_playoutDeviceIsSpecified) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Playout device is not specified");
        return -1;
    }

    // Nothing needs to be done here, we use a flag to have consistent
    // behavior with other platforms
    _speakerIsInitialized = true;

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MicrophoneIsAvailable(
    bool& available) {

    // We always assume it's available
    available = true;

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::InitMicrophone() {

    CriticalSectionScoped lock(&_critSect);

    if (_recording) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  Recording already started");
        return -1;
    }

    if (!_recordingDeviceIsSpecified) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Recording device is not specified");
        return -1;
    }

    // Nothing needs to be done here, we use a flag to have consistent
    // behavior with other platforms
    _micIsInitialized = true;

    return 0;
}

bool AudioDeviceAndroidOpenSLES::SpeakerIsInitialized() const {

    return _speakerIsInitialized;
}

bool AudioDeviceAndroidOpenSLES::MicrophoneIsInitialized() const {

    return _micIsInitialized;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SpeakerVolumeIsAvailable(
    bool& available) {

    available = true; // We assume we are always be able to set/get volume

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetSpeakerVolume(
    WebRtc_UWord32 volume) {

    if (!_speakerIsInitialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Speaker not initialized");
        return -1;
    }

    if (_slEngineObject == NULL) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "SetSpeakerVolume, SL Engine object doesnt exist");
        return -1;
    }

    if (_slEngine == NULL) {
        // Get the SL Engine Interface which is implicit
        if ((*_slEngineObject)->GetInterface(_slEngineObject, SL_IID_ENGINE,
                                             (void*) &_slEngine)
                != SL_RESULT_SUCCESS) {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  failed to GetInterface SL Engine Interface");
            return -1;
        }
    }
    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SpeakerVolume(
    WebRtc_UWord32& volume) const {
    return 0;
}

// ----------------------------------------------------------------------------
//  SetWaveOutVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetWaveOutVolume(
    WebRtc_UWord16 /*volumeLeft*/,
    WebRtc_UWord16 /*volumeRight*/) {

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

// ----------------------------------------------------------------------------
//  WaveOutVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidOpenSLES::WaveOutVolume(
    WebRtc_UWord16& /*volumeLeft*/,
    WebRtc_UWord16& /*volumeRight*/) const {

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MaxSpeakerVolume(
    WebRtc_UWord32& maxVolume) const {

    if (!_speakerIsInitialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Speaker not initialized");
        return -1;
    }

    maxVolume = _maxSpeakerVolume;

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MinSpeakerVolume(
    WebRtc_UWord32& minVolume) const {

    if (!_speakerIsInitialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Speaker not initialized");
        return -1;//
    }

    minVolume = _minSpeakerVolume;

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SpeakerVolumeStepSize(
    WebRtc_UWord16& stepSize) const {

    if (!_speakerIsInitialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Speaker not initialized");
        return -1;
    }
    stepSize = 1;

    return 0;
}

// ----------------------------------------------------------------------------
//  SpeakerMuteIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SpeakerMuteIsAvailable(
    bool& available) {

    available = false; // Speaker mute not supported on Android

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetSpeakerMute(bool /*enable*/) {

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SpeakerMute(bool& /*enabled*/) const {

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MicrophoneMuteIsAvailable(
    bool& available) {

    available = false; // Mic mute not supported on Android

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetMicrophoneMute(bool /*enable*/) {

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MicrophoneMute(
    bool& /*enabled*/) const {

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MicrophoneBoostIsAvailable(
    bool& available) {

    available = false; // Mic boost not supported on Android

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetMicrophoneBoost(bool enable) {

    if (!_micIsInitialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Microphone not initialized");
        return -1;
    }

    if (enable) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Enabling not available");
        return -1;
    }

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MicrophoneBoost(bool& enabled) const {

    if (!_micIsInitialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Microphone not initialized");
        return -1;
    }

    enabled = false;

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::StereoRecordingIsAvailable(
    bool& available) {

    available = false; // Stereo recording not supported on Android

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetStereoRecording(bool enable) {

    if (enable) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Enabling not available");
        return -1;
    }

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::StereoRecording(bool& enabled) const {

    enabled = false;

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::StereoPlayoutIsAvailable(
    bool& available) {

    available = false; // Stereo playout not supported on Android

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetStereoPlayout(bool enable) {

    if (enable) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Enabling not available");
        return -1;
    }

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::StereoPlayout(bool& enabled) const {

    enabled = false;

    return 0;
}


WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetAGC(bool enable) {

    _AGC = enable;

    return 0;
}

bool AudioDeviceAndroidOpenSLES::AGC() const {

    return _AGC;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MicrophoneVolumeIsAvailable(
    bool& available) {

    available = true;

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetMicrophoneVolume(
    WebRtc_UWord32 volume) {

    if (_slEngineObject == NULL) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "SetMicrophoneVolume, SL Engine Object doesnt exist");
        return -1;
    }

    /* Get the optional DEVICE VOLUME interface from the engine */
    if (_slMicVolume == NULL) {
        // Get the optional DEVICE VOLUME interface from the engine
        if ((*_slEngineObject)->GetInterface(_slEngineObject,
                                             SL_IID_DEVICEVOLUME,
                                             (void*) &_slMicVolume)
                != SL_RESULT_SUCCESS) {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  failed to create Output Mix object");
        }
    }

    if (_slMicVolume != NULL) {
        WebRtc_Word32 vol(0);
        vol = ((volume * (_maxSpeakerVolume - _minSpeakerVolume) +
                (int) (255 / 2)) / (255)) + _minSpeakerVolume;
        if ((*_slMicVolume)->SetVolume(_slMicVolume, _micDeviceId, vol)
                != SL_RESULT_SUCCESS) {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  failed to create Output Mix object");
        }
    }

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MicrophoneVolume(
    WebRtc_UWord32& /*volume*/) const {
    return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MaxMicrophoneVolume(
    WebRtc_UWord32& /*maxVolume*/) const {
    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MinMicrophoneVolume(
    WebRtc_UWord32& minVolume) const {

    minVolume = 0;
    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::MicrophoneVolumeStepSize(
    WebRtc_UWord16& stepSize) const {

    stepSize = 1;
    return 0;
}

WebRtc_Word16 AudioDeviceAndroidOpenSLES::PlayoutDevices() {

    return 1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetPlayoutDevice(
    WebRtc_UWord16 index) {

    if (_playIsInitialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Playout already initialized");
        return -1;
    }

    if (0 != index) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Device index is out of range [0,0]");
        return -1;
    }

    // Do nothing but set a flag, this is to have consistent behaviour
    // with other platforms
    _playoutDeviceIsSpecified = true;

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetPlayoutDevice(
    AudioDeviceModule::WindowsDeviceType /*device*/) {

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::PlayoutDeviceName(
    WebRtc_UWord16 index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize]) {

    if (0 != index) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Device index is out of range [0,0]");
        return -1;
    }

    // Return empty string
    memset(name, 0, kAdmMaxDeviceNameSize);

    if (guid) {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::RecordingDeviceName(
    WebRtc_UWord16 index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize]) {

    if (0 != index) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Device index is out of range [0,0]");
        return -1;
    }

    // Return empty string
    memset(name, 0, kAdmMaxDeviceNameSize);

    if (guid) {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    return 0;
}

WebRtc_Word16 AudioDeviceAndroidOpenSLES::RecordingDevices() {

    return 1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetRecordingDevice(
    WebRtc_UWord16 index) {

    if (_recIsInitialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Recording already initialized");
        return -1;
    }

    if (0 != index) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Device index is out of range [0,0]");
        return -1;
    }

    // Do nothing but set a flag, this is to have consistent behaviour with
    // other platforms
    _recordingDeviceIsSpecified = true;

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetRecordingDevice(
    AudioDeviceModule::WindowsDeviceType /*device*/) {

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::PlayoutIsAvailable(bool& available) {

    available = false;

    // Try to initialize the playout side
    WebRtc_Word32 res = InitPlayout();

    // Cancel effect of initialization
    StopPlayout();

    if (res != -1) {
        available = true;
    }

    return res;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::RecordingIsAvailable(
    bool& available) {

    available = false;

    // Try to initialize the playout side
    WebRtc_Word32 res = InitRecording();

    // Cancel effect of initialization
    StopRecording();

    if (res != -1) {
        available = true;
    }

    return res;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::InitPlayout() {

    CriticalSectionScoped lock(&_critSect);

    if (!_initialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "  Not initialized");
        return -1;
    }

    if (_playing) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  Playout already started");
        return -1;
    }

    if (!_playoutDeviceIsSpecified) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Playout device is not specified");
        return -1;
    }

    if (_playIsInitialized) {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Playout already initialized");
        return 0;
    }

    // Initialize the speaker
    if (InitSpeaker() == -1) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  InitSpeaker() failed");
    }

    if (_slEngineObject == NULL || _slEngine == NULL) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  SLObject or Engiine is NULL");
        return -1;
    }

    WebRtc_Word32 res = -1;
    SLDataFormat_PCM pcm;
    SLDataSource audioSource;
    SLDataLocator_AndroidSimpleBufferQueue simpleBufferQueue;
    SLDataSink audioSink;
    SLDataLocator_OutputMix locator_outputmix;

    // Create Output Mix object to be used by player
    SLInterfaceID ids[N_MAX_INTERFACES];
    SLboolean req[N_MAX_INTERFACES];
    for (unsigned int i = 0; i < N_MAX_INTERFACES; i++) {
        ids[i] = SL_IID_NULL;
        req[i] = SL_BOOLEAN_FALSE;
    }
    ids[0] = SL_IID_ENVIRONMENTALREVERB;
    res = (*_slEngine)->CreateOutputMix(_slEngine, &_slOutputMixObject, 1, ids,
                                        req);
    if (res != SL_RESULT_SUCCESS) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to get SL Output Mix object");
        return -1;
    }
    // Realizing the Output Mix object in synchronous mode.
    res = (*_slOutputMixObject)->Realize(_slOutputMixObject, SL_BOOLEAN_FALSE);
    if (res != SL_RESULT_SUCCESS) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to realize SL Output Mix object");
        return -1;
    }

    // The code below can be moved to startplayout instead
    /* Setup the data source structure for the buffer queue */
    simpleBufferQueue.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
    /* Two buffers in our buffer queue, to have low latency*/
    simpleBufferQueue.numBuffers = N_PLAY_QUEUE_BUFFERS;
    // TODO(xians), figure out if we should support stereo playout for android
    /* Setup the format of the content in the buffer queue */
    pcm.formatType = SL_DATAFORMAT_PCM;
    pcm.numChannels = 1;
    // _samplingRateOut is initilized in InitSampleRate()
    pcm.samplesPerSec = SL_SAMPLINGRATE_16;
    pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
    pcm.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
    pcm.channelMask = SL_SPEAKER_FRONT_CENTER;
    pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;
    audioSource.pFormat = (void *) &pcm;
    audioSource.pLocator = (void *) &simpleBufferQueue;
    /* Setup the data sink structure */
    locator_outputmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
    locator_outputmix.outputMix = _slOutputMixObject;
    audioSink.pLocator = (void *) &locator_outputmix;
    audioSink.pFormat = NULL;

    // Set arrays required[] and iidArray[] for SEEK interface
    // (PlayItf is implicit)
    ids[0] = SL_IID_BUFFERQUEUE;
    ids[1] = SL_IID_EFFECTSEND;
    req[0] = SL_BOOLEAN_TRUE;
    req[1] = SL_BOOLEAN_TRUE;
    // Create the music player
    res = (*_slEngine)->CreateAudioPlayer(_slEngine, &_slPlayer, &audioSource,
                                          &audioSink, 2, ids, req);
    if (res != SL_RESULT_SUCCESS) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to create Audio Player");
        return -1;
    }

    // Realizing the player in synchronous mode.
    res = (*_slPlayer)->Realize(_slPlayer, SL_BOOLEAN_FALSE);
    if (res != SL_RESULT_SUCCESS) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to realize the player");
        return -1;
    }
    // Get seek and play interfaces
    res = (*_slPlayer)->GetInterface(_slPlayer, SL_IID_PLAY,
                                     (void*) &_slPlayerPlay);
    if (res != SL_RESULT_SUCCESS) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to get Player interface");
        return -1;
    }
    res = (*_slPlayer)->GetInterface(_slPlayer, SL_IID_BUFFERQUEUE,
                                     (void*) &_slPlayerSimpleBufferQueue);
    if (res != SL_RESULT_SUCCESS) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to get Player Simple Buffer Queue interface");
        return -1;
    }

    // Setup to receive buffer queue event callbacks
    res = (*_slPlayerSimpleBufferQueue)->RegisterCallback(
        _slPlayerSimpleBufferQueue,
        PlayerSimpleBufferQueueCallback,
        this);
    if (res != SL_RESULT_SUCCESS) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to register Player Callback");
        return -1;
    }
    _playIsInitialized = true;
    return 0;
}

// ----------------------------------------------------------------------------
//  InitRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidOpenSLES::InitRecording() {

    CriticalSectionScoped lock(&_critSect);

    if (!_initialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "  Not initialized");
        return -1;
    }

    if (_recording) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  Recording already started");
        return -1;
    }

    if (!_recordingDeviceIsSpecified) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Recording device is not specified");
        return -1;
    }

    if (_recIsInitialized) {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Recording already initialized");
        return 0;
    }

    // Initialize the microphone
    if (InitMicrophone() == -1) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  InitMicrophone() failed");
    }

    if (_slEngineObject == NULL || _slEngine == NULL) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Recording object is NULL");
        return -1;
    }

    WebRtc_Word32 res(-1);
    SLDataSource audioSource;
    SLDataLocator_IODevice micLocator;
    SLDataSink audioSink;
    SLDataFormat_PCM pcm;
    SLDataLocator_AndroidSimpleBufferQueue simpleBufferQueue;

    // Setup the data source structure
    micLocator.locatorType = SL_DATALOCATOR_IODEVICE;
    micLocator.deviceType = SL_IODEVICE_AUDIOINPUT;
    micLocator.deviceID = SL_DEFAULTDEVICEID_AUDIOINPUT; //micDeviceID;
    micLocator.device = NULL;
    audioSource.pLocator = (void *) &micLocator;
    audioSource.pFormat = NULL;

    /* Setup the data source structure for the buffer queue */
    simpleBufferQueue.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
    simpleBufferQueue.numBuffers = N_REC_QUEUE_BUFFERS;
    /* Setup the format of the content in the buffer queue */
    pcm.formatType = SL_DATAFORMAT_PCM;
    pcm.numChannels = 1;
    // _samplingRateIn is initialized in initSampleRate()
    pcm.samplesPerSec = SL_SAMPLINGRATE_16;
    pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
    pcm.containerSize = 16;
    pcm.channelMask = SL_SPEAKER_FRONT_CENTER;
    pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;
    audioSink.pFormat = (void *) &pcm;
    audioSink.pLocator = (void *) &simpleBufferQueue;

    const SLInterfaceID id[1] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE };
    const SLboolean req[1] = { SL_BOOLEAN_TRUE };
    res = (*_slEngine)->CreateAudioRecorder(_slEngine, &_slRecorder,
                                            &audioSource, &audioSink, 1, id,
                                            req);
    if (res != SL_RESULT_SUCCESS) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to create Recorder");
        return -1;
    }

    // Realizing the recorder in synchronous mode.
    res = (*_slRecorder)->Realize(_slRecorder, SL_BOOLEAN_FALSE);
    if (res != SL_RESULT_SUCCESS) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to realize Recorder");
        return -1;
    }

    // Get the RECORD interface - it is an implicit interface
    res = (*_slRecorder)->GetInterface(_slRecorder, SL_IID_RECORD,
                                       (void*) &_slRecorderRecord);
    if (res != SL_RESULT_SUCCESS) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to get Recorder interface");
        return -1;
    }

    // Get the simpleBufferQueue interface
    res = (*_slRecorder)->GetInterface(_slRecorder,
                                       SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                       (void*) &_slRecorderSimpleBufferQueue);
    if (res != SL_RESULT_SUCCESS) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to get Recorder Simple Buffer Queue");
        return -1;
    }

    // Setup to receive buffer queue event callbacks
    res = (*_slRecorderSimpleBufferQueue)->RegisterCallback(
        _slRecorderSimpleBufferQueue,
        RecorderSimpleBufferQueueCallback,
        this);
    if (res != SL_RESULT_SUCCESS) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to register Recorder Callback");
        return -1;
    }

    _recIsInitialized = true;
    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::StartRecording() {

    CriticalSectionScoped lock(&_critSect);

    if (!_recIsInitialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Recording not initialized");
        return -1;
    }

    if (_recording) {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Recording already started");
        return 0;
    }

    if (_slRecorderRecord == NULL) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "  RecordITF is NULL");
        return -1;
    }

    if (_slRecorderSimpleBufferQueue == NULL) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Recorder Simple Buffer Queue is NULL");
        return -1;
    }

    // Reset recording buffer
    memset(_recQueueBuffer, 0, sizeof(_recQueueBuffer)); // empty the queue
    _recQueueSeq = 0;

    const char* threadName = "webrtc_opensles_audio_capture_thread";
    _ptrThreadRec = ThreadWrapper::CreateThread(RecThreadFunc, this,
            kRealtimePriority, threadName);
    if (_ptrThreadRec == NULL)
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                "  failed to create the rec audio thread");
        return -1;
    }

    unsigned int threadID(0);
    if (!_ptrThreadRec->Start(threadID))
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                "  failed to start the rec audio thread");
        delete _ptrThreadRec;
        _ptrThreadRec = NULL;
        return -1;
    }
    _recThreadID = threadID;
    _recThreadIsInitialized = true;
    memset(_recBuffer, 0, sizeof(_recBuffer));
    memset(_recLength, 0, sizeof(_recLength));
    memset(_recSeqNumber, 0, sizeof(_recSeqNumber));
    _recCurrentSeq = 0;
    _recBufferTotalSize = 0;
    _recWarning = 0;
    _recError = 0;

    // Enqueue N_REC_QUEUE_BUFFERS -1 zero buffers to get the ball rolling
    // find out how it behaves when the sample rate is 44100
    WebRtc_Word32 res(-1);
    WebRtc_UWord32 nSample10ms = _adbSampleRate / 100;
    for (int i = 0; i < (N_REC_QUEUE_BUFFERS - 1); i++) {
        // We assign 10ms buffer to each queue, size given in bytes.
        res = (*_slRecorderSimpleBufferQueue)->Enqueue(
            _slRecorderSimpleBufferQueue,
            (void*) _recQueueBuffer[_recQueueSeq],
            2 * nSample10ms);
        if (res != SL_RESULT_SUCCESS) {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  failed to Enqueue Empty Buffer to recorder");
            return -1;
        }
        _recQueueSeq++;
    }
    // Record the audio
    res = (*_slRecorderRecord)->SetRecordState(_slRecorderRecord,
                                               SL_RECORDSTATE_RECORDING);
    if (res != SL_RESULT_SUCCESS) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to start recording");
        return -1;
    }
    _recording = true;

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::StopRecording() {

    CriticalSectionScoped lock(&_critSect);

    if (!_recIsInitialized) {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Recording is not initialized");
        return 0;
    }

    // Stop the recording thread
    if (_ptrThreadRec != NULL)
    {
        WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
                "Stopping capture thread");
        bool res = _ptrThreadRec->Stop();
        if (!res) {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                                    "Failed to stop Capture thread ");
        } else {
            delete _ptrThreadRec;
            _ptrThreadRec = NULL;
            _recThreadIsInitialized = false;
        }
    }

    if ((_slRecorderRecord != NULL) && (_slRecorder != NULL)) {
        // Record the audio
        WebRtc_Word32 res = (*_slRecorderRecord)->SetRecordState(
            _slRecorderRecord,
            SL_RECORDSTATE_STOPPED);
        if (res != SL_RESULT_SUCCESS) {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  failed to stop recording");
            return -1;
        }
        res = (*_slRecorderSimpleBufferQueue)->Clear(
              _slRecorderSimpleBufferQueue);
        if (res != SL_RESULT_SUCCESS) {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  failed to clear recorder buffer queue");
            return -1;
        }

        // Destroy the recorder object
        (*_slRecorder)->Destroy(_slRecorder);
        _slRecorder = NULL;
        _slRecorderRecord = NULL;
        _slRecorderRecord = NULL;
    }

    _recIsInitialized = false;
    _recording = false;
    _recWarning = 0;
    _recError = 0;
    _recQueueSeq = 0;
    return 0;
}

bool AudioDeviceAndroidOpenSLES::RecordingIsInitialized() const {

    return _recIsInitialized;
}


bool AudioDeviceAndroidOpenSLES::Recording() const {

    return _recording;
}

bool AudioDeviceAndroidOpenSLES::PlayoutIsInitialized() const {

    return _playIsInitialized;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::StartPlayout() {

    CriticalSectionScoped lock(&_critSect);

    if (!_playIsInitialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Playout not initialized");
        return -1;
    }

    if (_playing) {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Playout already started");
        return 0;
    }

    if (_slPlayerPlay == NULL) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "  PlayItf is NULL");
        return -1;
    }
    if (_slPlayerSimpleBufferQueue == NULL) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  PlayerSimpleBufferQueue is NULL");
        return -1;
    }

    _recQueueSeq = 0;

    WebRtc_Word32 res(-1);
    /* Enqueue a set of zero buffers to get the ball rolling */
    WebRtc_UWord32 nSample10ms = _adbSampleRate / 100;
    WebRtc_Word8 playBuffer[2 * nSample10ms];
    WebRtc_UWord32 noSamplesOut(0);
    {
        noSamplesOut = _ptrAudioBuffer->RequestPlayoutData(nSample10ms);
        //Lock();
        // Get data from Audio Device Buffer
        noSamplesOut = _ptrAudioBuffer->GetPlayoutData(playBuffer);
        // Insert what we have in data buffer
        memcpy(_playQueueBuffer[_playQueueSeq], playBuffer, 2 * noSamplesOut);
        //UnLock();

        //WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
        // "_playQueueSeq (%u)  noSamplesOut (%d)", _playQueueSeq,
        //noSamplesOut);
        // write the buffer data we got from VoE into the device
        res = (*_slPlayerSimpleBufferQueue)->Enqueue(
            _slPlayerSimpleBufferQueue,
            (void*) _playQueueBuffer[_playQueueSeq],
            2 * noSamplesOut);
        if (res != SL_RESULT_SUCCESS) {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  player simpler buffer queue Enqueue failed, %d",
                         noSamplesOut);
            //return ; dong return
        }
        _playQueueSeq = (_playQueueSeq + 1) % N_PLAY_QUEUE_BUFFERS;
    }

    // Play the PCM samples using a buffer queue
    res = (*_slPlayerPlay)->SetPlayState(_slPlayerPlay, SL_PLAYSTATE_PLAYING);
    if (res != SL_RESULT_SUCCESS) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  failed to start playout");
        return -1;
    }

    _playWarning = 0;
    _playError = 0;
    _playing = true;

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::StopPlayout() {

    CriticalSectionScoped lock(&_critSect);

    if (!_playIsInitialized) {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Playout is not initialized");
        return 0;
    }

    if ((_slPlayerPlay != NULL) && (_slOutputMixObject == NULL) && (_slPlayer
            == NULL)) {
        // Make sure player is stopped 
        WebRtc_Word32 res =
                (*_slPlayerPlay)->SetPlayState(_slPlayerPlay,
                                               SL_PLAYSTATE_STOPPED);
        if (res != SL_RESULT_SUCCESS) {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  failed to stop playout");
            return -1;
        }
        res = (*_slPlayerSimpleBufferQueue)->Clear(_slPlayerSimpleBufferQueue);
        if (res != SL_RESULT_SUCCESS) {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  failed to clear recorder buffer queue");
            return -1;
        }

        // Destroy the player
        (*_slPlayer)->Destroy(_slPlayer);
        // Destroy Output Mix object 
        (*_slOutputMixObject)->Destroy(_slOutputMixObject);
        _slPlayer = NULL;
        _slPlayerPlay = NULL;
        _slPlayerSimpleBufferQueue = NULL;
        _slOutputMixObject = NULL;
    }

    _playIsInitialized = false;
    _playing = false;
    _playWarning = 0;
    _playError = 0;
    _playQueueSeq = 0;

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::PlayoutDelay(WebRtc_UWord16& delayMS) const {
    delayMS = _playoutDelay;

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::RecordingDelay(WebRtc_UWord16& delayMS) const {
    delayMS = _recordingDelay;

    return 0;
}

bool AudioDeviceAndroidOpenSLES::Playing() const {

    return _playing;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetPlayoutBuffer(
    const AudioDeviceModule::BufferType /*type*/,
    WebRtc_UWord16 /*sizeMS*/) {

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::PlayoutBuffer(
    AudioDeviceModule::BufferType& type,
    WebRtc_UWord16& sizeMS) const {

    type = AudioDeviceModule::kAdaptiveBufferSize;
    sizeMS = _playoutDelay; // Set to current playout delay

    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::CPULoad(WebRtc_UWord16& /*load*/) const {

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

bool AudioDeviceAndroidOpenSLES::PlayoutWarning() const {
    return (_playWarning > 0);
}

bool AudioDeviceAndroidOpenSLES::PlayoutError() const {
    return (_playError > 0);
}

bool AudioDeviceAndroidOpenSLES::RecordingWarning() const {
    return (_recWarning > 0);
}

bool AudioDeviceAndroidOpenSLES::RecordingError() const {
    return (_recError > 0);
}

void AudioDeviceAndroidOpenSLES::ClearPlayoutWarning() {
    _playWarning = 0;
}

void AudioDeviceAndroidOpenSLES::ClearPlayoutError() {
    _playError = 0;
}

void AudioDeviceAndroidOpenSLES::ClearRecordingWarning() {
    _recWarning = 0;
}

void AudioDeviceAndroidOpenSLES::ClearRecordingError() {
    _recError = 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::SetLoudspeakerStatus(bool enable) {
    _loudSpeakerOn = enable;
    return 0;
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::GetLoudspeakerStatus(
    bool& enabled) const {

    enabled = _loudSpeakerOn;
    return 0;
}

// ============================================================================
//                                 Private Methods
// ============================================================================

void AudioDeviceAndroidOpenSLES::PlayerSimpleBufferQueueCallback(
    SLAndroidSimpleBufferQueueItf queueItf,
    void *pContext) {
    AudioDeviceAndroidOpenSLES* ptrThis =
            static_cast<AudioDeviceAndroidOpenSLES*> (pContext);
    ptrThis->PlayerSimpleBufferQueueCallbackHandler(queueItf);
}

void AudioDeviceAndroidOpenSLES::PlayerSimpleBufferQueueCallbackHandler(
    SLAndroidSimpleBufferQueueItf queueItf) {
    WebRtc_Word32 res;
    //Lock();
    //WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
    //"_playQueueSeq (%u)", _playQueueSeq);
    if (_playing && (_playQueueSeq < N_PLAY_QUEUE_BUFFERS)) {
        //WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice,
        //_id, "playout callback ");
        unsigned int noSamp10ms = _adbSampleRate / 100;
        // Max 10 ms @ samplerate kHz / 16 bit
        WebRtc_Word8 playBuffer[2 * noSamp10ms];
        int noSamplesOut = 0;

        // Assumption for implementation
        // assert(PLAYBUFSIZESAMPLES == noSamp10ms);

        // TODO(xians), update the playout delay
        //UnLock();

        noSamplesOut = _ptrAudioBuffer->RequestPlayoutData(noSamp10ms);
        //Lock();
        // Get data from Audio Device Buffer
        noSamplesOut = _ptrAudioBuffer->GetPlayoutData(playBuffer);
        // Cast OK since only equality comparison
        if (noSamp10ms != (unsigned int) noSamplesOut) {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "noSamp10ms (%u) != noSamplesOut (%d)", noSamp10ms,
                         noSamplesOut);

            if (_playWarning > 0) {
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                             "  Pending play warning exists");
            }
            _playWarning = 1;
        }
        // Insert what we have in data buffer
        memcpy(_playQueueBuffer[_playQueueSeq], playBuffer, 2 * noSamplesOut);
        //UnLock();

        //WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
        //"_playQueueSeq (%u)  noSamplesOut (%d)", _playQueueSeq, noSamplesOut);
        // write the buffer data we got from VoE into the device
        res = (*_slPlayerSimpleBufferQueue)->Enqueue(
            _slPlayerSimpleBufferQueue,
            _playQueueBuffer[_playQueueSeq],
            2 * noSamplesOut);
        if (res != SL_RESULT_SUCCESS) {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  player simpler buffer queue Enqueue failed, %d",
                         noSamplesOut);
            return;
        }
        // update the playout delay
        UpdatePlayoutDelay(noSamplesOut);
        // update the play buffer sequency
        _playQueueSeq = (_playQueueSeq + 1) % N_PLAY_QUEUE_BUFFERS;
    }
}

void AudioDeviceAndroidOpenSLES::RecorderSimpleBufferQueueCallback(
    SLAndroidSimpleBufferQueueItf queueItf,
    void *pContext) {
    AudioDeviceAndroidOpenSLES* ptrThis =
            static_cast<AudioDeviceAndroidOpenSLES*> (pContext);
    ptrThis->RecorderSimpleBufferQueueCallbackHandler(queueItf);
}

void AudioDeviceAndroidOpenSLES::RecorderSimpleBufferQueueCallbackHandler(
    SLAndroidSimpleBufferQueueItf queueItf) {
    WebRtc_Word32 res;
    //WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
    //"  RecorderSimpleBufferQueueCallbackHandler");
    if (_recording) {
        // Insert all data in temp buffer into recording buffers
        // There is zero or one buffer partially full at any given time,
        // all others are full or empty
        // Full means filled with noSamp10ms samples.

        const unsigned int noSamp10ms = _adbSampleRate / 100;
        //        WebRtc_UWord16 queuePos = 0;
        //        WebRtc_UWord16 checkQueuePos = 0;
        unsigned int dataPos = 0;
        WebRtc_UWord16 bufPos = 0;
        WebRtc_Word16 insertPos = -1;
        unsigned int nCopy = 0; // Number of samples to copy
        //        WebRtc_Word32 isData = 0;

        while (dataPos < noSamp10ms)//REC_BUF_SIZE_IN_SAMPLES) //noSamp10ms)

        {
            // Loop over all recording buffers or until we find the partially
            // full buffer
            // First choice is to insert into partially full buffer,
            // second choice is to insert into empty buffer
            bufPos = 0;
            insertPos = -1;
            nCopy = 0;
            while (bufPos < N_REC_BUFFERS)
            {
                if ((_recLength[bufPos] > 0) && (_recLength[bufPos]
                                < noSamp10ms))
                {
                    // Found the partially full buffer
                    insertPos = static_cast<WebRtc_Word16> (bufPos);
                    bufPos = N_REC_BUFFERS; // Don't need to search more
                }
                else if ((-1 == insertPos) && (0 == _recLength[bufPos]))
                {
                    // Found an empty buffer
                    insertPos = static_cast<WebRtc_Word16> (bufPos);
                }
                ++bufPos;
            }

            if (insertPos > -1)
            {
                // We found a non-full buffer, copy data from the buffer queue
                // o recBuffer
                unsigned int dataToCopy = noSamp10ms - dataPos;
                unsigned int currentRecLen = _recLength[insertPos];
                unsigned int roomInBuffer = noSamp10ms - currentRecLen;
                nCopy = (dataToCopy < roomInBuffer ? dataToCopy : roomInBuffer);
                memcpy(&_recBuffer[insertPos][currentRecLen],
                        &_recQueueBuffer[_recQueueSeq][dataPos],
                        nCopy * sizeof(short));
                if (0 == currentRecLen)
                {
                    _recSeqNumber[insertPos] = _recCurrentSeq;
                    ++_recCurrentSeq;
                }
                _recBufferTotalSize += nCopy;
                // Has to be done last to avoid interrupt problems
                // between threads
                _recLength[insertPos] += nCopy;
                dataPos += nCopy;
            }
            else
            {
                // Didn't find a non-full buffer
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice,
                        _id, "  Could not insert into recording buffer");
                if (_recWarning > 0)
                {
                    WEBRTC_TRACE(kTraceWarning,
                            kTraceAudioDevice, _id,
                            "  Pending rec warning exists");
                }
                _recWarning = 1;
                dataPos = noSamp10ms; // Don't try to insert more
            }
        }

        // clean the queue buffer
        // Start with empty buffer
        memset(_recQueueBuffer[_recQueueSeq], 0, 2 * REC_BUF_SIZE_IN_SAMPLES);
        // write the empty buffer to the queue
        res = (*_slRecorderSimpleBufferQueue)->Enqueue(
              _slRecorderSimpleBufferQueue,
              (void*) _recQueueBuffer[_recQueueSeq],
              2 * noSamp10ms);
        if (res != SL_RESULT_SUCCESS) {
            return;
        }
        // update the rec queue seq
        _recQueueSeq = (_recQueueSeq + 1) % N_REC_QUEUE_BUFFERS;
        // wake up the recording thread
        _timeEventRec.Set();
    }
}

void AudioDeviceAndroidOpenSLES::CheckErr(SLresult res) {
    if (res != SL_RESULT_SUCCESS) {
        // Debug printing to be placed here
        exit(-1);
    }
}

void AudioDeviceAndroidOpenSLES::UpdatePlayoutDelay(
    WebRtc_UWord32 nSamplePlayed) {
    // currently just do some simple calculation, should we setup a timer for
    // the callback to have a more accurate delay
    // Android CCD asks for 10ms as the maximum warm output latency, so we
    // simply add (nPlayQueueBuffer -1 + 0.5)*10ms
    // This playout delay should be seldom changed
    _playoutDelay = (N_PLAY_QUEUE_BUFFERS - 0.5) * 10 + N_PLAY_QUEUE_BUFFERS
            * nSamplePlayed / (_adbSampleRate / 1000);
}

void AudioDeviceAndroidOpenSLES::UpdateRecordingDelay() {
    // // Android CCD asks for 10ms as the maximum warm input latency,
    // so we simply add 10ms
    _recordingDelay = 10;
    const WebRtc_UWord32 noSamp10ms = _adbSampleRate / 100;
    //    if (_recBufferTotalSize > noSamp10ms)
    //    {
    _recordingDelay += (N_REC_QUEUE_BUFFERS * noSamp10ms) / (_adbSampleRate
            / 1000);
    //    }
}

WebRtc_Word32 AudioDeviceAndroidOpenSLES::InitSampleRate() {
    if (_slEngineObject == NULL) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "  SL Object is NULL");
      return -1;
    }

    _samplingRateIn = SL_SAMPLINGRATE_16;
    _samplingRateOut = SL_SAMPLINGRATE_16;
    _adbSampleRate = 16000;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id,
                 "  sample rate set to (%d)", _adbSampleRate);
    return 0;

}

// ============================================================================
//                                  Thread Methods
// ============================================================================

bool AudioDeviceAndroidOpenSLES::RecThreadFunc(void* pThis) {
  return (static_cast<AudioDeviceAndroidOpenSLES*>(pThis)->RecThreadProcess());
}

bool AudioDeviceAndroidOpenSLES::RecThreadProcess() {

    //    Lock();
    // Wait for 100ms for the signal from device callback
    // In case no callback comes in 100ms, we check the buffer anyway
    _timeEventRec.Wait(100);

    int bufPos = 0;
    unsigned int lowestSeq = 0;
    int lowestSeqBufPos = 0;
    bool foundBuf = true;
    const unsigned int noSamp10ms = _adbSampleRate / 100;

    while (foundBuf)
    {
        // Check if we have any buffer with data to insert into the
        // Audio Device Buffer,
        // and find the one with the lowest seq number
        foundBuf = false;

        for (bufPos = 0; bufPos < N_REC_BUFFERS; ++bufPos)
        {
            if (noSamp10ms == _recLength[bufPos])
            {
                if (!foundBuf) {
                    lowestSeq = _recSeqNumber[bufPos];
                    lowestSeqBufPos = bufPos;
                    foundBuf = true;
                } else if (_recSeqNumber[bufPos] < lowestSeq)
                {
                    lowestSeq = _recSeqNumber[bufPos];
                    lowestSeqBufPos = bufPos;
                }
            }
        } // for

        // Insert data into the Audio Device Buffer if found any
        if (foundBuf)
        {
            UpdateRecordingDelay();
            // Set the recorded buffer
            _ptrAudioBuffer->SetRecordedBuffer(_recBuffer[lowestSeqBufPos],
                                               noSamp10ms);

            // Don't need to set the current mic level in ADB since we only
            // support digital AGC,
            // and besides we cannot get or set the iPhone mic level anyway.

            // Set VQE info, use clockdrift == 0
            _ptrAudioBuffer->SetVQEData(_playoutDelay, _recordingDelay, 0);

            // Deliver recorded samples at specified sample rate, mic level
            // etc. to the observer using callback
            //UnLock();
            _ptrAudioBuffer->DeliverRecordedData();
            //Lock();

            // Make buffer available
            _recSeqNumber[lowestSeqBufPos] = 0;
            _recBufferTotalSize -= _recLength[lowestSeqBufPos];
            // Must be done last to avoid interrupt problems between threads
            _recLength[lowestSeqBufPos] = 0;
        }

    } // while (foundBuf)
    //UnLock();
    return true;
}

} // namespace webrtc
