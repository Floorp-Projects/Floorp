/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_DEVICE_OPENSLES_ANDROID_H_
#define SRC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_DEVICE_OPENSLES_ANDROID_H_

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <SLES/OpenSLES_AndroidConfiguration.h>

#include <queue>

#include "modules/audio_device/audio_device_generic.h"
#include "system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {

class EventWrapper;

class ThreadWrapper;

class AudioDeviceAndroidOpenSLES: public AudioDeviceGeneric {
 public:
  explicit AudioDeviceAndroidOpenSLES(const int32_t id);
  ~AudioDeviceAndroidOpenSLES();

  // Retrieve the currently utilized audio layer
  virtual int32_t
  ActiveAudioLayer(AudioDeviceModule::AudioLayer& audioLayer) const;  // NOLINT

  // Main initializaton and termination
  virtual int32_t Init();
  virtual int32_t Terminate();
  virtual bool Initialized() const;

  // Device enumeration
  virtual int16_t PlayoutDevices();
  virtual int16_t RecordingDevices();
  virtual int32_t
  PlayoutDeviceName(uint16_t index,
                    char name[kAdmMaxDeviceNameSize],
                    char guid[kAdmMaxGuidSize]);
  virtual int32_t
  RecordingDeviceName(uint16_t index,
                      char name[kAdmMaxDeviceNameSize],
                      char guid[kAdmMaxGuidSize]);

  // Device selection
  virtual int32_t SetPlayoutDevice(uint16_t index);
  virtual int32_t
  SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType device);
  virtual int32_t SetRecordingDevice(uint16_t index);
  virtual int32_t
  SetRecordingDevice(AudioDeviceModule::WindowsDeviceType device);

  // Audio transport initialization
  virtual int32_t PlayoutIsAvailable(bool& available);  // NOLINT
  virtual int32_t InitPlayout();
  virtual bool PlayoutIsInitialized() const;
  virtual int32_t RecordingIsAvailable(bool& available);  // NOLINT
  virtual int32_t InitRecording();
  virtual bool RecordingIsInitialized() const;

  // Audio transport control
  virtual int32_t StartPlayout();
  virtual int32_t StopPlayout();
  virtual bool Playing() const;
  virtual int32_t StartRecording();
  virtual int32_t StopRecording();
  virtual bool Recording() const;

  // Microphone Automatic Gain Control (AGC)
  virtual int32_t SetAGC(bool enable);
  virtual bool AGC() const;

  // Volume control based on the Windows Wave API (Windows only)
  virtual int32_t SetWaveOutVolume(uint16_t volumeLeft, uint16_t volumeRight);
  virtual int32_t WaveOutVolume(
      uint16_t& volumeLeft,  // NOLINT
      uint16_t& volumeRight) const;  // NOLINT

  // Audio mixer initialization
  virtual int32_t SpeakerIsAvailable(bool& available);  // NOLINT
  virtual int32_t InitSpeaker();
  virtual bool SpeakerIsInitialized() const;
  virtual int32_t MicrophoneIsAvailable(
      bool& available);
  virtual int32_t InitMicrophone();
  virtual bool MicrophoneIsInitialized() const;

  // Speaker volume controls
  virtual int32_t SpeakerVolumeIsAvailable(
      bool& available);  // NOLINT
  virtual int32_t SetSpeakerVolume(uint32_t volume);
  virtual int32_t SpeakerVolume(
      uint32_t& volume) const;  // NOLINT
  virtual int32_t MaxSpeakerVolume(
      uint32_t& maxVolume) const;  // NOLINT
  virtual int32_t MinSpeakerVolume(
      uint32_t& minVolume) const;  // NOLINT
  virtual int32_t SpeakerVolumeStepSize(
      uint16_t& stepSize) const;  // NOLINT

  // Microphone volume controls
  virtual int32_t MicrophoneVolumeIsAvailable(
      bool& available);  // NOLINT
  virtual int32_t SetMicrophoneVolume(uint32_t volume);
  virtual int32_t MicrophoneVolume(
      uint32_t& volume) const;  // NOLINT
  virtual int32_t MaxMicrophoneVolume(
      uint32_t& maxVolume) const;  // NOLINT
  virtual int32_t MinMicrophoneVolume(
      uint32_t& minVolume) const;  // NOLINT
  virtual int32_t
  MicrophoneVolumeStepSize(uint16_t& stepSize) const;  // NOLINT

  // Speaker mute control
  virtual int32_t SpeakerMuteIsAvailable(bool& available);  // NOLINT
  virtual int32_t SetSpeakerMute(bool enable);
  virtual int32_t SpeakerMute(bool& enabled) const;  // NOLINT

  // Microphone mute control
  virtual int32_t MicrophoneMuteIsAvailable(bool& available);  // NOLINT
  virtual int32_t SetMicrophoneMute(bool enable);
  virtual int32_t MicrophoneMute(bool& enabled) const;  // NOLINT

  // Microphone boost control
  virtual int32_t MicrophoneBoostIsAvailable(bool& available);  // NOLINT
  virtual int32_t SetMicrophoneBoost(bool enable);
  virtual int32_t MicrophoneBoost(bool& enabled) const;  // NOLINT

  // Stereo support
  virtual int32_t StereoPlayoutIsAvailable(bool& available);  // NOLINT
  virtual int32_t SetStereoPlayout(bool enable);
  virtual int32_t StereoPlayout(bool& enabled) const;  // NOLINT
  virtual int32_t StereoRecordingIsAvailable(bool& available);  // NOLINT
  virtual int32_t SetStereoRecording(bool enable);
  virtual int32_t StereoRecording(bool& enabled) const;  // NOLINT

  // Delay information and control
  virtual int32_t
  SetPlayoutBuffer(const AudioDeviceModule::BufferType type,
                   uint16_t sizeMS);
  virtual int32_t PlayoutBuffer(
      AudioDeviceModule::BufferType& type,  // NOLINT
      uint16_t& sizeMS) const;
  virtual int32_t PlayoutDelay(
      uint16_t& delayMS) const;  // NOLINT
  virtual int32_t RecordingDelay(
      uint16_t& delayMS) const;  // NOLINT

  // CPU load
  virtual int32_t CPULoad(uint16_t& load) const;  // NOLINT

  // Error and warning information
  virtual bool PlayoutWarning() const;
  virtual bool PlayoutError() const;
  virtual bool RecordingWarning() const;
  virtual bool RecordingError() const;
  virtual void ClearPlayoutWarning();
  virtual void ClearPlayoutError();
  virtual void ClearRecordingWarning();
  virtual void ClearRecordingError();

  // Attach audio buffer
  virtual void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);

  // Speaker audio routing
  virtual int32_t SetLoudspeakerStatus(bool enable);
  virtual int32_t GetLoudspeakerStatus(bool& enable) const;  // NOLINT

  static const uint32_t N_MAX_INTERFACES = 3;
  static const uint32_t N_MAX_OUTPUT_DEVICES = 6;
  static const uint32_t N_MAX_INPUT_DEVICES = 3;

  static const uint32_t N_REC_SAMPLES_PER_SEC = 16000;  // Default fs
  static const uint32_t N_PLAY_SAMPLES_PER_SEC = 16000;  // Default fs

  static const uint32_t N_REC_CHANNELS = 1;
  static const uint32_t N_PLAY_CHANNELS = 1;

  static const uint32_t REC_BUF_SIZE_IN_SAMPLES = 480;
  static const uint32_t PLAY_BUF_SIZE_IN_SAMPLES = 480;

  static const uint32_t REC_MAX_TEMP_BUF_SIZE_PER_10ms =
      N_REC_CHANNELS * REC_BUF_SIZE_IN_SAMPLES * sizeof(int16_t);

  static const uint32_t PLAY_MAX_TEMP_BUF_SIZE_PER_10ms =
      N_PLAY_CHANNELS * PLAY_BUF_SIZE_IN_SAMPLES * sizeof(int16_t);

  // Number of the buffers in playout queue
  static const uint16_t N_PLAY_QUEUE_BUFFERS = 8;
  // Number of buffers in recording queue
  // TODO(xian): Reduce the numbers of buffers to improve the latency.
  static const uint16_t N_REC_QUEUE_BUFFERS = 8;
  // Some values returned from getMinBufferSize
  // (Nexus S playout  72ms, recording 64ms)
  // (Galaxy,         167ms,           44ms)
  // (Nexus 7,         72ms,           48ms)
  // (Xoom             92ms,           40ms)

 private:
  // Lock
  void Lock() {
    crit_sect_.Enter();
  };
  void UnLock() {
    crit_sect_.Leave();
  };

  static void PlayerSimpleBufferQueueCallback(
      SLAndroidSimpleBufferQueueItf queueItf,
      void *pContext);
  static void RecorderSimpleBufferQueueCallback(
      SLAndroidSimpleBufferQueueItf queueItf,
      void *pContext);
  void PlayerSimpleBufferQueueCallbackHandler(
      SLAndroidSimpleBufferQueueItf queueItf);
  void RecorderSimpleBufferQueueCallbackHandler(
      SLAndroidSimpleBufferQueueItf queueItf);
  void CheckErr(SLresult res);

  // Delay updates
  void UpdateRecordingDelay();
  void UpdatePlayoutDelay(uint32_t nSamplePlayed);

  // Init
  int32_t InitSampleRate();

  // Misc
  AudioDeviceBuffer* voe_audio_buffer_;
  CriticalSectionWrapper& crit_sect_;
  // callback_crit_sect_ is used to lock rec_queue and rec_voe_ready_queue
  // and also for changing is_recording to false.  If you hold this and
  // crit_sect_, you must grab crit_sect_ first
  CriticalSectionWrapper& callback_crit_sect_;
  int32_t id_;

  // audio unit
  SLObjectItf sles_engine_;

  // playout device
  SLObjectItf sles_player_;
  SLEngineItf sles_engine_itf_;
  SLPlayItf sles_player_itf_;
  SLAndroidSimpleBufferQueueItf sles_player_sbq_itf_;
  SLObjectItf sles_output_mixer_;
  SLVolumeItf sles_speaker_volume_;

  // recording device
  SLObjectItf sles_recorder_;
  SLRecordItf sles_recorder_itf_;
  SLAndroidSimpleBufferQueueItf sles_recorder_sbq_itf_;
  SLDeviceVolumeItf sles_mic_volume_;
  uint32_t mic_dev_id_;

  uint32_t play_warning_, play_error_;
  uint32_t rec_warning_, rec_error_;

  // States
  bool is_recording_dev_specified_;
  bool is_playout_dev_specified_;
  bool is_initialized_;
  bool is_recording_;
  bool is_playing_;
  bool is_rec_initialized_;
  bool is_play_initialized_;
  bool is_mic_initialized_;
  bool is_speaker_initialized_;

  // Delay
  uint16_t playout_delay_;
  uint16_t recording_delay_;

  // AGC state
  bool agc_enabled_;

  // Threads
  ThreadWrapper* rec_thread_;
  uint32_t rec_thread_id_;
  static bool RecThreadFunc(void* context);
  bool RecThreadFuncImpl();
  EventWrapper& rec_timer_;

  uint32_t mic_sampling_rate_;
  uint32_t speaker_sampling_rate_;
  uint32_t max_speaker_vol_;
  uint32_t min_speaker_vol_;
  bool loundspeaker_on_;

  SLDataFormat_PCM player_pcm_;
  SLDataFormat_PCM record_pcm_;

  std::queue<int8_t*> rec_queue_;
  std::queue<int8_t*> rec_voe_audio_queue_;
  std::queue<int8_t*> rec_voe_ready_queue_;
  int8_t rec_buf_[N_REC_QUEUE_BUFFERS][
      N_REC_CHANNELS * sizeof(int16_t) * REC_BUF_SIZE_IN_SAMPLES];
  int8_t rec_voe_buf_[N_REC_QUEUE_BUFFERS][
      N_REC_CHANNELS * sizeof(int16_t) * REC_BUF_SIZE_IN_SAMPLES];

  std::queue<int8_t*> play_queue_;
  int8_t play_buf_[N_PLAY_QUEUE_BUFFERS][
      N_PLAY_CHANNELS * sizeof(int16_t) * PLAY_BUF_SIZE_IN_SAMPLES];

  // dlopen for OpenSLES
  void *opensles_lib_;
  SLInterfaceID SL_IID_ENGINE_;
  SLInterfaceID SL_IID_BUFFERQUEUE_;
  SLInterfaceID SL_IID_ANDROIDCONFIGURATION_;
  SLInterfaceID SL_IID_PLAY_;
  SLInterfaceID SL_IID_ANDROIDSIMPLEBUFFERQUEUE_;
  SLInterfaceID SL_IID_RECORD_;
};

}  // namespace webrtc

#endif  // SRC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_DEVICE_OPENSLES_ANDROID_H_
