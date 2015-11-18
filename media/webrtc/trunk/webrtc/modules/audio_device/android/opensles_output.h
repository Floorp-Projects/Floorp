/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_OPENSLES_OUTPUT_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_OPENSLES_OUTPUT_H_

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <SLES/OpenSLES_AndroidConfiguration.h>

#include "webrtc/base/scoped_ptr.h"
#if !defined(WEBRTC_GONK)
#include "webrtc/modules/audio_device/android/audio_manager.h"
#include "webrtc/modules/audio_device/android/audio_manager_jni.h"
#endif
#include "webrtc/modules/audio_device/android/low_latency_event.h"
#include "webrtc/modules/audio_device/android/audio_common.h"
#include "webrtc/modules/audio_device/include/audio_device_defines.h"
#include "webrtc/modules/audio_device/include/audio_device.h"

namespace webrtc {

class AudioDeviceBuffer;
class CriticalSectionWrapper;
class FineAudioBuffer;
class SingleRwFifo;
class ThreadWrapper;

#if defined(WEBRTC_ANDROID_OPENSLES_OUTPUT)
// allow us to replace it with a dummy

// OpenSL implementation that facilitate playing PCM data to an android device.
// This class is Thread-compatible. I.e. Given an instance of this class, calls
// to non-const methods require exclusive access to the object.
class OpenSlesOutput : public PlayoutDelayProvider {
 public:
  // TODO(henrika): use this new audio manager instead of old.
  explicit OpenSlesOutput(AudioManager* audio_manager);
  virtual ~OpenSlesOutput();

  static int32_t SetAndroidAudioDeviceObjects(void* javaVM,
                                              void* context);
  static void ClearAndroidAudioDeviceObjects();

  // Main initializaton and termination
  int32_t Init();
  int32_t Terminate();
  bool Initialized() const { return initialized_; }

  // Device enumeration
  int16_t PlayoutDevices() { return 1; }

  int32_t PlayoutDeviceName(uint16_t index,
                            char name[kAdmMaxDeviceNameSize],
                            char guid[kAdmMaxGuidSize]);

  // Device selection
  int32_t SetPlayoutDevice(uint16_t index);
  int32_t SetPlayoutDevice(
      AudioDeviceModule::WindowsDeviceType device) { return 0; }

  // No-op
  int32_t SetPlayoutSampleRate(uint32_t sample_rate_hz) { return 0; }

  // Audio transport initialization
  int32_t PlayoutIsAvailable(bool& available);  // NOLINT
  int32_t InitPlayout();
  bool PlayoutIsInitialized() const { return play_initialized_; }

  // Audio transport control
  int32_t StartPlayout();
  int32_t StopPlayout();
  bool Playing() const { return playing_; }

  // Audio mixer initialization
  int32_t InitSpeaker();
  bool SpeakerIsInitialized() const { return speaker_initialized_; }

  // Speaker volume controls
  int32_t SpeakerVolumeIsAvailable(bool& available);  // NOLINT
  int32_t SetSpeakerVolume(uint32_t volume);
  int32_t SpeakerVolume(uint32_t& volume) const { return 0; }  // NOLINT
  int32_t MaxSpeakerVolume(uint32_t& maxVolume) const;  // NOLINT
  int32_t MinSpeakerVolume(uint32_t& minVolume) const;  // NOLINT
  int32_t SpeakerVolumeStepSize(uint16_t& stepSize) const;  // NOLINT

  // Speaker mute control
  int32_t SpeakerMuteIsAvailable(bool& available);  // NOLINT
  int32_t SetSpeakerMute(bool enable) { return -1; }
  int32_t SpeakerMute(bool& enabled) const { return -1; }  // NOLINT


  // Stereo support
  int32_t StereoPlayoutIsAvailable(bool& available);  // NOLINT
  int32_t SetStereoPlayout(bool enable);
  int32_t StereoPlayout(bool& enabled) const;  // NOLINT

  // Delay information and control
  int32_t SetPlayoutBuffer(const AudioDeviceModule::BufferType type,
                                   uint16_t sizeMS) { return -1; }
  int32_t PlayoutBuffer(AudioDeviceModule::BufferType& type,  // NOLINT
                        uint16_t& sizeMS) const;
  int32_t PlayoutDelay(uint16_t& delayMS) const;  // NOLINT


  // Error and warning information
  bool PlayoutWarning() const { return false; }
  bool PlayoutError() const { return false; }
  void ClearPlayoutWarning() {}
  void ClearPlayoutError() {}

  // Attach audio buffer
  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);

  // Speaker audio routing
  int32_t SetLoudspeakerStatus(bool enable);
  int32_t GetLoudspeakerStatus(bool& enable) const;  // NOLINT

 protected:
  virtual int PlayoutDelayMs();

 private:
  enum {
    kNumInterfaces = 3,
    // TODO(xians): Reduce the numbers of buffers to improve the latency.
    //              Currently 30ms worth of buffers are needed due to audio
    //              pipeline processing jitter. Note: kNumOpenSlBuffers must
    //              not be changed.
    // According to the opensles documentation in the ndk:
    // The lower output latency path is used only if the application requests a
    // buffer count of 2 or more. Use minimum number of buffers to keep delay
    // as low as possible.
    kNumOpenSlBuffers = 2,
    // NetEq delivers frames on a 10ms basis. This means that every 10ms there
    // will be a time consuming task. Keeping 10ms worth of buffers will ensure
    // that there is 10ms to perform the time consuming task without running
    // into underflow.
    // In addition to the 10ms that needs to be stored for NetEq processing
    // there will be jitter in audio pipe line due to the acquisition of locks.
    // Note: The buffers in the OpenSL queue do not count towards the 10ms of
    // frames needed since OpenSL needs to have them ready for playout.
    kNum10MsToBuffer = 6,
  };

  bool InitSampleRate();
  bool SetLowLatency();
  void UpdatePlayoutDelay();
  // It might be possible to dynamically add or remove buffers based on how
  // close to depletion the fifo is. Few buffers means low delay. Too few
  // buffers will cause underrun. Dynamically changing the number of buffer
  // will greatly increase code complexity.
  void CalculateNumFifoBuffersNeeded();
  void AllocateBuffers();
  int TotalBuffersUsed() const;
  bool EnqueueAllBuffers();
  // This function also configures the audio player, e.g. sample rate to use
  // etc, so it should be called when starting playout.
  bool CreateAudioPlayer();
  void DestroyAudioPlayer();

  // When underrun happens there won't be a new frame ready for playout that
  // can be retrieved yet. Since the OpenSL thread must return ASAP there will
  // be one less queue available to OpenSL. This function handles this case
  // gracefully by restarting the audio, pushing silent frames to OpenSL for
  // playout. This will sound like a click. Underruns are also logged to
  // make it possible to identify these types of audio artifacts.
  // This function returns true if there has been underrun. Further processing
  // of audio data should be avoided until this function returns false again.
  // The function needs to be protected by |crit_sect_|.
  bool HandleUnderrun(int event_id, int event_msg);

  static void PlayerSimpleBufferQueueCallback(
      SLAndroidSimpleBufferQueueItf queueItf,
      void* pContext);
  // This function must not take any locks or do any heavy work. It is a
  // requirement for the OpenSL implementation to work as intended. The reason
  // for this is that taking locks exposes the OpenSL thread to the risk of
  // priority inversion.
  void PlayerSimpleBufferQueueCallbackHandler(
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

  bool initialized_;
  bool speaker_initialized_;
  bool play_initialized_;

  // Members that are read/write accessed concurrently by the process thread and
  // threads calling public functions of this class.
  rtc::scoped_ptr<ThreadWrapper> play_thread_;  // Processing thread
  rtc::scoped_ptr<CriticalSectionWrapper> crit_sect_;
  // This member controls the starting and stopping of playing audio to the
  // the device.
  bool playing_;

  // Only one thread, T1, may push and only one thread, T2, may pull. T1 may or
  // may not be the same thread as T2. T1 is the process thread and T2 is the
  // OpenSL thread.
  rtc::scoped_ptr<SingleRwFifo> fifo_;
  int num_fifo_buffers_needed_;
  LowLatencyEvent event_;
  int number_underruns_;

  // OpenSL handles
  SLObjectItf sles_engine_;
  SLEngineItf sles_engine_itf_;
  SLObjectItf sles_player_;
  SLPlayItf sles_player_itf_;
  SLAndroidSimpleBufferQueueItf sles_player_sbq_itf_;
  SLObjectItf sles_output_mixer_;

  // Audio buffers
  AudioDeviceBuffer* audio_buffer_;
  rtc::scoped_ptr<FineAudioBuffer> fine_buffer_;
  rtc::scoped_ptr<rtc::scoped_ptr<int8_t[]>[]> play_buf_;
  // Index in |rec_buf_| pointing to the audio buffer that will be ready the
  // next time PlayerSimpleBufferQueueCallbackHandler is invoked.
  // Ready means buffer is ready to be played out to device.
  int active_queue_;

  // Audio settings
  uint32_t speaker_sampling_rate_;
  int buffer_size_samples_;
  int buffer_size_bytes_;

  // Audio status
  uint16_t playout_delay_;

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
  SLInterfaceID SL_IID_PLAY_;
  SLInterfaceID SL_IID_ANDROIDSIMPLEBUFFERQUEUE_;
  SLInterfaceID SL_IID_VOLUME_;
};

#else

// Dummy OpenSlesOutput
class OpenSlesOutput : public PlayoutDelayProvider {
 public:
  explicit OpenSlesOutput(AudioManager* audio_manager) :
    initialized_(false), speaker_initialized_(false),
    play_initialized_(false), playing_(false)
  {}
  virtual ~OpenSlesOutput() {}

  static int32_t SetAndroidAudioDeviceObjects(void* javaVM,
                                              void* context) { return 0; }
  static void ClearAndroidAudioDeviceObjects() {}

  // Main initializaton and termination
  int32_t Init() { initialized_ = true; return 0; }
  int32_t Terminate() { initialized_ = false; return 0; }
  bool Initialized() const { return initialized_; }

  // Device enumeration
  int16_t PlayoutDevices() { return 1; }

  int32_t PlayoutDeviceName(uint16_t index,
                            char name[kAdmMaxDeviceNameSize],
                            char guid[kAdmMaxGuidSize])
  {
    assert(index == 0);
    // Empty strings.
    name[0] = '\0';
    guid[0] = '\0';
    return 0;
  }

  // Device selection
  int32_t SetPlayoutDevice(uint16_t index)
  {
    assert(index == 0);
    return 0;
  }
  int32_t SetPlayoutDevice(
      AudioDeviceModule::WindowsDeviceType device) { return 0; }

  // No-op
  int32_t SetPlayoutSampleRate(uint32_t sample_rate_hz) { return 0; }

  // Audio transport initialization
  int32_t PlayoutIsAvailable(bool& available)  // NOLINT
  {
    available = true;
    return 0;
  }
  int32_t InitPlayout()
  {
    assert(initialized_);
    play_initialized_ = true;
    return 0;
  }
  bool PlayoutIsInitialized() const { return play_initialized_; }

  // Audio transport control
  int32_t StartPlayout()
  {
    assert(play_initialized_);
    assert(!playing_);
    playing_ = true;
    return 0;
  }

  int32_t StopPlayout()
  {
    playing_ = false;
    return 0;
  }

  bool Playing() const { return playing_; }

  // Audio mixer initialization
  int32_t SpeakerIsAvailable(bool& available)  // NOLINT
  {
    available = true;
    return 0;
  }
  int32_t InitSpeaker()
  {
    assert(!playing_);
    speaker_initialized_ = true;
    return 0;
  }
  bool SpeakerIsInitialized() const { return speaker_initialized_; }

  // Speaker volume controls
  int32_t SpeakerVolumeIsAvailable(bool& available)  // NOLINT
  {
    available = true;
    return 0;
  }
  int32_t SetSpeakerVolume(uint32_t volume)
  {
    assert(speaker_initialized_);
    assert(initialized_);
    return 0;
  }
  int32_t SpeakerVolume(uint32_t& volume) const { return 0; }  // NOLINT
  int32_t MaxSpeakerVolume(uint32_t& maxVolume) const  // NOLINT
  {
    assert(speaker_initialized_);
    assert(initialized_);
    maxVolume = 0;
    return 0;
  }
  int32_t MinSpeakerVolume(uint32_t& minVolume) const  // NOLINT
  {
    assert(speaker_initialized_);
    assert(initialized_);
    minVolume = 0;
    return 0;
  }
  int32_t SpeakerVolumeStepSize(uint16_t& stepSize) const  // NOLINT
  {
    assert(speaker_initialized_);
    assert(initialized_);
    stepSize = 0;
    return 0;
  }

  // Speaker mute control
  int32_t SpeakerMuteIsAvailable(bool& available)  // NOLINT
  {
    available = true;
    return 0;
  }
  int32_t SetSpeakerMute(bool enable) { return -1; }
  int32_t SpeakerMute(bool& enabled) const { return -1; }  // NOLINT


  // Stereo support
  int32_t StereoPlayoutIsAvailable(bool& available)  // NOLINT
  {
    available = true;
    return 0;
  }
  int32_t SetStereoPlayout(bool enable)
  {
    return 0;
  }
  int32_t StereoPlayout(bool& enabled) const  // NOLINT
  {
    enabled = kNumChannels == 2;
    return 0;
  }

  // Delay information and control
  int32_t SetPlayoutBuffer(const AudioDeviceModule::BufferType type,
                                   uint16_t sizeMS) { return -1; }
  int32_t PlayoutBuffer(AudioDeviceModule::BufferType& type,  // NOLINT
                        uint16_t& sizeMS) const
  {
    type = AudioDeviceModule::kAdaptiveBufferSize;
    sizeMS = 40;
    return 0;
  }
  int32_t PlayoutDelay(uint16_t& delayMS) const  // NOLINT
  {
    delayMS = 0;
    return 0;
  }


  // Error and warning information
  bool PlayoutWarning() const { return false; }
  bool PlayoutError() const { return false; }
  void ClearPlayoutWarning() {}
  void ClearPlayoutError() {}

  // Attach audio buffer
  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {}

  // Speaker audio routing
  int32_t SetLoudspeakerStatus(bool enable) { return 0; }
  int32_t GetLoudspeakerStatus(bool& enable) const { enable = true; return 0; }  // NOLINT

 protected:
  virtual int PlayoutDelayMs() { return 40; }

 private:
  bool initialized_;
  bool speaker_initialized_;
  bool play_initialized_;
  bool playing_;
};
#endif

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_OPENSLES_OUTPUT_H_
