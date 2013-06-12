/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/include/audio_device.h"

namespace webrtc {

class FakeAudioDeviceModule : public AudioDeviceModule {
 public:
  FakeAudioDeviceModule() {}
  ~FakeAudioDeviceModule() {}
  virtual int32_t AddRef() { return 0; }
  virtual int32_t Release() { return 0; }
  virtual int32_t RegisterEventObserver(AudioDeviceObserver* eventCallback) {
    return 0;
  }
  virtual int32_t RegisterAudioCallback(AudioTransport* audioCallback) {
    return 0;
  }
  virtual int32_t Init() { return 0; }
  virtual int32_t SpeakerIsAvailable(bool* available) {
    *available = true;
    return 0;
  }
  virtual int32_t InitSpeaker() { return 0; }
  virtual int32_t SetPlayoutDevice(uint16_t index) { return 0; }
  virtual int32_t SetPlayoutDevice(WindowsDeviceType device) { return 0; }
  virtual int32_t SetStereoPlayout(bool enable) { return 0; }
  virtual int32_t StopPlayout() { return 0; }
  virtual int32_t MicrophoneIsAvailable(bool* available) {
    *available = true;
    return 0;
  }
  virtual int32_t InitMicrophone() { return 0; }
  virtual int32_t SetRecordingDevice(uint16_t index) { return 0; }
  virtual int32_t SetRecordingDevice(WindowsDeviceType device) { return 0; }
  virtual int32_t SetStereoRecording(bool enable) { return 0; }
  virtual int32_t SetAGC(bool enable) { return 0; }
  virtual int32_t StopRecording() { return 0; }
  virtual int32_t TimeUntilNextProcess() { return 0; }
  virtual int32_t Process() { return 0; }
  virtual int32_t Terminate() { return 0; }

  virtual int32_t ActiveAudioLayer(AudioLayer* audioLayer) const {
    assert(false);
    return 0;
  }
  virtual ErrorCode LastError() const {
    assert(false);
    return  kAdmErrNone;
  }
  virtual bool Initialized() const {
    assert(false);
    return true;
  }
  virtual int16_t PlayoutDevices() {
    assert(false);
    return 0;
  }
  virtual int16_t RecordingDevices() {
    assert(false);
    return 0;
  }
  virtual int32_t PlayoutDeviceName(uint16_t index,
                            char name[kAdmMaxDeviceNameSize],
                            char guid[kAdmMaxGuidSize]) {
    assert(false);
    return 0;
  }
  virtual int32_t RecordingDeviceName(uint16_t index,
                              char name[kAdmMaxDeviceNameSize],
                              char guid[kAdmMaxGuidSize]) {
    assert(false);
    return 0;
  }
  virtual int32_t PlayoutIsAvailable(bool* available) {
    assert(false);
    return 0;
  }
  virtual int32_t InitPlayout() {
    assert(false);
    return 0;
  }
  virtual bool PlayoutIsInitialized() const {
    assert(false);
    return true;
  }
  virtual int32_t RecordingIsAvailable(bool* available) {
    assert(false);
    return 0;
  }
  virtual int32_t InitRecording() {
    assert(false);
    return 0;
  }
  virtual bool RecordingIsInitialized() const {
    assert(false);
    return true;
  }
  virtual int32_t StartPlayout() {
    assert(false);
    return 0;
  }
  virtual bool Playing() const {
    assert(false);
    return false;
  }
  virtual int32_t StartRecording() {
    assert(false);
    return 0;
  }
  virtual bool Recording() const {
    assert(false);
    return false;
  }
  virtual bool AGC() const {
    assert(false);
    return true;
  }
  virtual int32_t SetWaveOutVolume(uint16_t volumeLeft,
                           uint16_t volumeRight) {
    assert(false);
    return 0;
  }
  virtual int32_t WaveOutVolume(uint16_t* volumeLeft,
                        uint16_t* volumeRight) const {
    assert(false);
    return 0;
  }
  virtual bool SpeakerIsInitialized() const {
    assert(false);
    return true;
  }
  virtual bool MicrophoneIsInitialized() const {
    assert(false);
    return true;
  }
  virtual int32_t SpeakerVolumeIsAvailable(bool* available) {
    assert(false);
    return 0;
  }
  virtual int32_t SetSpeakerVolume(uint32_t volume) {
    assert(false);
    return 0;
  }
  virtual int32_t SpeakerVolume(uint32_t* volume) const {
    assert(false);
    return 0;
  }
  virtual int32_t MaxSpeakerVolume(uint32_t* maxVolume) const {
    assert(false);
    return 0;
  }
  virtual int32_t MinSpeakerVolume(uint32_t* minVolume) const {
    assert(false);
    return 0;
  }
  virtual int32_t SpeakerVolumeStepSize(uint16_t* stepSize) const {
    assert(false);
    return 0;
  }
  virtual int32_t MicrophoneVolumeIsAvailable(bool* available) {
    assert(false);
    return 0;
  }
  virtual int32_t SetMicrophoneVolume(uint32_t volume) {
    assert(false);
    return 0;
  }
  virtual int32_t MicrophoneVolume(uint32_t* volume) const {
    assert(false);
    return 0;
  }
  virtual int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const {
    assert(false);
    return 0;
  }
  virtual int32_t MinMicrophoneVolume(uint32_t* minVolume) const {
    assert(false);
    return 0;
  }
  virtual int32_t MicrophoneVolumeStepSize(uint16_t* stepSize) const {
    assert(false);
    return 0;
  }
  virtual int32_t SpeakerMuteIsAvailable(bool* available) {
    assert(false);
    return 0;
  }
  virtual int32_t SetSpeakerMute(bool enable) {
    assert(false);
    return 0;
  }
  virtual int32_t SpeakerMute(bool* enabled) const {
    assert(false);
    return 0;
  }
  virtual int32_t MicrophoneMuteIsAvailable(bool* available) {
    assert(false);
    return 0;
  }
  virtual int32_t SetMicrophoneMute(bool enable) {
    assert(false);
    return 0;
  }
  virtual int32_t MicrophoneMute(bool* enabled) const {
    assert(false);
    return 0;
  }
  virtual int32_t MicrophoneBoostIsAvailable(bool* available) {
    assert(false);
    return 0;
  }
  virtual int32_t SetMicrophoneBoost(bool enable) {
    assert(false);
    return 0;
  }
  virtual int32_t MicrophoneBoost(bool* enabled) const {
    assert(false);
    return 0;
  }
  virtual int32_t StereoPlayoutIsAvailable(bool* available) const {
    *available = false;
    return 0;
  }
  virtual int32_t StereoPlayout(bool* enabled) const {
    assert(false);
    return 0;
  }
  virtual int32_t StereoRecordingIsAvailable(bool* available) const {
    *available = false;
    return 0;
  }
  virtual int32_t StereoRecording(bool* enabled) const {
    assert(false);
    return 0;
  }
  virtual int32_t SetRecordingChannel(const ChannelType channel) {
    assert(false);
    return 0;
  }
  virtual int32_t RecordingChannel(ChannelType* channel) const {
    assert(false);
    return 0;
  }
  virtual int32_t SetPlayoutBuffer(const BufferType type,
                           uint16_t sizeMS = 0) {
    assert(false);
    return 0;
  }
  virtual int32_t PlayoutBuffer(BufferType* type, uint16_t* sizeMS) const {
    assert(false);
    return 0;
  }
  virtual int32_t PlayoutDelay(uint16_t* delayMS) const {
    assert(false);
    return 0;
  }
  virtual int32_t RecordingDelay(uint16_t* delayMS) const {
    assert(false);
    return 0;
  }
  virtual int32_t CPULoad(uint16_t* load) const {
    assert(false);
    return 0;
  }
  virtual int32_t StartRawOutputFileRecording(
      const char pcmFileNameUTF8[kAdmMaxFileNameSize]) {
    assert(false);
    return 0;
  }
  virtual int32_t StopRawOutputFileRecording() {
    assert(false);
    return 0;
  }
  virtual int32_t StartRawInputFileRecording(
      const char pcmFileNameUTF8[kAdmMaxFileNameSize]) {
    assert(false);
    return 0;
  }
  virtual int32_t StopRawInputFileRecording() {
    assert(false);
    return 0;
  }
  virtual int32_t SetRecordingSampleRate(const uint32_t samplesPerSec) {
    assert(false);
    return 0;
  }
  virtual int32_t RecordingSampleRate(uint32_t* samplesPerSec) const {
    assert(false);
    return 0;
  }
  virtual int32_t SetPlayoutSampleRate(const uint32_t samplesPerSec) {
    assert(false);
    return 0;
  }
  virtual int32_t PlayoutSampleRate(uint32_t* samplesPerSec) const {
    assert(false);
    return 0;
  }
  virtual int32_t ResetAudioDevice() {
    assert(false);
    return 0;
  }
  virtual int32_t SetLoudspeakerStatus(bool enable) {
    assert(false);
    return 0;
  }
  virtual int32_t GetLoudspeakerStatus(bool* enabled) const {
    assert(false);
    return 0;
  }
  virtual int32_t EnableBuiltInAEC(bool enable) {
    assert(false);
    return -1;
  }
  virtual bool BuiltInAECIsEnabled() const {
    assert(false);
    return false;
  }
};

}  // namespace webrtc
