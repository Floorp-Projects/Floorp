/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/android/opensles_input.h"

#include <assert.h>
#include <dlfcn.h>

#include "OpenSLESProvider.h"
#include "webrtc/modules/audio_device/android/audio_common.h"
#include "webrtc/modules/audio_device/android/opensles_common.h"
#include "webrtc/modules/audio_device/android/single_rw_fifo.h"
#include "webrtc/modules/audio_device/audio_device_buffer.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

#if defined(WEBRTC_GONK) && defined(WEBRTC_HARDWARE_AEC_NS)
#include <media/AudioSystem.h>
#include <audio_effects/effect_aec.h>
#include <audio_effects/effect_ns.h>
#include <utils/Errors.h>
#endif

#define VOID_RETURN
#define OPENSL_RETURN_ON_FAILURE(op, ret_val)                    \
  do {                                                           \
    SLresult err = (op);                                         \
    if (err != SL_RESULT_SUCCESS) {                              \
      assert(false);                                             \
      return ret_val;                                            \
    }                                                            \
  } while (0)

static const SLEngineOption kOption[] = {
  { SL_ENGINEOPTION_THREADSAFE, static_cast<SLuint32>(SL_BOOLEAN_TRUE) },
};

enum {
  kNoOverrun,
  kOverrun,
};

namespace webrtc {

OpenSlesInput::OpenSlesInput(
    PlayoutDelayProvider* delay_provider, AudioManager* audio_manager)
    : delay_provider_(delay_provider),
      initialized_(false),
      mic_initialized_(false),
      rec_initialized_(false),
      crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      recording_(false),
      num_fifo_buffers_needed_(0),
      number_overruns_(0),
      sles_engine_(NULL),
      sles_engine_itf_(NULL),
      sles_recorder_(NULL),
      sles_recorder_itf_(NULL),
      sles_recorder_sbq_itf_(NULL),
      audio_buffer_(NULL),
      active_queue_(0),
      rec_sampling_rate_(0),
      agc_enabled_(false),
#if defined(WEBRTC_GONK) && defined(WEBRTC_HARDWARE_AEC_NS)
      aec_(NULL),
      ns_(NULL),
#endif
      recording_delay_(0),
      opensles_lib_(NULL) {
}

OpenSlesInput::~OpenSlesInput() {
}

int32_t OpenSlesInput::SetAndroidAudioDeviceObjects(void* javaVM,
                                                    void* context) {
#if !defined(WEBRTC_GONK)
  AudioManagerJni::SetAndroidAudioDeviceObjects(javaVM, context);
#endif
  return 0;
}

void OpenSlesInput::ClearAndroidAudioDeviceObjects() {
#if !defined(WEBRTC_GONK)
  AudioManagerJni::ClearAndroidAudioDeviceObjects();
#endif
}

int32_t OpenSlesInput::Init() {
  assert(!initialized_);

  /* Try to dynamically open the OpenSLES library */
  opensles_lib_ = dlopen("libOpenSLES.so", RTLD_LAZY);
  if (!opensles_lib_) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, 0,
                   "  failed to dlopen OpenSLES library");
      return -1;
  }

  f_slCreateEngine = (slCreateEngine_t)dlsym(opensles_lib_, "slCreateEngine");
  SL_IID_ENGINE_ = *(SLInterfaceID *)dlsym(opensles_lib_, "SL_IID_ENGINE");
  SL_IID_BUFFERQUEUE_ = *(SLInterfaceID *)dlsym(opensles_lib_, "SL_IID_BUFFERQUEUE");
  SL_IID_ANDROIDCONFIGURATION_ = *(SLInterfaceID *)dlsym(opensles_lib_, "SL_IID_ANDROIDCONFIGURATION");
  SL_IID_ANDROIDSIMPLEBUFFERQUEUE_ = *(SLInterfaceID *)dlsym(opensles_lib_, "SL_IID_ANDROIDSIMPLEBUFFERQUEUE");
  SL_IID_RECORD_ = *(SLInterfaceID *)dlsym(opensles_lib_, "SL_IID_RECORD");

  if (!f_slCreateEngine ||
      !SL_IID_ENGINE_ ||
      !SL_IID_BUFFERQUEUE_ ||
      !SL_IID_ANDROIDCONFIGURATION_ ||
      !SL_IID_ANDROIDSIMPLEBUFFERQUEUE_ ||
      !SL_IID_RECORD_) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, 0,
                   "  failed to find OpenSLES function");
      return -1;
  }

  // Set up OpenSL engine.
#ifndef MOZILLA_INTERNAL_API
  OPENSL_RETURN_ON_FAILURE(f_slCreateEngine(&sles_engine_, 1, kOption, 0,
                                            NULL, NULL),
                           -1);
#else
  OPENSL_RETURN_ON_FAILURE(mozilla_get_sles_engine(&sles_engine_, 1, kOption), -1);
#endif
#ifndef MOZILLA_INTERNAL_API
  OPENSL_RETURN_ON_FAILURE((*sles_engine_)->Realize(sles_engine_,
                                                    SL_BOOLEAN_FALSE),
                           -1);
#else
  OPENSL_RETURN_ON_FAILURE(mozilla_realize_sles_engine(sles_engine_), -1);
#endif
  OPENSL_RETURN_ON_FAILURE((*sles_engine_)->GetInterface(sles_engine_,
                                                         SL_IID_ENGINE_,
                                                         &sles_engine_itf_),
                           -1);

  if (InitSampleRate() != 0) {
    return -1;
  }
  AllocateBuffers();
  initialized_ = true;
  return 0;
}

int32_t OpenSlesInput::Terminate() {
  // It is assumed that the caller has stopped recording before terminating.
  assert(!recording_);
#ifndef MOZILLA_INTERNAL_API
  (*sles_engine_)->Destroy(sles_engine_);
#else
  mozilla_destroy_sles_engine(&sles_engine_);
#endif
  initialized_ = false;
  mic_initialized_ = false;
  rec_initialized_ = false;
  dlclose(opensles_lib_);
  return 0;
}

int32_t OpenSlesInput::RecordingDeviceName(uint16_t index,
                                           char name[kAdmMaxDeviceNameSize],
                                           char guid[kAdmMaxGuidSize]) {
  assert(index == 0);
  // Empty strings.
  name[0] = '\0';
  guid[0] = '\0';
  return 0;
}

int32_t OpenSlesInput::SetRecordingDevice(uint16_t index) {
  assert(index == 0);
  return 0;
}

int32_t OpenSlesInput::RecordingIsAvailable(bool& available) {  // NOLINT
  available = true;
  return 0;
}

int32_t OpenSlesInput::InitRecording() {
  assert(initialized_);
  rec_initialized_ = true;
  return 0;
}

int32_t OpenSlesInput::StartRecording() {
  assert(rec_initialized_);
  assert(!recording_);
  if (!CreateAudioRecorder()) {
    return -1;
  }
  // Setup to receive buffer queue event callbacks.
  OPENSL_RETURN_ON_FAILURE(
      (*sles_recorder_sbq_itf_)->RegisterCallback(
          sles_recorder_sbq_itf_,
          RecorderSimpleBufferQueueCallback,
          this),
      -1);

  if (!EnqueueAllBuffers()) {
    return -1;
  }

  {
    // To prevent the compiler from e.g. optimizing the code to
    // recording_ = StartCbThreads() which wouldn't have been thread safe.
    CriticalSectionScoped lock(crit_sect_.get());
    recording_ = true;
  }
  if (!StartCbThreads()) {
    recording_ = false;
    return -1;
  }
  return 0;
}

int32_t OpenSlesInput::StopRecording() {
  StopCbThreads();
  DestroyAudioRecorder();
  recording_ = false;
  return 0;
}

int32_t OpenSlesInput::SetAGC(bool enable) {
  agc_enabled_ = enable;
  return 0;
}

int32_t OpenSlesInput::InitMicrophone() {
  assert(initialized_);
  assert(!recording_);
  mic_initialized_ = true;
  return 0;
}

int32_t OpenSlesInput::MicrophoneVolumeIsAvailable(bool& available) {  // NOLINT
  available = false;
  return 0;
}

int32_t OpenSlesInput::MinMicrophoneVolume(
    uint32_t& minVolume) const {  // NOLINT
  minVolume = 0;
  return 0;
}

int32_t OpenSlesInput::MicrophoneVolumeStepSize(
    uint16_t& stepSize) const {
  stepSize = 1;
  return 0;
}

int32_t OpenSlesInput::MicrophoneMuteIsAvailable(bool& available) {  // NOLINT
  available = false;  // Mic mute not supported on Android
  return 0;
}

int32_t OpenSlesInput::MicrophoneBoostIsAvailable(bool& available) {  // NOLINT
  available = false;  // Mic boost not supported on Android.
  return 0;
}

int32_t OpenSlesInput::SetMicrophoneBoost(bool enable) {
  assert(false);
  return -1;  // Not supported
}

int32_t OpenSlesInput::MicrophoneBoost(bool& enabled) const {  // NOLINT
  assert(false);
  return -1;  // Not supported
}

int32_t OpenSlesInput::StereoRecordingIsAvailable(bool& available) {  // NOLINT
  available = false;  // Stereo recording not supported on Android.
  return 0;
}

int32_t OpenSlesInput::SetStereoRecording(bool enable) {  // NOLINT
  if (enable) {
    return -1;
  } else {
    return 0;
  }
}

int32_t OpenSlesInput::StereoRecording(bool& enabled) const {  // NOLINT
  enabled = false;
  return 0;
}

int32_t OpenSlesInput::RecordingDelay(uint16_t& delayMS) const {  // NOLINT
  delayMS = recording_delay_;
  return 0;
}

void OpenSlesInput::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  audio_buffer_ = audioBuffer;
}

int OpenSlesInput::InitSampleRate() {
  UpdateSampleRate();
  audio_buffer_->SetRecordingSampleRate(rec_sampling_rate_);
  audio_buffer_->SetRecordingChannels(kNumChannels);
  UpdateRecordingDelay();
  return 0;
}

int OpenSlesInput::buffer_size_samples() const {
  // Since there is no low latency recording, use buffer size corresponding to
  // 10ms of data since that's the framesize WebRTC uses. Getting any other
  // size would require patching together buffers somewhere before passing them
  // to WebRTC.
  return rec_sampling_rate_ * 10 / 1000;
}

int OpenSlesInput::buffer_size_bytes() const {
  return buffer_size_samples() * kNumChannels * sizeof(int16_t);
}

void OpenSlesInput::UpdateRecordingDelay() {
  // TODO(hellner): Add accurate delay estimate.
  // On average half the current buffer will have been filled with audio.
  int outstanding_samples =
      (TotalBuffersUsed() - 0.5) * buffer_size_samples();
  recording_delay_ = outstanding_samples / (rec_sampling_rate_ / 1000);
}

void OpenSlesInput::UpdateSampleRate() {
#if !defined(WEBRTC_GONK)
  rec_sampling_rate_ = audio_manager_.low_latency_supported() ?
      audio_manager_.native_output_sample_rate() : kDefaultSampleRate;
#else
  rec_sampling_rate_ = kDefaultSampleRate;
#endif
}

void OpenSlesInput::CalculateNumFifoBuffersNeeded() {
  // Buffer size is 10ms of data.
  num_fifo_buffers_needed_ = kNum10MsToBuffer;
}

void OpenSlesInput::AllocateBuffers() {
  // Allocate FIFO to handle passing buffers between processing and OpenSL
  // threads.
  CalculateNumFifoBuffersNeeded();
  assert(num_fifo_buffers_needed_ > 0);
  fifo_.reset(new SingleRwFifo(num_fifo_buffers_needed_));

  // Allocate the memory area to be used.
  rec_buf_.reset(new rtc::scoped_ptr<int8_t[]>[TotalBuffersUsed()]);
  for (int i = 0; i < TotalBuffersUsed(); ++i) {
    rec_buf_[i].reset(new int8_t[buffer_size_bytes()]);
  }
}

int OpenSlesInput::TotalBuffersUsed() const {
  return num_fifo_buffers_needed_ + kNumOpenSlBuffers;
}

bool OpenSlesInput::EnqueueAllBuffers() {
  active_queue_ = 0;
  number_overruns_ = 0;
  for (int i = 0; i < kNumOpenSlBuffers; ++i) {
    memset(rec_buf_[i].get(), 0, buffer_size_bytes());
    OPENSL_RETURN_ON_FAILURE(
        (*sles_recorder_sbq_itf_)->Enqueue(
            sles_recorder_sbq_itf_,
            reinterpret_cast<void*>(rec_buf_[i].get()),
            buffer_size_bytes()),
        false);
  }
  // In case of underrun the fifo will be at capacity. In case of first enqueue
  // no audio can have been returned yet meaning fifo must be empty. Any other
  // values are unexpected.
  assert(fifo_->size() == fifo_->capacity() ||
         fifo_->size() == 0);
  // OpenSL recording has been stopped. I.e. only this thread is touching
  // |fifo_|.
  while (fifo_->size() != 0) {
    // Clear the fifo.
    fifo_->Pop();
  }
  return true;
}

void OpenSlesInput::SetupVoiceMode() {
  SLAndroidConfigurationItf configItf;
  SLresult res = (*sles_recorder_)->GetInterface(sles_recorder_, SL_IID_ANDROIDCONFIGURATION_,
                                                 (void*)&configItf);
  WEBRTC_TRACE(kTraceError, kTraceAudioDevice, 0, "OpenSL GetInterface: %d", res);

  if (res == SL_RESULT_SUCCESS) {
    SLuint32 voiceMode = SL_ANDROID_RECORDING_PRESET_VOICE_COMMUNICATION;
    SLuint32 voiceSize = sizeof(voiceMode);

    res = (*configItf)->SetConfiguration(configItf,
                                         SL_ANDROID_KEY_RECORDING_PRESET,
                                         &voiceMode, voiceSize);
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, 0, "OpenSL Set Voice mode res: %d", res);
  }
}

#if defined(WEBRTC_GONK) && defined(WEBRTC_HARDWARE_AEC_NS)
bool OpenSlesInput::CheckPlatformAEC() {
  effect_descriptor_t fxDesc;
  uint32_t numFx;

  if (android::AudioEffect::queryNumberEffects(&numFx) != android::NO_ERROR) {
    return false;
  }

  WEBRTC_TRACE(kTraceError, kTraceAudioDevice, 0, "Platform has %d effects", numFx);

  for (uint32_t i = 0; i < numFx; i++) {
    if (android::AudioEffect::queryEffect(i, &fxDesc) != android::NO_ERROR) {
      continue;
    }
    if (memcmp(&fxDesc.type, FX_IID_AEC, sizeof(fxDesc.type)) == 0) {
      return true;
    }
  }

  return false;
}

void OpenSlesInput::SetupAECAndNS() {
  bool hasAec = CheckPlatformAEC();
  WEBRTC_TRACE(kTraceError, kTraceAudioDevice, 0, "Platform has AEC: %d", hasAec);
  // This code should not have been enabled if this fails, because it means the
  // software AEC has will have been disabled as well. If you hit this, you need
  // to fix your B2G config or fix the hardware AEC on your device.
  assert(hasAec);

  SLAndroidConfigurationItf configItf;
  SLresult res = (*sles_recorder_)->GetInterface(sles_recorder_, SL_IID_ANDROIDCONFIGURATION_,
                                                 (void*)&configItf);
  WEBRTC_TRACE(kTraceError, kTraceAudioDevice, 0, "OpenSL GetInterface: %d", res);

  if (res == SL_RESULT_SUCCESS) {
    SLuint32 sessionId = 0;
    SLuint32 idSize = sizeof(sessionId);
    res = (*configItf)->GetConfiguration(configItf,
                                         SL_ANDROID_KEY_RECORDING_SESSION_ID,
                                         &idSize, &sessionId);
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, 0, "OpenSL Get sessionId res: %d", res);

    if (res == SL_RESULT_SUCCESS && idSize == sizeof(sessionId)) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, 0, "OpenSL sessionId: %d", sessionId);

      aec_ = new android::AudioEffect(FX_IID_AEC, NULL, 0, 0, 0, sessionId, 0);
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, 0, "OpenSL aec: %p", aec_);

      if (aec_) {
        android::status_t status = aec_->initCheck();
        if (status == android::NO_ERROR || status == android::ALREADY_EXISTS) {
          WEBRTC_TRACE(kTraceError, kTraceAudioDevice, 0, "OpenSL aec enabled");
          aec_->setEnabled(true);
        } else {
          WEBRTC_TRACE(kTraceError, kTraceAudioDevice, 0, "OpenSL aec disabled: %d", status);
          delete aec_;
          aec_ = NULL;
        }
      }

      ns_ = new android::AudioEffect(FX_IID_NS, NULL, 0, 0, 0, sessionId, 0);
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, 0, "OpenSL ns: %p", ns_);

      if (ns_) {
        android::status_t status = ns_->initCheck();
        if (status == android::NO_ERROR || status == android::ALREADY_EXISTS) {
          WEBRTC_TRACE(kTraceError, kTraceAudioDevice, 0, "OpenSL ns enabled");
          ns_->setEnabled(true);
        } else {
          WEBRTC_TRACE(kTraceError, kTraceAudioDevice, 0, "OpenSL ns disabled: %d", status);
          delete ns_;
          ns_ = NULL;
        }
      }
    }
  }
}
#endif

bool OpenSlesInput::CreateAudioRecorder() {
  if (!event_.Start()) {
    assert(false);
    return false;
  }
  SLDataLocator_IODevice micLocator = {
    SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
    SL_DEFAULTDEVICEID_AUDIOINPUT, NULL };
  SLDataSource audio_source = { &micLocator, NULL };

  SLDataLocator_AndroidSimpleBufferQueue simple_buf_queue = {
    SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
    static_cast<SLuint32>(TotalBuffersUsed())
  };
  SLDataFormat_PCM configuration =
      webrtc_opensl::CreatePcmConfiguration(rec_sampling_rate_);
  SLDataSink audio_sink = { &simple_buf_queue, &configuration };

  // Interfaces for recording android audio data and Android are needed.
  // Note the interfaces still need to be initialized. This only tells OpenSl
  // that the interfaces will be needed at some point.
  const SLInterfaceID id[kNumInterfaces] = {
    SL_IID_ANDROIDSIMPLEBUFFERQUEUE_, SL_IID_ANDROIDCONFIGURATION_ };
  const SLboolean req[kNumInterfaces] = {
    SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };
  OPENSL_RETURN_ON_FAILURE(
      (*sles_engine_itf_)->CreateAudioRecorder(sles_engine_itf_,
                                               &sles_recorder_,
                                               &audio_source,
                                               &audio_sink,
                                               kNumInterfaces,
                                               id,
                                               req),
      false);

  SLAndroidConfigurationItf recorder_config;
  OPENSL_RETURN_ON_FAILURE(
      (*sles_recorder_)->GetInterface(sles_recorder_,
                                      SL_IID_ANDROIDCONFIGURATION_,
                                      &recorder_config),
      false);

  SetupVoiceMode();

  // Realize the recorder in synchronous mode.
  OPENSL_RETURN_ON_FAILURE((*sles_recorder_)->Realize(sles_recorder_,
                                                      SL_BOOLEAN_FALSE),
                           false);

#if defined(WEBRTC_GONK) && defined(WEBRTC_HARDWARE_AEC_NS)
  SetupAECAndNS();
#endif

  OPENSL_RETURN_ON_FAILURE(
      (*sles_recorder_)->GetInterface(sles_recorder_, SL_IID_RECORD_,
                                      static_cast<void*>(&sles_recorder_itf_)),
      false);
  OPENSL_RETURN_ON_FAILURE(
      (*sles_recorder_)->GetInterface(
          sles_recorder_,
          SL_IID_ANDROIDSIMPLEBUFFERQUEUE_,
          static_cast<void*>(&sles_recorder_sbq_itf_)),
      false);
  return true;
}

void OpenSlesInput::DestroyAudioRecorder() {
  event_.Stop();

#if defined(WEBRTC_GONK) && defined(WEBRTC_HARDWARE_AEC_NS)
  delete aec_;
  delete ns_;
  aec_ = NULL;
  ns_ = NULL;
#endif

  if (sles_recorder_sbq_itf_) {
    // Release all buffers currently queued up.
    OPENSL_RETURN_ON_FAILURE(
        (*sles_recorder_sbq_itf_)->Clear(sles_recorder_sbq_itf_),
        VOID_RETURN);
    sles_recorder_sbq_itf_ = NULL;
  }
  sles_recorder_itf_ = NULL;

  if (sles_recorder_) {
    (*sles_recorder_)->Destroy(sles_recorder_);
    sles_recorder_ = NULL;
  }
}

bool OpenSlesInput::HandleOverrun(int event_id, int event_msg) {
  if (!recording_) {
    return false;
  }
  if (event_id == kNoOverrun) {
    return false;
  }
  assert(event_id == kOverrun);
  assert(event_msg > 0);
  // Wait for all enqueued buffers be flushed.
  if (event_msg != kNumOpenSlBuffers) {
    return true;
  }
  // All buffers passed to OpenSL have been flushed. Restart the audio from
  // scratch.
  // No need to check sles_recorder_itf_ as recording_ would be false before it
  // is set to NULL.
  OPENSL_RETURN_ON_FAILURE(
      (*sles_recorder_itf_)->SetRecordState(sles_recorder_itf_,
                                            SL_RECORDSTATE_STOPPED),
      true);
  EnqueueAllBuffers();
  OPENSL_RETURN_ON_FAILURE(
      (*sles_recorder_itf_)->SetRecordState(sles_recorder_itf_,
                                            SL_RECORDSTATE_RECORDING),
      true);
  return true;
}

void OpenSlesInput::RecorderSimpleBufferQueueCallback(
    SLAndroidSimpleBufferQueueItf queue_itf,
    void* context) {
  OpenSlesInput* audio_device = reinterpret_cast<OpenSlesInput*>(context);
  audio_device->RecorderSimpleBufferQueueCallbackHandler(queue_itf);
}

void OpenSlesInput::RecorderSimpleBufferQueueCallbackHandler(
    SLAndroidSimpleBufferQueueItf queue_itf) {
  if (fifo_->size() >= fifo_->capacity() || number_overruns_ > 0) {
    ++number_overruns_;
    event_.SignalEvent(kOverrun, number_overruns_);
    return;
  }
  int8_t* audio = rec_buf_[active_queue_].get();
  // There is at least one spot available in the fifo.
  fifo_->Push(audio);
  active_queue_ = (active_queue_ + 1) % TotalBuffersUsed();
  event_.SignalEvent(kNoOverrun, 0);
  // active_queue_ is indexing the next buffer to record to. Since the current
  // buffer has been recorded it means that the buffer index
  // kNumOpenSlBuffers - 1 past |active_queue_| contains the next free buffer.
  // Since |fifo_| wasn't at capacity, at least one buffer is free to be used.
  int next_free_buffer =
      (active_queue_ + kNumOpenSlBuffers - 1) % TotalBuffersUsed();
  OPENSL_RETURN_ON_FAILURE(
      (*sles_recorder_sbq_itf_)->Enqueue(
          sles_recorder_sbq_itf_,
          reinterpret_cast<void*>(rec_buf_[next_free_buffer].get()),
          buffer_size_bytes()),
      VOID_RETURN);
}

bool OpenSlesInput::StartCbThreads() {
  rec_thread_ = ThreadWrapper::CreateThread(CbThread, this,
                                            "opensl_rec_thread");
  assert(rec_thread_.get());
  if (!rec_thread_->Start()) {
    assert(false);
    return false;
  }
  rec_thread_->SetPriority(kRealtimePriority);
  OPENSL_RETURN_ON_FAILURE(
      (*sles_recorder_itf_)->SetRecordState(sles_recorder_itf_,
                                            SL_RECORDSTATE_RECORDING),
      false);
  return true;
}

void OpenSlesInput::StopCbThreads() {
  {
    CriticalSectionScoped lock(crit_sect_.get());
    recording_ = false;
  }
  if (sles_recorder_itf_) {
    OPENSL_RETURN_ON_FAILURE(
        (*sles_recorder_itf_)->SetRecordState(sles_recorder_itf_,
                                              SL_RECORDSTATE_STOPPED),
        VOID_RETURN);
  }
  if (rec_thread_.get() == NULL) {
    return;
  }
  event_.Stop();
  if (rec_thread_->Stop()) {
    rec_thread_.reset();
  } else {
    assert(false);
  }
}

bool OpenSlesInput::CbThread(void* context) {
  return reinterpret_cast<OpenSlesInput*>(context)->CbThreadImpl();
}

bool OpenSlesInput::CbThreadImpl() {
  int event_id;
  int event_msg;
  // event_ must not be waited on while a lock has been taken.
  event_.WaitOnEvent(&event_id, &event_msg);

  CriticalSectionScoped lock(crit_sect_.get());
  if (HandleOverrun(event_id, event_msg)) {
    return recording_;
  }
  // If the fifo_ has audio data process it.
  while (fifo_->size() > 0 && recording_) {
    int8_t* audio = fifo_->Pop();
    audio_buffer_->SetRecordedBuffer(audio, buffer_size_samples());
    audio_buffer_->SetVQEData(delay_provider_ ?
                              delay_provider_->PlayoutDelayMs() : 0,
                              recording_delay_, 0);
    audio_buffer_->DeliverRecordedData();
  }
  return recording_;
}

}  // namespace webrtc
