/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_OPENSLES_INPUT_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_OPENSLES_INPUT_H_

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <SLES/OpenSLES_AndroidConfiguration.h>

#if !defined(WEBRTC_GONK)
#include "webrtc/modules/audio_device/android/audio_manager_jni.h"
#endif
#include "webrtc/modules/audio_device/android/low_latency_event.h"
#include "webrtc/modules/audio_device/android/opensles_common.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/modules/audio_device/include/audio_device_defines.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class AudioDeviceBuffer;
class CriticalSectionWrapper;
class PlayoutDelayProvider;
class SingleRwFifo;
class ThreadWrapper;

// OpenSL implementation that facilitate capturing PCM data from an android
// device's microphone.
// This class is Thread-compatible. I.e. Given an instance of this class, calls
// to non-const methods require exclusive access to the object.
class OpenSlesInput {
 public:
  OpenSlesInput(const int32_t id,
                webrtc_opensl::PlayoutDelayProvider* delay_provider);
  ~OpenSlesInput();

  // Main initializaton and termination
  int32_t Init();
  int32_t Terminate();
  bool Initialized() const { return initialized_; }

  // Device enumeration
  int16_t RecordingDevices() { return 1; }
  int32_t RecordingDeviceName(uint16_t index,
                              char name[kAdmMaxDeviceNameSize],
                              char guid[kAdmMaxGuidSize]);

  // Device selection
  int32_t SetRecordingDevice(uint16_t index);
  int32_t SetRecordingDevice(
      AudioDeviceModule::WindowsDeviceType device) { return -1; }

  // Audio transport initialization
  int32_t RecordingIsAvailable(bool& available);  // NOLINT
  int32_t InitRecording();
  bool RecordingIsInitialized() const { return rec_initialized_; }

  // Audio transport control
  int32_t StartRecording();
  int32_t StopRecording();
  bool Recording() const { return recording_; }

  // Microphone Automatic Gain Control (AGC)
  int32_t SetAGC(bool enable);
  bool AGC() const { return agc_enabled_; }

  // Audio mixer initialization
  int32_t MicrophoneIsAvailable(bool& available);  // NOLINT
  int32_t InitMicrophone();
  bool MicrophoneIsInitialized() const { return mic_initialized_; }

  // Microphone volume controls
  int32_t MicrophoneVolumeIsAvailable(bool& available);  // NOLINT
  // TODO(leozwang): Add microphone volume control when OpenSL APIs
  // are available.
  int32_t SetMicrophoneVolume(uint32_t volume) { return 0; }
  int32_t MicrophoneVolume(uint32_t& volume) const { return -1; }  // NOLINT
  int32_t MaxMicrophoneVolume(
      uint32_t& maxVolume) const { return 0; }  // NOLINT
  int32_t MinMicrophoneVolume(uint32_t& minVolume) const;  // NOLINT
  int32_t MicrophoneVolumeStepSize(
      uint16_t& stepSize) const;  // NOLINT

  // Microphone mute control
  int32_t MicrophoneMuteIsAvailable(bool& available);  // NOLINT
  int32_t SetMicrophoneMute(bool enable) { return -1; }
  int32_t MicrophoneMute(bool& enabled) const { return -1; }  // NOLINT

  // Microphone boost control
  int32_t MicrophoneBoostIsAvailable(bool& available);  // NOLINT
  int32_t SetMicrophoneBoost(bool enable);
  int32_t MicrophoneBoost(bool& enabled) const;  // NOLINT

  // Stereo support
  int32_t StereoRecordingIsAvailable(bool& available);  // NOLINT
  int32_t SetStereoRecording(bool enable);
  int32_t StereoRecording(bool& enabled) const;  // NOLINT

  // Delay information and control
  int32_t RecordingDelay(uint16_t& delayMS) const;  // NOLINT

  bool RecordingWarning() const { return false; }
  bool RecordingError() const  { return false; }
  void ClearRecordingWarning() {}
  void ClearRecordingError() {}

  // Attach audio buffer
  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);

 private:
  enum {
    kNumInterfaces = 2,
    // Keep as few OpenSL buffers as possible to avoid wasting memory. 2 is
    // minimum for playout. Keep 2 for recording as well.
    kNumOpenSlBuffers = 2,
    kNum10MsToBuffer = 8,
  };

  int InitSampleRate();
  int buffer_size_samples() const;
  int buffer_size_bytes() const;
  void UpdateRecordingDelay();
  void UpdateSampleRate();
  void CalculateNumFifoBuffersNeeded();
  void AllocateBuffers();
  int TotalBuffersUsed() const;
  bool EnqueueAllBuffers();
  // This function also configures the audio recorder, e.g. sample rate to use
  // etc, so it should be called when starting recording.
  bool CreateAudioRecorder();
  void DestroyAudioRecorder();

  // When overrun happens there will be more frames received from OpenSL than
  // the desired number of buffers. It is possible to expand the number of
  // buffers as you go but that would greatly increase the complexity of this
  // code. HandleOverrun gracefully handles the scenario by restarting playout,
  // throwing away all pending audio data. This will sound like a click. This
  // is also logged to identify these types of clicks.
  // This function returns true if there has been overrun. Further processing
  // of audio data should be avoided until this function returns false again.
  // The function needs to be protected by |crit_sect_|.
  bool HandleOverrun(int event_id, int event_msg);

  static void RecorderSimpleBufferQueueCallback(
      SLAndroidSimpleBufferQueueItf queueItf,
      void* pContext);
  // This function must not take any locks or do any heavy work. It is a
  // requirement for the OpenSL implementation to work as intended. The reason
  // for this is that taking locks exposes the OpenSL thread to the risk of
  // priority inversion.
  void RecorderSimpleBufferQueueCallbackHandler(
      SLAndroidSimpleBufferQueueItf queueItf);

  bool StartCbThreads();
  void StopCbThreads();
  static bool CbThread(void* context);
  // This function must be protected against data race with threads calling this
  // class' public functions. It is a requirement for this class to be
  // Thread-compatible.
  bool CbThreadImpl();

#if !defined(WEBRTC_GONK)
  // Java API handle
  AudioManagerJni audio_manager_;
#endif

  int id_;
  webrtc_opensl::PlayoutDelayProvider* delay_provider_;
  bool initialized_;
  bool mic_initialized_;
  bool rec_initialized_;

  // Members that are read/write accessed concurrently by the process thread and
  // threads calling public functions of this class.
  scoped_ptr<ThreadWrapper> rec_thread_;  // Processing thread
  scoped_ptr<CriticalSectionWrapper> crit_sect_;
  // This member controls the starting and stopping of recording audio to the
  // the device.
  bool recording_;

  // Only one thread, T1, may push and only one thread, T2, may pull. T1 may or
  // may not be the same thread as T2. T2 is the process thread and T1 is the
  // OpenSL thread.
  scoped_ptr<SingleRwFifo> fifo_;
  int num_fifo_buffers_needed_;
  LowLatencyEvent event_;
  int number_overruns_;

  // OpenSL handles
  SLObjectItf sles_engine_;
  SLEngineItf sles_engine_itf_;
  SLObjectItf sles_recorder_;
  SLRecordItf sles_recorder_itf_;
  SLAndroidSimpleBufferQueueItf sles_recorder_sbq_itf_;

  // Audio buffers
  AudioDeviceBuffer* audio_buffer_;
  // Holds all allocated memory such that it is deallocated properly.
  scoped_array<scoped_array<int8_t> > rec_buf_;
  // Index in |rec_buf_| pointing to the audio buffer that will be ready the
  // next time RecorderSimpleBufferQueueCallbackHandler is invoked.
  // Ready means buffer contains audio data from the device.
  int active_queue_;

  // Audio settings
  uint32_t rec_sampling_rate_;
  bool agc_enabled_;

  // Audio status
  uint16_t recording_delay_;

  // dlopen for OpenSLES
  void *opensles_lib_;
  typedef SLresult (*slCreateEngine_t)(SLObjectItf *,
                                       SLuint32,
                                       const SLEngineOption *,
                                       SLuint32,
                                       const SLInterfaceID *,
                                       const SLboolean *);
  slCreateEngine_t f_slCreateEngine;
  SLInterfaceID SL_IID_ENGINE_;
  SLInterfaceID SL_IID_BUFFERQUEUE_;
  SLInterfaceID SL_IID_ANDROIDCONFIGURATION_;
  SLInterfaceID SL_IID_ANDROIDSIMPLEBUFFERQUEUE_;
  SLInterfaceID SL_IID_RECORD_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_OPENSLES_INPUT_H_
