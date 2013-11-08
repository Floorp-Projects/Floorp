/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_VOLUME_CONTROL_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_VOLUME_CONTROL_IMPL_H

#include "webrtc/voice_engine/include/voe_volume_control.h"

#include "webrtc/voice_engine/shared_data.h"

namespace webrtc {

class VoEVolumeControlImpl : public VoEVolumeControl
{
public:
    virtual int SetSpeakerVolume(unsigned int volume);

    virtual int GetSpeakerVolume(unsigned int& volume);

    virtual int SetSystemOutputMute(bool enable);

    virtual int GetSystemOutputMute(bool& enabled);

    virtual int SetMicVolume(unsigned int volume);

    virtual int GetMicVolume(unsigned int& volume);

    virtual int SetInputMute(int channel, bool enable);

    virtual int GetInputMute(int channel, bool& enabled);

    virtual int SetSystemInputMute(bool enable);

    virtual int GetSystemInputMute(bool& enabled);

    virtual int GetSpeechInputLevel(unsigned int& level);

    virtual int GetSpeechOutputLevel(int channel, unsigned int& level);

    virtual int GetSpeechInputLevelFullRange(unsigned int& level);

    virtual int GetSpeechOutputLevelFullRange(int channel,
                                              unsigned int& level);

    virtual int SetChannelOutputVolumeScaling(int channel, float scaling);

    virtual int GetChannelOutputVolumeScaling(int channel, float& scaling);

    virtual int SetOutputVolumePan(int channel, float left, float right);

    virtual int GetOutputVolumePan(int channel, float& left, float& right);


protected:
    VoEVolumeControlImpl(voe::SharedData* shared);
    virtual ~VoEVolumeControlImpl();

private:
    voe::SharedData* _shared;
};

}  // namespace webrtc

#endif    // WEBRTC_VOICE_ENGINE_VOE_VOLUME_CONTROL_IMPL_H
