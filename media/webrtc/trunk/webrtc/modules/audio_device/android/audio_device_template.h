/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_DEVICE_TEMPLATE_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_DEVICE_TEMPLATE_H_

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_device/android/audio_manager.h"
#include "webrtc/modules/audio_device/audio_device_generic.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

// InputType/OutputType can be any class that implements the capturing/rendering
// part of the AudioDeviceGeneric API.
template <class InputType, class OutputType>
class AudioDeviceTemplate : public AudioDeviceGeneric {
 public:
  static void SetAndroidAudioDeviceObjects(void* javaVM,
                                           void* context) {
    AudioManager::SetAndroidAudioDeviceObjects(javaVM, context);
    OutputType::SetAndroidAudioDeviceObjects(javaVM, context);
    InputType::SetAndroidAudioDeviceObjects(javaVM, context);
  }

  static void ClearAndroidAudioDeviceObjects() {
    OutputType::ClearAndroidAudioDeviceObjects();
    InputType::ClearAndroidAudioDeviceObjects();
    AudioManager::ClearAndroidAudioDeviceObjects();
  }

  // TODO(henrika): remove id.
  explicit AudioDeviceTemplate(const int32_t id)
      : audio_manager_(),
        output_(&audio_manager_),
        input_(&output_, &audio_manager_) {
  }

  virtual ~AudioDeviceTemplate() {
  }

  int32_t ActiveAudioLayer(
      AudioDeviceModule::AudioLayer& audioLayer) const override {
    audioLayer = AudioDeviceModule::kPlatformDefaultAudio;
    return 0;
  };

  int32_t Init() override {
    return audio_manager_.Init() | output_.Init() | input_.Init();
  }

  int32_t Terminate() override {
    return output_.Terminate() | input_.Terminate() | audio_manager_.Close();
  }

  bool Initialized() const override {
    return true;
  }

  int16_t PlayoutDevices() override {
    return 1;
  }

  int16_t RecordingDevices() override {
    return 1;
  }

  int32_t PlayoutDeviceName(
      uint16_t index,
      char name[kAdmMaxDeviceNameSize],
      char guid[kAdmMaxGuidSize]) override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t RecordingDeviceName(
      uint16_t index,
      char name[kAdmMaxDeviceNameSize],
      char guid[kAdmMaxGuidSize]) override {
    return input_.RecordingDeviceName(index, name, guid);
  }

  int32_t SetPlayoutDevice(uint16_t index) override {
    // OK to use but it has no effect currently since device selection is
    // done using Andoid APIs instead.
    return 0;
  }

  int32_t SetPlayoutDevice(
      AudioDeviceModule::WindowsDeviceType device) override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SetRecordingDevice(uint16_t index) override {
    // OK to use but it has no effect currently since device selection is
    // done using Andoid APIs instead.
    return 0;
  }

  int32_t SetRecordingDevice(
      AudioDeviceModule::WindowsDeviceType device) override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t PlayoutIsAvailable(bool& available) override {
    available = true;
    return 0;
  }

  int32_t InitPlayout() override {
    return output_.InitPlayout();
  }

  bool PlayoutIsInitialized() const override {
    return output_.PlayoutIsInitialized();
  }

  int32_t RecordingIsAvailable(bool& available) override {
    available = true;
    return 0;
  }

  int32_t InitRecording() override {
    return input_.InitRecording();
  }

  bool RecordingIsInitialized() const override {
    return input_.RecordingIsInitialized();
  }

  int32_t StartPlayout() override {
    return output_.StartPlayout();
  }

  int32_t StopPlayout() override {
    return output_.StopPlayout();
  }

  bool Playing() const override {
    return output_.Playing();
  }

  int32_t StartRecording() override {
    return input_.StartRecording();
  }

  int32_t StopRecording() override {
    return input_.StopRecording();
  }

  bool Recording() const override {
    return input_.Recording() ;
  }

  int32_t SetAGC(bool enable) override {
    if (enable) {
      FATAL() << "Should never be called";
    }
    return -1;
  }

  bool AGC() const override {
    return false;
  }

  int32_t SetWaveOutVolume(
      uint16_t volumeLeft, uint16_t volumeRight) override {
     FATAL() << "Should never be called";
    return -1;
  }

  int32_t WaveOutVolume(
      uint16_t& volumeLeft, uint16_t& volumeRight) const override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t InitSpeaker() override {
    return 0;
  }

  bool SpeakerIsInitialized() const override {
    return true;
  }

  int32_t InitMicrophone() override {
    return 0;
  }

  bool MicrophoneIsInitialized() const override {
    return true;
  }

  int32_t SpeakerVolumeIsAvailable(bool& available) override {
    return output_.SpeakerVolumeIsAvailable(available);
  }

  int32_t SetSpeakerVolume(uint32_t volume) override {
    return output_.SetSpeakerVolume(volume);
  }

  int32_t SpeakerVolume(uint32_t& volume) const override {
    return output_.SpeakerVolume(volume);
  }

  int32_t MaxSpeakerVolume(uint32_t& maxVolume) const override {
    return output_.MaxSpeakerVolume(maxVolume);
  }

  int32_t MinSpeakerVolume(uint32_t& minVolume) const override {
    return output_.MinSpeakerVolume(minVolume);
  }

  int32_t SpeakerVolumeStepSize(uint16_t& stepSize) const override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t MicrophoneVolumeIsAvailable(bool& available) override{
    available = false;
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SetMicrophoneVolume(uint32_t volume) override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t MicrophoneVolume(uint32_t& volume) const override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t MinMicrophoneVolume(uint32_t& minVolume) const override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t MicrophoneVolumeStepSize(uint16_t& stepSize) const override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SpeakerMuteIsAvailable(bool& available) override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SetSpeakerMute(bool enable) override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SpeakerMute(bool& enabled) const override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t MicrophoneMuteIsAvailable(bool& available) override {
    FATAL() << "Not implemented";
    return -1;
  }

  int32_t SetMicrophoneMute(bool enable) override {
    FATAL() << "Not implemented";
    return -1;
  }

  int32_t MicrophoneMute(bool& enabled) const override {
    FATAL() << "Not implemented";
    return -1;
  }

  int32_t MicrophoneBoostIsAvailable(bool& available) override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SetMicrophoneBoost(bool enable) override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t MicrophoneBoost(bool& enabled) const override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t StereoPlayoutIsAvailable(bool& available) override {
    available = false;
    return 0;
  }

  // TODO(henrika): add support.
  int32_t SetStereoPlayout(bool enable) override {
    return -1;
  }

  // TODO(henrika): add support.
  int32_t StereoPlayout(bool& enabled) const override {
    enabled = false;
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t StereoRecordingIsAvailable(bool& available) override {
    available = false;
    return 0;
  }

  int32_t SetStereoRecording(bool enable) override {
    return -1;
  }

  int32_t StereoRecording(bool& enabled) const override {
    enabled = false;
    return 0;
  }

  int32_t SetPlayoutBuffer(
      const AudioDeviceModule::BufferType type, uint16_t sizeMS) override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t PlayoutBuffer(
      AudioDeviceModule::BufferType& type, uint16_t& sizeMS) const override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t PlayoutDelay(uint16_t& delayMS) const override {
    return output_.PlayoutDelay(delayMS);
  }

  int32_t RecordingDelay(uint16_t& delayMS) const override {
    return input_.RecordingDelay(delayMS);
  }

  int32_t CPULoad(uint16_t& load) const override {
    FATAL() << "Should never be called";
    return -1;
  }

  bool PlayoutWarning() const override {
    return false;
  }

  bool PlayoutError() const override {
    return false;
  }

  bool RecordingWarning() const override {
    return false;
  }

  bool RecordingError() const override {
    return false;
  }

  void ClearPlayoutWarning() override {}

  void ClearPlayoutError() override {}

  void ClearRecordingWarning() override {}

  void ClearRecordingError() override {}

  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) override {
    output_.AttachAudioBuffer(audioBuffer);
    input_.AttachAudioBuffer(audioBuffer);
  }

  // TODO(henrika): remove
  int32_t SetPlayoutSampleRate(const uint32_t samplesPerSec) override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t SetLoudspeakerStatus(bool enable) override {
    FATAL() << "Should never be called";
    return -1;
  }

  int32_t GetLoudspeakerStatus(bool& enable) const override {
    FATAL() << "Should never be called";
    return -1;
  }

  bool BuiltInAECIsAvailable() const override {
    return input_.BuiltInAECIsAvailable();
  }

  int32_t EnableBuiltInAEC(bool enable) override {
    return input_.EnableBuiltInAEC(enable);
  }

 private:
  AudioManager audio_manager_;
  OutputType output_;
  InputType input_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_DEVICE_TEMPLATE_H_
