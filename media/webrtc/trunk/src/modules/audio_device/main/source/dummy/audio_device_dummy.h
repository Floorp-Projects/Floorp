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
    AudioDeviceDummy(const WebRtc_Word32 id) {}
    ~AudioDeviceDummy() {}

    // Retrieve the currently utilized audio layer
    virtual WebRtc_Word32 ActiveAudioLayer(
        AudioDeviceModule::AudioLayer& audioLayer) const { return -1; }

    // Main initializaton and termination
    virtual WebRtc_Word32 Init() { return 0; }
    virtual WebRtc_Word32 Terminate() { return 0; }
    virtual bool Initialized() const { return true; }

    // Device enumeration
    virtual WebRtc_Word16 PlayoutDevices() { return -1; }
    virtual WebRtc_Word16 RecordingDevices() { return -1; }
    virtual WebRtc_Word32 PlayoutDeviceName(
        WebRtc_UWord16 index,
        char name[kAdmMaxDeviceNameSize],
        char guid[kAdmMaxGuidSize]) { return -1; }
    virtual WebRtc_Word32 RecordingDeviceName(
        WebRtc_UWord16 index,
        char name[kAdmMaxDeviceNameSize],
        char guid[kAdmMaxGuidSize]) { return -1; }

    // Device selection
    virtual WebRtc_Word32 SetPlayoutDevice(WebRtc_UWord16 index) { return -1; }
    virtual WebRtc_Word32 SetPlayoutDevice(
        AudioDeviceModule::WindowsDeviceType device) { return -1; }
    virtual WebRtc_Word32 SetRecordingDevice(WebRtc_UWord16 index) {
      return -1;
    }
    virtual WebRtc_Word32 SetRecordingDevice(
        AudioDeviceModule::WindowsDeviceType device) { return -1; }

    // Audio transport initialization
    virtual WebRtc_Word32 PlayoutIsAvailable(bool& available) {
      return -1; }
    virtual WebRtc_Word32 InitPlayout() { return -1; };
    virtual bool PlayoutIsInitialized() const { return false; }
    virtual WebRtc_Word32 RecordingIsAvailable(bool& available) { return -1; }
    virtual WebRtc_Word32 InitRecording() { return -1; }
    virtual bool RecordingIsInitialized() const { return false; }

    // Audio transport control
    virtual WebRtc_Word32 StartPlayout() { return -1; }
    virtual WebRtc_Word32 StopPlayout() { return -1; }
    virtual bool Playing() const { return false; }
    virtual WebRtc_Word32 StartRecording() { return -1; }
    virtual WebRtc_Word32 StopRecording() { return -1; }
    virtual bool Recording() const { return false; }

    // Microphone Automatic Gain Control (AGC)
    virtual WebRtc_Word32 SetAGC(bool enable) { return -1; }
    virtual bool AGC() const { return false; }

    // Volume control based on the Windows Wave API (Windows only)
    virtual WebRtc_Word32 SetWaveOutVolume(
        WebRtc_UWord16 volumeLeft, WebRtc_UWord16 volumeRight) { return -1; }
    virtual WebRtc_Word32 WaveOutVolume(
        WebRtc_UWord16& volumeLeft,
        WebRtc_UWord16& volumeRight) const { return -1; }

    // Audio mixer initialization
    virtual WebRtc_Word32 SpeakerIsAvailable(bool& available) { return -1; }
    virtual WebRtc_Word32 InitSpeaker() { return -1; }
    virtual bool SpeakerIsInitialized() const { return false; }
    virtual WebRtc_Word32 MicrophoneIsAvailable(bool& available) { return -1; }
    virtual WebRtc_Word32 InitMicrophone() { return -1; }
    virtual bool MicrophoneIsInitialized() const { return false; }

    // Speaker volume controls
    virtual WebRtc_Word32 SpeakerVolumeIsAvailable(bool& available) {
      return -1;
    }
    virtual WebRtc_Word32 SetSpeakerVolume(WebRtc_UWord32 volume) { return -1; }
    virtual WebRtc_Word32 SpeakerVolume(WebRtc_UWord32& volume) const {
      return -1;
    }
    virtual WebRtc_Word32 MaxSpeakerVolume(WebRtc_UWord32& maxVolume) const {
      return -1;
    }
    virtual WebRtc_Word32 MinSpeakerVolume(WebRtc_UWord32& minVolume) const {
      return -1;
    }
    virtual WebRtc_Word32 SpeakerVolumeStepSize(
        WebRtc_UWord16& stepSize) const { return -1; }

    // Microphone volume controls
    virtual WebRtc_Word32 MicrophoneVolumeIsAvailable(bool& available) {
      return -1;
    }
    virtual WebRtc_Word32 SetMicrophoneVolume(WebRtc_UWord32 volume) {
      return -1;
    }
    virtual WebRtc_Word32 MicrophoneVolume(WebRtc_UWord32& volume) const {
      return -1;
    }
    virtual WebRtc_Word32 MaxMicrophoneVolume(WebRtc_UWord32& maxVolume) const {
      return -1;
    }
    virtual WebRtc_Word32 MinMicrophoneVolume(
        WebRtc_UWord32& minVolume) const { return -1; }
    virtual WebRtc_Word32 MicrophoneVolumeStepSize(
        WebRtc_UWord16& stepSize) const { return -1; }

    // Speaker mute control
    virtual WebRtc_Word32 SpeakerMuteIsAvailable(bool& available) { return -1; }
    virtual WebRtc_Word32 SetSpeakerMute(bool enable) { return -1; }
    virtual WebRtc_Word32 SpeakerMute(bool& enabled) const { return -1; }

    // Microphone mute control
    virtual WebRtc_Word32 MicrophoneMuteIsAvailable(bool& available) {
      return -1;
    }
    virtual WebRtc_Word32 SetMicrophoneMute(bool enable) { return -1; }
    virtual WebRtc_Word32 MicrophoneMute(bool& enabled) const { return -1; }

    // Microphone boost control
    virtual WebRtc_Word32 MicrophoneBoostIsAvailable(bool& available) {
      return -1;
    }
    virtual WebRtc_Word32 SetMicrophoneBoost(bool enable) { return -1; }
    virtual WebRtc_Word32 MicrophoneBoost(bool& enabled) const { return -1; }

    // Stereo support
    virtual WebRtc_Word32 StereoPlayoutIsAvailable(bool& available) {
      return -1;
    }
    virtual WebRtc_Word32 SetStereoPlayout(bool enable) { return -1; }
    virtual WebRtc_Word32 StereoPlayout(bool& enabled) const { return -1; }
    virtual WebRtc_Word32 StereoRecordingIsAvailable(bool& available) {
      return -1;
    }
    virtual WebRtc_Word32 SetStereoRecording(bool enable) { return -1; }
    virtual WebRtc_Word32 StereoRecording(bool& enabled) const { return -1; }

    // Delay information and control
    virtual WebRtc_Word32 SetPlayoutBuffer(
        const AudioDeviceModule::BufferType type,
        WebRtc_UWord16 sizeMS) { return -1; }
    virtual WebRtc_Word32 PlayoutBuffer(
        AudioDeviceModule::BufferType& type,
        WebRtc_UWord16& sizeMS) const { return -1; }
    virtual WebRtc_Word32 PlayoutDelay(WebRtc_UWord16& delayMS) const {
      return -1;
    }
    virtual WebRtc_Word32 RecordingDelay(WebRtc_UWord16& delayMS) const {
      return -1;
    }

    // CPU load
    virtual WebRtc_Word32 CPULoad(WebRtc_UWord16& load) const { return -1; }

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
