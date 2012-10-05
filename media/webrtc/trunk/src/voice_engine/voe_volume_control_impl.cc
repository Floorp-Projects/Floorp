/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_volume_control_impl.h"

#include "channel.h"
#include "critical_section_wrapper.h"
#include "output_mixer.h"
#include "trace.h"
#include "transmit_mixer.h"
#include "voe_errors.h"
#include "voice_engine_impl.h"

namespace webrtc {

VoEVolumeControl* VoEVolumeControl::GetInterface(VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_VOLUME_CONTROL_API
    return NULL;
#else
    if (NULL == voiceEngine)
    {
        return NULL;
    }
    VoiceEngineImpl* s = reinterpret_cast<VoiceEngineImpl*>(voiceEngine);
    s->AddRef();
    return s;
#endif
}

#ifdef WEBRTC_VOICE_ENGINE_VOLUME_CONTROL_API

VoEVolumeControlImpl::VoEVolumeControlImpl(voe::SharedData* shared)
    : _shared(shared)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "VoEVolumeControlImpl::VoEVolumeControlImpl() - ctor");
}

VoEVolumeControlImpl::~VoEVolumeControlImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "VoEVolumeControlImpl::~VoEVolumeControlImpl() - dtor");
}

int VoEVolumeControlImpl::SetSpeakerVolume(unsigned int volume)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetSpeakerVolume(volume=%u)", volume);
    IPHONE_NOT_SUPPORTED();

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (volume > kMaxVolumeLevel)
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "SetSpeakerVolume() invalid argument");
        return -1;
    }

    WebRtc_UWord32 maxVol(0);
    WebRtc_UWord32 spkrVol(0);

    // scale: [0,kMaxVolumeLevel] -> [0,MaxSpeakerVolume]
    if (_shared->audio_device()->MaxSpeakerVolume(&maxVol) != 0)
    {
        _shared->SetLastError(VE_MIC_VOL_ERROR, kTraceError,
            "SetSpeakerVolume() failed to get max volume");
        return -1;
    }
    // Round the value and avoid floating computation.
    spkrVol = (WebRtc_UWord32)((volume * maxVol +
        (int)(kMaxVolumeLevel / 2)) / (kMaxVolumeLevel));

    // set the actual volume using the audio mixer
    if (_shared->audio_device()->SetSpeakerVolume(spkrVol) != 0)
    {
        _shared->SetLastError(VE_MIC_VOL_ERROR, kTraceError,
            "SetSpeakerVolume() failed to set speaker volume");
        return -1;
    }
    return 0;
}

int VoEVolumeControlImpl::GetSpeakerVolume(unsigned int& volume)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "GetSpeakerVolume()");
    IPHONE_NOT_SUPPORTED();

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    WebRtc_UWord32 spkrVol(0);
    WebRtc_UWord32 maxVol(0);

    if (_shared->audio_device()->SpeakerVolume(&spkrVol) != 0)
    {
        _shared->SetLastError(VE_GET_MIC_VOL_ERROR, kTraceError,
            "GetSpeakerVolume() unable to get speaker volume");
        return -1;
    }

    // scale: [0, MaxSpeakerVolume] -> [0, kMaxVolumeLevel]
    if (_shared->audio_device()->MaxSpeakerVolume(&maxVol) != 0)
    {
        _shared->SetLastError(VE_GET_MIC_VOL_ERROR, kTraceError,
            "GetSpeakerVolume() unable to get max speaker volume");
        return -1;
    }
    // Round the value and avoid floating computation.
    volume = (WebRtc_UWord32) ((spkrVol * kMaxVolumeLevel +
        (int)(maxVol / 2)) / (maxVol));

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
        VoEId(_shared->instance_id(), -1),
        "GetSpeakerVolume() => volume=%d", volume);
    return 0;
}

int VoEVolumeControlImpl::SetSystemOutputMute(bool enable)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "GetSystemOutputMute(enabled=%d)", enable);

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    if (_shared->audio_device()->SetSpeakerMute(enable) != 0)
    {
        _shared->SetLastError(VE_GET_MIC_VOL_ERROR, kTraceError,
            "SpeakerMute() unable to Set speaker mute");
        return -1;
    }

    return 0;
}

int VoEVolumeControlImpl::GetSystemOutputMute(bool& enabled)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "GetSystemOutputMute(enabled=?)");

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    if (_shared->audio_device()->SpeakerMute(&enabled) != 0)
    {
        _shared->SetLastError(VE_GET_MIC_VOL_ERROR, kTraceError,
            "SpeakerMute() unable to get speaker mute state");
        return -1;
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
        VoEId(_shared->instance_id(), -1),
        "GetSystemOutputMute() => %d", enabled);
    return 0;
}

int VoEVolumeControlImpl::SetMicVolume(unsigned int volume)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetMicVolume(volume=%u)", volume);
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED();

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (volume > kMaxVolumeLevel)
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "SetMicVolume() invalid argument");
        return -1;
    }

    WebRtc_UWord32 maxVol(0);
    WebRtc_UWord32 micVol(0);

    // scale: [0, kMaxVolumeLevel] -> [0,MaxMicrophoneVolume]
    if (_shared->audio_device()->MaxMicrophoneVolume(&maxVol) != 0)
    {
        _shared->SetLastError(VE_MIC_VOL_ERROR, kTraceError,
            "SetMicVolume() failed to get max volume");
        return -1;
    }

    if (volume == kMaxVolumeLevel) {
      // On Linux running pulse, users are able to set the volume above 100%
      // through the volume control panel, where the +100% range is digital
      // scaling. WebRTC does not support setting the volume above 100%, and
      // simply ignores changing the volume if the user tries to set it to
      // |kMaxVolumeLevel| while the current volume is higher than |maxVol|.
      if (_shared->audio_device()->MicrophoneVolume(&micVol) != 0) {
        _shared->SetLastError(VE_GET_MIC_VOL_ERROR, kTraceError,
            "SetMicVolume() unable to get microphone volume");
        return -1;
      }
      if (micVol >= maxVol)
        return 0;
    }

    // Round the value and avoid floating point computation.
    micVol = (WebRtc_UWord32) ((volume * maxVol +
        (int)(kMaxVolumeLevel / 2)) / (kMaxVolumeLevel));

    // set the actual volume using the audio mixer
    if (_shared->audio_device()->SetMicrophoneVolume(micVol) != 0)
    {
        _shared->SetLastError(VE_MIC_VOL_ERROR, kTraceError,
            "SetMicVolume() failed to set mic volume");
        return -1;
    }
    return 0;
}

int VoEVolumeControlImpl::GetMicVolume(unsigned int& volume)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "GetMicVolume()");
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED();

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    WebRtc_UWord32 micVol(0);
    WebRtc_UWord32 maxVol(0);

    if (_shared->audio_device()->MicrophoneVolume(&micVol) != 0)
    {
        _shared->SetLastError(VE_GET_MIC_VOL_ERROR, kTraceError,
            "GetMicVolume() unable to get microphone volume");
        return -1;
    }

    // scale: [0, MaxMicrophoneVolume] -> [0, kMaxVolumeLevel]
    if (_shared->audio_device()->MaxMicrophoneVolume(&maxVol) != 0)
    {
        _shared->SetLastError(VE_GET_MIC_VOL_ERROR, kTraceError,
            "GetMicVolume() unable to get max microphone volume");
        return -1;
    }
    if (micVol < maxVol) {
      // Round the value and avoid floating point calculation.
      volume = (WebRtc_UWord32) ((micVol * kMaxVolumeLevel +
          (int)(maxVol / 2)) / (maxVol));
    } else {
      // Truncate the value to the kMaxVolumeLevel.
      volume = kMaxVolumeLevel;
    }

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
        VoEId(_shared->instance_id(), -1),
        "GetMicVolume() => volume=%d", volume);
    return 0;
}

int VoEVolumeControlImpl::SetInputMute(int channel, bool enable)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetInputMute(channel=%d, enable=%d)", channel, enable);

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (channel == -1)
    {
        // Mute before demultiplexing <=> affects all channels
        return _shared->transmit_mixer()->SetMute(enable);
    }
    else
    {
        // Mute after demultiplexing <=> affects one channel only
        voe::ScopedChannel sc(_shared->channel_manager(), channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                "SetInputMute() failed to locate channel");
            return -1;
        }
        return channelPtr->SetMute(enable);
    }
    return 0;
}

int VoEVolumeControlImpl::GetInputMute(int channel, bool& enabled)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "GetInputMute(channel=%d)", channel);

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (channel == -1)
    {
        enabled = _shared->transmit_mixer()->Mute();
    }
    else
    {
        voe::ScopedChannel sc(_shared->channel_manager(), channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                "SetInputMute() failed to locate channel");
            return -1;
        }
        enabled = channelPtr->Mute();
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
        VoEId(_shared->instance_id(), -1),
        "GetInputMute() => enabled = %d", (int)enabled);
    return 0;
}

int VoEVolumeControlImpl::SetSystemInputMute(bool enable)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetSystemInputMute(enabled=%d)", enable);

    if (!_shared->statistics().Initialized())
    {
            _shared->SetLastError(VE_NOT_INITED, kTraceError);
            return -1;
    }

    if (_shared->audio_device()->SetMicrophoneMute(enable) != 0)
    {
        _shared->SetLastError(VE_GET_MIC_VOL_ERROR, kTraceError,
            "MicrophoneMute() unable to set microphone mute state");
        return -1;
    }

    return 0;
}

int VoEVolumeControlImpl::GetSystemInputMute(bool& enabled)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "GetSystemInputMute(enabled=?)");

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    if (_shared->audio_device()->MicrophoneMute(&enabled) != 0)
    {
        _shared->SetLastError(VE_GET_MIC_VOL_ERROR, kTraceError,
            "MicrophoneMute() unable to get microphone mute state");
        return -1;
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
        VoEId(_shared->instance_id(), -1),
        "GetSystemInputMute() => %d", enabled);
	return 0;
}

int VoEVolumeControlImpl::GetSpeechInputLevel(unsigned int& level)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "GetSpeechInputLevel()");

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    WebRtc_Word8 currentLevel = _shared->transmit_mixer()->AudioLevel();
    level = static_cast<unsigned int> (currentLevel);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
        VoEId(_shared->instance_id(), -1),
        "GetSpeechInputLevel() => %d", level);
    return 0;
}

int VoEVolumeControlImpl::GetSpeechOutputLevel(int channel,
                                               unsigned int& level)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "GetSpeechOutputLevel(channel=%d, level=?)", channel);
	
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (channel == -1)
    {
        return _shared->output_mixer()->GetSpeechOutputLevel(
            (WebRtc_UWord32&)level);
    }
    else
    {
        voe::ScopedChannel sc(_shared->channel_manager(), channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                "GetSpeechOutputLevel() failed to locate channel");
            return -1;
        }
        channelPtr->GetSpeechOutputLevel((WebRtc_UWord32&)level);
    }
    return 0;
}

int VoEVolumeControlImpl::GetSpeechInputLevelFullRange(unsigned int& level)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "GetSpeechInputLevelFullRange(level=?)");

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    WebRtc_Word16 currentLevel = _shared->transmit_mixer()->
        AudioLevelFullRange();
    level = static_cast<unsigned int> (currentLevel);
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
        VoEId(_shared->instance_id(), -1),
        "GetSpeechInputLevelFullRange() => %d", level);
    return 0;
}

int VoEVolumeControlImpl::GetSpeechOutputLevelFullRange(int channel,
                                                        unsigned int& level)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "GetSpeechOutputLevelFullRange(channel=%d, level=?)", channel);

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (channel == -1)
    {
        return _shared->output_mixer()->GetSpeechOutputLevelFullRange(
            (WebRtc_UWord32&)level);
    }
    else
    {
        voe::ScopedChannel sc(_shared->channel_manager(), channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                "GetSpeechOutputLevelFullRange() failed to locate channel");
            return -1;
        }
        channelPtr->GetSpeechOutputLevelFullRange((WebRtc_UWord32&)level);
    }
    return 0;
}

int VoEVolumeControlImpl::SetChannelOutputVolumeScaling(int channel,
                                                        float scaling)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetChannelOutputVolumeScaling(channel=%d, scaling=%3.2f)",
               channel, scaling);
    IPHONE_NOT_SUPPORTED();
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    if (scaling < kMinOutputVolumeScaling ||
        scaling > kMaxOutputVolumeScaling)
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "SetChannelOutputVolumeScaling() invalid parameter");
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetChannelOutputVolumeScaling() failed to locate channel");
        return -1;
    }
    return channelPtr->SetChannelOutputVolumeScaling(scaling);
}

int VoEVolumeControlImpl::GetChannelOutputVolumeScaling(int channel,
                                                        float& scaling)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "GetChannelOutputVolumeScaling(channel=%d, scaling=?)", channel);
    IPHONE_NOT_SUPPORTED();
    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_shared->channel_manager(), channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "GetChannelOutputVolumeScaling() failed to locate channel");
        return -1;
    }
    return channelPtr->GetChannelOutputVolumeScaling(scaling);
}

int VoEVolumeControlImpl::SetOutputVolumePan(int channel,
                                             float left,
                                             float right)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetOutputVolumePan(channel=%d, left=%2.1f, right=%2.1f)",
               channel, left, right);
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED();

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    bool available(false);
    _shared->audio_device()->StereoPlayoutIsAvailable(&available);
    if (!available)
    {
        _shared->SetLastError(VE_FUNC_NO_STEREO, kTraceError,
            "SetOutputVolumePan() stereo playout not supported");
        return -1;
    }
    if ((left < kMinOutputVolumePanning)  ||
        (left > kMaxOutputVolumePanning)  ||
        (right < kMinOutputVolumePanning) ||
        (right > kMaxOutputVolumePanning))
    {
        _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
            "SetOutputVolumePan() invalid parameter");
        return -1;
    }

    if (channel == -1)
    {
        // Master balance (affectes the signal after output mixing)
        return _shared->output_mixer()->SetOutputVolumePan(left, right);
    }
    else
    {
        // Per-channel balance (affects the signal before output mixing)
        voe::ScopedChannel sc(_shared->channel_manager(), channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                "SetOutputVolumePan() failed to locate channel");
            return -1;
        }
        return channelPtr->SetOutputVolumePan(left, right);
    }
    return 0;
}

int VoEVolumeControlImpl::GetOutputVolumePan(int channel,
                                             float& left,
                                             float& right)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "GetOutputVolumePan(channel=%d, left=?, right=?)", channel);
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED();

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    bool available(false);
    _shared->audio_device()->StereoPlayoutIsAvailable(&available);
    if (!available)
    {
        _shared->SetLastError(VE_FUNC_NO_STEREO, kTraceError,
            "GetOutputVolumePan() stereo playout not supported");
        return -1;
    }

    if (channel == -1)
    {
        return _shared->output_mixer()->GetOutputVolumePan(left, right);
    }
    else
    {
        voe::ScopedChannel sc(_shared->channel_manager(), channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                "GetOutputVolumePan() failed to locate channel");
            return -1;
        }
        return channelPtr->GetOutputVolumePan(left, right);
    }
    return 0;
}

#endif  // #ifdef WEBRTC_VOICE_ENGINE_VOLUME_CONTROL_API

}  // namespace webrtc
