/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/voe_video_sync_impl.h"

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/voice_engine/channel.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

namespace webrtc {

VoEVideoSync* VoEVideoSync::GetInterface(VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_VIDEO_SYNC_API
    return NULL;
#else
    if (NULL == voiceEngine)
    {
        return NULL;
    }
    VoiceEngineImpl* s = static_cast<VoiceEngineImpl*>(voiceEngine);
    s->AddRef();
    return s;
#endif
}

#ifdef WEBRTC_VOICE_ENGINE_VIDEO_SYNC_API

VoEVideoSyncImpl::VoEVideoSyncImpl(voe::SharedData* shared) : _shared(shared)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoEVideoSyncImpl::VoEVideoSyncImpl() - ctor");
}

VoEVideoSyncImpl::~VoEVideoSyncImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoEVideoSyncImpl::~VoEVideoSyncImpl() - dtor");
}

int VoEVideoSyncImpl::GetPlayoutTimestamp(int channel, unsigned int& timestamp)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetPlayoutTimestamp(channel=%d, timestamp=?)", channel);

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channel_ptr = ch.channel();
    if (channel_ptr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "GetPlayoutTimestamp() failed to locate channel");
        return -1;
    }
    return channel_ptr->GetPlayoutTimestamp(timestamp);
}

int VoEVideoSyncImpl::SetInitTimestamp(int channel,
                                       unsigned int timestamp)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetInitTimestamp(channel=%d, timestamp=%lu)",
                 channel, timestamp);

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetInitTimestamp() failed to locate channel");
        return -1;
    }
    return channelPtr->SetInitTimestamp(timestamp);
}

int VoEVideoSyncImpl::SetInitSequenceNumber(int channel,
                                            short sequenceNumber)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetInitSequenceNumber(channel=%d, sequenceNumber=%hd)",
                 channel, sequenceNumber);

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetInitSequenceNumber() failed to locate channel");
        return -1;
    }
    return channelPtr->SetInitSequenceNumber(sequenceNumber);
}

int VoEVideoSyncImpl::SetMinimumPlayoutDelay(int channel,int delayMs)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetMinimumPlayoutDelay(channel=%d, delayMs=%d)",
                 channel, delayMs);

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetMinimumPlayoutDelay() failed to locate channel");
        return -1;
    }
    return channelPtr->SetMinimumPlayoutDelay(delayMs);
}

int VoEVideoSyncImpl::SetCurrentSyncOffset(int channel, int offsetMs)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetCurrentSyncOffset(channel=%d, offsetMs=%d)",
                 channel, offsetMs);
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetCurrentSyncOffset() failed to locate channel");
        return -1;
    }
    channelPtr->SetCurrentSyncOffset(offsetMs);
    return 0;
}

int VoEVideoSyncImpl::SetInitialPlayoutDelay(int channel, int delay_ms)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetInitialPlayoutDelay(channel=%d, delay_ms=%d)",
                 channel, delay_ms);

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "SetInitialPlayoutDelay() failed to locate channel");
        return -1;
    }
    return channelPtr->SetInitialPlayoutDelay(delay_ms);
}

int VoEVideoSyncImpl::GetDelayEstimate(int channel,
                                       int* jitter_buffer_delay_ms,
                                       int* playout_buffer_delay_ms,
                                       int* avsync_offset_ms) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "GetDelayEstimate(channel=%d, delayMs=?)", channel);

  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "GetDelayEstimate() failed to locate channel");
    return -1;
  }
  if (!channelPtr->GetDelayEstimate(jitter_buffer_delay_ms,
                                    playout_buffer_delay_ms,
                                    avsync_offset_ms)) {
    return -1;
  }
  return 0;
}

int VoEVideoSyncImpl::GetPlayoutBufferSize(int& bufferMs)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "GetPlayoutBufferSize(bufferMs=?)");

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    AudioDeviceModule::BufferType type
        (AudioDeviceModule::kFixedBufferSize);
    uint16_t sizeMS(0);
    if (_shared->audio_device()->PlayoutBuffer(&type, &sizeMS) != 0)
    {
        _shared->SetLastError(VE_AUDIO_DEVICE_MODULE_ERROR, kTraceError,
            "GetPlayoutBufferSize() failed to read buffer size");
        return -1;
    }
    bufferMs = sizeMS;
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice,
        VoEId(_shared->instance_id(), -1),
        "GetPlayoutBufferSize() => bufferMs=%d", bufferMs);
    return 0;
}

int VoEVideoSyncImpl::GetRtpRtcp(int channel, RtpRtcp** rtpRtcpModule,
                                 RtpReceiver** rtp_receiver)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetRtpRtcp(channel=%i)", channel);

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL)
    {
        _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
            "GetPlayoutTimestamp() failed to locate channel");
        return -1;
    }
    return channelPtr->GetRtpRtcp(rtpRtcpModule, rtp_receiver);
}

int VoEVideoSyncImpl::GetLeastRequiredDelayMs(int channel) const {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "GetLeastRequiredDelayMS(channel=%d)", channel);

  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channel_ptr = ch.channel();
  if (channel_ptr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "GetLeastRequiredDelayMs() failed to locate channel");
    return -1;
  }
  return channel_ptr->least_required_delay_ms();
}

#endif  // #ifdef WEBRTC_VOICE_ENGINE_VIDEO_SYNC_API

}  // namespace webrtc
