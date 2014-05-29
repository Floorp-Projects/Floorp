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

#include "webrtc/modules/audio_device/android/audio_common.h"
#include "webrtc/modules/audio_device/android/opensles_common.h"
#include "webrtc/modules/audio_device/android/single_rw_fifo.h"
#include "webrtc/modules/audio_device/audio_device_buffer.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

#define VOID_RETURN
#define OPENSL_RETURN_ON_FAILURE(op, ret_val)                    \
  do {                                                           \
    SLresult err = (op);                                         \
    if (err != SL_RESULT_SUCCESS) {                              \
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, id_,          \
                   "OpenSL error: %d", err);                     \
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
    const int32_t id, PlayoutDelayProvider* delay_provider)
    : id_(id),
      delay_provider_(delay_provider),
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
      recording_delay_(0) {
}

OpenSlesInput::~OpenSlesInput() {
}

int32_t OpenSlesInput::SetAndroidAudioDeviceObjects(void* javaVM,
                                                    void* env,
                                                    void* context) {
  return 0;
}

void OpenSlesInput::ClearAndroidAudioDeviceObjects() {
}

int32_t OpenSlesInput::Init() {
  assert(!initialized_);

  // Set up OpenSL engine.
  OPENSL_RETURN_ON_FAILURE(slCreateEngine(&sles_engine_, 1, kOption, 0,
                                          NULL, NULL),
                           -1);
  OPENSL_RETURN_ON_FAILURE((*sles_engine_)->Realize(sles_engine_,
                                                    SL_BOOLEAN_FALSE),
                           -1);
  OPENSL_RETURN_ON_FAILURE((*sles_engine_)->GetInterface(sles_engine_,
                                                         SL_IID_ENGINE,
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
  (*sles_engine_)->Destroy(sles_engine_);
  initialized_ = false;
  mic_initialized_ = false;
  rec_initialized_ = false;
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

int32_t OpenSlesInput::MicrophoneIsAvailable(bool& available) {  // NOLINT
  available = true;
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
  rec_sampling_rate_ = audio_manager_.low_latency_supported() ?
      audio_manager_.native_output_sample_rate() : kDefaultSampleRate;
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
  rec_buf_.reset(new scoped_array<int8_t>[TotalBuffersUsed()]);
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
    SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION };
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

  // Realize the recorder in synchronous mode.
  OPENSL_RETURN_ON_FAILURE((*sles_recorder_)->Realize(sles_recorder_,
                                                      SL_BOOLEAN_FALSE),
                           false);
  OPENSL_RETURN_ON_FAILURE(
      (*sles_recorder_)->GetInterface(sles_recorder_, SL_IID_RECORD,
                                      static_cast<void*>(&sles_recorder_itf_)),
      false);
  OPENSL_RETURN_ON_FAILURE(
      (*sles_recorder_)->GetInterface(
          sles_recorder_,
          SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
          static_cast<void*>(&sles_recorder_sbq_itf_)),
      false);
  return true;
}

void OpenSlesInput::DestroyAudioRecorder() {
  event_.Stop();
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
  WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, id_, "Audio overrun");
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
  rec_thread_.reset(ThreadWrapper::CreateThread(CbThread,
                                                this,
                                                kRealtimePriority,
                                                "opensl_rec_thread"));
  assert(rec_thread_.get());
  unsigned int thread_id = 0;
  if (!rec_thread_->Start(thread_id)) {
    assert(false);
    return false;
  }
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
    audio_buffer_->SetVQEData(delay_provider_->PlayoutDelayMs(),
                              recording_delay_, 0);
    audio_buffer_->DeliverRecordedData();
  }
  return recording_;
}

}  // namespace webrtc
