/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/voe_encryption_impl.h"


#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/voice_engine/channel.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

namespace webrtc {

VoEEncryption* VoEEncryption::GetInterface(VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_ENCRYPTION_API
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

#ifdef WEBRTC_VOICE_ENGINE_ENCRYPTION_API

VoEEncryptionImpl::VoEEncryptionImpl(voe::SharedData* shared) : _shared(shared)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoEEncryptionImpl::VoEEncryptionImpl() - ctor");
}

VoEEncryptionImpl::~VoEEncryptionImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "VoEEncryptionImpl::~VoEEncryptionImpl() - dtor");
}

int VoEEncryptionImpl::RegisterExternalEncryption(int channel,
                                                  Encryption& encryption)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "RegisterExternalEncryption(channel=%d, encryption=0x%x)",
                 channel, &encryption);
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
            "RegisterExternalEncryption() failed to locate channel");
        return -1;
    }
    return channelPtr->RegisterExternalEncryption(encryption);
}

int VoEEncryptionImpl::DeRegisterExternalEncryption(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "DeRegisterExternalEncryption(channel=%d)", channel);
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
            "DeRegisterExternalEncryption() failed to locate channel");
        return -1;
    }
    return channelPtr->DeRegisterExternalEncryption();
}

#endif  // #ifdef WEBRTC_VOICE_ENGINE_ENCRYPTION_API

// EOF
}  // namespace webrtc
