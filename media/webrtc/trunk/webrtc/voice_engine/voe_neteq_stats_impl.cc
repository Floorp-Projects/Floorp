/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_neteq_stats_impl.h"

#include "audio_coding_module.h"
#include "channel.h"
#include "critical_section_wrapper.h"
#include "trace.h"
#include "voe_errors.h"
#include "voice_engine_impl.h"


namespace webrtc {

VoENetEqStats* VoENetEqStats::GetInterface(VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
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

#ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API

VoENetEqStatsImpl::VoENetEqStatsImpl(voe::SharedData* shared) : _shared(shared)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoENetEqStatsImpl::VoENetEqStatsImpl() - ctor");
}

VoENetEqStatsImpl::~VoENetEqStatsImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoENetEqStatsImpl::~VoENetEqStatsImpl() - dtor");
}

int VoENetEqStatsImpl::GetNetworkStatistics(int channel,
                                            NetworkStatistics& stats)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "GetNetworkStatistics(channel=%d, stats=?)", channel);
    ANDROID_NOT_SUPPORTED(_shared->statistics());
    IPHONE_NOT_SUPPORTED(_shared->statistics());

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
            "GetNetworkStatistics() failed to locate channel");
        return -1;
    }

    return channelPtr->GetNetworkStatistics(stats);
}

#endif  // #ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API

}   // namespace webrtc
