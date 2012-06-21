/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_HARDWARE_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_HARDWARE_IMPL_H

#include "voe_hardware.h"

#include "ref_count.h"
#include "shared_data.h"

namespace webrtc
{
class CpuWrapper;

class VoEHardwareImpl: public VoEHardware,
                       public voe::RefCount
{
public:
    virtual int Release();

    virtual int GetNumOfRecordingDevices(int& devices);

    virtual int GetNumOfPlayoutDevices(int& devices);

    virtual int GetRecordingDeviceName(int index,
                                       char strNameUTF8[128],
                                       char strGuidUTF8[128]);

    virtual int GetPlayoutDeviceName(int index,
                                     char strNameUTF8[128],
                                     char strGuidUTF8[128]);

    virtual int GetRecordingDeviceStatus(bool& isAvailable);

    virtual int GetPlayoutDeviceStatus(bool& isAvailable);

    virtual int SetRecordingDevice(
        int index,
        StereoChannel recordingChannel = kStereoBoth);

    virtual int SetPlayoutDevice(int index);

    virtual int SetAudioDeviceLayer(AudioLayers audioLayer);

    virtual int GetAudioDeviceLayer(AudioLayers& audioLayer);

    virtual int GetCPULoad(int& loadPercent);

    virtual int GetSystemCPULoad(int& loadPercent);

    virtual int ResetAudioDevice();

    virtual int AudioDeviceControl(unsigned int par1,
                                   unsigned int par2,
                                   unsigned int par3);

    virtual int SetLoudspeakerStatus(bool enable);

    virtual int GetLoudspeakerStatus(bool& enabled);

    virtual int EnableBuiltInAEC(bool enable);
    virtual bool BuiltInAECIsEnabled() const;

protected:
    VoEHardwareImpl(voe::SharedData* shared);
    virtual ~VoEHardwareImpl();

private:
    CpuWrapper* _cpu;
    voe::SharedData* _shared;
};

} // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_HARDWARE_IMPL_H
