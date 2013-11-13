/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/voe_base_impl.h"

#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "webrtc/modules/audio_device/audio_device_impl.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/file_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/voice_engine/channel.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/output_mixer.h"
#include "webrtc/voice_engine/transmit_mixer.h"
#include "webrtc/voice_engine/utility.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

#if (defined(_WIN32) && defined(_DLL) && (_MSC_VER == 1400))
// Fix for VS 2005 MD/MDd link problem
#include <stdio.h>
extern "C"
    { FILE _iob[3] = {   __iob_func()[0], __iob_func()[1], __iob_func()[2]}; }
#endif

namespace webrtc
{

VoEBase* VoEBase::GetInterface(VoiceEngine* voiceEngine)
{
    if (NULL == voiceEngine)
    {
        return NULL;
    }
    VoiceEngineImpl* s = static_cast<VoiceEngineImpl*>(voiceEngine);
    s->AddRef();
    return s;
}

VoEBaseImpl::VoEBaseImpl(voe::SharedData* shared) :
    _voiceEngineObserverPtr(NULL),
    _callbackCritSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _voiceEngineObserver(false), _oldVoEMicLevel(0), _oldMicLevel(0),
    _shared(shared)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoEBaseImpl() - ctor");
}

VoEBaseImpl::~VoEBaseImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "~VoEBaseImpl() - dtor");

    TerminateInternal();

    delete &_callbackCritSect;
}

void VoEBaseImpl::OnErrorIsReported(ErrorCode error)
{
    CriticalSectionScoped cs(&_callbackCritSect);
    if (_voiceEngineObserver)
    {
        if (_voiceEngineObserverPtr)
        {
            int errCode(0);
            if (error == AudioDeviceObserver::kRecordingError)
            {
                errCode = VE_RUNTIME_REC_ERROR;
                WEBRTC_TRACE(kTraceInfo, kTraceVoice,
                    VoEId(_shared->instance_id(), -1),
                    "VoEBaseImpl::OnErrorIsReported() => VE_RUNTIME_REC_ERROR");
            }
            else if (error == AudioDeviceObserver::kPlayoutError)
            {
                errCode = VE_RUNTIME_PLAY_ERROR;
                WEBRTC_TRACE(kTraceInfo, kTraceVoice,
                    VoEId(_shared->instance_id(), -1),
                    "VoEBaseImpl::OnErrorIsReported() => "
                    "VE_RUNTIME_PLAY_ERROR");
            }
            // Deliver callback (-1 <=> no channel dependency)
            _voiceEngineObserverPtr->CallbackOnError(-1, errCode);
        }
    }
}

void VoEBaseImpl::OnWarningIsReported(WarningCode warning)
{
    CriticalSectionScoped cs(&_callbackCritSect);
    if (_voiceEngineObserver)
    {
        if (_voiceEngineObserverPtr)
        {
            int warningCode(0);
            if (warning == AudioDeviceObserver::kRecordingWarning)
            {
                warningCode = VE_RUNTIME_REC_WARNING;
                WEBRTC_TRACE(kTraceInfo, kTraceVoice,
                    VoEId(_shared->instance_id(), -1),
                    "VoEBaseImpl::OnErrorIsReported() => "
                    "VE_RUNTIME_REC_WARNING");
            }
            else if (warning == AudioDeviceObserver::kPlayoutWarning)
            {
                warningCode = VE_RUNTIME_PLAY_WARNING;
                WEBRTC_TRACE(kTraceInfo, kTraceVoice,
                    VoEId(_shared->instance_id(), -1),
                    "VoEBaseImpl::OnErrorIsReported() => "
                    "VE_RUNTIME_PLAY_WARNING");
            }
            // Deliver callback (-1 <=> no channel dependency)
            _voiceEngineObserverPtr->CallbackOnError(-1, warningCode);
        }
    }
}

int32_t VoEBaseImpl::RecordedDataIsAvailable(
        const void* audioSamples,
        uint32_t nSamples,
        uint8_t nBytesPerSample,
        uint8_t nChannels,
        uint32_t samplesPerSec,
        uint32_t totalDelayMS,
        int32_t clockDrift,
        uint32_t currentMicLevel,
        bool keyPressed,
        uint32_t& newMicLevel)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoEBaseImpl::RecordedDataIsAvailable(nSamples=%u, "
                     "nBytesPerSample=%u, nChannels=%u, samplesPerSec=%u, "
                     "totalDelayMS=%u, clockDrift=%d, currentMicLevel=%u)",
                 nSamples, nBytesPerSample, nChannels, samplesPerSec,
                 totalDelayMS, clockDrift, currentMicLevel);
    newMicLevel = static_cast<uint32_t>(ProcessRecordedDataWithAPM(
        NULL, 0, audioSamples, samplesPerSec, nChannels, nSamples,
        totalDelayMS, clockDrift, currentMicLevel, keyPressed));

    return 0;
}

int32_t VoEBaseImpl::NeedMorePlayData(
        uint32_t nSamples,
        uint8_t nBytesPerSample,
        uint8_t nChannels,
        uint32_t samplesPerSec,
        void* audioSamples,
        uint32_t& nSamplesOut)
{
    WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoEBaseImpl::NeedMorePlayData(nSamples=%u, "
                     "nBytesPerSample=%d, nChannels=%d, samplesPerSec=%u)",
                 nSamples, nBytesPerSample, nChannels, samplesPerSec);

    assert(_shared->output_mixer() != NULL);

    // TODO(andrew): if the device is running in mono, we should tell the mixer
    // here so that it will only request mono from AudioCodingModule.
    // Perform mixing of all active participants (channel-based mixing)
    _shared->output_mixer()->MixActiveChannels();

    // Additional operations on the combined signal
    _shared->output_mixer()->DoOperationsOnCombinedSignal();

    // Retrieve the final output mix (resampled to match the ADM)
    _shared->output_mixer()->GetMixedAudio(samplesPerSec, nChannels,
        &_audioFrame);

    assert(static_cast<int>(nSamples) == _audioFrame.samples_per_channel_);
    assert(samplesPerSec ==
        static_cast<uint32_t>(_audioFrame.sample_rate_hz_));

    // Deliver audio (PCM) samples to the ADM
    memcpy(
           (int16_t*) audioSamples,
           (const int16_t*) _audioFrame.data_,
           sizeof(int16_t) * (_audioFrame.samples_per_channel_
                   * _audioFrame.num_channels_));

    nSamplesOut = _audioFrame.samples_per_channel_;

    return 0;
}

int VoEBaseImpl::OnDataAvailable(const int voe_channels[],
                                 int number_of_voe_channels,
                                 const int16_t* audio_data,
                                 int sample_rate,
                                 int number_of_channels,
                                 int number_of_frames,
                                 int audio_delay_milliseconds,
                                 int current_volume,
                                 bool key_pressed,
                                 bool need_audio_processing) {
  WEBRTC_TRACE(kTraceStream, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "VoEBaseImpl::OnDataAvailable(number_of_voe_channels=%d, "
               "sample_rate=%d, number_of_channels=%d, number_of_frames=%d, "
               "audio_delay_milliseconds=%d, current_volume=%d, "
               "key_pressed=%d, need_audio_processing=%d)",
               number_of_voe_channels, sample_rate, number_of_channels,
               number_of_frames, audio_delay_milliseconds, current_volume,
               key_pressed, need_audio_processing);
  if (number_of_voe_channels == 0)
    return 0;

  if (need_audio_processing) {
    return ProcessRecordedDataWithAPM(
        voe_channels, number_of_voe_channels, audio_data, sample_rate,
        number_of_channels, number_of_frames, audio_delay_milliseconds,
        0, current_volume, key_pressed);
  }

  // No need to go through the APM, demultiplex the data to each VoE channel,
  // encode and send to the network.
  for (int i = 0; i < number_of_voe_channels; ++i) {
    voe::ChannelOwner ch =
        _shared->channel_manager().GetChannel(voe_channels[i]);
    voe::Channel* channel_ptr = ch.channel();
    if (!channel_ptr)
      continue;

    if (channel_ptr->InputIsOnHold()) {
      channel_ptr->UpdateLocalTimeStamp();
    } else if (channel_ptr->Sending()) {
      channel_ptr->Demultiplex(audio_data, sample_rate, number_of_frames,
                               number_of_channels);
      channel_ptr->PrepareEncodeAndSend(sample_rate);
      channel_ptr->EncodeAndSend();
    }
  }

  // Return 0 to indicate no need to change the volume.
  return 0;
}

int VoEBaseImpl::RegisterVoiceEngineObserver(VoiceEngineObserver& observer)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "RegisterVoiceEngineObserver(observer=0x%d)", &observer);
    CriticalSectionScoped cs(&_callbackCritSect);
    if (_voiceEngineObserverPtr)
    {
        _shared->SetLastError(VE_INVALID_OPERATION, kTraceError,
            "RegisterVoiceEngineObserver() observer already enabled");
        return -1;
    }

    // Register the observer in all active channels
    for (voe::ChannelManager::Iterator it(&_shared->channel_manager());
         it.IsValid();
         it.Increment()) {
      it.GetChannel()->RegisterVoiceEngineObserver(observer);
    }

    _shared->transmit_mixer()->RegisterVoiceEngineObserver(observer);

    _voiceEngineObserverPtr = &observer;
    _voiceEngineObserver = true;

    return 0;
}

int VoEBaseImpl::DeRegisterVoiceEngineObserver()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "DeRegisterVoiceEngineObserver()");
    CriticalSectionScoped cs(&_callbackCritSect);
    if (!_voiceEngineObserverPtr)
    {
        _shared->SetLastError(VE_INVALID_OPERATION, kTraceError,
            "DeRegisterVoiceEngineObserver() observer already disabled");
        return 0;
    }

    _voiceEngineObserver = false;
    _voiceEngineObserverPtr = NULL;

    // Deregister the observer in all active channels
    for (voe::ChannelManager::Iterator it(&_shared->channel_manager());
         it.IsValid();
         it.Increment()) {
      it.GetChannel()->DeRegisterVoiceEngineObserver();
    }

    return 0;
}

int VoEBaseImpl::Init(AudioDeviceModule* external_adm,
                      AudioProcessing* audioproc)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
        "Init(external_adm=0x%p)", external_adm);
    CriticalSectionScoped cs(_shared->crit_sec());

    WebRtcSpl_Init();

    if (_shared->statistics().Initialized())
    {
        return 0;
    }

    if (_shared->process_thread())
    {
        if (_shared->process_thread()->Start() != 0)
        {
            _shared->SetLastError(VE_THREAD_ERROR, kTraceError,
                "Init() failed to start module process thread");
            return -1;
        }
    }

    // Create an internal ADM if the user has not added an external
    // ADM implementation as input to Init().
    if (external_adm == NULL)
    {
        // Create the internal ADM implementation.
        _shared->set_audio_device(AudioDeviceModuleImpl::Create(
            VoEId(_shared->instance_id(), -1), _shared->audio_device_layer()));

        if (_shared->audio_device() == NULL)
        {
            _shared->SetLastError(VE_NO_MEMORY, kTraceCritical,
                "Init() failed to create the ADM");
            return -1;
        }
    }
    else
    {
        // Use the already existing external ADM implementation.
        _shared->set_audio_device(external_adm);
        WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_shared->instance_id(), -1),
            "An external ADM implementation will be used in VoiceEngine");
    }

    // Register the ADM to the process thread, which will drive the error
    // callback mechanism
    if (_shared->process_thread() &&
        _shared->process_thread()->RegisterModule(_shared->audio_device()) != 0)
    {
        _shared->SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR, kTraceError,
            "Init() failed to register the ADM");
        return -1;
    }

    bool available(false);

    // --------------------
    // Reinitialize the ADM

    // Register the AudioObserver implementation
    if (_shared->audio_device()->RegisterEventObserver(this) != 0) {
      _shared->SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR, kTraceWarning,
          "Init() failed to register event observer for the ADM");
    }

    // Register the AudioTransport implementation
    if (_shared->audio_device()->RegisterAudioCallback(this) != 0) {
      _shared->SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR, kTraceWarning,
          "Init() failed to register audio callback for the ADM");
    }

    // ADM initialization
    if (_shared->audio_device()->Init() != 0)
    {
        _shared->SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR, kTraceError,
            "Init() failed to initialize the ADM");
        return -1;
    }

    // Initialize the default speaker
    if (_shared->audio_device()->SetPlayoutDevice(
            WEBRTC_VOICE_ENGINE_DEFAULT_DEVICE) != 0)
    {
        _shared->SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR, kTraceInfo,
            "Init() failed to set the default output device");
    }
    if (_shared->audio_device()->SpeakerIsAvailable(&available) != 0)
    {
        _shared->SetLastError(VE_CANNOT_ACCESS_SPEAKER_VOL, kTraceInfo,
            "Init() failed to check speaker availability, trying to "
            "initialize speaker anyway");
    }
    else if (!available)
    {
        _shared->SetLastError(VE_CANNOT_ACCESS_SPEAKER_VOL, kTraceInfo,
            "Init() speaker not available, trying to initialize speaker "
            "anyway");
    }
    if (_shared->audio_device()->InitSpeaker() != 0)
    {
        _shared->SetLastError(VE_CANNOT_ACCESS_SPEAKER_VOL, kTraceInfo,
            "Init() failed to initialize the speaker");
    }

    // Initialize the default microphone
    if (_shared->audio_device()->SetRecordingDevice(
            WEBRTC_VOICE_ENGINE_DEFAULT_DEVICE) != 0)
    {
        _shared->SetLastError(VE_SOUNDCARD_ERROR, kTraceInfo,
            "Init() failed to set the default input device");
    }
    if (_shared->audio_device()->MicrophoneIsAvailable(&available) != 0)
    {
        _shared->SetLastError(VE_CANNOT_ACCESS_MIC_VOL, kTraceInfo,
            "Init() failed to check microphone availability, trying to "
            "initialize microphone anyway");
    }
    else if (!available)
    {
        _shared->SetLastError(VE_CANNOT_ACCESS_MIC_VOL, kTraceInfo,
            "Init() microphone not available, trying to initialize "
            "microphone anyway");
    }
    if (_shared->audio_device()->InitMicrophone() != 0)
    {
        _shared->SetLastError(VE_CANNOT_ACCESS_MIC_VOL, kTraceInfo,
            "Init() failed to initialize the microphone");
    }

    // Set number of channels
    if (_shared->audio_device()->StereoPlayoutIsAvailable(&available) != 0) {
      _shared->SetLastError(VE_SOUNDCARD_ERROR, kTraceWarning,
          "Init() failed to query stereo playout mode");
    }
    if (_shared->audio_device()->SetStereoPlayout(available) != 0)
    {
        _shared->SetLastError(VE_SOUNDCARD_ERROR, kTraceWarning,
            "Init() failed to set mono/stereo playout mode");
    }

    // TODO(andrew): These functions don't tell us whether stereo recording
    // is truly available. We simply set the AudioProcessing input to stereo
    // here, because we have to wait until receiving the first frame to
    // determine the actual number of channels anyway.
    //
    // These functions may be changed; tracked here:
    // http://code.google.com/p/webrtc/issues/detail?id=204
    _shared->audio_device()->StereoRecordingIsAvailable(&available);
    if (_shared->audio_device()->SetStereoRecording(available) != 0)
    {
        _shared->SetLastError(VE_SOUNDCARD_ERROR, kTraceWarning,
            "Init() failed to set mono/stereo recording mode");
    }

    if (!audioproc) {
      audioproc = AudioProcessing::Create(VoEId(_shared->instance_id(), -1));
      if (!audioproc) {
        LOG(LS_ERROR) << "Failed to create AudioProcessing.";
        _shared->SetLastError(VE_NO_MEMORY);
        return -1;
      }
    }
    _shared->set_audio_processing(audioproc);

    // Set the error state for any failures in this block.
    _shared->SetLastError(VE_APM_ERROR);
    if (audioproc->echo_cancellation()->set_device_sample_rate_hz(48000)) {
      LOG_FERR1(LS_ERROR, set_device_sample_rate_hz, 48000);
      return -1;
    }
    // Assume 16 kHz mono until the audio frames are received from the capture
    // device, at which point this can be updated.
    if (audioproc->set_sample_rate_hz(16000)) {
      LOG_FERR1(LS_ERROR, set_sample_rate_hz, 16000);
      return -1;
    }
    if (audioproc->set_num_channels(1, 1) != 0) {
      LOG_FERR2(LS_ERROR, set_num_channels, 1, 1);
      return -1;
    }
    if (audioproc->set_num_reverse_channels(1) != 0) {
      LOG_FERR1(LS_ERROR, set_num_reverse_channels, 1);
      return -1;
    }

    // Configure AudioProcessing components. All are disabled by default.
    if (audioproc->high_pass_filter()->Enable(true) != 0) {
      LOG_FERR1(LS_ERROR, high_pass_filter()->Enable, true);
      return -1;
    }
    if (audioproc->echo_cancellation()->enable_drift_compensation(false) != 0) {
      LOG_FERR1(LS_ERROR, enable_drift_compensation, false);
      return -1;
    }
    if (audioproc->noise_suppression()->set_level(kDefaultNsMode) != 0) {
      LOG_FERR1(LS_ERROR, noise_suppression()->set_level, kDefaultNsMode);
      return -1;
    }
    GainControl* agc = audioproc->gain_control();
    if (agc->set_analog_level_limits(kMinVolumeLevel, kMaxVolumeLevel) != 0) {
      LOG_FERR2(LS_ERROR, agc->set_analog_level_limits, kMinVolumeLevel,
                kMaxVolumeLevel);
      return -1;
    }
    if (agc->set_mode(kDefaultAgcMode) != 0) {
      LOG_FERR1(LS_ERROR, agc->set_mode, kDefaultAgcMode);
      return -1;
    }
    if (agc->Enable(kDefaultAgcState) != 0) {
      LOG_FERR1(LS_ERROR, agc->Enable, kDefaultAgcState);
      return -1;
    }
    _shared->SetLastError(0);  // Clear error state.

#ifdef WEBRTC_VOICE_ENGINE_AGC
    bool agc_enabled = agc->mode() == GainControl::kAdaptiveAnalog &&
                       agc->is_enabled();
    if (_shared->audio_device()->SetAGC(agc_enabled) != 0) {
      LOG_FERR1(LS_ERROR, audio_device()->SetAGC, agc_enabled);
      _shared->SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR);
      // TODO(ajm): No error return here due to
      // https://code.google.com/p/webrtc/issues/detail?id=1464
    }
#endif

    return _shared->statistics().SetInitialized();
}

int VoEBaseImpl::Terminate()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "Terminate()");
    CriticalSectionScoped cs(_shared->crit_sec());
    return TerminateInternal();
}

int VoEBaseImpl::CreateChannel()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "CreateChannel()");
    CriticalSectionScoped cs(_shared->crit_sec());

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    voe::ChannelOwner channel_owner =
        _shared->channel_manager().CreateChannel();

    if (channel_owner.channel()->SetEngineInformation(
            _shared->statistics(),
            *_shared->output_mixer(),
            *_shared->transmit_mixer(),
            *_shared->process_thread(),
            *_shared->audio_device(),
            _voiceEngineObserverPtr,
            &_callbackCritSect) != 0) {
      _shared->SetLastError(
          VE_CHANNEL_NOT_CREATED,
          kTraceError,
          "CreateChannel() failed to associate engine and channel."
          " Destroying channel.");
      _shared->channel_manager()
          .DestroyChannel(channel_owner.channel()->ChannelId());
      return -1;
    } else if (channel_owner.channel()->Init() != 0) {
      _shared->SetLastError(
          VE_CHANNEL_NOT_CREATED,
          kTraceError,
          "CreateChannel() failed to initialize channel. Destroying"
          " channel.");
      _shared->channel_manager()
          .DestroyChannel(channel_owner.channel()->ChannelId());
      return -1;
    }

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
        VoEId(_shared->instance_id(), -1),
        "CreateChannel() => %d", channel_owner.channel()->ChannelId());
    return channel_owner.channel()->ChannelId();
}

int VoEBaseImpl::DeleteChannel(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "DeleteChannel(channel=%d)", channel);
    CriticalSectionScoped cs(_shared->crit_sec());

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    {
        voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
        voe::Channel* channelPtr = ch.channel();
        if (channelPtr == NULL)
        {
            _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                "DeleteChannel() failed to locate channel");
            return -1;
        }
    }

    _shared->channel_manager().DestroyChannel(channel);

    if (StopSend() != 0)
    {
        return -1;
    }

    if (StopPlayout() != 0)
    {
        return -1;
    }

    return 0;
}

int VoEBaseImpl::StartReceive(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StartReceive(channel=%d)", channel);
    CriticalSectionScoped cs(_shared->crit_sec());
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "StartReceive() failed to locate channel");
        return -1;
    }
    return channelPtr->StartReceiving();
}

int VoEBaseImpl::StopReceive(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StopListen(channel=%d)", channel);
    CriticalSectionScoped cs(_shared->crit_sec());
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetLocalReceiver() failed to locate channel");
        return -1;
    }
    return channelPtr->StopReceiving();
}

int VoEBaseImpl::StartPlayout(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StartPlayout(channel=%d)", channel);
    CriticalSectionScoped cs(_shared->crit_sec());
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "StartPlayout() failed to locate channel");
        return -1;
    }
    if (channelPtr->Playing())
    {
        return 0;
    }
    if (StartPlayout() != 0)
    {
        _shared->SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR, kTraceError,
            "StartPlayout() failed to start playout");
        return -1;
    }
    return channelPtr->StartPlayout();
}

int VoEBaseImpl::StopPlayout(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StopPlayout(channel=%d)", channel);
    CriticalSectionScoped cs(_shared->crit_sec());
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "StopPlayout() failed to locate channel");
        return -1;
    }
    if (channelPtr->StopPlayout() != 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
            VoEId(_shared->instance_id(), -1),
            "StopPlayout() failed to stop playout for channel %d", channel);
    }
    return StopPlayout();
}

int VoEBaseImpl::StartSend(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StartSend(channel=%d)", channel);
    CriticalSectionScoped cs(_shared->crit_sec());
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "StartSend() failed to locate channel");
        return -1;
    }
    if (channelPtr->Sending())
    {
        return 0;
    }
    if (StartSend() != 0)
    {
        _shared->SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR, kTraceError,
            "StartSend() failed to start recording");
        return -1;
    }
    return channelPtr->StartSend();
}

int VoEBaseImpl::StopSend(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StopSend(channel=%d)", channel);
    CriticalSectionScoped cs(_shared->crit_sec());
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "StopSend() failed to locate channel");
        return -1;
    }
    if (channelPtr->StopSend() != 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice,
            VoEId(_shared->instance_id(), -1),
            "StopSend() failed to stop sending for channel %d", channel);
    }
    return StopSend();
}

int VoEBaseImpl::GetVersion(char version[1024])
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetVersion(version=?)");
    assert(kVoiceEngineVersionMaxMessageSize == 1024);

    if (version == NULL)
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError);
        return (-1);
    }

    char versionBuf[kVoiceEngineVersionMaxMessageSize];
    char* versionPtr = versionBuf;

    int32_t len = 0;
    int32_t accLen = 0;

    len = AddVoEVersion(versionPtr);
    if (len == -1)
    {
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kVoiceEngineVersionMaxMessageSize);

    len = AddBuildInfo(versionPtr);
    if (len == -1)
    {
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kVoiceEngineVersionMaxMessageSize);

#ifdef WEBRTC_EXTERNAL_TRANSPORT
    len = AddExternalTransportBuild(versionPtr);
    if (len == -1)
    {
         return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kVoiceEngineVersionMaxMessageSize);
#endif
#ifdef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
    len = AddExternalRecAndPlayoutBuild(versionPtr);
    if (len == -1)
    {
        return -1;
    }
    versionPtr += len;
    accLen += len;
    assert(accLen < kVoiceEngineVersionMaxMessageSize);
 #endif

    memcpy(version, versionBuf, accLen);
    version[accLen] = '\0';

    // to avoid the truncation in the trace, split the string into parts
    char partOfVersion[256];
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
        VoEId(_shared->instance_id(), -1), "GetVersion() =>");
    for (int partStart = 0; partStart < accLen;)
    {
        memset(partOfVersion, 0, sizeof(partOfVersion));
        int partEnd = partStart + 180;
        while (version[partEnd] != '\n' && version[partEnd] != '\0')
        {
            partEnd--;
        }
        if (partEnd < accLen)
        {
            memcpy(partOfVersion, &version[partStart], partEnd - partStart);
        }
        else
        {
            memcpy(partOfVersion, &version[partStart], accLen - partStart);
        }
        partStart = partEnd;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
            VoEId(_shared->instance_id(), -1), "%s", partOfVersion);
    }

    return 0;
}

int32_t VoEBaseImpl::AddBuildInfo(char* str) const
{
    return sprintf(str, "Build: svn:%s %s\n", WEBRTC_SVNREVISION, BUILDINFO);
}

int32_t VoEBaseImpl::AddVoEVersion(char* str) const
{
    return sprintf(str, "VoiceEngine 4.1.0\n");
}

#ifdef WEBRTC_EXTERNAL_TRANSPORT
int32_t VoEBaseImpl::AddExternalTransportBuild(char* str) const
{
    return sprintf(str, "External transport build\n");
}
#endif

#ifdef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
int32_t VoEBaseImpl::AddExternalRecAndPlayoutBuild(char* str) const
{
    return sprintf(str, "External recording and playout build\n");
}
#endif

int VoEBaseImpl::LastError()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "LastError()");
    return (_shared->statistics().LastError());
}


int VoEBaseImpl::SetNetEQPlayoutMode(int channel, NetEqModes mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetNetEQPlayoutMode(channel=%i, mode=%i)", channel, mode);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetNetEQPlayoutMode() failed to locate channel");
        return -1;
    }
    return channelPtr->SetNetEQPlayoutMode(mode);
}

int VoEBaseImpl::GetNetEQPlayoutMode(int channel, NetEqModes& mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetNetEQPlayoutMode(channel=%i, mode=?)", channel);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "GetNetEQPlayoutMode() failed to locate channel");
        return -1;
    }
    return channelPtr->GetNetEQPlayoutMode(mode);
}

int VoEBaseImpl::SetOnHoldStatus(int channel, bool enable, OnHoldModes mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetOnHoldStatus(channel=%d, enable=%d, mode=%d)", channel,
                 enable, mode);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetOnHoldStatus() failed to locate channel");
        return -1;
    }
    return channelPtr->SetOnHoldStatus(enable, mode);
}

int VoEBaseImpl::GetOnHoldStatus(int channel, bool& enabled, OnHoldModes& mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetOnHoldStatus(channel=%d, enabled=?, mode=?)", channel);
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "GetOnHoldStatus() failed to locate channel");
        return -1;
    }
    return channelPtr->GetOnHoldStatus(enabled, mode);
}

int32_t VoEBaseImpl::StartPlayout()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoEBaseImpl::StartPlayout()");
    if (_shared->audio_device()->Playing())
    {
        return 0;
    }
    if (!_shared->ext_playout())
    {
        if (_shared->audio_device()->InitPlayout() != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "StartPlayout() failed to initialize playout");
            return -1;
        }
        if (_shared->audio_device()->StartPlayout() != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "StartPlayout() failed to start playout");
            return -1;
        }
    }
    return 0;
}

int32_t VoEBaseImpl::StopPlayout() {
  WEBRTC_TRACE(kTraceInfo,
               kTraceVoice,
               VoEId(_shared->instance_id(), -1),
               "VoEBaseImpl::StopPlayout()");
  // Stop audio-device playing if no channel is playing out
  if (_shared->NumOfSendingChannels() == 0) {
    if (_shared->audio_device()->StopPlayout() != 0) {
      _shared->SetLastError(VE_CANNOT_STOP_PLAYOUT,
                            kTraceError,
                            "StopPlayout() failed to stop playout");
      return -1;
    }
  }
  return 0;
}

int32_t VoEBaseImpl::StartSend()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoEBaseImpl::StartSend()");
    if (_shared->audio_device()->Recording())
    {
        return 0;
    }
    if (!_shared->ext_recording())
    {
        if (_shared->audio_device()->InitRecording() != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "StartSend() failed to initialize recording");
            return -1;
        }
        if (_shared->audio_device()->StartRecording() != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVoice,
                VoEId(_shared->instance_id(), -1),
                "StartSend() failed to start recording");
            return -1;
        }
    }

    return 0;
}

int32_t VoEBaseImpl::StopSend()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoEBaseImpl::StopSend()");

    if (_shared->NumOfSendingChannels() == 0 &&
        !_shared->transmit_mixer()->IsRecordingMic())
    {
        // Stop audio-device recording if no channel is recording
        if (_shared->audio_device()->StopRecording() != 0)
        {
            _shared->SetLastError(VE_CANNOT_STOP_RECORDING, kTraceError,
                "StopSend() failed to stop recording");
            return -1;
        }
        _shared->transmit_mixer()->StopSend();
    }

    return 0;
}

int32_t VoEBaseImpl::TerminateInternal()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoEBaseImpl::TerminateInternal()");

    // Delete any remaining channel objects
    _shared->channel_manager().DestroyAllChannels();

    if (_shared->process_thread())
    {
        if (_shared->audio_device())
        {
            if (_shared->process_thread()->
                    DeRegisterModule(_shared->audio_device()) != 0)
            {
                _shared->SetLastError(VE_THREAD_ERROR, kTraceError,
                    "TerminateInternal() failed to deregister ADM");
            }
        }
        if (_shared->process_thread()->Stop() != 0)
        {
            _shared->SetLastError(VE_THREAD_ERROR, kTraceError,
                "TerminateInternal() failed to stop module process thread");
        }
    }

    if (_shared->audio_device())
    {
        if (_shared->audio_device()->StopPlayout() != 0)
        {
            _shared->SetLastError(VE_SOUNDCARD_ERROR, kTraceWarning,
                "TerminateInternal() failed to stop playout");
        }
        if (_shared->audio_device()->StopRecording() != 0)
        {
            _shared->SetLastError(VE_SOUNDCARD_ERROR, kTraceWarning,
                "TerminateInternal() failed to stop recording");
        }
        if (_shared->audio_device()->RegisterEventObserver(NULL) != 0) {
          _shared->SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR, kTraceWarning,
              "TerminateInternal() failed to de-register event observer "
              "for the ADM");
        }
        if (_shared->audio_device()->RegisterAudioCallback(NULL) != 0) {
          _shared->SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR, kTraceWarning,
              "TerminateInternal() failed to de-register audio callback "
              "for the ADM");
        }
        if (_shared->audio_device()->Terminate() != 0)
        {
            _shared->SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR, kTraceError,
                "TerminateInternal() failed to terminate the ADM");
        }
        _shared->set_audio_device(NULL);
    }

    if (_shared->audio_processing()) {
        _shared->set_audio_processing(NULL);
    }

    return _shared->statistics().SetUnInitialized();
}

int VoEBaseImpl::ProcessRecordedDataWithAPM(
    const int voe_channels[],
    int number_of_voe_channels,
    const void* audio_data,
    uint32_t sample_rate,
    uint8_t number_of_channels,
    uint32_t number_of_frames,
    uint32_t audio_delay_milliseconds,
    int32_t clock_drift,
    uint32_t current_volume,
    bool key_pressed) {
  assert(_shared->transmit_mixer() != NULL);
  assert(_shared->audio_device() != NULL);

  bool is_analog_agc(false);
  if (_shared->audio_processing() &&
      _shared->audio_processing()->gain_control()->mode() ==
          GainControl::kAdaptiveAnalog) {
    is_analog_agc = true;
  }

  // Only deal with the volume in adaptive analog mode.
  uint32_t max_volume = 0;
  uint16_t current_voe_mic_level = 0;
  if (is_analog_agc) {
    // Scale from ADM to VoE level range
    if (_shared->audio_device()->MaxMicrophoneVolume(&max_volume) == 0) {
      if (max_volume) {
        current_voe_mic_level = static_cast<uint16_t>(
            (current_volume * kMaxVolumeLevel +
                static_cast<int>(max_volume / 2)) / max_volume);
      }
    }
    // We learned that on certain systems (e.g Linux) the current_voe_mic_level
    // can be greater than the maxVolumeLevel therefore
    // we are going to cap the current_voe_mic_level to the maxVolumeLevel
    // and change the maxVolume to current_volume if it turns out that
    // the current_voe_mic_level is indeed greater than the maxVolumeLevel.
    if (current_voe_mic_level > kMaxVolumeLevel) {
      current_voe_mic_level = kMaxVolumeLevel;
      max_volume = current_volume;
    }
  }

  // Keep track if the MicLevel has been changed by the AGC, if not,
  // use the old value AGC returns to let AGC continue its trend,
  // so eventually the AGC is able to change the mic level. This handles
  // issues with truncation introduced by the scaling.
  if (_oldMicLevel == current_volume)
    current_voe_mic_level = static_cast<uint16_t>(_oldVoEMicLevel);

  // Perform channel-independent operations
  // (APM, mix with file, record to file, mute, etc.)
  _shared->transmit_mixer()->PrepareDemux(
      audio_data, number_of_frames, number_of_channels, sample_rate,
      static_cast<uint16_t>(audio_delay_milliseconds), clock_drift,
      current_voe_mic_level, key_pressed);

  // Copy the audio frame to each sending channel and perform
  // channel-dependent operations (file mixing, mute, etc.), encode and
  // packetize+transmit the RTP packet. When |number_of_voe_channels| == 0,
  // do the operations on all the existing VoE channels; otherwise the
  // operations will be done on specific channels.
  if (number_of_voe_channels == 0) {
    _shared->transmit_mixer()->DemuxAndMix();
    _shared->transmit_mixer()->EncodeAndSend();
  } else {
    _shared->transmit_mixer()->DemuxAndMix(voe_channels,
                                           number_of_voe_channels);
    _shared->transmit_mixer()->EncodeAndSend(voe_channels,
                                             number_of_voe_channels);
  }

  if (!is_analog_agc)
    return 0;

  // Scale from VoE to ADM level range.
  uint32_t new_voe_mic_level = _shared->transmit_mixer()->CaptureLevel();

  // Keep track of the value AGC returns.
  _oldVoEMicLevel = new_voe_mic_level;
  _oldMicLevel = current_volume;

  if (new_voe_mic_level != current_voe_mic_level) {
    // Return the new volume if AGC has changed the volume.
    return static_cast<int>(
        (new_voe_mic_level * max_volume +
            static_cast<int>(kMaxVolumeLevel / 2)) / kMaxVolumeLevel);
  }

  // Return 0 to indicate no change on the volume.
  return 0;
}

}  // namespace webrtc
