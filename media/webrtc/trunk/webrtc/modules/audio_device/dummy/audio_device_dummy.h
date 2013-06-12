/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_DUMMY_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_DUMMY_H

#include <stdio.h>

#include "audio_device_generic.h"

namespace webrtc {

class AudioDeviceDummy : public AudioDeviceGeneric
{
public:
    AudioDeviceDummy(const int32_t id) {}
    ~AudioDeviceDummy() {}

    // Retrieve the currently utilized audio layer
    virtual int32_t ActiveAudioLayer(
        AudioDeviceModule::AudioLayer& audioLayer) const { return -1; }

    // Main initializaton and termination
    virtual int32_t Init() { return 0; }
    virtual int32_t Terminate() { return 0; }
    virtual bool Initialized() const { return true; }

    // Device enumeration
    virtual int16_t PlayoutDevices() { return -1; }
    virtual int16_t RecordingDevices() { return -1; }
    virtual int32_t PlayoutDeviceName(
        uint16_t index,
        char name[kAdmMaxDeviceNameSize],
        char guid[kAdmMaxGuidSize]) { return -1; }
    virtual int32_t RecordingDeviceName(
        uint16_t index,
        char name[kAdmMaxDeviceNameSize],
        char guid[kAdmMaxGuidSize]) { return -1; }

    // Device selection
    virtual int32_t SetPlayoutDevice(uint16_t index) { return -1; }
    virtual int32_t SetPlayoutDevice(
        AudioDeviceModule::WindowsDeviceType device) { return -1; }
    virtual int32_t SetRecordingDevice(uint16_t index) {
      return -1;
    }
    virtual int32_t SetRecordingDevice(
        AudioDeviceModule::WindowsDeviceType device) { return -1; }

    // Audio transport initialization
    virtual int32_t PlayoutIsAvailable(bool& available) {
      return -1; }
    virtual int32_t InitPlayout() { return -1; };
    virtual bool PlayoutIsInitialized() const { return false; }
    virtual int32_t RecordingIsAvailable(bool& available) { return -1; }
    virtual int32_t InitRecording() { return -1; }
    virtual bool RecordingIsInitialized() const { return false; }

    // Audio transport control
    virtual int32_t StartPlayout() { return -1; }
    virtual int32_t StopPlayout() { return -1; }
    virtual bool Playing() const { return false; }
    virtual int32_t StartRecording() { return -1; }
    virtual int32_t StopRecording() { return -1; }
    virtual bool Recording() const { return false; }

    // Microphone Automatic Gain Control (AGC)
    virtual int32_t SetAGC(bool enable) { return -1; }
    virtual bool AGC() const { return false; }

    // Volume control based on the Windows Wave API (Windows only)
    virtual int32_t SetWaveOutVolume(
        uint16_t volumeLeft, uint16_t volumeRight) { return -1; }
    virtual int32_t WaveOutVolume(
        uint16_t& volumeLeft,
        uint16_t& volumeRight) const { return -1; }

    // Audio mixer initialization
    virtual int32_t SpeakerIsAvailable(bool& available) { return -1; }
    virtual int32_t InitSpeaker() { return -1; }
    virtual bool SpeakerIsInitialized() const { return false; }
    virtual int32_t MicrophoneIsAvailable(bool& available) { return -1; }
    virtual int32_t InitMicrophone() { return -1; }
    virtual bool MicrophoneIsInitialized() const { return false; }

    // Speaker volume controls
    virtual int32_t SpeakerVolumeIsAvailable(bool& available) {
      return -1;
    }
    virtual int32_t SetSpeakerVolume(uint32_t volume) { return -1; }
    virtual int32_t SpeakerVolume(uint32_t& volume) const {
      return -1;
    }
    virtual int32_t MaxSpeakerVolume(uint32_t& maxVolume) const {
      return -1;
    }
    virtual int32_t MinSpeakerVolume(uint32_t& minVolume) const {
      return -1;
    }
    virtual int32_t SpeakerVolumeStepSize(
        uint16_t& stepSize) const { return -1; }

    // Microphone volume controls
    virtual int32_t MicrophoneVolumeIsAvailable(bool& available) {
      return -1;
    }
    virtual int32_t SetMicrophoneVolume(uint32_t volume) {
      return -1;
    }
    virtual int32_t MicrophoneVolume(uint32_t& volume) const {
      return -1;
    }
    virtual int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const {
      return -1;
    }
    virtual int32_t MinMicrophoneVolume(
        uint32_t& minVolume) const { return -1; }
    virtual int32_t MicrophoneVolumeStepSize(
        uint16_t& stepSize) const { return -1; }

    // Speaker mute control
    virtual int32_t SpeakerMuteIsAvailable(bool& available) { return -1; }
    virtual int32_t SetSpeakerMute(bool enable) { return -1; }
    virtual int32_t SpeakerMute(bool& enabled) const { return -1; }

    // Microphone mute control
    virtual int32_t MicrophoneMuteIsAvailable(bool& available) {
      return -1;
    }
    virtual int32_t SetMicrophoneMute(bool enable) { return -1; }
    virtual int32_t MicrophoneMute(bool& enabled) const { return -1; }

    // Microphone boost control
    virtual int32_t MicrophoneBoostIsAvailable(bool& available) {
      return -1;
    }
    virtual int32_t SetMicrophoneBoost(bool enable) { return -1; }
    virtual int32_t MicrophoneBoost(bool& enabled) const { return -1; }

    // Stereo support
    virtual int32_t StereoPlayoutIsAvailable(bool& available) {
      return -1;
    }
    virtual int32_t SetStereoPlayout(bool enable) { return -1; }
    virtual int32_t StereoPlayout(bool& enabled) const { return -1; }
    virtual int32_t StereoRecordingIsAvailable(bool& available) {
      return -1;
    }
    virtual int32_t SetStereoRecording(bool enable) { return -1; }
    virtual int32_t StereoRecording(bool& enabled) const { return -1; }

    // Delay information and control
    virtual int32_t SetPlayoutBuffer(
        const AudioDeviceModule::BufferType type,
        uint16_t sizeMS) { return -1; }
    virtual int32_t PlayoutBuffer(
        AudioDeviceModule::BufferType& type,
        uint16_t& sizeMS) const { return -1; }
    virtual int32_t PlayoutDelay(uint16_t& delayMS) const {
      return -1;
    }
    virtual int32_t RecordingDelay(uint16_t& delayMS) const {
      return -1;
    }

    // CPU load
    virtual int32_t CPULoad(uint16_t& load) const { return -1; }

    virtual bool PlayoutWarning() const { return false; }
    virtual bool PlayoutError() const { return false; }
    virtual bool RecordingWarning() const { return false; }
    virtual bool RecordingError() const { return false; }
    virtual void ClearPlayoutWarning() {}
    virtual void ClearPlayoutError() {}
    virtual void ClearRecordingWarning() {}
    virtual void ClearRecordingError() {}

    virtual void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {}
};

}  // namespace webrtc

#endif  // WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_DUMMY_H
