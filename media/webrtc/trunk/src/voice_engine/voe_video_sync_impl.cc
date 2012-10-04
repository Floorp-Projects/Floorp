/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_video_sync_impl.h"

#include "channel.h"
#include "critical_section_wrapper.h"
#include "trace.h"
#include "voe_errors.h"
#include "voice_engine_impl.h"

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
    VoiceEngineImpl* s = reinterpret_cast<VoiceEngineImpl*>(voiceEngine);
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
    ANDROID_NOT_SUPPORTED(_shared->statistics());
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
            "GetPlayoutTimestamp() failed to locate channel");
        return -1;
    }
    return channelPtr->GetPlayoutTimestamp(timestamp);
}

int VoEVideoSyncImpl::SetInitTimestamp(int channel,
                                       unsigned int timestamp)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "SetInitTimestamp(channel=%d, timestamp=%lu)",
                 channel, timestamp);
    ANDROID_NOT_SUPPORTED(_shared->statistics());
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
    ANDROID_NOT_SUPPORTED(_shared->statistics());
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
    ANDROID_NOT_SUPPORTED(_shared->statistics());
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
            "SetMinimumPlayoutDelay() failed to locate channel");
        return -1;
    }
    return channelPtr->SetMinimumPlayoutDelay(delayMs);
}

int VoEVideoSyncImpl::GetDelayEstimate(int channel, int& delayMs)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetDelayEstimate(channel=%d, delayMs=?)", channel);
    ANDROID_NOT_SUPPORTED(_shared->statistics());
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
            "GetDelayEstimate() failed to locate channel");
        return -1;
    }
    return channelPtr->GetDelayEstimate(delayMs);
}

int VoEVideoSyncImpl::GetPlayoutBufferSize(int& bufferMs)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "GetPlayoutBufferSize(bufferMs=?)");
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED();

    if (!_shared->statistics().Initialized())
    {
        _shared->SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    AudioDeviceModule::BufferType type
        (AudioDeviceModule::kFixedBufferSize);
    WebRtc_UWord16 sizeMS(0);
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

int VoEVideoSyncImpl::GetRtpRtcp(int channel, RtpRtcp* &rtpRtcpModule)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetRtpRtcp(channel=%i)", channel);
    
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
            "GetPlayoutTimestamp() failed to locate channel");
        return -1;
    }
    return channelPtr->GetRtpRtcp(rtpRtcpModule);
}


#endif  // #ifdef WEBRTC_VOICE_ENGINE_VIDEO_SYNC_API

}  // namespace webrtc
