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

#include "webrtc/modules/audio_device/audio_device_generic.h"

#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

// InputType/OutputType can be any class that implements the capturing/rendering
// part of the AudioDeviceGeneric API.
template <class InputType, class OutputType>
class AudioDeviceTemplate : public AudioDeviceGeneric {
 public:
  static int32_t SetAndroidAudioDeviceObjects(void* javaVM,
                                       void* env,
                                       void* context) {
    if (OutputType::SetAndroidAudioDeviceObjects(javaVM, env, context) == -1) {
      return -1;
    }
    return InputType::SetAndroidAudioDeviceObjects(javaVM, env, context);
  }

  static void ClearAndroidAudioDeviceObjects() {
    OutputType::ClearAndroidAudioDeviceObjects();
    InputType::ClearAndroidAudioDeviceObjects();
  }

  explicit AudioDeviceTemplate(const int32_t id)
      : output_(id),
        input_(id, &output_) {
  }

  virtual ~AudioDeviceTemplate() {
  }

  int32_t ActiveAudioLayer(
      AudioDeviceModule::AudioLayer& audioLayer) const { // NOLINT
    audioLayer = AudioDeviceModule::kPlatformDefaultAudio;
    return 0;
  }

  int32_t Init() {
    return output_.Init() | input_.Init();
  }

  int32_t Terminate()  {
    return output_.Terminate() | input_.Terminate();
  }

  bool Initialized() const {
    return output_.Initialized() && input_.Initialized();
  }

  int16_t PlayoutDevices() {
    return output_.PlayoutDevices();
  }

  int16_t RecordingDevices() {
    return input_.RecordingDevices();
  }

  int32_t PlayoutDeviceName(
      uint16_t index,
      char name[kAdmMaxDeviceNameSize],
      char guid[kAdmMaxGuidSize]) {
    return output_.PlayoutDeviceName(index, name, guid);
  }

  int32_t RecordingDeviceName(
      uint16_t index,
      char name[kAdmMaxDeviceNameSize],
      char guid[kAdmMaxGuidSize]) {
    return input_.RecordingDeviceName(index, name, guid);
  }

  int32_t SetPlayoutDevice(uint16_t index) {
    return output_.SetPlayoutDevice(index);
  }

  int32_t SetPlayoutDevice(
      AudioDeviceModule::WindowsDeviceType device) {
    return output_.SetPlayoutDevice(device);
  }

  int32_t SetRecordingDevice(uint16_t index) {
    return input_.SetRecordingDevice(index);
  }

  int32_t SetRecordingDevice(
      AudioDeviceModule::WindowsDeviceType device) {
    return input_.SetRecordingDevice(device);
  }

  int32_t PlayoutIsAvailable(
      bool& available) {  // NOLINT
    return output_.PlayoutIsAvailable(available);
  }

  int32_t InitPlayout() {
    return output_.InitPlayout();
  }

  bool PlayoutIsInitialized() const {
    return output_.PlayoutIsInitialized();
  }

  int32_t RecordingIsAvailable(
      bool& available) {  // NOLINT
    return input_.RecordingIsAvailable(available);
  }

  int32_t InitRecording() {
    return input_.InitRecording();
  }

  bool RecordingIsInitialized() const {
    return input_.RecordingIsInitialized();
  }

  int32_t StartPlayout() {
    return output_.StartPlayout();
  }

  int32_t StopPlayout() {
    return output_.StopPlayout();
  }

  bool Playing() const {
    return output_.Playing();
  }

  int32_t StartRecording() {
    return input_.StartRecording();
  }

  int32_t StopRecording() {
    return input_.StopRecording();
  }

  bool Recording() const {
    return input_.Recording() ;
  }

  int32_t SetAGC(bool enable) {
    return input_.SetAGC(enable);
  }

  bool AGC() const {
    return input_.AGC();
  }

  int32_t SetWaveOutVolume(uint16_t volumeLeft,
                           uint16_t volumeRight) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, 0,
                 "  API call not supported on this platform");
    return -1;
  }

  int32_t WaveOutVolume(
      uint16_t& volumeLeft,           // NOLINT
      uint16_t& volumeRight) const {  // NOLINT
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, 0,
                 "  API call not supported on this platform");
    return -1;
  }

  int32_t SpeakerIsAvailable(
      bool& available) {  // NOLINT
    return output_.SpeakerIsAvailable(available);
  }

  int32_t InitSpeaker() {
    return output_.InitSpeaker();
  }

  bool SpeakerIsInitialized() const {
    return output_.SpeakerIsInitialized();
  }

  int32_t MicrophoneIsAvailable(
      bool& available) {  // NOLINT
    return input_.MicrophoneIsAvailable(available);
  }

  int32_t InitMicrophone() {
    return input_.InitMicrophone();
  }

  bool MicrophoneIsInitialized() const {
    return input_.MicrophoneIsInitialized();
  }

  int32_t SpeakerVolumeIsAvailable(
      bool& available) {  // NOLINT
    return output_.SpeakerVolumeIsAvailable(available);
  }

  int32_t SetSpeakerVolume(uint32_t volume) {
    return output_.SetSpeakerVolume(volume);
  }

  int32_t SpeakerVolume(
      uint32_t& volume) const {  // NOLINT
    return output_.SpeakerVolume(volume);
  }

  int32_t MaxSpeakerVolume(
      uint32_t& maxVolume) const {  // NOLINT
    return output_.MaxSpeakerVolume(maxVolume);
  }

  int32_t MinSpeakerVolume(
      uint32_t& minVolume) const {  // NOLINT
    return output_.MinSpeakerVolume(minVolume);
  }

  int32_t SpeakerVolumeStepSize(
      uint16_t& stepSize) const {  // NOLINT
    return output_.SpeakerVolumeStepSize(stepSize);
  }

  int32_t MicrophoneVolumeIsAvailable(
      bool& available) {  // NOLINT
    return input_.MicrophoneVolumeIsAvailable(available);
  }

  int32_t SetMicrophoneVolume(uint32_t volume) {
    return input_.SetMicrophoneVolume(volume);
  }

  int32_t MicrophoneVolume(
      uint32_t& volume) const {  // NOLINT
    return input_.MicrophoneVolume(volume);
  }

  int32_t MaxMicrophoneVolume(
      uint32_t& maxVolume) const {  // NOLINT
    return input_.MaxMicrophoneVolume(maxVolume);
  }

  int32_t MinMicrophoneVolume(
      uint32_t& minVolume) const {  // NOLINT
    return input_.MinMicrophoneVolume(minVolume);
  }

  int32_t MicrophoneVolumeStepSize(
      uint16_t& stepSize) const {  // NOLINT
    return input_.MicrophoneVolumeStepSize(stepSize);
  }

  int32_t SpeakerMuteIsAvailable(
      bool& available) {  // NOLINT
    return output_.SpeakerMuteIsAvailable(available);
  }

  int32_t SetSpeakerMute(bool enable) {
    return output_.SetSpeakerMute(enable);
  }

  int32_t SpeakerMute(
      bool& enabled) const {  // NOLINT
    return output_.SpeakerMute(enabled);
  }

  int32_t MicrophoneMuteIsAvailable(
      bool& available) {  // NOLINT
    return input_.MicrophoneMuteIsAvailable(available);
  }

  int32_t SetMicrophoneMute(bool enable) {
    return input_.SetMicrophoneMute(enable);
  }

  int32_t MicrophoneMute(
      bool& enabled) const {  // NOLINT
    return input_.MicrophoneMute(enabled);
  }

  int32_t MicrophoneBoostIsAvailable(
      bool& available) {  // NOLINT
    return input_.MicrophoneBoostIsAvailable(available);
  }

  int32_t SetMicrophoneBoost(bool enable) {
    return input_.SetMicrophoneBoost(enable);
  }

  int32_t MicrophoneBoost(
      bool& enabled) const {  // NOLINT
    return input_.MicrophoneBoost(enabled);
  }

  int32_t StereoPlayoutIsAvailable(
      bool& available) {  // NOLINT
    return output_.StereoPlayoutIsAvailable(available);
  }

  int32_t SetStereoPlayout(bool enable) {
    return output_.SetStereoPlayout(enable);
  }

  int32_t StereoPlayout(
      bool& enabled) const {  // NOLINT
    return output_.StereoPlayout(enabled);
  }

  int32_t StereoRecordingIsAvailable(
      bool& available) {  // NOLINT
    return input_.StereoRecordingIsAvailable(available);
  }

  int32_t SetStereoRecording(bool enable) {
    return input_.SetStereoRecording(enable);
  }

  int32_t StereoRecording(
      bool& enabled) const {  // NOLINT
    return input_.StereoRecording(enabled);
  }

  int32_t SetPlayoutBuffer(
      const AudioDeviceModule::BufferType type,
      uint16_t sizeMS) {
    return output_.SetPlayoutBuffer(type, sizeMS);
  }

  int32_t PlayoutBuffer(
      AudioDeviceModule::BufferType& type,
      uint16_t& sizeMS) const {  // NOLINT
    return output_.PlayoutBuffer(type, sizeMS);
  }

  int32_t PlayoutDelay(
      uint16_t& delayMS) const {  // NOLINT
    return output_.PlayoutDelay(delayMS);
  }

  int32_t RecordingDelay(
      uint16_t& delayMS) const {  // NOLINT
    return input_.RecordingDelay(delayMS);
  }

  int32_t CPULoad(
      uint16_t& load) const {  // NOLINT
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, 0,
                 "  API call not supported on this platform");
    return -1;
  }

  bool PlayoutWarning() const {
    return output_.PlayoutWarning();
  }

  bool PlayoutError() const {
    return output_.PlayoutError();
  }

  bool RecordingWarning() const {
    return input_.RecordingWarning();
  }

  bool RecordingError() const {
    return input_.RecordingError();
  }

  void ClearPlayoutWarning() {
    return output_.ClearPlayoutWarning();
  }

  void ClearPlayoutError() {
    return output_.ClearPlayoutError();
  }

  void ClearRecordingWarning() {
    return input_.ClearRecordingWarning();
  }

  void ClearRecordingError() {
    return input_.ClearRecordingError();
  }

  void AttachAudioBuffer(
      AudioDeviceBuffer* audioBuffer) {
    output_.AttachAudioBuffer(audioBuffer);
    input_.AttachAudioBuffer(audioBuffer);
  }

  int32_t SetRecordingSampleRate(
      const uint32_t samplesPerSec) {
    return input_.SetRecordingSampleRate(samplesPerSec);
  }

  int32_t SetPlayoutSampleRate(
      const uint32_t samplesPerSec) {
    return output_.SetPlayoutSampleRate(samplesPerSec);
  }

  int32_t SetLoudspeakerStatus(bool enable) {
    return output_.SetLoudspeakerStatus(enable);
  }

  int32_t GetLoudspeakerStatus(
      bool& enable) const {  // NOLINT
    return output_.GetLoudspeakerStatus(enable);
  }

 private:
  OutputType output_;
  InputType input_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_DEVICE_TEMPLATE_H_
