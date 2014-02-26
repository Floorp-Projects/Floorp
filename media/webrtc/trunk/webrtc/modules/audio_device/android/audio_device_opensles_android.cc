/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/android/audio_device_opensles_android.h"

#include "webrtc/modules/audio_device/android/opensles_input.h"
#include "webrtc/modules/audio_device/android/opensles_output.h"

namespace webrtc {

AudioDeviceAndroidOpenSLES::AudioDeviceAndroidOpenSLES(const int32_t id)
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
    : output_(id),
      input_(id, &output_)
#else
    : input_(id, 0)
#endif
{
}

AudioDeviceAndroidOpenSLES::~AudioDeviceAndroidOpenSLES() {
}

int32_t AudioDeviceAndroidOpenSLES::ActiveAudioLayer(
    AudioDeviceModule::AudioLayer& audioLayer) const { // NOLINT
  return 0;
}

int32_t AudioDeviceAndroidOpenSLES::Init() {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.Init() | input_.Init();
#else
  return input_.Init();
#endif
}

int32_t AudioDeviceAndroidOpenSLES::Terminate()  {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.Terminate() | input_.Terminate();
#else
  return input_.Terminate();
#endif
}

bool AudioDeviceAndroidOpenSLES::Initialized() const {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.Initialized() && input_.Initialized();
#else
  return input_.Initialized();
#endif
}

int16_t AudioDeviceAndroidOpenSLES::PlayoutDevices() {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.PlayoutDevices();
#else
  return 0;
#endif
}

int16_t AudioDeviceAndroidOpenSLES::RecordingDevices() {
  return input_.RecordingDevices();
}

int32_t AudioDeviceAndroidOpenSLES::PlayoutDeviceName(
    uint16_t index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize]) {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.PlayoutDeviceName(index, name, guid);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::RecordingDeviceName(
    uint16_t index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize]) {
  return input_.RecordingDeviceName(index, name, guid);
}

int32_t AudioDeviceAndroidOpenSLES::SetPlayoutDevice(uint16_t index) {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.SetPlayoutDevice(index);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::SetPlayoutDevice(
    AudioDeviceModule::WindowsDeviceType device) {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.SetPlayoutDevice(device);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::SetRecordingDevice(uint16_t index) {
  return input_.SetRecordingDevice(index);
}

int32_t AudioDeviceAndroidOpenSLES::SetRecordingDevice(
    AudioDeviceModule::WindowsDeviceType device) {
  return input_.SetRecordingDevice(device);
}

int32_t AudioDeviceAndroidOpenSLES::PlayoutIsAvailable(
    bool& available) {  // NOLINT
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.PlayoutIsAvailable(available);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::InitPlayout() {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.InitPlayout();
#else
  return -1;
#endif
}

bool AudioDeviceAndroidOpenSLES::PlayoutIsInitialized() const {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.PlayoutIsInitialized();
#else
  return false;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::RecordingIsAvailable(
    bool& available) {  // NOLINT
  return input_.RecordingIsAvailable(available);
}

int32_t AudioDeviceAndroidOpenSLES::InitRecording() {
  return input_.InitRecording();
}

bool AudioDeviceAndroidOpenSLES::RecordingIsInitialized() const {
  return input_.RecordingIsInitialized();
}

int32_t AudioDeviceAndroidOpenSLES::StartPlayout() {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.StartPlayout();
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::StopPlayout() {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.StopPlayout();
#else
  return -1;
#endif
}

bool AudioDeviceAndroidOpenSLES::Playing() const {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.Playing();
#else
  return false;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::StartRecording() {
  return input_.StartRecording();
}

int32_t AudioDeviceAndroidOpenSLES::StopRecording() {
  return input_.StopRecording();
}

bool AudioDeviceAndroidOpenSLES::Recording() const {
  return input_.Recording() ;
}

int32_t AudioDeviceAndroidOpenSLES::SetAGC(bool enable) {
  return input_.SetAGC(enable);
}

bool AudioDeviceAndroidOpenSLES::AGC() const {
  return input_.AGC();
}

int32_t AudioDeviceAndroidOpenSLES::SetWaveOutVolume(uint16_t volumeLeft,
                                                     uint16_t volumeRight) {
  return -1;
}

int32_t AudioDeviceAndroidOpenSLES::WaveOutVolume(
    uint16_t& volumeLeft,           // NOLINT
    uint16_t& volumeRight) const {  // NOLINT
  return -1;
}

int32_t AudioDeviceAndroidOpenSLES::SpeakerIsAvailable(
    bool& available) {  // NOLINT
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.SpeakerIsAvailable(available);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::InitSpeaker() {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.InitSpeaker();
#else
  return -1;
#endif
}

bool AudioDeviceAndroidOpenSLES::SpeakerIsInitialized() const {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.SpeakerIsInitialized();
#else
  return false;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::MicrophoneIsAvailable(
    bool& available) {  // NOLINT
  return input_.MicrophoneIsAvailable(available);
}

int32_t AudioDeviceAndroidOpenSLES::InitMicrophone() {
  return input_.InitMicrophone();
}

bool AudioDeviceAndroidOpenSLES::MicrophoneIsInitialized() const {
  return input_.MicrophoneIsInitialized();
}

int32_t AudioDeviceAndroidOpenSLES::SpeakerVolumeIsAvailable(
    bool& available) {  // NOLINT
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.SpeakerVolumeIsAvailable(available);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::SetSpeakerVolume(uint32_t volume) {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.SetSpeakerVolume(volume);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::SpeakerVolume(
    uint32_t& volume) const {  // NOLINT
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.SpeakerVolume(volume);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::MaxSpeakerVolume(
    uint32_t& maxVolume) const {  // NOLINT
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.MaxSpeakerVolume(maxVolume);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::MinSpeakerVolume(
    uint32_t& minVolume) const {  // NOLINT
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.MinSpeakerVolume(minVolume);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::SpeakerVolumeStepSize(
    uint16_t& stepSize) const {  // NOLINT
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.SpeakerVolumeStepSize(stepSize);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::MicrophoneVolumeIsAvailable(
    bool& available) {  // NOLINT
  return input_.MicrophoneVolumeIsAvailable(available);
}

int32_t AudioDeviceAndroidOpenSLES::SetMicrophoneVolume(uint32_t volume) {
  return input_.SetMicrophoneVolume(volume);
}

int32_t AudioDeviceAndroidOpenSLES::MicrophoneVolume(
    uint32_t& volume) const {  // NOLINT
  return input_.MicrophoneVolume(volume);
}

int32_t AudioDeviceAndroidOpenSLES::MaxMicrophoneVolume(
    uint32_t& maxVolume) const {  // NOLINT
  return input_.MaxMicrophoneVolume(maxVolume);
}

int32_t AudioDeviceAndroidOpenSLES::MinMicrophoneVolume(
    uint32_t& minVolume) const {  // NOLINT
  return input_.MinMicrophoneVolume(minVolume);
}

int32_t AudioDeviceAndroidOpenSLES::MicrophoneVolumeStepSize(
    uint16_t& stepSize) const {  // NOLINT
  return input_.MicrophoneVolumeStepSize(stepSize);
}

int32_t AudioDeviceAndroidOpenSLES::SpeakerMuteIsAvailable(
    bool& available) {  // NOLINT
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.SpeakerMuteIsAvailable(available);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::SetSpeakerMute(bool enable) {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.SetSpeakerMute(enable);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::SpeakerMute(
    bool& enabled) const {  // NOLINT
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.SpeakerMute(enabled);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::MicrophoneMuteIsAvailable(
    bool& available) {  // NOLINT
  return input_.MicrophoneMuteIsAvailable(available);
}

int32_t AudioDeviceAndroidOpenSLES::SetMicrophoneMute(bool enable) {
  return input_.SetMicrophoneMute(enable);
}

int32_t AudioDeviceAndroidOpenSLES::MicrophoneMute(
    bool& enabled) const {  // NOLINT
  return input_.MicrophoneMute(enabled);
}

int32_t AudioDeviceAndroidOpenSLES::MicrophoneBoostIsAvailable(
    bool& available) {  // NOLINT
  return input_.MicrophoneBoostIsAvailable(available);
}

int32_t AudioDeviceAndroidOpenSLES::SetMicrophoneBoost(bool enable) {
  return input_.SetMicrophoneBoost(enable);
}

int32_t AudioDeviceAndroidOpenSLES::MicrophoneBoost(
    bool& enabled) const {  // NOLINT
  return input_.MicrophoneBoost(enabled);
}

int32_t AudioDeviceAndroidOpenSLES::StereoPlayoutIsAvailable(
    bool& available) {  // NOLINT
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.StereoPlayoutIsAvailable(available);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::SetStereoPlayout(bool enable) {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.SetStereoPlayout(enable);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::StereoPlayout(
    bool& enabled) const {  // NOLINT
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.StereoPlayout(enabled);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::StereoRecordingIsAvailable(
    bool& available) {  // NOLINT
  return input_.StereoRecordingIsAvailable(available);
}

int32_t AudioDeviceAndroidOpenSLES::SetStereoRecording(bool enable) {
  return input_.SetStereoRecording(enable);
}

int32_t AudioDeviceAndroidOpenSLES::StereoRecording(
    bool& enabled) const {  // NOLINT
  return input_.StereoRecording(enabled);
}

int32_t AudioDeviceAndroidOpenSLES::SetPlayoutBuffer(
    const AudioDeviceModule::BufferType type,
    uint16_t sizeMS) {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.SetPlayoutBuffer(type, sizeMS);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::PlayoutBuffer(
    AudioDeviceModule::BufferType& type,
    uint16_t& sizeMS) const {  // NOLINT
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.PlayoutBuffer(type, sizeMS);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::PlayoutDelay(
    uint16_t& delayMS) const {  // NOLINT
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.PlayoutDelay(delayMS);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::RecordingDelay(
    uint16_t& delayMS) const {  // NOLINT
  return input_.RecordingDelay(delayMS);
}

int32_t AudioDeviceAndroidOpenSLES::CPULoad(
    uint16_t& load) const {  // NOLINT
  return -1;
}

bool AudioDeviceAndroidOpenSLES::PlayoutWarning() const {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.PlayoutWarning();
#else
  return false;
#endif
}

bool AudioDeviceAndroidOpenSLES::PlayoutError() const {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.PlayoutError();
#else
  return false;
#endif
}

bool AudioDeviceAndroidOpenSLES::RecordingWarning() const {
  return input_.RecordingWarning();
}

bool AudioDeviceAndroidOpenSLES::RecordingError() const {
  return input_.RecordingError();
}

void AudioDeviceAndroidOpenSLES::ClearPlayoutWarning() {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.ClearPlayoutWarning();
#else
  return;
#endif
}

void AudioDeviceAndroidOpenSLES::ClearPlayoutError() {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.ClearPlayoutError();
#else
  return;
#endif
}

void AudioDeviceAndroidOpenSLES::ClearRecordingWarning() {
  return input_.ClearRecordingWarning();
}

void AudioDeviceAndroidOpenSLES::ClearRecordingError() {
  return input_.ClearRecordingError();
}

void AudioDeviceAndroidOpenSLES::AttachAudioBuffer(
    AudioDeviceBuffer* audioBuffer) {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  output_.AttachAudioBuffer(audioBuffer);
#endif
  input_.AttachAudioBuffer(audioBuffer);
}

int32_t AudioDeviceAndroidOpenSLES::SetLoudspeakerStatus(bool enable) {
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.SetLoudspeakerStatus(enable);
#else
  return -1;
#endif
}

int32_t AudioDeviceAndroidOpenSLES::GetLoudspeakerStatus(
    bool& enable) const {  // NOLINT
#ifdef WEBRTC_ANDROID_OPENSLES_OUTPUT
  return output_.GetLoudspeakerStatus(enable);
#else
  return -1;
#endif
}

}  // namespace webrtc
