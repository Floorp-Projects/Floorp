/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//
//  - Audio device handling.
//  - Device information.
//  - CPU load monitoring.
//
// Usage example, omitting error checking:
//
//  using namespace webrtc;
//  VoiceEngine* voe = VoiceEngine::Create();
//  VoEBase* base = VoEBase::GetInterface(voe);
//  VoEHardware* hardware  = VoEHardware::GetInterface(voe);
//  base->Init();
//  ...
//  int n_devices = hardware->GetNumOfPlayoutDevices();
//  ...
//  base->Terminate();
//  base->Release();
//  hardware->Release();
//  VoiceEngine::Delete(voe);
//
#ifndef WEBRTC_VOICE_ENGINE_VOE_HARDWARE_H
#define WEBRTC_VOICE_ENGINE_VOE_HARDWARE_H

#include "webrtc/common_types.h"

namespace webrtc {

class VoiceEngine;

class WEBRTC_DLLEXPORT VoEHardware
{
public:
    // Factory for the VoEHardware sub-API. Increases an internal
    // reference counter if successful. Returns NULL if the API is not
    // supported or if construction fails.
    static VoEHardware* GetInterface(VoiceEngine* voiceEngine);

    // Releases the VoEHardware sub-API and decreases an internal
    // reference counter. Returns the new reference count. This value should
    // be zero for all sub-API:s before the VoiceEngine object can be safely
    // deleted.
    virtual int Release() = 0;

    // Gets the number of audio devices available for recording.
    virtual int GetNumOfRecordingDevices(int& devices) = 0;

    // Gets the number of audio devices available for playout.
    virtual int GetNumOfPlayoutDevices(int& devices) = 0;

    // Gets the name of a specific recording device given by an |index|.
    // On Windows Vista/7, it also retrieves an additional unique ID
    // (GUID) for the recording device.
    virtual int GetRecordingDeviceName(int index, char strNameUTF8[128],
                                       char strGuidUTF8[128]) = 0;

    // Gets the name of a specific playout device given by an |index|.
    // On Windows Vista/7, it also retrieves an additional unique ID
    // (GUID) for the playout device.
    virtual int GetPlayoutDeviceName(int index, char strNameUTF8[128],
                                     char strGuidUTF8[128]) = 0;

    // Checks if the sound card is available to be opened for recording.
    virtual int GetRecordingDeviceStatus(bool& isAvailable) = 0;

    // Checks if the sound card is available to be opened for playout.
    virtual int GetPlayoutDeviceStatus(bool& isAvailable) = 0;

    // Sets the audio device used for recording.
    virtual int SetRecordingDevice(
        int index, StereoChannel recordingChannel = kStereoBoth) = 0;

    // Sets the audio device used for playout.
    virtual int SetPlayoutDevice(int index) = 0;

    // Sets the type of audio device layer to use.
    virtual int SetAudioDeviceLayer(AudioLayers audioLayer) = 0;

    // Gets the currently used (active) audio device layer.
    virtual int GetAudioDeviceLayer(AudioLayers& audioLayer) = 0;

    // Gets the VoiceEngine's current CPU consumption in terms of the percent
    // of total CPU availability. [Windows only]
    virtual int GetCPULoad(int& loadPercent) = 0;

    // Not supported
    virtual int ResetAudioDevice() = 0;

    // Not supported
    virtual int AudioDeviceControl(
        unsigned int par1, unsigned int par2, unsigned int par3) = 0;

    // Not supported
    virtual int SetLoudspeakerStatus(bool enable) = 0;

    // Not supported
    virtual int GetLoudspeakerStatus(bool& enabled) = 0;

    // Native sample rate controls (samples/sec)
    virtual int SetRecordingSampleRate(unsigned int samples_per_sec) = 0;
    virtual int RecordingSampleRate(unsigned int* samples_per_sec) const = 0;
    virtual int SetPlayoutSampleRate(unsigned int samples_per_sec) = 0;
    virtual int PlayoutSampleRate(unsigned int* samples_per_sec) const = 0;

    // *Experimental - not recommended for use.*
    // Enables the Windows Core Audio built-in AEC. Fails on other platforms.
    //
    // Currently incompatible with the standard VoE AEC and AGC; don't attempt
    // to enable them while this is active.
    //
    // Must be called before VoEBase::StartSend(). When enabled:
    // 1. VoEBase::StartPlayout() must be called before VoEBase::StartSend().
    // 2. VoEBase::StopSend() should be called before VoEBase::StopPlayout().
    //    The reverse order may cause garbage audio to be rendered or the
    //    capture side to halt until StopSend() is called.
    //
    //    As a consequence, SetPlayoutDevice() should be used with caution
    //    during a call. It will function, but may cause the above issues for
    //    the duration it takes to complete. (In practice, it should complete
    //    fast enough to avoid audible degradation).
    virtual int EnableBuiltInAEC(bool enable) = 0;
    virtual bool BuiltInAECIsEnabled() const = 0;


protected:
    VoEHardware() {}
    virtual ~VoEHardware() {}
};

}  // namespace webrtc

#endif  //  WEBRTC_VOICE_ENGINE_VOE_HARDWARE_H
